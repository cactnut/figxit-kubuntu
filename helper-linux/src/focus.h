#pragma once

// Follows which element has keyboard focus inside apps, and tells where the
// text area around it is, for apps whose terminal fills only part of a
// window, like VS Code. AT-SPI on Linux (accessibility.h); tests give the
// helper a fake.

#include <QObject>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <optional>

class FocusTracker : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // The text area with focus in the app with this pid, in global logical
    // pixels, for a window whose client area starts at client.
    virtual std::optional<QRectF> surface(qint64 pid, QPoint client) = 0;

    // Names the element with focus, to tell later whether focus moved.
    // Empty when no element has focus.
    virtual QString focusedId() const = 0;

Q_SIGNALS:
    // Another element has focus, or none has.
    void focusChanged();
};
