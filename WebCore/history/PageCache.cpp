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

#include "Cache.h"
#include "CachedPage.h"
#include "FrameLoader.h"
#include "HistoryItem.h"
#include "Logging.h"
#include "SystemTime.h"
#include <wtf/CurrentTime.h>

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
    , m_size(0)
    , m_head(0)
    , m_tail(0)
    , m_autoreleaseTimer(this, &PageCache::releaseAutoreleasedPagesNowOrReschedule)
{
}

void PageCache::setCapacity(int capacity)
{
    ASSERT(capacity >= 0);
    m_capacity = max(capacity, 0);

    prune();
}

void PageCache::add(PassRefPtr<HistoryItem> prpItem, PassRefPtr<CachedPage> cachedPage)
{
    ASSERT(prpItem);
    ASSERT(cachedPage);
    
    HistoryItem* item = prpItem.releaseRef(); // Balanced in remove().

    // Remove stale cache entry if necessary.
    if (item->m_cachedPage)
        remove(item);

    item->m_cachedPage = cachedPage;
    addToLRUList(item);
    ++m_size;
    
    prune();
}

void PageCache::remove(HistoryItem* item)
{
    // Safely ignore attempts to remove items not in the cache.
    if (!item || !item->m_cachedPage)
        return;

    autorelease(item->m_cachedPage.release());
    removeFromLRUList(item);
    --m_size;

    item->deref(); // Balanced in add().
}

void PageCache::prune()
{
    while (m_size > m_capacity) {
        ASSERT(m_tail && m_tail->m_cachedPage);
        remove(m_tail);
    }
}

void PageCache::addToLRUList(HistoryItem* item)
{
    item->m_next = m_head;
    item->m_prev = 0;

    if (m_head) {
        ASSERT(m_tail);
        m_head->m_prev = item;
    } else {
        ASSERT(!m_tail);
        m_tail = item;
    }

    m_head = item;
}

void PageCache::removeFromLRUList(HistoryItem* item)
{
    if (!item->m_next) {
        ASSERT(item == m_tail);
        m_tail = item->m_prev;
    } else {
        ASSERT(item != m_tail);
        item->m_next->m_prev = item->m_prev;
    }

    if (!item->m_prev) {
        ASSERT(item == m_head);
        m_head = item->m_next;
    } else {
        ASSERT(item != m_head);
        item->m_prev->m_next = item->m_next;
    }
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

    // Postpone dead pruning until all our resources have gone dead.
    cache()->setPruneEnabled(false);

    CachedPageSet tmp;
    tmp.swap(m_autoreleaseSet);

    CachedPageSet::iterator end = tmp.end();
    for (CachedPageSet::iterator it = tmp.begin(); it != end; ++it)
        (*it)->clear();

    // Now do the prune.
    cache()->setPruneEnabled(true);
    cache()->prune();
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
