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

#include "SuspendedPageProxy.h"
#include "WebBackForwardListItem.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"

namespace WebKit {

WebBackForwardCache::WebBackForwardCache() = default;

WebBackForwardCache::~WebBackForwardCache() = default;

inline void WebBackForwardCache::removeOldestEntry()
{
    ASSERT(!m_itemsWithCachedPage.isEmpty());
    removeEntry(*m_itemsWithCachedPage.first());
}

void WebBackForwardCache::setCapacity(unsigned capacity)
{
    m_capacity = capacity;
    while (size() > capacity)
        removeOldestEntry();
}

void WebBackForwardCache::addEntry(WebBackForwardListItem& item, std::unique_ptr<SuspendedPageProxy>&& suspendedPage)
{
    ASSERT(capacity());

    item.setSuspendedPage(WTFMove(suspendedPage));
    m_itemsWithCachedPage.add(&item);

    if (size() > capacity())
        removeOldestEntry();
    ASSERT(size() <= capacity());
}

void WebBackForwardCache::removeEntry(WebBackForwardListItem& item)
{
    ASSERT(m_itemsWithCachedPage.contains(&item));
    m_itemsWithCachedPage.remove(&item);
    item.setSuspendedPage(nullptr);
}

std::unique_ptr<SuspendedPageProxy> WebBackForwardCache::takeEntry(WebBackForwardListItem& item)
{
    ASSERT(m_itemsWithCachedPage.contains(&item));
    m_itemsWithCachedPage.remove(&item);
    return item.takeSuspendedPage();
}

void WebBackForwardCache::removeEntriesForProcess(WebProcessProxy& process)
{
    removeEntriesMatching([&process](auto& item) {
        return &item.suspendedPage()->process() == &process;
    });
}

void WebBackForwardCache::removeEntriesForSession(PAL::SessionID sessionID)
{
    removeEntriesMatching([sessionID](auto& item) {
        return item.suspendedPage()->process().websiteDataStore().sessionID() == sessionID;
    });
}

void WebBackForwardCache::removeEntriesForPage(WebPageProxy& page)
{
    removeEntriesMatching([pageID = page.identifier()](auto& item) {
        return item.pageID() == pageID;
    });
}

void WebBackForwardCache::removeEntriesMatching(const Function<bool(WebBackForwardListItem&)>& matches)
{
    for (auto it = m_itemsWithCachedPage.begin(); it != m_itemsWithCachedPage.end();) {
        auto current = it;
        ++it;
        if (matches(**current)) {
            (*current)->setSuspendedPage(nullptr);
            m_itemsWithCachedPage.remove(current);
        }
    }
}

void WebBackForwardCache::clear()
{
    auto itemsWithCachedPage = WTFMove(m_itemsWithCachedPage);
    for (auto* item : itemsWithCachedPage)
        item->setSuspendedPage(nullptr);
}

} // namespace WebKit.
