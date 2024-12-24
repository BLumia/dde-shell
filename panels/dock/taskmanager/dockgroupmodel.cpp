// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dockgroupmodel.h"
#include "rolegroupmodel.h"
#include "taskmanager.h"

namespace dock
{
DockGroupModel::DockGroupModel(QAbstractItemModel *sourceModel, int role, QObject *parent)
    : RoleGroupModel(sourceModel, role, parent)
    , m_roleForDeduplication(role)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent, int first, int last) {
        if (!parent.isValid())
            return;
        Q_EMIT dataChanged(index(parent.row(), 0), index(parent.row(), 0), {TaskManager::WindowsRole});
    });
    connect(this, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex &parent, int first, int last) {
        if (!parent.isValid())
            return;
        Q_EMIT dataChanged(index(parent.row(), 0), index(parent.row(), 0), {TaskManager::WindowsRole});
    });
}

QVariant DockGroupModel::data(const QModelIndex &index, int role) const
{
    if (role == m_roleForDeduplication) {
        return RoleGroupModel::data(index, role);
    }

    if (TaskManager::WindowsRole == role) {
        QStringList stringList;
        auto variantList = all(index, role);
        std::transform(variantList.begin(), variantList.end(), std::back_inserter(stringList), [](const QVariant &var) {
            return var.toString();
        });
        return stringList;
    } else if (TaskManager::ActiveRole == role || TaskManager::AttentionRole == role) {
        return any(index, role);
    }

    return RoleGroupModel::data(index, role);
}

int DockGroupModel::rowCount(const QModelIndex &index) const
{
    return index.isValid() ? 0 : RoleGroupModel::rowCount();
}

QModelIndex DockGroupModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    return RoleGroupModel::index(row, column);
}

bool DockGroupModel::any(const QModelIndex &index, int role) const
{
    auto rowCount = RoleGroupModel::rowCount(index);
    for (int i = 0; i < rowCount; i++) {
        auto cIndex = RoleGroupModel::index(i, 0, index);
        if (RoleGroupModel::data(cIndex, role).toBool())
            return true;
    }

    return false;
}

QVariantList DockGroupModel::all(const QModelIndex &index, int role) const
{
    QVariantList res;
    auto rowCount = RoleGroupModel::rowCount(index);
    for (int i = 0; i < rowCount; i++) {
        auto cIndex = RoleGroupModel::index(i, 0, index);
        auto window = RoleGroupModel::data(index, role);
        if (window.isValid())
            res.append(window);
    }

    return res;
}

}