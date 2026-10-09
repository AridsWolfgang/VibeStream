#pragma once
#include <QListWidget>
#include <QMainWindow>
#include <QSlider>
#include <QTableWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTimer;
#include "downloader.h"
#include "library.h"
#include "player.h"

// Classic music-app layout: sidebar nav | song table | bottom now-playing bar.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Library *lib, Player *player, Downloader *dl,
                        QWidget *parent = nullptr);

private slots:
    void onNavChanged(int row);
    void onSearchChanged(const QString &text);
    void doSearch();
    void playLibraryRow(int row);
    void playQueueRow(int row);
    void playPlaylistRow(int row);
    void onToggle();
    void onNext();
    void onPrev();
    void onStop();
    void onAddToQueue();
    void onRepeat();
    void onShuffle();
    void onVolume(int v);
    void onSeekReleased();
    void onTick();
    void onRescan();
    void onChooseFolder();
    void onDownloadUrl();
    void onSaveQueue();
    void refreshPlaylists();

private:
    void buildUi();
    void applyTheme();
    void showSongs(const QList<Song> &songs, QTableWidget *t);
    void refreshQueueTable();
    void playAt(int i);

    Library *m_lib;
    Player *m_player;
    Downloader *m_dl;
    QList<Song> m_shown, m_queue;
    int m_qindex = 0;
    int m_repeat = 0;  // 0 none, 1 all, 2 one
    bool m_shuffle = false;
    bool m_seeking = false;
    QTimer *m_searchTimer;

    QListWidget *m_nav, *m_playlists;
    QStackedWidget *m_stack;
    QTableWidget *m_songTable, *m_queueTable, *m_plTable;
    QLineEdit *m_search;
    QLabel *m_title, *m_artist, *m_times, *m_cover, *m_status;
    QPushButton *m_playBtn, *m_repeatBtn, *m_shuffleBtn;
    QSlider *m_progress, *m_volume;
    qint64 m_currentPl = -1;
};
