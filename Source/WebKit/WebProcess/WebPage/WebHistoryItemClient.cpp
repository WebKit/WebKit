/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#include "WebHistoryItemClient.h"

#include "MessageSenderInlines.h"
#include "SessionState.h"
#include "SessionStateConversion.h"
#include "WebBackForwardListMessages.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/HistoryItem.h>

namespace WebKit {

WebHistoryItemClient::WebHistoryItemClient(WebPage& page)
    : m_page(page)
{
}

ScopeExit<CompletionHandler<void()>> WebHistoryItemClient::ignoreChangesForScope()
{
    m_shouldIgnoreChanges = true;
    return makeScopeExit(CompletionHandler<void()> { [this, protectedThis = Ref { *this }] {
        m_shouldIgnoreChanges = false;
    } });
}

void WebHistoryItemClient::historyItemChanged(const WebCore::HistoryItem& item)
{
    if (m_shouldIgnoreChanges)
        return;
    if (RefPtr page = m_page.get())
        page->send(Messages::WebBackForwardList::BackForwardUpdateItem(toFrameState(item)));
}

void WebHistoryItemClient::clearChildren(const WebCore::HistoryItem& item) const
{
    if (m_shouldIgnoreChanges)
        return;
    if (RefPtr page = m_page.get())
        page->send(Messages::WebBackForwardList::BackForwardClearChildren(item.itemID(), item.frameItemID()));
}

}
