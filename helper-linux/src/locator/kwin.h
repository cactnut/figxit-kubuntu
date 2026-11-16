#pragma once

#include "../locator.h"

// Loads a script into KWin that calls back over D-Bus when the active window
// or its geometry changes. The script is unloaded when the helper quits.
class KWinLocator : public WindowLocator
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.figxit.Locator")

public:
    explicit KWinLocator(QObject *parent = nullptr);
    ~KWinLocator() override;

    static bool available();
    bool start();
    std::optional<WindowInfo> active() const override;

public Q_SLOTS:
    Q_SCRIPTABLE void window(const QString &json);
    Q_SCRIPTABLE void noWindow();

private:
    std::optional<WindowInfo> m_active;
    bool m_loaded = false;
};
