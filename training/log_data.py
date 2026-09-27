"""
log_data.py — live serial data logger with in-session labeling.

Upload the raw CSV-streaming sketch to the NodeMCU first (a version of
AVsentry.ino with the ML/actuator logic stripped out, printing plain
"temp,hum,ir" lines over Serial — see README for that variant). Then
run this script, choose the scenario you are about to simulate, and
every row logged during that run is tagged with that label automatically.

Run once per scenario (normal, discomfort_hot, discomfort_humid,
occupied_uncomfortable, intrusion), appending to the same CSV each time.
"""

import serial
import csv
import time
from datetime import datetime

# ---- CONFIG ----
PORT = "COM4"          # update to match your NodeMCU's port
BAUD = 115200
OUTPUT_FILE = "sensor_log.csv"

VALID_LABELS = [
    "normal",
    "intrusion",
    "discomfort_hot",
    "discomfort_humid",
    "occupied_uncomfortable",
]


def choose_label():
    print("\nWhich scenario are you simulating for this session?")
    for i, label in enumerate(VALID_LABELS, start=1):
        print(f"  {i}. {label}")
    while True:
        choice = input("Enter number: ").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(VALID_LABELS):
            return VALID_LABELS[int(choice) - 1]
        print("Invalid choice, try again.")


label = choose_label()
print(f"\nLabeling all rows in this session as: {label}")

ser = serial.Serial(PORT, BAUD, timeout=2)
# Disabling DTR/RTS avoids some USB-serial chips holding the ESP8266
# in reset when the port opens, which otherwise causes silent no-data.
ser.setDTR(False)
ser.setRTS(False)
time.sleep(2)
ser.reset_input_buffer()

print(f"Listening on {PORT}... Press Ctrl+C to stop.\n")

try:
    with open(OUTPUT_FILE, "x", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp", "hour", "temp", "humidity", "ir", "label"])
except FileExistsError:
    pass

try:
    while True:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        if not line:
            continue

        parts = line.split(",")
        if len(parts) != 3:
            print(f"Skipped malformed line: {line}")
            continue

        temp, hum, ir = parts
        now = datetime.now()

        with open(OUTPUT_FILE, "a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([now.isoformat(), now.hour, temp, hum, ir, label])

        print(f"{now.strftime('%H:%M:%S')} | temp={temp} hum={hum} ir={ir} | label={label}")

except KeyboardInterrupt:
    print(f"\nStopped. Session labeled '{label}' saved to {OUTPUT_FILE}")
    ser.close()
