package ssatr.semafor;

/**
 * Setările dashboard-ului și numele topic-urilor MQTT.
 *
 * Brokerul este partajat de toți studenții. Înlocuiți STUDENT cu numele vostru
 * (fără spații sau diacritice), identic cu STUDENT din config.h al plăcii.
 */
public class Config {

    public static final String BROKER = "tcp://control.aut.utcluj.ro:11188";

    public static final String STUDENT = "popescu-ion";

    private static final String DEVICES = "ssatr/" + STUDENT + "/devices/";

    /** starea lămpilor, publicată de placă: "deviceId;timestamp;faza;masini;pietoni;asteptati;secunde" */
    public static String lightsTopic(String deviceId) {
        return DEVICES + deviceId + "/lights";
    }

    /** comenzi de la dashboard către placă: "BUTTON" */
    public static String commandTopic(String deviceId) {
        return DEVICES + deviceId + "/commands";
    }

    /** după cât timp fără mesaje "lights" este considerată placa inactivă */
    public static final long STALE_MS = 2000;

    /** cât așteaptă dashboard-ul, după BUTTON, aprinderea lămpii AȘTEPTAȚI */
    public static final long BUTTON_TIMEOUT_MS = 5000;
}
