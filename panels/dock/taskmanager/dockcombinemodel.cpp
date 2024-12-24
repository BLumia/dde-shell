// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dockcombinemodel.h"
#include "globals.h"
#include "rolecombinemodel.h"
#include "taskmanager.h"

namespace dock
{
DockCombineModel::DockCombineModel(QAbstractItemModel *major, QAbstractItemModel *minor, int majorRoles, CombineFunc func, QObject *parent)
    : RoleCombineModel(major, minor, majorRoles, func, parent)
{
    // due to role has changed by RoleGroupModel, so we redirect role to TaskManager::Roles.
    m_roleMaps = {{TaskManager::ActiveRole, RoleCombineModel::roleNames().key(MODEL_ACTIVE)},
                  {TaskManager::AttentionRole, RoleCombineModel::roleNames().key(MODEL_ATTENTION)},
                  {TaskManager::DesktopIdRole, RoleCombineModel::roleNames().key(MODEL_DESKTOPID)},
                  {TaskManager::IconNameRole, RoleCombineModel::roleNames().key(MODEL_ICONNAME)},
                  {TaskManager::IdentityRole, RoleCombineModel::roleNames().key(MODEL_IDENTIFY)},
                  {TaskManager::MenusRole, RoleCombineModel::roleNames().key(MODEL_MENUS)},
                  {TaskManager::NameRole, RoleCombineModel::roleNames().key(MODEL_NAME)},
                  {TaskManager::WinIdRole, RoleCombineModel::roleNames().key(MODEL_WINID)},
                  {TaskManager::WinTitleRole, RoleCombineModel::roleNames().key(MODEL_TITLE)}};
}

QHash<int, QByteArray> DockCombineModel::roleNames() const
{
    return {{TaskManager::ActiveRole, MODEL_ACTIVE},
            {TaskManager::AttentionRole, MODEL_ATTENTION},
            {TaskManager::DesktopIdRole, MODEL_DESKTOPID},
            {TaskManager::IconNameRole, MODEL_ICONNAME},
            {TaskManager::IdentityRole, MODEL_IDENTIFY},
            {TaskManager::MenusRole, MODEL_MENUS},
            {TaskManager::NameRole, MODEL_NAME},
            {TaskManager::WinIdRole, MODEL_WINID},
            {TaskManager::WinTitleRole, MODEL_TITLE}};
}

QVariant DockCombineModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
    case TaskManager::DesktopIdRole: {
        auto res = RoleCombineModel::data(index, m_roleMaps.value(TaskManager::DesktopIdRole)).toString();
        if (res.isEmpty()) {
            auto data = RoleCombineModel::data(index, m_roleMaps.value(TaskManager::IdentityRole)).toStringList();
            res = data.value(0, "");
        }
        return res;
    }
    case TaskManager::IconNameRole: {
        auto icon = RoleCombineModel::data(index, m_roleMaps.value(TaskManager::IconNameRole)).toString();
        if (icon.isEmpty()) {
            icon = RoleCombineModel::data(index, m_roleMaps.value(TaskManager::WinIconRole)).toString();
        }
        if (icon.isEmpty()) {
            icon = DEFAULT_APP_ICONNAME;
        }
        return icon;
    }
    default: {
        auto newRole = m_roleMaps.value(role, -1);
        if (newRole == -1) {
            qWarning() << "failed!!!!!!!!" << static_cast<TaskManager::Roles>(role);
            return QVariant();
        }

        return RoleCombineModel::data(index, newRole);
    }
    }

    return QVariant();
}
}
