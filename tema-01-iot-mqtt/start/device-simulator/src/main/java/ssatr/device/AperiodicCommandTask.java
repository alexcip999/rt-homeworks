package ssatr.device;

import java.util.Locale;
import java.util.concurrent.BlockingQueue;

/**
 * TASK APERIODIC: execută comenzile primite de la business service.
 *
 * Comenzile sosesc oricând, pe MQTT, fără un interval minim între ele, și sunt
 * puse în coada commands. Fiecare comandă eliberează un job al taskului.
 * Comenzi care trebuie suportate:
 *
 *   SET_INTERVAL <ms>       schimbă perioada T a taskului periodic
 *   SET_THRESHOLD <valoare> schimbă pragul de alarmă
 *   RESET                   readuce senzorul la valoarea inițială
 *   STATUS                  raportează perioada și pragul curente
 *
 * După executarea fiecărei comenzi, device-ul publică pe Config.statusTopic(deviceId)
 * o confirmare: "deviceId;timestamp;text", de exemplu
 * "device-1;1727600000000;OK SET_INTERVAL 1000".
 */
public class AperiodicCommandTask implements Runnable {

    private final String deviceId;
    private final TemperatureSensor sensor;
    private final MqttConnection mqtt;
    private final BlockingQueue<String> commands;
    private final PeriodicTelemetryTask periodic;

    public AperiodicCommandTask(String deviceId, TemperatureSensor sensor, MqttConnection mqtt,
                                BlockingQueue<String> commands, PeriodicTelemetryTask periodic) {
        this.deviceId = deviceId;
        this.sensor = sensor;
        this.mqtt = mqtt;
        this.commands = commands;
        this.periodic = periodic;
    }

    @Override
    public void run() {
        try {
            for (;;) {
                String command = commands.take().trim();
                if (command.isEmpty()) {
                    continue;
                }

                String reply = execute(command);
                System.out.printf("[%s] command \"%s\" -> %s%n", deviceId, command, reply);

                long timestamp = System.currentTimeMillis();
                String message = deviceId + ";" + timestamp + ";" + reply;
                mqtt.publish(Config.statusTopic(deviceId), message);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    private String execute(String command) {
        String[] parts = command.split("\\s+");
        String name = parts[0].toUpperCase(Locale.ROOT);

        try {
            switch (name) {
                case "SET_INTERVAL": {
                    long periodMs = Long.parseLong(parts[1]);
                    if (periodMs <= 0) {
                        return "ERROR SET_INTERVAL: period must be positive";
                    }
                    periodic.setPeriodMs(periodMs);
                    return "OK SET_INTERVAL " + periodMs;
                }
                case "SET_THRESHOLD": {
                    double threshold = Double.parseDouble(parts[1]);
                    if (!Double.isFinite(threshold)) {
                        return "ERROR SET_THRESHOLD: invalid value";
                    }
                    periodic.setThreshold(threshold);
                    return "OK SET_THRESHOLD " + String.format(Locale.US, "%.2f", threshold);
                }
                case "RESET":
                    sensor.reset();
                    return "OK RESET";
                case "STATUS":
                    return String.format(Locale.US, "OK STATUS period=%d threshold=%.2f",
                            periodic.getPeriodMs(), periodic.getThreshold());
                default:
                    return "ERROR unknown command: " + name;
            }
        } catch (ArrayIndexOutOfBoundsException e) {
            return "ERROR " + name + ": missing parameter";
        } catch (NumberFormatException e) {
            return "ERROR " + name + ": invalid parameter";
        }
    }
}
