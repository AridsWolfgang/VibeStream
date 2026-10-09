"""Mirrors include/vibestream/config.h — cross-platform JSON config."""
import json
import os
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class VSConfig:
    volume: float = 0.75
    repeat: int = 0  # 0=none, 1=all, 2=one (matches vs_repeat_mode)
    shuffle: bool = False
    crossfade: float = 2.0
    music_dir: str = ""
    download_dir: str = ""
    bindings: dict = field(default_factory=dict)


def _home() -> Path:
    return Path(os.path.expanduser("~"))


def get_config_path() -> Path:
    if os.name == "nt":
        base = Path(os.environ.get("APPDATA", str(_home() / "AppData/Roaming")))
        return base / "vibestream" / "config.json"
    xdg = os.environ.get("XDG_CONFIG_HOME")
    if xdg:
        return Path(xdg) / "vibestream" / "config.json"
    return _home() / ".config" / "vibestream" / "config.json"


def config_defaults(cfg: VSConfig) -> VSConfig:
    home = _home()
    cfg.music_dir = str(home / "Music")
    cfg.download_dir = str(home / "Music" / "vibestream")
    cfg.bindings = {"q": 1, " ": 2, "s": 3, "n": 4, "p": 5}
    return cfg


def config_load() -> VSConfig:
    cfg = VSConfig()
    config_defaults(cfg)
    path = get_config_path()
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return cfg
    cfg.volume = float(data.get("volume", cfg.volume))
    cfg.crossfade = float(data.get("crossfade", cfg.crossfade))
    rep = data.get("repeat", "none")
    cfg.repeat = {"all": 1, "one": 2}.get(rep, 0) if isinstance(rep, str) else int(rep)
    cfg.shuffle = bool(data.get("shuffle", False))
    cfg.music_dir = str(data.get("music_dir", cfg.music_dir))
    cfg.download_dir = str(data.get("download_dir", cfg.download_dir))
    return cfg


def config_save(cfg: VSConfig) -> None:
    path = get_config_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    rep = {1: "all", 2: "one"}.get(cfg.repeat, "none")
    data = {
        "volume": cfg.volume,
        "crossfade": cfg.crossfade,
        "shuffle": cfg.shuffle,
        "music_dir": cfg.music_dir,
        "download_dir": cfg.download_dir,
        "repeat": rep,
    }
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")
