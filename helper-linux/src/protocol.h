#pragma once

// The messages the engine sends to the helper, one JSON object per line, and
// the parts of the replies. Same shape as
// helper/Sources/figxit-helper/Protocol.swift.

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>

class QRectF;
struct Placement;

struct Grid {
    int cols = 0;
    int rows = 0;
    int col = 0;
    int row = 0;
    std::optional<double> cellPxW;
    std::optional<double> cellPxH;
    double padX = 0;
    double padY = 0;
    bool pane = false;
    bool right = false;
};

struct Item {
    QString label;
    QString detail;
    QString icon;
    QString tint;
};

struct Request {
    QString cmd;
    std::optional<Grid> grid;
    std::optional<QList<Item>> items;
    int selected = 0;
    int caret = 0;

    static std::optional<Request> parse(const QByteArray &line);
};

QList<Item> parseItems(const QByteArray &json);
QByteArray statusReply(bool ok, const QString &error = {});

// Rect and Placement in Protocol.swift: {x, y, w, h}, and the place of the
// cursor's cell with the surface it was found in.
QJsonObject rectJson(const QRectF &rect);
QJsonObject placementJson(const Placement &placement);
