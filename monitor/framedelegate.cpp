#include "framedelegate.h"

#include "canframemodel.h"

#include <QColor>
#include <QPainter>

FrameDelegate::FrameDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void FrameDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QStyleOptionViewItem styled(option);
    const QByteArray payload = index.data(CanFrameModel::PayloadRole).toByteArray();
    if (!payload.isEmpty() && payload.contains(char(0x7F))) styled.backgroundBrush = QColor(255, 225, 225);
    QStyledItemDelegate::paint(painter, styled, index);
}
