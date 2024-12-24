// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "rolegroupmodel.h"

namespace dock
{
class DockGroupModel : public RoleGroupModel
{
    Q_OBJECT

public:
    explicit DockGroupModel(QAbstractItemModel *sourceModel, int role, QObject *parent = nullptr);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &index = QModelIndex()) const override;
    Q_INVOKABLE virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

private:
    bool any(const QModelIndex &index, int role) const;
    QVariantList all(const QModelIndex &index, int role) const;

private:
    int m_roleForDeduplication;
};
}
