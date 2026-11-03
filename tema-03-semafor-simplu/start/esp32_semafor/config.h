// Setările semaforului și numele topic-urilor MQTT.
//
// Brokerul este partajat de toți studenții. Înlocuiți STUDENT cu numele vostru
// (fără spații sau diacritice), identic cu Config.STUDENT din dashboard.

#pragma once

#include <Arduino.h>

// Rețeaua Wi-Fi. ESP32-C3 merge doar pe 2.4 GHz. "iot-demo" este rețeaua din
// laborator; acasă puneți aici rețeaua voastră sau hotspot-ul telefonului.
const char* const WIFI_SSID = "iot-demo";
const char* const WIFI_PASSWORD = "demo123!";

const char* const BROKER_URI = "mqtt://control.aut.utcluj.ro:11188";

// serverul de la care placa ia ora exactă (SNTP), pentru timestamp-uri
const char* const NTP_SERVER = "pool.ntp.org";

const char* const STUDENT = "popescu-ion";

// ID-ul device-ului, folosit în topic-uri și în mesaje: "esp32-" urmat de
// ultimele 6 cifre hexa ale adresei MAC a plăcii, de exemplu "esp32-a1b2c3".
// Se citește la pornire (connection.cpp) și se afișează pe Serial.
extern const char* const DEVICE_ID;

// Pinii plăcii ESP32-C3 Super Mini
const int BUTTON_PIN = 9;  // butonul BOOT: LOW cât timp este apăsat (INPUT_PULLUP)
const int LED_PIN = 8;     // LED-ul albastru de pe placă: aprins pe LOW

// Perioadele celor două taskuri periodice
const uint32_t CONTROL_PERIOD_MS = 100;  // taskul de control: fazele semaforului
const uint32_t OUTPUT_PERIOD_MS = 500;   // taskul de ieșire: lămpile, publicate pe "lights"

// Duratele fazelor, în ms (vezi README.md)
const uint32_t CAR_GREEN_MIN_MS = 10000;  // CAR_GREEN: verdele minim pentru mașini
const uint32_t CAR_YELLOW_MS = 3000;      // CAR_YELLOW
const uint32_t ALL_RED_MS = 2000;         // ALL_RED: roșu pentru toți, înainte de pietoni
const uint32_t PED_GREEN_MS = 8000;       // PED_GREEN: verde pentru pietoni

// intervalul minim între sosiri pentru taskul sporadic (apăsările butonului)
const uint32_t MIN_INTERARRIVAL_MS = 1000;

// Scrie în buf topic-ul "ssatr/<STUDENT>/devices/<DEVICE_ID>/<kind>". kind este:
//   "lights"    starea lămpilor: "deviceId;timestamp;faza;masini;pietoni;asteptati;secunde"
//   "commands"  comenzi de la dashboard: "BUTTON"
//
//   char topic[80];
//   make_topic(topic, sizeof(topic), "lights");
inline void make_topic(char* buf, size_t size, const char* kind) {
    snprintf(buf, size, "ssatr/%s/devices/%s/%s", STUDENT, DEVICE_ID, kind);
}
