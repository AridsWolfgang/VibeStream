"""VibeStream GUI entry — python gui/main.py [--rescan [dir]]"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from vibestream_gui.app import App
from vibestream_gui.config import config_load, config_save
from vibestream_gui.downloader import Downloader
from vibestream_gui.library import Library
from vibestream_gui.player import Player


def get_db_path() -> str:
    if os.name == "nt":
        base = Path(os.environ.get("LOCALAPPDATA", str(Path.home() / "AppData/Local")))
        return str(base / "vibestream" / "library.db")
    xdg = os.environ.get("XDG_DATA_HOME")
    if xdg:
        return str(Path(xdg) / "vibestream" / "library.db")
    return str(Path.home() / ".local" / "share" / "vibestream" / "library.db")


def main(argv):
    cfg = config_load()
    rescan_dir = None
    if "--rescan" in argv or "-r" in argv:
        i = (argv.index("--rescan") if "--rescan" in argv else argv.index("-r"))
        rescan_dir = argv[i + 1] if i + 1 < len(argv) and not argv[i + 1].startswith("-") else cfg.music_dir
    lib = Library(get_db_path())
    if rescan_dir:
        print(f"Scanning {rescan_dir}…")
        print(f"Added {lib.scan(rescan_dir)} new files.")
        return 0
    player = Player()
    player.crossfade = cfg.crossfade
    dl = Downloader(cfg.download_dir)
    app = App(cfg, lib, player, dl)
    try:
        app.mainloop()
    finally:
        config_save(cfg)
        lib.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
