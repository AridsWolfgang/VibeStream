#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include "downloader.h"
#include "library.h"
#include "mainwindow.h"
#include "player.h"

static QString dbPath() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return base + "/vibestream/library.db";
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("VibeStream");
    QCoreApplication::setApplicationName("VibeStreamQt");

    Library lib;
    if (!lib.open(dbPath())) return 1;

    // headless rescan: vibestream-qt --rescan [dir]
    const QStringList args = app.arguments();
    if (args.contains("--rescan") || args.contains("-r")) {
        int i = qMax(args.indexOf("--rescan"), args.indexOf("-r"));
        QString dir = (i + 1 < args.size() && !args[i + 1].startsWith('-'))
                          ? args[i + 1]
                          : QSettings().value("musicDir", QDir::homePath() + "/Music").toString();
        const int n = lib.scan(dir);
        QTextStream(stdout) << "Added " << n << " new files.\n";
        return 0;
    }

    Player player;
    QSettings s;
    player.setVolume(s.value("volume", 0.75).toDouble());
    Downloader dl(s.value("downloadDir",
                          QDir::homePath() + "/Music/vibestream").toString());

    MainWindow w(&lib, &player, &dl);
    w.resize(1180, 720);
    w.show();
    const int rc = app.exec();
    s.setValue("volume", player.volume());
    return rc;
}
