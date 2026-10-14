#pragma once

#include "isotpreassembler.h"

#include <QByteArray>
#include <QObject>
#include <QtGlobal>

class CanConnection;
class QCanBusFrame;

class UdsClient : public QObject {
    Q_OBJECT
public:
    explicit UdsClient(CanConnection* connection, QObject* parent = nullptr);
    void setIdentifiers(quint32 requestId, quint32 responseId);
    bool sendRequest(const QByteArray& payload, QString* error = nullptr);

signals:
    void responseReady(const QByteArray& payload);
    void transportError(const QString& message);

private slots:
    void processFrame(const QCanBusFrame& frame);

private:
    CanConnection* connection_;
    quint32 requestId_{0x7E0};
    quint32 responseId_{0x7E8};
    QByteArray transmitPayload_;
    int transmitOffset_{0};
    quint8 sequence_{1};
    IsoTpReassembler reassembler_;

    bool sendCan(const QByteArray& data, QString* error = nullptr);
    void continueMultiFrame();
};
