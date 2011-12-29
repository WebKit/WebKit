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

#include "NativeWebMouseEvent.h"
#include "WebContextMenuItemData.h"
#include "WebPageProxy.h"
#include <WebCore/ContextMenu.h>
#include <WebCore/GtkUtilities.h>
#include <gtk/gtk.h>
#include <wtf/text/CString.h>


static const char* gContextMenuActionId = "webkit-context-menu-action";

using namespace WebCore;

namespace WebKit {

static void contextMenuItemActivatedCallback(GtkAction* action, WebPageProxy* page)
{
    gboolean isToggle = GTK_IS_TOGGLE_ACTION(action);
    WebKit::WebContextMenuItemData item(isToggle ? WebCore::CheckableActionType : WebCore::ActionType,
                                        static_cast<WebCore::ContextMenuAction>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), gContextMenuActionId))),
                                        gtk_action_get_label(action), gtk_action_get_sensitive(action),
                                        isToggle ? gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action)) : false);
    page->contextMenuItemSelected(item);
}

GtkMenu* WebContextMenuProxyGtk::createGtkMenu(const Vector<WebContextMenuItemData>& items)
{
    ContextMenu menu;
    for (size_t i = 0; i < items.size(); i++) {
        const WebContextMenuItemData& item = items.at(i);
        ContextMenuItem menuItem(item.type(), item.action(), item.title(), item.enabled(), item.checked());
        GtkAction* action = menuItem.gtkAction();

        if (action && (item.type() == WebCore::ActionType || item.type() == WebCore::CheckableActionType)) {
            g_object_set_data(G_OBJECT(action), gContextMenuActionId, GINT_TO_POINTER(item.action()));
            g_signal_connect(action, "activate", G_CALLBACK(contextMenuItemActivatedCallback), m_page);
        }

        if (item.type() == WebCore::SubmenuType) {
            ContextMenu subMenu(createGtkMenu(item.submenu()));
            menuItem.setSubMenu(&subMenu);
        }
        menu.appendItem(menuItem);
    }
    return menu.releasePlatformDescription();
}

void WebContextMenuProxyGtk::showContextMenu(const WebCore::IntPoint& position, const Vector<WebContextMenuItemData>& items)
{
    if (items.isEmpty())
        return;

    m_popup = createGtkMenu(items);
    m_popupPosition = convertWidgetPointToScreenPoint(m_webView, position);

    // Display menu initiated by right click (mouse button pressed = 3).
    NativeWebMouseEvent* mouseEvent = m_page->currentlyProcessedMouseDownEvent();
    const GdkEvent* event = mouseEvent ? mouseEvent->nativeEvent() : 0;
    gtk_menu_popup(m_popup, 0, 0, reinterpret_cast<GtkMenuPositionFunc>(menuPositionFunction), this,
                   event ? event->button.button : 3, event ? event->button.time : GDK_CURRENT_TIME);
}

void WebContextMenuProxyGtk::hideContextMenu()
{
    gtk_menu_popdown(m_popup);
}

WebContextMenuProxyGtk::WebContextMenuProxyGtk(GtkWidget* webView, WebPageProxy* page)
    : m_webView(webView)
    , m_page(page)
    , m_popup(0)
{
}

WebContextMenuProxyGtk::~WebContextMenuProxyGtk()
{
    if (m_popup)
        gtk_widget_destroy(GTK_WIDGET(m_popup));
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
