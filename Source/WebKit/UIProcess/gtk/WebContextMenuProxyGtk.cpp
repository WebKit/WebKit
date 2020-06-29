/*
 * Copyright (C) 2011, 2020 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebContextMenuProxyGtk.h"

#if ENABLE(CONTEXT_MENUS)

#include "APIContextMenuClient.h"
#include "NativeWebMouseEvent.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuItemData.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/GtkUtilities.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <wtf/text/CString.h>

static const char* gContextMenuActionId = "webkit-context-menu-action";
static const char* gContextMenuTitle = "webkit-context-menu-title";
static const char* gContextMenuItemGroup = "webkitContextMenu";

namespace WebKit {
using namespace WebCore;

#if USE(GTK4)
const char* WebContextMenuProxyGtk::widgetDismissedSignal = "closed";

static GtkWidget* createMenuWidget(GtkWidget* webView)
{
    auto* widget = gtk_popover_menu_new_from_model(nullptr);
    gtk_popover_set_has_arrow(GTK_POPOVER(widget), FALSE);
    gtk_popover_set_position(GTK_POPOVER(widget), GTK_POS_BOTTOM);
    gtk_widget_set_halign(widget, GTK_ALIGN_START);
    gtk_widget_set_parent(widget, webView);
    return widget;
}

static inline void popupMenuWidget(GtkWidget* widget, const GdkEvent*, const GdkRectangle& target)
{
    gtk_popover_set_pointing_to(GTK_POPOVER(widget), &target);
    gtk_popover_popup(GTK_POPOVER(widget));
}

static inline void popdownMenuWidget(GtkWidget* widget)
{
    gtk_popover_popdown(GTK_POPOVER(widget));
}

static inline bool menuWidgetHasItems(GtkWidget* widget)
{
    return g_menu_model_get_n_items(gtk_popover_menu_get_menu_model(GTK_POPOVER_MENU(widget)));
}

static inline void bindModelToMenuWidget(GtkWidget* widget, GMenuModel* model)
{
    gtk_popover_menu_set_menu_model(GTK_POPOVER_MENU(widget), model);
}

static inline void destroyMenuWidget(GtkWidget* widget)
{
    gtk_widget_unparent(widget);
}
#else
const char* WebContextMenuProxyGtk::widgetDismissedSignal = "deactivate";

static GtkWidget* createMenuWidget(GtkWidget* webView)
{
    auto* widget = gtk_menu_new();
    gtk_menu_attach_to_widget(GTK_MENU(widget), webView, nullptr);
    return widget;
}

static void popupMenuWidget(GtkWidget* widget, const GdkEvent* triggerEvent, const GdkRectangle& target)
{
    GUniquePtr<GdkEvent> keyEvent;
    auto* window = gtk_widget_get_window(gtk_menu_get_attach_widget(GTK_MENU(widget)));
    if (!triggerEvent) {
        // When the context menu is requested using the keyboard, there is no
        // triggerEvent: use a fake event for the “Menu” key being released.
        keyEvent.reset(gdk_event_new(GDK_KEY_RELEASE));
        gdk_event_set_device(keyEvent.get(), gdk_seat_get_keyboard(gdk_display_get_default_seat(gdk_window_get_display(window))));
        auto* event = reinterpret_cast<GdkEventKey*>(keyEvent.get());
        event->window = window;
        // Take a ref of the GdkWindow when creating the GdkEvent, since the event will unref it on destroy.
        g_object_ref(window);
        event->time = GDK_CURRENT_TIME;
        event->keyval = GDK_KEY_Menu;
        triggerEvent = keyEvent.get();
    };
    gtk_menu_popup_at_rect(GTK_MENU(widget), window, &target, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, triggerEvent);
}

static inline void popdownMenuWidget(GtkWidget* widget)
{
    gtk_menu_popdown(GTK_MENU(widget));
}

static inline bool menuWidgetHasItems(GtkWidget* widget)
{
    unsigned childCount = 0;
    gtk_container_foreach(GTK_CONTAINER(widget), [](GtkWidget*, gpointer data) { (*static_cast<unsigned*>(data))++; }, &childCount);
    return childCount > 0;
}

static inline void bindModelToMenuWidget(GtkWidget* widget, GMenuModel* model)
{
    gtk_menu_shell_bind_model(GTK_MENU_SHELL(widget), model, nullptr, TRUE);
}

static inline void destroyMenuWidget(GtkWidget* widget)
{
    gtk_widget_destroy(widget);
}
#endif // USE(GTK4)

static void contextMenuItemActivatedCallback(GAction* action, GVariant*, WebPageProxy* page)
{
    auto* stateType = g_action_get_state_type(action);
    gboolean isToggle = stateType && g_variant_type_equal(stateType, G_VARIANT_TYPE_BOOLEAN);
    GRefPtr<GVariant> state = isToggle ? adoptGRef(g_action_get_state(action)) : nullptr;
    WebContextMenuItemData item(isToggle ? CheckableActionType : ActionType,
        static_cast<ContextMenuAction>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), gContextMenuActionId))),
        String::fromUTF8(static_cast<const char*>(g_object_get_data(G_OBJECT(action), gContextMenuTitle))), g_action_get_enabled(action),
        state ? g_variant_get_boolean(state.get()) : false);
    if (isToggle)
        g_action_change_state(action, g_variant_new_boolean(!g_variant_get_boolean(state.get())));
    page->contextMenuItemSelected(item);
}

void WebContextMenuProxyGtk::append(GMenu* menu, const WebContextMenuItemGlib& menuItem)
{
    unsigned long signalHandlerId;
    GRefPtr<GMenuItem> gMenuItem;
    GAction* action = menuItem.gAction();
    ASSERT(action);
    g_action_map_add_action(G_ACTION_MAP(m_actionGroup.get()), action);

    switch (menuItem.type()) {
    case ActionType:
    case CheckableActionType: {
        gMenuItem = adoptGRef(g_menu_item_new(menuItem.title().utf8().data(), nullptr));
        GUniquePtr<char> actionName(g_strdup_printf("%s.%s", gContextMenuItemGroup, g_action_get_name(action)));
        g_menu_item_set_action_and_target_value(gMenuItem.get(), actionName.get(), menuItem.gActionTarget());

        if (menuItem.action() < ContextMenuItemBaseApplicationTag) {
            g_object_set_data(G_OBJECT(action), gContextMenuActionId, GINT_TO_POINTER(menuItem.action()));
            g_object_set_data_full(G_OBJECT(action), gContextMenuTitle, g_strdup(menuItem.title().utf8().data()), g_free);
            signalHandlerId = g_signal_connect(action, "activate", G_CALLBACK(contextMenuItemActivatedCallback), m_page);
            m_signalHandlers.set(signalHandlerId, action);
        }
        break;
    }
    case SubmenuType: {
        GRefPtr<GMenu> submenu = buildMenu(menuItem.submenuItems());
        gMenuItem = adoptGRef(g_menu_item_new_submenu(menuItem.title().utf8().data(), G_MENU_MODEL(submenu.get())));
        break;
    }
    case SeparatorType:
        ASSERT_NOT_REACHED();
        break;
    }

    g_menu_append_item(menu, gMenuItem.get());
}

GRefPtr<GMenu> WebContextMenuProxyGtk::buildMenu(const Vector<WebContextMenuItemGlib>& items)
{
    GRefPtr<GMenu> menu = adoptGRef(g_menu_new());
    GMenu* sectionMenu = menu.get();
    for (const auto& item : items) {
        if (item.type() == SeparatorType) {
            GRefPtr<GMenu> section = adoptGRef(g_menu_new());
            g_menu_append_section(menu.get(), nullptr, G_MENU_MODEL(section.get()));
            sectionMenu = section.get();
        } else
            append(sectionMenu, item);
    }

    return menu;
}

Vector<WebContextMenuItemGlib> WebContextMenuProxyGtk::populateSubMenu(const WebContextMenuItemData& subMenuItemData)
{
    Vector<WebContextMenuItemGlib> items;
    for (const auto& itemData : subMenuItemData.submenu()) {
        if (itemData.type() == SubmenuType)
            items.append(WebContextMenuItemGlib(itemData, populateSubMenu(itemData)));
        else
            items.append(itemData);
    }
    return items;
}

void WebContextMenuProxyGtk::populate(const Vector<WebContextMenuItemGlib>& items)
{
    GRefPtr<GMenu> menu = buildMenu(items);
    bindModelToMenuWidget(m_menu, G_MENU_MODEL(menu.get()));
}

void WebContextMenuProxyGtk::populate(const Vector<Ref<WebContextMenuItem>>& items)
{
    GRefPtr<GMenu> menu = adoptGRef(g_menu_new());
    GMenu* sectionMenu = menu.get();
    for (const auto& item : items) {
        switch (item->data().type()) {
        case SeparatorType: {
            GRefPtr<GMenu> section = adoptGRef(g_menu_new());
            g_menu_append_section(menu.get(), nullptr, G_MENU_MODEL(section.get()));
            sectionMenu = section.get();
            break;
        }
        case SubmenuType: {
            WebContextMenuItemGlib menuitem(item->data(), populateSubMenu(item->data()));
            append(sectionMenu, menuitem);
            break;
        }
        case ActionType:
        case CheckableActionType: {
            WebContextMenuItemGlib menuitem(item->data());
            append(sectionMenu, menuitem);
            break;
        }
        }
    }
    bindModelToMenuWidget(m_menu, G_MENU_MODEL(menu.get()));
}

void WebContextMenuProxyGtk::show()
{
    Vector<Ref<WebContextMenuItem>> proposedAPIItems;
    for (auto& item : m_context.menuItems()) {
        if (item.action() != ContextMenuItemTagShareMenu)
            proposedAPIItems.append(WebContextMenuItem::create(item));
    }

    m_page->contextMenuClient().getContextMenuFromProposedMenu(*m_page, WTFMove(proposedAPIItems), WebContextMenuListenerProxy::create(this).get(), m_context.webHitTestResultData(), m_page->process().transformHandlesToObjects(m_userData.object()).get());
}

void WebContextMenuProxyGtk::showContextMenuWithItems(Vector<Ref<WebContextMenuItem>>&& items)
{
    if (!items.isEmpty())
        populate(items);

    if (!menuWidgetHasItems(m_menu))
        return;

    NativeWebMouseEvent* mouseEvent = m_page->currentlyProcessedMouseDownEvent();
    const GdkEvent* event = mouseEvent ? mouseEvent->nativeEvent() : nullptr;
    const GdkRectangle rect = { m_context.menuLocation().x(), m_context.menuLocation().y(), 1, 1 };
    popupMenuWidget(m_menu, event, rect);
}

WebContextMenuProxyGtk::WebContextMenuProxyGtk(GtkWidget* webView, WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
    : WebContextMenuProxy(WTFMove(context), userData)
    , m_webView(webView)
    , m_page(&page)
    , m_menu(createMenuWidget(m_webView))
{
    gtk_widget_insert_action_group(GTK_WIDGET(m_menu), gContextMenuItemGroup, G_ACTION_GROUP(m_actionGroup.get()));
    webkitWebViewBaseSetActiveContextMenuProxy(WEBKIT_WEB_VIEW_BASE(m_webView), this);
}

WebContextMenuProxyGtk::~WebContextMenuProxyGtk()
{
    popdownMenuWidget(m_menu);

    for (auto& handler : m_signalHandlers)
        g_signal_handler_disconnect(handler.value, handler.key);
    m_signalHandlers.clear();

    gtk_widget_insert_action_group(GTK_WIDGET(m_menu), gContextMenuItemGroup, nullptr);
    destroyMenuWidget(m_menu);
}

} // namespace WebKit
#endif // ENABLE(CONTEXT_MENUS)
