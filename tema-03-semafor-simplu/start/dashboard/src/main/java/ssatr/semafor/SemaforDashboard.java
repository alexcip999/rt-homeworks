package ssatr.semafor;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.FlowLayout;
import java.awt.Font;
import java.awt.GridLayout;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.function.BiConsumer;

import javax.swing.BorderFactory;
import javax.swing.JButton;
import javax.swing.JComboBox;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;
import javax.swing.SwingUtilities;
import javax.swing.Timer;

/**
 * Dashboard-ul trecerii de pietoni (aplicație gata implementată).
 *
 * Rulare:  mvn -q compile exec:java
 *
 * Dashboard-ul este "hardware-ul" semaforului: lămpile desenate se aprind și se
 * sting doar după mesajele "lights" publicate de placă. Toată logica (fazele,
 * butonul) este pe placă. Dashboard-ul:
 *  - desenează lămpile, lampa AȘTEPTAȚI și numărătoarea inversă;
 *  - verifică regulile de siguranță (verde simultan, verde → roșu fără galben);
 *  - trimite plăcii comanda BUTTON (butonul virtual pentru pietoni);
 *  - măsoară cât durează de la BUTTON până la aprinderea lămpii AȘTEPTAȚI.
 */
public class SemaforDashboard {

    private static final DateTimeFormatter CLOCK = DateTimeFormatter.ofPattern("HH:mm:ss.SSS");

    private final MqttConnection mqtt;

    // folosite doar pe thread-ul interfeței grafice (EDT)
    private final Map<String, DeviceState> devices = new LinkedHashMap<>();

    private final JComboBox<String> deviceBox = new JComboBox<>();
    private final LightsPanel lightsPanel = new LightsPanel();
    private final JLabel phaseLabel = new JLabel();
    private final JLabel lastMessageLabel = new JLabel();
    private final JLabel buttonLabel = new JLabel();
    private final JLabel safetyLabel = new JLabel();
    private final JTextArea log = new JTextArea(10, 60);

    public SemaforDashboard(MqttConnection mqtt) {
        this.mqtt = mqtt;
    }

    public static void main(String[] args) throws Exception {
        MqttConnection mqtt = new MqttConnection(Config.STUDENT + "-semafor-dashboard");
        mqtt.connect();

        BlockingQueue<String> lightsInbox = new LinkedBlockingQueue<>();
        // "+" înseamnă "orice device"
        mqtt.subscribe(Config.lightsTopic("+"), lightsInbox);

        SemaforDashboard dashboard = new SemaforDashboard(mqtt);
        SwingUtilities.invokeAndWait(dashboard::createWindow);

        // Mesajele se preiau din cozi pe thread-uri separate și se predau
        // interfeței grafice, care le prelucrează pe EDT.
        drain("lights", lightsInbox, dashboard::onLights);
    }

    private static void drain(String name, BlockingQueue<String> inbox, BiConsumer<String, Long> handler) {
        Thread t = new Thread(() -> {
            try {
                while (true) {
                    String text = inbox.take();
                    long receivedAt = System.currentTimeMillis();
                    SwingUtilities.invokeLater(() -> handler.accept(text, receivedAt));
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }, name);
        t.setDaemon(true);
        t.start();
    }

    // ---------------------------------------------------------------- fereastra

    private void createWindow() {
        JFrame frame = new JFrame("Semafor pentru pietoni – " + Config.STUDENT);
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

        JPanel top = new JPanel(new FlowLayout(FlowLayout.LEFT));
        top.add(new JLabel("Device:"));
        deviceBox.setPrototypeDisplayValue("esp32-xxxxxx-xxxx");
        deviceBox.addActionListener(e -> refresh());
        top.add(deviceBox);
        phaseLabel.setFont(phaseLabel.getFont().deriveFont(Font.BOLD, 18f));
        top.add(phaseLabel);

        JPanel info = new JPanel(new GridLayout(3, 1));
        info.setBorder(BorderFactory.createEmptyBorder(4, 8, 4, 8));
        info.add(lastMessageLabel);
        info.add(buttonLabel);
        info.add(safetyLabel);

        JPanel buttons = new JPanel(new FlowLayout(FlowLayout.LEFT));
        buttons.add(commandButton("Buton pietoni", "BUTTON"));

        JPanel center = new JPanel(new BorderLayout());
        center.add(lightsPanel, BorderLayout.CENTER);
        center.add(info, BorderLayout.SOUTH);

        log.setEditable(false);
        log.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 12));
        JPanel bottom = new JPanel(new BorderLayout());
        bottom.add(buttons, BorderLayout.NORTH);
        bottom.add(new JScrollPane(log), BorderLayout.CENTER);

        frame.add(top, BorderLayout.NORTH);
        frame.add(center, BorderLayout.CENTER);
        frame.add(bottom, BorderLayout.SOUTH);
        frame.pack();
        frame.setLocationRelativeTo(null);
        frame.setVisible(true);

        // ceasul interfeței: vechimea ultimului mesaj, placa inactivă, timeout la BUTTON
        new Timer(200, e -> refresh()).start();

        log("dashboard pornit; așteptăm mesaje pe " + Config.lightsTopic("+"));
        refresh();
    }

    private JButton commandButton(String text, String command) {
        JButton b = new JButton(text);
        b.addActionListener(e -> send(command));
        return b;
    }

    // ---------------------------------------------------------------- mesaje

    private void onLights(String text, long receivedAt) {
        DeviceState.Lights m;
        try {
            m = DeviceState.Lights.parse(text);
        } catch (IllegalArgumentException e) {
            log("mesaj lights invalid (" + e.getMessage() + "): " + text);
            return;
        }

        DeviceState s = devices.get(m.deviceId());
        if (s == null) {
            s = new DeviceState(m.deviceId());
            devices.put(m.deviceId(), s);
            deviceBox.addItem(m.deviceId());  // primul device adăugat este selectat automat
            log("device nou: " + m.deviceId());
        }

        String before = s.phase;
        List<String> errors = s.update(m, receivedAt);
        if (!before.equals(s.phase)) {
            log("[" + s.deviceId + "] faza " + s.phase + "  (mașini " + s.car + ", pietoni " + s.ped + ")");
        }
        for (String error : errors) {
            log("[" + s.deviceId + "] EROARE DE SIGURANȚĂ: " + error);
        }
        refresh();
    }

    // ---------------------------------------------------------------- comenzi

    private void send(String command) {
        DeviceState s = selected();
        if (s == null) {
            log("niciun device selectat: comanda " + command + " nu a fost trimisă");
            return;
        }
        mqtt.publish(Config.commandTopic(s.deviceId), command);
        log("[" + s.deviceId + "] → " + command);
        if (command.equals("BUTTON")) {
            s.buttonSent(System.currentTimeMillis());
        }
        refresh();
    }

    // ---------------------------------------------------------------- afișare

    private DeviceState selected() {
        Object id = deviceBox.getSelectedItem();
        return id == null ? null : devices.get(id.toString());
    }

    private void refresh() {
        long now = System.currentTimeMillis();
        DeviceState s = selected();
        if (s != null) {
            s.checkButtonTimeout(now);
        }
        boolean stale = s == null || s.isStale(now);
        lightsPanel.show(s, stale);

        if (s == null) {
            phaseLabel.setText("");
            lastMessageLabel.setText("niciun device");
            buttonLabel.setText(" ");
            safetyLabel.setText(" ");
            return;
        }

        phaseLabel.setText("   faza: " + s.phase);
        lastMessageLabel.setText(String.format("ultimul mesaj: acum %.1f s   (%d mesaje)",
                (now - s.lastReceived) / 1000.0, s.messages));
        lastMessageLabel.setForeground(stale ? new Color(0xd70015) : Color.DARK_GRAY);
        buttonLabel.setText("buton pietoni: " + (s.buttonResult == null ? "-" : s.buttonResult));
        if (s.safetyErrors == 0) {
            safetyLabel.setText("siguranță: fără erori");
            safetyLabel.setForeground(new Color(0x248a3d));
        } else {
            String count = s.safetyErrors == 1 ? "o eroare" : s.safetyErrors + " erori";
            safetyLabel.setText("siguranță: " + count + ", ultima: " + s.lastError);
            safetyLabel.setForeground(new Color(0xd70015));
        }
    }

    private void log(String text) {
        log.append(LocalTime.now().format(CLOCK) + "  " + text + "\n");
        log.setCaretPosition(log.getDocument().getLength());
    }
}
