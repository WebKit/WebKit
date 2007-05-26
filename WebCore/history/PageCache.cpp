/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PageCache.h"

#include "CachedPage.h"
#include "FrameLoader.h"
#include "HistoryItem.h"
#include "Logging.h"
#include "SystemTime.h"

using namespace std;

namespace WebCore {

static const double autoreleaseInterval = 3;

PageCache* pageCache()
{
    static PageCache* staticPageCache = new PageCache;
    return staticPageCache;
}

PageCache::PageCache()
    : m_capacity(0)
    , m_autoreleaseTimer(this, &PageCache::releaseAutoreleasedPagesNowOrReschedule)
{
}

void PageCache::setCapacity(int capacity)
{
    ASSERT(capacity >= 0);
    m_capacity = max(capacity, 0);

    prune();
}

void PageCache::add(PassRefPtr<HistoryItem> historyItem, PassRefPtr<CachedPage> cachedPage)
{
    historyItem->setInPageCache(true);
    m_LRUList.add(historyItem.get());
    m_cachedPages.set(historyItem, cachedPage);

    prune();
}

void PageCache::remove(HistoryItem* historyItem)
{
    if (!historyItem->isInPageCache()) {
        ASSERT(!m_cachedPages.contains(historyItem));
        return;
    }

    historyItem->setInPageCache(false);
    m_LRUList.remove(historyItem);
    CachedPageMap::iterator it = m_cachedPages.find(historyItem);
    ASSERT(it != m_cachedPages.end());
    if (it != m_cachedPages.end()) {
        autorelease(it->second.release());
        m_cachedPages.remove(it);
    }
}

CachedPage* PageCache::get(HistoryItem* historyItem)
{
    return m_cachedPages.get(historyItem).get();
}

void PageCache::prune()
{
    while (m_cachedPages.size() > m_capacity)
        remove(*m_LRUList.begin());
}

void PageCache::releaseAutoreleasedPagesNowOrReschedule(Timer<PageCache>* timer)
{
    double loadDelta = currentTime() - FrameLoader::timeOfLastCompletedLoad();
    float userDelta = userIdleTime();
    
    // FIXME: <rdar://problem/5211190> This limit of 42 risks growing the page cache far beyond its nominal capacity.
    if ((userDelta < 0.5 || loadDelta < 1.25) && m_autoreleaseSet.size() < 42) {
        LOG(PageCache, "WebCorePageCache: Postponing releaseAutoreleasedPagesNowOrReschedule() - %f since last load, %f since last input, %i objects pending release", loadDelta, userDelta, m_autoreleaseSet.size());
        timer->startOneShot(autoreleaseInterval);
        return;
    }

    LOG(PageCache, "WebCorePageCache: Releasing page caches - %f seconds since last load, %f since last input, %i objects pending release", loadDelta, userDelta, m_autoreleaseSet.size());
    releaseAutoreleasedPagesNow();
}

void PageCache::releaseAutoreleasedPagesNow()
{
    m_autoreleaseTimer.stop();

    CachedPageSet tmp;
    tmp.swap(m_autoreleaseSet);

    CachedPageSet::iterator end = tmp.end();
    for (CachedPageSet::iterator it = tmp.begin(); it != end; ++it)
        (*it)->close();
}

void PageCache::autorelease(PassRefPtr<CachedPage> page)
{
    ASSERT(page);
    ASSERT(!m_autoreleaseSet.contains(page.get()));
    m_autoreleaseSet.add(page);
    if (!m_autoreleaseTimer.isActive())
        m_autoreleaseTimer.startOneShot(autoreleaseInterval);
}

} // namespace WebCore
