#pragma once

// Finds a terminal's text area inside a bigger window, like the terminal
// panel of VS Code, the way the macOS helper does with Accessibility. Talks
// AT-SPI over D-Bus: follows which element has keyboard focus, then walks up
// from it to the first element larger than 100 by 100 pixels.
//
// Only apps that expose their accessibility tree can be found. VS Code does
// with "force-renderer-accessibility": true in ~/.vscode/argv.json.

#include <QDBusConnection>
#include <QHash>
#include <QObject>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <optional>

class QDBusMessage;

class AccessibleFocus : public QObject
{
    Q_OBJECT

public:
    explicit AccessibleFocus(QObject *parent = nullptr);

    bool start();

    // The text area with focus in the app with this pid, in global logical
    // pixels, for a window whose client area starts at client.
    std::optional<QRectF> surface(qint64 pid, QPoint client);

private Q_SLOTS:
    void stateChanged(const QDBusMessage &message);

private:
    struct Node {
        QString service;
        QString path;
        bool operator==(const Node &) const = default;
    };

    qint64 pidOf(const QString &service);
    std::optional<QRectF> extents(const Node &node);
    std::optional<Node> parent(const Node &node);
    uint role(const Node &node);

    QDBusConnection m_bus;
    std::optional<Node> m_focused;
    QHash<QString, qint64> m_pids;
};
