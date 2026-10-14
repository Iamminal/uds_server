#include "filterproxymodel.h"

#include <QAbstractItemModel>

FilterProxyModel::FilterProxyModel(QObject* parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void FilterProxyModel::setSearchText(const QString& text) {
    search_ = text.trimmed();
    invalidateFilter();
}

bool FilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    if (search_.isEmpty()) return true;
    const auto* model = sourceModel();
    for (int column = 0; column < model->columnCount(sourceParent); ++column) {
        if (model->index(sourceRow, column, sourceParent).data().toString().contains(search_, Qt::CaseInsensitive)) return true;
    }
    return false;
}
