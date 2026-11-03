// Conexiunea plăcii: Wi-Fi, ora exactă (SNTP) și clientul MQTT.
// Gata implementată (connection.cpp), nu trebuie modificată: este
// aceeași ca în tema 02 (echivalentul clasei MqttConnection din tema 01).
//
// Ordinea apelurilor, în setup():
//
//   wifi_connect();   // blochează până când placa este în rețea
//   time_sync();      // după Wi-Fi: ora exactă, pentru epoch_ms()
//   ...               // mutexul și coada voastră (TODO 0 a)
//   mqtt_start();     // după cozi: comenzile primite ajung în command_received()
//   ...               // taskurile voastre (TODO 0 b): publică prin mqtt_publish()

#pragma once

#include <Arduino.h>

// lungimea maximă a unei comenzi primite, cu tot cu '\0'; textul mai lung este trunchiat
const size_t COMMAND_MAX_LEN = 64;

// Conectează placa la rețeaua WIFI_SSID din config.h. Blochează până la
// conectare; dacă durează, afișează rețelele pe care le vede placa.
void wifi_connect(void);

// Sincronizează ceasul plăcii prin SNTP (NTP_SERVER din config.h).
// Se apelează după wifi_connect().
void time_sync(void);

// Timpul curent în milisecunde de la 1 ianuarie 1970 (Unix epoch), aceeași
// valoare ca System.currentTimeMillis() în Java. Corect după time_sync();
// înainte numără de la pornire. Se tipărește cu %lld:
//
//   Serial.printf("%lld\n", (long long)epoch_ms());
int64_t epoch_ms(void);

// Pornește clientul MQTT. Conectarea se face în fundal; la fiecare
// (re)conectare, clientul se abonează la topic-ul "commands" al device-ului.
// Pentru fiecare comandă primită apelează command_received().
void mqtt_start(void);

// Publică text pe topic-ul device-ului de tipul kind (aici, doar "lights"),
// cu QoS 1. Se poate apela din orice task, după mqtt_start(); dacă
// placa nu s-a conectat încă la broker, mesajul așteaptă în clientul MQTT.
// Întoarce false dacă mesajul nu a putut fi trimis.
//
//   mqtt_publish("lights", "esp32-a1b2c3;1727600000000;CAR_GREEN;GREEN;RED;0;0");
bool mqtt_publish(const char* kind, const char* text);

// Apelată de clientul MQTT pentru fiecare comandă primită, de exemplu
// "BUTTON". O scrieți voi, în esp32_semafor.ino.
//
// Rulează pe taskul clientului MQTT, nu pe taskurile voastre: doar puneți
// comanda în coadă, nu o executați aici. text este valid doar în timpul
// apelului, deci se copiază (xQueueSend copiază elementul cozii).
void command_received(const char* text);
