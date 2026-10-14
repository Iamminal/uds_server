#include "udsdecoder.h"

#include "hexutil.h"

#include <QHash>

QString UdsDecoder::serviceName(quint8 sid) {
    static const QHash<quint8, QString> names{
        {0x10, QStringLiteral("DiagnosticSessionControl")}, {0x11, QStringLiteral("ECUReset")},
        {0x14, QStringLiteral("ClearDiagnosticInformation")}, {0x19, QStringLiteral("ReadDTCInformation")},
        {0x22, QStringLiteral("ReadDataByIdentifier")}, {0x27, QStringLiteral("SecurityAccess")},
        {0x28, QStringLiteral("CommunicationControl")}, {0x2E, QStringLiteral("WriteDataByIdentifier")},
        {0x31, QStringLiteral("RoutineControl")}, {0x3E, QStringLiteral("TesterPresent")}};
    if (names.contains(sid)) return names.value(sid);
    if (sid >= 0x40 && names.contains(sid - 0x40)) return names.value(sid - 0x40) + QStringLiteral(" positive response");
    return QStringLiteral("SID 0x%1").arg(sid, 2, 16, QLatin1Char('0')).toUpper();
}

QString UdsDecoder::negativeResponseName(quint8 code) {
    static const QHash<quint8, QString> names{
        {0x10, QStringLiteral("generalReject")}, {0x11, QStringLiteral("serviceNotSupported")},
        {0x12, QStringLiteral("subFunctionNotSupported")}, {0x13, QStringLiteral("incorrectLengthOrFormat")},
        {0x21, QStringLiteral("busyRepeatRequest")}, {0x22, QStringLiteral("conditionsNotCorrect")},
        {0x24, QStringLiteral("requestSequenceError")}, {0x31, QStringLiteral("requestOutOfRange")},
        {0x33, QStringLiteral("securityAccessDenied")}, {0x35, QStringLiteral("invalidKey")},
        {0x36, QStringLiteral("exceededAttempts")}, {0x37, QStringLiteral("delayNotExpired")},
        {0x78, QStringLiteral("responsePending")}, {0x7E, QStringLiteral("subFunctionNotSupportedInSession")},
        {0x7F, QStringLiteral("serviceNotSupportedInSession")}};
    return names.value(code, QStringLiteral("NRC 0x%1").arg(code, 2, 16, QLatin1Char('0')).toUpper());
}

QString UdsDecoder::decodeCanFrame(const QByteArray& frame) {
    if (frame.isEmpty()) return QStringLiteral("empty CAN frame");
    const quint8 pci = static_cast<quint8>(frame.at(0));
    switch (pci >> 4) {
        case 0: return QStringLiteral("ISO-TP single (%1): %2").arg(pci & 0x0F).arg(decodeMessage(frame.mid(1, pci & 0x0F)));
        case 1: return frame.size() >= 2
                           ? QStringLiteral("ISO-TP first, total %1").arg(((pci & 0x0F) << 8) | static_cast<quint8>(frame.at(1)))
                           : QStringLiteral("malformed ISO-TP first frame");
        case 2: return QStringLiteral("ISO-TP consecutive SN=%1").arg(pci & 0x0F);
        case 3: return QStringLiteral("ISO-TP flow control FS=%1 BS=%2 STmin=0x%3")
                       .arg(pci & 0x0F).arg(frame.size() > 1 ? static_cast<quint8>(frame.at(1)) : 0)
                       .arg(frame.size() > 2 ? static_cast<quint8>(frame.at(2)) : 0, 2, 16, QLatin1Char('0')).toUpper();
        default: return QStringLiteral("unknown ISO-TP PCI");
    }
}

QString UdsDecoder::decodeMessage(const QByteArray& payload) {
    if (payload.isEmpty()) return QStringLiteral("empty UDS message");
    const quint8 sid = static_cast<quint8>(payload.at(0));
    if (sid == 0x7F && payload.size() >= 3) {
        return QStringLiteral("negative %1: %2")
            .arg(serviceName(static_cast<quint8>(payload.at(1))), negativeResponseName(static_cast<quint8>(payload.at(2))));
    }
    QString detail = serviceName(sid);
    const quint8 requestSid = sid >= 0x40 ? sid - 0x40 : sid;
    if ((requestSid == 0x22 || requestSid == 0x2E) && payload.size() >= 3) {
        const quint16 did = (static_cast<quint8>(payload.at(1)) << 8) | static_cast<quint8>(payload.at(2));
        detail += QStringLiteral(" DID=0x%1").arg(did, 4, 16, QLatin1Char('0')).toUpper();
    } else if (payload.size() >= 2) {
        detail += QStringLiteral(" sub=0x%1").arg(static_cast<quint8>(payload.at(1)), 2, 16, QLatin1Char('0')).toUpper();
    }
    return detail;
}
