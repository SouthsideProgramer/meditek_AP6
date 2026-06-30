import tkinter as tk
from tkinter import ttk, filedialog, scrolledtext
import subprocess
import threading
import queue
import os
import signal

C_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "c")

BINARIES = {
    "TCP": os.path.join(C_DIR, "tcp_client"),
    "UDP": os.path.join(C_DIR, "udp_client"),
}

class ClientGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Client - Task 3 (C backend)")
        self.root.geometry("700x500")

        self.proc = None
        self.running = False
        self.log_q = queue.Queue()

        self.build_ui()
        self.poll_log()

    def build_ui(self):
        nb = ttk.Notebook(self.root)
        nb.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        for proto in ("TCP", "UDP"):
            f = ttk.Frame(nb, padding=10)
            nb.add(f, text=f"{proto} Client")

            ttk.Label(f, text=f"{proto} Configuration", font=("", 12, "bold")).pack(anchor=tk.W)
            gf = ttk.Frame(f)
            gf.pack(fill=tk.X, pady=10)

            ttk.Label(gf, text="Server IP:").grid(row=0, column=0, sticky=tk.W, padx=5, pady=5)
            e = ttk.Entry(gf, width=20)
            e.insert(0, "192.168.1.100")
            e.grid(row=0, column=1, sticky=tk.W, padx=5, pady=5)
            setattr(self, f"{proto.lower()}_ip", e)

            ttk.Label(gf, text="Port:").grid(row=0, column=2, sticky=tk.W, padx=5, pady=5)
            e = ttk.Entry(gf, width=10)
            e.insert(0, "8080" if proto == "TCP" else "9090")
            e.grid(row=0, column=3, sticky=tk.W, padx=5, pady=5)
            setattr(self, f"{proto.lower()}_port", e)

            ttk.Label(gf, text="Save as:").grid(row=1, column=0, sticky=tk.W, padx=5, pady=5)
            e = ttk.Entry(gf, width=50)
            e.insert(0, f"received_{proto.lower()}.txt")
            e.grid(row=1, column=1, columnspan=2, sticky=tk.W+tk.E, padx=5, pady=5)
            setattr(self, f"{proto.lower()}_save", e)
            ttk.Button(gf, text="Browse", command=lambda en=e: self.browse(en)).grid(row=1, column=3, padx=5)

        ctrl = ttk.Frame(self.root)
        ctrl.pack(fill=tk.X, padx=10)

        self.dl_btn = ttk.Button(ctrl, text="Download", command=self.start)
        self.dl_btn.pack(side=tk.LEFT, padx=5)
        self.cancel_btn = ttk.Button(ctrl, text="Cancel", command=self.stop, state=tk.DISABLED)
        self.cancel_btn.pack(side=tk.LEFT, padx=5)
        self.status_lbl = ttk.Label(ctrl, text="Idle")
        self.status_lbl.pack(side=tk.RIGHT, padx=5)

        self.log = scrolledtext.ScrolledText(self.root, height=12, state=tk.DISABLED)
        self.log.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))

    def browse(self, entry):
        p = filedialog.asksaveasfilename()
        if p:
            entry.delete(0, tk.END), entry.insert(0, p)

    def writelog(self, msg):
        self.log_q.put(msg)

    def poll_log(self):
        while not self.log_q.empty():
            self.log.config(state=tk.NORMAL)
            self.log.insert(tk.END, self.log_q.get() + "\n")
            self.log.see(tk.END)
            self.log.config(state=tk.DISABLED)
        self.root.after(100, self.poll_log)

    def start(self):
        if self.running:
            return
        nb = self.root.winfo_children()[0]
        tab = nb.index(nb.select())
        proto = ["TCP", "UDP"][tab]
        ip = getattr(self, f"{proto.lower()}_ip").get().strip()
        port = getattr(self, f"{proto.lower()}_port").get().strip()
        save = getattr(self, f"{proto.lower()}_save").get().strip()
        if not ip or not port or not save:
            self.writelog("Fill in IP, port and save path")
            return
        binpath = BINARIES[proto]
        if not os.path.isfile(binpath):
            self.writelog(f"Binary not found. Run 'make' in c/ first.")
            return

        self.running = True
        self.dl_btn.config(state=tk.DISABLED)
        self.cancel_btn.config(state=tk.NORMAL)
        self.status_lbl.config(text=f"Downloading from {ip}:{port}...")

        threading.Thread(target=self.run_client, args=(binpath, ip, port, save), daemon=True).start()

    def run_client(self, binpath, ip, port, save):
        try:
            self.proc = subprocess.Popen(
                [binpath, ip, port, save],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
            )
            for line in self.proc.stdout:
                self.writelog(line.rstrip())
            self.proc.wait()
        except Exception as e:
            self.writelog(f"Error: {e}")
        self.root.after(0, self.on_stopped)

    def stop(self):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            self.writelog("Cancelled by user")

    def on_stopped(self):
        self.running = False
        self.proc = None
        self.dl_btn.config(state=tk.NORMAL)
        self.cancel_btn.config(state=tk.DISABLED)
        self.status_lbl.config(text="Idle")

if __name__ == "__main__":
    tk.Tk().mainloop()
