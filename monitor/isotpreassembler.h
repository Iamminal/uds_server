#pragma once

#include <QByteArray>
#include <QHash>
#include <QtGlobal>

#include <optional>

class IsoTpReassembler {
public:
    std::optional<QByteArray> ingest(quint32 canId, const QByteArray& frame);
    void reset(quint32 canId);
    void clear();

private:
    struct State {
        QByteArray data;
        int expected{0};
        quint8 sequence{1};
    };
    QHash<quint32, State> states_;
};
