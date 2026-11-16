#pragma once

#include "../locator.h"

// Loads a script into KWin that calls back over D-Bus when the active window
// or its geometry changes. The script is unloaded when the helper quits.
// Each name loads one script, so a helper on another socket, as in tests,
// leaves the script of the running helper alone.
class KWinLocator : public WindowLocator
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.figxit.Locator")

public:
    explicit KWinLocator(QString name, QObject *parent = nullptr);
    ~KWinLocator() override;

    // KWin is on the session bus.
    static bool available();

    bool start() override;
    std::optional<WindowInfo> active() const override;

public Q_SLOTS:
    Q_SCRIPTABLE void window(const QString &json);
    Q_SCRIPTABLE void noWindow();

private:
    QString m_name;
    std::optional<WindowInfo> m_active;
    bool m_loaded = false;
};
