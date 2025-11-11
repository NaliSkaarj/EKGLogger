#!/usr/bin/env python3
import struct
import sys
import os
import tkinter as tk
from tkinter import filedialog, messagebox
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
import matplotlib.pyplot as plt


def read_bpm_from_file(filename):
    """Read BPM from binary file (2B = uint16 * 100)."""
    bpms = []
    try:
        with open(filename, "rb") as f:
            while True:
                bytes_read = f.read(2)
                if len(bytes_read) < 2:
                    break
                (val,) = struct.unpack("<H", bytes_read)
                bpms.append(val / 100.0)
    except Exception as e:
        messagebox.showerror("Read file error", f"Read file failed:\n{e}")
    return bpms


class BPMViewerApp:
    def __init__(self, root, initial_dir=None):
        self.root = root
        self.root.title("BPM File Viewer")

        # --- left side: chart ---
        self.fig, self.ax = plt.subplots(figsize=(7, 4))
        self.ax.format_coord = lambda x, y: ""  # turn off coordinates display
        self.ax.set_title("Chart BPM")
        self.ax.set_xlabel("Sample number")
        self.ax.set_ylabel("BPM")
        self.ax.grid(True)

        self.canvas = FigureCanvasTkAgg(self.fig, master=self.root)
        self.canvas.get_tk_widget().pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # --- interactive matplotlib toolbar ---
        self.toolbar = NavigationToolbar2Tk(self.canvas, self.root, pack_toolbar=False)
        self.toolbar.update()
        self.toolbar.pack(side=tk.TOP, fill=tk.X)

        # --- Min/max Y fields and checkbox (grid, label + entry side by side) ---
        ycontrol_frame = tk.Frame(self.root)
        ycontrol_frame.pack(fill=tk.X, pady=5)

        # Min Y
        tk.Label(ycontrol_frame, text="Min Y (BPM):").grid(row=0, column=0, sticky="w", padx=2, pady=2)
        self.min_y_entry = tk.Entry(ycontrol_frame, width=10)
        self.min_y_entry.grid(row=0, column=1, padx=2, pady=2)

        # Max Y
        tk.Label(ycontrol_frame, text="Max Y (BPM):").grid(row=1, column=0, sticky="w", padx=2, pady=2)
        self.max_y_entry = tk.Entry(ycontrol_frame, width=10)
        self.max_y_entry.grid(row=1, column=1, padx=2, pady=2)

        # Checkbox (in separate row)
        self.auto_y_var = tk.BooleanVar(value=True)
        self.auto_y_check = tk.Checkbutton(
            ycontrol_frame, text="Auto scale", variable=self.auto_y_var
        )
        self.auto_y_check.grid(row=2, column=0, columnspan=2, sticky="w", padx=2, pady=2)

        # --- right side: file list + button ---
        right_frame = tk.Frame(self.root)
        right_frame.pack(side=tk.RIGHT, fill=tk.Y)

        self.btn_select_dir = tk.Button(
            right_frame, text="Select directory", command=self.select_directory
        )
        self.btn_select_dir.pack(pady=5, padx=10, fill=tk.X)

        self.listbox = tk.Listbox(right_frame, width=30, selectmode=tk.MULTIPLE)
        self.listbox.pack(padx=10, pady=5, fill=tk.Y, expand=True)
        self.listbox.bind("<<ListboxSelect>>", self.on_file_select)

        # --- when directory was provided as argument ---
        if initial_dir:
            self.load_directory(initial_dir)
        else:
            self.select_directory()

    def select_directory(self):
        """Allows the user to select a directory containing files."""
        dirname = filedialog.askdirectory(title="Select directory with BPM files")
        if dirname:
            self.load_directory(dirname)

    def load_directory(self, dirname):
        """Load list of files from the directory (sorted alphabetically)."""
        self.dirname = dirname
        self.listbox.delete(0, tk.END)
        valid_ext = (".bin", ".dat", ".raw", ".txt")

        # get files and sort
        files = sorted(
            [f for f in os.listdir(dirname) if f.lower().endswith(valid_ext)]
        )

        for file in files:
            self.listbox.insert(tk.END, file)

    def on_file_select(self, event):
        """Display all selected files on a single plot."""
        selection = self.listbox.curselection()
        if not selection:
            return

        self.ax.clear()
        for idx in selection:
            filename = os.path.join(self.dirname, self.listbox.get(idx))
            bpms = read_bpm_from_file(filename)
            if not bpms:
                continue
            self.ax.plot(bpms, marker="o", linestyle="-", label=os.path.basename(filename))

        self.ax.set_title("BPM file comparison")
        self.ax.set_xlabel("Sample number")
        self.ax.set_ylabel("BPM")
        self.ax.grid(True)
        self.ax.legend()

        # --- Y axis settings ---
        if not self.auto_y_var.get():
            try:
                ymin = float(self.min_y_entry.get())
                ymax = float(self.max_y_entry.get())
                if ymin < ymax:
                    self.ax.set_ylim(ymin, ymax)
            except ValueError:
                pass  # if fields are empty or invalid, leave auto scale

        self.canvas.draw()

    def plot_bpm(self, bpms, filename):
        """Draws a BPM plot for the selected file."""
        self.ax.clear()
        self.ax.plot(bpms, marker="o", linestyle="-", color="b")
        self.ax.set_title(f"{os.path.basename(filename)}")
        self.ax.set_xlabel("Sample number")
        self.ax.set_ylabel("BPM")
        self.ax.grid(True)

        # --- Y axis settings ---
        if not self.auto_y_var.get():
            try:
                ymin = float(self.min_y_entry.get())
                ymax = float(self.max_y_entry.get())
                if ymin < ymax:
                    self.ax.set_ylim(ymin, ymax)
            except ValueError:
                pass  # if fields are empty or invalid, leave auto scale

        self.canvas.draw()


def main():
    initial_dir = None
    if len(sys.argv) >= 2:
        initial_dir = sys.argv[1]
        if not os.path.isdir(initial_dir):
            print(f"Error: {initial_dir} is not a directory.")
            sys.exit(1)

    root = tk.Tk()
    app = BPMViewerApp(root, initial_dir)
    root.mainloop()


if __name__ == "__main__":
    main()
