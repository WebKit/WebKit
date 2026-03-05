/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "WebBackForwardListProxy.h"

#include "Logging.h"
#include "MessageSenderInlines.h"
#include "SessionState.h"
#include "SessionStateConversion.h"
#include "WebBackForwardListMessages.h"
#include "WebHistoryItemClient.h"
#include "WebPage.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/BackForwardCache.h>
#include <WebCore/HistoryController.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/Page.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessID.h>

namespace WebKit {
using namespace WebCore;

void WebBackForwardListProxy::removeItem(BackForwardFrameItemIdentifier frameItemID)
{
    WebCore::Page::clearPreviousItemFromAllPages(frameItemID);
}

WebBackForwardListProxy::WebBackForwardListProxy(WebPage& page)
    : m_page(&page)
{
}

void WebBackForwardListProxy::addItem(Ref<HistoryItem>&& item)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    LOG(BackForward, "(Back/Forward) WebProcess pid %i setting item %p for id %s with url %s", getCurrentProcessID(), item.ptr(), item->itemID().toString().utf8().data(), item->urlString().utf8().data());
    m_cachedBackForwardListCounts = std::nullopt;
    page->send(Messages::WebBackForwardList::BackForwardAddItem(toFrameState(item.get())));
}

void WebBackForwardListProxy::setChildItem(BackForwardFrameItemIdentifier frameItemID, Ref<HistoryItem>&& item)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebBackForwardList::BackForwardSetChildItem(frameItemID, toFrameState(item)));
}

void WebBackForwardListProxy::goToItem(HistoryItem& item)
{
    if (!m_page)
        return;

    auto sendResult = m_page->sendSync(Messages::WebBackForwardList::BackForwardGoToItem(item.itemID()));
    auto [backForwardListCounts] = sendResult.takeReplyOr(WebBackForwardListCounts { });
    m_cachedBackForwardListCounts = backForwardListCounts;
}

Vector<Ref<HistoryItem>> WebBackForwardListProxy::allItems(FrameIdentifier frameID)
{
    RefPtr page = m_page.get();
    if (!page)
        return { };

    auto sendResult = m_page->sendSync(Messages::WebBackForwardList::BackForwardAllItems(frameID));
    auto [allFrameStates] = sendResult.takeReplyOr(Vector<Ref<FrameState>> { });

    Ref historyItemClient = page->historyItemClient();
    auto ignoreHistoryItemChangesForScope = historyItemClient->ignoreChangesForScope();

    Vector<Ref<HistoryItem>> allItems;
    for (Ref frameState : allFrameStates)
        allItems.append(toHistoryItem(historyItemClient, WTF::move(frameState)));

    return allItems;
}

RefPtr<HistoryItem> WebBackForwardListProxy::itemAtIndex(int itemIndex, FrameIdentifier frameID)
{
    RefPtr page = m_page.get();
    if (!page)
        return nullptr;

    auto sendResult = page->sendSync(Messages::WebBackForwardList::BackForwardItemAtIndex(itemIndex, frameID));
    auto [frameState] = sendResult.takeReplyOr(nullptr);
    if (!frameState)
        return nullptr;

    Ref historyItemClient = page->historyItemClient();
    auto ignoreHistoryItemChangesForScope = historyItemClient->ignoreChangesForScope();
    return toHistoryItem(historyItemClient, Ref { *frameState });
}

unsigned WebBackForwardListProxy::backListCount() const
{
    return cacheListCountsIfNecessary().backCount;
}

unsigned WebBackForwardListProxy::forwardListCount() const
{
    return cacheListCountsIfNecessary().forwardCount;
}

bool WebBackForwardListProxy::containsItem(const WebCore::HistoryItem& item) const
{
    auto sendResult = m_page->sendSync(Messages::WebBackForwardList::BackForwardListContainsItem(item.itemID()), m_page->identifier());
    auto [contains] = sendResult.takeReplyOr(false);
    return contains;
}

const WebBackForwardListCounts& WebBackForwardListProxy::cacheListCountsIfNecessary() const
{
    if (!m_cachedBackForwardListCounts) {
        WebBackForwardListCounts backForwardListCounts;
        if (m_page) {
            auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebBackForwardList::BackForwardListCounts(), m_page->identifier());
            if (sendResult.succeeded())
                std::tie(backForwardListCounts) = sendResult.takeReply();
        }
        m_cachedBackForwardListCounts = backForwardListCounts;
    }
    return *m_cachedBackForwardListCounts;
}

void WebBackForwardListProxy::clearCachedListCounts()
{
    m_cachedBackForwardListCounts = WebBackForwardListCounts { };
}

void WebBackForwardListProxy::close()
{
    ASSERT(m_page);
    m_page = nullptr;
    m_cachedBackForwardListCounts = WebBackForwardListCounts { };
}

} // namespace WebKit
