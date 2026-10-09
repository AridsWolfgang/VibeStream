"""Mirrors include/vibestream/downloader.h — yt-dlp wrapper."""
import shutil
import subprocess
import threading
from dataclasses import dataclass, field
from pathlib import Path

PENDING, DOWNLOADING, DONE, FAILED = "pending", "downloading", "done", "failed"


@dataclass
class DownloadTask:
    url: str
    title: str = ""
    path: str = ""
    status: str = PENDING
    progress: float = 0.0


class Downloader:
    def __init__(self, download_dir: str):
        self.download_dir = download_dir
        Path(download_dir).mkdir(parents=True, exist_ok=True)
        self.tasks: list[DownloadTask] = []
        self._lock = threading.Lock()

    def enqueue(self, url: str) -> int:
        with self._lock:
            self.tasks.append(DownloadTask(url=url))
            return len(self.tasks) - 1

    def _run_one(self, task: DownloadTask):
        exe = shutil.which("yt-dlp") or "yt-dlp"
        out = str(Path(self.download_dir) / "%(title)s.%(ext)s")
        cmd = [exe, "--extract-audio", "--audio-format", "mp3",
               "--no-playlist", "-o", out, task.url]
        try:
            cp = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
            task.status = DONE if cp.returncode == 0 else FAILED
            task.progress = 1.0 if cp.returncode == 0 else 0.0
        except Exception:
            task.status = FAILED

    def process(self, background: bool = True):
        with self._lock:
            todo = [t for t in self.tasks if t.status == PENDING]
        for t in todo:
            t.status = DOWNLOADING
            if background:
                threading.Thread(target=self._run_one, args=(t,), daemon=True).start()
            else:
                self._run_one(t)

    def tasks_get(self):
        with self._lock:
            return list(self.tasks)
