import sys
import serial
import time

from PyQt6.QtWidgets import QApplication, QWidget, QVBoxLayout, QLabel, QPushButton
from PyQt6.QtCore import QTimer, Qt
from PyQt6.QtGui import QPainter, QColor


class JoystickCanvas(QWidget):
    def __init__(self, gui):
        super().__init__()
        self.gui = gui
        self.setMinimumHeight(300)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        w = self.width()
        h = self.height()

        painter.setPen(QColor(150, 150, 150))
        painter.drawRect(0, 0, w - 1, h - 1)

        painter.setPen(QColor(220, 220, 220))
        painter.drawLine(w // 2, 0, w // 2, h)
        painter.drawLine(0, h // 2, w, h // 2)

        x = int((self.gui.jx / 1023) * w)
        y = int((1 - self.gui.jy / 1023) * h)

        painter.setBrush(QColor(0, 122, 255))
        painter.setPen(Qt.PenStyle.NoPen)
        painter.drawEllipse(x - 10, y - 10, 20, 20)


class JoystickGUI(QWidget):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("Lab 4: Joystick Test")
        self.resize(400, 500)

        self.jx = 512
        self.jy = 512
        self.direction = "NEUTRAL"
        self.running = True
        self.samples = 0
        self.last_time = time.time()

        layout = QVBoxLayout()

        self.status_label = QLabel("Data: ON")
        self.x_label = QLabel("X: 512")
        self.y_label = QLabel("Y: 512")
        self.dir_label = QLabel("Direction: NEUTRAL")
        self.rate_label = QLabel("Sample Rate: 0 Hz")

        self.canvas = JoystickCanvas(self)
        self.toggle_button = QPushButton("Stop Test")
        self.toggle_button.clicked.connect(self.toggle_test)

        layout.addWidget(self.status_label)
        layout.addWidget(self.canvas)
        layout.addWidget(self.x_label)
        layout.addWidget(self.y_label)
        layout.addWidget(self.dir_label)
        layout.addWidget(self.rate_label)
        layout.addWidget(self.toggle_button)

        self.setLayout(layout)

        try:
            self.ser = serial.Serial("COM10", 115200, timeout=0.01)
            self.status_label.setText("Data: ON")
        except Exception as e:
            self.ser = None
            self.status_label.setText(f"Data: ERROR ({e})")

        self.timer = QTimer()
        self.timer.timeout.connect(self.read_serial)
        self.timer.start(10)

    def toggle_test(self):
        self.running = not self.running

        if self.running:
            self.status_label.setText("Data: ON")
            self.toggle_button.setText("Stop Test")
        else:
            self.status_label.setText("Data: PAUSED")
            self.toggle_button.setText("Start Test")

    def read_serial(self):
        if not self.running:
            return

        if not self.ser:
            return

        if self.ser.in_waiting <= 0:
            return

        try:
            line = self.ser.readline().decode("utf-8").strip()

            if not line:
                return

            parts = line.split(",")

            if len(parts) != 3:
                return

            self.jx = int(parts[0])
            self.jy = int(parts[1])
            self.direction = parts[2]

            self.x_label.setText(f"X: {self.jx}")
            self.y_label.setText(f"Y: {self.jy}")
            self.dir_label.setText(f"Direction: {self.direction}")

            self.samples += 1

            now = time.time()
            if now - self.last_time >= 1.0:
                self.rate_label.setText(f"Sample Rate: {self.samples} Hz")
                self.samples = 0
                self.last_time = now

            self.canvas.update()

        except:
            pass


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = JoystickGUI()
    window.show()
    sys.exit(app.exec())