#pragma once

// Finds a terminal's text area inside a bigger window, like the terminal
// panel of VS Code, the way the macOS helper does with Accessibility. Talks
// AT-SPI over D-Bus: follows which element has keyboard focus, then walks up
// from it to the first element larger than 100 by 100 pixels.
//
// The calls block, and each show asks for the area again, so the elements
// found for the focused one are kept until focus moves. Then a show asks only
// for where they are now.
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

    // Names the element with focus, to tell later whether focus moved.
    // Empty when no element has focus.
    QString focusedId() const;

Q_SIGNALS:
    // Another element has focus, or none has.
    void focusChanged();

private Q_SLOTS:
    void stateChanged(const QDBusMessage &message);

private:
    struct Node {
        QString service;
        QString path;
        bool operator==(const Node &) const = default;
    };

    // The elements that give the text area of the focused element.
    struct Path {
        Node area;                    // the focused element or one of its first parents
        std::optional<Node> clip;     // the scroll pane around area, which hides what is outside it
        std::optional<Node> document; // the web document holding area, in Chromium
        std::optional<Node> host;     // the native view around the document
    };

    std::optional<Path> findPath(const Node &focused);
    std::optional<QRectF> measure(const Path &path, QPoint client);
    qint64 pidOf(const QString &service);
    std::optional<QRectF> extents(const Node &node);
    std::optional<Node> parent(const Node &node);
    uint role(const Node &node);

    QDBusConnection m_bus;
    std::optional<Node> m_focused;
    std::optional<Path> m_path;
    QHash<QString, qint64> m_pids;
};
