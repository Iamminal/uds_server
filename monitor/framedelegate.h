#pragma once

#include <QStyledItemDelegate>

class FrameDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FrameDelegate(QObject* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
