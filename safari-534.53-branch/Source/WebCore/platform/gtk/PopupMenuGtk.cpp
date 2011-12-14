/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2010 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "PopupMenuGtk.h"

#include "FrameView.h"
#include "GOwnPtr.h"
#include "GtkVersioning.h"
#include "HostWindow.h"
#include "PlatformString.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <wtf/text/CString.h>

namespace WebCore {

static const uint32_t gSearchTimeoutMs = 1000;

PopupMenuGtk::PopupMenuGtk(PopupMenuClient* client)
    : m_popupClient(client)
    , m_previousKeyEventCharacter(0)
    , m_currentlySelectedMenuItem(0)
{
}

PopupMenuGtk::~PopupMenuGtk()
{
    if (m_popup) {
        g_signal_handlers_disconnect_matched(m_popup.get(), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
        hide();
    }
}

void PopupMenuGtk::show(const IntRect& rect, FrameView* view, int index)
{
    ASSERT(client());

    if (!m_popup) {
        m_popup = GTK_MENU(gtk_menu_new());
        g_signal_connect(m_popup.get(), "unmap", G_CALLBACK(PopupMenuGtk::menuUnmapped), this);
        g_signal_connect(m_popup.get(), "key-press-event", G_CALLBACK(PopupMenuGtk::keyPressEventCallback), this);
    } else
        gtk_container_foreach(GTK_CONTAINER(m_popup.get()), reinterpret_cast<GtkCallback>(menuRemoveItem), this);

    int x = 0;
    int y = 0;
    GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(view->hostWindow()->platformPageClient()));
    if (window)
        gdk_window_get_origin(window, &x, &y);
    m_menuPosition = view->contentsToWindow(rect.location());
    m_menuPosition = IntPoint(m_menuPosition.x() + x, m_menuPosition.y() + y + rect.height());
    m_indexMap.clear();

    const int size = client()->listSize();
    for (int i = 0; i < size; ++i) {
        GtkWidget* item;
        if (client()->itemIsSeparator(i))
            item = gtk_separator_menu_item_new();
        else
            item = gtk_menu_item_new_with_label(client()->itemText(i).utf8().data());

        m_indexMap.add(item, i);
        g_signal_connect(item, "activate", G_CALLBACK(PopupMenuGtk::menuItemActivated), this);
        g_signal_connect(item, "select", G_CALLBACK(PopupMenuGtk::selectItemCallback), this);

        // FIXME: Apply the PopupMenuStyle from client()->itemStyle(i)
        gtk_widget_set_sensitive(item, client()->itemIsEnabled(i));
        gtk_menu_shell_append(GTK_MENU_SHELL(m_popup.get()), item);
        gtk_widget_show(item);
    }

    gtk_menu_set_active(m_popup.get(), index);


    // The size calls are directly copied from gtkcombobox.c which is LGPL
    GtkRequisition requisition;
    gtk_widget_set_size_request(GTK_WIDGET(m_popup.get()), -1, -1);
#ifdef GTK_API_VERSION_2
    gtk_widget_size_request(GTK_WIDGET(m_popup.get()), &requisition);
#else
    gtk_widget_get_preferred_size(GTK_WIDGET(m_popup.get()), &requisition, 0);
#endif

    gtk_widget_set_size_request(GTK_WIDGET(m_popup.get()), std::max(rect.width(), requisition.width), -1);

    GList* children = gtk_container_get_children(GTK_CONTAINER(m_popup.get()));
    GList* p = children;
    if (size) {
        for (int i = 0; i < size; i++) {
            if (i > index)
              break;

            GtkWidget* item = reinterpret_cast<GtkWidget*>(p->data);
            GtkRequisition itemRequisition;
#ifdef GTK_API_VERSION_2
            gtk_widget_get_child_requisition(item, &itemRequisition);
#else
            gtk_widget_get_preferred_size(item, &itemRequisition, 0);
#endif
            m_menuPosition.setY(m_menuPosition.y() - itemRequisition.height);

            p = g_list_next(p);
        }
    } else {
        // Center vertically the empty popup in the combo box area
        m_menuPosition.setY(m_menuPosition.y() - rect.height() / 2);
    }

    g_list_free(children);
    gtk_menu_popup(m_popup.get(), 0, 0, reinterpret_cast<GtkMenuPositionFunc>(menuPositionFunction), this, 0, gtk_get_current_event_time());
}

void PopupMenuGtk::hide()
{
    ASSERT(m_popup);
    gtk_menu_popdown(m_popup.get());
}

void PopupMenuGtk::updateFromElement()
{
    client()->setTextFromItem(client()->selectedIndex());
}

void PopupMenuGtk::disconnectClient()
{
    m_popupClient = 0;
}

bool PopupMenuGtk::typeAheadFind(GdkEventKey* event)
{
    // If we were given a non-printable character just skip it.
    gunichar unicodeCharacter = gdk_keyval_to_unicode(event->keyval);
    if (!unicodeCharacter) {
        resetTypeAheadFindState();
        return false;
    }

    glong charactersWritten;
    GOwnPtr<gunichar2> utf16String(g_ucs4_to_utf16(&unicodeCharacter, 1, 0, &charactersWritten, 0));
    if (!utf16String) {
        resetTypeAheadFindState();
        return false;
    }

    // If the character is the same as the last character, the user is probably trying to
    // cycle through the menulist entries. This matches the WebCore behavior for collapsed
    // menulists.
    bool repeatingCharacter = unicodeCharacter != m_previousKeyEventCharacter;
    if (event->time - m_previousKeyEventTimestamp > gSearchTimeoutMs)
        m_currentSearchString = String(static_cast<UChar*>(utf16String.get()), charactersWritten);
    else if (repeatingCharacter)
        m_currentSearchString.append(String(static_cast<UChar*>(utf16String.get()), charactersWritten));

    m_previousKeyEventTimestamp = event->time;
    m_previousKeyEventCharacter = unicodeCharacter;

    // Like the Chromium port, we case fold before searching, because 
    // strncmp does not handle non-ASCII characters.
    GOwnPtr<gchar> searchStringWithCaseFolded(g_utf8_casefold(m_currentSearchString.utf8().data(), -1));
    size_t prefixLength = strlen(searchStringWithCaseFolded.get());

    GList* children = gtk_container_get_children(GTK_CONTAINER(m_popup.get()));
    if (!children)
        return true;

    // If a menu item has already been selected, start searching from the current
    // item down the list. This will make multiple key presses of the same character
    // advance the selection.
    GList* currentChild = children;
    if (m_currentlySelectedMenuItem) {
        currentChild = g_list_find(children, m_currentlySelectedMenuItem);
        if (!currentChild) {
            m_currentlySelectedMenuItem = 0;
            currentChild = children;
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
            currentChild = children;

        GOwnPtr<gchar> itemText(g_utf8_casefold(gtk_menu_item_get_label(GTK_MENU_ITEM(currentChild->data)), -1));
        if (!strncmp(searchStringWithCaseFolded.get(), itemText.get(), prefixLength)) {
            gtk_menu_shell_select_item(GTK_MENU_SHELL(m_popup.get()), GTK_WIDGET(currentChild->data));
            return true;
        }
    } while (currentChild != firstChild);

    return true;
}

void PopupMenuGtk::menuItemActivated(GtkMenuItem* item, PopupMenuGtk* that)
{
    ASSERT(that->client());
    ASSERT(that->m_indexMap.contains(GTK_WIDGET(item)));
    that->client()->valueChanged(that->m_indexMap.get(GTK_WIDGET(item)));
}

void PopupMenuGtk::menuUnmapped(GtkWidget*, PopupMenuGtk* that)
{
    ASSERT(that->client());
    that->resetTypeAheadFindState();
    that->client()->popupDidHide();
}

void PopupMenuGtk::menuPositionFunction(GtkMenu*, gint* x, gint* y, gboolean* pushIn, PopupMenuGtk* that)
{
    *x = that->m_menuPosition.x();
    *y = that->m_menuPosition.y();
    *pushIn = true;
}

void PopupMenuGtk::resetTypeAheadFindState()
{
    m_currentlySelectedMenuItem = 0;
    m_previousKeyEventCharacter = 0;
    m_currentSearchString = "";
}

void PopupMenuGtk::menuRemoveItem(GtkWidget* widget, PopupMenuGtk* that)
{
    ASSERT(that->m_popup);
    gtk_container_remove(GTK_CONTAINER(that->m_popup.get()), widget);
}

int PopupMenuGtk::selectItemCallback(GtkMenuItem* item, PopupMenuGtk* that)
{
    that->m_currentlySelectedMenuItem = GTK_WIDGET(item);
    return FALSE;
}

int PopupMenuGtk::keyPressEventCallback(GtkWidget* widget, GdkEventKey* event, PopupMenuGtk* that)
{
    return that->typeAheadFind(event);
}

}

