/*
 * Copyright (C) 2017 Igalia S.L.
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
 */

#include "config.h"
#include "WebKitPopupMenu.h"

#include "NativeWebMouseEvent.h"
#include "WebKitOptionMenuPrivate.h"
#include "WebKitWebViewPrivate.h"

namespace WebKit {
using namespace WebCore;

WebKitPopupMenu::WebKitPopupMenu(GtkWidget* webView, WebPopupMenuProxy::Client& client)
    : WebPopupMenuProxyGtk(webView, client)
{
}

static void menuCloseCallback(WebKitPopupMenu* popupMenu)
{
    popupMenu->activateItem(WTF::nullopt);
}

void WebKitPopupMenu::showPopupMenu(const IntRect& rect, TextDirection direction, double pageScaleFactor, const Vector<WebPopupItem>& items, const PlatformPopupMenuData& platformData, int32_t selectedIndex)
{
    GRefPtr<WebKitOptionMenu> menu = adoptGRef(webkitOptionMenuCreate(*this, items, selectedIndex));
    const GdkEvent* event = m_client->currentlyProcessedMouseDownEvent() ? m_client->currentlyProcessedMouseDownEvent()->nativeEvent() : nullptr;
    if (webkitWebViewShowOptionMenu(WEBKIT_WEB_VIEW(m_webView), rect, menu.get(), event)) {
        m_menu = WTFMove(menu);
        g_signal_connect_swapped(m_menu.get(), "close", G_CALLBACK(menuCloseCallback), this);
    } else
        WebPopupMenuProxyGtk::showPopupMenu(rect, direction, pageScaleFactor, items, platformData, selectedIndex);
}

void WebKitPopupMenu::hidePopupMenu()
{
    if (!m_menu) {
        WebPopupMenuProxyGtk::hidePopupMenu();
        return;
    }
    g_signal_handlers_disconnect_matched(m_menu.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    webkit_option_menu_close(m_menu.get());
}

void WebKitPopupMenu::cancelTracking()
{
    if (!m_menu) {
        WebPopupMenuProxyGtk::cancelTracking();
        return;
    }
    hidePopupMenu();
    m_menu = nullptr;
}

void WebKitPopupMenu::activateItem(Optional<unsigned> itemIndex)
{
    WebPopupMenuProxyGtk::activateItem(itemIndex);
    if (m_menu) {
        g_signal_handlers_disconnect_matched(m_menu.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        m_menu = nullptr;
    }
}

} // namespace WebKit
