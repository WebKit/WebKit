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

#include "DestinationColorSpace.h"
#include "GraphicsContextCG.h"
#include <CoreGraphics/CoreGraphics.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>

static const Seconds collectionInterval { 500_ms };
static const Seconds surfaceAgeBeforeMarkingPurgeable { 2_s };

#define ENABLE_IOSURFACE_POOL_STATISTICS 0
#if ENABLE_IOSURFACE_POOL_STATISTICS
#define DUMP_POOL_STATISTICS(commands) do { ALWAYS_LOG_WITH_STREAM(commands); } while (0);
#else
#define DUMP_POOL_STATISTICS(commands) ((void)0)
#endif

namespace WebCore {

IOSurfacePool::IOSurfacePool()
    : m_collectionTimer(RunLoop::main(), this, &IOSurfacePool::collectionTimerFired)
{
}

IOSurfacePool::~IOSurfacePool()
{
    callOnMainRunLoopAndWait([&] {
        discardAllSurfaces();
    });
}

IOSurfacePool& IOSurfacePool::sharedPool()
{
    static LazyNeverDestroyed<IOSurfacePool> pool;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, [] {
        pool.construct();
    });
    return pool;
}

Ref<IOSurfacePool> IOSurfacePool::create()
{
    return adoptRef(*new IOSurfacePool);
}

static bool surfaceMatchesParameters(IOSurface& surface, IntSize requestedSize, const DestinationColorSpace& colorSpace, IOSurface::Format format)
{
    if (!surface.hasFormat(format))
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

std::unique_ptr<IOSurface> IOSurfacePool::takeSurface(IntSize size, const DestinationColorSpace& colorSpace, IOSurface::Format format)
{
    Locker locker { m_lock };
    CachedSurfaceMap::iterator mapIter = m_cachedSurfaces.find(size);

    if (mapIter == m_cachedSurfaces.end()) {
        DUMP_POOL_STATISTICS(stream << "IOSurfacePool::takeSurface - failed to find surface matching size " << size << " color space " << colorSpace << " format " << format << "\n" << poolStatistics());
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

        surface->setVolatile(false);

        DUMP_POOL_STATISTICS(stream << "IOSurfacePool::takeSurface - taking surface " << surface.get() << " with size " << size << " color space " << colorSpace << " format " << format << "\n" << poolStatistics());
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

        surface->setVolatile(false);

        DUMP_POOL_STATISTICS(stream << "IOSurfacePool::takeSurface - taking surface " << surface.get() << " with size " << size << " color space " << colorSpace << " format " << format << "\n" << poolStatistics());
        return surface;
    }

    DUMP_POOL_STATISTICS(stream << "IOSurfacePool::takeSurface - failing\n" << poolStatistics());
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

void IOSurfacePool::addSurface(std::unique_ptr<IOSurface>&& surface)
{
    Locker locker { m_lock };
    if (!shouldCacheSurface(*surface))
        return;

    bool surfaceIsInUse = surface->isInUse();

    willAddSurface(*surface, surfaceIsInUse);

    if (surfaceIsInUse) {
        m_inUseSurfaces.prepend(WTFMove(surface));
        scheduleCollectionTimer();
        DUMP_POOL_STATISTICS(stream << "addSurface - in-use\n" << poolStatistics());
        return;
    }

    insertSurfaceIntoPool(WTFMove(surface));
    DUMP_POOL_STATISTICS(stream << "addSurface\n" << poolStatistics());
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
    Locker locker { m_lock };
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
    DUMP_POOL_STATISTICS(stream << "before evict\n" << poolStatistics());

    if (additionalSize >= m_maximumBytesCached) {
        discardAllSurfacesInternal();
        DUMP_POOL_STATISTICS(stream << "after evict all\n" << poolStatistics());
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

    DUMP_POOL_STATISTICS(stream << "after evict\n" << poolStatistics());
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

        surfaceAndDetails.key->setVolatile(true);
        surfaceAndDetails.value.hasMarkedPurgeable = true;
    }

    return markedAllSurfaces;
}

void IOSurfacePool::collectionTimerFired()
{
    Locker locker { m_lock };
    collectInUseSurfaces();
    bool markedAllSurfaces = markOlderSurfacesPurgeable();

    if (!m_inUseSurfaces.size() && markedAllSurfaces)
        m_collectionTimer.stop();

    platformGarbageCollectNow();
    DUMP_POOL_STATISTICS(stream << "collectionTimerFired\n" << poolStatistics());
}

void IOSurfacePool::scheduleCollectionTimer()
{
    if (!m_collectionTimer.isActive())
        m_collectionTimer.startRepeating(collectionInterval);
}

void IOSurfacePool::discardAllSurfaces()
{
    Locker locker { m_lock };
    discardAllSurfacesInternal();
}

void IOSurfacePool::discardAllSurfacesInternal()
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

String IOSurfacePool::poolStatistics() const
{
#if ENABLE_IOSURFACE_POOL_STATISTICS
    TextStream stream;
    stream << "Process " << getpid() << " IOSurfacePool Statistics:\n";

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

        stream << "   " << keyAndSurfaces.key << ": " << keyAndSurfaces.value.size() << " surfaces for " << queueSize / (1024.0 * 1024.0) << " MB (" << queuePurgeableSize / (1024.0 * 1024.0) << " MB purgeable)\n";
    }

    size_t inUseSize = 0;
    for (const auto& surface : m_inUseSurfaces) {
        totalSurfaces++;
        inUseSize += surface->totalBytes();
    }

    totalSize += inUseSize;
    stream << "   IN USE: " << m_inUseSurfaces.size() << " surfaces for " << inUseSize / (1024.0 * 1024.0) << " MB";

    // FIXME: Should move consistency checks elsewhere, and always perform them in debug builds.
    ASSERT(m_bytesCached == totalSize);
    ASSERT(m_bytesCached <= m_maximumBytesCached);

    stream << "   TOTAL: " << totalSurfaces << " surfaces for " << totalSize / (1024.0 * 1024.0) << " MB (" << totalPurgeableSize / (1024.0 * 1024.0) << " MB purgeable)\n";
    return stream.release();
#else
    return emptyString();
#endif
}

}
#endif // HAVE(IOSURFACE)
