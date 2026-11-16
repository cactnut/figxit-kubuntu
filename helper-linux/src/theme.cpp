#include "theme.h"

#include <QDir>
#include <QFile>

QPalette breezePalette(bool dark)
{
    QPalette palette;
    palette.setColor(QPalette::Window, dark ? QColor(32, 35, 38) : QColor(239, 240, 241));
    palette.setColor(QPalette::WindowText, dark ? QColor(252, 252, 252) : QColor(35, 38, 41));
    palette.setColor(QPalette::Highlight, QColor(61, 174, 233));
    palette.setColor(QPalette::HighlightedText, QColor(252, 252, 252));
    return palette;
}

QPalette paletteFromKdeGlobals(const QString &text)
{
    QPalette palette = breezePalette(false);
    QString section;
    for (const QString &raw : text.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            section = line;
            continue;
        }
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals < 0) continue;
        const QStringList rgb = line.mid(equals + 1).split(QLatin1Char(','));
        if (rgb.size() < 3) continue;
        const QColor color(rgb.at(0).trimmed().toInt(), rgb.at(1).trimmed().toInt(), rgb.at(2).trimmed().toInt());
        const QString key = section + line.left(equals).trimmed();
        if (key == QLatin1String("[Colors:Window]BackgroundNormal")) palette.setColor(QPalette::Window, color);
        else if (key == QLatin1String("[Colors:Window]ForegroundNormal")) palette.setColor(QPalette::WindowText, color);
        else if (key == QLatin1String("[Colors:Selection]BackgroundNormal")) palette.setColor(QPalette::Highlight, color);
        else if (key == QLatin1String("[Colors:Selection]ForegroundNormal")) palette.setColor(QPalette::HighlightedText, color);
    }
    return palette;
}

QPalette desktopPalette()
{
    QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (!config.startsWith(QLatin1Char('/'))) config = QDir::homePath() + QStringLiteral("/.config");
    QFile file(config + QStringLiteral("/kdeglobals"));
    if (!file.open(QIODevice::ReadOnly)) return breezePalette(false);
    return paletteFromKdeGlobals(QString::fromUtf8(file.readAll()));
}
