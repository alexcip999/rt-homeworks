# SSATR – Teme de laborator

Teme pentru disciplina **Structuri Software pentru Aplicații de Timp Real (SSATR)**, program de master.

Fiecare temă se află într-un subfolder separat și conține enunțul (`README.md`) și, opțional, exemple și un proiect de start.

## Teme

| Nr. | Temă | Tehnologii |
|-----|------|------------|
| 01  | [Aplicație IoT cu taskuri periodice, sporadice și aperiodice](tema-01-iot-mqtt/) | Java, MQTT, diagrame UML |
| 02  | [Device IoT pe ESP32-C3 cu taskuri FreeRTOS](tema-02-esp32-mqtt/) | ESP32-C3, Arduino, FreeRTOS, MQTT |
| 03  | [Semafor pentru pietoni pe ESP32-C3](tema-03-semafor-simplu/) | ESP32-C3, Arduino, FreeRTOS, MQTT, Java |
| 04  | | |

## Tehnologii folosite

- **Java** – programare concurentă, fire de execuție, planificare, sincronizare
- **Arduino / ESP32** – sisteme embedded, FreeRTOS, senzori și actuatori
- **Linux cu kernel RT** (`PREEMPT_RT`) – latență, priorități, politici de planificare
- **Servicii de mesagerie** – RabbitMQ, Kafka, MQTT etc.
- **Diagrame** – UML (secvență, stări, activitate, componente), rețele Petri, diagrame de timp

## Unelte necesare

- **Java** JDK 17 – 21
- **IDE**: IntelliJ IDEA sau NetBeans
- **Maven** (inclus în IntelliJ și NetBeans)
- **Arduino IDE**
- **git**
- cont **GitHub**
- **Docker** și **Docker Compose**
- **MQTTX** 
- **draw.io**
- client **SSH**
- placă de dezvoltare **ESP32-C3 Super Mini** și cablu USB-C de date

## Structura unei teme

```
tema-NN-nume-scurt/
├── README.md     # enunțul temei (obligatoriu)
├── exemple/      # cod sau diagrame demonstrative (opțional)
└── start/        # proiect de pornire pentru studenți (opțional)
```

## Cum se lucrează

1. Faceți un fork al acestui repository. Lucrați în fork-ul vostru.
2. Citiți enunțul din `README.md` al temei.
3. Dacă tema are un folder `start/`, porniți de la acesta.
4. Faceți commit și push cu soluția în fork-ul vostru.

## Predare

Tema se predă în **Microsoft Teams**, la assignment-ul corespunzător temei:

1. Aflați commit ID-ul soluției finale, după push:
   ```bash
   git rev-parse HEAD
   ```
2. Adăugați commit ID-ul ca si **comentariu** la assignment.
3. Apăsați **Turn in**.

Se evaluează exact commit-ul indicat. Commit-urile făcute după predare nu sunt luate în considerare.
