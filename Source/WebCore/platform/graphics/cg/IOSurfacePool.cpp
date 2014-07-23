/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include "IOSurfacePool.h"

#if USE(IOSURFACE)

#include "GraphicsContextCG.h"
#include "IOSurface.h"
#include <CoreGraphics/CoreGraphics.h>
#include <chrono>
#include <wtf/NeverDestroyed.h>

const std::chrono::milliseconds collectionInterval = 500_ms;
const std::chrono::seconds surfaceAgeBeforeMarkingPurgeable = 2_s;
const size_t defaultMaximumBytesCached = 1024 * 1024 * 64;

// We'll never allow more than 1/2 of the cache to be filled with in-use surfaces, because
// they can't be immediately returned when requested (but will be freed up in the future).
const size_t maximumInUseBytes = defaultMaximumBytesCached / 2;

#define ENABLE_IOSURFACE_POOL_STATISTICS false
#if ENABLE_IOSURFACE_POOL_STATISTICS
#define DUMP_POOL_STATISTICS() do { showPoolStatistics(); } while (0);
#else
#define DUMP_POOL_STATISTICS() ((void)0)
#endif

namespace WebCore {

IOSurfacePool::IOSurfacePool()
    : m_collectionTimer(this, &IOSurfacePool::collectionTimerFired)
    , m_bytesCached(0)
    , m_inUseBytesCached(0)
    , m_maximumBytesCached(defaultMaximumBytesCached)
{
}

IOSurfacePool& IOSurfacePool::sharedPool()
{
    static NeverDestroyed<IOSurfacePool> pool;
    return pool;
}

static bool surfaceMatchesParameters(IOSurface& surface, const IntSize& requestedSize, ColorSpace colorSpace)
{
    IntSize surfaceSize = surface.size();
    if (colorSpace != surface.colorSpace())
        return false;
    if (surfaceSize != requestedSize)
        return false;
    return true;
}

void IOSurfacePool::willAddSurface(IOSurface* surface, bool inUse)
{
    CachedSurfaceDetails& details = m_surfaceDetails.add(surface, CachedSurfaceDetails()).iterator->value;
    details.resetLastUseTime();

    surface->releaseGraphicsContext();

    size_t surfaceBytes = surface->totalBytes();

    evict(surfaceBytes);

    m_bytesCached += surfaceBytes;
    if (inUse)
        m_inUseBytesCached += surfaceBytes;
}

void IOSurfacePool::didRemoveSurface(IOSurface* surface, bool inUse)
{
    size_t surfaceBytes = surface->totalBytes();
    m_bytesCached -= surfaceBytes;
    if (inUse)
        m_inUseBytesCached -= surfaceBytes;

    m_surfaceDetails.remove(surface);
}

void IOSurfacePool::didUseSurfaceOfSize(IntSize size)
{
    m_sizesInPruneOrder.remove(m_sizesInPruneOrder.reverseFind(size));
    m_sizesInPruneOrder.append(size);
}

PassRefPtr<IOSurface> IOSurfacePool::takeSurface(IntSize size, ColorSpace colorSpace)
{
    CachedSurfaceMap::iterator mapIter = m_cachedSurfaces.find(size);

    if (mapIter == m_cachedSurfaces.end()) {
        DUMP_POOL_STATISTICS();
        return nullptr;
    }

    for (auto surfaceIter = mapIter->value.begin(); surfaceIter != mapIter->value.end(); ++surfaceIter) {
        if (!surfaceMatchesParameters(*surfaceIter->get(), size, colorSpace))
            continue;
        
        RefPtr<IOSurface> surface = surfaceIter->get();
        mapIter->value.remove(surfaceIter);

        didUseSurfaceOfSize(size);

        if (mapIter->value.isEmpty()) {
            m_cachedSurfaces.remove(mapIter);
            m_sizesInPruneOrder.removeLast();
        }

        didRemoveSurface(surface.get(), false);

        surface->setIsVolatile(false);

        DUMP_POOL_STATISTICS();
        return surface.release();
    }

    // Some of the in-use surfaces may no longer actually be in-use, but we haven't moved them over yet.
    for (auto surfaceIter = m_inUseSurfaces.begin(); surfaceIter != m_inUseSurfaces.end(); ++surfaceIter) {
        if (!surfaceMatchesParameters(*surfaceIter->get(), size, colorSpace))
            continue;
        if (surfaceIter->get()->isInUse())
            continue;
        
        RefPtr<IOSurface> surface = surfaceIter->get();
        m_inUseSurfaces.remove(surfaceIter);
        didRemoveSurface(surface.get(), true);

        surface->setIsVolatile(false);

        DUMP_POOL_STATISTICS();
        return surface.release();
    }

    DUMP_POOL_STATISTICS();
    return nullptr;
}

void IOSurfacePool::addSurface(IOSurface* surface)
{
    if (surface->totalBytes() > m_maximumBytesCached)
        return;

    // There's no reason to pool empty surfaces; we should never allocate them in the first place.
    // This also covers isZero(), which would cause trouble when used as the key in m_cachedSurfaces.
    if (surface->size().isEmpty())
        return;

    bool surfaceIsInUse = surface->isInUse();

    willAddSurface(surface, surfaceIsInUse);

    if (surfaceIsInUse) {
        m_inUseSurfaces.prepend(surface);
        scheduleCollectionTimer();
        DUMP_POOL_STATISTICS();
        return;
    }

    insertSurfaceIntoPool(surface);
    DUMP_POOL_STATISTICS();
}

void IOSurfacePool::insertSurfaceIntoPool(IOSurface* surface)
{
    auto insertedTuple = m_cachedSurfaces.add(surface->size(), CachedSurfaceQueue());
    insertedTuple.iterator->value.prepend(surface);
    if (!insertedTuple.isNewEntry)
        m_sizesInPruneOrder.remove(m_sizesInPruneOrder.reverseFind(surface->size()));
    m_sizesInPruneOrder.append(surface->size());

    scheduleCollectionTimer();
}

void IOSurfacePool::setPoolSize(size_t poolSizeInBytes)
{
    m_maximumBytesCached = poolSizeInBytes;
    evict(0);
}

void IOSurfacePool::tryEvictInUseSurface()
{
    if (m_inUseSurfaces.isEmpty())
        return;

    RefPtr<IOSurface> surface = m_inUseSurfaces.takeLast();
    didRemoveSurface(surface.get(), true);
}

void IOSurfacePool::tryEvictOldestCachedSurface()
{
    if (m_cachedSurfaces.isEmpty())
        return;

    if (m_sizesInPruneOrder.isEmpty())
        return;

    CachedSurfaceMap::iterator surfaceQueueIter = m_cachedSurfaces.find(m_sizesInPruneOrder.first());
    ASSERT(!surfaceQueueIter->value.isEmpty());
    RefPtr<IOSurface> surface = surfaceQueueIter->value.takeLast();
    didRemoveSurface(surface.get(), false);

    if (surfaceQueueIter->value.isEmpty()) {
        m_cachedSurfaces.remove(surfaceQueueIter);
        m_sizesInPruneOrder.remove(0);
    }
}

void IOSurfacePool::evict(size_t additionalSize)
{
    if (additionalSize >= m_maximumBytesCached) {
        discardAllSurfaces();
        return;
    }

    // FIXME: Perhaps purgeable surfaces should count less against the cap?
    // We don't want to end up with a ton of empty (purged) surfaces, though, as that would defeat the purpose of the pool.
    size_t targetSize = m_maximumBytesCached - additionalSize;

    // Interleave eviction of old cached surfaces and more recent in-use surfaces.
    // In-use surfaces are more recently used, but less useful in the pool, as they aren't
    // immediately available when requested.
    while (m_bytesCached > targetSize) {
        tryEvictOldestCachedSurface();

        if (m_inUseBytesCached > maximumInUseBytes || m_bytesCached > targetSize)
            tryEvictInUseSurface();
    }

    while (m_inUseBytesCached > maximumInUseBytes || m_bytesCached > targetSize)
        tryEvictInUseSurface();
}

void IOSurfacePool::collectInUseSurfaces()
{
    CachedSurfaceQueue newInUseSurfaces;
    for (CachedSurfaceQueue::iterator surfaceIter = m_inUseSurfaces.begin(); surfaceIter != m_inUseSurfaces.end(); ++surfaceIter) {
        IOSurface* surface = surfaceIter->get();
        if (surface->isInUse()) {
            newInUseSurfaces.append(*surfaceIter);
            continue;
        }

        m_inUseBytesCached -= surface->totalBytes();
        insertSurfaceIntoPool(surface);
    }

    m_inUseSurfaces = newInUseSurfaces;
}

bool IOSurfacePool::markOlderSurfacesPurgeable()
{
    bool markedAllSurfaces = true;
    auto markTime = std::chrono::steady_clock::now();

    for (auto& surfaceAndDetails : m_surfaceDetails) {
        if (surfaceAndDetails.value.hasMarkedPurgeable)
            continue;

        if (markTime - surfaceAndDetails.value.lastUseTime < surfaceAgeBeforeMarkingPurgeable) {
            markedAllSurfaces = false;
            continue;
        }

        surfaceAndDetails.key->setIsVolatile(true);
        surfaceAndDetails.value.hasMarkedPurgeable = true;
    }

    return markedAllSurfaces;
}

void IOSurfacePool::collectionTimerFired(Timer<IOSurfacePool>&)
{
    collectInUseSurfaces();
    bool markedAllSurfaces = markOlderSurfacesPurgeable();

    if (!m_inUseSurfaces.size() && markedAllSurfaces)
        m_collectionTimer.stop();

    platformGarbageCollectNow();
    DUMP_POOL_STATISTICS();
}

void IOSurfacePool::scheduleCollectionTimer()
{
    if (!m_collectionTimer.isActive())
        m_collectionTimer.startRepeating(collectionInterval);
}

void IOSurfacePool::discardAllSurfaces()
{
    m_bytesCached = 0;
    m_inUseBytesCached = 0;
    m_surfaceDetails.clear();
    m_cachedSurfaces.clear();
    m_inUseSurfaces.clear();
    m_sizesInPruneOrder.clear();
    m_collectionTimer.stop();
    platformGarbageCollectNow();
}

void IOSurfacePool::showPoolStatistics()
{
#if ENABLE_IOSURFACE_POOL_STATISTICS
    WTFLogAlways("IOSurfacePool Statistics\n");
    unsigned totalSurfaces = 0;
    size_t totalSize = 0;
    size_t totalPurgeableSize = 0;

    for (const auto& keyAndSurfaces : m_cachedSurfaces) {
        ASSERT(!keyAndSurfaces.value.isEmpty());
        size_t queueSize = 0;
        size_t queuePurgeableSize = 0;
        for (const auto& surface : keyAndSurfaces.value) {
            size_t surfaceBytes = surface->totalBytes();

            totalSurfaces++;
            queueSize += surfaceBytes;

            if (surface->isVolatile())
                queuePurgeableSize += surfaceBytes;
        }

        totalSize += queueSize;
        totalPurgeableSize += queuePurgeableSize;

        WTFLogAlways("   %d x %d: %zu surfaces for %zd KB (%zd KB purgeable)", keyAndSurfaces.key.width(), keyAndSurfaces.key.height(), keyAndSurfaces.value.size(), queueSize / 1024, queuePurgeableSize / 1024);
    }

    size_t inUseSize = 0;
    for (const auto& surface : m_inUseSurfaces) {
        totalSurfaces++;
        inUseSize += surface->totalBytes();
    }

    totalSize += inUseSize;
    WTFLogAlways("   IN USE: %zu surfaces for %zd KB", m_inUseSurfaces.size(), inUseSize / 1024);

    // FIXME: Should move consistency checks elsewhere, and always perform them in debug builds.
    ASSERT(m_bytesCached == totalSize);
    ASSERT(m_bytesCached <= m_maximumBytesCached);

    WTFLogAlways("   TOTAL: %d surfaces for %zd KB (%zd KB purgeable)\n", totalSurfaces, totalSize / 1024, totalPurgeableSize / 1024);
#endif
}

}
#endif // USE(IOSURFACE)
