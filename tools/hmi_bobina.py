"""HMI sencillo para el controlador de bobina (USART3, 115200 bps).

Uso:  python tools\\hmi_bobina.py [PUERTO]      (por defecto COM3)
Requiere: pip install pyserial

Pide STATUS al MCU cada segundo. Eso mantiene vivo el latido: si se cierra
el HMI con la salida encendida, el MCU la apaga a los 5 s (SAFE OFF).
"""

import json
import queue
import sys
import threading
import time
import tkinter as tk
from tkinter import ttk

import serial

DEFAULT_PORT = "COM3"
BAUDRATE = 115200
I_MAX_A = 6.0           # mismo valor que CFG_I_MAX_A en app_config.h
POLL_MS = 1000          # periodo de STATUS
STALE_S = 3.5           # sin STATUS durante este tiempo -> datos "sin datos"

# Mensajes periodicos que no se muestran en el registro
LOG_IGNORE_PREFIXES = ("ALIVE", "IMEAS:", '{"state"', '{"temp"')


class SerialLink:
    """Puerto serie con hilo lector que deja las lineas en una cola."""

    def __init__(self):
        self.ser = None
        self.lines = queue.Queue()
        self._stop = threading.Event()
        self._thread = None
        self._wlock = threading.Lock()

    @property
    def is_open(self):
        return self.ser is not None and self.ser.is_open

    def open(self, port):
        self.ser = serial.serial_for_url(port, BAUDRATE, timeout=0.2, write_timeout=0.5)
        self._stop.clear()
        self._thread = threading.Thread(target=self._reader, daemon=True)
        self._thread.start()

    def close(self):
        self._stop.set()
        if self.ser is not None:
            try:
                self.ser.close()
            except serial.SerialException:
                pass
        self.ser = None

    def send(self, line):
        if not self.is_open:
            return False
        try:
            with self._wlock:
                self.ser.write((line + "\r\n").encode("ascii"))
            return True
        except (serial.SerialException, OSError) as exc:
            self.lines.put(("__error__", str(exc)))
            return False

    def _reader(self):
        buf = b""
        while not self._stop.is_set():
            try:
                chunk = self.ser.read(256)
            except (serial.SerialException, OSError, TypeError, AttributeError) as exc:
                if not self._stop.is_set():
                    self.lines.put(("__error__", str(exc)))
                return
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                text = raw.decode("ascii", errors="replace").strip()
                if text:
                    self.lines.put(("line", text))


def parse_json_line(text):
    """Devuelve el dict si la linea es JSON valido, si no None.
    El firmware puede escribir 'null' para valores no disponibles."""
    if not text.startswith("{"):
        return None
    try:
        return json.loads(text)
    except ValueError:
        return None


class HmiApp:
    def __init__(self, root, port):
        self.root = root
        self.link = SerialLink()
        self.last_status_t = 0.0
        self.status = {}

        root.title("HMI Controlador de Bobina")
        root.resizable(False, False)
        root.protocol("WM_DELETE_WINDOW", self.on_close)

        style = ttk.Style(root)
        style.configure("Big.TLabel", font=("Segoe UI", 26, "bold"))
        style.configure("Cap.TLabel", font=("Segoe UI", 10))
        style.configure("Off.TButton", foreground="#b00000", font=("Segoe UI", 10, "bold"))

        # --- Conexion ----------------------------------------------------
        top = ttk.Frame(root, padding=8)
        top.grid(row=0, column=0, sticky="ew")
        ttk.Label(top, text="Puerto:").pack(side="left")
        self.port_var = tk.StringVar(value=port)
        ttk.Entry(top, textvariable=self.port_var, width=10).pack(side="left", padx=4)
        self.conn_btn = ttk.Button(top, text="Conectar", command=self.toggle_connection)
        self.conn_btn.pack(side="left", padx=4)
        self.conn_lbl = ttk.Label(top, text="Desconectado", foreground="gray")
        self.conn_lbl.pack(side="left", padx=8)

        # --- Indicadores -------------------------------------------------
        ind = ttk.Frame(root, padding=(8, 0))
        ind.grid(row=1, column=0, sticky="ew")
        self.var_setpoint = tk.StringVar(value="--")
        self.var_imeas = tk.StringVar(value="--")
        self.var_vout = tk.StringVar(value="--")
        self.var_dir = tk.StringVar(value="--")
        for col, (cap, var) in enumerate((
                ("Consigna", self.var_setpoint),
                ("Corriente real", self.var_imeas),
                ("Tension DC-DC", self.var_vout),
                ("Direccion", self.var_dir))):
            box = ttk.LabelFrame(ind, text=cap, padding=8)
            box.grid(row=0, column=col, padx=4, pady=4, sticky="nsew")
            ttk.Label(box, textvariable=var, style="Big.TLabel", width=8,
                      anchor="center").pack()

        self.var_state = tk.StringVar(value="Estado: --")
        self.var_fault = tk.StringVar(value="Ultimo fallo: --")
        info = ttk.Frame(root, padding=(12, 0))
        info.grid(row=2, column=0, sticky="ew")
        ttk.Label(info, textvariable=self.var_state, style="Cap.TLabel").pack(side="left")
        self.fault_lbl = ttk.Label(info, textvariable=self.var_fault, style="Cap.TLabel")
        self.fault_lbl.pack(side="left", padx=20)
        ttk.Label(info, text="(Corriente real: magnitud INA219 con el signo de la direccion)",
                  style="Cap.TLabel", foreground="gray").pack(side="right")

        # --- Control -----------------------------------------------------
        ctl = ttk.LabelFrame(root, text="Control", padding=8)
        ctl.grid(row=3, column=0, sticky="ew", padx=8, pady=8)
        ttk.Label(ctl, text=f"Corriente (0 - {I_MAX_A:.2f} A):").grid(row=0, column=0, sticky="w")
        self.i_var = tk.StringVar(value="1.00")
        i_entry = ttk.Entry(ctl, textvariable=self.i_var, width=8, font=("Segoe UI", 12))
        i_entry.grid(row=0, column=1, padx=6)
        i_entry.bind("<Return>", lambda _e: self.apply_current())
        self.dir_var = tk.IntVar(value=+1)
        ttk.Radiobutton(ctl, text="Positiva (+)", variable=self.dir_var, value=+1).grid(row=0, column=2, padx=4)
        ttk.Radiobutton(ctl, text="Negativa (-)", variable=self.dir_var, value=-1).grid(row=0, column=3, padx=4)
        ttk.Button(ctl, text="Aplicar", command=self.apply_current).grid(row=0, column=4, padx=8)
        ttk.Button(ctl, text="ON", command=lambda: self.send_cmd("ON")).grid(row=0, column=5, padx=4)
        ttk.Button(ctl, text="OFF", style="Off.TButton",
                   command=lambda: self.send_cmd("OFF")).grid(row=0, column=6, padx=4)
        self.ctl_msg = ttk.Label(ctl, text="", foreground="#b00000")
        self.ctl_msg.grid(row=1, column=0, columnspan=7, sticky="w", pady=(6, 0))

        # --- Registro ----------------------------------------------------
        logf = ttk.LabelFrame(root, text="Mensajes del MCU", padding=4)
        logf.grid(row=4, column=0, sticky="ew", padx=8, pady=(0, 8))
        self.log = tk.Text(logf, height=10, width=90, state="disabled", font=("Consolas", 9))
        self.log.pack(side="left", fill="both")
        sb = ttk.Scrollbar(logf, command=self.log.yview)
        sb.pack(side="right", fill="y")
        self.log["yscrollcommand"] = sb.set

        root.bind("<Escape>", lambda _e: self.send_cmd("OFF"))

        self.root.after(100, self.process_lines)
        self.root.after(POLL_MS, self.poll_status)
        self.connect()

    # ----------------------------------------------------------------- serie
    def connect(self):
        port = self.port_var.get().strip()
        try:
            self.link.open(port)
        except (serial.SerialException, OSError, ValueError) as exc:
            self.conn_lbl.config(text=f"Error: {exc}", foreground="#b00000")
            return
        self.conn_lbl.config(text=f"Conectado a {port}", foreground="#007000")
        self.conn_btn.config(text="Desconectar")
        self.add_log(f"--- Conectado a {port} ---")
        self.send_cmd("STATUS", log=False)

    def disconnect(self, reason=None):
        self.link.close()
        self.conn_btn.config(text="Conectar")
        self.conn_lbl.config(text=reason or "Desconectado",
                             foreground="#b00000" if reason else "gray")
        self.clear_indicators()

    def toggle_connection(self):
        if self.link.is_open:
            self.disconnect()
        else:
            self.connect()

    def send_cmd(self, cmd, log=True):
        if not self.link.is_open:
            self.ctl_msg.config(text="No conectado")
            return
        if self.link.send(cmd) and log:
            self.add_log(f"> {cmd}")

    # --------------------------------------------------------------- control
    def apply_current(self):
        try:
            value = float(self.i_var.get().replace(",", "."))
        except ValueError:
            self.ctl_msg.config(text="Valor de corriente no valido")
            return
        if not 0.0 < value <= I_MAX_A:
            self.ctl_msg.config(text=f"La corriente debe estar entre 0 y {I_MAX_A:.2f} A (usa OFF para 0)")
            return
        self.ctl_msg.config(text="")
        sign = "-" if self.dir_var.get() < 0 else "+"
        self.send_cmd(f"I {sign}{value:.2f}")

    # ----------------------------------------------------------- recepcion
    def poll_status(self):
        if self.link.is_open:
            self.send_cmd("STATUS", log=False)
            if self.last_status_t and (time.monotonic() - self.last_status_t) > STALE_S:
                self.conn_lbl.config(text="Conectado, sin respuesta del MCU", foreground="#c07000")
        self.root.after(POLL_MS, self.poll_status)

    def process_lines(self):
        try:
            while True:
                kind, text = self.link.lines.get_nowait()
                if kind == "__error__":
                    self.disconnect(f"Puerto perdido: {text}")
                    continue
                self.handle_line(text)
        except queue.Empty:
            pass
        self.root.after(100, self.process_lines)

    def handle_line(self, text):
        data = parse_json_line(text)
        if data is not None and "state" in data:
            self.update_status(data)
        elif data is not None and "intensity" in data:
            self.update_imeas(data.get("intensity"))

        if not text.startswith(LOG_IGNORE_PREFIXES):
            self.add_log(text)

    def update_status(self, st):
        self.status = st
        self.last_status_t = time.monotonic()
        self.conn_lbl.config(text=f"Conectado a {self.port_var.get().strip()}", foreground="#007000")

        direction = st.get("dir", 1)
        sign = "-" if direction < 0 else "+"
        iset = st.get("iset_A")
        self.var_setpoint.set(f"{sign}{iset:.2f} A" if isinstance(iset, (int, float)) and iset > 0 else "0.00 A")

        vout = st.get("vout_V")
        self.var_vout.set(f"{vout:.2f} V" if isinstance(vout, (int, float)) else "ERR")

        self.var_dir.set("+ POS" if direction > 0 else "- NEG")
        self.update_imeas(st.get("imeas_mA"))

        self.var_state.set(f"Estado: {st.get('state', '?')}   Aplicado al buck: "
                           f"{st.get('iapplied_A', 0):.2f} A")
        fault = st.get("last_fault", "?")
        self.var_fault.set(f"Ultimo fallo: {fault}")
        self.fault_lbl.config(foreground="#b00000" if fault not in ("NONE", "?") else "black")

    def update_imeas(self, imeas_mA):
        if not isinstance(imeas_mA, (int, float)):
            self.var_imeas.set("--")
            return
        direction = self.status.get("dir", 1)
        amps = abs(imeas_mA) / 1000.0
        if amps == 0.0:
            self.var_imeas.set("0.00 A")
        else:
            self.var_imeas.set(f"{'-' if direction < 0 else '+'}{amps:.2f} A")

    def clear_indicators(self):
        for var in (self.var_setpoint, self.var_imeas, self.var_vout, self.var_dir):
            var.set("--")
        self.var_state.set("Estado: --")
        self.var_fault.set("Ultimo fallo: --")
        self.last_status_t = 0.0
        self.status = {}

    def add_log(self, text):
        self.log.config(state="normal")
        self.log.insert("end", time.strftime("%H:%M:%S ") + text + "\n")
        if int(self.log.index("end-1c").split(".")[0]) > 500:
            self.log.delete("1.0", "100.0")
        self.log.see("end")
        self.log.config(state="disabled")

    def on_close(self):
        self.link.close()
        self.root.destroy()


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PORT
    root = tk.Tk()
    HmiApp(root, port)
    root.mainloop()


if __name__ == "__main__":
    main()
