# VibeStream

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![TUI](https://img.shields.io/badge/UI-TUI-brightgreen)](https://invisible-island.net/ncurses/)

**VibeStream** is a feature‑rich terminal‑based music player and downloader.
Play local audio, manage playlists, and download tracks directly from YouTube and other platforms – all from the comfort of your terminal.

## Features

- Play local music (35+ formats: MP3, FLAC, WAV, AIFF, OGG, OPUS, M4A, AAC, WMA, APE, WavPack, MPC, MOD/XM/S3M/IT, MIDI, DSF, MKA, …)
- Download audio from YouTube, SoundCloud, etc. (via yt-dlp)
- Modern TUI with three-pane layout, progress bars
- Vim-inspired key bindings
- Library management with SQLite database
- Playlists, shuffle, repeat, volume control, seeking
- Highly configurable via JSON file
- Cross-platform – Linux, macOS, Windows (WSL/MSYS2)

## Quick Start

### Prerequisites

- C11 compiler (GCC, Clang, or MSVC)
- CMake 3.15+
- ncurses
- SQLite3
- [yt-dlp](https://github.com/yt-dlp/yt-dlp) (for downloads)
- [ffmpeg](https://ffmpeg.org/) (for audio conversion)

### Build from Source

```bash
git clone https://github.com/AridsWolfgang/VibeStream.git
cd VibeStream
mkdir build && cd build
cmake ..
make
sudo make install
```

### Usage

```bash
# Scan your music directory
vibestream --rescan ~/Music

# Launch the player
vibestream
```

## Key Bindings

| Key | Action |
|-----|--------|
| `Tab` | Switch focus (Library / Queue) |
| `j` / `k` | Navigate lists |
| `Enter` | Play selected song |
| `Space` | Play / Pause |
| `s` | Stop |
| `n` | Next track |
| `p` | Previous track |
| `+` / `-` | Volume up / down |
| `h` / `l` | Seek backward / forward (5s) |
| `r` | Toggle repeat (none / all / one) |
| `x` | Toggle shuffle |
| `/` | Search library |
| `a` | Add song to queue |
| `D` | Remove song from queue |
| `q` | Quit |
