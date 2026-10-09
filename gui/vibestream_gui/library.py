"""Mirrors include/vibestream/library.h — SQLite library with mutagen tags."""
import os
import sqlite3
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

AUDIO_EXTS = {
    ".mp3", ".mp2", ".mp1", ".mpa",
    ".flac", ".wav", ".wave", ".aiff", ".aif", ".aifc",
    ".ogg", ".oga", ".spx", ".opus",
    ".m4a", ".aac", ".adts", ".wma",
    ".ape", ".wv", ".tta", ".mpc", ".mpp", ".mp+",
    ".mid", ".midi", ".kar", ".rmi",
    ".mod", ".xm", ".s3m", ".it", ".mtm", ".umx", ".mo3",
    ".dsf", ".dff", ".mka", ".weba",
}

SCHEMA = """
CREATE TABLE IF NOT EXISTS songs (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  path TEXT UNIQUE NOT NULL,
  title TEXT,
  artist TEXT,
  album TEXT,
  genre TEXT,
  year INTEGER DEFAULT 0,
  track INTEGER DEFAULT 0,
  duration REAL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_songs_artist ON songs(artist);
CREATE INDEX IF NOT EXISTS idx_songs_album ON songs(album);
CREATE TABLE IF NOT EXISTS playlists (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS playlist_songs (
  playlist_id INTEGER,
  song_id INTEGER,
  position INTEGER,
  FOREIGN KEY(playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
  FOREIGN KEY(song_id) REFERENCES songs(id),
  PRIMARY KEY(playlist_id, song_id)
);
"""


@dataclass
class Song:
    id: int = 0
    title: str = ""
    artist: str = "Unknown"
    album: str = "Unknown"
    genre: str = ""
    year: int = 0
    track: int = 0
    path: str = ""
    duration: float = 0.0


def _read_tags(path: str) -> dict:
    try:
        import mutagen

        f = mutagen.File(path, easy=True)
        if f is None:
            return {}
        info = {}
        g = lambda k: (f.get(k, [""])[0] if f.get(k) else "")
        info["title"] = g("title")
        info["artist"] = g("artist") or g("albumartist")
        info["album"] = g("album")
        info["genre"] = g("genre")
        try:
            info["year"] = int(str(g("date") or g("year"))[:4] or 0)
        except ValueError:
            info["year"] = 0
        try:
            info["track"] = int(str(g("tracknumber") or "0").split("/")[0] or 0)
        except ValueError:
            info["track"] = 0
        try:
            info["duration"] = float(getattr(f.info, "length", 0.0) or 0.0)
        except (AttributeError, ValueError):
            info["duration"] = 0.0
        return info
    except Exception:
        return {}


class Library:
    def __init__(self, db_path: str):
        Path(db_path).parent.mkdir(parents=True, exist_ok=True)
        self.db = sqlite3.connect(db_path)
        self.db.executescript(SCHEMA)

    def close(self):
        self.db.close()

    def scan(self, music_dir: str, recursive: bool = True) -> int:
        root = Path(music_dir).expanduser()
        if not root.is_dir():
            return 0
        pattern = "**/*" if recursive else "*"
        n = 0
        new_ids = []
        for p in root.glob(pattern):
            if p.is_file() and p.suffix.lower() in AUDIO_EXTS:
                cur = self.db.execute(
                    "INSERT OR IGNORE INTO songs (path, title) VALUES (?, ?)",
                    (str(p), p.stem),
                )
                if cur.rowcount:
                    new_ids.append(cur.lastrowid)
                    n += 1
        self.db.commit()
        if new_ids:
            self.update_metadata(new_ids)
        return n

    def update_metadata(self, ids=None):
        if ids is not None:
            if not ids:
                return
            ph = ",".join("?" * len(ids))
            rows = self.db.execute(
                f"SELECT id, path, title FROM songs WHERE id IN ({ph})", list(ids)
            ).fetchall()
        else:
            rows = self.db.execute(
                "SELECT id, path, title FROM songs WHERE artist IS NULL OR artist='' OR artist='Unknown'"
            ).fetchall()
        if not rows:
            return
        workers = min(8, (os.cpu_count() or 4))
        with ThreadPoolExecutor(max_workers=workers) as ex:
            tagged = list(ex.map(lambda r: (r[0], r[2], _read_tags(r[1])), rows))
        for sid, title, tags in tagged:
            if not tags:
                continue
            self.db.execute(
                "UPDATE songs SET title=?, artist=?, album=?, genre=?, year=?, track=?, duration=? WHERE id=?",
                (
                    tags.get("title") or title,
                    tags.get("artist") or None,
                    tags.get("album") or None,
                    tags.get("genre") or None,
                    tags.get("year") or 0,
                    tags.get("track") or 0,
                    tags.get("duration") or 0.0,
                    sid,
                ),
            )
        self.db.commit()

    def _row_to_song(self, r) -> Song:
        return Song(
            id=r[0], path=r[1] or "", title=r[2] or Path(r[1] or "").stem,
            artist=r[3] or "Unknown", album=r[4] or "Unknown",
            genre=r[5] or "", year=r[6] or 0, track=r[7] or 0,
            duration=r[8] or 0.0,
        )

    def songs_get(self):
        rows = self.db.execute(
            "SELECT id, path, title, artist, album, genre, year, track, duration FROM songs ORDER BY title"
        ).fetchall()
        return [self._row_to_song(r) for r in rows]

    def search(self, query: str):
        pat = f"%{query}%"
        rows = self.db.execute(
            "SELECT id, path, title, artist, album, genre, year, track, duration FROM songs "
            "WHERE title LIKE ? OR artist LIKE ? OR album LIKE ? ORDER BY title",
            (pat, pat, pat),
        ).fetchall()
        return [self._row_to_song(r) for r in rows]

    def artists_get(self):
        rows = self.db.execute(
            "SELECT DISTINCT COALESCE(NULLIF(artist,''),'Unknown') FROM songs ORDER BY artist"
        ).fetchall()
        return [r[0] for r in rows]

    def playlists_get(self):
        return self.db.execute("SELECT id, name FROM playlists ORDER BY name").fetchall()

    def playlist_create(self, name: str) -> int:
        cur = self.db.execute("INSERT INTO playlists (name) VALUES (?)", (name,))
        self.db.commit()
        return cur.lastrowid

    def playlist_songs(self, playlist_id: int):
        rows = self.db.execute(
            "SELECT s.id, s.path, s.title, s.artist, s.album, s.genre, s.year, s.track, s.duration "
            "FROM songs s JOIN playlist_songs ps ON s.id = ps.song_id "
            "WHERE ps.playlist_id=? ORDER BY ps.position", (playlist_id,),
        ).fetchall()
        return [self._row_to_song(r) for r in rows]

    def playlist_save_queue(self, name: str, songs) -> int:
        pid = self.playlist_create(name)
        for i, s in enumerate(songs):
            self.db.execute(
                "INSERT INTO playlist_songs (playlist_id, song_id, position) VALUES (?, ?, ?)",
                (pid, s.id, i),
            )
        self.db.commit()
        return pid
