#include "hexutil.h"

#include <QRegularExpression>

namespace HexUtil {

QString format(const QByteArray& bytes) {
    return QString::fromLatin1(bytes.toHex(' ').toUpper());
}

QByteArray parse(const QString& text, bool* ok) {
    QString compact = text;
    compact.remove(QRegularExpression(QStringLiteral("(?:0[xX]|[^0-9A-Fa-f])")));
    const bool valid = !compact.isEmpty() && compact.size() % 2 == 0 &&
                       !compact.contains(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]")));
    if (ok) *ok = valid;
    return valid ? QByteArray::fromHex(compact.toLatin1()) : QByteArray{};
}

QString ascii(const QByteArray& bytes) {
    QString result;
    result.reserve(bytes.size());
    for (const char byte : bytes) {
        const auto value = static_cast<unsigned char>(byte);
        result.append(value >= 32 && value < 127 ? QChar(value) : QChar('.'));
    }
    return result;
}

}  // namespace HexUtil
