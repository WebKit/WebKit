/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebContextMenuProxyWin.h"

using namespace WebCore;

namespace WebKit {

WebContextMenuProxyWin::WebContextMenuProxyWin(HWND parentWindow, WebPageProxy* page)
    : m_window(parentWindow)
    , m_page(page)
    , m_menu(0)
{
}

void WebContextMenuProxyWin::populateMenu(HMENU menu, const Vector<WebContextMenuItemData>& items)
{
    for (size_t i = 0; i < items.size(); ++i) {
        const WebContextMenuItemData& itemData = items[i];
        switch (itemData.type()) {
        case ActionType:
        case CheckableActionType: {
            UINT flags = itemData.enabled() ? MF_ENABLED : MF_DISABLED;
            if (itemData.checked())
                flags |= MF_CHECKED;
            String title = itemData.title();
            ::AppendMenu(menu, flags, itemData.action(), title.charactersWithNullTermination());

            m_actionMap.add(itemData.action(), itemData);
            break;
        }
        case SeparatorType:
            ::AppendMenu(menu, MF_SEPARATOR, 0, 0);
            break;
        case SubmenuType: {
            HMENU subMenu = ::CreatePopupMenu();
            populateMenu(subMenu, itemData.submenu());
            String title = itemData.title();
            ::AppendMenu(menu, MF_POPUP, reinterpret_cast<UINT>(subMenu), title.charactersWithNullTermination());
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

void WebContextMenuProxyWin::showContextMenu(const IntPoint& origin, const Vector<WebContextMenuItemData>& items)
{
    if (items.isEmpty())
        return;

    // Hide any context menu we have showing (this also destroys the menu).
    hideContextMenu();

    m_menu = ::CreatePopupMenu();
    populateMenu(m_menu, items);

    POINT point = POINT(origin);
    if (!::ClientToScreen(m_window, &point))
        return;

    UINT flags = TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_VERPOSANIMATION | TPM_HORIZONTAL | TPM_LEFTALIGN | TPM_HORPOSANIMATION | TPM_RETURNCMD | TPM_NONOTIFY;
    int selectedCommand = ::TrackPopupMenuEx(m_menu, flags, point.x, point.y, m_window, 0);
    if (!selectedCommand)
        return;

    m_page->contextMenuItemSelected(m_actionMap.get(selectedCommand));
}

void WebContextMenuProxyWin::hideContextMenu()
{
    if (m_menu) {
        ::DestroyMenu(m_menu);
        m_menu = 0;
    }

    m_actionMap.clear();
}

} // namespace WebKit
