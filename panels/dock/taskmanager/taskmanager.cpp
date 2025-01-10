// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "abstracttaskmanagerinterface.h"
#include "appitem.h"

#include "abstractwindow.h"
#include "abstractwindowmonitor.h"
#include "desktopfileamparser.h"
#include "desktopfileparserfactory.h"
#include "dockcombinemodel.h"
#include "dockglobalelementmodel.h"
#include "dockitemmodel.h"
#include "dsglobal.h"
#include "globals.h"
#include "itemmodel.h"
#include "pluginfactory.h"
#include "taskmanager.h"
#include "taskmanageradaptor.h"
#include "taskmanagersettings.h"
#include "treelandwindowmonitor.h"

#include <QGuiApplication>
#include <QStringLiteral>

#include <appletbridge.h>

#ifdef BUILD_WITH_X11
#include "x11windowmonitor.h"
#endif

Q_LOGGING_CATEGORY(taskManagerLog, "dde.shell.dock.taskmanager", QtInfoMsg)

#define Settings TaskManagerSettings::instance()

#define DESKTOPFILEFACTORY DesktopfileParserFactory<    \
                            DesktopFileAMParser,        \
                            DesktopfileAbstractParser   \
                        >

namespace dock {
class BoolFilterModel : public QSortFilterProxyModel, public AbstractTaskManagerInterface
{
    Q_OBJECT
public:
    explicit BoolFilterModel(QAbstractItemModel *sourceModel, int role, QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
        , m_role(role)
    {
        setSourceModel(sourceModel);
    }

    void requestActivate(const QModelIndex &index) const override
    {
        callInterfaceMethod(this, index, &AbstractTaskManagerInterface::requestActivate);
    }

    void requestOpenUrls(const QModelIndex &index, const QList<QUrl> &urls) const override
    {
        callInterfaceMethod(this, index, &AbstractTaskManagerInterface::requestOpenUrls, urls);
    }

    void requestNewInstance(const QModelIndex &index, const QString &action) const override
    {
        callInterfaceMethod(this, index, &AbstractTaskManagerInterface::requestNewInstance, action);
    }

    void requestClose(const QModelIndex &index, bool force = false) const override
    {
        callInterfaceMethod(this, index, &AbstractTaskManagerInterface::requestClose, force);
    }

    void requestUpdateWindowGeometry(const QModelIndex &index, const QRect &geometry, QObject *delegate) const override
    {
        callInterfaceMethod(this, index, &AbstractTaskManagerInterface::requestUpdateWindowGeometry, geometry, delegate);
    }

    void requestPreview(const QModelIndexList &indexes,
                        QObject *relativePositionItem,
                        int32_t previewXoffset,
                        int32_t previewYoffset,
                        uint32_t direction) const override
    {
        return callInterfaceMethod(this,
                                   indexes,
                                   &AbstractTaskManagerInterface::requestPreview,
                                   relativePositionItem,
                                   previewXoffset,
                                   previewYoffset,
                                   direction);
    }

    void requestWindowsView(const QModelIndexList &indexes) const override
    {
        return callInterfaceMethod(this, indexes, &AbstractTaskManagerInterface::requestWindowsView);
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        if (sourceRow > sourceModel()->rowCount())
            return false;

        QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
        return !sourceModel()->data(index, m_role).toBool();
    }

private:
    int m_role;
};

TaskManager::TaskManager(QObject* parent)
    : DContainment(parent)
    , m_windowFullscreen(false)
{
    connect(Settings, &TaskManagerSettings::allowedForceQuitChanged, this, &TaskManager::allowedForceQuitChanged);
    connect(Settings, &TaskManagerSettings::windowSplitChanged, this, &TaskManager::windowSplitChanged);
}

bool TaskManager::load()
{
    auto platformName = QGuiApplication::platformName();
    if (QStringLiteral("wayland") == platformName) {
        m_windowMonitor.reset(new TreeLandWindowMonitor());
    }

#ifdef BUILD_WITH_X11
    else if (QStringLiteral("xcb") == platformName) {
        m_windowMonitor.reset(new X11WindowMonitor());
    }
#endif
    return true;
}

bool TaskManager::init()
{
    auto adaptor = new TaskManagerAdaptor(this);
    QDBusConnection::sessionBus().registerService("org.deepin.ds.Dock.TaskManager");
    QDBusConnection::sessionBus().registerObject("/org/deepin/ds/Dock/TaskManager", "org.deepin.ds.Dock.TaskManager", this);

    DApplet::init();

    DS_NAMESPACE::DAppletBridge bridge("org.deepin.ds.dde-apps");
    BoolFilterModel *leftModel = new BoolFilterModel(m_windowMonitor.data(), m_windowMonitor->roleNames().key("shouldSkip"), this);
    if (auto applet = bridge.applet()) {
        auto model = applet->property("appModel").value<QAbstractItemModel *>();
        Q_ASSERT(model);
        m_activeAppModel = new DockCombineModel(leftModel, model, TaskManager::IdentityRole, [](QVariant data, QAbstractItemModel *model) -> QModelIndex {
            auto roleNames = model->roleNames();
            QList<QByteArray> identifiedOrders = {MODEL_DESKTOPID, MODEL_STARTUPWMCLASS, MODEL_NAME, MODEL_ICONNAME};

            auto identifies = data.toStringList();
            for (auto id : identifies) {
                if (id.isEmpty()) {
                    continue;
                }

                for (auto identifiedOrder : identifiedOrders) {
                    auto res = model->match(model->index(0, 0), roleNames.key(identifiedOrder), id, 1, Qt::MatchFixedString | Qt::MatchWrap).value(0);
                    if (res.isValid())
                        return res;
                }
            }

            auto res = model->match(model->index(0, 0), roleNames.key(MODEL_DESKTOPID), identifies.value(0), 1, Qt::MatchEndsWith);
            return res.value(0);
        });

        m_dockGlobalElementModel = new DockGlobalElementModel(model, m_activeAppModel, this);
        m_itemModel = new DockItemModel(m_dockGlobalElementModel, nullptr);
    }

    connect(m_windowMonitor.data(), &AbstractWindowMonitor::windowFullscreenChanged, this, [this] (bool isFullscreen) {
        m_windowFullscreen = isFullscreen;
        emit windowFullscreenChanged(isFullscreen);
    });

    QTimer::singleShot(500, [this]() {
        if (m_windowMonitor)
            m_windowMonitor->start();
    });
    return true;
}

QAbstractItemModel *TaskManager::dataModel()
{
    return m_itemModel;
}

void TaskManager::requestActivate(const QModelIndex &index) const
{
    m_itemModel->requestActivate(index);
}

void TaskManager::requestOpenUrls(const QModelIndex &index, const QList<QUrl> &urls) const
{
    m_itemModel->requestOpenUrls(index, urls);
}

void TaskManager::requestNewInstance(const QModelIndex &index, const QString &action) const
{
    m_itemModel->requestNewInstance(index, action);
}

void TaskManager::requestClose(const QModelIndex &index, bool force) const
{
    m_itemModel->requestClose(index, force);
}

void TaskManager::requestUpdateWindowGeometry(const QModelIndex &index, const QRect &geometry, QObject *delegate) const
{
    m_itemModel->requestUpdateWindowGeometry(index, geometry, delegate);
}

void TaskManager::requestPreview(const QModelIndexList &indexes,
                                 QObject *relativePositionItem,
                                 int32_t previewXoffset,
                                 int32_t previewYoffset,
                                 uint32_t direction) const
{
    m_itemModel->requestPreview(indexes, relativePositionItem, previewXoffset, previewYoffset, direction);
}
void TaskManager::requestWindowsView(const QModelIndexList &indexes) const
{
    m_itemModel->requestWindowsView(indexes);
}

QModelIndex TaskManager::index(int row, int column, const QModelIndex &parent)
{
    return m_itemModel->index(row, column, parent);
}

void TaskManager::clickItem(const QString &itemId, const QString &menuId)
{
}

void TaskManager::showItemPreview(const QString &itemId, QObject* relativePositionItem, int32_t previewXoffset, int32_t previewYoffset, uint32_t direction)
{
    auto item = ItemModel::instance()->getItemById(itemId).get();
    if (!item) return;

    QPointer<AppItem> pItem = reinterpret_cast<AppItem*>(item);
    if (pItem.isNull()) return;

    m_windowMonitor->showItemPreview(pItem, relativePositionItem, previewXoffset, previewYoffset, direction);
}

void TaskManager::hideItemPreview()
{
    m_windowMonitor->hideItemPreview();
}

void TaskManager::setAppItemWindowIconGeometry(const QString& appid, QObject* relativePositionItem, const int& x1, const int& y1, const int& x2, const int& y2)
{
    QPointer<AppItem> item = static_cast<AppItem*>(ItemModel::instance()->getItemById(appid).get());
    if (item.isNull()) return;

    for (auto window : item->getAppendWindows()) {
        window->setWindowIconGeometry(qobject_cast<QWindow*>(relativePositionItem), QRect(QPoint(x1, y1),QPoint(x2, y2)));
    }
}

bool TaskManager::allowForceQuit()
{
    return Settings->isAllowedForceQuit();
}

QString TaskManager::desktopIdToAppId(const QString& desktopId)
{
    return Q_LIKELY(desktopId.endsWith(".desktop")) ? desktopId.chopped(8) : desktopId;
}

bool TaskManager::requestDockByDesktopId(const QString& desktopID)
{
    if (desktopID.startsWith("internal/")) return false;
    return RequestDock(desktopIdToAppId(desktopID));
}

bool TaskManager::requestUndockByDesktopId(const QString& desktopID)
{
    if (desktopID.startsWith("internal/")) return false;
    return RequestUndock(desktopIdToAppId(desktopID));
}

bool TaskManager::RequestDock(QString appID)
{
    auto desktopfileParser = DESKTOPFILEFACTORY::createById(appID, "amAPP");

    auto res = desktopfileParser->isValied();
    if (!res.first) {
        qCWarning(taskManagerLog) << res.second;
        return false;
    }

    QPointer<AppItem> appitem = desktopfileParser->getAppItem();
    if (appitem.isNull()) {
        appitem = new AppItem(appID);
        appitem->setDesktopFileParser(desktopfileParser);
        ItemModel::instance()->addItem(appitem);
    }
    appitem->setDocked(true);
    return true;
}

bool TaskManager::IsDocked(QString appID)
{
    auto desktopfileParser = DESKTOPFILEFACTORY::createById(appID, "amAPP");

    auto res = desktopfileParser->isValied();
    if (!res.first) {
        qCWarning(taskManagerLog) << res.second;
        return false;
    }

    QPointer<AppItem> appitem = desktopfileParser->getAppItem();
    if (appitem.isNull()) {
        return false;
    }
    return appitem->isDocked();
}

bool TaskManager::RequestUndock(QString appID)
{
    auto desktopfileParser = DESKTOPFILEFACTORY::createById(appID, "amAPP");
    auto res = desktopfileParser->isValied();
    if (!res.first) {
        qCWarning(taskManagerLog) << res.second;
        return false;
    }
    QPointer<AppItem> appitem = desktopfileParser->getAppItem();
    if (appitem.isNull()) {
        return false;
    }
    appitem->setDocked(false);
    return true;
}

bool TaskManager::windowSplit()
{
    return Settings->isWindowSplit();
}

bool TaskManager::windowFullscreen()
{
    return m_windowFullscreen;
}

D_APPLET_CLASS(TaskManager)
}

#include "taskmanager.moc"
