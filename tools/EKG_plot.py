import serial
import matplotlib.pyplot as plt
from collections import deque
import threading

PORT = "/dev/ttyUSB0"
BAUDRATE = 115200
MAX_POINTS = 500

data = deque([0]*MAX_POINTS, maxlen=MAX_POINTS)

ser = serial.Serial(PORT, BAUDRATE, timeout=1)

# --- konfiguracja wykresu ---
fig, ax = plt.subplots()
line, = ax.plot(data)
ax.set_ylim(-10, 1100)
ax.set_xlim(0, MAX_POINTS)

# --- wątek tylko do czytania UART ---
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

# --- funkcja aktualizująca wykres ---
def update(frame):
    line.set_ydata(data)
    line.set_xdata(range(len(data)))
    return line,

# --- animacja w głównym wątku ---
import matplotlib.animation as animation
ani = animation.FuncAnimation(fig, update, interval=1, save_count=MAX_POINTS, cache_frame_data=False)
plt.show()

ser.close()
