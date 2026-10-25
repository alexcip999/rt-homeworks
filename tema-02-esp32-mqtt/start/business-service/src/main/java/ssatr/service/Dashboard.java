package ssatr.service;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Ultima stare cunoscută a fiecărui device: ultima temperatură, momentul
 * ultimului mesaj, numărul de alarme etc.
 *
 * Este actualizată de thread-urile MessageProcessor și citită de CommandConsole,
 * deci accesul trebuie sincronizat.
 */
public class Dashboard {

    private static class DeviceState {
        double lastTemperature;
        long lastMessageAt;
        int alarmCount;
    }

    private final Map<String, DeviceState> devices = new LinkedHashMap<>();

    public synchronized void updateTelemetry(String deviceId, long timestamp, double temperature) {
        DeviceState state = devices.computeIfAbsent(deviceId, id -> new DeviceState());
        state.lastTemperature = temperature;
        state.lastMessageAt = timestamp;
    }

    public synchronized void updateAlarm(String deviceId, long timestamp, double temperature) {
        DeviceState state = devices.computeIfAbsent(deviceId, id -> new DeviceState());
        state.lastTemperature = temperature;
        state.lastMessageAt = timestamp;
        state.alarmCount++;
    }

    public synchronized void touch(String deviceId, long timestamp) {
        DeviceState state = devices.computeIfAbsent(deviceId, id -> new DeviceState());
        state.lastMessageAt = timestamp;
    }

    public synchronized void print() {
        if (devices.isEmpty()) {
            System.out.println("(niciun device nu a trimis încă vreun mesaj)");
            return;
        }

        System.out.printf("%-12s %8s %16s %8s%n", "DEVICE", "TEMP", "ULTIMUL MESAJ", "ALARME");

        long now = System.currentTimeMillis();
        for (Map.Entry<String, DeviceState> entry : devices.entrySet()) {
            DeviceState state = entry.getValue();
            System.out.printf("%-12s %8.2f %16s %8d%n",
                    entry.getKey(), state.lastTemperature, formatAge(now - state.lastMessageAt), state.alarmCount);
        }
    }

    private static String formatAge(long millis) {
        long seconds = millis / 1000;
        if (seconds < 60) {
            return "acum " + seconds + " s";
        }
        long minutes = seconds / 60;
        return "acum " + minutes + " min";
    }
}
