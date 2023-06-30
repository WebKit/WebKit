/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WebBackForwardCache.h"

#include "Logging.h"
#include "SuspendedPageProxy.h"
#include "WebBackForwardCacheEntry.h"
#include "WebBackForwardListItem.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"

namespace WebKit {

WebBackForwardCache::WebBackForwardCache(WebProcessPool& processPool)
    : m_processPool(processPool)
{
}

WebBackForwardCache::~WebBackForwardCache()
{
    clear();
}

inline void WebBackForwardCache::removeOldestEntry()
{
    ASSERT(!m_itemsWithCachedPage.isEmpty());
    removeEntry(*m_itemsWithCachedPage.first());
}

void WebBackForwardCache::setCapacity(unsigned capacity)
{
    if (m_capacity == capacity)
        return;

    m_capacity = capacity;
    while (size() > capacity)
        removeOldestEntry();

    m_processPool.sendToAllProcesses(Messages::WebProcess::SetBackForwardCacheCapacity(m_capacity));
}

void WebBackForwardCache::addEntry(WebBackForwardListItem& item, std::unique_ptr<WebBackForwardCacheEntry>&& backForwardCacheEntry)
{
    ASSERT(capacity());
    ASSERT(backForwardCacheEntry);

    if (item.backForwardCacheEntry()) {
        ASSERT(m_itemsWithCachedPage.contains(&item));
        m_itemsWithCachedPage.removeFirst(&item);
    }

    item.setBackForwardCacheEntry(WTFMove(backForwardCacheEntry));
    m_itemsWithCachedPage.append(&item);

    if (size() > capacity())
        removeOldestEntry();
    ASSERT(size() <= capacity());

    RELEASE_LOG(BackForwardCache, "WebBackForwardCache::addEntry: item=%s, hasSuspendedPage=%d, size=%u/%u", item.itemID().toString().utf8().data(), !!item.suspendedPage(), size(), capacity());
}

void WebBackForwardCache::addEntry(WebBackForwardListItem& item, std::unique_ptr<SuspendedPageProxy>&& suspendedPage)
{
    addEntry(item, makeUnique<WebBackForwardCacheEntry>(*this, item.itemID(), suspendedPage->process().coreProcessIdentifier(), WTFMove(suspendedPage)));
}

void WebBackForwardCache::addEntry(WebBackForwardListItem& item, WebCore::ProcessIdentifier processIdentifier)
{
    addEntry(item, makeUnique<WebBackForwardCacheEntry>(*this, item.itemID(), WTFMove(processIdentifier)));
}

void WebBackForwardCache::removeEntry(WebBackForwardListItem& item)
{
    ASSERT(m_itemsWithCachedPage.contains(&item));
    m_itemsWithCachedPage.removeFirst(&item);
    item.setBackForwardCacheEntry(nullptr);
    RELEASE_LOG(BackForwardCache, "WebBackForwardCache::removeEntry: item=%s, size=%u/%u", item.itemID().toString().utf8().data(), size(), capacity());
}

void WebBackForwardCache::removeEntry(SuspendedPageProxy& suspendedPage)
{
    removeEntriesMatching([&suspendedPage](auto& item) {
        return item.suspendedPage() == &suspendedPage;
    });
}

std::unique_ptr<SuspendedPageProxy> WebBackForwardCache::takeSuspendedPage(WebBackForwardListItem& item)
{
    RELEASE_LOG(BackForwardCache, "WebBackForwardCache::takeSuspendedPage: item=%s", item.itemID().toString().utf8().data());

    ASSERT(m_itemsWithCachedPage.contains(&item));
    ASSERT(item.backForwardCacheEntry());
    auto suspendedPage = item.backForwardCacheEntry()->takeSuspendedPage();
    ASSERT(suspendedPage);
    removeEntry(item);
    return suspendedPage;
}

void WebBackForwardCache::removeEntriesForProcess(WebProcessProxy& process)
{
    removeEntriesMatching([processIdentifier = process.coreProcessIdentifier()](auto& entry) {
        ASSERT(entry.backForwardCacheEntry());
        return entry.backForwardCacheEntry()->processIdentifier() == processIdentifier;
    });
}

void WebBackForwardCache::removeEntriesForSession(PAL::SessionID sessionID)
{
    removeEntriesMatching([sessionID](auto& item) {
        auto* dataStore = item.backForwardCacheEntry()->process()->websiteDataStore();
        return dataStore && dataStore->sessionID() == sessionID;
    });
}

void WebBackForwardCache::removeEntriesForPage(WebPageProxy& page)
{
    removeEntriesMatching([pageID = page.identifier()](auto& item) {
        return item.pageID() == pageID;
    });
}

void WebBackForwardCache::removeEntriesForPageAndProcess(WebPageProxy& page, WebProcessProxy& process)
{
    removeEntriesMatching([pageID = page.identifier(), processIdentifier = process.coreProcessIdentifier()](auto& item) {
        ASSERT(item.backForwardCacheEntry());
        return item.pageID() == pageID && item.backForwardCacheEntry()->processIdentifier() == processIdentifier;
    });
}

void WebBackForwardCache::removeEntriesMatching(const Function<bool(WebBackForwardListItem&)>& matches)
{
    Vector<Ref<WebBackForwardListItem>> itemsWithEntriesToClear;
    m_itemsWithCachedPage.removeAllMatching([&](auto* item) {
        if (matches(*item)) {
            itemsWithEntriesToClear.append(*item);
            return true;
        }
        return false;
    });

    for (auto& item : itemsWithEntriesToClear)
        item->setBackForwardCacheEntry(nullptr);
}

void WebBackForwardCache::clear()
{
    RELEASE_LOG(BackForwardCache, "WebBackForwardCache::clear");
    auto itemsWithCachedPage = WTFMove(m_itemsWithCachedPage);
    for (auto* item : itemsWithCachedPage)
        item->setBackForwardCacheEntry(nullptr);
}

void WebBackForwardCache::pruneToSize(unsigned newSize)
{
    RELEASE_LOG(BackForwardCache, "WebBackForwardCache::pruneToSize(%u)", newSize);
    while (size() > newSize)
        removeOldestEntry();
}

} // namespace WebKit.
