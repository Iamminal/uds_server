#include "udsclient.h"

#include "canconnection.h"

#include <QCanBusFrame>

UdsClient::UdsClient(CanConnection* connection, QObject* parent)
    : QObject(parent), connection_(connection) {
    connect(connection_, &CanConnection::frameReceived, this, &UdsClient::processFrame);
}

void UdsClient::setIdentifiers(quint32 requestId, quint32 responseId) {
    requestId_ = requestId;
    responseId_ = responseId;
    reassembler_.clear();
}

bool UdsClient::sendCan(const QByteArray& data, QString* error) {
    QByteArray padded = data.left(8);
    padded.append(8 - padded.size(), '\0');
    return connection_->sendFrame(requestId_, padded, error);
}

bool UdsClient::sendRequest(const QByteArray& payload, QString* error) {
    if (payload.isEmpty() || payload.size() > 4095) {
        if (error) *error = QStringLiteral("UDS payload must contain 1..4095 bytes");
        return false;
    }
    if (!transmitPayload_.isEmpty()) {
        if (error) *error = QStringLiteral("an ISO-TP transmission is already active");
        return false;
    }
    if (payload.size() <= 7) return sendCan(QByteArray(1, static_cast<char>(payload.size())) + payload, error);

    QByteArray first;
    first.append(static_cast<char>(0x10 | ((payload.size() >> 8) & 0x0F)));
    first.append(static_cast<char>(payload.size() & 0xFF));
    first.append(payload.left(6));
    if (!sendCan(first, error)) return false;
    transmitPayload_ = payload;
    transmitOffset_ = 6;
    sequence_ = 1;
    return true;
}

void UdsClient::processFrame(const QCanBusFrame& frame) {
    if (frame.frameId() != responseId_ || frame.payload().isEmpty()) return;
    const QByteArray data = frame.payload();
    const quint8 type = static_cast<quint8>(data.at(0)) >> 4;
    if (type == 3 && !transmitPayload_.isEmpty()) {
        const quint8 status = static_cast<quint8>(data.at(0)) & 0x0F;
        if (status == 0) continueMultiFrame();
        else if (status == 2) {
            transmitPayload_.clear();
            emit transportError(QStringLiteral("server rejected multi-frame request"));
        }
        return;
    }
    if (type == 1) sendCan(QByteArray::fromHex("300000"));
    if (const auto complete = reassembler_.ingest(responseId_, data)) emit responseReady(*complete);
}

void UdsClient::continueMultiFrame() {
    while (transmitOffset_ < transmitPayload_.size()) {
        QByteArray frame(1, static_cast<char>(0x20 | sequence_));
        frame.append(transmitPayload_.mid(transmitOffset_, 7));
        if (!sendCan(frame)) {
            transmitPayload_.clear();
            emit transportError(QStringLiteral("failed to send ISO-TP consecutive frame"));
            return;
        }
        transmitOffset_ += 7;
        sequence_ = (sequence_ + 1) & 0x0F;
    }
    transmitPayload_.clear();
}
