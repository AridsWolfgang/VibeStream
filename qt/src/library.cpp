#include "library.h"
#include <QDateTime>
#include <QDirIterator>
#include <QEventLoop>
#include <QFileInfo>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>

static const char *kSchema =
    "CREATE TABLE IF NOT EXISTS songs ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  path TEXT UNIQUE NOT NULL,"
    "  title TEXT, artist TEXT, album TEXT, genre TEXT,"
    "  year INTEGER DEFAULT 0, track INTEGER DEFAULT 0,"
    "  duration REAL DEFAULT 0);"
    "CREATE INDEX IF NOT EXISTS idx_songs_artist ON songs(artist);"
    "CREATE INDEX IF NOT EXISTS idx_songs_album ON songs(album);"
    "CREATE TABLE IF NOT EXISTS playlists ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL);"
    "CREATE TABLE IF NOT EXISTS playlist_songs ("
    "  playlist_id INTEGER, song_id INTEGER, position INTEGER,"
    "  FOREIGN KEY(playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,"
    "  FOREIGN KEY(song_id) REFERENCES songs(id),"
    "  PRIMARY KEY(playlist_id, song_id));";

const QStringList &Library::extensions() {
    static const QStringList exts = {
        "mp3", "mp2", "mp1", "mpa", "flac", "wav", "wave", "aiff", "aif",
        "aifc", "ogg", "oga", "spx", "opus", "m4a", "aac", "adts", "wma",
        "ape", "wv", "tta", "mpc", "mpp", "mp+", "mid", "midi", "kar",
        "rmi", "mod", "xm", "s3m", "it", "mtm", "umx", "mo3", "dsf",
        "dff", "mka", "weba",
    };
    return exts;
}

Library::Library(QObject *parent) : QObject(parent) {}

bool Library::open(const QString &dbPath) {
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    m_db = QSqlDatabase::addDatabase("QSQLITE", "vibestream");
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) return false;
    QSqlQuery q(m_db);
    for (const QString &stmt : QString(kSchema).split(';', Qt::SkipEmptyParts))
        if (!q.exec(stmt)) return false;
    return true;
}

Song Library::probeFile(const QString &path, const QString &fallback) {
    Song s;
    s.path = path;
    s.title = fallback;
    QMediaPlayer p;
    QEventLoop loop;
    QObject::connect(&p, &QMediaPlayer::mediaStatusChanged, &loop,
                     [&](QMediaPlayer::MediaStatus st) {
                         if (st == QMediaPlayer::LoadedMedia ||
                             st == QMediaPlayer::InvalidMedia ||
                             st == QMediaPlayer::NoMedia)
                             loop.quit();
                     });
    QTimer::singleShot(1500, &loop, &QEventLoop::quit);
    p.setSource(QUrl::fromLocalFile(path));
    loop.exec();
    const QMediaMetaData md = p.metaData();
    const QString t = md.stringValue(QMediaMetaData::Title);
    if (!t.isEmpty()) s.title = t;
    QString a = md.stringValue(QMediaMetaData::AlbumArtist);
    if (a.isEmpty()) a = md.stringValue(QMediaMetaData::ContributingArtist);
    if (!a.isEmpty()) s.artist = a;
    const QString al = md.stringValue(QMediaMetaData::AlbumTitle);
    if (!al.isEmpty()) s.album = al;
    s.genre = md.stringValue(QMediaMetaData::Genre);
    const QDateTime dt = md.value(QMediaMetaData::Date).toDateTime();
    if (dt.isValid()) s.year = dt.date().year();
    s.track = md.value(QMediaMetaData::TrackNumber).toInt();
    if (p.duration() > 0) s.duration = p.duration() / 1000.0;
    return s;
}

int Library::scan(const QString &dir) {
    QDir root(dir);
    if (!root.exists()) return 0;
    QList<qint64> fresh;
    QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO songs (path, title) VALUES (?, ?)");
    int n = 0;
    while (it.hasNext()) {
        const QString fp = it.next();
        if (!extensions().contains(QFileInfo(fp).suffix().toLower())) continue;
        q.addBindValue(fp);
        q.addBindValue(QFileInfo(fp).completeBaseName());
        if (q.exec() && q.numRowsAffected() > 0) {
            fresh << q.lastInsertId().toLongLong();
            ++n;
        }
    }
    for (qint64 id : fresh) {  // tag new files only (never a full-table pass)
        QSqlQuery get(m_db);
        get.prepare("SELECT path, title FROM songs WHERE id=?");
        get.addBindValue(id);
        if (get.exec() && get.next()) {
            const Song s = probeFile(get.value(0).toString(), get.value(1).toString());
            QSqlQuery up(m_db);
            up.prepare("UPDATE songs SET title=?, artist=?, album=?, genre=?,"
                       " year=?, track=?, duration=? WHERE id=?");
            up.addBindValue(s.title);
            up.addBindValue(s.artist == "Unknown" ? QVariant(QVariant::String) : s.artist);
            up.addBindValue(s.album == "Unknown" ? QVariant(QVariant::String) : s.album);
            up.addBindValue(s.genre.isEmpty() ? QVariant(QVariant::String) : s.genre);
            up.addBindValue(s.year);
            up.addBindValue(s.track);
            up.addBindValue(s.duration);
            up.addBindValue(id);
            up.exec();
        }
    }
    return n;
}

Song Library::rowToSong(QSqlQuery &q) {
    Song s;
    s.id = q.value(0).toLongLong();
    s.path = q.value(1).toString();
    s.title = q.value(2).toString();
    if (s.title.isEmpty()) s.title = QFileInfo(s.path).completeBaseName();
    s.artist = q.value(3).toString();
    if (s.artist.isEmpty()) s.artist = "Unknown";
    s.album = q.value(4).toString();
    if (s.album.isEmpty()) s.album = "Unknown";
    s.genre = q.value(5).toString();
    s.year = q.value(6).toInt();
    s.track = q.value(7).toInt();
    s.duration = q.value(8).toDouble();
    return s;
}

static const char *kCols =
    "id, path, title, artist, album, genre, year, track, duration";

QList<Song> Library::allSongs() {
    QList<Song> out;
    QSqlQuery q(m_db);
    if (q.exec(QString("SELECT %1 FROM songs ORDER BY title").arg(kCols)))
        while (q.next()) out << rowToSong(q);
    return out;
}

QList<Song> Library::search(const QString &query) {
    QList<Song> out;
    QSqlQuery q(m_db);
    q.prepare(QString("SELECT %1 FROM songs WHERE title LIKE ?"
                      " OR artist LIKE ? OR album LIKE ? ORDER BY title").arg(kCols));
    const QString pat = "%" + query + "%";
    q.addBindValue(pat); q.addBindValue(pat); q.addBindValue(pat);
    if (q.exec())
        while (q.next()) out << rowToSong(q);
    return out;
}

QList<QPair<qint64, QString>> Library::playlists() {
    QList<QPair<qint64, QString>> out;
    QSqlQuery q(m_db);
    if (q.exec("SELECT id, name FROM playlists ORDER BY name"))
        while (q.next()) out << qMakePair(q.value(0).toLongLong(), q.value(1).toString());
    return out;
}

qint64 Library::createPlaylist(const QString &name) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO playlists (name) VALUES (?)");
    q.addBindValue(name);
    return q.exec() ? q.lastInsertId().toLongLong() : -1;
}

QList<Song> Library::playlistSongs(qint64 pid) {
    QList<Song> out;
    QSqlQuery q(m_db);
    q.prepare(QString("SELECT s.%1 FROM songs s JOIN playlist_songs ps"
                      " ON s.id = ps.song_id WHERE ps.playlist_id=?"
                      " ORDER BY ps.position").arg(QString(kCols).replace("id", "s.id").replace(", path", ", s.path").replace(", title", ", s.title").replace(", artist", ", s.artist").replace(", album", ", s.album").replace(", genre", ", s.genre").replace(", year", ", s.year").replace(", track", ", s.track").replace(", duration", ", s.duration")));
    q.addBindValue(pid);
    if (q.exec())
        while (q.next()) out << rowToSong(q);
    return out;
}

qint64 Library::saveQueue(const QString &name, const QList<Song> &queue) {
    const qint64 pid = createPlaylist(name);
    if (pid < 0) return -1;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO playlist_songs (playlist_id, song_id, position) VALUES (?, ?, ?)");
    int pos = 0;
    for (const Song &s : queue) {
        q.addBindValue(pid); q.addBindValue(s.id); q.addBindValue(pos++);
        q.exec();
    }
    return pid;
}
