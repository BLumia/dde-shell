// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dockitemmodel.h"
#include "globals.h"
#include "taskmanager.h"
#include "taskmanagersettings.h"

#include <QAbstractListModel>
#include <tuple>

namespace dock
{
DockItemModel::DockItemModel(QAbstractItemModel *appsModel, QAbstractItemModel *activeAppModel, QObject *parent)
    : QAbstractListModel(parent)
    , m_appsModel(appsModel)
    , m_activeAppModel(activeAppModel)
{
    connect(TaskManagerSettings::instance(), &TaskManagerSettings::dockedElementsChanged, this, &DockItemModel::loadDockedElements);
    connect(m_appsModel, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex &parent, int first, int last) {
        for (int i = first; i <= last; ++i) {
            auto it = std::find_if(m_data.begin(), m_data.end(), [this, &i](auto data) {
                return std::get<1>(data) == m_appsModel && std::get<2>(data) == i;
            });
            if (it != m_data.end()) {
                auto pos = it - m_data.end();
                beginRemoveRows(QModelIndex(), pos, pos);
                m_data.remove(pos);
                endRemoveRows();
            }
        }
        std::for_each(m_data.begin(), m_data.end(), [this, first, last](auto data) {
            if (std::get<1>(data) == m_appsModel && std::get<2>(data) >= first) {
                data = std::make_tuple(std::get<0>(data), std::get<1>(data), std::get<2>(data) - last + first);
            }
        });
    });

    connect(m_activeAppModel, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent, int first, int last) {
        for (int i = first; i <= last; ++i) {
            auto index = m_activeAppModel->index(i, 0);
            auto desktopId = index.data(TaskManager::DesktopIdRole).toString();

            auto it = std::find_if(m_data.begin(), m_data.end(), [this, &desktopId](auto data) {
                return m_appsModel == std::get<1>(data) && desktopId == std::get<0>(data);
            });

            if (it != m_data.end()) {
                *it = std::make_tuple(desktopId, m_activeAppModel, i);
                auto pIndex = this->index(it - m_data.begin(), 0);
                Q_EMIT dataChanged(pIndex, pIndex, {TaskManager::ActiveRole, TaskManager::AttentionRole, TaskManager::WindowsRole});

            } else {
                beginInsertRows(QModelIndex(), m_data.size(), m_data.size());
                m_data.append(std::make_tuple(desktopId, m_activeAppModel, first));
                endInsertRows();
            }
        }

        std::for_each(m_data.begin(), m_data.end(), [this, first, last](auto data) {
            if (std::get<1>(data) == m_activeAppModel && std::get<2>(data) > first) {
                data = std::make_tuple(std::get<0>(data), std::get<1>(data), std::get<2>(data) + last - first + 1);
            }
        });
    });

    connect(m_activeAppModel, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex &parent, int first, int last) {
        for (int i = first; i <= last; ++i) {
            auto it = std::find_if(m_data.begin(), m_data.end(), [this, i](auto data) {
                return std::get<1>(data) == m_activeAppModel && std::get<2>(data) == i;
            });

            if (it == m_data.end())
                continue;

            auto pos = it - m_data.begin();
            auto id = std::get<0>(*it);

            auto oit = std::find_if(m_data.constBegin(), m_data.constEnd(), [this, &id, i](auto data) {
                return std::get<0>(data) == id && std::get<1>(data) == m_activeAppModel && std::get<2>(data) != i;
            });

            if (oit == m_data.constEnd() && m_dockedElements.contains(std::make_tuple("desktop", id))) {
                auto pIndex = this->index(pos, 0);
                auto res = m_appsModel->match(m_appsModel->index(0, 0), TaskManager::DesktopIdRole, id, 1, Qt::MatchExactly);
                if (res.isEmpty()) {
                    beginRemoveRows(QModelIndex(), pos, pos);
                    m_data.remove(pos);
                    endRemoveRows();
                } else {
                    auto row = res.first().row();
                    *it = std::make_tuple(id, m_appsModel, row);
                    Q_EMIT dataChanged(pIndex, pIndex, {TaskManager::ActiveRole, TaskManager::AttentionRole, TaskManager::WindowsRole});
                }
            } else {
                beginRemoveRows(QModelIndex(), pos, pos);
                m_data.remove(pos);
                endRemoveRows();
            }
        }
        std::for_each(m_data.begin(), m_data.end(), [this, first, last](auto data) {
            if (std::get<1>(data) == m_activeAppModel && std::get<2>(data) >= first) {
                data = std::make_tuple(std::get<0>(data), std::get<1>(data), std::get<2>(data) - last + first);
            }
        });
    });

    connect(m_activeAppModel,
            &QAbstractItemModel::dataChanged,
            this,
            [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
                int first = topLeft.row(), last = bottomRight.row();
                for (int i = first; i <= last; i++) {
                    auto it = std::find_if(m_data.constBegin(), m_data.constEnd(), [this, i](auto data) {
                        return std::get<1>(data) == m_activeAppModel && std::get<2>(data) == i;
                    });

                    if (it == m_data.end())
                        return;
                    auto pos = it - m_data.constBegin();

                    auto oldRoles = roles;
                    auto desktopId = roles.indexOf(TaskManager::DesktopIdRole);
                    auto identifyId = roles.indexOf(TaskManager::IdentityRole);
                    if (desktopId != -1 || identifyId != -1) {
                        oldRoles.append(TaskManager::ItemIdRole);
                    }
                    Q_EMIT dataChanged(index(pos, 0), index(pos, 0), oldRoles);
                }
            });

    QMetaObject::invokeMethod(this, &DockItemModel::loadDockedElements, Qt::QueuedConnection);
}

QHash<int, QByteArray> DockItemModel::roleNames() const
{
    return {
        {TaskManager::ItemIdRole, MODEL_ITEMID},
        {TaskManager::NameRole, MODEL_NAME},
        {TaskManager::IconNameRole, MODEL_ICONNAME},
        {TaskManager::ActiveRole, MODEL_ACTIVE},
        {TaskManager::AttentionRole, MODEL_ATTENTION},
        {TaskManager::MenusRole, MODEL_MENUS},
        {TaskManager::DockedRole, MODEL_DOCKED},
        {TaskManager::WindowsRole, MODEL_WINDOWS},
        {TaskManager::WinIdRole, MODEL_WINID},
    };
}

QModelIndex DockItemModel::index(int row, int column, const QModelIndex &parent) const
{
    return createIndex(row, column);
}

QModelIndex DockItemModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}

int DockItemModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

void DockItemModel::loadDockedElements()
{
    QList<std::tuple<QString, QString>> res;
    for (auto elementInfo : TaskManagerSettings::instance()->dockedElements()) {
        auto pair = elementInfo.split('/');
        if (pair.size() != 2)
            continue;

        auto type = pair[0];
        auto id = pair[1];

        // check desktop is installed
        QAbstractItemModel *model = nullptr;
        int row = 0;
        if (type == "desktop") {
            model = m_appsModel;
            auto res = m_appsModel->match(m_appsModel->index(0, 0), TaskManager::DesktopIdRole, id, 1, Qt::MatchExactly);
            if (res.isEmpty())
                continue;
            row = res.first().row();
        }
        res.append(std::make_tuple(type, id));
        beginInsertRows(QModelIndex(), m_data.size(), m_data.size());
        m_data.append(std::make_tuple(id, model, row));
        endInsertRows();
    }

    m_dockedElements = res;
}

int DockItemModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_data.size();
}

QVariant DockItemModel::data(const QModelIndex &index, int role) const
{
    if (index.row() > m_data.size())
        return {};

    auto data = m_data.value(index.row());
    auto id = std::get<0>(data);
    auto model = std::get<1>(data);
    auto row = std::get<2>(data);

    if (role == TaskManager::ItemIdRole)
        return id;

    if (model) {
        if (role == TaskManager::WindowsRole && model == m_activeAppModel) {
            return QStringList{model->index(row, 0).data(TaskManager::WinIdRole).toString()};
        }

        auto data = model->index(row, 0).data(role);
        return data;
    }

    return QVariant();
}
}
