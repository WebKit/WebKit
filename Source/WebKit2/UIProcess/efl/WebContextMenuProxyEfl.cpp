/*
 * Copyright (C) 2012 Samsung Electronics.
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
#include "WebContextMenuProxyEfl.h"

#if ENABLE(CONTEXT_MENUS)

#include "APIContextMenuClient.h"
#include "EwkView.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuItemData.h"
#include "WebPageProxy.h"
#include <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {

WebContextMenuProxyEfl::WebContextMenuProxyEfl(EwkView* ewkView, WebPageProxy& page, const ContextMenuContextData& context, const UserData& userData)
    : WebContextMenuProxy(context, userData)
    , m_ewkView(ewkView)
    , m_page(page)
{
}

WebContextMenuProxyEfl::~WebContextMenuProxyEfl()
{
}

void WebContextMenuProxyEfl::show()
{
    showContextMenu();
}

void WebContextMenuProxyEfl::showContextMenu()
{
    Vector<RefPtr<WebContextMenuItem>> proposedAPIItems;
    for (auto& item : m_context.menuItems()) {
        if (item.action() != ContextMenuItemTagShareMenu)
            proposedAPIItems.append(WebContextMenuItem::create(item));
    }

    Vector<RefPtr<WebContextMenuItem>> clientItems;
    bool useProposedItems = true;

    if (m_page.contextMenuClient().getContextMenuFromProposedMenu(m_page, proposedAPIItems, clientItems, m_context.webHitTestResultData(), m_page.process().transformHandlesToObjects(m_userData.object()).get()))
        useProposedItems = false;

    const Vector<RefPtr<WebContextMenuItem>>& items = useProposedItems ? proposedAPIItems : clientItems;

    if (items.isEmpty())
        return;

    Vector<RefPtr<API::Object>> menuItems;
    menuItems.reserveInitialCapacity(items.size());

    for (const auto& menuItem : items)
        menuItems.uncheckedAppend(menuItem);

    if (m_ewkView)
        m_ewkView->showContextMenu(toAPI(m_context.menuLocation()), toAPI(API::Array::create(WTFMove(menuItems)).ptr()));
}

void WebContextMenuProxyEfl::hideContextMenu()
{
    notImplemented();
}

void WebContextMenuProxyEfl::cancelTracking()
{
    notImplemented();
}


} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
