#pragma once

#include <QSortFilterProxyModel>

class FilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit FilterProxyModel(QObject* parent = nullptr);
    void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString search_;
};
