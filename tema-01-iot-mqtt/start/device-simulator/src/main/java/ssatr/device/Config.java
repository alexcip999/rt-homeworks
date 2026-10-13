package ssatr.device;

/**
 * Setările aplicației și numele topic-urilor MQTT.
 *
 * Brokerul este partajat de toți studenții. Înlocuiți STUDENT cu numele vostru
 * (fără spații sau diacritice), identic în ambele aplicații.
 */
public class Config {

    public static final String BROKER = "tcp://control.aut.utcluj.ro:11188";

    public static final String STUDENT = "constantin-alexandru";

    private static final String DEVICES = "ssatr/" + STUDENT + "/devices/";

    /** date periodice: "deviceId;timestamp;temperatura" */
    public static String telemetryTopic(String deviceId) {
        return DEVICES + deviceId + "/telemetry";
    }

    /** alarme (sporadice): "deviceId;timestamp;temperatura;prag" */
    public static String alarmTopic(String deviceId) {
        return DEVICES + deviceId + "/alarm";
    }

    /** comenzi de la business service către device: "SET_INTERVAL 2000" etc. */
    public static String commandTopic(String deviceId) {
        return DEVICES + deviceId + "/commands";
    }

    /** confirmări trimise de device după executarea unei comenzi: "deviceId;timestamp;text" */
    public static String statusTopic(String deviceId) {
        return DEVICES + deviceId + "/status";
    }
}
