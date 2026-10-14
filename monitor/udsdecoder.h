#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

class UdsDecoder {
public:
    static QString decodeCanFrame(const QByteArray& frame);
    static QString decodeMessage(const QByteArray& payload);
    static QString serviceName(quint8 sid);
    static QString negativeResponseName(quint8 code);
};
