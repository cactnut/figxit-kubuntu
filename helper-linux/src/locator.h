#pragma once

// Tells where the active window is. Wayland does not let a client see other
// windows, so each desktop needs its own way: a KWin script on KDE Plasma,
// a Shell extension on GNOME, swaymsg or hyprctl elsewhere. backends.cpp
// picks one.

#include <QObject>
#include <QRect>
#include <QString>
#include <optional>

struct WindowInfo {
    QString id;
    qint64 pid = 0;
    QString appId;
    QRect client; // without decorations, in global logical pixels
    QString output;
    qreal scale = 0; // the output's scale, fractional ones too; 0 when unknown

    bool operator==(const WindowInfo &) const = default;
};

class WindowLocator : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Starts following the active window. False when the desktop refuses.
    virtual bool start() = 0;
    virtual std::optional<WindowInfo> active() const = 0;

Q_SIGNALS:
    void changed();
};
