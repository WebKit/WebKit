/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#if ENABLE(CONTEXT_MENUS)

#include "APIContextMenuClient.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuItemData.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {
using namespace WebCore;

static HMENU createMenu(const ContextMenuContextData &context)
{
    HMENU menu = ::CreatePopupMenu();
    MENUINFO menuInfo;
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIM_STYLE;
    menuInfo.dwStyle = MNS_NOTIFYBYPOS;
    menuInfo.dwMenuData = (ULONG_PTR)&context;
    ::SetMenuInfo(menu, &menuInfo);
    return menu;
}

static void populate(const ContextMenuContextData &, HMENU, const Vector<WebContextMenuItemData>&);

static void createMenuItem(const ContextMenuContextData &context, HMENU menu, const WebContextMenuItemData &data)
{
    UINT flags = 0;

    flags |= data.enabled() ? MF_ENABLED : MF_DISABLED;
    flags |= data.checked() ? MF_CHECKED : MF_UNCHECKED;

    switch (data.type()) {
    case ActionType:
    case CheckableActionType:
        ::AppendMenu(menu, flags | MF_STRING, data.action(), data.title().wideCharacters().data());
        break;
    case SeparatorType:
        ::AppendMenu(menu, flags | MF_SEPARATOR, data.action(), nullptr);
        break;
    case SubmenuType:
        HMENU submenu = createMenu(context);
        populate(context, submenu, data.submenu());
        ::AppendMenu(menu, flags | MF_POPUP, (UINT_PTR)submenu, data.title().wideCharacters().data());
        break;
    }
}

static void populate(const ContextMenuContextData &context, HMENU menu, const Vector<WebContextMenuItemData>& items)
{
    for (auto& data : items)
        createMenuItem(context, menu, data);
}

static void populate(const ContextMenuContextData &context, HMENU menu, const Vector<Ref<WebContextMenuItem>>& items)
{
    for (auto& item : items) {
        auto data = item->data();
        createMenuItem(context, menu, data);
    }
}

void WebContextMenuProxyWin::showContextMenuWithItems(Vector<Ref<WebContextMenuItem>>&& items)
{
    populate(m_context, m_menu, items);

    UINT flags = TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_VERPOSANIMATION | TPM_HORIZONTAL | TPM_LEFTALIGN | TPM_HORPOSANIMATION;
    POINT pt { m_context.menuLocation().x(), m_context.menuLocation().y() };
    HWND wnd = page()->viewWidget();
    ::ClientToScreen(wnd, &pt);
    ::TrackPopupMenuEx(m_menu, flags, pt.x, pt.y, page()->viewWidget(), nullptr);
}

WebContextMenuProxyWin::WebContextMenuProxyWin(WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
    : WebContextMenuProxy(page, WTFMove(context), userData)
{
    m_menu = createMenu(m_context);
}

WebContextMenuProxyWin::~WebContextMenuProxyWin()
{
    if (m_menu)
        ::DestroyMenu(m_menu);
}

} // namespace WebKit
#endif // ENABLE(CONTEXT_MENUS)
