#include "trafficstatistics.h"

#include <QString>

TrafficStatistics::TrafficStatistics(QObject* parent) : QObject(parent) {}

void TrafficStatistics::recordRx(int bytes) {
    ++rxFrames_;
    rxBytes_ += static_cast<quint64>(bytes);
    emit changed(summary());
}

void TrafficStatistics::recordTx(int bytes) {
    ++txFrames_;
    txBytes_ += static_cast<quint64>(bytes);
    emit changed(summary());
}

void TrafficStatistics::reset() {
    rxFrames_ = txFrames_ = rxBytes_ = txBytes_ = 0;
    emit changed(summary());
}

QString TrafficStatistics::summary() const {
    return QStringLiteral("RX %1 frames / %2 bytes    TX %3 frames / %4 bytes")
        .arg(rxFrames_).arg(rxBytes_).arg(txFrames_).arg(txBytes_);
}
