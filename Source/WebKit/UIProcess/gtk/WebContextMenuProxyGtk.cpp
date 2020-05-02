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
        g_menu_item_set_action_and_target_value(gMenuItem.get(), g_action_get_name(action), menuItem.gActionTarget());

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
    gtk_popover_bind_model(m_menu, G_MENU_MODEL(menu.get()), gContextMenuItemGroup);
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
    gtk_popover_bind_model(m_menu, G_MENU_MODEL(menu.get()), gContextMenuItemGroup);
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

    unsigned childCount = 0;
    gtk_container_foreach(GTK_CONTAINER(m_menu), [](GtkWidget*, gpointer data) { (*static_cast<unsigned*>(data))++; }, &childCount);
    if (!childCount)
        return;

    const GdkRectangle rect = { m_context.menuLocation().x(), m_context.menuLocation().y(), 1, 1 };
    gtk_popover_set_pointing_to(m_menu, &rect);
    gtk_popover_popup(m_menu);
}

WebContextMenuProxyGtk::WebContextMenuProxyGtk(GtkWidget* webView, WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
    : WebContextMenuProxy(WTFMove(context), userData)
    , m_webView(webView)
    , m_page(&page)
    , m_menu(GTK_POPOVER(gtk_popover_menu_new()))
{
    gtk_popover_set_has_arrow(m_menu, FALSE);
    gtk_popover_set_position(m_menu, GTK_POS_BOTTOM);
    gtk_popover_set_relative_to(m_menu, m_webView);
    gtk_widget_insert_action_group(GTK_WIDGET(m_menu), gContextMenuItemGroup, G_ACTION_GROUP(m_actionGroup.get()));
    webkitWebViewBaseSetActiveContextMenuProxy(WEBKIT_WEB_VIEW_BASE(m_webView), this);
}

WebContextMenuProxyGtk::~WebContextMenuProxyGtk()
{
    gtk_popover_popdown(m_menu);

    for (auto& handler : m_signalHandlers)
        g_signal_handler_disconnect(handler.value, handler.key);
    m_signalHandlers.clear();

    gtk_widget_insert_action_group(GTK_WIDGET(m_menu), gContextMenuItemGroup, nullptr);
    gtk_widget_destroy(GTK_WIDGET(m_menu));
}

} // namespace WebKit
#endif // ENABLE(CONTEXT_MENUS)
