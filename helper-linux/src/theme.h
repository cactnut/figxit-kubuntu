#pragma once

// The popup's colors. The helper runs without a Qt platform theme, which
// starts slowly and wants a config file of its own, so it reads the colors
// itself. KDE keeps them in kdeglobals.

#include <QPalette>
#include <QString>

QPalette breezePalette(bool dark);
QPalette paletteFromKdeGlobals(const QString &text);
QPalette desktopPalette();
