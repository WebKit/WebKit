/*
 * Copyright (C) 2011 Igalia S.L.
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
#include <gtk/gtk.h>
#include <wtf/text/CString.h>


static const char* gContextMenuActionId = "webkit-context-menu-action";

using namespace WebCore;

namespace WebKit {

static void contextMenuItemActivatedCallback(GtkAction* action, WebPageProxy* page)
{
    gboolean isToggle = GTK_IS_TOGGLE_ACTION(action);
    WebContextMenuItemData item(isToggle ? CheckableActionType : ActionType,
        static_cast<ContextMenuAction>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), gContextMenuActionId))),
        String::fromUTF8(gtk_action_get_label(action)), gtk_action_get_sensitive(action),
        isToggle ? gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action)) : false);
    page->contextMenuItemSelected(item);
}

static void contextMenuItemVisibilityChanged(GtkAction*, GParamSpec*, WebContextMenuProxyGtk* contextMenuProxy)
{
    GtkMenu* menu = contextMenuProxy->gtkMenu();
    if (!menu)
        return;

    GUniquePtr<GList> items(gtk_container_get_children(GTK_CONTAINER(menu)));
    bool previousVisibleItemIsNotASeparator = false;
    GtkWidget* lastItemVisibleSeparator = 0;
    for (GList* iter = items.get(); iter; iter = g_list_next(iter)) {
        GtkWidget* widget = GTK_WIDGET(iter->data);

        if (GTK_IS_SEPARATOR_MENU_ITEM(widget)) {
            if (previousVisibleItemIsNotASeparator) {
                gtk_widget_show(widget);
                lastItemVisibleSeparator = widget;
                previousVisibleItemIsNotASeparator = false;
            } else
                gtk_widget_hide(widget);
        } else if (gtk_widget_get_visible(widget)) {
            lastItemVisibleSeparator = 0;
            previousVisibleItemIsNotASeparator = true;
        }
    }

    if (lastItemVisibleSeparator)
        gtk_widget_hide(lastItemVisibleSeparator);
}

void WebContextMenuProxyGtk::append(GtkMenu* menu, const WebContextMenuItemGtk& menuItem)
{
    unsigned long signalHandlerId;
    GtkWidget* gtkMenuItem;
    if (GtkAction* action = menuItem.gtkAction()) {
        gtkMenuItem = gtk_action_create_menu_item(action);

        switch (menuItem.type()) {
        case ActionType:
        case CheckableActionType:
            g_object_set_data(G_OBJECT(action), gContextMenuActionId, GINT_TO_POINTER(menuItem.action()));
            signalHandlerId = g_signal_connect(action, "activate", G_CALLBACK(contextMenuItemActivatedCallback), m_page);
            m_signalHandlers.set(signalHandlerId, action);
            signalHandlerId = g_signal_connect(action, "notify::visible", G_CALLBACK(contextMenuItemVisibilityChanged), this);
            m_signalHandlers.set(signalHandlerId, action);
            break;
        case SubmenuType: {
            signalHandlerId = g_signal_connect(action, "notify::visible", G_CALLBACK(contextMenuItemVisibilityChanged), this);
            m_signalHandlers.set(signalHandlerId, action);
            GtkMenu* submenu = GTK_MENU(gtk_menu_new());
            for (const auto& item : menuItem.submenuItems())
                append(submenu, item);
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(gtkMenuItem), GTK_WIDGET(submenu));
            break;
        }
        case SeparatorType:
            ASSERT_NOT_REACHED();
            break;
        }
    } else {
        ASSERT(menuItem.type() == SeparatorType);
        gtkMenuItem = gtk_separator_menu_item_new();
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtkMenuItem);
    gtk_widget_show(gtkMenuItem);
}

// Populate the context menu ensuring that:
//  - There aren't separators next to each other.
//  - There aren't separators at the beginning of the menu.
//  - There aren't separators at the end of the menu.
void WebContextMenuProxyGtk::populate(Vector<WebContextMenuItemGtk>& items)
{
    bool previousIsSeparator = false;
    bool isEmpty = true;
    for (size_t i = 0; i < items.size(); i++) {
        WebContextMenuItemGtk& menuItem = items.at(i);
        if (menuItem.type() == SeparatorType) {
            previousIsSeparator = true;
            continue;
        }

        if (previousIsSeparator && !isEmpty)
            append(m_menu, items.at(i - 1));
        previousIsSeparator = false;

        append(m_menu, menuItem);
        isEmpty = false;
    }
}

void WebContextMenuProxyGtk::populate(const Vector<RefPtr<WebContextMenuItem>>& items)
{
    for (const auto& item : items) {
        WebContextMenuItemGtk menuitem(item->data());
        append(m_menu, menuitem);
    }
}

void WebContextMenuProxyGtk::show()
{
    Vector<RefPtr<WebContextMenuItem>> proposedAPIItems;
    for (auto& item : m_context.menuItems()) {
        if (item.action() != ContextMenuItemTagShareMenu)
            proposedAPIItems.append(WebContextMenuItem::create(item));
    }

    Vector<RefPtr<WebContextMenuItem>> clientItems;
    bool useProposedItems = true;

    if (m_page->contextMenuClient().getContextMenuFromProposedMenu(*m_page, proposedAPIItems, clientItems, m_context.webHitTestResultData(), m_page->process().transformHandlesToObjects(m_userData.object()).get()))
        useProposedItems = false;

    const Vector<RefPtr<WebContextMenuItem>>& items = useProposedItems ? proposedAPIItems : clientItems;

    if (!items.isEmpty())
        populate(items);

    unsigned childCount = 0;
    gtk_container_foreach(GTK_CONTAINER(m_menu), [](GtkWidget*, gpointer data) { (*static_cast<unsigned*>(data))++; }, &childCount);
    if (!childCount)
        return;

    m_popupPosition = convertWidgetPointToScreenPoint(m_webView, m_context.menuLocation());

    // Display menu initiated by right click (mouse button pressed = 3).
    NativeWebMouseEvent* mouseEvent = m_page->currentlyProcessedMouseDownEvent();
    const GdkEvent* event = mouseEvent ? mouseEvent->nativeEvent() : 0;
    gtk_menu_attach_to_widget(m_menu, GTK_WIDGET(m_webView), nullptr);
    gtk_menu_popup(m_menu, nullptr, nullptr, reinterpret_cast<GtkMenuPositionFunc>(menuPositionFunction), this,
                   event ? event->button.button : 3, event ? event->button.time : GDK_CURRENT_TIME);
}

WebContextMenuProxyGtk::WebContextMenuProxyGtk(GtkWidget* webView, WebPageProxy& page, const ContextMenuContextData& context, const UserData& userData)
    : WebContextMenuProxy(context, userData)
    , m_webView(webView)
    , m_page(&page)
    , m_menu(GTK_MENU(gtk_menu_new()))
{
    webkitWebViewBaseSetActiveContextMenuProxy(WEBKIT_WEB_VIEW_BASE(m_webView), this);
}

WebContextMenuProxyGtk::~WebContextMenuProxyGtk()
{
    gtk_menu_popdown(m_menu);

    for (auto& handler : m_signalHandlers)
        g_signal_handler_disconnect(handler.value, handler.key);
    m_signalHandlers.clear();

    gtk_widget_destroy(GTK_WIDGET(m_menu));
}

void WebContextMenuProxyGtk::menuPositionFunction(GtkMenu* menu, gint* x, gint* y, gboolean* pushIn, WebContextMenuProxyGtk* popupMenu)
{
    GtkRequisition menuSize;
    gtk_widget_get_preferred_size(GTK_WIDGET(menu), &menuSize, 0);

    GdkScreen* screen = gtk_widget_get_screen(popupMenu->m_webView);
    *x = popupMenu->m_popupPosition.x();
    if ((*x + menuSize.width) >= gdk_screen_get_width(screen))
        *x -= menuSize.width;

    *y = popupMenu->m_popupPosition.y();
    if ((*y + menuSize.height) >= gdk_screen_get_height(screen))
        *y -= menuSize.height;

    *pushIn = FALSE;
}

} // namespace WebKit
#endif // ENABLE(CONTEXT_MENUS)
