package ssatr.device;

import java.util.Locale;
import java.util.concurrent.BlockingQueue;

/**
 * TASK SPORADIC: tratează alarmele de temperatură.
 *
 * Evenimentele sosesc în coada alarmEvents (puse de taskul periodic).
 * Intervalul minim între sosiri este MIN_INTERARRIVAL_MS: între două eliberări
 * ale taskului (două alarme publicate) trebuie să treacă cel puțin atât.
 * Intervalul este impus de cod: evenimentele care sosesc mai des sunt
 * respinse, ca să nu fie inundat brokerul.
 */
public class SporadicAlarmTask implements Runnable {

    static final long MIN_INTERARRIVAL_MS = 5000;

    private final String deviceId;
    private final MqttConnection mqtt;
    private final BlockingQueue<Double> alarmEvents;
    private final PeriodicTelemetryTask periodic;

    public SporadicAlarmTask(String deviceId, MqttConnection mqtt, BlockingQueue<Double> alarmEvents,
                             PeriodicTelemetryTask periodic) {
        this.deviceId = deviceId;
        this.mqtt = mqtt;
        this.alarmEvents = alarmEvents;
        this.periodic = periodic;
    }

    @Override
    public void run() {
        long lastRelease = 0;

        try {
            for (;;) {
                double temperature = alarmEvents.take();

                long now = System.currentTimeMillis();
                long sinceLast = now - lastRelease;
                if (sinceLast < MIN_INTERARRIVAL_MS) {
                    System.out.printf("[%s] alarm REJECTED: only %d ms passed (minimum interval %d ms)%n",
                            deviceId, sinceLast, MIN_INTERARRIVAL_MS);
                    continue;
                }
                lastRelease = now;

                String message = deviceId + ";" + now + ";"
                        + String.format(Locale.US, "%.2f", temperature) + ";"
                        + String.format(Locale.US, "%.2f", periodic.getThreshold());
                mqtt.publish(Config.alarmTopic(deviceId), message);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
