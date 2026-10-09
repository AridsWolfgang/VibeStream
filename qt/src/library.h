#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QStringList>

// Qt port of include/vibestream/library.h — same SQLite schema,
// so existing library.db files are reused as-is.
struct Song {
    qint64 id = 0;
    QString title, artist = "Unknown", album = "Unknown", genre, path;
    int year = 0, track = 0;
    double duration = 0.0;
};

class Library : public QObject {
    Q_OBJECT
public:
    explicit Library(QObject *parent = nullptr);
    bool open(const QString &dbPath);
    int scan(const QString &dir);                       // returns # new files
    QList<Song> allSongs();
    QList<Song> search(const QString &q);
    QList<QPair<qint64, QString>> playlists();
    qint64 createPlaylist(const QString &name);
    QList<Song> playlistSongs(qint64 pid);
    qint64 saveQueue(const QString &name, const QList<Song> &queue);
    static const QStringList &extensions();

private:
    QSqlDatabase m_db;
    Song probeFile(const QString &path, const QString &fallbackTitle);
    static Song rowToSong(class QSqlQuery &q);
};
