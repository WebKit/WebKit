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
#include "SessionState.h"
#include "SessionStateConversion.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/BackForwardCache.h>
#include <WebCore/Frame.h>
#include <WebCore/HistoryController.h>
#include <WebCore/HistoryItem.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessID.h>

namespace WebKit {
using namespace WebCore;

// FIXME <rdar://problem/8819268>: This leaks all HistoryItems that go into these maps.
// We need to clear up the life time of these objects.

typedef HashMap<BackForwardItemIdentifier, RefPtr<HistoryItem>> IDToHistoryItemMap; // "ID" here is the item ID.
static IDToHistoryItemMap& idToHistoryItemMap()
{
    static NeverDestroyed<IDToHistoryItemMap> map;
    return map;
}

void WebBackForwardListProxy::addItemFromUIProcess(const BackForwardItemIdentifier& itemID, Ref<HistoryItem>&& item, PageIdentifier pageID, OverwriteExistingItem overwriteExistingItem)
{
    if (overwriteExistingItem == OverwriteExistingItem::No && idToHistoryItemMap().contains(itemID))
        return;

    idToHistoryItemMap().set(itemID, item.ptr());
    clearCachedListCounts();
}

static void WK2NotifyHistoryItemChanged(HistoryItem& item)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::UpdateBackForwardItem(toBackForwardListItemState(item)), 0);
}

HistoryItem* WebBackForwardListProxy::itemForID(const BackForwardItemIdentifier& itemID)
{
    return idToHistoryItemMap().get(itemID);
}

void WebBackForwardListProxy::removeItem(const BackForwardItemIdentifier& itemID)
{
    RefPtr<HistoryItem> item = idToHistoryItemMap().take(itemID);
    if (!item)
        return;
    
    BackForwardCache::singleton().remove(*item);
    WebCore::Page::clearPreviousItemFromAllPages(item.get());
}

WebBackForwardListProxy::WebBackForwardListProxy(WebPage& page)
    : m_page(&page)
{
    // FIXME: This means that if we mix legacy WebKit and modern WebKit in the same process, we won't get both notifications.
    WebCore::notifyHistoryItemChanged = WK2NotifyHistoryItemChanged;
}

void WebBackForwardListProxy::addItem(Ref<HistoryItem>&& item)
{
    if (!m_page)
        return;

    auto result = idToHistoryItemMap().add(item->identifier(), item.ptr());
    ASSERT_UNUSED(result, result.isNewEntry);

    LOG(BackForward, "(Back/Forward) WebProcess pid %i setting item %p for id %s with url %s", getCurrentProcessID(), item.ptr(), item->identifier().logString(), item->urlString().utf8().data());
    clearCachedListCounts();
    m_page->send(Messages::WebPageProxy::BackForwardAddItem(toBackForwardListItemState(item.get())));
}

void WebBackForwardListProxy::goToItem(HistoryItem& item)
{
    if (!m_page)
        return;

    auto sendResult = m_page->sendSync(Messages::WebPageProxy::BackForwardGoToItem(item.identifier()));
    auto [backForwardListCounts] = sendResult.takeReplyOr(WebBackForwardListCounts { });
    m_cachedBackForwardListCounts = backForwardListCounts;
}

RefPtr<HistoryItem> WebBackForwardListProxy::itemAtIndex(int itemIndex)
{
    if (!m_page)
        return nullptr;

    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::BackForwardItemAtIndex(itemIndex), m_page->identifier());
    auto [itemID] = sendResult.takeReplyOr(std::nullopt);
    if (!itemID)
        return nullptr;

    return idToHistoryItemMap().get(*itemID);
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
    // Items are removed asynchronously from idToHistoryItemMap() via IPC from the UIProcess so we need to ask
    // the UIProcess to make sure this HistoryItem is still part of the back/forward list.
    auto sendResult = m_page->sendSync(Messages::WebPageProxy::BackForwardListContainsItem(item.identifier()), m_page->identifier());
    auto [contains] = sendResult.takeReplyOr(false);
    return contains;
}

const WebBackForwardListCounts& WebBackForwardListProxy::cacheListCountsIfNecessary() const
{
    if (!m_cachedBackForwardListCounts) {
        WebBackForwardListCounts backForwardListCounts;
        if (m_page) {
            auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::BackForwardListCounts(), m_page->identifier());
            if (sendResult)
                std::tie(backForwardListCounts) = sendResult.takeReply();
        }
        m_cachedBackForwardListCounts = backForwardListCounts;
    }
    return *m_cachedBackForwardListCounts;
}

void WebBackForwardListProxy::clearCachedListCounts()
{
    m_cachedBackForwardListCounts = std::nullopt;
}

void WebBackForwardListProxy::close()
{
    ASSERT(m_page);
    m_page = nullptr;
    m_cachedBackForwardListCounts = WebBackForwardListCounts { };
}

void WebBackForwardListProxy::clear()
{
    m_cachedBackForwardListCounts = WebBackForwardListCounts { }; // Clearing the back/forward list will cause the counts to become 0.
    m_page->send(Messages::WebPageProxy::BackForwardClear());
}

} // namespace WebKit
