// Device-ul IoT din tema 01, de data aceasta pe un ESP32-C3 Super Mini.
//
// Aceeași logică și același protocol ca device-simulator din tema 01, cu trei
// taskuri FreeRTOS în loc de trei thread-uri Java:
//
//   task periodic --(coada de alarme)--> task sporadic
//        |                                     |
//        +---- telemetry ---> MQTT <--- alarm -+
//                              |
//                          commands --> command_received() --(coada de comenzi)--> task aperiodic ---> status
//
// Gata implementate (nu trebuie modificate):
//   config.h         setările: Wi-Fi, broker, STUDENT, valorile inițiale; DEVICE_ID
//                    se citește din adresa MAC a plăcii
//   connection.h/cpp Wi-Fi, ora exactă (SNTP), clientul MQTT, mqtt_publish(), epoch_ms()
//
// Prioritățile taskurilor (ESP32-C3 are un singur nucleu, deci contează):
//   periodic  5  are termen la fiecare eliberare, cea mai scurtă perioadă
//   sporadic  4  cel mult un job la MIN_INTERARRIVAL_MS
//   aperiodic 3  comenzile operatorului, fără constrângeri de timp
// Toate sunt peste loopTask (1) și sub taskurile sistemului (20, 23).
//
// Placa: ESP32C3 Dev Module, USB CDC On Boot: Enabled. Serial Monitor: 115200 baud.

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "connection.h"

const UBaseType_t PRIORITY_PERIODIC = 5;
const UBaseType_t PRIORITY_SPORADIC = 4;
const UBaseType_t PRIORITY_APERIODIC = 3;
const uint32_t TASK_STACK_BYTES = 4096;

const UBaseType_t ALARM_QUEUE_LEN = 8;
const UBaseType_t COMMAND_QUEUE_LEN = 8;

// evenimentul pus de taskul periodic în coada de alarme (copiat prin valoare)
struct AlarmEvent {
    float temperature;
    float threshold;
};

// comanda pusă de command_received() în coada de comenzi: textul însuși, nu un pointer
struct Command {
    char text[COMMAND_MAX_LEN];
};

static QueueHandle_t g_alarm_queue = NULL;
static QueueHandle_t g_command_queue = NULL;

// Scrise de taskul aperiodic, citite de taskul periodic la fiecare job.
// Pe un nucleu de 32 de biți, citirea și scrierea lor sunt atomice.
static volatile uint32_t g_period_ms = PERIOD_MS_INITIAL;
static volatile float g_threshold = THRESHOLD_INITIAL;

// Temperatura cipului, în °C, de la senzorul intern al ESP32-C3.
// Este temperatura siliciului, nu a camerei: imediat după pornirea Wi-Fi-ului
// poate fi în jur de 50 °C, apoi scade în câteva minute spre 35-40 °C.
// Rezoluția este de aproximativ 1 °C. Ca să provocați alarme, puneți pragul
// cu SET_THRESHOLD cu un grad sub valoarea citită.
float read_temperature(void) {
    return temperatureRead();
}

// TASK PERIODIC, cu perioada T = g_period_ms: fiecare job citește senzorul și
// publică telemetria. Dacă temperatura depășește pragul, doar semnalează un
// eveniment în coada de alarme; alarma o tratează taskul sporadic.
// Eliberările respectă grila r_k = r_0 + kT: xTaskDelayUntil așteaptă până la
// last + T și mută last cu T, deci timpul de execuție al jobului nu produce derivă.
static void periodic_task(void* arg) {
    (void)arg;
    TickType_t last = xTaskGetTickCount();  // r_0

    for (;;) {
        float temperature = read_temperature();
        float threshold = g_threshold;

        char message[64];
        snprintf(message, sizeof(message), "%s;%lld;%.2f", DEVICE_ID, (long long)epoch_ms(), temperature);
        mqtt_publish("telemetry", message);
        Serial.printf("[periodic] job la %lu ms: %s\n", (unsigned long)millis(), message);

        if (temperature > threshold) {
            AlarmEvent ev = {temperature, threshold};
            // timeout 0: taskul periodic nu se blochează dacă coada este plină
            if (xQueueSend(g_alarm_queue, &ev, 0) != pdTRUE) {
                Serial.println("[periodic] coada de alarme plină, eveniment pierdut");
            }
        }

        // perioada se citește la fiecare job: SET_INTERVAL are efect de la următoarea eliberare
        if (xTaskDelayUntil(&last, pdMS_TO_TICKS(g_period_ms)) == pdFALSE) {
            // Jobul a durat mai mult decât perioada (de exemplu primul mqtt_publish,
            // cât timp clientul încă se conectează): eliberarea următoare era deja în
            // trecut. Fără corecție, xTaskDelayUntil ar recupera toate eliberările
            // ratate în rafală, una după alta; le sărim și reluăm grila de acum.
            last = xTaskGetTickCount();
            Serial.println("[periodic] eliberare ratată (jobul a depășit perioada), grila se reia de acum");
        }
    }
}

// TASK SPORADIC: tratează alarmele de temperatură. Impune intervalul minim
// între sosiri MIN_INTERARRIVAL_MS: evenimentele care sosesc mai des sunt
// respinse. Intervalul se măsoară cu millis() (monoton), nu cu epoch_ms().
static void sporadic_task(void* arg) {
    (void)arg;
    bool released = false;  // nicio alarmă publicată încă
    uint32_t last_release = 0;

    for (;;) {
        AlarmEvent ev;
        xQueueReceive(g_alarm_queue, &ev, portMAX_DELAY);

        uint32_t now = millis();
        uint32_t since_last = now - last_release;  // corect și la depășirea lui millis()
        if (released && since_last < MIN_INTERARRIVAL_MS) {
            Serial.printf("[sporadic] alarmă RESPINSĂ: au trecut doar %lu ms (minim %lu ms)\n",
                          (unsigned long)since_last, (unsigned long)MIN_INTERARRIVAL_MS);
            continue;
        }
        released = true;
        last_release = now;

        char message[80];
        snprintf(message, sizeof(message), "%s;%lld;%.2f;%.2f", DEVICE_ID, (long long)epoch_ms(), ev.temperature,
                 ev.threshold);
        mqtt_publish("alarm", message);
        Serial.printf("[sporadic] alarmă publicată: %s\n", message);
    }
}

// Elimină spațiile de la început și de la sfârșit, pe loc.
static char* trim(char* s) {
    while (isspace((unsigned char)*s)) s++;
    char* end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return s;
}

// Execută o comandă și scrie în reply confirmarea ("OK ..." sau "ERROR ...").
static void execute_command(char* command, char* reply, size_t size) {
    char* saveptr = NULL;
    char* name = strtok_r(command, " \t", &saveptr);
    char* param = strtok_r(NULL, " \t", &saveptr);
    char* extra = strtok_r(NULL, " \t", &saveptr);
    for (char* p = name; *p; p++) *p = toupper((unsigned char)*p);

    if (strcmp(name, "SET_INTERVAL") == 0) {
        if (param == NULL) {
            snprintf(reply, size, "ERROR SET_INTERVAL: missing parameter");
            return;
        }
        char* end;
        long period = strtol(param, &end, 10);
        if (*end != '\0' || extra != NULL) {
            snprintf(reply, size, "ERROR SET_INTERVAL: invalid parameter");
        } else if (period <= 0) {
            snprintf(reply, size, "ERROR SET_INTERVAL: period must be positive");
        } else {
            g_period_ms = (uint32_t)period;
            snprintf(reply, size, "OK SET_INTERVAL %lu", (unsigned long)g_period_ms);
        }
    } else if (strcmp(name, "SET_THRESHOLD") == 0) {
        if (param == NULL) {
            snprintf(reply, size, "ERROR SET_THRESHOLD: missing parameter");
            return;
        }
        char* end;
        float threshold = strtof(param, &end);
        if (*end != '\0' || extra != NULL || !isfinite(threshold)) {
            snprintf(reply, size, "ERROR SET_THRESHOLD: invalid value");
        } else {
            g_threshold = threshold;
            snprintf(reply, size, "OK SET_THRESHOLD %.2f", threshold);
        }
    } else if (strcmp(name, "RESET") == 0) {
        g_period_ms = PERIOD_MS_INITIAL;
        g_threshold = THRESHOLD_INITIAL;
        snprintf(reply, size, "OK RESET period=%lu threshold=%.2f", (unsigned long)PERIOD_MS_INITIAL,
                 THRESHOLD_INITIAL);
    } else if (strcmp(name, "STATUS") == 0) {
        snprintf(reply, size, "OK STATUS period=%lu threshold=%.2f", (unsigned long)g_period_ms, g_threshold);
    } else {
        snprintf(reply, size, "ERROR unknown command: %s", name);
    }
}

// TASK APERIODIC: fiecare comandă din coada de comenzi eliberează un job, care
// o execută și publică o confirmare pe topic-ul status. O comandă greșită
// produce "ERROR ..." și taskul continuă.
static void aperiodic_task(void* arg) {
    (void)arg;

    for (;;) {
        Command cmd;
        xQueueReceive(g_command_queue, &cmd, portMAX_DELAY);

        char* command = trim(cmd.text);
        if (*command == '\0') continue;

        char reply[96];
        execute_command(command, reply, sizeof(reply));

        char message[160];
        snprintf(message, sizeof(message), "%s;%lld;%s", DEVICE_ID, (long long)epoch_ms(), reply);
        mqtt_publish("status", message);
        Serial.printf("[aperiodic] %s\n", message);
    }
}

// Apelată de clientul MQTT pentru fiecare comandă primită (vezi connection.h).
// Rulează pe taskul clientului MQTT: doar copiază comanda în coadă, fără să se blocheze.
void command_received(const char* text) {
    Serial.printf("[mqtt] comandă primită: %s\n", text);
    Command cmd;
    strncpy(cmd.text, text, sizeof(cmd.text) - 1);
    cmd.text[sizeof(cmd.text) - 1] = '\0';
    if (xQueueSend(g_command_queue, &cmd, 0) != pdTRUE) {
        Serial.println("[mqtt] coada de comenzi plină, comandă pierdută");
    }
}

static void start_task(TaskFunction_t fn, const char* name, UBaseType_t priority) {
    if (xTaskCreate(fn, name, TASK_STACK_BYTES, NULL, priority, NULL) != pdPASS) {
        Serial.printf("NU s-a putut porni taskul %s\n", name);
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
    Serial.printf("=== tema 02: STUDENT %s, DEVICE_ID %s ===\n", STUDENT, DEVICE_ID);

    wifi_connect();
    time_sync();

    // cozile, înainte de mqtt_start(): de acolo încolo pot sosi comenzi
    g_alarm_queue = xQueueCreate(ALARM_QUEUE_LEN, sizeof(AlarmEvent));
    g_command_queue = xQueueCreate(COMMAND_QUEUE_LEN, sizeof(Command));
    if (g_alarm_queue == NULL || g_command_queue == NULL) {
        Serial.println("NU s-au putut crea cozile");
    }

    mqtt_start();

    // taskurile, după mqtt_start(), ca primele mesaje publicate să nu se piardă
    start_task(periodic_task, "periodic", PRIORITY_PERIODIC);
    start_task(sporadic_task, "sporadic", PRIORITY_SPORADIC);
    start_task(aperiodic_task, "aperiodic", PRIORITY_APERIODIC);
}

void loop() {
    // Totul se face în taskuri: loop() nu are nimic de făcut.
    delay(1000);
}
