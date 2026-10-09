#include "player.h"

Player::Player(QObject *parent) : QObject(parent) {
    m_player.setAudioOutput(&m_out);
    m_out.setVolume(m_volume);
    connect(&m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState s) {
                m_state = (s == QMediaPlayer::PlayingState) ? Playing
                          : (s == QMediaPlayer::PausedState) ? Paused : Stopped;
                emit stateChanged();
            });
}

bool Player::play(const QString &path) {
    m_path = path;
    m_player.setSource(QUrl::fromLocalFile(path));
    m_player.play();
    return true;
}

void Player::toggle() {
    if (m_state == Playing) m_player.pause();
    else if (m_state == Paused) m_player.play();
}

void Player::stop() { m_player.stop(); }

void Player::seek(qint64 ms) { m_player.setPosition(qMax<qint64>(0, ms)); }

void Player::seekRelative(qint64 deltaMs) { seek(position() + deltaMs); }

void Player::setVolume(double v) {
    m_volume = qBound(0.0, v, 1.0);
    m_out.setVolume(m_volume);
}

qint64 Player::position() const { return m_player.position(); }

qint64 Player::duration() const { return m_player.duration(); }
