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
#include <WebCore/Frame.h>
#include <WebCore/HistoryController.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/PageCache.h>
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

void WebBackForwardListProxy::addItemFromUIProcess(const BackForwardItemIdentifier& itemID, Ref<HistoryItem>&& item, uint64_t pageID, OverwriteExistingItem overwriteExistingItem)
{
    // This item/itemID pair should not already exist in our map.
    ASSERT_UNUSED(overwriteExistingItem, overwriteExistingItem == OverwriteExistingItem::Yes || !idToHistoryItemMap().contains(itemID));
    idToHistoryItemMap().set(itemID, item.ptr());
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
        
    PageCache::singleton().remove(*item);
    WebCore::Page::clearPreviousItemFromAllPages(item.get());
}

WebBackForwardListProxy::WebBackForwardListProxy(WebPage& page)
    : m_page(&page)
{
    WebCore::notifyHistoryItemChanged = WK2NotifyHistoryItemChanged;
}

void WebBackForwardListProxy::addItem(Ref<HistoryItem>&& item)
{
    if (!m_page)
        return;

    auto result = idToHistoryItemMap().add(item->identifier(), item.ptr());
    ASSERT_UNUSED(result, result.isNewEntry);

    LOG(BackForward, "(Back/Forward) WebProcess pid %i setting item %p for id %s with url %s", getCurrentProcessID(), item.ptr(), item->identifier().logString(), item->urlString().utf8().data());
    m_page->send(Messages::WebPageProxy::BackForwardAddItem(toBackForwardListItemState(item.get())));
}

void WebBackForwardListProxy::goToItem(HistoryItem& item)
{
    if (!m_page)
        return;

    SandboxExtension::Handle sandboxExtensionHandle;
    m_page->sendSync(Messages::WebPageProxy::BackForwardGoToItem(item.identifier()), Messages::WebPageProxy::BackForwardGoToItem::Reply(sandboxExtensionHandle));
    m_page->sandboxExtensionTracker().beginLoad(m_page->mainWebFrame(), WTFMove(sandboxExtensionHandle));
}

RefPtr<HistoryItem> WebBackForwardListProxy::itemAtIndex(int itemIndex)
{
    if (!m_page)
        return nullptr;

    Optional<BackForwardItemIdentifier> itemID;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::BackForwardItemAtIndex(itemIndex), Messages::WebPageProxy::BackForwardItemAtIndex::Reply(itemID), m_page->pageID()))
        return nullptr;

    if (!itemID)
        return nullptr;

    return idToHistoryItemMap().get(*itemID);
}

unsigned WebBackForwardListProxy::backListCount() const
{
    if (!m_page)
        return 0;

    unsigned backListCount = 0;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::BackForwardBackListCount(), Messages::WebPageProxy::BackForwardBackListCount::Reply(backListCount), m_page->pageID()))
        return 0;

    return backListCount;
}

unsigned WebBackForwardListProxy::forwardListCount() const
{
    if (!m_page)
        return 0;

    unsigned forwardListCount = 0;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::BackForwardForwardListCount(), Messages::WebPageProxy::BackForwardForwardListCount::Reply(forwardListCount), m_page->pageID()))
        return 0;

    return forwardListCount;
}

void WebBackForwardListProxy::close()
{
    ASSERT(m_page);
    m_page = nullptr;
}

void WebBackForwardListProxy::clear()
{
    m_page->send(Messages::WebPageProxy::BackForwardClear());
}

} // namespace WebKit
