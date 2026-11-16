#pragma once

// The helper itself: answers the engine's requests and opens the list of
// suggestions under the cursor's cell. What depends on the desktop comes in
// from outside, from backends.h in the app and as fakes in tests, and main.cpp
// carries the requests and the events over helper.sock.

#include "focus.h"
#include "locator.h"
#include "placement.h"
#include "popup.h"
#include "protocol.h"
#include "surface.h"

#include <QByteArray>
#include <QObject>
#include <QRectF>
#include <QString>
#include <memory>
#include <optional>

class QScreen;

class Helper : public QObject
{
    Q_OBJECT

public:
    // What depends on the desktop. Without a surface the list cannot open;
    // without a locator it opens at a fixed place; without focus it is placed
    // in the whole window.
    struct Backends {
        std::unique_ptr<PopupSurface> surface;
        std::unique_ptr<WindowLocator> locator;
        std::unique_ptr<FocusTracker> focus;
    };

    Helper(ListWindow *window, Backends backends);

    // Answers one request line from the engine with one reply line.
    QByteArray handle(const QByteArray &line);

Q_SIGNALS:
    // The list closed without the engine asking: another window or element
    // took focus, or the list found no place. {"event":"hidden"} tells it.
    void hidden();
    // The engine asked the helper to quit.
    void quitRequested();

private:
    struct Shown {
        std::optional<WindowInfo> window;
        Grid grid;
        QList<Item> items;
        int selected = 0;
        QString focus; // the focused element the list was placed under, when AT-SPI told where it is
    };

    struct Located {
        Fit fitted;
        std::optional<QRectF> area; // the text area, when the app tells it over AT-SPI
        qreal scale = 1;
        QScreen *screen = nullptr;
    };

    Located locate(const Grid &grid, const WindowInfo &target);
    QByteArray probe(const std::optional<Grid> &grid);
    QString place();
    void windowChanged();
    void focusChanged();
    void forget(bool tellEngine);

    std::unique_ptr<PopupSurface> m_surface;
    std::unique_ptr<WindowLocator> m_locator;
    std::unique_ptr<FocusTracker> m_focus;
    Popup m_popup;
    std::optional<Shown> m_shown;
};
