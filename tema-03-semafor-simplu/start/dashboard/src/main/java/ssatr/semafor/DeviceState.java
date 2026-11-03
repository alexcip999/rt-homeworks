package ssatr.semafor;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Ultima stare cunoscută a unui semafor, plus verificările de siguranță.
 *
 * Mesajul "lights" are 7 câmpuri separate prin ";":
 *   deviceId;timestamp;faza;masini;pietoni;asteptati;secunde
 *   esp32-a1b2c3;1727600000000;PED_GREEN;RED;GREEN;0;7
 *
 * Obiectul este folosit doar pe thread-ul interfeței grafice (EDT), deci nu
 * are nevoie de sincronizare.
 */
public class DeviceState {

    static final Set<String> PHASES =
            Set.of("CAR_GREEN", "CAR_YELLOW", "ALL_RED", "PED_GREEN", "PED_FLASH", "PED_CLEAR", "NIGHT");
    static final Set<String> CAR_COLORS = Set.of("RED", "YELLOW", "GREEN", "OFF");
    static final Set<String> PED_COLORS = Set.of("RED", "GREEN", "OFF");

    final String deviceId;

    String phase = "?";
    String car = "OFF";
    String ped = "OFF";
    boolean wait;
    int seconds;

    /** momentul (System.currentTimeMillis) la care a sosit ultimul mesaj; 0 = niciun mesaj */
    long lastReceived;
    long messages;

    int safetyErrors;
    String lastError;

    /** momentul la care s-a trimis BUTTON, cât timp se așteaptă lampa AȘTEPTAȚI; 0 = nu se așteaptă */
    long buttonSentAt;
    String buttonResult;

    DeviceState(String deviceId) {
        this.deviceId = deviceId;
    }

    /** Câmpurile unui mesaj "lights", după validare. */
    record Lights(String deviceId, long timestamp, String phase, String car, String ped, boolean waitLamp, int seconds) {

        /** @throws IllegalArgumentException dacă mesajul nu respectă formatul */
        static Lights parse(String text) {
            String[] f = text.trim().split(";");
            if (f.length != 7) {
                throw new IllegalArgumentException("are " + f.length + " câmpuri în loc de 7");
            }
            long timestamp;
            int seconds;
            try {
                timestamp = Long.parseLong(f[1]);
                seconds = Integer.parseInt(f[6]);
            } catch (NumberFormatException e) {
                throw new IllegalArgumentException("timestamp sau secunde nu sunt numere");
            }
            check(PHASES, f[2], "faza");
            check(CAR_COLORS, f[3], "culoarea pentru mașini");
            check(PED_COLORS, f[4], "culoarea pentru pietoni");
            if (!f[5].equals("0") && !f[5].equals("1")) {
                throw new IllegalArgumentException("asteptati trebuie să fie 0 sau 1, nu " + f[5]);
            }
            if (seconds < 0) {
                throw new IllegalArgumentException("secunde negativ: " + seconds);
            }
            return new Lights(f[0], timestamp, f[2], f[3], f[4], f[5].equals("1"), seconds);
        }

        private static void check(Set<String> allowed, String value, String what) {
            if (!allowed.contains(value)) {
                throw new IllegalArgumentException(what + " necunoscută: " + value);
            }
        }
    }

    /**
     * Aplică un mesaj nou și întoarce erorile de siguranță găsite (lista e goală
     * dacă totul este în regulă).
     */
    List<String> update(Lights m, long receivedAt) {
        List<String> errors = new ArrayList<>();

        // pietonii pot avea verde doar când mașinile au roșu
        if (m.ped().equals("GREEN") && !m.car().equals("RED")) {
            errors.add("CONFLICT: pietoni GREEN, mașini " + m.car());
        }
        // la mașini, verdele trece în roșu doar prin galben
        if (messages > 0 && car.equals("GREEN") && m.car().equals("RED")) {
            errors.add("mașini: GREEN → RED fără YELLOW");
        }

        if (buttonSentAt > 0 && m.waitLamp()) {
            buttonResult = "AȘTEPTAȚI aprinsă după " + (receivedAt - buttonSentAt) + " ms";
            buttonSentAt = 0;
        }

        phase = m.phase();
        car = m.car();
        ped = m.ped();
        wait = m.waitLamp();
        seconds = m.seconds();
        lastReceived = receivedAt;
        messages++;

        if (!errors.isEmpty()) {
            safetyErrors += errors.size();
            lastError = errors.get(errors.size() - 1);
        }
        return errors;
    }

    /** Apelată după ce s-a trimis BUTTON: de acum se măsoară timpul până la aprinderea lămpii AȘTEPTAȚI. */
    void buttonSent(long now) {
        buttonSentAt = now;
        buttonResult = "se așteaptă lampa AȘTEPTAȚI...";
    }

    /** Verifică dacă lampa AȘTEPTAȚI nu s-a aprins în BUTTON_TIMEOUT_MS. */
    void checkButtonTimeout(long now) {
        if (buttonSentAt > 0 && now - buttonSentAt > Config.BUTTON_TIMEOUT_MS) {
            buttonResult = "AȘTEPTAȚI nu s-a aprins în " + Config.BUTTON_TIMEOUT_MS / 1000 + " s";
            buttonSentAt = 0;
        }
    }

    boolean isStale(long now) {
        return lastReceived == 0 || now - lastReceived > Config.STALE_MS;
    }
}
