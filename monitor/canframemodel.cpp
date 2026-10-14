#include "canframemodel.h"

#include "hexutil.h"

#include <QBrush>
#include <QColor>

CanFrameModel::CanFrameModel(QObject* parent) : QAbstractTableModel(parent) { timer_.start(); }

int CanFrameModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : entries_.size(); }
int CanFrameModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : ColumnCount; }

QVariant CanFrameModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= entries_.size()) return {};
    const auto& entry = entries_.at(index.row());
    if (role == RawIdRole) return entry.id;
    if (role == PayloadRole) return entry.payload;
    if (role == TransmitRole) return entry.transmitted;
    if (role == Qt::ForegroundRole && entry.transmitted) return QBrush(QColor(QStringLiteral("#2468a2")));
    if (role != Qt::DisplayRole) return {};
    switch (index.column()) {
        case Time: return QString::number(entry.microseconds / 1000000.0, 'f', 6);
        case Direction: return entry.transmitted ? QStringLiteral("TX") : QStringLiteral("RX");
        case Identifier: return QStringLiteral("0x%1").arg(entry.id, entry.id > 0x7FF ? 8 : 3, 16, QLatin1Char('0')).toUpper();
        case Length: return entry.payload.size();
        case Data: return HexUtil::format(entry.payload);
        case Decoded: return entry.decoded;
        default: return {};
    }
}

QVariant CanFrameModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    static const QStringList names{QStringLiteral("Time (s)"), QStringLiteral("Dir"), QStringLiteral("CAN ID"),
                                   QStringLiteral("DLC"), QStringLiteral("Data"), QStringLiteral("Interpretation")};
    return section >= 0 && section < names.size() ? names.at(section) : QVariant{};
}

void CanFrameModel::append(const QCanBusFrame& frame, bool transmitted, const QString& decoded) {
    const int row = entries_.size();
    beginInsertRows({}, row, row);
    entries_.append({timer_.nsecsElapsed() / 1000, transmitted, static_cast<quint32>(frame.frameId()), frame.payload(), decoded});
    endInsertRows();
}

void CanFrameModel::clear() {
    beginResetModel();
    entries_.clear();
    timer_.restart();
    endResetModel();
}
