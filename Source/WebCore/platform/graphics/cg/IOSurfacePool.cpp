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

#if HAVE(IOSURFACE)

#include "GraphicsContextCG.h"
#include <CoreGraphics/CoreGraphics.h>
#include <wtf/NeverDestroyed.h>

static const Seconds collectionInterval { 500_ms };
static const Seconds surfaceAgeBeforeMarkingPurgeable { 2_s };
const size_t defaultMaximumBytesCached = 1024 * 1024 * 64;

// We'll never allow more than 1/2 of the cache to be filled with in-use surfaces, because
// they can't be immediately returned when requested (but will be freed up in the future).
const size_t maximumInUseBytes = defaultMaximumBytesCached / 2;

#define ENABLE_IOSURFACE_POOL_STATISTICS 0
#if ENABLE_IOSURFACE_POOL_STATISTICS
#define DUMP_POOL_STATISTICS(reason) do { showPoolStatistics(reason); } while (0);
#else
#define DUMP_POOL_STATISTICS(reason) ((void)0)
#endif

namespace WebCore {

IOSurfacePool::IOSurfacePool()
    : m_collectionTimer(*this, &IOSurfacePool::collectionTimerFired)
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

static bool surfaceMatchesParameters(IOSurface& surface, IntSize requestedSize, CGColorSpaceRef colorSpace, IOSurface::Format format)
{
    if (format != surface.format())
        return false;
    if (colorSpace != surface.colorSpace())
        return false;
    if (requestedSize != surface.size())
        return false;
    return true;
}

void IOSurfacePool::willAddSurface(IOSurface& surface, bool inUse)
{
    CachedSurfaceDetails& details = m_surfaceDetails.add(&surface, CachedSurfaceDetails()).iterator->value;
    details.resetLastUseTime();

    surface.releaseGraphicsContext();

    size_t surfaceBytes = surface.totalBytes();

    evict(surfaceBytes);

    m_bytesCached += surfaceBytes;
    if (inUse)
        m_inUseBytesCached += surfaceBytes;
}

void IOSurfacePool::didRemoveSurface(IOSurface& surface, bool inUse)
{
    size_t surfaceBytes = surface.totalBytes();
    m_bytesCached -= surfaceBytes;
    if (inUse)
        m_inUseBytesCached -= surfaceBytes;

    m_surfaceDetails.remove(&surface);
}

void IOSurfacePool::didUseSurfaceOfSize(IntSize size)
{
    m_sizesInPruneOrder.remove(m_sizesInPruneOrder.reverseFind(size));
    m_sizesInPruneOrder.append(size);
}

std::unique_ptr<IOSurface> IOSurfacePool::takeSurface(IntSize size, CGColorSpaceRef colorSpace, IOSurface::Format format)
{
    CachedSurfaceMap::iterator mapIter = m_cachedSurfaces.find(size);

    if (mapIter == m_cachedSurfaces.end()) {
        DUMP_POOL_STATISTICS("takeSurface - none with matching size");
        return nullptr;
    }

    for (auto surfaceIter = mapIter->value.begin(); surfaceIter != mapIter->value.end(); ++surfaceIter) {
        if (!surfaceMatchesParameters(*surfaceIter->get(), size, colorSpace, format))
            continue;

        auto surface = WTFMove(*surfaceIter);
        mapIter->value.remove(surfaceIter);

        didUseSurfaceOfSize(size);

        if (mapIter->value.isEmpty()) {
            m_cachedSurfaces.remove(mapIter);
            m_sizesInPruneOrder.removeLast();
        }

        didRemoveSurface(*surface, false);

        surface->setIsVolatile(false);

        DUMP_POOL_STATISTICS("takeSurface - taking");
        return surface;
    }

    // Some of the in-use surfaces may no longer actually be in-use, but we haven't moved them over yet.
    for (auto surfaceIter = m_inUseSurfaces.begin(); surfaceIter != m_inUseSurfaces.end(); ++surfaceIter) {
        if (!surfaceMatchesParameters(*surfaceIter->get(), size, colorSpace, format))
            continue;
        if (surfaceIter->get()->isInUse())
            continue;
        
        auto surface = WTFMove(*surfaceIter);
        m_inUseSurfaces.remove(surfaceIter);
        didRemoveSurface(*surface, true);

        surface->setIsVolatile(false);

        DUMP_POOL_STATISTICS("takeSurface - taking in-use");
        return surface;
    }

    DUMP_POOL_STATISTICS("takeSurface - failing");
    return nullptr;
}

bool IOSurfacePool::shouldCacheSurface(const IOSurface& surface) const
{
    if (surface.totalBytes() > m_maximumBytesCached)
        return false;

    // There's no reason to pool empty surfaces; we should never allocate them in the first place.
    // This also covers isZero(), which would cause trouble when used as the key in m_cachedSurfaces.
    if (surface.size().isEmpty())
        return false;

    return true;
}

void IOSurfacePool::addSurface(std::unique_ptr<IOSurface> surface)
{
    if (!shouldCacheSurface(*surface))
        return;

    bool surfaceIsInUse = surface->isInUse();

    willAddSurface(*surface, surfaceIsInUse);

    if (surfaceIsInUse) {
        m_inUseSurfaces.prepend(WTFMove(surface));
        scheduleCollectionTimer();
        DUMP_POOL_STATISTICS("addSurface - in-use");
        return;
    }

    insertSurfaceIntoPool(WTFMove(surface));
    DUMP_POOL_STATISTICS("addSurface");
}

void IOSurfacePool::insertSurfaceIntoPool(std::unique_ptr<IOSurface> surface)
{
    IntSize surfaceSize = surface->size();
    auto insertedTuple = m_cachedSurfaces.add(surfaceSize, CachedSurfaceQueue());
    insertedTuple.iterator->value.prepend(WTFMove(surface));
    if (!insertedTuple.isNewEntry)
        m_sizesInPruneOrder.remove(m_sizesInPruneOrder.reverseFind(surfaceSize));
    m_sizesInPruneOrder.append(surfaceSize);

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

    auto surface = m_inUseSurfaces.takeLast();
    didRemoveSurface(*surface, true);
}

void IOSurfacePool::tryEvictOldestCachedSurface()
{
    if (m_cachedSurfaces.isEmpty())
        return;

    if (m_sizesInPruneOrder.isEmpty())
        return;

    CachedSurfaceMap::iterator surfaceQueueIter = m_cachedSurfaces.find(m_sizesInPruneOrder.first());
    ASSERT(!surfaceQueueIter->value.isEmpty());
    auto surface = surfaceQueueIter->value.takeLast();
    didRemoveSurface(*surface, false);

    if (surfaceQueueIter->value.isEmpty()) {
        m_cachedSurfaces.remove(surfaceQueueIter);
        m_sizesInPruneOrder.remove(0);
    }
}

void IOSurfacePool::evict(size_t additionalSize)
{
    DUMP_POOL_STATISTICS("before evict");

    if (additionalSize >= m_maximumBytesCached) {
        discardAllSurfaces();
        DUMP_POOL_STATISTICS("after evict all");
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

    DUMP_POOL_STATISTICS("after evict");
}

void IOSurfacePool::collectInUseSurfaces()
{
    CachedSurfaceQueue newInUseSurfaces;
    for (CachedSurfaceQueue::iterator surfaceIter = m_inUseSurfaces.begin(); surfaceIter != m_inUseSurfaces.end(); ++surfaceIter) {
        IOSurface* surface = surfaceIter->get();
        if (surface->isInUse()) {
            newInUseSurfaces.append(WTFMove(*surfaceIter));
            continue;
        }

        m_inUseBytesCached -= surface->totalBytes();
        insertSurfaceIntoPool(WTFMove(*surfaceIter));
    }

    m_inUseSurfaces = WTFMove(newInUseSurfaces);
}

bool IOSurfacePool::markOlderSurfacesPurgeable()
{
    bool markedAllSurfaces = true;
    auto markTime = MonotonicTime::now();

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

void IOSurfacePool::collectionTimerFired()
{
    collectInUseSurfaces();
    bool markedAllSurfaces = markOlderSurfacesPurgeable();

    if (!m_inUseSurfaces.size() && markedAllSurfaces)
        m_collectionTimer.stop();

    platformGarbageCollectNow();
    DUMP_POOL_STATISTICS("collectionTimerFired");
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

void IOSurfacePool::showPoolStatistics(const char* reason)
{
#if ENABLE_IOSURFACE_POOL_STATISTICS
    WTFLogAlways("IOSurfacePool Statistics: %s\n", reason);
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

        WTFLogAlways("   %d x %d: %zu surfaces for %.2f MB (%.2f MB purgeable)", keyAndSurfaces.key.width(), keyAndSurfaces.key.height(), keyAndSurfaces.value.size(), queueSize / (1024.0 * 1024.0), queuePurgeableSize / (1024.0 * 1024.0));
    }

    size_t inUseSize = 0;
    for (const auto& surface : m_inUseSurfaces) {
        totalSurfaces++;
        inUseSize += surface->totalBytes();
    }

    totalSize += inUseSize;
    WTFLogAlways("   IN USE: %zu surfaces for %.2f MB", m_inUseSurfaces.size(), inUseSize / (1024.0 * 1024.0));

    // FIXME: Should move consistency checks elsewhere, and always perform them in debug builds.
    ASSERT(m_bytesCached == totalSize);
    ASSERT(m_bytesCached <= m_maximumBytesCached);

    WTFLogAlways("   TOTAL: %d surfaces for %.2f MB (%.2f MB purgeable)\n", totalSurfaces, totalSize / (1024.0 * 1024.0), totalPurgeableSize / (1024.0 * 1024.0));
#else
    UNUSED_PARAM(reason);
#endif
}

}
#endif // HAVE(IOSURFACE)
