// Conexiunea plăcii: Wi-Fi, ora exactă (SNTP) și clientul MQTT (gata implementată).
//
// Codul pornește de la mqtt_hello din laboratorul 0 (rt-labs-esp32c3). Clientul
// MQTT este cel din ESP-IDF, inclus în core-ul Arduino: nu trebuie instalată
// nicio bibliotecă. El rulează în propriul task FreeRTOS și apelează
// on_mqtt_event() pentru tot ce se întâmplă: conectare, deconectare, mesaj primit.

#include "connection.h"

#include <WiFi.h>
#include <esp_mac.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "mqtt_client.h"

// "esp32-" + ultimii 3 octeți ai adresei MAC (partea proprie fiecărei plăci).
// Adresa vine din eFuse, deci se poate citi înainte de pornirea Wi-Fi-ului.
static const char* read_device_id(void) {
    static char id[16];
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(id, sizeof(id), "esp32-%02x%02x%02x", mac[3], mac[4], mac[5]);
    return id;
}

// inițializată înainte de setup()
const char* const DEVICE_ID = read_device_id();

static esp_mqtt_client_handle_t g_client = NULL;
static char g_command_topic[80];

void wifi_connect(void) {
    Serial.printf("conectare la Wi-Fi \"%s\"", WIFI_SSID);
    WiFi.mode(WIFI_STA);  // stație: client al unui punct de acces
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    // Antena mică a plăcii Super Mini se conectează adesea mai bine la putere redusă.
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() - start > 20000) {
            // Nu s-a conectat în 20 s: afișăm ce rețele vede placa, ca să se
            // poată verifica numele rețelei (doar 2.4 GHz), apoi reîncercăm.
            Serial.printf("\nnu m-am conectat la \"%s\"; rețele vizibile:\n", WIFI_SSID);
            WiFi.disconnect();
            int n = WiFi.scanNetworks();
            for (int i = 0; i < n; i++) {
                Serial.printf("  %s (%d dBm)\n", WiFi.SSID(i).c_str(), (int)WiFi.RSSI(i));
            }
            Serial.println("verificați WIFI_SSID și WIFI_PASSWORD din config.h; reîncerc");
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            start = millis();
        }
    }
    Serial.printf("\nWi-Fi conectat, adresa IP %s\n", WiFi.localIP().toString().c_str());
}

void time_sync(void) {
    // ora UTC, fără fus orar: pentru epoch_ms() contează doar UTC
    configTime(0, 0, NTP_SERVER);
    struct tm now;
    if (getLocalTime(&now, 10000)) {  // așteaptă sincronizarea cel mult 10 s
        Serial.printf("ora sincronizată prin SNTP: %04d-%02d-%02d %02d:%02d:%02d UTC\n", now.tm_year + 1900,
                      now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);
    } else {
        Serial.println("ora NU s-a sincronizat: timestamp-urile vor număra de la pornire");
    }
}

int64_t epoch_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Apelată de taskul clientului MQTT, nu de loop() sau de taskurile voastre.
static void on_mqtt_event(void* handler_arg, esp_event_base_t base, int32_t event_id, void* event_data) {
    (void)handler_arg;  // nefolosite
    (void)base;
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;

    if (event_id == MQTT_EVENT_CONNECTED) {
        // Abonarea se reface la fiecare (re)conectare: brokerul uită
        // abonările unui client care s-a deconectat.
        esp_mqtt_client_subscribe(g_client, g_command_topic, 1);
        Serial.printf("[mqtt] conectat la %s, abonat la %s\n", BROKER_URI, g_command_topic);
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        Serial.println("[mqtt] deconectat, clientul reîncearcă singur");
    } else if (event_id == MQTT_EVENT_DATA) {
        // ev->data NU se termină cu '\0': copiem ev->data_len octeți
        // (trunchiat la COMMAND_MAX_LEN - 1) și adăugăm '\0'.
        char text[COMMAND_MAX_LEN];
        int n = ev->data_len;
        if (n > (int)COMMAND_MAX_LEN - 1) n = COMMAND_MAX_LEN - 1;
        memcpy(text, ev->data, n);
        text[n] = '\0';
        command_received(text);
    } else if (event_id == MQTT_EVENT_ERROR) {
        Serial.println("[mqtt] eroare (broker inaccesibil? portul blocat de rețea?)");
    }
}

void mqtt_start(void) {
    make_topic(g_command_topic, sizeof(g_command_topic), "commands");

    // client id unic pe broker: doi clienți cu același id se deconectează unul pe altul
    static char client_id[64];
    snprintf(client_id, sizeof(client_id), "%s-%s", STUDENT, DEVICE_ID);

    esp_mqtt_client_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.broker.address.uri = BROKER_URI;
    cfg.credentials.client_id = client_id;

    g_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(g_client, MQTT_EVENT_ANY, on_mqtt_event, NULL);
    esp_mqtt_client_start(g_client);  // se conectează în fundal
}

bool mqtt_publish(const char* kind, const char* text) {
    if (g_client == NULL) {
        Serial.println("[mqtt] mqtt_start() nu a fost apelat, mesaj pierdut");
        return false;
    }
    char topic[80];
    make_topic(topic, sizeof(topic), kind);
    // lungime 0: clientul folosește strlen(text); QoS 1, fără retain.
    // Dacă placa nu este conectată încă, mesajul așteaptă în clientul MQTT.
    int id = esp_mqtt_client_publish(g_client, topic, text, 0, 1, 0);
    if (id < 0) {
        Serial.printf("[mqtt] publicarea pe %s a eșuat\n", topic);
        return false;
    }
    return true;
}
