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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
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
#include "WebPopupMenuProxyGtk.h"

#include "NativeWebMouseEvent.h"
#include "WebPopupItem.h"
#include <WebCore/GtkUtilities.h>
#include <WebCore/IntRect.h>
#include <gtk/gtk.h>
#include <wtf/gobject/GUniquePtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

WebPopupMenuProxyGtk::WebPopupMenuProxyGtk(GtkWidget* webView, WebPopupMenuProxy::Client* client)
    : WebPopupMenuProxy(client)
    , m_webView(webView)
    , m_popup(gtk_menu_new())
    , m_activeItem(-1)
    , m_previousKeyEventCharacter(0)
    , m_previousKeyEventTimestamp(0)
    , m_currentlySelectedMenuItem(nullptr)
{
    g_signal_connect(m_popup, "key-press-event", G_CALLBACK(keyPressEventCallback), this);
}

WebPopupMenuProxyGtk::~WebPopupMenuProxyGtk()
{
    if (m_popup) {
        g_signal_handlers_disconnect_matched(m_popup, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
        hidePopupMenu();
        gtk_widget_destroy(m_popup);
    }
}

GtkAction* WebPopupMenuProxyGtk::createGtkActionForMenuItem(const WebPopupItem& item, int itemIndex)
{
    GUniquePtr<char> actionName(g_strdup_printf("popup-menu-action-%d", itemIndex));
    GtkAction* action = gtk_action_new(actionName.get(), item.m_text.utf8().data(), item.m_toolTip.utf8().data(), 0);
    g_object_set_data(G_OBJECT(action), "popup-menu-action-index", GINT_TO_POINTER(itemIndex));
    g_signal_connect(action, "activate", G_CALLBACK(menuItemActivated), this);
    gtk_action_set_sensitive(action, item.m_isEnabled);

    return action;
}

void WebPopupMenuProxyGtk::populatePopupMenu(const Vector<WebPopupItem>& items)
{
    int itemIndex = 0;
    for (const auto& item : items) {
        if (item.m_type == WebPopupItem::Separator) {
            GtkWidget* menuItem = gtk_separator_menu_item_new();
            gtk_menu_shell_append(GTK_MENU_SHELL(m_popup), menuItem);
            gtk_widget_show(menuItem);
        } else {
            GRefPtr<GtkAction> action = adoptGRef(createGtkActionForMenuItem(item, itemIndex));
            GtkWidget* menuItem = gtk_action_create_menu_item(action.get());
            gtk_widget_set_tooltip_text(menuItem, gtk_action_get_tooltip(action.get()));
            g_signal_connect(menuItem, "select", G_CALLBACK(selectItemCallback), this);
            gtk_menu_shell_append(GTK_MENU_SHELL(m_popup), menuItem);

            if (gtk_action_is_visible(action.get()))
                gtk_widget_show(menuItem);
        }
        itemIndex++;
    }
}

void WebPopupMenuProxyGtk::showPopupMenu(const IntRect& rect, TextDirection, double /* pageScaleFactor */, const Vector<WebPopupItem>& items, const PlatformPopupMenuData&, int32_t selectedIndex)
{
    populatePopupMenu(items);
    gtk_menu_set_active(GTK_MENU(m_popup), selectedIndex);

    resetTypeAheadFindState();


    IntPoint menuPosition = convertWidgetPointToScreenPoint(m_webView, rect.location());
    menuPosition.move(0, rect.height());

    // This approach follows the one in gtkcombobox.c.
    GtkRequisition requisition;
    gtk_widget_set_size_request(m_popup, -1, -1);
    gtk_widget_get_preferred_size(m_popup, &requisition, nullptr);
    gtk_widget_set_size_request(m_popup, std::max(rect.width(), requisition.width), -1);

    if (int itemCount = items.size()) {
        GUniquePtr<GList> children(gtk_container_get_children(GTK_CONTAINER(m_popup)));
        int i;
        GList* child;
        for (i = 0, child = children.get(); i < itemCount; i++, child = g_list_next(child)) {
            if (i > selectedIndex)
                break;

            GtkWidget* item = GTK_WIDGET(child->data);
            GtkRequisition itemRequisition;
            gtk_widget_get_preferred_size(item, &itemRequisition, nullptr);
            menuPosition.setY(menuPosition.y() - itemRequisition.height);
        }
    } else {
        // Center vertically the empty popup in the combo box area.
        menuPosition.setY(menuPosition.y() - rect.height() / 2);
    }

    gulong unmapHandler = g_signal_connect(m_popup, "unmap", G_CALLBACK(menuUnmapped), this);

    const GdkEvent* event = m_client->currentlyProcessedMouseDownEvent() ? m_client->currentlyProcessedMouseDownEvent()->nativeEvent() : nullptr;
    gtk_menu_popup_for_device(GTK_MENU(m_popup), event ? gdk_event_get_device(event) : nullptr, nullptr, nullptr,
        [](GtkMenu*, gint* x, gint* y, gboolean* pushIn, gpointer userData) {
            // We can pass a pointer to the menuPosition local variable because the nested main loop ensures this is called in the function context.
            IntPoint* menuPosition = static_cast<IntPoint*>(userData);
            *x = menuPosition->x();
            *y = menuPosition->y();
            *pushIn = TRUE;
        }, &menuPosition, nullptr, event && event->type == GDK_BUTTON_PRESS ? event->button.button : 1,
        event ? gdk_event_get_time(event) : GDK_CURRENT_TIME);

    // Now that the menu has a position, schedule a resize to make sure it's resized to fit vertically in the work area.
    gtk_widget_queue_resize(m_popup);

    // PopupMenu can fail to open when there is no mouse grab.
    // Ensure WebCore does not go into some pesky state.
    if (!gtk_widget_get_visible(m_popup)) {
       m_client->failedToShowPopupMenu();
       return;
    }

    // WebPageProxy expects the menu to run in a nested run loop, since it invalidates the
    // menu right after calling WebPopupMenuProxy::showPopupMenu().
    m_runLoop = adoptGRef(g_main_loop_new(nullptr, FALSE));

// This is to suppress warnings about gdk_threads_leave and gdk_threads_enter.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gdk_threads_leave();
    g_main_loop_run(m_runLoop.get());
    gdk_threads_enter();
#pragma GCC diagnostic pop

    m_runLoop.clear();

    g_signal_handler_disconnect(m_popup, unmapHandler);

    if (!m_client)
        return;

    m_client->valueChangedForPopupMenu(this, m_activeItem);
}

void WebPopupMenuProxyGtk::hidePopupMenu()
{
    gtk_menu_popdown(GTK_MENU(m_popup));
    resetTypeAheadFindState();
}

bool WebPopupMenuProxyGtk::typeAheadFind(GdkEventKey* event)
{
    // If we were given a non-printable character just skip it.
    gunichar unicodeCharacter = gdk_keyval_to_unicode(event->keyval);
    if (!g_unichar_isprint(unicodeCharacter)) {
        resetTypeAheadFindState();
        return false;
    }

    glong charactersWritten;
    GUniquePtr<gunichar2> utf16String(g_ucs4_to_utf16(&unicodeCharacter, 1, nullptr, &charactersWritten, nullptr));
    if (!utf16String) {
        resetTypeAheadFindState();
        return false;
    }

    // If the character is the same as the last character, the user is probably trying to
    // cycle through the menulist entries. This matches the WebCore behavior for collapsed menulists.
    static const uint32_t searchTimeoutMs = 1000;
    bool repeatingCharacter = unicodeCharacter != m_previousKeyEventCharacter;
    if (event->time - m_previousKeyEventTimestamp > searchTimeoutMs)
        m_currentSearchString = String(reinterpret_cast<UChar*>(utf16String.get()), charactersWritten);
    else if (repeatingCharacter)
        m_currentSearchString.append(String(reinterpret_cast<UChar*>(utf16String.get()), charactersWritten));

    m_previousKeyEventTimestamp = event->time;
    m_previousKeyEventCharacter = unicodeCharacter;

    GUniquePtr<GList> children(gtk_container_get_children(GTK_CONTAINER(m_popup)));
    if (!children)
        return true;

    // We case fold before searching, because strncmp does not handle non-ASCII characters.
    GUniquePtr<gchar> searchStringWithCaseFolded(g_utf8_casefold(m_currentSearchString.utf8().data(), -1));
    size_t prefixLength = strlen(searchStringWithCaseFolded.get());

    // If a menu item has already been selected, start searching from the current
    // item down the list. This will make multiple key presses of the same character
    // advance the selection.
    GList* currentChild = children.get();
    if (m_currentlySelectedMenuItem) {
        currentChild = g_list_find(children.get(), m_currentlySelectedMenuItem);
        if (!currentChild) {
            m_currentlySelectedMenuItem = nullptr;
            currentChild = children.get();
        }

        // Repeating characters should iterate.
        if (repeatingCharacter) {
            if (GList* nextChild = g_list_next(currentChild))
                currentChild = nextChild;
        }
    }

    GList* firstChild = currentChild;
    do {
        currentChild = g_list_next(currentChild);
        if (!currentChild)
            currentChild = children.get();

        GUniquePtr<gchar> itemText(g_utf8_casefold(gtk_menu_item_get_label(GTK_MENU_ITEM(currentChild->data)), -1));
        if (!strncmp(searchStringWithCaseFolded.get(), itemText.get(), prefixLength)) {
            gtk_menu_shell_select_item(GTK_MENU_SHELL(m_popup), GTK_WIDGET(currentChild->data));
            break;
        }
    } while (currentChild != firstChild);

    return true;
}

void WebPopupMenuProxyGtk::resetTypeAheadFindState()
{
    m_currentlySelectedMenuItem = nullptr;
    m_previousKeyEventCharacter = 0;
    m_previousKeyEventTimestamp = 0;
    m_currentSearchString = emptyString();
}

void WebPopupMenuProxyGtk::shutdownRunLoop()
{
    if (g_main_loop_is_running(m_runLoop.get()))
        g_main_loop_quit(m_runLoop.get());
}

void WebPopupMenuProxyGtk::menuItemActivated(GtkAction* action, WebPopupMenuProxyGtk* popupMenu)
{
    popupMenu->setActiveItem(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(action), "popup-menu-action-index")));
    popupMenu->shutdownRunLoop();
}

void WebPopupMenuProxyGtk::menuUnmapped(GtkWidget*, WebPopupMenuProxyGtk* popupMenu)
{
    popupMenu->shutdownRunLoop();
}

void WebPopupMenuProxyGtk::selectItemCallback(GtkWidget* item, WebPopupMenuProxyGtk* popupMenu)
{
    popupMenu->setCurrentlySelectedMenuItem(item);
}

gboolean WebPopupMenuProxyGtk::keyPressEventCallback(GtkWidget*, GdkEventKey* event, WebPopupMenuProxyGtk* popupMenu)
{
    return popupMenu->typeAheadFind(event);
}

} // namespace WebKit
