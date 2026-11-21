// Semafor pentru pietoni, cu buton, pe ESP32-C3 Super Mini.
//
// Placa este controlerul semaforului; lămpile sunt desenate de dashboard, care
// le aprinde după mesajele "lights" publicate de placă. Patru activități:
//
//   ISR buton BOOT ----+
//                      +--(coada butonului)--> task sporadic
//   command_received --+   (BUTTON)                 |
//                                                   v
//                                         [ starea semaforului ] <-- mutex
//                                            ^          |
//                                            |          v
//                         task periodic de control   task periodic de ieșire
//                         (fazele, T = 100 ms)       (lămpile, T = 500 ms) --> lights
//
// Gata implementate (nu trebuie modificate):
//   config.h         setările: Wi-Fi, broker, STUDENT, pinii, perioadele, duratele fazelor
//   connection.h/cpp Wi-Fi, ora exactă (SNTP), clientul MQTT, mqtt_publish(), epoch_ms()
//
// Așa cum este, schița se conectează la Wi-Fi și la broker și afișează pe
// Serial comenzile primite. Restul îl scrieți voi: TODO 0 ... TODO 3.
//
// Placa: ESP32C3 Dev Module, USB CDC On Boot: Enabled. Serial Monitor: 115200 baud.

#include "config.h"
#include "connection.h"

// Fazele semaforului (vezi tabelul și diagrama din README.md). Numele din
// PHASE_NAMES sunt cele din mesajul "lights", în aceeași ordine ca în enum.
enum Phase { CAR_GREEN, CAR_YELLOW, ALL_RED, PED_GREEN };
const char* const PHASE_NAMES[] = {"CAR_GREEN", "CAR_YELLOW", "ALL_RED", "PED_GREEN"};

// Culorile unei lămpi, cu numele din mesajul "lights".
enum Light { RED, YELLOW, GREEN };
const char* const LIGHT_NAMES[] = {"RED", "YELLOW", "GREEN"};

// TODO 0 (c): STAREA SEMAFORULUI, partajată de taskuri și protejată de g_state_mutex.
//   Mutexul se ține doar cât se citesc sau se scriu câmpurile: fără
//   mqtt_publish(), Serial sau așteptări cât timp este luat.
struct LightState {
    Phase phase;              // faza curentă
    uint32_t phase_start_ms;  // momentul intrării în faza curentă (millis())
    bool ped_request;         // cererea pietonului (lampa AȘTEPTAȚI)
};

static LightState g_state = {CAR_GREEN, 0, false};
static SemaphoreHandle_t g_state_mutex = NULL;

// Elementul cozii butonului: o apăsare, cu momentul sosirii ei (ms de la
// pornire, din tickuri) și sursa. Se copiază în coadă, nu este pointer.
enum PressSource { SRC_BOOT, SRC_DASHBOARD };
const char* const SOURCE_NAMES[] = {"BOOT", "dashboard"};

struct ButtonPress {
    uint32_t time_ms;
    PressSource source;
};

static QueueHandle_t g_button_queue = NULL;

// Prioritățile (între 2 și 19). Rate Monotonic: perioada mai mică, prioritatea
// mai mare; taskul sporadic, cu intervalul minim între sosiri de 1000 ms, este
// tratat ca un task cu perioada de 1000 ms.
const UBaseType_t CONTROL_PRIORITY = 5;   // T = 100 ms
const UBaseType_t OUTPUT_PRIORITY = 4;    // T = 500 ms
const UBaseType_t SPORADIC_PRIORITY = 3;  // T_min = 1000 ms
const uint32_t TASK_STACK = 4096;

// Durata fazei p; pentru CAR_GREEN, durata minimă.
static uint32_t phase_duration_ms(Phase p) {
    switch (p) {
        case CAR_GREEN: return CAR_GREEN_MIN_MS;
        case CAR_YELLOW: return CAR_YELLOW_MS;
        case ALL_RED: return ALL_RED_MS;
        case PED_GREEN: return PED_GREEN_MS;
    }
    return 0;
}

// TODO 1: TASK PERIODIC DE CONTROL, cu perioada CONTROL_PERIOD_MS.
//   Automatul cu fazele din README.md. Nu publică nimic, doar actualizează starea.
static void control_task(void* arg) {
    const TickType_t period = pdMS_TO_TICKS(CONTROL_PERIOD_MS);
    TickType_t last = xTaskGetTickCount();  // r_0
    for (;;) {
        bool changed = false;

        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        uint32_t now = millis();
        uint32_t lasted = now - g_state.phase_start_ms;
        Phase phase = g_state.phase;
        if (lasted >= phase_duration_ms(phase)) {
            switch (phase) {
                case CAR_GREEN:
                    // fără cerere, verdele pentru mașini continuă oricât
                    if (g_state.ped_request) phase = CAR_YELLOW;
                    break;
                case CAR_YELLOW: phase = ALL_RED; break;
                case ALL_RED:
                    phase = PED_GREEN;
                    g_state.ped_request = false;  // pietonul primește ce a cerut
                    break;
                case PED_GREEN: phase = CAR_GREEN; break;
            }
            if (phase != g_state.phase) {
                g_state.phase = phase;
                g_state.phase_start_ms = now;
                changed = true;
            }
        }
        xSemaphoreGive(g_state_mutex);

        if (changed) {
            Serial.printf("[control] %lu ms: faza %s (precedenta a durat %lu ms)\n", (unsigned long)now,
                          PHASE_NAMES[phase], (unsigned long)lasted);
        }

        // Așteptare absolută: r_{k+1} = r_k + T. Dacă jobul a depășit perioada,
        // grila se reia de acum, fără o rafală de joburi întârziate.
        if (xTaskDelayUntil(&last, period) == pdFALSE) last = xTaskGetTickCount();
    }
}

// TODO 2: TASK PERIODIC DE IEȘIRE, cu perioada OUTPUT_PERIOD_MS.
//   Lămpile din faza curentă, publicate pe "lights", și LED-ul pentru AȘTEPTAȚI.
static void output_task(void* arg) {
    const TickType_t period = pdMS_TO_TICKS(OUTPUT_PERIOD_MS);
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        LightState s = g_state;  // copie locală, mutexul se eliberează imediat
        uint32_t now = millis();
        xSemaphoreGive(g_state_mutex);

        Light cars = RED;
        Light peds = RED;
        uint32_t seconds = 0;
        switch (s.phase) {
            case CAR_GREEN: cars = GREEN; break;
            case CAR_YELLOW: cars = YELLOW; break;
            case ALL_RED: break;
            case PED_GREEN: {
                peds = GREEN;
                uint32_t lasted = now - s.phase_start_ms;
                uint32_t left = lasted < PED_GREEN_MS ? PED_GREEN_MS - lasted : 0;
                seconds = (left + 999) / 1000;  // rotunjit în sus: 7,2 s -> 8
                // faza poate dura cu până la o perioadă de control peste PED_GREEN_MS;
                // cât timp pietonii au verde, numărătoarea nu coboară la 0 (= nu se afișează)
                if (seconds == 0) seconds = 1;
                break;
            }
        }

        char text[96];
        snprintf(text, sizeof(text), "%s;%lld;%s;%s;%s;%d;%lu", DEVICE_ID, (long long)epoch_ms(),
                 PHASE_NAMES[s.phase], LIGHT_NAMES[cars], LIGHT_NAMES[peds], s.ped_request ? 1 : 0,
                 (unsigned long)seconds);
        mqtt_publish("lights", text);
        digitalWrite(LED_PIN, s.ped_request ? LOW : HIGH);  // LED aprins pe LOW

        if (xTaskDelayUntil(&last, period) == pdFALSE) last = xTaskGetTickCount();
    }
}

// TODO 3: TASK SPORADIC, tratează apăsările din coada butonului (BOOT și BUTTON).
static void button_task(void* arg) {
    bool accepted_once = false;  // prima apăsare se acceptă mereu
    uint32_t last_accepted_ms = 0;
    ButtonPress press;
    for (;;) {
        xQueueReceive(g_button_queue, &press, portMAX_DELAY);

        uint32_t since = press.time_ms - last_accepted_ms;
        if (accepted_once && since < MIN_INTERARRIVAL_MS) {
            Serial.printf("[buton] apăsare %s RESPINSĂ: au trecut doar %lu ms (minim %lu ms)\n",
                          SOURCE_NAMES[press.source], (unsigned long)since, (unsigned long)MIN_INTERARRIVAL_MS);
            continue;
        }
        accepted_once = true;
        last_accepted_ms = press.time_ms;

        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        bool already = g_state.ped_request;
        g_state.ped_request = true;
        Phase phase = g_state.phase;
        xSemaphoreGive(g_state_mutex);

        Serial.printf("[buton] apăsare %s acceptată la %lu ms, în faza %s%s\n", SOURCE_NAMES[press.source],
                      (unsigned long)press.time_ms, PHASE_NAMES[phase],
                      already ? " (cererea exista deja)" : ": cerere înregistrată");
    }
}

// TODO 3: RUTINA DE ÎNTRERUPERE a butonului BOOT. Doar pune apăsarea în coadă,
//   cu momentul ei; bounce-ul îl filtrează taskul sporadic.
void IRAM_ATTR button_isr(void) {
    ButtonPress press = {(uint32_t)(xTaskGetTickCountFromISR() * portTICK_PERIOD_MS), SRC_BOOT};
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(g_button_queue, &press, &woken);
    portYIELD_FROM_ISR(woken);  // dacă a fost trezit un task mai prioritar, rulează imediat
}

// Apelată de clientul MQTT pentru fiecare comandă primită (vezi connection.h).
void command_received(const char* text) {
    Serial.printf("[mqtt] comandă primită: %s\n", text);
    // fără spațiile sau "\r\n" de la capete
    while (isspace((unsigned char)*text)) text++;
    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) len--;

    if (len == 6 && strncasecmp(text, "BUTTON", 6) == 0) {
        ButtonPress press = {(uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS), SRC_DASHBOARD};
        if (xQueueSend(g_button_queue, &press, 0) != pdTRUE) {
            Serial.println("[mqtt] coada butonului este plină, apăsarea se pierde");
        }
    } else {
        Serial.println("[mqtt] comandă necunoscută, ignorată");
    }
}

void setup() {
    Serial.begin(115200);
    // Fără așteptare la scriere: cu placa la USB, dar fără Serial Monitor deschis,
    // fiecare Serial.printf() ar aștepta până la 2 s să fie citit și ar întârzia
    // taskul care scrie. Așa, textul pe care nu-l citește nimeni se pierde.
    Serial.setTxTimeoutMs(0);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);  // așteptăm Serial Monitor, cel mult 3 s
    delay(200);
    Serial.println();
    Serial.printf("=== tema 03: STUDENT %s, DEVICE_ID %s ===\n", STUDENT, DEVICE_ID);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);  // LED stins
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    wifi_connect();
    time_sync();

    // TODO 0 (a): mutexul stării și coada butonului, înainte de mqtt_start():
    //   de acolo încolo pot sosi comenzi.
    g_state_mutex = xSemaphoreCreateMutex();
    g_button_queue = xQueueCreate(8, sizeof(ButtonPress));
    g_state.phase = CAR_GREEN;
    g_state.phase_start_ms = millis();

    mqtt_start();

    // TODO 0 (b): cele trei taskuri, apoi întreruperea butonului.
    xTaskCreate(control_task, "control", TASK_STACK, NULL, CONTROL_PRIORITY, NULL);
    xTaskCreate(output_task, "output", TASK_STACK, NULL, OUTPUT_PRIORITY, NULL);
    xTaskCreate(button_task, "button", TASK_STACK, NULL, SPORADIC_PRIORITY, NULL);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_isr, FALLING);
}

void loop() {
    // Totul se face în taskuri: loop() nu are nimic de făcut.
    delay(1000);
}
