#pragma once

// What the helper needs to know about a terminal app, by its app id. Most
// need nothing: the text area comes from the window, or over AT-SPI. Another
// terminal that needs something gets a line in terminals.cpp.

#include <QString>

struct TerminalProfile {
    // Reports its cell size in device pixels, not logical ones, as Ghostty
    // does on macOS (Geometry.physicalPixels). Not checked on Linux.
    bool devicePixels = false;
};

TerminalProfile terminalProfile(const QString &appId);
