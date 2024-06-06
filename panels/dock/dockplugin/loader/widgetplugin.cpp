// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "constants.h"
#include "plugin.h"
#include "widgetplugin.h"
#include "pluginsiteminterface.h"
#include "pluginsiteminterface_v2.h"
/*#include "pluginsiteminterface_v2.h"*/

#include <QMenu>
#include <QPainter>
#include <QProcess>
#include <QVBoxLayout>
#include <QMouseEvent>

#include <cstddef>
#include <DGuiApplicationHelper>
#include <qglobal.h>

#include "pluginitem.h"

DGUI_USE_NAMESPACE

namespace dock {
WidgetPlugin::WidgetPlugin(PluginsItemInterface* pluginItem)
    : QObject()
    , m_pluginItem(pluginItem)
{
    QMetaObject::invokeMethod(this, [this](){
        m_pluginItem->init(this);
    });

    // TODO 需要给插件一个回调函数，以便插件通过回调函数告诉dock信息;并在回调函数中直接给出Json返回值
    // using MessageCallbackFunc = QString (*)(PluginsItemInterfaceV2 *, const QString&);
}

WidgetPlugin::~WidgetPlugin()
{
}

void WidgetPlugin::itemAdded(PluginsItemInterface * const itemInter, const QString &itemKey)
{
    auto pluginItemV2 = dynamic_cast<PluginsItemInterfaceV2*>(m_pluginItem);
    Q_ASSERT(pluginItemV2);

    QWidget *widget = getQuickPluginTrayWidget(itemKey);
    if (!widget) {
        widget = new PluginItem(m_pluginItem, itemKey);
    } else {
        widget->setFixedSize(QSize(16, 16));
    }

    Plugin::EmbedPlugin* plugin;
    widget->setAttribute(Qt::WA_TranslucentBackground);
    widget->winId();

    plugin = Plugin::EmbedPlugin::get(widget->windowHandle());
    initConnections(plugin);
    plugin->setPluginFlags(pluginItemV2->flags());
    plugin->setItemKey(itemKey);
    plugin->setPluginType(1);
    widget->windowHandle()->hide();
    widget->show();

    // 模拟发送message request, 此调用应该在回调函数中
    Q_EMIT plugin->requestMessage("plugin test message");
}

void WidgetPlugin::itemUpdate(PluginsItemInterface * const itemInter, const QString &itemKey)
{
    if(m_widget) m_widget->update();

    auto widget = m_pluginItem->itemWidget(itemKey);
    if (widget) widget->update();

    auto quickPanel = m_pluginItem->itemWidget(QUICK_ITEM_KEY);
    if(quickPanel) quickPanel->update();

    auto popupWidget = m_pluginItem->itemPopupApplet(itemKey);
    if (popupWidget) popupWidget->update();

    auto tipsWidget = m_pluginItem->itemTipsWidget(itemKey);
    if (tipsWidget) tipsWidget->update();

}
void WidgetPlugin::itemRemoved(PluginsItemInterface * const itemInter, const QString &itemKey)
{
    auto widget = m_pluginItem->itemWidget(itemKey);
    if(widget) widget->hide();

    auto quickPanel = m_pluginItem->itemWidget(QUICK_ITEM_KEY);
    if(quickPanel) quickPanel->hide();

    auto popupWidget = m_pluginItem->itemPopupApplet(itemKey);
    if(popupWidget) popupWidget->hide();

    auto tipsWidget = m_pluginItem->itemTipsWidget(itemKey);
    if(tipsWidget) tipsWidget->hide();

}

void WidgetPlugin::requestWindowAutoHide(PluginsItemInterface * const itemInter, const QString &itemKey, const bool autoHide)
{
}

void WidgetPlugin::requestRefreshWindowVisible(PluginsItemInterface * const itemInter, const QString &itemKey)
{
}

void WidgetPlugin::requestSetAppletVisible(PluginsItemInterface * const itemInter, const QString &itemKey, const bool visible)
{
    QWidget* appletWidget = itemInter->itemPopupApplet(itemKey);
    appletWidget->setFixedSize(400, 400);
    appletWidget->setParent(nullptr);
    appletWidget->show();
}

void WidgetPlugin::saveValue(PluginsItemInterface * const itemInter, const QString &key, const QVariant &value)
{
}

const QVariant WidgetPlugin::getValue(PluginsItemInterface *const itemInter, const QString &key, const QVariant& fallback)
{
    return fallback;
}

void WidgetPlugin::removeValue(PluginsItemInterface *const itemInter, const QStringList &keyList)
{
}

void WidgetPlugin::updateDockInfo(PluginsItemInterface *const, const DockPart &part)
{
    switch (part) {
        case DockPart::QuickShow: {
            if (m_widget) {
                m_widget->update();

                auto plugin = getPlugin(m_widget.get());
            }
            break;
        }

        // TODO: implement below cases
        case DockPart::QuickPanel: {
            if (m_widget) {
                m_widget->update();
            }
            break;
        }

        case DockPart::SystemPanel: {
            break;
        }

        case DockPart::DCCSetting: {
            break;
        }
    }
}


const QString WidgetPlugin::pluginName() const
{
    return m_pluginItem->pluginName();
}

const QString WidgetPlugin::itemCommand(const QString &itemKey)
{
    return m_pluginItem->itemCommand(itemKey);
}

const QString WidgetPlugin::itemContextMenu(const QString &itemKey)
{
    return m_pluginItem->itemContextMenu(itemKey);
}

void WidgetPlugin::onDockPositionChanged(uint32_t position)
{
    qApp->setProperty(PROP_POSITION, position);
    m_pluginItem->positionChanged(static_cast<Dock::Position>(position));
}

void WidgetPlugin::onDockDisplayModeChanged(uint32_t displayMode)
{
    qApp->setProperty(PROP_DISPLAY_MODE, displayMode);
    m_pluginItem->displayModeChanged(static_cast<Dock::DisplayMode>(displayMode));
}

void WidgetPlugin::onDockEventMessageArrived(const QString &message)
{
    // TODO
}

QWidget* WidgetPlugin::getQuickPluginTrayWidget(const QString &itemKey)
{
    PluginsItemInterfaceV2 *interface_v2 = dynamic_cast<PluginsItemInterfaceV2 *>(m_pluginItem);
    if (!interface_v2) {
        return m_widget.get(); 
    }

    auto trayIcon = interface_v2->icon(Dock::IconType::IconType_None, (Dock::ThemeType)DGuiApplicationHelper::instance()->themeType());
    if (trayIcon.isNull()) {
        return m_widget.get();
    }

    if (m_widget.isNull()) {
        m_widget.reset(new TrayIconWidget(m_pluginItem, itemKey));

        connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [this, itemKey](){
            auto widget = getQuickPluginTrayWidget(itemKey);
            if (widget) widget->update();
        }, Qt::UniqueConnection);
    }

    return m_widget.get();
}

Plugin::EmbedPlugin* WidgetPlugin::getPlugin(QWidget* widget)
{
    widget->setParent(nullptr);
    widget->winId();
    return Plugin::EmbedPlugin::get(widget->windowHandle());
}

void WidgetPlugin::initConnections(Plugin::EmbedPlugin *plugin)
{
    if (!plugin)
        return;

    connect(plugin, &Plugin::EmbedPlugin::dockColorThemeChanged, this, [](uint32_t type){
        DGuiApplicationHelper::instance()->setPaletteType(static_cast<DGuiApplicationHelper::ColorType>(type));
    }, Qt::UniqueConnection);

    connect(plugin, &Plugin::EmbedPlugin::dockPositionChanged, this, &WidgetPlugin::onDockPositionChanged, Qt::UniqueConnection);
    connect(plugin, &Plugin::EmbedPlugin::eventMessage, this, &WidgetPlugin::onDockEventMessageArrived, Qt::UniqueConnection);
}

TrayIconWidget::TrayIconWidget(PluginsItemInterface* pluginItem, QString itemKey, QWidget* parent)
    : QWidget(parent)
    , m_pluginItem(pluginItem)
    , m_itemKey(itemKey)
    , m_menu(new QMenu)
{
    auto scale = QCoreApplication::testAttribute(Qt::AA_UseHighDpiPixmaps) ? 1 : qApp->devicePixelRatio();
    setFixedSize(PLUGIN_ICON_MIN_SIZE * scale, PLUGIN_ICON_MIN_SIZE * scale);
    connect(m_menu, &QMenu::triggered, this, [this](QAction *action){
        m_pluginItem->invokedMenuItem(m_itemKey, action->data().toString(), action->isCheckable() ? action->isChecked() : true);
    });
}

TrayIconWidget::~TrayIconWidget()
{}

void TrayIconWidget::paintEvent(QPaintEvent *event)
{
    auto func = [this]() -> QPixmap {
        PluginsItemInterfaceV2 *interface_v2 = dynamic_cast<PluginsItemInterfaceV2 *>(m_pluginItem);
        if (!interface_v2) {
            return QPixmap(); 
        }
        auto trayIcon = interface_v2->icon(Dock::IconType::IconType_None, (Dock::ThemeType)DGuiApplicationHelper::instance()->themeType());
        if (trayIcon.availableSizes().size() > 0) {
            QSize size = trayIcon.availableSizes().first();
            return trayIcon.pixmap(size);
        }

        int pixmapWidth = static_cast<int>(PLUGIN_ICON_MIN_SIZE * (QCoreApplication::testAttribute(Qt::AA_UseHighDpiPixmaps) ? 1 : qApp->devicePixelRatio()));
        int pixmapHeight = static_cast<int>(PLUGIN_ICON_MIN_SIZE * (QCoreApplication::testAttribute(Qt::AA_UseHighDpiPixmaps) ? 1 : qApp->devicePixelRatio()));
        return trayIcon.pixmap(pixmapWidth, pixmapHeight);
    };

    auto pixmap = func();
    pixmap.setDevicePixelRatio(qApp->devicePixelRatio());

    QPainter painter(this);
    auto scale = QCoreApplication::testAttribute(Qt::AA_UseHighDpiPixmaps) ? 1 : qApp->devicePixelRatio();
    QSize size = QCoreApplication::testAttribute(Qt::AA_UseHighDpiPixmaps) ? pixmap.size() / qApp->devicePixelRatio() : pixmap.size();
    QRect pixmapRect = QRect(QPoint(0, 0), QSize(PLUGIN_ICON_MIN_SIZE * scale, PLUGIN_ICON_MIN_SIZE * scale));
    painter.drawPixmap(pixmapRect, pixmap);
}

void TrayIconWidget::enterEvent(QEvent *event)
{
    auto popup = m_pluginItem->itemPopupApplet(m_itemKey);
    if (popup)
        popup->hide();

    QMetaObject::invokeMethod(this, [this](){
        auto toolTip = m_pluginItem->itemTipsWidget(m_itemKey);
        if (!toolTip) {
            toolTip = m_pluginItem->itemTipsWidget(QUICK_ITEM_KEY);
        }

        if (!toolTip) {
            qDebug() << "no tooltip";
            return;
        }

        toolTip->setAttribute(Qt::WA_TranslucentBackground);
        toolTip->winId();

        auto geometry = windowHandle()->geometry();
        auto pluginPopup = Plugin::PluginPopup::get(toolTip->windowHandle());
        pluginPopup->setPopupType(Plugin::PluginPopup::PopupTypeTooltip);
        pluginPopup->setX(geometry.x() + geometry.width() / 2), pluginPopup->setY(geometry.y() + geometry.height() / 2);
        toolTip->show();
    });
}

void TrayIconWidget::leaveEvent(QEvent *event)
{
    auto tooltip = m_pluginItem->itemTipsWidget(m_itemKey);
    if (tooltip && tooltip->windowHandle())
        tooltip->windowHandle()->hide();
}

void TrayIconWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        if (m_menu->actions().isEmpty()) {
            const QString menuJson = m_pluginItem->itemContextMenu(m_itemKey);
            if (menuJson.isEmpty())
                return;

            QJsonDocument jsonDocument = QJsonDocument::fromJson(menuJson.toLocal8Bit().data());
            if (jsonDocument.isNull())
                return;

            QJsonObject jsonMenu = jsonDocument.object();

            QJsonArray jsonMenuItems = jsonMenu.value("items").toArray();
            for (auto item : jsonMenuItems) {
                QJsonObject itemObj = item.toObject();
                QAction *action = new QAction(itemObj.value("itemText").toString());
                action->setCheckable(itemObj.value("isCheckable").toBool());
                action->setChecked(itemObj.value("checked").toBool());
                action->setData(itemObj.value("itemId").toString());
                action->setEnabled(itemObj.value("isActive").toBool());
                m_menu->addAction(action);
            }
        }

        m_menu->setAttribute(Qt::WA_TranslucentBackground, true);
        // FIXME: qt5integration drawMenuItemBackground will draw a background event is's transparent
        auto pa = this->palette();
        pa.setColor(QPalette::ColorRole::Window, Qt::transparent);
        m_menu->setPalette(pa);
        m_menu->winId();

        auto geometry = windowHandle()->geometry();
        auto pluginPopup = Plugin::PluginPopup::get(m_menu->windowHandle());
        pluginPopup->setPopupType(Plugin::PluginPopup::PopupTypeMenu);
        pluginPopup->setX(geometry.x() + geometry.width() / 2), pluginPopup->setY(geometry.y() + geometry.height() / 2);
        m_menu->setFixedSize(m_menu->sizeHint());
        m_menu->exec();
    } else if (event->button() == Qt::LeftButton) {
        auto popup = m_pluginItem->itemPopupApplet(m_itemKey);

        if (!popup) {
            popup = m_pluginItem->itemPopupApplet(QUICK_ITEM_KEY);
        }

        if (!popup) {
            auto cmd = m_pluginItem->itemCommand(m_itemKey).split(" ");
            QProcess::startDetached(cmd.first(), cmd.mid(1));
            return;
        }

        if (popup->isVisible()) {
            popup->hide();
            return;
        }

        popup->setAttribute(Qt::WA_TranslucentBackground);
        popup->winId();

        auto geometry = windowHandle()->geometry();
        auto pluginPopup = Plugin::PluginPopup::get(popup->windowHandle());
        pluginPopup->setPopupType(Plugin::PluginPopup::PopupTypePanel);
        pluginPopup->setX(geometry.x() + geometry.width() / 2), pluginPopup->setY(geometry.y() + geometry.height() / 2);
        popup->show();
    }
}

}
