#include "vibestream/library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sqlite3.h>

struct vs_library {
  sqlite3 *db;
};

static const char *extensions[] = {".mp3", ".mp2", ".mp1", ".mpa", ".flac", ".wav", ".wave", ".aiff", ".aif", ".aifc", ".ogg", ".oga", ".spx", ".opus", ".m4a", ".aac", ".adts", ".wma", ".ape", ".wv", ".tta", ".mpc", ".mpp", ".mp+", ".mid", ".midi", ".kar", ".rmi", ".mod", ".xm", ".s3m", ".it", ".mtm", ".umx", ".mo3", ".dsf", ".dff", ".mka", ".weba", NULL};

static int has_audio_ext(const char *path) {
  const char *dot = strrchr(path, '.');
  if (!dot) return 0;
  for (int i = 0; extensions[i]; i++) {
    if (strcasecmp(dot, extensions[i]) == 0) return 1;
  }
  return 0;
}

static void extract_ffprobe(const char *path, vs_song *song) {
  char cmd[4096];
  snprintf(cmd, sizeof(cmd),
    "ffprobe -v quiet -print_format json -show_format -show_streams "
    "\"%s\" 2>/dev/null",
    path);
  FILE *fp = popen(cmd, "r");
  if (!fp) return;

  char buf[65536];
  size_t pos = 0;
  int c;
  while ((c = fgetc(fp)) != EOF && pos < sizeof(buf) - 1)
    buf[pos++] = (char)c;
  buf[pos] = '\0';
  pclose(fp);

  char *p, *key, *val;

  p = strstr(buf, "\"artist\"");
  if (p && (p = strchr(p, ':'))) {
    while (*p && (*p == ':' || *p == ' ')) p++;
    if (*p == '"') {
      p++;
      key = p;
      while (*p && *p != '"') p++;
      *p = '\0';
      strncpy(song->artist, key, VS_ARTIST_MAX - 1);
      if (strcmp(song->artist, "Unknown") == 0 ||
          strcmp(song->artist, "unknown") == 0)
        song->artist[0] = '\0';
    }
  }

  p = strstr(buf, "\"album\"");
  if (p && (p = strchr(p, ':'))) {
    while (*p && (*p == ':' || *p == ' ')) p++;
    if (*p == '"') {
      p++;
      key = p;
      while (*p && *p != '"') p++;
      *p = '\0';
      strncpy(song->album, key, VS_ALBUM_MAX - 1);
    }
  }

  p = strstr(buf, "\"genre\"");
  if (p && (p = strchr(p, ':'))) {
    while (*p && (*p == ':' || *p == ' ')) p++;
    if (*p == '"') {
      p++;
      key = p;
      while (*p && *p != '"') p++;
      *p = '\0';
      strncpy(song->genre, key, VS_GENRE_MAX - 1);
    }
  }

  p = strstr(buf, "\"title\"");
  if (p && (p = strchr(p, ':'))) {
    while (*p && (*p == ':' || *p == ' ')) p++;
    if (*p == '"') {
      p++;
      key = p;
      while (*p && *p != '"') p++;
      *p = '\0';
      strncpy(song->title, key, VS_TITLE_MAX - 1);
    }
  }

  p = strstr(buf, "\"track\"");
  if (p && (p = strchr(p, ':'))) {
    while (*p && (*p == ':' || *p == ' ')) p++;
    val = p;
    song->track = atoi(val);
  }

  p = strstr(buf, "\"date\"");
  if (!p) p = strstr(buf, "\"year\"");
  if (p && (p = strchr(p, ':'))) {
    while (*p && (*p == ':' || *p == ' ')) p++;
    if (*p == '"') {
      p++;
      val = p;
      while (*p && *p != '"') p++;
      *p = '\0';
      song->year = atoi(val);
    } else {
      song->year = atoi(p);
    }
  }

  p = strstr(buf, "\"duration\"");
  if (p && (p = strchr(p, ':'))) {
    while (*p && (*p == ':' || *p == ' ')) p++;
    val = p;
    song->duration = atof(val);
  }

  char cover_cmd[4096];
  snprintf(cover_cmd, sizeof(cover_cmd),
    "ffmpeg -y -i \"%s\" -an -vframes 1 -f rawvideo -pix_fmt rgb24 - 2>/dev/null | "
    "head -c 1 > /dev/null 2>/dev/null && echo 1 || echo 0",
    path);
  fp = popen(cover_cmd, "r");
  if (fp) {
    int has_cover = fgetc(fp) == '1';
    pclose(fp);
    if (has_cover) {
      snprintf(song->cover_path, sizeof(song->cover_path),
        "%s.cover.rgb", path);
    }
  }
}

static void scan_dir_r(vs_library *lib, const char *dir, int recursive) {
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *ent;
  char full[VS_PATH_MAX];
  while ((ent = readdir(d))) {
    if (ent->d_name[0] == '.') continue;
    snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
    struct stat st;
    if (stat(full, &st) != 0) continue;
    if (S_ISDIR(st.st_mode)) {
      if (recursive) scan_dir_r(lib, full, recursive);
    } else if (S_ISREG(st.st_mode) && has_audio_ext(full)) {
      const char *name = ent->d_name;
      const char *dot = strrchr(name, '.');
      char title[VS_TITLE_MAX];
      if (dot) {
        size_t n = dot - name;
        if (n >= VS_TITLE_MAX) n = VS_TITLE_MAX - 1;
        strncpy(title, name, n);
        title[n] = '\0';
      } else {
        strncpy(title, name, VS_TITLE_MAX - 1);
      }

      sqlite3_stmt *stmt;
      const char *sql = "INSERT OR IGNORE INTO songs (path, title) VALUES (?, ?)";
      if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) continue;
      sqlite3_bind_text(stmt, 1, full, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, title, -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
    }
  }
  closedir(d);
}

int library_update_song_metadata(vs_library *lib, int64_t id) {
  vs_song song = {0};
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, path, title FROM songs WHERE id=? AND (artist IS NULL OR artist='')";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(stmt, 1, id);
  int rc = -1;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    song.id = sqlite3_column_int64(stmt, 0);
    strncpy(song.path, (const char *)sqlite3_column_text(stmt, 1), VS_PATH_MAX - 1);
    strncpy(song.title, (const char *)sqlite3_column_text(stmt, 2), VS_TITLE_MAX - 1);
    extract_ffprobe(song.path, &song);
    sqlite3_finalize(stmt);

    sql = "UPDATE songs SET title=?, artist=?, album=?, genre=?, year=?, track=?, duration=? WHERE id=?";
    sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, song.title[0] ? song.title : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, song.artist[0] ? song.artist : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, song.album[0] ? song.album : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, song.genre[0] ? song.genre : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, song.year);
    sqlite3_bind_int(stmt, 6, song.track);
    sqlite3_bind_double(stmt, 7, song.duration);
    sqlite3_bind_int64(stmt, 8, song.id);
    rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(stmt);
  } else {
    sqlite3_finalize(stmt);
  }
  return rc;
}

int library_update_metadata(vs_library *lib) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, path, title FROM songs WHERE artist IS NULL OR artist='' OR artist='Unknown'";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    vs_song song = {0};
    song.id = sqlite3_column_int64(stmt, 0);
    strncpy(song.path, (const char *)sqlite3_column_text(stmt, 1), VS_PATH_MAX - 1);
    strncpy(song.title, (const char *)sqlite3_column_text(stmt, 2), VS_TITLE_MAX - 1);
    extract_ffprobe(song.path, &song);

    sqlite3_stmt *upd;
    const char *usql = "UPDATE songs SET title=?, artist=?, album=?, genre=?, year=?, track=?, duration=? WHERE id=?";
    if (sqlite3_prepare_v2(lib->db, usql, -1, &upd, NULL) == SQLITE_OK) {
      sqlite3_bind_text(upd, 1, song.title[0] ? song.title : NULL, -1, SQLITE_STATIC);
      sqlite3_bind_text(upd, 2, song.artist[0] ? song.artist : NULL, -1, SQLITE_STATIC);
      sqlite3_bind_text(upd, 3, song.album[0] ? song.album : NULL, -1, SQLITE_STATIC);
      sqlite3_bind_text(upd, 4, song.genre[0] ? song.genre : NULL, -1, SQLITE_STATIC);
      sqlite3_bind_int(upd, 5, song.year);
      sqlite3_bind_int(upd, 6, song.track);
      sqlite3_bind_double(upd, 7, song.duration);
      sqlite3_bind_int64(upd, 8, song.id);
      sqlite3_step(upd);
      sqlite3_finalize(upd);
    }
  }
  sqlite3_finalize(stmt);
  return 0;
}

vs_library *library_open(const char *db_path) {
  vs_library *lib = calloc(1, sizeof(vs_library));
  if (!lib) return NULL;
  if (sqlite3_open(db_path, &lib->db) != SQLITE_OK) {
    free(lib);
    return NULL;
  }
  const char *schema =
    "CREATE TABLE IF NOT EXISTS songs ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  path TEXT UNIQUE NOT NULL,"
    "  title TEXT,"
    "  artist TEXT,"
    "  album TEXT,"
    "  genre TEXT,"
    "  year INTEGER DEFAULT 0,"
    "  track INTEGER DEFAULT 0,"
    "  duration REAL DEFAULT 0"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_songs_artist ON songs(artist);"
    "CREATE INDEX IF NOT EXISTS idx_songs_album ON songs(album);"
    "CREATE TABLE IF NOT EXISTS playlists ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS playlist_songs ("
    "  playlist_id INTEGER,"
    "  song_id INTEGER,"
    "  position INTEGER,"
    "  FOREIGN KEY(playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,"
    "  FOREIGN KEY(song_id) REFERENCES songs(id),"
    "  PRIMARY KEY(playlist_id, song_id)"
    ");";
  sqlite3_exec(lib->db, schema, NULL, NULL, NULL);
  return lib;
}

void library_close(vs_library *lib) {
  if (lib) { sqlite3_close(lib->db); free(lib); }
}

int library_scan(vs_library *lib, const char *dir, int recursive) {
  scan_dir_r(lib, dir, recursive);
  library_update_metadata(lib);
  return 0;
}

static int songs_from_stmt(sqlite3_stmt *stmt, vs_song **out, size_t *count) {
  size_t cap = 256;
  *out = malloc(cap * sizeof(vs_song));
  *count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (*count >= cap) {
      cap *= 2;
      *out = realloc(*out, cap * sizeof(vs_song));
    }
    vs_song *s = &(*out)[*count];
    s->id = sqlite3_column_int64(stmt, 0);
    strncpy(s->path, (const char *)(sqlite3_column_text(stmt, 1) ? (const char *)sqlite3_column_text(stmt, 1) : ""), VS_PATH_MAX - 1);
    strncpy(s->title, (const char *)(sqlite3_column_text(stmt, 2) ? (const char *)sqlite3_column_text(stmt, 2) : ""), VS_TITLE_MAX - 1);
    strncpy(s->artist, (const char *)(sqlite3_column_text(stmt, 3) ? (const char *)sqlite3_column_text(stmt, 3) : "Unknown"), VS_ARTIST_MAX - 1);
    strncpy(s->album, (const char *)(sqlite3_column_text(stmt, 4) ? (const char *)sqlite3_column_text(stmt, 4) : "Unknown"), VS_ALBUM_MAX - 1);
    strncpy(s->genre, (const char *)(sqlite3_column_text(stmt, 5) ? (const char *)sqlite3_column_text(stmt, 5) : ""), VS_GENRE_MAX - 1);
    s->year = sqlite3_column_int(stmt, 6);
    s->track = sqlite3_column_int(stmt, 7);
    s->duration = sqlite3_column_double(stmt, 8);
    s->playlist_id = 0;
    s->cover_path[0] = '\0';
    (*count)++;
  }
  return 0;
}

int library_songs_get(vs_library *lib, vs_song **out, size_t *count) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, path, title, artist, album, genre, year, track, duration FROM songs ORDER BY title";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  int rc = songs_from_stmt(stmt, out, count);
  sqlite3_finalize(stmt);
  return rc;
}

int library_song_by_id(vs_library *lib, int64_t id, vs_song *out) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, path, title, artist, album, genre, year, track, duration FROM songs WHERE id=?";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(stmt, 1, id);
  int rc = -1;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    out->id = sqlite3_column_int64(stmt, 0);
    strncpy(out->path, (const char *)sqlite3_column_text(stmt, 1) ? : "", VS_PATH_MAX - 1);
    strncpy(out->title, (const char *)sqlite3_column_text(stmt, 2) ? : "", VS_TITLE_MAX - 1);
    strncpy(out->artist, (const char *)sqlite3_column_text(stmt, 3) ? : "Unknown", VS_ARTIST_MAX - 1);
    strncpy(out->album, (const char *)sqlite3_column_text(stmt, 4) ? : "Unknown", VS_ALBUM_MAX - 1);
    strncpy(out->genre, (const char *)sqlite3_column_text(stmt, 5) ? : "", VS_GENRE_MAX - 1);
    out->year = sqlite3_column_int(stmt, 6);
    out->track = sqlite3_column_int(stmt, 7);
    out->duration = sqlite3_column_double(stmt, 8);
    out->playlist_id = 0;
    rc = 0;
  }
  sqlite3_finalize(stmt);
  return rc;
}

int library_artists_get(vs_library *lib, char ***out, size_t *count) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT DISTINCT COALESCE(NULLIF(artist,''),'Unknown') FROM songs WHERE artist IS NOT NULL ORDER BY artist";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  size_t cap = 64;
  *out = malloc(cap * sizeof(char *));
  *count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (*count >= cap) { cap *= 2; *out = realloc(*out, cap * sizeof(char *)); }
    const char *val = (const char *)sqlite3_column_text(stmt, 0);
    (*out)[*count] = strdup(val ? val : "Unknown");
    (*count)++;
  }
  sqlite3_finalize(stmt);
  return 0;
}

int library_albums_by_artist(vs_library *lib, const char *artist, char ***out, size_t *count) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT DISTINCT COALESCE(NULLIF(album,''),'Unknown') FROM songs WHERE artist=? AND album IS NOT NULL ORDER BY album";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(stmt, 1, artist, -1, SQLITE_STATIC);
  size_t cap = 64;
  *out = malloc(cap * sizeof(char *));
  *count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (*count >= cap) { cap *= 2; *out = realloc(*out, cap * sizeof(char *)); }
    const char *val = (const char *)sqlite3_column_text(stmt, 0);
    (*out)[*count] = strdup(val ? val : "Unknown");
    (*count)++;
  }
  sqlite3_finalize(stmt);
  return 0;
}

int library_songs_by_album(vs_library *lib, const char *artist, const char *album, vs_song **out, size_t *count) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, path, title, artist, album, genre, year, track, duration FROM songs WHERE artist=? AND album=? ORDER BY track, title";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(stmt, 1, artist, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, album, -1, SQLITE_STATIC);
  int rc = songs_from_stmt(stmt, out, count);
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlist_create(vs_library *lib, const char *name, int64_t *out_id) {
  sqlite3_stmt *stmt;
  const char *sql = "INSERT INTO playlists (name) VALUES (?)";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
  int rc = -1;
  if (sqlite3_step(stmt) == SQLITE_DONE) {
    if (out_id) *out_id = sqlite3_last_insert_rowid(lib->db);
    rc = 0;
  }
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlist_delete(vs_library *lib, int64_t id) {
  sqlite3_stmt *stmt;
  const char *sql = "DELETE FROM playlist_songs WHERE playlist_id=?";
  sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL);
  sqlite3_bind_int64(stmt, 1, id);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sql = "DELETE FROM playlists WHERE id=?";
  sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL);
  sqlite3_bind_int64(stmt, 1, id);
  int rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlists_get(vs_library *lib, vs_playlist **out, size_t *count) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, name FROM playlists ORDER BY name";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  size_t cap = 16;
  *out = malloc(cap * sizeof(vs_playlist));
  *count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (*count >= cap) { cap *= 2; *out = realloc(*out, cap * sizeof(vs_playlist)); }
    vs_playlist *pl = &(*out)[*count];
    pl->id = sqlite3_column_int64(stmt, 0);
    strncpy(pl->name, (const char *)sqlite3_column_text(stmt, 1), VS_NAME_MAX - 1);
    pl->songs = NULL; pl->count = 0; pl->capacity = 0;
    (*count)++;
  }
  sqlite3_finalize(stmt);
  return 0;
}

int library_playlist_by_id(vs_library *lib, int64_t id, vs_playlist *out) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, name FROM playlists WHERE id=?";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(stmt, 1, id);
  int rc = -1;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    out->id = sqlite3_column_int64(stmt, 0);
    strncpy(out->name, (const char *)sqlite3_column_text(stmt, 1), VS_NAME_MAX - 1);
    out->songs = NULL; out->count = 0; out->capacity = 0;
    rc = 0;
  }
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlist_clear(vs_library *lib, int64_t playlist_id) {
  sqlite3_stmt *stmt;
  const char *sql = "DELETE FROM playlist_songs WHERE playlist_id=?";
  sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL);
  sqlite3_bind_int64(stmt, 1, playlist_id);
  int rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlist_add_song(vs_library *lib, int64_t playlist_id, int64_t song_id) {
  sqlite3_stmt *stmt;
  const char *sql = "INSERT OR IGNORE INTO playlist_songs (playlist_id, song_id, position) "
    "VALUES (?, ?, (SELECT COALESCE(MAX(position),0)+1 FROM playlist_songs WHERE playlist_id=?))";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(stmt, 1, playlist_id);
  sqlite3_bind_int64(stmt, 2, song_id);
  sqlite3_bind_int64(stmt, 3, playlist_id);
  int rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlist_remove_song(vs_library *lib, int64_t playlist_id, int64_t song_id) {
  sqlite3_stmt *stmt;
  const char *sql = "DELETE FROM playlist_songs WHERE playlist_id=? AND song_id=?";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(stmt, 1, playlist_id);
  sqlite3_bind_int64(stmt, 2, song_id);
  int rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlist_songs(vs_library *lib, int64_t playlist_id, vs_song **out, size_t *count) {
  sqlite3_stmt *stmt;
  const char *sql =
    "SELECT s.id, s.path, s.title, s.artist, s.album, s.genre, s.year, s.track, s.duration "
    "FROM songs s JOIN playlist_songs ps ON s.id = ps.song_id "
    "WHERE ps.playlist_id=? ORDER BY ps.position";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(stmt, 1, playlist_id);
  int rc = songs_from_stmt(stmt, out, count);
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlist_save_queue(vs_library *lib, const char *name, vs_song *queue, size_t count) {
  int64_t pl_id;
  if (library_playlist_create(lib, name, &pl_id) != 0) return -1;
  for (size_t i = 0; i < count; i++) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO playlist_songs (playlist_id, song_id, position) VALUES (?, ?, ?)";
    if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) continue;
    sqlite3_bind_int64(stmt, 1, pl_id);
    sqlite3_bind_int64(stmt, 2, queue[i].id);
    sqlite3_bind_int(stmt, 3, (int)i);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
  return 0;
}

int library_search(vs_library *lib, const char *query, vs_song **out, size_t *count) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, path, title, artist, album, genre, year, track, duration FROM songs "
    "WHERE title LIKE ? OR artist LIKE ? OR album LIKE ? ORDER BY title";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  char pattern[VS_TITLE_MAX + 2];
  snprintf(pattern, sizeof(pattern), "%%%s%%", query);
  sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, pattern, -1, SQLITE_STATIC);
  int rc = songs_from_stmt(stmt, out, count);
  sqlite3_finalize(stmt);
  return rc;
}
