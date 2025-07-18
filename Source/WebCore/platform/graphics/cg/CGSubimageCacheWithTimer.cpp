/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CGSubimageCacheWithTimer.h"

#if USE(CG)

#if CACHE_SUBIMAGES

#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CGSubimageCacheWithTimer);

CGSubimageCacheWithTimer* CGSubimageCacheWithTimer::s_cache;

RetainPtr<CGImageRef> CGSubimageCacheWithTimer::getSubimage(CGImageRef image, const FloatRect& rect)
{
    return subimageCache().subimage(image, rect);
}

void CGSubimageCacheWithTimer::clearImage(CGImageRef image)
{
    if (subimageCacheExists())
        subimageCache().clearImageAndSubimages(image);
}

void CGSubimageCacheWithTimer::clear()
{
    if (subimageCacheExists())
        subimageCache().clearAll();
}

struct CGSubimageRequest {
    CGImageRef image;
    const FloatRect& rect;
};

struct CGSubimageCacheAdder {
    static unsigned hash(const CGSubimageRequest& value)
    {
        return CGSubimageCacheWithTimer::CacheHash::hash(value.image, value.rect);
    }

    static bool equal(const CGSubimageCacheWithTimer::CacheEntry& a, const CGSubimageRequest& b)
    {
        return a.image == b.image && a.rect == b.rect;
    }

    static void translate(CGSubimageCacheWithTimer::CacheEntry& entry, const CGSubimageRequest& request, unsigned /*hashCode*/)
    {
        entry.image = request.image;
        entry.subimage = adoptCF(CGImageCreateWithImageInRect(request.image, request.rect));
        entry.rect = request.rect;
    }
};

CGSubimageCacheWithTimer::CGSubimageCacheWithTimer()
    : m_timer(RunLoop::mainSingleton(), this, &CGSubimageCacheWithTimer::pruneCacheTimerFired)
{
}

void CGSubimageCacheWithTimer::pruneCacheTimerFired()
{
    Locker locker { m_lock };
    prune();
    if (m_cache.isEmpty()) {
        ASSERT(m_imageCounts.isEmpty());
        m_timer.stop();
    }
}

void CGSubimageCacheWithTimer::prune()
{
    auto now = MonotonicTime::now();

    Vector<CacheEntry> toBeRemoved;

    for (const auto& entry : m_cache) {
        if ((now - entry.lastAccessTime) > CGSubimageCacheWithTimer::cacheEntryLifetime)
            toBeRemoved.append(entry);
    }

    for (auto& entry : toBeRemoved) {
        m_imageCounts.remove(entry.image.get());
        m_cache.remove(entry);
    }
}

RetainPtr<CGImageRef> CGSubimageCacheWithTimer::subimage(CGImageRef image, const FloatRect& rect)
{
    Locker locker { m_lock };
    if (!m_timer.isActive())
        m_timer.startRepeating(CGSubimageCacheWithTimer::cachePruneDelay);

    if (m_cache.size() == CGSubimageCacheWithTimer::maxCacheSize) {
        CacheEntry entry = *m_cache.begin();
        m_imageCounts.remove(entry.image.get());
        m_cache.remove(entry);
    }

    ASSERT(m_cache.size() < CGSubimageCacheWithTimer::maxCacheSize);
    auto result = m_cache.add<CGSubimageCacheAdder>(CGSubimageRequest { image, rect });
    if (result.isNewEntry)
        m_imageCounts.add(image);

    result.iterator->lastAccessTime = MonotonicTime::now();
    return result.iterator->subimage;
}

void CGSubimageCacheWithTimer::clearImageAndSubimages(CGImageRef image)
{
    Locker locker { m_lock };
    if (m_imageCounts.contains(image)) {
        Vector<CacheEntry> toBeRemoved;
        for (const auto& entry : m_cache) {
            if (entry.image.get() == image)
                toBeRemoved.append(entry);
        }

        for (auto& entry : toBeRemoved)
            m_cache.remove(entry);

        m_imageCounts.removeAll(image);
    }
}

void CGSubimageCacheWithTimer::clearAll()
{
    Locker locker { m_lock };
    m_imageCounts.clear();
    m_cache.clear();
}

CGSubimageCacheWithTimer& CGSubimageCacheWithTimer::subimageCache()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        s_cache = new CGSubimageCacheWithTimer;
    });
    return *s_cache;
}

bool CGSubimageCacheWithTimer::subimageCacheExists()
{
    return !!s_cache;
}

} // namespace WebCore

#endif // CACHE_SUBIMAGES

#endif // USE(CG)
