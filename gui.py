import json
import os
import re
import threading
import time
import datetime
from queue import Queue, Empty

import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk, messagebox, font
from pynput import keyboard

# ================= CONFIGURATION =================
SETTINGS_PATH = os.path.join(os.path.expanduser("~"), ".esp_recoil_gui.json")

WEAPONS = [
    "AK47", "LR300", "MP5", "THOMPSON", "SAR", 
    "M39", "CUSTOMSMG", "HMLMG", "M249", "PYTHON"
]

DEFAULT_PROFILE = {
    "SENS": 0.78, "ADS": 1.00, "FOV": 90.0,
    "SMTH": 0.30, "GAIN": 2.20,
    "CROUCH": 0, "CROUCHM": 0.50
}

# --- THEME COLORS ---
C_BG      = "#1e1e1e"   # Dark Grey Background
C_FG      = "#d4d4d4"   # Light Grey Text
C_ACCENT  = "#007acc"   # Blue Accent
C_PANEL   = "#252526"   # Slightly lighter panel
C_SUCCESS = "#4ec9b0"   # Greenish
C_WARN    = "#ce9178"   # Orange/Reddish

# --- REGEX FOR FIRMWARE ---
STATE_RE = re.compile(
    r"ST:"
    r".*W=(?P<WEAPON>\S+)"
    r"(?:.*T=(?P<TBS>[-\d.]+))?"
    r".*S=(?P<SENS>[-\d.]+)"
    r"(?:.*A=(?P<ADS>[-\d.]+))?"
    r"(?:.*F=(?P<FOV>[-\d.]+))?"
    r"(?:.*G=(?P<GAIN>[-\d.]+))?"
    r"(?:.*SM=(?P<SMTH>[-\d.]+))?"
    r"(?:.*C=(?P<CROUCH>[01]))?"
    r"(?:.*ON=(?P<ON>[01]))?"
    r"(?:.*ATT=(?P<ATT>.*))?",
    re.IGNORECASE | re.DOTALL
)

# ================= BACKEND LOGIC =================
class SerialManager:
    def __init__(self, on_line):
        self.ser = None
        self._reader = None
        self._run = False
        self.on_line = on_line

    def list_ports(self):
        return [p.device for p in serial.tools.list_ports.comports()]

    def connect(self, port, baud=115200):
        self.close()
        try:
            self.ser = serial.Serial(port, baudrate=baud, timeout=1.0)
            self.ser.dtr = False
            self.ser.rts = False
            self._run = True
            self._reader = threading.Thread(target=self._loop, daemon=True)
            self._reader.start()
            return True, ""
        except Exception as e:
            self.ser = None
            self._run = False
            return False, str(e)

    def _loop(self):
        buf = bytearray()
        while self._run and self.ser:
            try:
                chunk = self.ser.read(256)
                if chunk:
                    buf.extend(chunk)
                    while b"\n" in buf:
                        line, _, buf = buf.partition(b"\n")
                        s = line.decode(errors="ignore").strip()
                        if s: self.on_line(s)
                else:
                    time.sleep(0.01)
            except:
                time.sleep(0.05)

    def send(self, text):
        if self.ser and self.ser.is_open:
            try:
                if not text.endswith("\n"): text += "\n"
                self.ser.write(text.encode("utf-8"))
            except: pass

    def close(self):
        self._run = False
        if self.ser:
            try: self.ser.close()
            except: pass
        self.ser = None

class Store:
    def __init__(self, path=SETTINGS_PATH):
        self.path = path
        self.data = {"weapons": {}}
        self.load()

    def load(self):
        if os.path.exists(self.path):
            try:
                with open(self.path, "r") as f: self.data = json.load(f)
            except: self.data = {"weapons": {}}
        for w in WEAPONS:
            if w not in self.data["weapons"]:
                self.data["weapons"][w] = DEFAULT_PROFILE.copy()
        self.save()

    def save(self):
        try:
            with open(self.path, "w") as f: json.dump(self.data, f, indent=2)
        except: pass

    def get(self, weapon):
        return dict(self.data["weapons"].get(weapon, DEFAULT_PROFILE))

    def set(self, weapon, key, value):
        self.data["weapons"].setdefault(weapon, DEFAULT_PROFILE.copy())[key] = value
        self.save()

# ================= MODERN UI =================
class ModernApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ESP32 Recoil Command")
        self.geometry("1000x750") # Slightly larger to prevent slider overlap
        self.configure(bg=C_BG)
        
        self.store = Store()
        self.serial = SerialManager(self._on_serial_rx)
        self.ui_q = Queue()
        self._kb_listener = None
        self._ctrl_held = False

        self._setup_style()
        self._build_layout()
        self._tick()
        self._start_hotkey()

    def _setup_style(self):
        style = ttk.Style(self)
        style.theme_use("clam")
        
        # Configure Colors
        style.configure(".", background=C_BG, foreground=C_FG, fieldbackground=C_PANEL)
        style.configure("TFrame", background=C_BG)
        style.configure("Panel.TFrame", background=C_PANEL, relief="flat")
        
        style.configure("TLabel", background=C_BG, foreground=C_FG, font=("Segoe UI", 10))
        style.configure("Panel.TLabel", background=C_PANEL, foreground=C_FG)
        style.configure("Header.TLabel", font=("Segoe UI", 12, "bold"), foreground=C_ACCENT)
        
        style.configure("TButton", background=C_PANEL, foreground=C_FG, borderwidth=1, focuscolor=C_ACCENT)
        style.map("TButton", background=[("active", C_ACCENT)], foreground=[("active", "white")])
        
        style.configure("Accent.TButton", background=C_ACCENT, foreground="white", font=("Segoe UI", 10, "bold"))
        style.map("Accent.TButton", background=[("active", "#005a9e")])

        style.configure("TCheckbutton", background=C_PANEL, foreground=C_FG)
        style.map("TCheckbutton", background=[("active", C_PANEL)])
        
        # --- DROPDOWN FIX ---
        style.configure("TCombobox", 
            fieldbackground=C_PANEL, 
            background=C_PANEL, 
            foreground=C_FG, 
            arrowcolor=C_FG
        )
        # Force the "Readonly" state to stay dark (Main box)
        style.map('TCombobox', 
            fieldbackground=[('readonly', C_PANEL)],
            selectbackground=[('readonly', C_PANEL)],
            selectforeground=[('readonly', C_FG)]
        )
        # Force the Dropdown Menu (Popdown Listbox) to be High Contrast
        self.option_add('*TCombobox*Listbox.background', '#202020') # Almost Black
        self.option_add('*TCombobox*Listbox.foreground', '#ffffff') # Pure White
        self.option_add('*TCombobox*Listbox.selectBackground', C_ACCENT)
        self.option_add('*TCombobox*Listbox.selectForeground', '#ffffff')

    def _build_layout(self):
        # === Main Containers ===
        self.columnconfigure(0, weight=1, minsize=350)
        self.columnconfigure(1, weight=2)
        self.rowconfigure(0, weight=1)

        left_pane = ttk.Frame(self, padding=10)
        left_pane.grid(row=0, column=0, sticky="nsew")
        
        right_pane = ttk.Frame(self, padding=10)
        right_pane.grid(row=0, column=1, sticky="nsew")

        # === LEFT PANE CONTENT ===
        
        # 1. Connection Box
        conn_frame = ttk.LabelFrame(left_pane, text="Connection", padding=10)
        conn_frame.pack(fill="x", pady=(0, 10))
        
        ttk.Label(conn_frame, text="Device Port:").pack(anchor="w")
        self.cmb_port = ttk.Combobox(conn_frame, state="readonly")
        self.cmb_port.pack(fill="x", pady=5)
        
        btn_row = ttk.Frame(conn_frame)
        btn_row.pack(fill="x")
        ttk.Button(btn_row, text="Refresh", command=self._refresh_ports).pack(side="left", fill="x", expand=True, padx=(0,2))
        self.btn_conn = ttk.Button(btn_row, text="Connect", style="Accent.TButton", command=self._toggle_conn)
        self.btn_conn.pack(side="left", fill="x", expand=True, padx=(2,0))

        # 2. Weapon Control
        wpn_frame = ttk.LabelFrame(left_pane, text="Loadout", padding=10)
        wpn_frame.pack(fill="x", pady=(0, 10))
        
        ttk.Label(wpn_frame, text="Active Weapon:").pack(anchor="w")
        self.cmb_weapon = ttk.Combobox(wpn_frame, values=WEAPONS, state="readonly", font=("Segoe UI", 11, "bold"))
        self.cmb_weapon.set(WEAPONS[0])
        self.cmb_weapon.pack(fill="x", pady=5)
        self.cmb_weapon.bind("<<ComboboxSelected>>", self._on_weapon_change)

        # Attachments
        lbl_att = ttk.Label(wpn_frame, text="Attachments:", font=("Segoe UI", 9, "bold"))
        lbl_att.pack(anchor="w", pady=(10, 2))
        
        att_grid = ttk.Frame(wpn_frame)
        att_grid.pack(fill="x")
        
        self.var_holo = tk.IntVar(); self.var_x8 = tk.IntVar()
        self.var_sil = tk.IntVar(); self.var_mb = tk.IntVar()
        
        # Checkbox Logic
        def t_holo():
            if self.var_holo.get(): self.var_x8.set(0)
            self._apply("HOLO", self.var_holo.get(), save=False)
        def t_x8():
            if self.var_x8.get(): self.var_holo.set(0)
            self._apply("X8", self.var_x8.get(), save=False)

        ttk.Checkbutton(att_grid, text="Holo", variable=self.var_holo, command=t_holo).grid(row=0, column=0, sticky="w")
        ttk.Checkbutton(att_grid, text="8x Scope", variable=self.var_x8, command=t_x8).grid(row=0, column=1, sticky="w")
        ttk.Checkbutton(att_grid, text="Silencer", variable=self.var_sil, command=lambda: self._apply("SIL", self.var_sil.get(), save=False)).grid(row=1, column=0, sticky="w")
        ttk.Checkbutton(att_grid, text="Boost", variable=self.var_mb, command=lambda: self._apply("MB", self.var_mb.get(), save=False)).grid(row=1, column=1, sticky="w")

        # 3. Tuning Sliders
        tune_frame = ttk.LabelFrame(left_pane, text="Tuning", padding=10)
        tune_frame.pack(fill="both", expand=True)

        self.var_sens = tk.DoubleVar(value=DEFAULT_PROFILE["SENS"])
        self.var_ads  = tk.DoubleVar(value=DEFAULT_PROFILE["ADS"])
        self.var_fov  = tk.DoubleVar(value=DEFAULT_PROFILE["FOV"])
        self.var_smth = tk.DoubleVar(value=DEFAULT_PROFILE["SMTH"])
        self.var_gain = tk.DoubleVar(value=DEFAULT_PROFILE["GAIN"])
        self.var_cmul = tk.DoubleVar(value=DEFAULT_PROFILE["CROUCHM"])

        self._mk_slider(tune_frame, "Sensitivity", self.var_sens, 0.1, 2.0, "SENS")
        self._mk_slider(tune_frame, "ADS Multiplier", self.var_ads, 0.1, 2.0, "ADS")
        self._mk_slider(tune_frame, "FOV", self.var_fov, 60, 120, "FOV")
        self._mk_slider(tune_frame, "Smoothing", self.var_smth, 0.05, 0.8, "SMTH")
        self._mk_slider(tune_frame, "Pattern Gain", self.var_gain, 0.1, 6.0, "GAIN")
        self._mk_slider(tune_frame, "Crouch Mult", self.var_cmul, 0.1, 2.0, "CROUCHM")

        # === RIGHT PANE CONTENT ===
        
        # 1. Live Dashboard
        dash = ttk.Frame(right_pane, style="Panel.TFrame", padding=10)
        dash.pack(fill="x", pady=(0, 10))
        
        ttk.Label(dash, text="LIVE DEVICE STATE", style="Header.TLabel", background=C_PANEL).pack(anchor="w")
        
        self.lbl_state_summ = ttk.Label(dash, text="Waiting for sync...", style="Panel.TLabel", font=("Consolas", 10))
        self.lbl_state_summ.pack(anchor="w", pady=(5,0))

        # 2. Controls & Sync
        ctrl_box = ttk.Frame(right_pane)
        ctrl_box.pack(fill="x", pady=(0,10))
        
        self.var_crouch = tk.IntVar()
        self.chk_crouch = ttk.Checkbutton(ctrl_box, text="Crouch Active (Ctrl Hold)", variable=self.var_crouch, command=lambda: self._apply("CROUCH", self.var_crouch.get(), save=True))
        self.chk_crouch.pack(side="left")
        
        ttk.Button(ctrl_box, text="GET STATE", style="Accent.TButton", command=lambda: self._send("GET:ALL")).pack(side="right")

        # 3. Console Log
        log_frame = ttk.LabelFrame(right_pane, text="Communication Log", padding=5)
        log_frame.pack(fill="both", expand=True)
        
        self.txt_log = tk.Text(log_frame, bg="#101010", fg="#cccccc", font=("Consolas", 9), state="disabled", wrap="word")
        scroll = ttk.Scrollbar(log_frame, command=self.txt_log.yview)
        self.txt_log.configure(yscrollcommand=scroll.set)
        
        self.txt_log.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")

        # Log Colors
        self.txt_log.tag_config("TX", foreground="#569cd6") # Blue
        self.txt_log.tag_config("RX", foreground="#6a9955") # Green
        self.txt_log.tag_config("SYS", foreground="#dcdcaa") # Yellow
        self.txt_log.tag_config("TIME", foreground="#555555") # Grey

        self._refresh_ports()
        self._log_sys("Application Ready.")
        self._load_profile(self.cmb_weapon.get())

    # ================= HELPER WIDGETS =================
    def _mk_slider(self, parent, text, var, min_v, max_v, key):
        f = ttk.Frame(parent)
        f.pack(fill="x", pady=2)
        
        top = ttk.Frame(f)
        top.pack(fill="x")
        ttk.Label(top, text=text).pack(side="left")
        ent = ttk.Entry(top, textvariable=var, width=6, justify="right")
        ent.pack(side="right")
        
        scale = ttk.Scale(f, from_=min_v, to=max_v, orient="horizontal", command=lambda v: var.set(f"{float(v):.3f}"))
        scale.pack(fill="x", pady=(2,5))
        
        # Bindings
        def on_release(e): self._apply(key, var.get())
        def on_ent(e): self._apply(key, var.get()); scale.set(var.get())
        
        scale.bind("<ButtonRelease-1>", on_release)
        ent.bind("<Return>", on_ent)
        
        # Initial sync
        var.trace_add("write", lambda *_: self._safe_scale_set(scale, var))
        self._safe_scale_set(scale, var)

    def _safe_scale_set(self, scale, var):
        try: scale.set(float(var.get()))
        except: pass

    # ================= LOGGING =================
    def _log_sys(self, msg): self._write_log("SYS", msg)
    def _log_tx(self, msg):  self._write_log("TX", msg)
    def _log_rx(self, msg):  self._write_log("RX", msg)

    def _write_log(self, tag, msg):
        ts = datetime.datetime.now().strftime("%H:%M:%S")
        self.txt_log.configure(state="normal")
        self.txt_log.insert("end", f"[{ts}] ", "TIME")
        self.txt_log.insert("end", f"[{tag}] ", tag)
        self.txt_log.insert("end", f"{msg}\n")
        self.txt_log.see("end")
        self.txt_log.configure(state="disabled")

    # ================= LOGIC =================
    def _refresh_ports(self):
        self.cmb_port['values'] = self.serial.list_ports()
        if self.cmb_port['values']: self.cmb_port.current(0)

    def _toggle_conn(self):
        if self.serial.ser:
            self.serial.close()
            self.btn_conn.configure(text="Connect", style="Accent.TButton")
            self._log_sys("Disconnected.")
        else:
            p = self.cmb_port.get()
            if not p: return
            ok, err = self.serial.connect(p)
            if ok:
                self.btn_conn.configure(text="Disconnect", style="TButton")
                self._log_sys(f"Connected to {p}. Waiting 2s for boot...")
                self.update()
                time.sleep(2.0)
                self._log_sys("Syncing...")
                self._on_weapon_change()
            else:
                self._log_sys(f"Error: {err}")

    def _on_weapon_change(self, _=None):
        w = self.cmb_weapon.get()
        self._load_profile(w)
        if self.serial.ser:
            self._send(f"WEAPON:{w}")
            self._push_profile(w)
            self._send("GET:ALL")
            self._apply_ctrl_hold()

    def _load_profile(self, w):
        p = self.store.get(w)
        self.var_sens.set(p["SENS"]); self.var_ads.set(p["ADS"])
        self.var_fov.set(p["FOV"]); self.var_smth.set(p["SMTH"])
        self.var_gain.set(p["GAIN"]); self.var_cmul.set(p["CROUCHM"])
        self.var_crouch.set(p["CROUCH"])
        # Reset attachments visually (device will confirm them)
        self.var_holo.set(0); self.var_x8.set(0); self.var_sil.set(0); self.var_mb.set(0)

    def _push_profile(self, w):
        p = self.store.get(w)
        self._send(f"SENS:{p['SENS']}"); self._send(f"ADS:{p['ADS']}")
        self._send(f"FOV:{p['FOV']}"); self._send(f"SMTH:{p['SMTH']}")
        self._send(f"GAIN:{p['GAIN']}"); self._send(f"CROUCHM:{p['CROUCHM']}")
        if not self._ctrl_held: self._send(f"CROUCH:{int(p['CROUCH'])}")

    def _apply(self, key, val, save=True):
        if not self.serial.ser: return
        val_str = f"{int(val)}" if key in ["HOLO","X8","SIL","MB","CROUCH"] else f"{float(val):.4f}"
        self._send(f"{key}:{val_str}")
        if save and key not in ["HOLO","X8","SIL","MB"]:
            self.store.set(self.cmb_weapon.get(), key, (int(val) if key=="CROUCH" else float(val)))

    def _send(self, text):
        self.serial.send(text)
        self._log_tx(text)

    def _on_serial_rx(self, line):
        self.ui_q.put(line)

    def _tick(self):
        try:
            while True:
                line = self.ui_q.get_nowait()
                self._log_rx(line)
                
                m = STATE_RE.match(line)
                if m: self._sync_from_device(m)
        except Empty: pass
        self.after(50, self._tick)

    def _sync_from_device(self, m):
        # Update Dashboard Label
        info = f"WPN: {m.group('WEAPON')} | SENS: {m.group('SENS')} | ON: {m.group('ON')}"
        if m.group('ATT'): info += f"\nATT: {m.group('ATT')}"
        self.lbl_state_summ.configure(text=info, foreground=C_SUCCESS)

        # Sync Logic
        dev_w = m.group("WEAPON")
        if dev_w in WEAPONS and dev_w != self.cmb_weapon.get():
            self.cmb_weapon.set(dev_w)
            self._load_profile(dev_w)

        def safe_upd(var, grp, t=float):
            if m.group(grp): var.set(t(m.group(grp)))

        safe_upd(self.var_sens, "SENS"); safe_upd(self.var_ads, "ADS")
        safe_upd(self.var_fov, "FOV"); safe_upd(self.var_smth, "SMTH")
        safe_upd(self.var_gain, "GAIN")
        
        if m.group("CROUCH") and not self._ctrl_held: 
            self.var_crouch.set(int(m.group("CROUCH")))

        # Sync Attachments
        if m.group("ATT"):
            att = m.group("ATT")
            self.var_holo.set(1 if "HOLO" in att else 0)
            self.var_x8.set(1 if "X8" in att else 0)
            self.var_sil.set(1 if "SIL" in att else 0)
            self.var_mb.set(1 if "MB" in att else 0)

    # ================= HOTKEYS =================
    def _start_hotkey(self):
        if not self._kb_listener:
            self._kb_listener = keyboard.Listener(on_press=self._on_kp, on_release=self._on_kr)
            self._kb_listener.start()

    def _on_kp(self, k):
        if k in (keyboard.Key.ctrl, keyboard.Key.ctrl_l, keyboard.Key.ctrl_r):
            if not self._ctrl_held:
                self._ctrl_held = True; self.var_crouch.set(1); self._apply("CROUCH", 1, save=False)

    def _on_kr(self, k):
        if k in (keyboard.Key.ctrl, keyboard.Key.ctrl_l, keyboard.Key.ctrl_r):
            if self._ctrl_held:
                self._ctrl_held = False
                base = self.store.get(self.cmb_weapon.get())["CROUCH"]
                self.var_crouch.set(base); self._apply("CROUCH", base, save=False)
                
    def _apply_ctrl_hold(self):
        val = 1 if self._ctrl_held else self.store.get(self.cmb_weapon.get())["CROUCH"]
        self.var_crouch.set(val); self._apply("CROUCH", val, save=False)

if __name__ == "__main__":
    app = ModernApp()
    app.mainloop()
