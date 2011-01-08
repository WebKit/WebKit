/*
 *  Copyright (C) 2007 Holger Hans Peter Freyther
 *  Copyright (C) 2010 Igalia S.L
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

#include "ContextMenuItem.h"

#include "ContextMenu.h"
#include "GOwnPtr.h"
#include "NotImplemented.h"
#include <gtk/gtk.h>
#include <wtf/text/CString.h>

#define WEBKIT_CONTEXT_MENU_ACTION "webkit-context-menu"

namespace WebCore {

static const char* gtkStockIDFromContextMenuAction(const ContextMenuAction& action)
{
    switch (action) {
    case ContextMenuItemTagCopyLinkToClipboard:
    case ContextMenuItemTagCopyImageToClipboard:
    case ContextMenuItemTagCopyMediaLinkToClipboard:
    case ContextMenuItemTagCopy:
        return GTK_STOCK_COPY;
    case ContextMenuItemTagOpenLinkInNewWindow:
    case ContextMenuItemTagOpenImageInNewWindow:
    case ContextMenuItemTagOpenFrameInNewWindow:
    case ContextMenuItemTagOpenMediaInNewWindow:
        return GTK_STOCK_OPEN;
    case ContextMenuItemTagDownloadLinkToDisk:
    case ContextMenuItemTagDownloadImageToDisk:
        return GTK_STOCK_SAVE;
    case ContextMenuItemTagGoBack:
        return GTK_STOCK_GO_BACK;
    case ContextMenuItemTagGoForward:
        return GTK_STOCK_GO_FORWARD;
    case ContextMenuItemTagStop:
        return GTK_STOCK_STOP;
    case ContextMenuItemTagReload:
        return GTK_STOCK_REFRESH;
    case ContextMenuItemTagCut:
        return GTK_STOCK_CUT;
    case ContextMenuItemTagPaste:
        return GTK_STOCK_PASTE;
    case ContextMenuItemTagDelete:
        return GTK_STOCK_DELETE;
    case ContextMenuItemTagSelectAll:
        return GTK_STOCK_SELECT_ALL;
    case ContextMenuItemTagSpellingGuess:
        return 0;
    case ContextMenuItemTagIgnoreSpelling:
        return GTK_STOCK_NO;
    case ContextMenuItemTagLearnSpelling:
        return GTK_STOCK_OK;
    case ContextMenuItemTagOther:
        return GTK_STOCK_MISSING_IMAGE;
    case ContextMenuItemTagSearchInSpotlight:
        return GTK_STOCK_FIND;
    case ContextMenuItemTagSearchWeb:
        return GTK_STOCK_FIND;
    case ContextMenuItemTagOpenWithDefaultApplication:
        return GTK_STOCK_OPEN;
    case ContextMenuItemPDFZoomIn:
        return GTK_STOCK_ZOOM_IN;
    case ContextMenuItemPDFZoomOut:
        return GTK_STOCK_ZOOM_OUT;
    case ContextMenuItemPDFAutoSize:
        return GTK_STOCK_ZOOM_FIT;
    case ContextMenuItemPDFNextPage:
        return GTK_STOCK_GO_FORWARD;
    case ContextMenuItemPDFPreviousPage:
        return GTK_STOCK_GO_BACK;
    // New tags, not part of API
    case ContextMenuItemTagOpenLink:
        return GTK_STOCK_OPEN;
    case ContextMenuItemTagCheckSpelling:
        return GTK_STOCK_SPELL_CHECK;
    case ContextMenuItemTagFontMenu:
        return GTK_STOCK_SELECT_FONT;
    case ContextMenuItemTagShowFonts:
        return GTK_STOCK_SELECT_FONT;
    case ContextMenuItemTagBold:
        return GTK_STOCK_BOLD;
    case ContextMenuItemTagItalic:
        return GTK_STOCK_ITALIC;
    case ContextMenuItemTagUnderline:
        return GTK_STOCK_UNDERLINE;
    case ContextMenuItemTagShowColors:
        return GTK_STOCK_SELECT_COLOR;
    case ContextMenuItemTagToggleMediaControls:
    case ContextMenuItemTagToggleMediaLoop:
        // No icon for this.
        return 0;
    case ContextMenuItemTagEnterVideoFullscreen:
        return GTK_STOCK_FULLSCREEN;
    default:
        return 0;
    }
}

// Extract the ActionType from the menu item
ContextMenuItem::ContextMenuItem(PlatformMenuItemDescription item)
    : m_platformDescription(item)
{
}

ContextMenuItem::ContextMenuItem(ContextMenu*)
{
    notImplemented();
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* subMenu)
{
    if (type == SeparatorType) {
        m_platformDescription = GTK_MENU_ITEM(gtk_separator_menu_item_new());
        return;
    }

    GOwnPtr<char> actionName(g_strdup_printf("context-menu-action-%d", action));
    GtkAction* platformAction = 0;

    if (type == CheckableActionType)
        platformAction = GTK_ACTION(gtk_toggle_action_new(actionName.get(), title.utf8().data(), 0, gtkStockIDFromContextMenuAction(action)));
    else
        platformAction = gtk_action_new(actionName.get(), title.utf8().data(), 0, gtkStockIDFromContextMenuAction(action));

    m_platformDescription = GTK_MENU_ITEM(gtk_action_create_menu_item(platformAction));
    g_object_unref(platformAction);

    g_object_set_data(G_OBJECT(m_platformDescription.get()), WEBKIT_CONTEXT_MENU_ACTION, GINT_TO_POINTER(action));

    if (subMenu)
        setSubMenu(subMenu);
}

ContextMenuItem::~ContextMenuItem()
{
}

PlatformMenuItemDescription ContextMenuItem::releasePlatformDescription()
{
    return m_platformDescription.leakRef();
}

ContextMenuItemType ContextMenuItem::type() const
{
    if (GTK_IS_SEPARATOR_MENU_ITEM(m_platformDescription.get()))
        return SeparatorType;
    if (GTK_IS_CHECK_MENU_ITEM(m_platformDescription.get()))
        return CheckableActionType;
    if (gtk_menu_item_get_submenu(m_platformDescription.get()))
        return SubmenuType;
    return ActionType;
}

void ContextMenuItem::setType(ContextMenuItemType type)
{
    if (type == SeparatorType)
        m_platformDescription = GTK_MENU_ITEM(gtk_separator_menu_item_new());
}

ContextMenuAction ContextMenuItem::action() const
{
    return static_cast<ContextMenuAction>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(m_platformDescription.get()), WEBKIT_CONTEXT_MENU_ACTION)));
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
    g_object_set_data(G_OBJECT(m_platformDescription.get()), WEBKIT_CONTEXT_MENU_ACTION, GINT_TO_POINTER(action));
}

String ContextMenuItem::title() const
{
    GtkAction* action = gtk_activatable_get_related_action(GTK_ACTIVATABLE(m_platformDescription.get()));
    return action ? String(gtk_action_get_label(action)) : String();
}

void ContextMenuItem::setTitle(const String& title)
{
    GtkAction* action = gtk_activatable_get_related_action(GTK_ACTIVATABLE(m_platformDescription.get()));
    if (action)
        gtk_action_set_label(action, title.utf8().data());
}

PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{
    GtkWidget* subMenu = gtk_menu_item_get_submenu(m_platformDescription.get());
    return subMenu ? GTK_MENU(subMenu) : 0;
}

void ContextMenuItem::setSubMenu(ContextMenu* menu)
{
    gtk_menu_item_set_submenu(m_platformDescription.get(), GTK_WIDGET(menu->platformDescription()));
}

void ContextMenuItem::setChecked(bool shouldCheck)
{
    GtkAction* action = gtk_activatable_get_related_action(GTK_ACTIVATABLE(m_platformDescription.get()));
    if (action && GTK_IS_TOGGLE_ACTION(action))
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), shouldCheck);
}

void ContextMenuItem::setEnabled(bool shouldEnable)
{
    GtkAction* action = gtk_activatable_get_related_action(GTK_ACTIVATABLE(m_platformDescription.get()));
    if (action)
        gtk_action_set_sensitive(action, shouldEnable);
}

}
