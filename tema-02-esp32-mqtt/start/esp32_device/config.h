// Setările device-ului și numele topic-urilor MQTT.
//
// Brokerul este partajat de toți studenții. Înlocuiți STUDENT cu numele vostru
// (fără spații sau diacritice), identic cu Config.STUDENT din business-service.

#pragma once

#include <Arduino.h>

// Rețeaua Wi-Fi. ESP32-C3 merge doar pe 2.4 GHz. "iot-demo" este rețeaua din
// laborator; acasă puneți aici rețeaua voastră sau hotspot-ul telefonului.
const char* const WIFI_SSID = "iot-demo";
const char* const WIFI_PASSWORD = "demo123!";

const char* const BROKER_URI = "mqtt://control.aut.utcluj.ro:11188";

// serverul de la care placa ia ora exactă (SNTP), pentru timestamp-uri
const char* const NTP_SERVER = "pool.ntp.org";

const char* const STUDENT = "constantin-alexandru";

// ID-ul device-ului, folosit în topic-uri și în mesaje: "esp32-" urmat de
// ultimele 6 cifre hexa ale adresei MAC a plăcii, de exemplu "esp32-a1b2c3".
// Fiecare placă are astfel alt ID, fără nicio setare. Se citește la pornire
// (connection.cpp) și se afișează pe Serial; se folosește ca un șir constant.
extern const char* const DEVICE_ID;

// Valorile de pornire, schimbate prin comenzi (SET_INTERVAL, SET_THRESHOLD)
// și readuse la acestea prin RESET.
const uint32_t PERIOD_MS_INITIAL = 2000;  // perioada T a taskului periodic
const float THRESHOLD_INITIAL = 45.0f;    // pragul de alarmă, în °C (vezi read_temperature())

// intervalul minim între sosiri pentru taskul sporadic
const uint32_t MIN_INTERARRIVAL_MS = 5000;

// Scrie în buf topic-ul "ssatr/<STUDENT>/devices/<DEVICE_ID>/<kind>". kind este:
//   "telemetry"  date periodice: "deviceId;timestamp;temperatura"
//   "alarm"      alarme (sporadice): "deviceId;timestamp;temperatura;prag"
//   "commands"   comenzi de la business service: "SET_INTERVAL 2000" etc.
//   "status"     confirmări după executarea unei comenzi: "deviceId;timestamp;text"
//
//   char topic[80];
//   make_topic(topic, sizeof(topic), "telemetry");
inline void make_topic(char* buf, size_t size, const char* kind) {
    snprintf(buf, size, "ssatr/%s/devices/%s/%s", STUDENT, DEVICE_ID, kind);
}
