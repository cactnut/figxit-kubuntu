// figxit-helper for Linux: draws the engine's list of suggestions.
//
//   figxit-helper                                   listen on helper.sock
//   figxit-helper --snapshot items.json out.png     draw a list into a PNG, in the
//                 [--light | --dark] [--scale N]   desktop's colors or Breeze's
//   figxit-helper --locate                          print the active window as KWin
//                                                   reports it
//
// The list opens under the cursor's cell in the active window, which a KWin
// script reports. FIGXIT_DEBUG=1 prints each placement. See docs/linux-port.md.

#include "accessibility.h"
#include "engine.h"
#include "locator/kwin.h"
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
#include <QScreen>
#include <QSocketNotifier>
#include <QTimer>
#include <csignal>
#include <cstdio>
#include <memory>
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

// Quits through the event loop on SIGTERM, SIGINT, and SIGHUP, so the KWin
// script is unloaded and the socket removed.
static int signalPipe[2];

static void quitOnSignals(QGuiApplication &app)
{
    if (::pipe2(signalPipe, O_CLOEXEC | O_NONBLOCK) != 0) return;
    auto *notifier = new QSocketNotifier(signalPipe[0], QSocketNotifier::Read, &app);
    QObject::connect(notifier, &QSocketNotifier::activated, &app, &QCoreApplication::quit);
    for (int signal : {SIGTERM, SIGINT, SIGHUP}) {
        std::signal(signal, [](int) {
            const char byte = 1;
            [[maybe_unused]] const auto written = ::write(signalPipe[1], &byte, 1);
        });
    }
}

// Prints the active window each time it changes, to see what KWin reports.
static int locate()
{
    if (!KWinLocator::available()) {
        std::fprintf(stderr, "figxit-helper: KWin is not on the session bus\n");
        return 1;
    }
    auto *locator = new KWinLocator(QStringLiteral("figxit-locator-locate"), qApp);
    QObject::connect(locator, &WindowLocator::changed, [locator] {
        const std::optional<WindowInfo> window = locator->active();
        if (!window) {
            std::printf("no window\n");
        } else {
            const QRect &r = window->client;
            std::printf("%s pid=%lld app=%s client=%d,%d %dx%d output=%s scale=%.3f\n", qPrintable(window->id), window->pid,
                        qPrintable(window->appId), r.x(), r.y(), r.width(), r.height(), qPrintable(window->output), window->scale);
            for (QScreen *screen : QGuiApplication::screens())
                if (screen->name() == window->output)
                    std::printf("  screen %s geometry=%d,%d %dx%d dpr=%.3f\n", qPrintable(screen->name()), screen->geometry().x(), screen->geometry().y(),
                                screen->geometry().width(), screen->geometry().height(), screen->devicePixelRatio());
        }
        std::fflush(stdout);
    });
    if (!locator->start()) {
        std::fprintf(stderr, "figxit-helper: cannot load the KWin script\n");
        return 1;
    }
    return qApp->exec();
}

static bool debugging()
{
    static const bool on = qEnvironmentVariableIntValue("FIGXIT_DEBUG") > 0;
    return on;
}

// Without a locator the list opens at a fixed place: 80,80 on the active
// output, or FIGXIT_POPUP_AT=x,y.
static Placement fixedPlacement()
{
    QPoint at(80, 80);
    const QStringList parts = qEnvironmentVariable("FIGXIT_POPUP_AT").split(QLatin1Char(','));
    if (parts.size() == 2) at = QPoint(parts.at(0).toInt(), parts.at(1).toInt());
    Placement placement;
    placement.source = QStringLiteral("fixed");
    placement.x = at.x();
    placement.yTop = placement.yBelow = at.y() - 2;
    return placement;
}

static QScreen *screenFor(const WindowInfo &window)
{
    for (QScreen *screen : QGuiApplication::screens())
        if (screen->name() == window.output) return screen;
    return QGuiApplication::screenAt(window.client.center());
}

// The engine and the shells skip starting Figxit again while this file is
// there. `figxit stop` puts it there; starting the helper takes it away.
static void markStopped(const QString &socket, bool stopped)
{
    QFile flag(QFileInfo(socket).absolutePath() + QStringLiteral("/stopped"));
    if (stopped) {
        if (flag.open(QIODevice::WriteOnly)) flag.close();
    } else {
        flag.remove();
    }
}

// The KWin script of the helper on the usual socket has a fixed name, so a new
// helper replaces the script a crashed one left. A helper on another socket,
// as in tests, gets a name of its own.
static QString scriptName(const QString &socket)
{
    if (!qEnvironmentVariableIsSet("FIGXIT_HELPER_SOCK")) return QStringLiteral("figxit-locator");
    return QStringLiteral("figxit-locator-") + QString::number(qHash(socket), 16);
}

class Helper
{
public:
    explicit Helper(const QString &socket)
        : m_socket(socket)
        , m_surface(&m_window)
        , m_popup(&m_window, &m_surface)
        , m_server(socket, [this](const QByteArray &line) { return handle(line); })
    {
    }

    bool start()
    {
        if (KWinLocator::available()) {
            m_locator = std::make_unique<KWinLocator>(scriptName(m_socket));
            if (m_locator->start()) {
                QObject::connect(m_locator.get(), &WindowLocator::changed, m_locator.get(), [this] { windowChanged(); });
            } else {
                std::fprintf(stderr, "figxit-helper: cannot load the KWin script, the list opens at a fixed place\n");
                m_locator.reset();
            }
        }
        m_focus = std::make_unique<AccessibleFocus>();
        if (!m_focus->start()) m_focus.reset();
        if (!m_server.start()) return false;
        m_engine = EngineSupervisor::create();
        if (m_engine) {
            markStopped(m_socket, false);
            m_engine->start();
        }
        return true;
    }

private:
    struct Shown {
        std::optional<WindowInfo> window;
        Grid grid;
        QList<Item> items;
        int selected = 0;
    };

    QByteArray handle(const QByteArray &line)
    {
        const std::optional<Request> request = Request::parse(line);
        if (!request) return statusReply(false, QStringLiteral("bad request"));
        const QString &cmd = request->cmd;
        if (cmd == QLatin1String("show")) {
            if (!request->grid || !request->items) return statusReply(false, QStringLiteral("show needs grid and items"));
            m_shown = Shown{m_locator ? m_locator->active() : std::nullopt, *request->grid, *request->items, request->selected};
            const QString why = place();
            if (why.isEmpty()) return statusReply(true);
            forget(true);
            return statusReply(false, why);
        }
        if (cmd == QLatin1String("select")) {
            if (!m_shown) return statusReply(false, QStringLiteral("nothing shown"));
            m_shown->selected = request->selected;
            m_popup.select(request->selected);
            return statusReply(true);
        }
        if (cmd == QLatin1String("hide")) {
            forget(false);
            return statusReply(true);
        }
        if (cmd == QLatin1String("open")) return statusReply(true);
        if (cmd == QLatin1String("quit")) {
            markStopped(m_socket, true);
            forget(false);
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            return statusReply(true);
        }
        if (cmd == QLatin1String("probe")) return statusReply(false, QStringLiteral("probe is not supported on Linux yet"));
        return statusReply(false, QStringLiteral("unknown cmd"));
    }

    // Opens the list under the cursor's cell. Returns why when it cannot.
    QString place()
    {
        if (!m_shown->window) {
            m_popup.show(m_shown->items, m_shown->selected, fixedPlacement(), nullptr);
            return {};
        }
        const WindowInfo &target = *m_shown->window;
        QScreen *screen = screenFor(target);
        const qreal scale = target.scale > 0 ? target.scale : screen ? screen->devicePixelRatio() : 1;
        const Grid &grid = m_shown->grid;
        // The text area when the app tells it over AT-SPI, as in VS Code; else the whole window.
        const std::optional<QRectF> area = m_focus ? m_focus->surface(target.pid, target.client.topLeft()) : std::nullopt;
        const Fit fitted = fit(grid, area.value_or(QRectF(target.client)), area.has_value(), scale, target.appId == QLatin1String("com.mitchellh.ghostty"));
        if (debugging()) {
            std::fprintf(stderr, "show app=%s client=%d,%d %dx%d scale=%.3f grid=%dx%d at %d,%d cellPx=%.1fx%.1f pane=%d",
                         qPrintable(target.appId), target.client.x(), target.client.y(), target.client.width(), target.client.height(), scale,
                         grid.cols, grid.rows, grid.col, grid.row, grid.cellPxW.value_or(0), grid.cellPxH.value_or(0), grid.pane);
            if (area) std::fprintf(stderr, " area=%.1f,%.1f %.1fx%.1f", area->x(), area->y(), area->width(), area->height());
        }
        if (!fitted.placement) {
            if (debugging()) std::fprintf(stderr, " -> %s\n", fitted.split ? "split" : "no place");
            return fitted.split ? QStringLiteral("split pane") : QStringLiteral("no place for the list");
        }
        const QRect rect = m_popup.show(m_shown->items, m_shown->selected, *fitted.placement, screen);
        if (debugging()) {
            const Placement &p = *fitted.placement;
            std::fprintf(stderr, " -> cell=%.2fx%.2f x=%.1f yTop=%.1f popup=%d,%d %dx%d\n", p.cellW, p.cellH, p.x, p.yTop, rect.x(), rect.y(),
                         rect.width(), rect.height());
        }
        return {};
    }

    // Follows the terminal when it moves, and closes the list when another
    // window becomes active.
    void windowChanged()
    {
        if (!m_shown || !m_shown->window) return;
        const std::optional<WindowInfo> active = m_locator->active();
        if (!active || active->id != m_shown->window->id) return forget(true);
        if (*active == *m_shown->window) return;
        m_shown->window = active;
        if (!place().isEmpty()) forget(true);
    }

    void forget(bool tellEngine)
    {
        m_shown.reset();
        m_popup.hide();
        if (tellEngine) m_server.broadcast(QByteArrayLiteral(R"({"event":"hidden"})"));
    }

    QString m_socket;
    ListWindow m_window;
    LayerShellSurface m_surface;
    Popup m_popup;
    Server m_server;
    std::unique_ptr<KWinLocator> m_locator;
    std::unique_ptr<AccessibleFocus> m_focus;
    std::unique_ptr<EngineSupervisor> m_engine;
    std::optional<Shown> m_shown;
};

int main(int argc, char **argv)
{
    std::signal(SIGPIPE, SIG_IGN);
    const auto hasArg = [&](const char *name) {
        for (int i = 1; i < argc; ++i)
            if (qstrcmp(argv[i], name) == 0) return true;
        return false;
    };
    const bool snapshotting = hasArg("--snapshot");
    const bool locating = hasArg("--locate");
    if (snapshotting) qputenv("QT_QPA_PLATFORM", "offscreen");
    // The KDE platform theme takes a second or more to load and writes
    // ~/.config/figxit-helperrc. The popup needs neither; see theme.h.
    qputenv("QT_QPA_PLATFORMTHEME", "generic");

    const QString socket = Server::defaultPath();
    if (!snapshotting && !locating && !lockInstance(socket)) {
        std::fprintf(stderr, "figxit-helper: already running at %s\n", qPrintable(socket));
        return 0;
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("figxit-helper"));
    QGuiApplication::setQuitOnLastWindowClosed(false);
    if (snapshotting) return snapshot(QGuiApplication::arguments());
    quitOnSignals(app);
    if (locating) return locate();

    Helper helper(socket);
    if (!helper.start()) {
        std::fprintf(stderr, "figxit-helper: cannot listen at %s\n", qPrintable(socket));
        return 1;
    }
    return app.exec();
}
