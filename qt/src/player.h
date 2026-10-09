#pragma once
#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>

// Qt port of include/vibestream/player.h — QMediaPlayer backend
// (FFmpeg backend plays all 39 formats; miniaudio-based player.c
//  can't compile under MSVC due to pthread.h).
class Player : public QObject {
    Q_OBJECT
public:
    enum State { Stopped, Playing, Paused };
    explicit Player(QObject *parent = nullptr);

    bool play(const QString &path);
    void toggle();
    void stop();
    void seek(qint64 ms);
    void seekRelative(qint64 deltaMs);
    void setVolume(double v);
    double volume() const { return m_volume; }
    qint64 position() const;
    qint64 duration() const;
    State state() const { return m_state; }
    QString currentPath() const { return m_path; }

signals:
    void stateChanged();

private:
    QMediaPlayer m_player;
    QAudioOutput m_out;
    State m_state = Stopped;
    QString m_path;
    double m_volume = 0.75;
};
