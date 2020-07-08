/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#include "WebContextMenuProxy.h"

#if ENABLE(CONTEXT_MENUS)

#include "APIContextMenuClient.h"
#include "WebPageMessages.h"

namespace WebKit {

WebContextMenuProxy::WebContextMenuProxy(WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
    : m_context(WTFMove(context))
    , m_userData(userData)
    , m_page(makeWeakPtr(page))
{
}

WebContextMenuProxy::~WebContextMenuProxy() = default;

Vector<Ref<WebContextMenuItem>> WebContextMenuProxy::proposedItems() const
{
    return WTF::map(m_context.menuItems(), [](auto& item) {
        return WebContextMenuItem::create(item);
    });
}

void WebContextMenuProxy::show()
{
    m_contextMenuListener = WebContextMenuListenerProxy::create(*this);
    page()->contextMenuClient().getContextMenuFromProposedMenu(*page(), proposedItems(), *m_contextMenuListener, m_context.webHitTestResultData(), page()->process().transformHandlesToObjects(m_userData.object()).get());
}

void WebContextMenuProxy::useContextMenuItems(Vector<Ref<WebContextMenuItem>>&& items)
{
    m_contextMenuListener = nullptr;

    auto page = makeRefPtr(this->page());
    if (!page)
        return;

    // Since showContextMenuWithItems can spin a nested run loop we need to turn off the responsiveness timer.
    page->process().stopResponsivenessTimer();

    // Protect |this| from being deallocated if WebPageProxy code is re-entered from the menu runloop or delegates.
    auto protectedThis = makeRef(*this);
    showContextMenuWithItems(WTFMove(items));

    // No matter the result of showContextMenuWithItems, always notify the WebProcess that the menu is hidden so it starts handling mouse events again.
    page->send(Messages::WebPage::ContextMenuHidden());
}

} // namespace WebKit

#endif
