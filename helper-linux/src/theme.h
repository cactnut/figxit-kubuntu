#pragma once

// The popup's colors. The helper runs without a Qt platform theme, which
// starts slowly and wants a config file of its own, so it reads the colors
// itself. KDE keeps them in kdeglobals.

#include <QPalette>
#include <QString>

QPalette breezePalette(bool dark);
QPalette paletteFromKdeGlobals(const QString &text);

// The desktop's colors: from kdeglobals on KDE Plasma. Elsewhere Breeze's,
// light or dark as the desktop portal's color-scheme says. The portal answers
// later, so a change shows from the list after the next one; call this once
// at start, so the first list has it.
QPalette desktopPalette();
