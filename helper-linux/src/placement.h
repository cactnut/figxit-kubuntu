#pragma once

// Where the cursor's cell is on screen, from the engine's grid and the
// terminal's window. A port of Geometry.fit in
// helper/Sources/figxit-helper/Geometry.swift, with its tests in
// tests/placement_test.cpp.

#include "protocol.h"

#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <optional>

struct Placement {
    QString source; // "ax" when base is the text area itself, "cg" when it is the window
    QRectF surface;
    qreal cellW = 0;
    qreal cellH = 0;
    qreal x = 0;
    qreal yTop = 0;
    qreal yBelow = 0;
};

struct Fit {
    std::optional<Placement> placement;
    bool split = false; // the grid fills only part of the window, like a split pane
};

// base is the terminal's text area when focused is true, else its window
// without decorations. A shell's text area sits at the bottom of the window,
// under any tab bar or menu bar. scale is the screen's device pixel ratio.
Fit fit(const Grid &grid, const QRectF &base, bool focused, qreal scale, bool physical);

// The popup's place: under the cursor's line, above it when there is no room
// below, and inside bounds when bounds is not empty.
QRect popupRect(const Placement &placement, QSize size, const QRect &bounds);

// The part of an accessible element that shows: clipped to its parent when the
// parent is a scroll pane, and nothing when it is 100 pixels or less across.
// A port of Geometry.visibleSurface.
std::optional<QRectF> visibleSurface(const QRectF &frame, bool inScrollPane, const std::optional<QRectF> &parent);

// Turns an element's extents, in window coordinates, into global logical
// pixels. In Chromium, elements of a web document report device pixels while
// the native view around the document reports logical ones; comparing the
// document with that host gives the scale and the offset. Without a document,
// extents are already logical and start at the window's client area.
QRectF toGlobal(const QRectF &extents, const std::optional<QRectF> &document, const std::optional<QRectF> &host, QPointF client);
