// figxit-helper for Linux: draws the engine's list of suggestions.
//
//   figxit-helper                                   listen on helper.sock
//   figxit-helper --snapshot items.json out.png     draw a list into a PNG, in the
//                 [--light | --dark] [--scale N]   desktop's colors or Breeze's
//
// For now the popup opens at a fixed place on the output the compositor
// picks: 80,80, or FIGXIT_POPUP_AT=x,y. See docs/linux-port.md.

#include "popup.h"
#include "protocol.h"
#include "server.h"
#include "surface/layershell.h"
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTimer>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

static int snapshot(const QStringList &args)
{
    const qsizetype at = args.indexOf(QStringLiteral("--snapshot"));
    if (at + 2 >= args.size()) {
        std::fprintf(stderr, "usage: figxit-helper --snapshot items.json out.png [--light | --dark] [--scale N]\n");
        return 2;
    }
    QFile file(args.at(at + 1));
    if (!file.open(QIODevice::ReadOnly)) return 2;
    const QList<Item> items = parseItems(file.readAll());
    const qsizetype scaleAt = args.indexOf(QStringLiteral("--scale"));
    const qreal scale = scaleAt >= 0 && scaleAt + 1 < args.size() ? args.at(scaleAt + 1).toDouble() : 2;

    ListWindow list;
    list.setItems(items, 0);
    const QSize size = list.sizeFor(items);
    QImage image(size * scale, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(scale);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const bool dark = args.contains(QStringLiteral("--dark"));
    list.paint(painter, size, dark || args.contains(QStringLiteral("--light")) ? breezePalette(dark) : desktopPalette());
    painter.end();
    return image.save(args.at(at + 2)) ? 0 : 3;
}

// Holds a lock next to the socket for as long as the helper runs, so a
// second helper that starts at the same time does not take the socket.
static bool lockInstance(const QString &socket)
{
    QDir().mkpath(QFileInfo(socket).absolutePath());
    const int fd = ::open(QFile::encodeName(socket + QStringLiteral(".lock")).constData(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) return true;
    if (::flock(fd, LOCK_EX | LOCK_NB) == 0) return true;
    ::close(fd);
    return false;
}

static QPoint fixedPlace()
{
    const QStringList parts = qEnvironmentVariable("FIGXIT_POPUP_AT").split(QLatin1Char(','));
    if (parts.size() == 2) return QPoint(parts.at(0).toInt(), parts.at(1).toInt());
    return QPoint(80, 80);
}

int main(int argc, char **argv)
{
    std::signal(SIGPIPE, SIG_IGN);
    const bool snapshotting = [&] {
        for (int i = 1; i < argc; ++i)
            if (qstrcmp(argv[i], "--snapshot") == 0) return true;
        return false;
    }();
    if (snapshotting) qputenv("QT_QPA_PLATFORM", "offscreen");
    // The KDE platform theme takes a second or more to load and writes
    // ~/.config/figxit-helperrc. The popup needs neither; see theme.h.
    qputenv("QT_QPA_PLATFORMTHEME", "generic");

    const QString socket = Server::defaultPath();
    if (!snapshotting && !lockInstance(socket)) {
        std::fprintf(stderr, "figxit-helper: already running at %s\n", qPrintable(socket));
        return 0;
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("figxit-helper"));
    QGuiApplication::setQuitOnLastWindowClosed(false);
    if (snapshotting) return snapshot(QGuiApplication::arguments());

    ListWindow window;
    LayerShellSurface surface(&window);
    Popup popup(&window, &surface);

    Server server(socket, [&](const QByteArray &line) -> QByteArray {
        const std::optional<Request> request = Request::parse(line);
        if (!request) return statusReply(false, QStringLiteral("bad request"));
        const QString &cmd = request->cmd;
        if (cmd == QLatin1String("show")) {
            if (!request->grid || !request->items) return statusReply(false, QStringLiteral("show needs grid and items"));
            popup.show(*request->items, request->selected, fixedPlace(), nullptr);
            return statusReply(true);
        }
        if (cmd == QLatin1String("select")) {
            if (!popup.visible()) return statusReply(false, QStringLiteral("nothing shown"));
            popup.select(request->selected);
            return statusReply(true);
        }
        if (cmd == QLatin1String("hide")) {
            popup.hide();
            return statusReply(true);
        }
        if (cmd == QLatin1String("open")) return statusReply(true);
        if (cmd == QLatin1String("quit")) {
            popup.hide();
            QTimer::singleShot(0, &app, &QCoreApplication::quit);
            return statusReply(true);
        }
        if (cmd == QLatin1String("probe")) return statusReply(false, QStringLiteral("probe is not supported on Linux yet"));
        return statusReply(false, QStringLiteral("unknown cmd"));
    });
    if (!server.start()) {
        std::fprintf(stderr, "figxit-helper: cannot listen at %s\n", qPrintable(socket));
        return 1;
    }
    return app.exec();
}
