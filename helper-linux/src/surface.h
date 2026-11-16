#pragma once

// Puts the popup window at a given place without taking focus.
// Each display server needs its own way to do that: layer-shell on KDE
// Plasma and other wlroots-like compositors, something else on GNOME or X11.

#include <QRect>

class QScreen;

class PopupSurface
{
public:
    virtual ~PopupSurface() = default;

    // rect is in global logical pixels when screen is set. Without a screen,
    // the compositor picks the output and rect is relative to it.
    virtual void place(const QRect &rect, QScreen *screen) = 0;
};
