#pragma once

// Picks the parts of the helper that depend on the desktop and the display
// server. Another desktop gets a WindowLocator or a PopupSurface of its own,
// and one line here in backends.cpp.

#include <QString>
#include <memory>

class FocusTracker;
class PopupSurface;
class QWindow;
class WindowLocator;

// The first locator that works on this desktop, started, or null when none
// does. FIGXIT_WINDOW_BACKEND names the one to use, or "none". name tells
// this helper's locator apart from others on the desktop, like KWin's script.
std::unique_ptr<WindowLocator> startLocator(const QString &name);

// How to put the popup at a place on this display server, or null when the
// helper has no way to.
std::unique_ptr<PopupSurface> makeSurface(QWindow *window);

// Follows keyboard focus over AT-SPI, which works on any Linux desktop, or
// null when there is no AT-SPI bus.
std::unique_ptr<FocusTracker> startFocus();
