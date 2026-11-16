// The fit tests are ported from helper/Tests/GeometryTests.swift.

#include "../src/placement.h"
#include "../src/protocol.h"

#include <QTest>

class PlacementTest : public QObject
{
    Q_OBJECT

    const QRectF window{100, 80, 987, 777};

    static Grid grid(const char *json)
    {
        return *Request::parse(QByteArray(R"({"cmd":"show","grid":)") + json + "}")->grid;
    }

    Fit fit(const char *json, std::optional<QRectF> base = std::nullopt, bool focused = false)
    {
        return ::fit(grid(json), base.value_or(window), focused, 1, false);
    }

private Q_SLOTS:
    void shellThatFillsTheWindowIsPlaced()
    {
        const Fit result = fit(R"({"cols":120,"rows":35,"col":6,"row":2,"cellPxW":8,"cellPxH":21,"pane":true})");
        QVERIFY(!result.split);
        QCOMPARE(result.placement->x, 148);
        QCOMPARE(result.placement->yTop, 80 + 42 + 42);
    }

    void halfWidthPaneWithoutAccessibilityIsASplit()
    {
        const Fit result = fit(R"({"cols":58,"rows":33,"col":6,"row":0,"cellPxW":8,"cellPxH":21,"pane":true})");
        QVERIFY(!result.placement);
        QVERIFY(result.split);
    }

    void tmuxInHalfTheWindowIsASplit()
    {
        const Fit result = fit(R"({"cols":58,"rows":33,"col":6,"row":0,"cellPxW":8,"cellPxH":21})");
        QVERIFY(!result.placement);
        QVERIFY(result.split);
    }

    void focusedPaneIsPlacedInsideThePane()
    {
        const Fit result = fit(R"({"cols":58,"rows":33,"col":6,"row":1,"cellPxW":8,"cellPxH":21,"pane":true})", QRectF(594, 141, 476, 715), true);
        QCOMPARE(result.placement->source, QStringLiteral("ax"));
        QCOMPARE(result.placement->x, 594 + 48);
        QCOMPARE(result.placement->yTop, 141 + 21);
    }

    void editorTerminalWithoutCellSizeUsesTheSurface()
    {
        const Fit result = fit(R"({"cols":149,"rows":25,"col":8,"row":3,"pane":true})", QRectF(455, 525, 1256, 425), true);
        QCOMPARE(result.placement->cellH, 17);
        QVERIFY(qAbs(result.placement->x - (455 + 8 * 1256.0 / 149)) < 0.001);
        QCOMPARE(result.placement->yTop, 525 + 51);
    }

    void cellSizeInPixelsOnAHiDpiScreenStillFits()
    {
        const Fit result = ::fit(grid(R"({"cols":120,"rows":35,"col":0,"row":0,"cellPxW":16,"cellPxH":42,"pane":true})"), window, false, 2, false);
        QVERIFY(!result.split);
        QCOMPARE(result.placement->cellW, 8);
    }

    void textAreaUnderATabBarCountsFromTheBottom()
    {
        // Konsole: menu bar and tab bar above the terminal, 40 rows of 17 px under them.
        const Fit result = fit(R"({"cols":100,"rows":40,"col":10,"row":0,"cellPxW":9,"cellPxH":17,"pane":true})", QRectF(0, 0, 920, 760));
        QCOMPARE(result.placement->yTop, 760 - 40 * 17);
        QCOMPARE(result.placement->x, 90);
    }

    void popupOpensUnderTheLine()
    {
        const Placement placement{QStringLiteral("cg"), {}, 8, 20, 300, 400, 420};
        QCOMPARE(popupRect(placement, QSize(260, 98), QRect(0, 0, 1920, 1080)), QRect(300, 422, 260, 98));
    }

    void popupOpensAboveTheLineNearTheBottom()
    {
        const Placement placement{QStringLiteral("cg"), {}, 8, 20, 300, 1000, 1020};
        QCOMPARE(popupRect(placement, QSize(260, 98), QRect(0, 0, 1920, 1080)), QRect(300, 1000 - 2 - 98, 260, 98));
    }

    void popupStaysOnItsScreen()
    {
        const Placement right{QStringLiteral("cg"), {}, 8, 20, 1800, 400, 420};
        QCOMPARE(popupRect(right, QSize(260, 98), QRect(1920, 0, 1920, 1080)).x(), 1920);
        QCOMPARE(popupRect(right, QSize(260, 98), QRect(0, 0, 1920, 1080)).x(), 1920 - 260);
    }

    void popupWithoutBoundsIsNotMoved()
    {
        const Placement placement{QStringLiteral("cg"), {}, 8, 20, 80, 58, 78};
        QCOMPARE(popupRect(placement, QSize(260, 98), QRect()), QRect(80, 80, 260, 98));
    }
};

QTEST_GUILESS_MAIN(PlacementTest)
#include "placement_test.moc"
