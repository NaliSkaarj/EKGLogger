import serial
from collections import deque
from pyqtgraph.Qt import QtGui, QtCore
import pyqtgraph as pg
import sys
import threading

# --- konfiguracja UART ---
PORT = "/dev/ttyUSB0"
BAUDRATE = 115200
MAX_POINTS = 500

data = deque([0]*MAX_POINTS, maxlen=MAX_POINTS)

ser = serial.Serial(PORT, BAUDRATE, timeout=1)

# --- konfiguracja GUI ---
app = QtGui.QApplication(sys.argv)
win = pg.GraphicsLayoutWidget(show=True, title="Realtime EKG/ADC")
win.resize(800, 400)
plot = win.addPlot(title="ADC Values")
curve = plot.plot(list(data))
plot.setYRange(-10, 1100)
plot.setXRange(0, MAX_POINTS)

# --- wątek do odczytu UART ---
def serial_reader():
    while True:
        try:
            line_raw = ser.readline().decode(errors="ignore").strip()
            if line_raw:
                value = float(line_raw)
                data.append(value)
        except (ValueError, serial.SerialException):
            break

thread = threading.Thread(target=serial_reader, daemon=True)
thread.start()

# --- funkcja aktualizacji wykresu ---
def update():
    curve.setData(list(data))  # szybka aktualizacja
    # curve.setPos(0,0) # opcjonalnie przesuwanie okna

timer = QtCore.QTimer()
timer.timeout.connect(update)
timer.start(1)  # odświeżanie co ~1 ms

# --- start aplikacji Qt ---
if __name__ == '__main__':
    QtGui.QApplication.instance().exec_()
    ser.close()
