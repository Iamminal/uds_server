#pragma once

#include <QAbstractTableModel>
#include <QCanBusFrame>
#include <QElapsedTimer>
#include <QVector>

class CanFrameModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { Time, Direction, Identifier, Length, Data, Decoded, ColumnCount };
    enum Role { RawIdRole = Qt::UserRole + 1, PayloadRole, TransmitRole };

    explicit CanFrameModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void append(const QCanBusFrame& frame, bool transmitted, const QString& decoded = {});
    void clear();

private:
    struct Entry {
        qint64 microseconds;
        bool transmitted;
        quint32 id;
        QByteArray payload;
        QString decoded;
    };
    QVector<Entry> entries_;
    QElapsedTimer timer_;
};
