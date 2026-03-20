import sys  # for starting the PyQt application and handling exit
import csv  # for logging events to a CSV file
from datetime import datetime # for PC timestamping of events

import serial
import serial.tools.list_ports # for finding available serial ports and identifying the correct one for the Arduino

from PyQt6.QtCore import QTimer

from PyQt6.QtWidgets import (
    QApplication,   # main application class
    QWidget,        # main window widget
    QVBoxLayout,    # vertical layout to stack widgets
    QLabel,         # for displaying text information
    QProgressBar,   # for visualizing the current dB level as a bar
    QListWidget     # for showing a list of logged threshold events with timestamps and dB levels
)

BAUD_RATE = 9600
CSV_FILE = "sound_events_db.csv"


# To find the correct serial port for the Arduino by looking for common keywords in the port description.
def find_serial_port():
    ports = list(serial.tools.list_ports.comports())

    for p in ports:
        desc = (p.description or "").lower()
        if "arduino" in desc or "ch340" in desc or "usb serial" in desc:
            return p.device

    # If no port matches the keywords, just return the first available port as a fallback (if any)
    if ports:
        return ports[0].device

    return None


class SoundGUI(QWidget):
    def __init__(self):
        # Initialize the QWidget base class
        super().__init__()

        self.setWindowTitle("Sound dB Monitor GUI")
        self.resize(800, 400) # self.resize(width, height)

        self.serial_port = None  # will hold the serial port connection to the Arduino
        self.prev_above = False # to track the previous state of whether the sound level was above the threshold, so we only log new events

        self.status_label = QLabel("Status: starting...")
        self.db_label = QLabel("Current Sound Level: 0.0 dB")
        self.event_label = QLabel("Interrupt Event Count: 0")
        self.threshold_label = QLabel("Threshold State: below")

        self.bar = QProgressBar()
        self.bar.setRange(0, 60) # set a reasonable default range for dB levels, will be updated dynamically based on readings
        self.bar.setValue(0)
        self.bar.setFormat("dB: %v") # show the current dB value on the bar itself

        self.log_list = QListWidget()

        layout = QVBoxLayout()
        layout.addWidget(self.status_label)
        layout.addWidget(self.db_label)
        layout.addWidget(self.event_label)
        layout.addWidget(self.threshold_label)
        layout.addWidget(self.bar)
        layout.addWidget(QLabel("Logged Threshold Events (dB)"))
        layout.addWidget(self.log_list)
        self.setLayout(layout)

        self.csv_file = open(CSV_FILE, "w", newline="", encoding="utf-8")
        self.csv_writer = csv.writer(self.csv_file)

        if self.csv_file.tell() == 0:
            self.csv_writer.writerow(["pc_time", "arduino_ms", "db_level"])

        port = find_serial_port()
        if port is None:
            self.status_label.setText("Status: No serial port found")
        else:
            try:
                self.serial_port = serial.Serial(port, BAUD_RATE, timeout=0.05)
                self.status_label.setText(f"Status: Connected to {port}")
            except Exception as e:
                self.status_label.setText(f"Status: Error opening port: {e}")

        self.timer = QTimer() # create a timer to periodically read from the serial port without blocking the GUI
        self.timer.timeout.connect(self.read_serial)
        self.timer.start(50)

    # This method is called every 50 ms by the timer to read any available data from the serial port, 
    # parse it, update the GUI, and log events when the sound level goes above the threshold.
    def read_serial(self):
        if not self.serial_port:
            return
        # Read all available lines from the serial port (non-blocking) and process them
        while self.serial_port.in_waiting:
            try:
                raw = self.serial_port.readline().decode(errors="ignore").strip()
                if not raw:
                    continue

                # Expected format:
                # millis,dbLevel,aboveThreshold,eventCount
                parts = raw.split(",")
                if len(parts) != 4:
                    continue

                arduino_ms = int(parts[0])
                db_level = float(parts[1])
                above = int(parts[2])
                event_count = int(parts[3])

                self.db_label.setText(f"Current Sound Level: {db_level:.1f} dB")
                self.event_label.setText(f"Interrupt Event Count: {event_count}")

                if above == 1:
                    self.threshold_label.setText("Threshold State: ABOVE")
                    self.bar.setStyleSheet("QProgressBar::chunk { background-color: red; }")
                else:
                    self.threshold_label.setText("Threshold State: below")
                    self.bar.setStyleSheet("QProgressBar::chunk { background-color: green; }")

                # Keep bar range reasonable
                bar_value = max(0, int(db_level))
                self.bar.setMaximum(max(60, bar_value + 5))
                self.bar.setValue(bar_value)

                # Log only when the signal newly goes above threshold
                if above == 1 and not self.prev_above:
                    pc_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                    self.csv_writer.writerow([pc_time, arduino_ms, f"{db_level:.1f}"])
                    self.csv_file.flush()
  
                    self.log_list.insertItem(0, f"{pc_time} | {db_level:.1f} dB")

                    if self.log_list.count() > 15:
                        self.log_list.takeItem(15)

                self.prev_above = (above == 1)

            except Exception:
                pass

    def closeEvent(self, event):
        try:
            if self.serial_port:
                self.serial_port.close()
        except Exception:
            pass

        try:
            self.csv_file.close()
        except Exception:
            pass

        event.accept()


def main():
    app = QApplication(sys.argv)
    window = SoundGUI()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()