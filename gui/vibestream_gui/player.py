"""Mirrors include/vibestream/player.h — pygame-ce audio backend."""
import time

STOPPED, PLAYING, PAUSED = 0, 1, 2


class Player:
    def __init__(self):
        import pygame

        pygame.mixer.init()
        self._pg = pygame
        self.state = STOPPED
        self.volume = 0.75
        self.crossfade = 2.0
        self.current_path = ""
        self._started_at = 0.0
        self._offset = 0.0
        self._paused_pos = 0.0
        self._duration = 0.0
        self._dur_cache = {}
        self.set_volume(self.volume)

    def _probe_duration(self, path: str) -> float:
        if path in self._dur_cache:
            return self._dur_cache[path]
        try:
            import mutagen

            f = mutagen.File(path)
            if f is not None and getattr(f, "info", None) is not None:
                dur = float(getattr(f.info, "length", 0.0) or 0.0)
                self._dur_cache[path] = dur
                return dur
        except Exception:
            pass
        return 0.0

    def play(self, path: str) -> int:
        try:
            fade_ms = int(max(0.0, min(10.0, self.crossfade)) * 1000)
            self._pg.mixer.music.load(path)
            self._pg.mixer.music.set_volume(self.volume)
            self._pg.mixer.music.play(fade_ms=fade_ms if fade_ms > 0 else 0)
            self.current_path = path
            self._duration = self._probe_duration(path)
            self._started_at = time.monotonic()
            self._offset = 0.0
            self.state = PLAYING
            return 0
        except Exception:
            return -1

    def toggle(self) -> int:
        if self.state == PLAYING:
            self._pg.mixer.music.pause()
            self._paused_pos = self.position()
            self.state = PAUSED
        elif self.state == PAUSED:
            self._pg.mixer.music.unpause()
            self._started_at = time.monotonic() - (self._paused_pos - self._offset)
            self.state = PLAYING
        else:
            return -1
        return 0

    def stop(self) -> int:
        self._pg.mixer.music.stop()
        self.state = STOPPED
        return 0

    def seek(self, seconds: float) -> int:
        try:
            seconds = max(0.0, seconds)
            self._pg.mixer.music.play(start=seconds)
            self._offset = seconds
            self._started_at = time.monotonic() - seconds
            self.state = PLAYING
            return 0
        except Exception:
            return -1

    def seek_relative(self, delta: float) -> int:
        return self.seek(self.position() + delta)

    def set_volume(self, vol: float):
        self.volume = max(0.0, min(1.0, vol))
        self._pg.mixer.music.set_volume(self.volume)

    def position(self) -> float:
        if self.state == STOPPED:
            return 0.0
        if self.state == PAUSED:
            return self._paused_pos
        ms = self._pg.mixer.music.get_pos()
        if ms < 0:
            # mixer clock unavailable (e.g. dummy driver): fall back to wall clock
            return time.monotonic() - self._started_at
        # get_pos() resets on play()/seek(), so add the seek offset back
        return self._offset + ms / 1000.0

    def duration(self) -> float:
        return self._duration
