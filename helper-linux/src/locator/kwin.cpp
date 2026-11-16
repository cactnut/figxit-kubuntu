#include "kwin.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <cmath>

static const QString kwinService = QStringLiteral("org.kde.KWin");
static const QString pluginName = QStringLiteral("figxit-locator");

static QDBusInterface scripting()
{
    return QDBusInterface(kwinService, QStringLiteral("/Scripting"), QStringLiteral("org.kde.kwin.Scripting"));
}

// Writes the script where KWin can read it: the runtime folder, or the temp
// folder when that is not writable.
static QString writeScript(const QString &service)
{
    QFile source(QStringLiteral(":/kwin-locator.js"));
    if (!source.open(QIODevice::ReadOnly)) return {};
    const QByteArray script = source.readAll().replace("FIGXIT_SERVICE", service.toUtf8());
    for (const QString &dir : {QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation), QDir::tempPath()}) {
        if (dir.isEmpty()) continue;
        QFile file(dir + QStringLiteral("/figxit-locator.js"));
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(script) == script.size()) return file.fileName();
    }
    return {};
}

KWinLocator::KWinLocator(QObject *parent)
    : WindowLocator(parent)
{
}

KWinLocator::~KWinLocator()
{
    if (m_loaded) scripting().call(QStringLiteral("unloadScript"), pluginName);
}

bool KWinLocator::available()
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    return bus.isConnected() && bus.interface()->isServiceRegistered(kwinService);
}

bool KWinLocator::start()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QStringLiteral("/Locator"), this, QDBusConnection::ExportScriptableSlots)) return false;
    const QString path = writeScript(bus.baseService());
    if (path.isEmpty()) return false;
    QDBusInterface kwin = scripting();
    kwin.call(QStringLiteral("unloadScript"), pluginName);
    const QDBusReply<int> id = kwin.call(QStringLiteral("loadScript"), path, pluginName);
    if (!id.isValid() || id.value() < 0) return false;
    m_loaded = true;
    kwin.call(QStringLiteral("start"));
    return true;
}

std::optional<WindowInfo> KWinLocator::active() const
{
    return m_active;
}

void KWinLocator::window(const QString &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json.toUtf8()).object();
    WindowInfo info;
    info.id = object.value(QLatin1String("id")).toString();
    info.pid = object.value(QLatin1String("pid")).toInteger();
    info.appId = object.value(QLatin1String("appId")).toString();
    info.client = QRect(std::lround(object.value(QLatin1String("x")).toDouble()),
                        std::lround(object.value(QLatin1String("y")).toDouble()),
                        std::lround(object.value(QLatin1String("width")).toDouble()),
                        std::lround(object.value(QLatin1String("height")).toDouble()));
    info.output = object.value(QLatin1String("output")).toString();
    info.scale = object.value(QLatin1String("scale")).toDouble();
    if (m_active == info) return;
    m_active = info;
    Q_EMIT changed();
}

void KWinLocator::noWindow()
{
    if (!m_active) return;
    m_active.reset();
    Q_EMIT changed();
}
