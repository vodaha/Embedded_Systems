import sys
import sqlite3
from datetime import datetime

import serial
import serial.tools.list_ports

from PyQt6.QtCore import QThread, pyqtSignal
from PyQt6.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QComboBox, QLabel, QTableWidget,
    QTableWidgetItem, QTextEdit
)


DB_FILE = "rfid_tags.db"
BAUD_RATE = 9600


# -------------------- Database --------------------
def init_db():
    conn = sqlite3.connect(DB_FILE)
    cur = conn.cursor()

    cur.execute("""
        CREATE TABLE IF NOT EXISTS tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uid TEXT UNIQUE NOT NULL,
            scan_count INTEGER NOT NULL,
            first_seen TEXT NOT NULL,
            last_seen TEXT NOT NULL
        )
    """)

    conn.commit()
    conn.close()


def save_tag(uid):
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    conn = sqlite3.connect(DB_FILE)
    cur = conn.cursor()

    cur.execute("SELECT id, scan_count FROM tags WHERE uid = ?", (uid,))
    row = cur.fetchone()

    if row is None:
        cur.execute("""
            INSERT INTO tags (uid, scan_count, first_seen, last_seen)
            VALUES (?, ?, ?, ?)
        """, (uid, 1, now, now))
    else:
        tag_id, count = row
        cur.execute("""
            UPDATE tags
            SET scan_count = ?, last_seen = ?
            WHERE id = ?
        """, (count + 1, now, tag_id))

    conn.commit()
    conn.close()


def load_tags():
    conn = sqlite3.connect(DB_FILE)
    cur = conn.cursor()

    cur.execute("""
        SELECT id, uid, scan_count, first_seen, last_seen
        FROM tags
        ORDER BY id
    """)

    rows = cur.fetchall()
    conn.close()

    return rows


# -------------------- Serial Thread --------------------
class SerialThread(QThread):
    line_received = pyqtSignal(str)
    status_changed = pyqtSignal(str)

    def __init__(self, port):
        super().__init__()
        self.port = port
        self.running = True
        self.ser = None

    def run(self):
        try:
            self.ser = serial.Serial(self.port, BAUD_RATE, timeout=0.2)
            self.status_changed.emit(f"Connected to {self.port}")

            while self.running:
                line = self.ser.readline().decode(errors="ignore").strip()

                if line:
                    self.line_received.emit(line)

        except Exception as e:
            self.status_changed.emit(f"Serial error: {e}")

        finally:
            if self.ser and self.ser.is_open:
                self.ser.close()

            self.status_changed.emit("Disconnected")

    def stop(self):
        self.running = False
        self.wait()


# -------------------- GUI --------------------
class RFIDGui(QWidget):
    def __init__(self):
        super().__init__()

        self.serial_thread = None

        self.setWindowTitle("RFID Database - PyQt6")
        self.resize(800, 500)

        init_db()
        self.build_ui()
        self.refresh_ports()
        self.refresh_table()

    def build_ui(self):
        layout = QVBoxLayout()

        # Control buttons
        top = QHBoxLayout()

        self.port_box = QComboBox()
        self.refresh_ports_btn = QPushButton("Refresh Ports")
        self.connect_btn = QPushButton("Connect")
        self.disconnect_btn = QPushButton("Disconnect")
        self.refresh_table_btn = QPushButton("Refresh Table")

        top.addWidget(QLabel("Port:"))
        top.addWidget(self.port_box)
        top.addWidget(self.refresh_ports_btn)
        top.addWidget(self.connect_btn)
        top.addWidget(self.disconnect_btn)
        top.addWidget(self.refresh_table_btn)

        layout.addLayout(top)

        self.status_label = QLabel("Not connected")
        layout.addWidget(self.status_label)

        # Tag table
        self.table = QTableWidget()
        self.table.setColumnCount(5)
        self.table.setHorizontalHeaderLabels(
            ["ID", "UID", "Scan Count", "First Seen", "Last Seen"]
        )
        layout.addWidget(self.table)

        # Serial log
        self.log = QTextEdit()
        self.log.setReadOnly(True)
        layout.addWidget(self.log)

        self.setLayout(layout)

        self.refresh_ports_btn.clicked.connect(self.refresh_ports)
        self.connect_btn.clicked.connect(self.connect_serial)
        self.disconnect_btn.clicked.connect(self.disconnect_serial)
        self.refresh_table_btn.clicked.connect(self.refresh_table)

    def refresh_ports(self):
        self.port_box.clear()

        ports = serial.tools.list_ports.comports()

        for port in ports:
            self.port_box.addItem(port.device)

    def connect_serial(self):
        port = self.port_box.currentText()

        if not port:
            self.status_label.setText("No port selected")
            return

        self.serial_thread = SerialThread(port)
        self.serial_thread.line_received.connect(self.handle_serial_line)
        self.serial_thread.status_changed.connect(self.status_label.setText)
        self.serial_thread.start()

    def disconnect_serial(self):
        if self.serial_thread:
            self.serial_thread.stop()
            self.serial_thread = None

    def handle_serial_line(self, line):
        self.log.append(line)

        if line.startswith("TAG,"):
            uid = line.split(",", 1)[1].strip().upper()

            if uid:
                save_tag(uid)
                self.refresh_table()

    def refresh_table(self):
        rows = load_tags()

        self.table.setRowCount(len(rows))

        for row_index, row_data in enumerate(rows):
            for col_index, value in enumerate(row_data):
                self.table.setItem(
                    row_index,
                    col_index,
                    QTableWidgetItem(str(value))
                )

        self.table.resizeColumnsToContents()

    def closeEvent(self, event):
        self.disconnect_serial()
        event.accept()


# -------------------- Main --------------------
if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = RFIDGui()
    window.show()
    sys.exit(app.exec())