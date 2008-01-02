/*
 *  Copyright (C) 2007 Holger Hans Peter Freyther
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ContextMenu.h"
#include "ContextMenuItem.h"
#include "CString.h"
#include "NotImplemented.h"

#include <gtk/gtk.h>

#define WEBKIT_CONTEXT_MENU_ACTION "webkit-context-menu"

namespace WebCore {

ContextMenuItem::ContextMenuItem(PlatformMenuItemDescription item)
    : m_platformDescription(item)
{
    g_object_ref(m_platformDescription);
}

ContextMenuItem::ContextMenuItem(ContextMenu*)
{
    notImplemented();
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* subMenu)
    : m_platformDescription(0)
{
    if (type == SeparatorType) {
        m_platformDescription = GTK_MENU_ITEM(gtk_separator_menu_item_new());
    } else {
        m_platformDescription = GTK_MENU_ITEM(gtk_menu_item_new_with_label(title.utf8().data()));
        setAction(action);

        if (subMenu)
            setSubMenu(subMenu);
    }

#if GLIB_CHECK_VERSION(2,10,0)
    g_object_ref_sink(G_OBJECT(m_platformDescription));
#else
    g_object_ref(G_OBJECT(m_platformDescription));
    gtk_object_sink(GTK_OBJECT(m_platformDescription));
#endif
}

ContextMenuItem::~ContextMenuItem()
{
    if (m_platformDescription)
        g_object_unref(m_platformDescription);
}

PlatformMenuItemDescription ContextMenuItem::releasePlatformDescription()
{
    PlatformMenuItemDescription menuItem = m_platformDescription;
    m_platformDescription = 0;
    return menuItem;
}

ContextMenuItemType ContextMenuItem::type() const
{
    if (GTK_IS_SEPARATOR_MENU_ITEM(m_platformDescription))
        return SeparatorType;
    if (gtk_menu_item_get_submenu(m_platformDescription))
        return SubmenuType;

    return ActionType;
}

void ContextMenuItem::setType(ContextMenuItemType)
{
    notImplemented();
}

ContextMenuAction ContextMenuItem::action() const
{
    ASSERT(m_platformDescription);
    
    return *static_cast<ContextMenuAction*>(g_object_get_data(G_OBJECT(m_platformDescription), WEBKIT_CONTEXT_MENU_ACTION));
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
    ASSERT(m_platformDescription);

    // FIXME: Check the memory management of menuAction.
    ContextMenuAction* menuAction = static_cast<ContextMenuAction*>(malloc(sizeof(ContextMenuAction*)));
    *menuAction = action;
    g_object_set_data(G_OBJECT(m_platformDescription), WEBKIT_CONTEXT_MENU_ACTION, menuAction);
}

String ContextMenuItem::title() const
{
    notImplemented();
    return String();
}

void ContextMenuItem::setTitle(const String&)
{
    notImplemented();
}

PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{
    return GTK_MENU(gtk_menu_item_get_submenu(m_platformDescription));
}

void ContextMenuItem::setSubMenu(ContextMenu* menu)
{
    gtk_menu_item_set_submenu(m_platformDescription, GTK_WIDGET(menu->releasePlatformDescription()));
}

void ContextMenuItem::setChecked(bool shouldCheck)
{
    ASSERT(type() == ActionType);

    if (GTK_IS_CHECK_MENU_ITEM(m_platformDescription)) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(m_platformDescription), shouldCheck);
    } else {
        // TODO: Need to morph this into a GtkCheckMenuItem.
        notImplemented();
    }
}

void ContextMenuItem::setEnabled(bool shouldEnable)
{
    gtk_widget_set_sensitive(GTK_WIDGET(m_platformDescription), shouldEnable);
}

}
