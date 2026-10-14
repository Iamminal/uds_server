#pragma once

#include <QByteArray>
#include <QString>

namespace HexUtil {
QString format(const QByteArray& bytes);
QByteArray parse(const QString& text, bool* ok = nullptr);
QString ascii(const QByteArray& bytes);
}
