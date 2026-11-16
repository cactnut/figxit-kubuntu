#pragma once

#include "../surface.h"

class QWindow;

namespace LayerShellQt
{
class Window;
}

// A wlr-layer-shell surface on the overlay layer, anchored to the top left
// of the output and moved with its margins. It takes no keyboard focus.
class LayerShellSurface : public PopupSurface
{
public:
    explicit LayerShellSurface(QWindow *window);

    void place(const QRect &rect, QScreen *screen) override;

private:
    QWindow *m_window;
    LayerShellQt::Window *m_layer;
};
