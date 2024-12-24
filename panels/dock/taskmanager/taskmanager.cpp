// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appitem.h"

#include "abstractwindow.h"
#include "abstractwindowmonitor.h"
#include "desktopfileamparser.h"
#include "desktopfileparserfactory.h"
#include "dockcombinemodel.h"
#include "dockgroupmodel.h"
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
class BoolFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit BoolFilterModel(QAbstractItemModel *sourceModel, int role, QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
        , m_role(role)
    {
        setSourceModel(sourceModel);
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
    qRegisterMetaType<ObjectInterfaceMap>();
    qDBusRegisterMetaType<ObjectInterfaceMap>();
    qRegisterMetaType<ObjectMap>();
    qDBusRegisterMetaType<ObjectMap>();
    qDBusRegisterMetaType<QStringMap>();
    qRegisterMetaType<QStringMap>();
    qRegisterMetaType<PropMap>();
    qDBusRegisterMetaType<PropMap>();
    qDBusRegisterMetaType<QDBusObjectPath>();

    connect(Settings, &TaskManagerSettings::allowedForceQuitChanged, this, &TaskManager::allowedForceQuitChanged);
    connect(Settings, &TaskManagerSettings::windowSplitChanged, this, &TaskManager::windowSplitChanged);
}

bool TaskManager::load()
{
    loadDockedAppItems();
    auto platformName = QGuiApplication::platformName();
    if (QStringLiteral("wayland") == platformName) {
        m_windowMonitor.reset(new TreeLandWindowMonitor());
    }

#ifdef BUILD_WITH_X11
    else if (QStringLiteral("xcb") == platformName) {
        m_windowMonitor.reset(new X11WindowMonitor());
    }
#endif

    connect(m_windowMonitor.get(), &AbstractWindowMonitor::windowAdded, this, &TaskManager::handleWindowAdded);
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

            auto indentifies = data.toStringList();
            for (auto id : indentifies) {
                if (id.isEmpty()) {
                    continue;
                }

                for (auto identifiedOrder : identifiedOrders) {
                    auto res = model->match(model->index(0, 0), roleNames.key(identifiedOrder), id, 1, Qt::MatchFixedString | Qt::MatchWrap).value(0);
                    if (res.isValid())
                        return res;
                }
            }

            auto res = model->match(model->index(0, 0), roleNames.key(MODEL_DESKTOPID), indentifies.value(0), 1, Qt::MatchEndsWith);
            return res.value(0);
        });

        m_dockItemModel = new DockItemModel(model, m_activeAppModel, this);
        m_groupModel = new DockGroupModel(m_dockItemModel, TaskManager::ItemIdRole, this);
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
    return m_groupModel;
}

void TaskManager::handleWindowAdded(QPointer<AbstractWindow> window)
{
    if (!window || window->shouldSkip() || window->getAppItem() != nullptr) return;

    // TODO: remove below code and use use model replaced.
    QModelIndex res;
    if (m_activeAppModel) {
        res = m_activeAppModel->match(m_activeAppModel->index(0, 0), TaskManager::WinIdRole, window->id(), 1, Qt::MatchExactly).value(0);
    }

    QSharedPointer<DesktopfileAbstractParser> desktopfile = nullptr;
    if (res.isValid()) {
        desktopfile = DESKTOPFILEFACTORY::createById(res.data(TaskManager::DesktopIdRole).toString(), "amAPP");
    }

    if (desktopfile.isNull() || !desktopfile->isValied().first) {
        desktopfile = DESKTOPFILEFACTORY::createByWindow(window);
    }

    auto appitem = desktopfile->getAppItem();

    if (appitem.isNull() || (appitem->hasWindow() && windowSplit())) {
        auto id = windowSplit() ? QString("%1@%2").arg(desktopfile->id()).arg(window->id()) : desktopfile->id();
        appitem = new AppItem(id);
    }

    appitem->appendWindow(window);
    appitem->setDesktopFileParser(desktopfile);

    ItemModel::instance()->addItem(appitem);
}

void TaskManager::clickItem(const QString& itemId, const QString& menuId)
{
    auto item = ItemModel::instance()->getItemById(itemId);
    if (!item) {
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start("dde-am", {"--by-user", itemId, ""});
        process.waitForFinished();
        return;
    }

    if (menuId == DOCK_ACTION_ALLWINDOW) {
        QList<uint32_t> windowIds;
        auto windows = item->data().toStringList();
        std::transform(windows.begin(), windows.end(), std::back_inserter(windowIds), [](const QString &windowId) {
            return windowId.toUInt();
        });

        m_windowMonitor->presentWindows(windowIds);
        return;
    }

    item->handleClick(menuId);
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

void TaskManager::loadDockedAppItems()
{
    // TODO: add support for group and dir type
    for (const auto& appValueRef : TaskManagerSettings::instance()->dockedDesktopFiles()) {
        auto app = appValueRef.toObject();
        auto appid = app.value("id").toString();
        auto type = app.value("type").toString();
        auto desktopfile = DESKTOPFILEFACTORY::createById(appid, type);
        auto valid = desktopfile->isValied();

        if (!valid.first) {
            qInfo(taskManagerLog()) << "failed to load " << appid << " beacause " << valid.second;
            continue;
        }

        auto appitem = desktopfile->getAppItem();
        if (appitem.isNull()) {
            appitem = new AppItem(appid);
        }

        appitem->setDesktopFileParser(desktopfile);
        ItemModel::instance()->addItem(appitem);
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
