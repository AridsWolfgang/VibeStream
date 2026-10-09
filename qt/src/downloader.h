#pragma once
#include <QObject>
#include <QProcess>

// Qt port of include/vibestream/downloader.h — QProcess yt-dlp queue.
struct DlTask {
    QString url, title, path, status = "pending";
    double progress = 0.0;
};

class Downloader : public QObject {
    Q_OBJECT
public:
    explicit Downloader(const QString &dir, QObject *parent = nullptr);
    void enqueue(const QString &url);
    QList<DlTask> tasks() const { return m_tasks; }

signals:
    void tasksChanged();

private slots:
    void onReady();
    void onFinished(int code, QProcess::ExitStatus st);

private:
    void pump();
    QString m_dir;
    QList<DlTask> m_tasks;
    QProcess *m_proc = nullptr;
    int m_cur = -1;
};
