package ssatr.service;

import java.util.concurrent.BlockingQueue;

/**
 * Preia mesajele de un anumit tip ("telemetry", "alarm" sau "status") din coada
 * inbox, le salvează și actualizează dashboard-ul.
 *
 * Formatul mesajelor (câmpuri separate prin ";"):
 *   telemetry:  deviceId;timestamp;temperatura
 *   alarm:      deviceId;timestamp;temperatura;prag
 *   status:     deviceId;timestamp;text
 */
public class MessageProcessor implements Runnable {

    private final String kind;
    private final BlockingQueue<String> inbox;
    private final CsvStorage storage;
    private final Dashboard dashboard;

    public MessageProcessor(String kind, BlockingQueue<String> inbox, CsvStorage storage, Dashboard dashboard) {
        this.kind = kind;
        this.inbox = inbox;
        this.storage = storage;
        this.dashboard = dashboard;
    }

    @Override
    public void run() {
        try {
            for (;;) {
                String message = inbox.take();

                storage.append(kind, message);

                try {
                    dispatch(message);
                } catch (RuntimeException e) {
                    System.out.println("Mesaj " + kind + " invalid, ignorat: " + message);
                }
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    /** Interpretează mesajul în funcție de tipul lui (kind) și actualizează dashboard-ul. */
    private void dispatch(String message) {
        switch (kind) {
            case "telemetry": {
                String[] parts = message.split(";", 3);
                String deviceId = parts[0];
                long timestamp = Long.parseLong(parts[1]);
                double temperature = Double.parseDouble(parts[2]);
                dashboard.updateTelemetry(deviceId, timestamp, temperature);
                break;
            }
            case "alarm": {
                String[] parts = message.split(";", 4);
                String deviceId = parts[0];
                long timestamp = Long.parseLong(parts[1]);
                double temperature = Double.parseDouble(parts[2]);
                double threshold = Double.parseDouble(parts[3]);
                dashboard.updateAlarm(deviceId, timestamp, temperature);
                System.out.printf("[ALARMA] %s: temperatura %.2f > prag %.2f%n", deviceId, temperature, threshold);
                break;
            }
            case "status": {
                String[] parts = message.split(";", 3);
                String deviceId = parts[0];
                long timestamp = Long.parseLong(parts[1]);
                String text = parts[2];
                dashboard.touch(deviceId, timestamp);
                System.out.printf("[STATUS] %s: %s%n", deviceId, text);
                break;
            }
            default:
                throw new IllegalStateException("tip de mesaj necunoscut: " + kind);
        }
    }
}
