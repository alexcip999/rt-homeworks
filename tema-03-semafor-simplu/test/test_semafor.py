"""Test pe placa reala: Serial (COM8) + MQTT. Scenariile din README tema 03."""
import sys, time, threading
import serial
sys.stdout.reconfigure(encoding="utf-8")
import paho.mqtt.client as mqtt

STUDENT = "constantin-alexandru"
BASE = f"ssatr/{STUDENT}/devices/"
PORT = sys.argv[1] if len(sys.argv) > 1 else "COM8"
LOG = sys.argv[2] if len(sys.argv) > 2 else "run.log"

t0 = time.time()
logf = open(LOG, "w", encoding="utf-8")
lock = threading.Lock()


def log(src, text):
    line = f"{time.time() - t0:8.3f} [{src}] {text}"
    with lock:
        print(line, flush=True)
        logf.write(line + "\n")
        logf.flush()


msgs = []  # (t, fields)
device = {"id": None}
errors = []


def serial_reader():
    ser = serial.Serial()
    ser.port = PORT
    ser.baudrate = 115200
    ser.dtr = False
    ser.rts = False
    ser.timeout = 0.2
    ser.open()
    buf = b""
    while True:
        buf += ser.read(512)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            log("serial", line.decode("utf-8", "replace").rstrip())


prev = {"cars": None}


def on_message(c, u, m):
    text = m.payload.decode()
    t = time.time() - t0
    f = text.split(";")
    if m.topic.endswith("/lights"):
        if len(f) != 7:
            errors.append(f"{t:.3f} mesaj invalid: {text}")
            return
        dev, ts, phase, cars, peds, wait, sec = f
        device["id"] = dev
        msgs.append((t, phase, cars, peds, int(wait), int(sec)))
        if peds == "GREEN" and cars != "RED":
            errors.append(f"{t:.3f} CONFLICT pietoni GREEN, masini {cars}")
        if prev["cars"] == "GREEN" and cars == "RED":
            errors.append(f"{t:.3f} masini GREEN -> RED fara YELLOW")
        prev["cars"] = cars
        log("lights", text)
    else:
        log("mqtt", f"{m.topic} {text}")


threading.Thread(target=serial_reader, daemon=True).start()
cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"test-{STUDENT}-{int(t0)}")
cli.on_message = on_message
cli.connect("control.aut.utcluj.ro", 11188)
cli.subscribe(BASE + "#", qos=1)
cli.loop_start()


def button(n=1, gap=0.0):
    for _ in range(n):
        log("test", f">>> BUTTON catre {device['id']}")
        cli.publish(f"{BASE}{device['id']}/commands", "BUTTON", qos=1)
        time.sleep(gap)


def last_phase():
    return msgs[-1][1] if msgs else None


def wait_until(pred, timeout):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.05)
    return False


def section(name):
    log("test", "=" * 10 + " " + name)


# 0. asteptam placa
section("astept primele mesaje lights")
if not wait_until(lambda: device["id"] is not None, 90):
    log("test", "placa nu publica"); sys.exit(1)
wait_until(lambda: last_phase() == "CAR_GREEN" and msgs[-1][4] == 0, 40)
time.sleep(1)

# 1. fara apasari: ramane pe CAR_GREEN
section("1. 15 s fara apasari")
n0 = len(msgs)
time.sleep(15)
phases = {m[1] for m in msgs[n0:]}
log("test", f"faze vazute: {phases}")

# 2. apasare dupa verdele minim: ciclu complet
section("2. BUTTON cu masinile pe verde > 10 s")
tb = time.time() - t0
button()
wait_until(lambda: msgs[-1][4] == 1, 5)
log("test", f"AȘTEPTAȚI aprinsa dupa {(msgs[-1][0] - tb) * 1000:.0f} ms (vazut in lights)")
wait_until(lambda: last_phase() == "PED_GREEN", 15)
# 2b. apasare in PED_GREEN -> ramane cererea
time.sleep(2)
section("2b. BUTTON in PED_GREEN")
button()
wait_until(lambda: last_phase() == "CAR_GREEN", 15)
tg = time.time() - t0
section("2b. astept noul ciclu (dupa verdele minim)")
wait_until(lambda: last_phase() == "CAR_YELLOW", 20)
log("test", f"galben dupa {time.time() - t0 - tg:.1f} s de verde")
wait_until(lambda: last_phase() == "CAR_GREEN", 20)

# 3. apasare devreme in CAR_GREEN
section("3. BUTTON la 3 s dupa intrarea in CAR_GREEN")
tg = time.time() - t0
time.sleep(3)
button()
wait_until(lambda: last_phase() == "CAR_YELLOW", 20)
log("test", f"galben dupa {time.time() - t0 - tg:.1f} s de verde")
wait_until(lambda: last_phase() == "CAR_GREEN", 20)

# 4. apasari dese
section("4. 4 x BUTTON la 200 ms")
wait_until(lambda: last_phase() == "CAR_GREEN" and msgs[-1][4] == 0, 40)
time.sleep(1)
button(4, 0.2)
wait_until(lambda: last_phase() == "CAR_GREEN" and msgs[-1][4] == 0, 40)
time.sleep(3)

section("REZUMAT")
# duratele fazelor din mesaje
changes = []
for i, m in enumerate(msgs):
    if i == 0 or m[1] != msgs[i - 1][1]:
        changes.append((m[0], m[1]))
for (ta, pa), (tb_, pb) in zip(changes, changes[1:]):
    log("test", f"{pa:10s} {tb_ - ta:6.2f} s (din mesaje lights, rezolutie 0.5 s)")
cd = [m[5] for m in msgs if m[1] == "PED_GREEN"]
log("test", f"numaratori PED_GREEN: {cd}")
gaps = [b[0] - a[0] for a, b in zip(msgs, msgs[1:])]
log("test", f"interval lights: min {min(gaps)*1000:.0f} ms, max {max(gaps)*1000:.0f} ms, medie {sum(gaps)/len(gaps)*1000:.0f} ms, {len(msgs)} mesaje")
log("test", f"erori de siguranta / format: {errors if errors else 'niciuna'}")
cli.loop_stop()
