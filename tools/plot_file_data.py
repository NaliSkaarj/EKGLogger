#!/usr/bin/env python3
import struct
import sys
import matplotlib.pyplot as plt

def read_bpm_from_file(filename):
    bpms = []
    with open(filename, "rb") as f:
        while True:
            bytes_read = f.read(2)
            if len(bytes_read) < 2:
                break
            # Rozpakowanie 2 bajtów jako unsigned short (little-endian)
            (val,) = struct.unpack("<H", bytes_read)
            bpms.append(val / 100.0)
    return bpms

def main():
    if len(sys.argv) < 2:
        print("Użycie: python plot_bpm.py <plik>")
        sys.exit(1)

    filename = sys.argv[1]
    bpms = read_bpm_from_file(filename)

    if not bpms:
        print("Plik jest pusty lub nieprawidłowy.")
        sys.exit(1)

    plt.figure(figsize=(10, 5))
    plt.plot(bpms, marker='o', linestyle='-', color='b')
    plt.title("Tętno (BPM) z pliku")
    plt.xlabel("Numer próbki")
    plt.ylabel("BPM")
    plt.grid(True)
    plt.show()

if __name__ == "__main__":
    main()
