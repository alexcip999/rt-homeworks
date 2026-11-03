// 04_sporadic_interval_minim - un task sporadic cu interval minim între sosiri
//
// Apăsările vin din două surse, în aceeași coadă:
//   - butonul BOOT, prin rutina de întrerupere (ca în exemplul 03)
//   - tasta b din Serial Monitor, pusă în coadă din loop(), fără blocare
//     (timeout 0), așa cum command_received() pune o comandă din MQTT
//
// Taskul sporadic preia apăsările și le acceptă doar dacă de la ultima apăsare
// acceptată a trecut cel puțin intervalul minim între sosiri
// (kMinInterarrivalMs). Celelalte sunt respinse, cu un mesaj. Așa dispar și
// întreruperile în plus produse de bounce, fără niciun filtru în ISR.
// La fiecare apăsare acceptată, LED-ul plăcii își schimbă starea.
//
// Momentul sosirii se ia când apăsarea intră în coadă (în ISR sau în loop())
// și călătorește în elementul cozii. Dacă taskul ar citi ceasul abia când
// preia elementul, ar măsura când a ajuns el să-l trateze, nu când a sosit.
//
// Ce faceți: apăsați BOOT rar, apoi des; tastați b, apoi bbbb + Enter.

const int BUTTON_PIN = 9;
const int LED_PIN = 8;                     // LED-ul albastru: aprins pe LOW
const uint32_t kMinInterarrivalMs = 1000;  // intervalul minim între sosiri

struct Press {
    uint32_t at_ms;   // momentul sosirii, ms de la pornire
    bool from_board;  // true = BOOT (ISR), false = tasta b
};

static QueueHandle_t g_queue = NULL;

void IRAM_ATTR button_isr(void) {
    Press p = {(uint32_t)(xTaskGetTickCountFromISR() * portTICK_PERIOD_MS), true};
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(g_queue, &p, &woken);
    portYIELD_FROM_ISR(woken);
}

static void sporadic_task(void* arg) {
    (void)arg;
    bool any = false;         // a existat deja o apăsare acceptată?
    uint32_t last_ms = 0;     // momentul ultimei apăsări acceptate
    bool led_on = false;
    for (;;) {
        Press p;
        xQueueReceive(g_queue, &p, portMAX_DELAY);  // o apăsare eliberează un job
        const char* src = p.from_board ? "BOOT" : "tasta b";
        uint32_t since = p.at_ms - last_ms;

        if (any && since < kMinInterarrivalMs) {
            Serial.printf("[sporadic] %-7s t = %6lu ms  RESPINSĂ: la %lu ms după cea acceptată\n", src,
                          (unsigned long)p.at_ms, (unsigned long)since);
            continue;
        }
        any = true;
        last_ms = p.at_ms;
        led_on = !led_on;
        digitalWrite(LED_PIN, led_on ? LOW : HIGH);
        Serial.printf("[sporadic] %-7s t = %6lu ms  ACCEPTATĂ, LED %s\n", src, (unsigned long)p.at_ms,
                      led_on ? "aprins" : "stins");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);
    delay(200);
    Serial.println();
    Serial.println("=== tema 03, exemplul 04: sporadic_interval_minim ===");
    Serial.printf("apăsați BOOT sau tastați b; interval minim între sosiri %lu ms\n",
                  (unsigned long)kMinInterarrivalMs);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);  // LED stins
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    g_queue = xQueueCreate(10, sizeof(Press));

    xTaskCreate(sporadic_task, "sporadic", 4096, NULL, 3, NULL);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_isr, FALLING);
}

void loop() {
    while (Serial.available() > 0) {
        if (Serial.read() == 'b') {
            Press p = {millis(), false};
            if (xQueueSend(g_queue, &p, 0) != pdTRUE) Serial.println("[loop] coada plină, apăsare pierdută");
        }
    }
    delay(20);
}
