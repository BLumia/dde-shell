// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "abstractwindow.h"
#include "containment.h"
#include "dockcombinemodel.h"
#include "dockgroupmodel.h"
#include "dockitemmodel.h"

#include <QPointer>

namespace dock {
class AppItem;
class AbstractWindowMonitor;
class TaskManager : public DS_NAMESPACE::DContainment
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *dataModel READ dataModel NOTIFY dataModelChanged)

    Q_PROPERTY(bool windowSplit READ windowSplit NOTIFY windowSplitChanged)
    Q_PROPERTY(bool windowFullscreen READ windowFullscreen NOTIFY windowFullscreenChanged)
    Q_PROPERTY(bool allowForceQuit READ allowForceQuit NOTIFY allowedForceQuitChanged)

public:
    enum Roles {
        // abstract window
        WinIdRole = Qt::UserRole + 1,
        PidRole,
        IdentityRole,
        WinIconRole,
        WinTitleRole,
        ActiveRole,
        ShouldSkipRole,
        AttentionRole,

        // item
        ItemIdRole,
        MenusRole,
        WindowsRole,

        // from dde-apps
        DesktopIdRole = 0x1000,
        NameRole,
        IconNameRole,
        StartUpWMClassRole,
        NoDisplayRole,
        ActionsRole,
        DDECategoryRole,
        InstalledTimeRole,
        LastLaunchedTimeRole,
        LaunchedTimesRole,
        DockedRole,
        OnDesktopRole,
        AutoStartRole,
        AppTypeRole,
    };
    Q_ENUM(Roles)

    explicit TaskManager(QObject* parent = nullptr);

    QAbstractItemModel *dataModel();

    virtual bool init() override;
    virtual bool load() override;

    bool windowSplit();
    bool windowFullscreen();
    bool allowForceQuit();

    Q_INVOKABLE QString desktopIdToAppId(const QString& desktopId);
    Q_INVOKABLE bool requestDockByDesktopId(const QString& desktopID);
    Q_INVOKABLE bool requestUndockByDesktopId(const QString& desktopID);
    Q_INVOKABLE bool RequestDock(QString appID);
    Q_INVOKABLE bool IsDocked(QString appID);
    Q_INVOKABLE bool RequestUndock(QString appID);

    Q_INVOKABLE void clickItem(const QString& itemid, const QString& menuId);
    Q_INVOKABLE void showItemPreview(const QString& itemId, QObject* relativePositionItem, int32_t previewXoffset, int32_t previewYoffset, uint32_t direction);
    Q_INVOKABLE void hideItemPreview();

    Q_INVOKABLE void setAppItemWindowIconGeometry(const QString& appid, QObject* relativePositionItem, const int& x1, const int& y1, const int& x2, const int& y2);

Q_SIGNALS:
    void dataModelChanged();
    void windowSplitChanged();
    void windowFullscreenChanged(bool);
    void allowedForceQuitChanged();

private Q_SLOTS:
    void handleWindowAdded(QPointer<AbstractWindow> window);

private:
    void loadDockedAppItems();

private:
    QScopedPointer<AbstractWindowMonitor> m_windowMonitor;
    bool m_windowFullscreen;
    DockCombineModel *m_activeAppModel = nullptr;
    DockItemModel *m_dockItemModel = nullptr;
    DockGroupModel *m_groupModel = nullptr;
};

}

