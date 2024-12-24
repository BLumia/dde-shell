// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractProxyModel>
#include <QPointer>
#include <tuple>

namespace dock
{
class DockItemModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit DockItemModel(QAbstractItemModel *appsModel, QAbstractItemModel *m_activeAppModel, QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    inline int mapToSourceModelRole(QAbstractItemModel *model, int role) const;

private:
    void loadDockedElements();

private:
    // id, model, and pos
    QList<std::tuple<QString, QAbstractItemModel *, int>> m_data;

    // type, id
    QList<std::tuple<QString, QString>> m_dockedElements;
    QAbstractItemModel *m_appsModel;
    QAbstractItemModel *m_activeAppModel;
};
}
