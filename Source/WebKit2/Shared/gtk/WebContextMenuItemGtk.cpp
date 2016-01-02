/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "WebContextMenuItemGtk.h"

#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

using namespace WebCore;

namespace WebKit {

static const char* gtkStockIDFromContextMenuAction(ContextMenuAction action)
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
    case ContextMenuItemTagCopyImageUrlToClipboard:
        // No icon for this.
        return 0;
    case ContextMenuItemTagEnterVideoFullscreen:
        return GTK_STOCK_FULLSCREEN;
    default:
        return 0;
    }
}

WebContextMenuItemGtk::WebContextMenuItemGtk(ContextMenuItemType type, ContextMenuAction action, const String& title, bool enabled, bool checked)
    : WebContextMenuItemData(type, action, title, enabled, checked)
{
    ASSERT(type != SubmenuType);
    createGtkActionIfNeeded();
}

WebContextMenuItemGtk::WebContextMenuItemGtk(const WebContextMenuItemData& data)
    : WebContextMenuItemData(data.type() == SubmenuType ? ActionType : data.type(), data.action(), data.title(), data.enabled(), data.checked())
{
    createGtkActionIfNeeded();
}

WebContextMenuItemGtk::WebContextMenuItemGtk(const WebContextMenuItemGtk& data, Vector<WebContextMenuItemGtk>&& submenu)
    : WebContextMenuItemData(ActionType, data.action(), data.title(), data.enabled(), false)
{
    m_action = data.gtkAction();
    m_submenuItems = WTFMove(submenu);
}

WebContextMenuItemGtk::WebContextMenuItemGtk(GtkAction* action)
    : WebContextMenuItemData(GTK_IS_TOGGLE_ACTION(action) ? CheckableActionType : ActionType, ContextMenuItemBaseApplicationTag, String::fromUTF8(gtk_action_get_label(action)), gtk_action_get_sensitive(action), GTK_IS_TOGGLE_ACTION(action) ? gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action)) : false)
{
    m_action = action;
}

WebContextMenuItemGtk::~WebContextMenuItemGtk()
{
}

void WebContextMenuItemGtk::createGtkActionIfNeeded()
{
    if (type() == SeparatorType)
        return;

    GUniquePtr<char> actionName(g_strdup_printf("context-menu-action-%d", action()));
    if (type() == CheckableActionType) {
        m_action = adoptGRef(GTK_ACTION(gtk_toggle_action_new(actionName.get(), title().utf8().data(), nullptr, gtkStockIDFromContextMenuAction(action()))));
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(m_action.get()), checked());
    } else
        m_action = adoptGRef(gtk_action_new(actionName.get(), title().utf8().data(), 0, gtkStockIDFromContextMenuAction(action())));
    gtk_action_set_sensitive(m_action.get(), enabled());
}

} // namespace WebKit
