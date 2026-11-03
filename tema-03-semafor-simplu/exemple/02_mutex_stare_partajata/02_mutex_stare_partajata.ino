// 02_mutex_stare_partajata - o stare partajată de două taskuri, cu și fără mutex
//
// Starea are două câmpuri care trebuie să fie mereu egale: x și y.
//
//   scriitorul  prioritate 2, rulează continuu: x = n, puțin calcul, y = n
//   cititorul   prioritate 3, periodic (T = 1 ms): copiază starea și verifică x == y
//
// Cititorul este mai prioritar: când se eliberează, preemptează scriitorul.
// Fără mutex, uneori îl prinde între cele două scrieri și copiază o stare pe
// jumătate scrisă (x nou, y vechi): o stare inconsistentă.
// Cu mutex, cititorul nu poate copia starea cât timp scriitorul o modifică:
// se blochează în xSemaphoreTake până când scriitorul eliberează mutexul.
//
// Mutexul se ține cât mai puțin: doar cât se scriu sau se copiază câmpurile.
// Cititorul lucrează apoi cu copia locală, fără mutex.
//
// Taste (în Serial Monitor, urmate de Enter):
//   m  pornește / oprește mutexul (implicit pornit)
// La fiecare 2 s: câte scrieri, câte citiri și câte stări inconsistente.

struct State {
    uint32_t x;
    uint32_t y;
};

static State g_state = {0, 0};
static SemaphoreHandle_t g_mutex = NULL;
static volatile bool g_use_mutex = true;

static volatile uint32_t g_writes = 0;
static volatile uint32_t g_reads = 0;
static volatile uint32_t g_inconsistent = 0;

static void busy_us(uint32_t us) {
    uint32_t start = micros();
    while (micros() - start < us) {
    }
}

static void writer_task(void* arg) {
    (void)arg;
    uint32_t n = 0;
    for (;;) {
        n = n + 1;
        bool use = g_use_mutex;  // aceeași decizie pentru take și give
        if (use) xSemaphoreTake(g_mutex, portMAX_DELAY);
        g_state.x = n;
        busy_us(50);  // între cele două scrieri, starea este inconsistentă
        g_state.y = n;
        if (use) xSemaphoreGive(g_mutex);
        g_writes = g_writes + 1;

        // Din când în când cedăm procesorul, ca să ruleze și taskurile mai
        // puțin prioritare (loop(), taskul idle).
        if (n % 20 == 0) vTaskDelay(1);
    }
}

static void reader_task(void* arg) {
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        bool use = g_use_mutex;
        if (use) xSemaphoreTake(g_mutex, portMAX_DELAY);
        State copy = g_state;  // în mutex doar copiem starea...
        if (use) xSemaphoreGive(g_mutex);

        // ...și lucrăm cu copia după ce l-am eliberat
        g_reads = g_reads + 1;
        if (copy.x != copy.y) g_inconsistent = g_inconsistent + 1;
        xTaskDelayUntil(&last, pdMS_TO_TICKS(1));
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);
    delay(200);
    Serial.println();
    Serial.println("=== tema 03, exemplul 02: mutex_stare_partajata ===");
    Serial.println("tasta: m = pornește / oprește mutexul");

    g_mutex = xSemaphoreCreateMutex();  // înainte de taskurile care îl folosesc

    xTaskCreate(writer_task, "scriitor", 4096, NULL, 2, NULL);
    xTaskCreate(reader_task, "cititor", 4096, NULL, 3, NULL);
}

void loop() {
    static uint32_t last_report = millis();

    while (Serial.available() > 0) {
        if (Serial.read() == 'm') {
            g_use_mutex = !g_use_mutex;
            Serial.printf("\n--- mutex %s ---\n", g_use_mutex ? "PORNIT" : "OPRIT");
            // numărăm din nou, ca raportul următor să fie doar despre modul nou
            last_report = millis();
            g_writes = 0;
            g_reads = 0;
            g_inconsistent = 0;
        }
    }

    if (millis() - last_report >= 2000) {
        last_report = millis();
        Serial.printf("mutex %-6s scrieri %7lu, citiri %5lu, stări inconsistente %5lu\n",
                      g_use_mutex ? "PORNIT" : "OPRIT", (unsigned long)g_writes, (unsigned long)g_reads,
                      (unsigned long)g_inconsistent);
        g_writes = 0;
        g_reads = 0;
        g_inconsistent = 0;
    }
    delay(50);
}
