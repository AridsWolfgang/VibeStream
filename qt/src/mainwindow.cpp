#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

static QString fmtMs(qint64 ms) {
    const qint64 s = ms / 1000;
    return QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

MainWindow::MainWindow(Library *lib, Player *player, Downloader *dl, QWidget *parent)
    : QMainWindow(parent), m_lib(lib), m_player(player), m_dl(dl) {
    buildUi();
    applyTheme();
    m_songTable->setRowCount(0);
    showSongs(m_lib->allSongs(), m_songTable);
    refreshPlaylists();
    QTimer *tick = new QTimer(this);
    connect(tick, &QTimer::timeout, this, &MainWindow::onTick);
    tick->start(500);
}

void MainWindow::applyTheme() {
    // Classic dark music-app theme, Atlas accent.
    qApp->setStyleSheet(R"(
      QMainWindow, QWidget { background: #0A0A0A; color: #FAFAF8; }
      QListWidget, QTableWidget { background: #161616; border: 1px solid #242424;
        alternate-background-color: #101010; outline: 0; }
      QTableWidget { gridline-color: #242424; selection-background-color: #FAFAF8;
        selection-color: #0A0A0A; }
      QListWidget::item:selected { background: #FAFAF8; color: #0A0A0A; }
      QHeaderView::section { background: #0A0A0A; color: #969694; border: none;
        padding: 6px; font-size: 10px; }
      QLineEdit { background: #161616; border: 1px solid #3E3E3E; padding: 7px;
        selection-background-color: #FF3B30; }
      QPushButton { background: #FAFAF8; color: #0A0A0A; border: none;
        padding: 7px 14px; font-weight: bold; }
      QPushButton:hover { background: #FF3B30; color: #FFFFFF; }
      QPushButton:checked { background: #FF3B30; color: #FFFFFF; }
      QPushButton:disabled { background: #282828; color: #6E6E6C; }
      QSlider::groove:horizontal { background: #282828; height: 4px; }
      QSlider::handle:horizontal { background: #FF3B30; width: 12px; height: 12px;
        margin: -4px 0; border-radius: 6px; }
      QSplitter::handle { background: #242424; }
      QMenuBar, QMenu { background: #0A0A0A; color: #FAFAF8; }
      QMenu::item:selected { background: #FF3B30; }
      QLabel#npTitle { font-size: 16px; font-weight: bold; }
      QLabel#npArtist { color: #969694; }
      QLabel#cover { background: #282828; border: 1px solid #3E3E3E;
        font-size: 28px; color: #6E6E6C; }
      QLabel#cover[playing="true"] { border: 2px solid #FF3B30; color: #FF3B30; }
    )");
}

static QTableWidget *makeTable() {
    QTableWidget *t = new QTableWidget;
    t->setColumnCount(4);
    t->setHorizontalHeaderLabels({"TITLE", "ARTIST", "ALBUM", "TIME"});
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    t->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    t->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->verticalHeader()->setVisible(false);
    t->setAlternatingRowColors(true);
    return t;
}

void MainWindow::buildUi() {
    auto *file = menuBar()->addMenu("&File");
    file->addAction("Choose music folder…", this, &MainWindow::onChooseFolder);
    file->addAction("Rescan library", this, &MainWindow::onRescan);
    file->addSeparator();
    file->addAction("Quit", qApp, &QApplication::quit);
    auto *dl = menuBar()->addMenu("&Download");
    dl->addAction("Paste URL…", this, &MainWindow::onDownloadUrl);
    auto *pl = menuBar()->addMenu("&Playlist");
    pl->addAction("Save queue as playlist…", this, &MainWindow::onSaveQueue);

    auto *split = new QSplitter(this);
    setCentralWidget(split);

    // sidebar
    auto *side = new QWidget;
    auto *sideLay = new QVBoxLayout(side);
    sideLay->setContentsMargins(0, 0, 0, 0);
    auto *brand = new QLabel("VibeStream");
    brand->setStyleSheet("font-size: 22px; font-weight: bold; padding: 12px;");
    sideLay->addWidget(brand);
    m_nav = new QListWidget;
    m_nav->addItems({"All Songs", "Queue"});
    sideLay->addWidget(m_nav);
    auto *plHead = new QLabel("PLAYLISTS");
    plHead->setStyleSheet("color: #969694; font-size: 10px; padding: 8px 8px 0 8px;");
    sideLay->addWidget(plHead);
    m_playlists = new QListWidget;
    sideLay->addWidget(m_playlists, 1);
    connect(m_nav, &QListWidget::currentRowChanged, this, &MainWindow::onNavChanged);
    connect(m_playlists, &QListWidget::currentRowChanged, this, [this](int r) {
        if (r < 0) return;
        m_nav->setCurrentRow(-1);
        const auto pls = m_lib->playlists();
        if (r < pls.size()) {
            m_currentPl = pls[r].first;
            showSongs(m_lib->playlistSongs(m_currentPl), m_plTable);
            m_stack->setCurrentIndex(2);
        }
    });
    split->addWidget(side);

    // center
    auto *center = new QWidget;
    auto *cLay = new QVBoxLayout(center);
    cLay->setContentsMargins(8, 8, 8, 8);
    m_search = new QLineEdit;
    m_search->setPlaceholderText("Search title, artist, album…");
    m_search->setClearButtonEnabled(true);
    cLay->addWidget(m_search);
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(180);
    connect(m_search, &QLineEdit::textChanged, m_searchTimer,
            static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_searchTimer, &QTimer::timeout, this, &MainWindow::doSearch);
    connect(m_search, &QLineEdit::returnPressed, this, &MainWindow::doSearch);
    m_stack = new QStackedWidget;
    m_songTable = makeTable();
    m_queueTable = makeTable();
    m_plTable = makeTable();
    m_stack->addWidget(m_songTable);
    m_stack->addWidget(m_queueTable);
    m_stack->addWidget(m_plTable);
    cLay->addWidget(m_stack, 1);
    connect(m_songTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::playLibraryRow);
    connect(m_queueTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::playQueueRow);
    connect(m_plTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::playPlaylistRow);
    split->addWidget(center);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({220, 960});
    m_nav->setCurrentRow(0);

    // bottom now-playing bar
    auto *bar = new QWidget;
    bar->setFixedHeight(92);
    auto *bLay = new QHBoxLayout(bar);
    m_cover = new QLabel("♪");
    m_cover->setObjectName("cover");
    m_cover->setAlignment(Qt::AlignCenter);
    m_cover->setFixedSize(60, 60);
    bLay->addWidget(m_cover);
    auto *meta = new QWidget;
    auto *metaLay = new QVBoxLayout(meta);
    metaLay->setContentsMargins(0, 0, 0, 0);
    m_title = new QLabel("Nothing playing");
    m_title->setObjectName("npTitle");
    m_artist = new QLabel("Double-click a song to start");
    m_artist->setObjectName("npArtist");
    metaLay->addWidget(m_title);
    metaLay->addWidget(m_artist);
    bLay->addWidget(meta);
    auto mkBtn = [this, bLay](const QString &t, auto slot) {
        auto *b = new QPushButton(t);
        connect(b, &QPushButton::clicked, this, slot);
        bLay->addWidget(b);
        return b;
    };
    mkBtn("⏮", &MainWindow::onPrev);
    m_playBtn = mkBtn("▶", &MainWindow::onToggle);
    mkBtn("⏭", &MainWindow::onNext);
    mkBtn("⏹", &MainWindow::onStop);
    auto *addBtn = mkBtn("+ Queue", &MainWindow::onAddToQueue);
    addBtn->setToolTip("Add selected library song to queue");
    auto *progBox = new QWidget;
    auto *progLay = new QVBoxLayout(progBox);
    progLay->setContentsMargins(0, 0, 0, 0);
    m_progress = new QSlider(Qt::Horizontal);
    m_progress->setRange(0, 1000);
    m_times = new QLabel("0:00 / 0:00");
    m_times->setStyleSheet("color: #969694; font-size: 11px;");
    progLay->addWidget(m_progress);
    progLay->addWidget(m_times);
    bLay->addWidget(progBox, 1);
    connect(m_progress, &QSlider::sliderPressed, this, [this] { m_seeking = true; });
    connect(m_progress, &QSlider::sliderReleased, this, &MainWindow::onSeekReleased);
    m_repeatBtn = new QPushButton("Repeat: Off");
    m_shuffleBtn = new QPushButton("Shuffle: Off");
    m_shuffleBtn->setCheckable(true);
    connect(m_repeatBtn, &QPushButton::clicked, this, &MainWindow::onRepeat);
    connect(m_shuffleBtn, &QPushButton::clicked, this, &MainWindow::onShuffle);
    bLay->addWidget(m_repeatBtn);
    bLay->addWidget(m_shuffleBtn);
    m_volume = new QSlider(Qt::Horizontal);
    m_volume->setRange(0, 100);
    m_volume->setFixedWidth(110);
    m_volume->setValue(75);
    connect(m_volume, &QSlider::valueChanged, this, &MainWindow::onVolume);
    bLay->addWidget(new QLabel("Vol"));
    bLay->addWidget(m_volume);

    auto *dock = new QDockWidget(this);
    dock->setWidget(bar);
    dock->setTitleBarWidget(new QWidget(dock));
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::BottomDockWidgetArea, dock);

    m_status = new QLabel;
    statusBar()->addWidget(m_status, 1);
    m_status->setText("Ready — double-click a song");

    QSettings s;
    m_player->setVolume(s.value("volume", 0.75).toDouble());
    m_volume->setValue(int(m_player->volume() * 100));
}

// ---- slots ----
void MainWindow::onNavChanged(int row) {
    if (row < 0) return;
    m_playlists->setCurrentRow(-1);
    m_currentPl = -1;
    m_stack->setCurrentIndex(row == 1 ? 1 : 0);
    if (row == 1) refreshQueueTable();
}

void MainWindow::doSearch() {
    const QString q = m_search->text().trimmed();
    showSongs(q.isEmpty() ? m_lib->allSongs() : m_lib->search(q), m_songTable);
    m_nav->setCurrentRow(0);
    m_stack->setCurrentIndex(0);
}

void MainWindow::onSearchChanged(const QString &) {}

void MainWindow::showSongs(const QList<Song> &songs, QTableWidget *t) {
    if (t == m_songTable) m_shown = songs;
    t->setRowCount(songs.size());
    for (int i = 0; i < songs.size(); ++i) {
        const Song &s = songs[i];
        t->setItem(i, 0, new QTableWidgetItem(s.title));
        t->setItem(i, 1, new QTableWidgetItem(s.artist));
        t->setItem(i, 2, new QTableWidgetItem(s.album));
        const QString dur = s.duration > 0 ? fmtMs(qint64(s.duration * 1000)) : "—";
        auto *di = new QTableWidgetItem(dur);
        di->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        t->setItem(i, 3, di);
    }
}

void MainWindow::refreshQueueTable() {
    m_queueTable->setRowCount(m_queue.size());
    for (int i = 0; i < m_queue.size(); ++i) {
        const Song &s = m_queue[i];
        const QString mark = (i == m_qindex && m_player->state() != Player::Stopped) ? "▶ " : "";
        m_queueTable->setItem(i, 0, new QTableWidgetItem(mark + s.title));
        m_queueTable->setItem(i, 1, new QTableWidgetItem(s.artist));
        m_queueTable->setItem(i, 2, new QTableWidgetItem(s.album));
        m_queueTable->setItem(i, 3, new QTableWidgetItem(
            s.duration > 0 ? fmtMs(qint64(s.duration * 1000)) : "—"));
    }
}

void MainWindow::playAt(int i) {
    if (m_queue.isEmpty()) return;
    m_qindex = qBound(0, i, m_queue.size() - 1);
    const Song &s = m_queue[m_qindex];
    if (m_player->play(s.path)) {
        m_title->setText(s.title);
        m_artist->setText(s.artist + "  ·  " + s.album);
        m_status->setText("Playing: " + s.artist + " — " + s.title);
    } else {
        QMessageBox::warning(this, "Playback failed", "Cannot play:\n" + s.path);
    }
    refreshQueueTable();
}

void MainWindow::playLibraryRow(int row) {
    if (row < 0 || row >= m_shown.size()) return;
    m_queue << m_shown[row];
    playAt(m_queue.size() - 1);
}

void MainWindow::playQueueRow(int row) {
    if (row >= 0 && row < m_queue.size()) playAt(row);
}

void MainWindow::playPlaylistRow(int row) {
    if (m_currentPl < 0) return;
    const QList<Song> songs = m_lib->playlistSongs(m_currentPl);
    if (row < 0 || row >= songs.size()) return;
    m_queue << songs[row];
    playAt(m_queue.size() - 1);
}

void MainWindow::onToggle() {
    if (m_player->state() == Player::Stopped) playAt(m_qindex);
    else m_player->toggle();
    refreshQueueTable();
}

void MainWindow::onNext() {
    if (m_queue.isEmpty()) return;
    if (m_repeat == 2) playAt(m_qindex);
    else if (m_qindex + 1 < m_queue.size()) playAt(m_qindex + 1);
    else if (m_repeat == 1) playAt(0);
    else { m_player->stop(); refreshQueueTable(); }
}

void MainWindow::onPrev() {
    if (m_player->position() > 3000) m_player->seek(0);
    else if (m_qindex > 0) playAt(m_qindex - 1);
}

void MainWindow::onStop() {
    m_player->stop();
    refreshQueueTable();
}

void MainWindow::onAddToQueue() {
    const int row = m_songTable->currentRow();
    if (row >= 0 && row < m_shown.size()) {
        m_queue << m_shown[row];
        refreshQueueTable();
        m_status->setText("Queued: " + m_shown[row].title);
    }
}

void MainWindow::onRepeat() {
    m_repeat = (m_repeat + 1) % 3;
    static const char *k[] = {"Repeat: Off", "Repeat: All", "Repeat: One"};
    m_repeatBtn->setText(k[m_repeat]);
}

void MainWindow::onShuffle() {
    m_shuffle = !m_shuffle;
    m_shuffleBtn->setChecked(m_shuffle);
    m_shuffleBtn->setText(m_shuffle ? "Shuffle: On" : "Shuffle: Off");
    if (m_shuffle && m_queue.size() > 1) {
        const qint64 curId = m_queue[m_qindex].id;
        for (int i = m_queue.size() - 1; i > 0; --i)
            m_queue.swapItemsAt(i, QRandomGenerator::global()->bounded(i + 1));
        m_qindex = 0;
        for (int i = 0; i < m_queue.size(); ++i)
            if (m_queue[i].id == curId) { m_qindex = i; break; }
        refreshQueueTable();
    }
}

void MainWindow::onVolume(int v) {
    m_player->setVolume(v / 100.0);
    QSettings().setValue("volume", m_player->volume());
}

void MainWindow::onSeekReleased() {
    m_seeking = false;
    const qint64 dur = m_player->duration();
    if (dur > 0) m_player->seek(dur * m_progress->value() / 1000);
}

void MainWindow::onTick() {
    const qint64 pos = m_player->position(), dur = m_player->duration();
    const bool playing = m_player->state() == Player::Playing;
    m_playBtn->setText(playing ? "⏸" : "▶");
    m_cover->setProperty("playing", playing);
    m_cover->style()->unpolish(m_cover);
    m_cover->style()->polish(m_cover);
    if (dur > 0) {
        m_times->setText(fmtMs(pos) + " / " + fmtMs(dur));
        if (!m_seeking) m_progress->setValue(int(pos * 1000 / dur));
        if (playing && dur - pos < 400) onNext();
    }
    if (!m_dl->tasks().isEmpty()) {
        int active = 0;
        for (const DlTask &t : m_dl->tasks())
            if (t.status == "downloading" || t.status == "pending") ++active;
        if (active > 0) m_status->setText(QString("Downloads active: %1").arg(active));
    }
}

void MainWindow::onRescan() {
    QSettings s;
    const QString dir = s.value("musicDir", QDir::homePath() + "/Music").toString();
    m_status->setText("Scanning " + dir + "…");
    qApp->processEvents();
    const int n = m_lib->scan(dir);
    showSongs(m_lib->allSongs(), m_songTable);
    m_status->setText(QString("Scan done — %1 new files").arg(n));
}

void MainWindow::onChooseFolder() {
    QSettings s;
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Choose music folder", s.value("musicDir", QDir::homePath() + "/Music").toString());
    if (!dir.isEmpty()) {
        s.setValue("musicDir", dir);
        onRescan();
    }
}

void MainWindow::onDownloadUrl() {
    const QString url = QInputDialog::getText(this, "Download", "YouTube / SoundCloud URL:");
    if (!url.isEmpty()) {
        m_dl->enqueue(url);
        m_status->setText("Download queued: " + url);
    }
}

void MainWindow::onSaveQueue() {
    if (m_queue.isEmpty()) return;
    const QString name = QInputDialog::getText(this, "Save playlist", "Playlist name:");
    if (!name.isEmpty() && m_lib->saveQueue(name, m_queue) >= 0) {
        refreshPlaylists();
        m_status->setText("Playlist saved: " + name);
    }
}

void MainWindow::refreshPlaylists() {
    m_playlists->clear();
    for (const auto &p : m_lib->playlists()) m_playlists->addItem(p.second);
}
