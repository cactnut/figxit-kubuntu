#pragma once

// Runs figxit-engine from the folder the helper is in, and starts it again
// when it stops, like EngineSupervisor in helper/Sources/figxit-helper/main.swift.

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>
#include <memory>

class EngineSupervisor : public QObject
{
    Q_OBJECT

public:
    // Null when FIGXIT_NO_ENGINE is set or there is no figxit-engine next to the helper.
    static std::unique_ptr<EngineSupervisor> create();

    explicit EngineSupervisor(QString program);
    ~EngineSupervisor() override;

    void start();
    void stop();

private:
    void exited();

    QString m_program;
    QProcess m_process;
    QElapsedTimer m_started;
    int m_failures = 0;
    bool m_stopping = false;
};
