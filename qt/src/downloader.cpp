#include "downloader.h"
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>

Downloader::Downloader(const QString &dir, QObject *parent)
    : QObject(parent), m_dir(dir) {
    QDir().mkpath(dir);
}

void Downloader::enqueue(const QString &url) {
    DlTask t;
    t.url = url;
    m_tasks << t;
    emit tasksChanged();
    pump();
}

void Downloader::pump() {
    if (m_proc) return;
    m_cur = -1;
    for (int i = 0; i < m_tasks.size(); ++i)
        if (m_tasks[i].status == "pending") { m_cur = i; break; }
    if (m_cur < 0) return;
    QString exe = QStandardPaths::findExecutable("yt-dlp");
    if (exe.isEmpty()) {
        m_tasks[m_cur].status = "failed";
        emit tasksChanged();
        pump();
        return;
    }
    m_tasks[m_cur].status = "downloading";
    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &Downloader::onReady);
    connect(m_proc, &QProcess::readyReadStandardError, this, &Downloader::onReady);
    connect(m_proc, &QProcess::finished, this, &Downloader::onFinished);
    m_proc->setProgram(exe);
    m_proc->setArguments({"--extract-audio", "--audio-format", "mp3", "--no-playlist",
                          "--progress", "--newline", "-o",
                          m_dir + "/%(title)s.%(ext)s", m_tasks[m_cur].url});
    m_proc->start();
    emit tasksChanged();
}

void Downloader::onReady() {
    if (!m_proc || m_cur < 0) return;
    static const QRegularExpression re(R"((\d+(?:\.\d+)?)%)");
    const QString out = m_proc->readAllStandardOutput() + m_proc->readAllStandardError();
    QRegularExpressionMatch m;
    for (const QString &line : out.split('\n'))
        if ((m = re.match(line)).hasMatch())
            m_tasks[m_cur].progress = m.captured(1).toDouble() / 100.0;
    emit tasksChanged();
}

void Downloader::onFinished(int code, QProcess::ExitStatus) {
    if (m_cur >= 0) {
        m_tasks[m_cur].status = (code == 0) ? "done" : "failed";
        if (code == 0) m_tasks[m_cur].progress = 1.0;
    }
    m_proc->deleteLater();
    m_proc = nullptr;
    emit tasksChanged();
    pump();
}
