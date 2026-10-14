#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

class TrafficStatistics : public QObject {
    Q_OBJECT
public:
    explicit TrafficStatistics(QObject* parent = nullptr);
    void recordRx(int bytes);
    void recordTx(int bytes);
    void reset();
    QString summary() const;

signals:
    void changed(const QString& summary);

private:
    quint64 rxFrames_{0};
    quint64 txFrames_{0};
    quint64 rxBytes_{0};
    quint64 txBytes_{0};
};
