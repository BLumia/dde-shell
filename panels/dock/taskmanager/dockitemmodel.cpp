// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dockitemmodel.h"
#include "abstracttaskmanagerinterface.h"
#include "dockgroupmodel.h"
#include "globals.h"
#include "taskmanager.h"
#include "taskmanagersettings.h"

namespace dock
{
DockItemModel::DockItemModel(QAbstractItemModel *globalModel, QObject *parent)
    : QAbstractProxyModel(parent)
    , m_globalModel(globalModel)
    , m_split(!TaskManagerSettings::instance()->isWindowSplit())
    , m_isUpdating(false)
{
    auto updateSourceModel = [this]() {
        if (TaskManagerSettings::instance()->isWindowSplit() == m_split)
            return;

        m_split = TaskManagerSettings::instance()->isWindowSplit();
        if (m_split) {
            setSourceModel(m_globalModel);
            m_groupModel.reset(nullptr);
        } else {
            m_groupModel.reset(new DockGroupModel(m_globalModel, TaskManager::DesktopIdRole));
            setSourceModel(m_groupModel.get());
        }
    };

    connect(TaskManagerSettings::instance(), &TaskManagerSettings::windowSplitChanged, this, updateSourceModel);
    QMetaObject::invokeMethod(this, updateSourceModel, Qt::QueuedConnection);
}

void DockItemModel::requestActivate(const QModelIndex &index) const
{
    callInterfaceMethod(this, index, &AbstractTaskManagerInterface::requestActivate);
}

void DockItemModel::requestOpenUrls(const QModelIndex &index, const QList<QUrl> &urls) const
{
    callInterfaceMethod(this, index, &AbstractTaskManagerInterface::requestOpenUrls, urls);
}

void DockItemModel::requestNewInstance(const QModelIndex &index, const QString &action) const
{
    callInterfaceMethod(this, index, &AbstractTaskManagerInterface::requestNewInstance, action);
}

void DockItemModel::requestClose(const QModelIndex &index, bool force) const
{
    callInterfaceMethod(this, index, &AbstractTaskManagerInterface::requestClose, force);
}

void DockItemModel::requestUpdateWindowGeometry(const QModelIndex &index, const QRect &geometry, QObject *delegate) const
{
    callInterfaceMethod(this, index, &AbstractTaskManagerInterface::requestUpdateWindowGeometry, geometry, delegate);
}

void DockItemModel::requestPreview(const QModelIndexList &indexes,
                                   QObject *relativePositionItem,
                                   int32_t previewXoffset,
                                   int32_t previewYoffset,
                                   uint32_t direction) const
{
    callInterfaceMethod(this, indexes, &AbstractTaskManagerInterface::requestPreview, relativePositionItem, previewXoffset, previewYoffset, direction);
}

void DockItemModel::requestWindowsView(const QModelIndexList &indexes) const
{
    callInterfaceMethod(this, indexes, &AbstractTaskManagerInterface::requestWindowsView);
}

void DockItemModel::setSourceModel(QAbstractItemModel *model)
{
    if (sourceModel() == model)
        return;

    m_isUpdating = true;
    if (sourceModel()) {
        sourceModel()->disconnect(this);
    }

    auto currentCount = this->rowCount();
    auto newCount = model->rowCount();
    QAbstractProxyModel::setSourceModel(model);

    if (newCount > currentCount) {
        beginInsertRows(QModelIndex(), currentCount, newCount - 1);
        endInsertRows();
    } else if (newCount < currentCount) {
        beginRemoveRows(QModelIndex(), newCount, currentCount);
        endRemoveRows();
    }

    connect(sourceModel(), &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent, int first, int last) {
        if (parent.isValid() || m_isUpdating)
            return;
        beginInsertRows(QModelIndex(), first, last);
        endInsertRows();
    });
    connect(sourceModel(), &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex &parent, int first, int last) {
        if (parent.isValid() || m_isUpdating)
            return;
        beginRemoveRows(QModelIndex(), first, last);
        endRemoveRows();
    });
    connect(sourceModel(), &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
        if (m_isUpdating)
            return;
        auto first = topLeft.row();
        auto last = bottomRight.row();
        Q_EMIT dataChanged(index(first, 0), index(last, 0), roles);
    });

    connect(sourceModel(), &QAbstractItemModel::destroyed, this, [this]() {
        if (m_isUpdating)
            return;

        beginResetModel();
        endResetModel();
    });

    auto bottomRight = this->index(std::min(currentCount, newCount), 0);
    Q_EMIT dataChanged(index(0, 0), bottomRight);
    m_isUpdating = false;
}

QModelIndex DockItemModel::getItemIndexById(const QString &id)
{
    for (int i = 0; i < rowCount(); ++i) {
        if (id == data(index(i, 0), TaskManager::DesktopIdRole)) {
            return index(i, 0);
        }
    }
    return QModelIndex();
}

void DockItemModel::dumpItemInfo(const QModelIndex &index)
{
    qDebug() << "Index in DockItemModel:" << index
             << "DesktopIdRole:" << data(index, TaskManager::DesktopIdRole)
             << "DockedRole:" << data(index, TaskManager::DockedRole);
}

QHash<int, QByteArray> DockItemModel::roleNames() const
{
    return m_globalModel->roleNames();
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

int DockItemModel::rowCount(const QModelIndex &parent) const
{
    auto sourceModel = this->sourceModel();
    return sourceModel == nullptr ? 0 : sourceModel->rowCount();
}

QVariant DockItemModel::data(const QModelIndex &index, int role) const
{
    auto sourceModel = this->sourceModel();
    auto data = sourceModel->data(sourceModel->index(index.row(), index.column()), role);
    if (role == TaskManager::IconNameRole) {
        if (data.toString().isEmpty()) {
            return DEFAULT_APP_ICONNAME;
        }
    }

    return data;
}

QModelIndex DockItemModel::mapToSource(const QModelIndex &proxyIndex) const
{
    return sourceModel()->index(proxyIndex.row(), proxyIndex.column());
}

QModelIndex DockItemModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    return index(sourceIndex.row(), sourceIndex.column());
}
}
