"""Tkinter GUI — Atlas design-system edition.

Maps Atlas (D:\\LoneDevWolf\\Projects\\Atlas) Swiss tokens 1:1:
paper/paper-soft/ink/muted/line/stone/accent/accent2/card,
Space Grotesk display / Instrument Sans body / JetBrains Mono data,
sharp corners, hairline rules, black CTA buttons that invert on hover,
mono uppercase meta labels. Includes Atlas's light/dark theme parity.
Replaces src/ui.c TUI as the cross-platform frontend.
"""
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog
from tkinter import font as tkfont

THEMES = {
    "paper": {  # Atlas :root
        "PAPER": "#FAFAF8", "SOFT": "#F4F4F2", "CARD": "#FFFFFF",
        "INK": "#0A0A0A", "MUTED": "#626260", "MUTED2": "#969694",
        "LINE": "#E2E2E0", "STRONG": "#C8C8C6", "STONE": "#E8E8E6",
        "ACCENT": "#FF3B30",
    },
    "ink": {  # Atlas [data-theme="ink"]
        "PAPER": "#0A0A0A", "SOFT": "#111111", "CARD": "#161616",
        "INK": "#FAFAF8", "MUTED": "#969694", "MUTED2": "#6E6E6C",
        "LINE": "#242424", "STRONG": "#3E3E3E", "STONE": "#282828",
        "ACCENT": "#FF453A",
    },
}
ACCENT2 = "#0A84FF"  # Atlas accent2, both themes

# module palette (re-pointed on theme toggle, widgets rebuilt)
PAPER = SOFT = CARD = INK = MUTED = MUTED2 = LINE = STRONG = STONE = ACCENT = ""

F_TITLE = F_HEAD = F_META = F_LIST = F_NP = F_MONO = ("TkDefaultFont", 9)


def _apply_theme(name):
    global PAPER, SOFT, CARD, INK, MUTED, MUTED2, LINE, STRONG, STONE, ACCENT
    t = THEMES[name]
    PAPER, SOFT, CARD = t["PAPER"], t["SOFT"], t["CARD"]
    INK, MUTED, MUTED2 = t["INK"], t["MUTED"], t["MUTED2"]
    LINE, STRONG, STONE = t["LINE"], t["STRONG"], t["STONE"]
    ACCENT = t["ACCENT"]


def _pick(cands):
    avail = set(tkfont.families())
    for c in cands:
        if c in avail:
            return c
    return cands[-1]


def _setup_fonts():
    global F_TITLE, F_HEAD, F_META, F_LIST, F_NP, F_MONO
    disp = _pick(["Space Grotesk", "Instrument Sans", "Inter", "Helvetica", "Arial"])
    body = _pick(["Instrument Sans", "Inter", "Helvetica", "Arial"])
    mono = _pick(["JetBrains Mono", "Geist Mono", "Consolas", "Courier"])
    F_TITLE = (disp, 30, "bold")
    F_HEAD = (mono, 10, "bold")
    F_META = (mono, 9)
    F_LIST = (body, 11)
    F_NP = (disp, 20, "bold")
    F_MONO = (mono, 9)
    return disp, body, mono


fmt_time = lambda s: f"{int(s)//60}:{int(s)%60:02d}"


class App(tk.Tk):
    def __init__(self, cfg, library, player, downloader):
        super().__init__()
        self.cfg, self.lib, self.player, self.dl = cfg, library, player, downloader
        self.theme = "paper"
        _apply_theme(self.theme)
        self.fonts = _setup_fonts()
        self.title("VibeStream")
        self.geometry("1180x710")
        # persistent state across theme rebuilds
        self.search_var = tk.StringVar()
        self.mode_var = tk.StringVar(value="Library")
        self.np_var = tk.StringVar(value="Queue is empty — double-click a song")
        self.np_artist = tk.StringVar(value="—")
        self.time_var = tk.StringVar(value="0:00 / 0:00")
        self.stat_var = tk.StringVar(value="STATUS · READY")
        self.vol_var = tk.StringVar(value=f"VOL {int(cfg.volume * 100)}")
        self.songs, self.queue, self.qindex = [], [], 0
        self._search_after = None
        self._build()
        self.refresh_library()
        self.player.set_volume(cfg.volume)
        self.bind("<space>", lambda e: self.toggle())
        self.bind("n", lambda e: self.next())
        self.bind("p", lambda e: self.prev())
        self.after(500, self._tick)

    # ---- atlas primitives (.btn-swiss / .meta-label / .hairline) ----
    def _rule(self, parent, h=2, color=None):
        f = tk.Frame(parent, bg=color or INK, height=h)
        f.pack(fill="x")
        return f

    def _cta(self, parent, text, cmd):
        """Atlas .btn-swiss: black block, mono uppercase, inverts on hover."""
        b = tk.Button(parent, text=text.upper(), command=cmd, font=F_META,
                      bg=INK, fg=PAPER, activebackground=PAPER, activeforeground=INK,
                      relief="flat", padx=14, pady=7, cursor="hand2")
        b.pack(side="left", padx=(0, 8))
        return b

    def _ghost(self, parent, text, cmd, side="left"):
        b = tk.Button(parent, text=text, command=cmd, font=F_META,
                      bg=PAPER, fg=INK, activebackground=INK, activeforeground=PAPER,
                      relief="solid", borderwidth=1, padx=10, pady=6, cursor="hand2")
        b.pack(side=side, padx=(0, 8) if side == "left" else (8, 0))
        return b

    def _meta(self, parent, text, color=None, side="left"):
        l = tk.Label(parent, text=text.upper(), font=F_META,
                     fg=color or MUTED, bg=PAPER)
        l.pack(side=side)
        return l

    def _section_head(self, parent, num, title, count_var=None):
        """Atlas .swiss-section: 2px top rule, mono accent index."""
        self._rule(parent, h=2)
        row = tk.Frame(parent, bg=PAPER)
        row.pack(fill="x", pady=(8, 6))
        tk.Label(row, text=num, font=F_HEAD, fg=ACCENT, bg=PAPER).pack(side="left")
        tk.Label(row, text="  " + title.upper(), font=F_HEAD, fg=INK, bg=PAPER).pack(side="left")
        if count_var is not None:
            tk.Label(row, textvariable=count_var, font=F_META, fg=MUTED, bg=PAPER).pack(side="right")
        return row

    # ---- layout ----
    def _build(self):
        self.configure(bg=PAPER)
        # masthead
        hdr = tk.Frame(self, bg=PAPER)
        hdr.pack(fill="x", padx=24, pady=(18, 12))
        left = tk.Frame(hdr, bg=PAPER)
        left.pack(side="left")
        self._meta(left, "VS—01 · Terminal music player · GUI 1.0", color=ACCENT)
        tk.Label(left, text="VibeStream", font=F_TITLE, fg=INK, bg=PAPER).pack(anchor="w")
        self._meta(left, f"{len(self._all_exts())} formats · {self.fonts[0]} / {self.fonts[1]} / {self.fonts[2]}")
        right = tk.Frame(hdr, bg=PAPER)
        right.pack(side="right", anchor="s")
        self._meta(right, "01 Library    02 Queue    03 Now playing", side="right")
        theme_btn = tk.Frame(right, bg=PAPER)
        theme_btn.pack(side="right", pady=(0, 6))
        tk.Frame(theme_btn, bg=ACCENT, width=10, height=10).pack(side="left", padx=(0, 8))
        self._ghost(theme_btn, f"Theme · {self.theme}", self.toggle_theme, side="right")
        self._rule(self, h=3)

        # toolbar
        bar = tk.Frame(self, bg=PAPER)
        bar.pack(fill="x", padx=24, pady=12)
        self._meta(bar, "Search")
        tk.Frame(bar, bg=PAPER, width=8).pack(side="left")
        se = tk.Entry(bar, textvariable=self.search_var, font=F_LIST, width=26,
                      bg=CARD, fg=INK, relief="solid", borderwidth=1,
                      insertbackground=INK, highlightthickness=1,
                      highlightbackground=STRONG, highlightcolor=ACCENT2)
        se.pack(side="left", padx=(0, 16))
        se.bind("<KeyRelease>", self._on_search_key)
        self._cta(bar, "Scan", self.on_scan)
        self._cta(bar, "Download", self.on_download)
        self._ghost(bar, "Save queue", self.on_save_queue)
        self._ghost(bar, "Retag", self.on_retag)
        self.mode_pl_btn = tk.Button(bar, text="PLAYLISTS", font=F_META, relief="solid",
                                     borderwidth=1, padx=12, pady=6, cursor="hand2",
                                     command=lambda: self.set_mode("Playlists"))
        self.mode_pl_btn.pack(side="right")
        self.mode_lib_btn = tk.Button(bar, text="LIBRARY", font=F_META, relief="solid",
                                      borderwidth=1, padx=12, pady=6, cursor="hand2",
                                      command=lambda: self.set_mode("Library"))
        self.mode_lib_btn.pack(side="right", padx=(0, 8))
        self._paint_mode()
        self._rule(self, h=1, color=LINE)

        # 01 / 02
        mid = tk.Frame(self, bg=PAPER)
        mid.pack(fill="both", expand=True, padx=24, pady=12)
        left = tk.Frame(mid, bg=PAPER)
        right = tk.Frame(mid, bg=PAPER)
        left.pack(side="left", fill="both", expand=True, padx=(0, 12))
        right.pack(side="left", fill="both", expand=True, padx=(12, 0))
        self.lib_count = tk.StringVar(value="")
        self.q_count = tk.StringVar(value="")
        self._section_head(left, "01", "Library", self.lib_count)
        self._section_head(right, "02", "Queue", self.q_count)
        # Atlas .swiss-table th: mono uppercase muted column headers
        self._meta(left, "Artist — Title")
        self._meta(right, "Order — Artist — Title")
        list_kw = dict(bg=CARD, fg=INK, font=F_LIST, relief="flat",
                       selectbackground=INK, selectforeground=PAPER,
                       activestyle="none", highlightthickness=1,
                       highlightbackground=STRONG, highlightcolor=INK)
        self.libbox = tk.Listbox(left, **list_kw)
        self.libbox.pack(fill="both", expand=True, pady=(4, 0))
        self.libbox.bind("<Double-Button-1>", lambda e: self.play_selected())
        self.qbox = tk.Listbox(right, **list_kw)
        self.qbox.pack(fill="both", expand=True, pady=(4, 0))
        self.qbox.bind("<Double-Button-1>", lambda e: self.play_queue_selected())
        self._rule(self, h=1, color=LINE)

        # 03 now playing
        np = tk.Frame(self, bg=PAPER)
        np.pack(fill="x", padx=24, pady=12)
        self._rule(np, h=2)
        head = tk.Frame(np, bg=PAPER)
        head.pack(fill="x", pady=(8, 2))
        tk.Label(head, text="03", font=F_HEAD, fg=ACCENT, bg=PAPER).pack(side="left")
        tk.Label(head, text="  NOW PLAYING", font=F_HEAD, fg=INK, bg=PAPER).pack(side="left")
        self.rec_dot = tk.Frame(head, bg=PAPER, width=10, height=10)
        self.rec_dot.pack(side="left", padx=(10, 0))
        tk.Label(head, textvariable=self.time_var, font=F_MONO, fg=INK, bg=PAPER).pack(side="right")
        tk.Label(np, textvariable=self.np_artist, font=F_META, fg=MUTED, bg=PAPER).pack(anchor="w")
        tk.Label(np, textvariable=self.np_var, font=F_NP, fg=INK, bg=PAPER,
                 anchor="w", justify="left").pack(fill="x", pady=(0, 8))

        self.bar = tk.Canvas(np, height=16, bg=PAPER, highlightthickness=0)
        self.bar.pack(fill="x", pady=(0, 10))
        self.bar.bind("<Button-1>", self._seek_click)
        self.bar.bind("<B1-Motion>", self._seek_click)
        self.bar.bind("<Configure>", lambda e: self._draw_progress())

        ctl = tk.Frame(np, bg=PAPER)
        ctl.pack(fill="x")
        self._cta(ctl, "Prev", self.prev)
        self._cta(ctl, "Play / Pause", self.toggle)
        self._cta(ctl, "Next", self.next)
        self._ghost(ctl, "Stop", self.stop)
        self._ghost(ctl, "+ Add", self.add_selected)
        rctl = tk.Frame(ctl, bg=PAPER)
        rctl.pack(side="right")
        self.shuffle_btn = self._ghost(rctl, "", self.toggle_shuffle, side="left")
        self.repeat_btn = self._ghost(rctl, "", self.toggle_repeat, side="left")
        self.shuffle_btn.configure(text=f"SHUFFLE · {'ON' if self.cfg.shuffle else 'OFF'}")
        self.repeat_btn.configure(text=["REPEAT · NONE", "REPEAT · ALL", "REPEAT · ONE"][self.cfg.repeat])
        tk.Label(rctl, textvariable=self.vol_var, font=F_META, fg=INK, bg=PAPER).pack(side="left", padx=(8, 4))
        self._ghost(rctl, "–", lambda: self._vol_step(-5), side="left")
        self._ghost(rctl, "+", lambda: self._vol_step(5), side="left")

        # footer hairline + status
        self._rule(self, h=3)
        foot = tk.Frame(self, bg=PAPER)
        foot.pack(fill="x", padx=24, pady=(8, 14))
        tk.Label(foot, textvariable=self.stat_var, font=F_MONO, fg=INK, bg=PAPER).pack(side="left")
        self._meta(foot, "Sharp · Grid 12 · CH", side="right")

    @staticmethod
    def _all_exts():
        from vibestream_gui.library import AUDIO_EXTS
        return AUDIO_EXTS

    # ---- theme ----
    def toggle_theme(self):
        if self._search_after:
            self.after_cancel(self._search_after)
            self._search_after = None
        self.theme = "ink" if self.theme == "paper" else "paper"
        _apply_theme(self.theme)
        for w in self.winfo_children():
            w.destroy()
        self._build()
        self.refresh_library()
        self.refresh_queue()
        self.stat_var.set(f"STATUS · THEME {self.theme.upper()}")

    def _paint_mode(self):
        on = dict(bg=INK, fg=PAPER)
        off = dict(bg=PAPER, fg=INK)
        self.mode_lib_btn.configure(**(on if self.mode_var.get() == "Library" else off))
        self.mode_pl_btn.configure(**(on if self.mode_var.get() == "Playlists" else off))

    def set_mode(self, m):
        self.mode_var.set(m)
        self._paint_mode()
        self.refresh_library()

    # ---- data ----
    def _on_search_key(self, e):
        if self._search_after:
            self.after_cancel(self._search_after)
        self._search_after = self.after(180, self.refresh_library)

    def refresh_library(self):
        q = self.search_var.get().strip()
        if self.mode_var.get() == "Playlists":
            pls = self.lib.playlists_get()
            self.songs = []
            items = [f"{name}" for _, name in pls] or ["(no playlists — use SAVE QUEUE)"]
            self._pl_ids = [pid for pid, _ in pls]
        else:
            self.songs = self.lib.search(q) if q else self.lib.songs_get()
            items = [f"{s.artist} — {s.title}" for s in self.songs] or ["(no songs — SCAN)"]
        self.libbox.delete(0, "end")
        if items:
            self.libbox.insert("end", *items)
        self.lib_count.set(f"{len(items)} ITEMS")

    def refresh_queue(self):
        self.qbox.delete(0, "end")
        cur = self.qindex if self.player.state != 0 else -1
        qitems = [f"{'■ ' if i == cur else '    '}{s.artist} — {s.title}"
                  for i, s in enumerate(self.queue)]
        if qitems:
            self.qbox.insert("end", *qitems)
        self.q_count.set(f"{len(self.queue)} ITEMS")

    # ---- transport ----
    def _play_at(self, i):
        if not self.queue:
            return
        self.qindex = max(0, min(i, len(self.queue) - 1))
        s = self.queue[self.qindex]
        if self.player.play(s.path) == 0:
            self.np_artist.set(f"{s.artist.upper()}  ·  {s.album.upper()}")
            self.np_var.set(s.title)
        else:
            messagebox.showerror("Playback failed", f"Cannot play:\n{s.path}")
        self.refresh_queue()

    def play_selected(self):
        if self.mode_var.get() == "Playlists":
            sel = self.libbox.curselection()
            if sel and hasattr(self, "_pl_ids") and sel[0] < len(self._pl_ids):
                for s in self.lib.playlist_songs(self._pl_ids[sel[0]]):
                    self.queue.append(s)
                self.refresh_queue()
                self._play_at(len(self.queue) - 1)
            return
        sel = self.libbox.curselection()
        if sel and sel[0] < len(self.songs):
            self.queue.append(self.songs[sel[0]])
            self.refresh_queue()
            self._play_at(len(self.queue) - 1)

    def play_queue_selected(self):
        sel = self.qbox.curselection()
        if sel:
            self._play_at(sel[0])

    def add_selected(self):
        sel = self.libbox.curselection()
        if sel and sel[0] < len(self.songs):
            self.queue.append(self.songs[sel[0]])
            self.refresh_queue()

    def toggle(self):
        if self.player.state == 0:
            self._play_at(self.qindex)
        else:
            self.player.toggle()

    def stop(self):
        self.player.stop()
        self.refresh_queue()

    def next(self):
        if not self.queue:
            return
        if self.cfg.repeat == 2:
            self._play_at(self.qindex)
        elif self.qindex + 1 < len(self.queue):
            self._play_at(self.qindex + 1)
        elif self.cfg.repeat == 1:
            self._play_at(0)
        else:
            self.stop()

    def prev(self):
        if self.player.position() > 3.0:
            self.player.seek(0)
        elif self.qindex > 0:
            self._play_at(self.qindex - 1)

    def toggle_shuffle(self):
        import random

        self.cfg.shuffle = not self.cfg.shuffle
        if self.cfg.shuffle and len(self.queue) > 1:
            cur = self.queue[self.qindex]
            rest = self.queue[:]
            random.shuffle(rest)
            self.queue = rest
            self.qindex = self.queue.index(cur)
            self.refresh_queue()
        self.shuffle_btn.configure(text=f"SHUFFLE · {'ON' if self.cfg.shuffle else 'OFF'}")

    def toggle_repeat(self):
        self.cfg.repeat = (self.cfg.repeat + 1) % 3
        self.repeat_btn.configure(text=["REPEAT · NONE", "REPEAT · ALL", "REPEAT · ONE"][self.cfg.repeat])

    def _vol_step(self, d):
        self.player.set_volume(self.player.volume + d / 100)
        self.cfg.volume = self.player.volume
        self.vol_var.set(f"VOL {int(self.player.volume*100)}")

    # ---- progress rule ----
    def _draw_progress(self, pos=None, dur=None):
        c = self.bar
        w = max(50, c.winfo_width())
        c.delete("all")
        c.create_rectangle(0, 7, w, 9, fill=STONE, outline="")
        pos = self.player.position() if pos is None else pos
        dur = self.player.duration() if dur in (None, 0) else dur
        if dur and dur > 0:
            x = min(w, max(0, pos / dur * w))
            c.create_rectangle(0, 6, x, 10, fill=INK, outline="")
            c.create_rectangle(x - 4, 2, x + 4, 14, fill=ACCENT, outline="")

    def _seek_click(self, e):
        dur = self.player.duration()
        if dur and dur > 0:
            w = max(50, self.bar.winfo_width())
            self.player.seek(max(0.0, min(1.0, e.x / w)) * dur)

    # ---- actions ----
    def on_scan(self):
        d = filedialog.askdirectory(initialdir=self.cfg.music_dir or None)
        if d:
            self.cfg.music_dir = d
            self.stat_var.set(f"STATUS · SCANNING {d}")
            self.update()
            n = self.lib.scan(d)
            self.refresh_library()
            self.stat_var.set(f"STATUS · SCAN DONE — {n} NEW / {len(self.songs)} TOTAL")

    def on_retag(self):
        self.lib.update_metadata()
        self.refresh_library()
        self.stat_var.set("STATUS · TAGS UPDATED")

    def on_save_queue(self):
        if not self.queue:
            return
        name = simpledialog.askstring("Save playlist", "Playlist name:")
        if name:
            self.lib.playlist_save_queue(name, self.queue)
            self.refresh_library()

    def on_download(self):
        url = simpledialog.askstring("Download", "YouTube / SoundCloud URL:")
        if url:
            self.dl.enqueue(url)
            self.dl.process(background=True)
            messagebox.showinfo("Download", f"Queued:\n{url}\nSaves to {self.dl.download_dir}")

    def _tick(self):
        try:
            pos, dur = self.player.position(), self.player.duration()
            playing = self.player.state == 1
            self.rec_dot.configure(bg=ACCENT if playing else PAPER)
            if dur > 0:
                self.time_var.set(f"{fmt_time(pos)} / {fmt_time(dur)}")
                self._draw_progress(pos, dur)
                if playing and pos >= dur - 0.4:
                    self.next()
        finally:
            self.after(500, self._tick)
