#include "accessibility.h"
#include "placement.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <cstdio>

// AT-SPI role numbers and coordinate types, from atspi-constants.h.
static constexpr uint roleScrollPane = 49;
static constexpr uint roleDocumentWeb = 95;
static constexpr uint coordsWindow = 1;
static constexpr int timeoutMs = 100;
// How far up from the focused element the text area may be: 3, as in the
// macOS helper. And how far up the web document holding it may be.
static constexpr int surfaceDepth = 3;
static constexpr int documentDepth = 60;

static const QString nullPath = QStringLiteral("/org/a11y/atspi/null");

AccessibleFocus::AccessibleFocus(QObject *parent)
    : QObject(parent)
    , m_bus(QString())
{
}

bool AccessibleFocus::start()
{
    const QDBusReply<QString> address = QDBusConnection::sessionBus().call(
        QDBusMessage::createMethodCall(QStringLiteral("org.a11y.Bus"), QStringLiteral("/org/a11y/bus"), QStringLiteral("org.a11y.Bus"), QStringLiteral("GetAddress")),
        QDBus::Block, timeoutMs * 10);
    if (!address.isValid() || address.value().isEmpty()) return false;
    m_bus = QDBusConnection::connectToBus(address.value(), QStringLiteral("figxit-a11y"));
    if (!m_bus.isConnected()) return false;

    m_bus.connect(QString(), QString(), QStringLiteral("org.a11y.atspi.Event.Object"), QStringLiteral("StateChanged"), this,
                  SLOT(stateChanged(QDBusMessage)));
    // Apps send events only when someone listens for them.
    QDBusMessage listen = QDBusMessage::createMethodCall(QStringLiteral("org.a11y.atspi.Registry"), QStringLiteral("/org/a11y/atspi/registry"),
                                                         QStringLiteral("org.a11y.atspi.Registry"), QStringLiteral("RegisterEvent"));
    listen << QStringLiteral("object:state-changed:focused") << QStringList() << QString();
    m_bus.call(listen, QDBus::Block, timeoutMs * 10);
    return true;
}

void AccessibleFocus::stateChanged(const QDBusMessage &message)
{
    const QList<QVariant> args = message.arguments();
    if (args.size() < 2 || args.at(0).toString() != QLatin1String("focused")) return;
    const Node node{message.service(), message.path()};
    if (args.at(1).toInt() == 1) m_focused = node;
    else if (m_focused == node) m_focused.reset();
    static const bool debugging = qEnvironmentVariableIntValue("FIGXIT_DEBUG") > 0;
    if (debugging && m_focused == node) std::fprintf(stderr, "focus pid=%lld role=%u\n", pidOf(node.service), role(node));
}

qint64 AccessibleFocus::pidOf(const QString &service)
{
    const auto cached = m_pids.constFind(service);
    if (cached != m_pids.constEnd()) return *cached;
    const QDBusReply<uint> pid = m_bus.call(QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                                                           QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetConnectionUnixProcessID"))
                                                << service,
                                            QDBus::Block, timeoutMs);
    const qint64 value = pid.isValid() ? pid.value() : 0;
    m_pids.insert(service, value);
    return value;
}

std::optional<QRectF> AccessibleFocus::extents(const Node &node)
{
    QDBusMessage call = QDBusMessage::createMethodCall(node.service, node.path, QStringLiteral("org.a11y.atspi.Component"), QStringLiteral("GetExtents"));
    call << coordsWindow;
    const QDBusMessage reply = m_bus.call(call, QDBus::Block, timeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return std::nullopt;
    const QDBusArgument argument = reply.arguments().at(0).value<QDBusArgument>();
    int x = 0, y = 0, width = 0, height = 0;
    argument.beginStructure();
    argument >> x >> y >> width >> height;
    argument.endStructure();
    return QRectF(x, y, width, height);
}

std::optional<AccessibleFocus::Node> AccessibleFocus::parent(const Node &node)
{
    QDBusMessage call = QDBusMessage::createMethodCall(node.service, node.path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    call << QStringLiteral("org.a11y.atspi.Accessible") << QStringLiteral("Parent");
    const QDBusMessage reply = m_bus.call(call, QDBus::Block, timeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return std::nullopt;
    const QDBusArgument argument = reply.arguments().at(0).value<QDBusVariant>().variant().value<QDBusArgument>();
    QString service;
    QDBusObjectPath path;
    argument.beginStructure();
    argument >> service >> path;
    argument.endStructure();
    if (service.isEmpty() || path.path().isEmpty() || path.path() == nullPath) return std::nullopt;
    return Node{service, path.path()};
}

uint AccessibleFocus::role(const Node &node)
{
    const QDBusReply<uint> role = m_bus.call(
        QDBusMessage::createMethodCall(node.service, node.path, QStringLiteral("org.a11y.atspi.Accessible"), QStringLiteral("GetRole")), QDBus::Block,
        timeoutMs);
    return role.isValid() ? role.value() : 0;
}

std::optional<QRectF> AccessibleFocus::surface(qint64 pid, QPoint client)
{
    if (!m_focused || !m_bus.isConnected() || pidOf(m_focused->service) != pid) return std::nullopt;

    // The text area: the focused element or one of its first parents.
    Node node = *m_focused;
    std::optional<QRectF> area;
    for (int depth = 0; depth <= surfaceDepth && !area; ++depth) {
        const std::optional<QRectF> frame = extents(node);
        const std::optional<Node> up = parent(node);
        if (!frame || !up) return std::nullopt;
        area = visibleSurface(*frame, role(*up) == roleScrollPane, extents(*up));
        node = *up;
    }
    if (!area) return std::nullopt;

    // The web document holding it, if any, and the native view around that.
    std::optional<QRectF> document;
    std::optional<QRectF> host;
    for (int depth = 0; depth < documentDepth; ++depth) {
        if (role(node) == roleDocumentWeb) {
            document = extents(node);
            if (const std::optional<Node> up = parent(node)) host = extents(*up);
            break;
        }
        const std::optional<Node> up = parent(node);
        if (!up) break;
        node = *up;
    }
    return toGlobal(*area, document, host, client);
}
