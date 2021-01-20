/*
 * Copyright (C) 2020 Igalia S.L.
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

#include "APIViewClient.h"
#include "WPEView.h"
#include "WebKitOptionMenuPrivate.h"
#include "WebKitWebViewClient.h"

namespace WebKit {
using namespace WebCore;

Ref<WebKitPopupMenu> WebKitPopupMenu::create(WKWPE::View& view, WebPopupMenuProxy::Client& client)
{
    ASSERT(view.client().isGLibBasedAPI());
    return adoptRef(*new WebKitPopupMenu(view, client));
}

WebKitPopupMenu::WebKitPopupMenu(WKWPE::View& view, WebPopupMenuProxy::Client& client)
    : WebPopupMenuProxy(client)
    , m_view(view)
{
}

static void menuCloseCallback(WebKitPopupMenu* popupMenu)
{
    popupMenu->activateItem(WTF::nullopt);
}

void WebKitPopupMenu::showPopupMenu(const IntRect& rect, TextDirection direction, double pageScaleFactor, const Vector<WebPopupItem>& items, const PlatformPopupMenuData& platformData, int32_t selectedIndex)
{
    GRefPtr<WebKitOptionMenu> menu = static_cast<WebKitWebViewClient&>(m_view.client()).showOptionMenu(*this, rect, items, selectedIndex);
    if (menu) {
        m_menu = WTFMove(menu);
        g_signal_connect_swapped(m_menu.get(), "close", G_CALLBACK(menuCloseCallback), this);
    }
}

void WebKitPopupMenu::hidePopupMenu()
{
    if (m_menu) {
        g_signal_handlers_disconnect_matched(m_menu.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        webkit_option_menu_close(m_menu.get());
    }
}

void WebKitPopupMenu::cancelTracking()
{
    hidePopupMenu();
    m_menu = nullptr;
}

void WebKitPopupMenu::selectItem(unsigned itemIndex)
{
    if (m_client)
        m_client->setTextFromItemForPopupMenu(this, itemIndex);
    m_selectedItem = itemIndex;
}

void WebKitPopupMenu::activateItem(Optional<unsigned> itemIndex)
{
    if (m_client)
        m_client->valueChangedForPopupMenu(this, itemIndex.valueOr(m_selectedItem.valueOr(-1)));
    if (m_menu) {
        g_signal_handlers_disconnect_matched(m_menu.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        m_menu = nullptr;
    }
}

} // namespace WebKit
