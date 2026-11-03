package ssatr.semafor;

import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.RenderingHints;

import javax.swing.JPanel;

/**
 * Desenul trecerii de pietoni: semaforul pentru mașini (stânga), trecerea de
 * pietoni (mijloc) și semaforul pentru pietoni, cu lampa AȘTEPTAȚI și
 * numărătoarea inversă (dreapta).
 *
 * Panoul doar desenează ce a primit în ultimul mesaj: nu are logică proprie,
 * iar lămpile intermitente clipesc doar dacă placa le aprinde și le stinge.
 */
public class LightsPanel extends JPanel {

    private static final Color BACKGROUND = new Color(0x2b2f33);
    private static final Color HOUSING = new Color(0x111111);
    private static final Color ROAD = new Color(0x4a4e52);

    private static final Color RED_ON = new Color(0xff3b30);
    private static final Color YELLOW_ON = new Color(0xffcc00);
    private static final Color GREEN_ON = new Color(0x34c759);
    private static final Color RED_OFF = new Color(0x3a1414);
    private static final Color YELLOW_OFF = new Color(0x3a3210);
    private static final Color GREEN_OFF = new Color(0x123a1c);

    private DeviceState state;
    private boolean stale = true;

    public LightsPanel() {
        setPreferredSize(new Dimension(640, 360));
        setBackground(BACKGROUND);
    }

    /** Desenează starea s (null = niciun device); stale = placa nu a mai trimis mesaje. */
    public void show(DeviceState s, boolean stale) {
        this.state = s;
        this.stale = stale;
        repaint();
    }

    @Override
    protected void paintComponent(Graphics g0) {
        super.paintComponent(g0);
        Graphics2D g = (Graphics2D) g0;
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);

        int w = getWidth();
        int h = getHeight();

        // drumul, cu trecerea de pietoni la mijloc
        int roadTop = h / 2 - 50;
        g.setColor(ROAD);
        g.fillRect(0, roadTop, w, 100);
        g.setColor(new Color(0xe0e0e0));
        for (int y = roadTop + 6; y < roadTop + 100 - 6; y += 16) {
            g.fillRect(w / 2 - 50, y, 100, 9);
        }

        // lămpile sunt aprinse doar dacă placa trimite mesaje
        boolean live = state != null && !stale;
        String car = live ? state.car : "OFF";
        String ped = live ? state.ped : "OFF";

        drawCarLight(g, 60, h / 2 - 135, car);
        drawPedLight(g, w - 200, h / 2 - 135, ped, live && state.wait, live ? state.seconds : 0);

        if (state == null) {
            caption(g, "așteptăm mesaje pe topic-ul lights...", w / 2, h - 20);
        } else if (stale) {
            caption(g, "placa nu mai trimite mesaje", w / 2, h - 20);
        }
    }

    private void drawCarLight(Graphics2D g, int x, int y, String car) {
        int r = 34;
        g.setColor(HOUSING);
        g.fillRoundRect(x, y, 2 * r + 24, 6 * r + 48, 24, 24);
        lamp(g, x + 12, y + 12, r, car.equals("RED") ? RED_ON : RED_OFF);
        lamp(g, x + 12, y + 24 + 2 * r, r, car.equals("YELLOW") ? YELLOW_ON : YELLOW_OFF);
        lamp(g, x + 12, y + 36 + 4 * r, r, car.equals("GREEN") ? GREEN_ON : GREEN_OFF);
        label(g, "mașini", x + r + 12, y + 6 * r + 70);
    }

    private void drawPedLight(Graphics2D g, int x, int y, String ped, boolean wait, int seconds) {
        int r = 34;
        g.setColor(HOUSING);
        g.fillRoundRect(x, y, 2 * r + 24, 4 * r + 36, 24, 24);
        lamp(g, x + 12, y + 12, r, ped.equals("RED") ? RED_ON : RED_OFF);
        lamp(g, x + 12, y + 24 + 2 * r, r, ped.equals("GREEN") ? GREEN_ON : GREEN_OFF);
        figure(g, x + 12 + r, y + 12 + r, false);
        figure(g, x + 12 + r, y + 24 + 3 * r, true);

        // numărătoarea inversă, lângă lampa verde
        g.setColor(HOUSING);
        g.fillRoundRect(x + 2 * r + 32, y + 24 + 2 * r, 64, 2 * r, 12, 12);
        if (seconds > 0) {
            g.setColor(GREEN_ON);
            g.setFont(new Font(Font.MONOSPACED, Font.BOLD, 40));
            center(g, String.valueOf(seconds), x + 2 * r + 64, y + 24 + 3 * r);
        }

        // lampa AȘTEPTAȚI (cererea pietonului a fost înregistrată)
        int wy = y + 4 * r + 48;
        g.setColor(HOUSING);
        g.fillRoundRect(x - 10, wy, 2 * r + 44, 40, 12, 12);
        g.setColor(wait ? YELLOW_ON : YELLOW_OFF);
        g.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 16));
        center(g, "AȘTEPTAȚI", x + r + 12, wy + 20);
        label(g, "pietoni", x + r + 12, wy + 66);
    }

    private static void lamp(Graphics2D g, int x, int y, int r, Color c) {
        g.setColor(c);
        g.fillOval(x, y, 2 * r, 2 * r);
    }

    /** Silueta pietonului: în picioare (roșu) sau mergând (verde). */
    private static void figure(Graphics2D g, int cx, int cy, boolean walking) {
        g.setColor(new Color(0, 0, 0, 90));
        g.setStroke(new BasicStroke(4, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
        g.fillOval(cx - 5, cy - 22, 10, 10);
        g.drawLine(cx, cy - 10, cx, cy + 6);
        if (walking) {
            g.drawLine(cx, cy - 6, cx - 9, cy + 2);
            g.drawLine(cx, cy - 6, cx + 9, cy - 2);
            g.drawLine(cx, cy + 6, cx - 9, cy + 20);
            g.drawLine(cx, cy + 6, cx + 8, cy + 20);
        } else {
            g.drawLine(cx, cy - 6, cx - 6, cy + 6);
            g.drawLine(cx, cy - 6, cx + 6, cy + 6);
            g.drawLine(cx - 3, cy + 6, cx - 3, cy + 20);
            g.drawLine(cx + 3, cy + 6, cx + 3, cy + 20);
        }
        g.setStroke(new BasicStroke(1));
    }

    private static void label(Graphics2D g, String text, int cx, int cy) {
        g.setColor(new Color(0xc8c8c8));
        g.setFont(new Font(Font.SANS_SERIF, Font.PLAIN, 14));
        center(g, text, cx, cy);
    }

    private static void caption(Graphics2D g, String text, int cx, int cy) {
        g.setColor(new Color(0xff9f0a));
        g.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 15));
        center(g, text, cx, cy);
    }

    private static void center(Graphics2D g, String text, int cx, int cy) {
        FontMetrics fm = g.getFontMetrics();
        g.drawString(text, cx - fm.stringWidth(text) / 2, cy + (fm.getAscent() - fm.getDescent()) / 2);
    }
}
