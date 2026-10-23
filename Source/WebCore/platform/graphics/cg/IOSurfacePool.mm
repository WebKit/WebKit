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

#import "config.h"
#import "IOSurfacePool.h"

#if HAVE(IOSURFACE)

#import "ColorSpace.h"
#import "GraphicsContextCG.h"
#import "ImageBufferBackend.h"
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>
#import <atomic>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WorkQueue.h>
#import <wtf/text/TextStream.h>

static const Seconds inUseCollectionInterval { 100_ms };

#define ENABLE_IOSURFACE_POOL_STATISTICS 0
#if ENABLE_IOSURFACE_POOL_STATISTICS
#define DUMP_POOL_STATISTICS(commands) do { ALWAYS_LOG_WITH_STREAM(commands); } while (0); // NOLINT
#else
#define DUMP_POOL_STATISTICS(commands) ((void)0)
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IOSurfacePool);

static WorkQueue& maintenanceQueueSingleton()
{
    static NeverDestroyed<Ref<WorkQueue>> queue = WorkQueue::create("IOSurfacePool maintenance queue"_s, WorkQueue::QOS::Utility);
    return queue.get();
}

static void releaseLayerContents()
{
    if (isWebThread())
        return;
    if (!RunLoop::isMain()) {
        RunLoop::mainSingleton().dispatch(releaseLayerContents);
        return;
    }
    // We need to trigger a CA commit in the web process to trigger the release layer-related memory, since the WebProcess doesn't normally do CA commits.
    [CATransaction begin];
    [CATransaction commit];
}

IOSurfacePool::IOSurfacePool()
{
    DUMP_POOL_STATISTICS(stream << "IOSurfacePool [" << this << "] constructor.");
}

IOSurfacePool::~IOSurfacePool()
{
    DUMP_POOL_STATISTICS(stream << "IOSurfacePool [" << this << "] destructor.");
    if (m_cachedSurfaces.isEmpty() && m_inUseSurfaces.isEmpty())
        return;
    // FIXME: Consider destroying the surfaces on the maintenance queue.
    m_cachedSurfaces.clear();
    m_inUseSurfaces.clear();
    releaseLayerContents();
}

IOSurfacePool& IOSurfacePool::sharedPoolSingleton()
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

static bool surfaceMatchesParameters(IOSurface& surface, IntSize requestedSize, const ColorSpace& colorSpace, IOSurface::Format format, UseLosslessCompression useLosslessCompression)
{
    // FIXME: It might be OK to take a surface that doesn't use compression when requesting one that does, but not the other way around.
    if (!surface.hasFormat({ format, useLosslessCompression }))
        return false;
    if (colorSpace != surface.colorSpace())
        return false;
    if (requestedSize != surface.size())
        return false;
    return true;
}

void IOSurfacePool::willAddSurface(IOSurface& surface, bool inUse)
{
    size_t surfaceBytes = surface.totalBytes();

    evict(surfaceBytes);

    m_bytesCached += surfaceBytes;
    if (inUse)
        m_inUseBytesCached += surfaceBytes;
    else
        surface.setVolatile(true);
}

void IOSurfacePool::didRemoveSurface(IOSurface& surface, bool inUse)
{
    size_t surfaceBytes = surface.totalBytes();
    m_bytesCached -= surfaceBytes;
    if (inUse)
        m_inUseBytesCached -= surfaceBytes;
}

void IOSurfacePool::didUseSurfaceOfSize(IntSize size)
{
    m_sizesInPruneOrder.removeLast(size);
    m_sizesInPruneOrder.append(size);
}

std::unique_ptr<IOSurface> IOSurfacePool::takeSurface(IntSize size, const ColorSpace& colorSpace, IOSurface::Format format, UseLosslessCompression useLosslessCompression)
{
    Locker locker { m_lock };
    if (auto mapIter = m_cachedSurfaces.find(size); mapIter != m_cachedSurfaces.end()) {
        for (auto surfaceIter = mapIter->value.begin(); surfaceIter != mapIter->value.end(); ++surfaceIter) {
            if (!surfaceMatchesParameters(*surfaceIter->get(), size, colorSpace, format, useLosslessCompression))
                continue;

            auto surface = WTF::move(*surfaceIter);
            mapIter->value.remove(surfaceIter);

            didUseSurfaceOfSize(size);

            if (mapIter->value.isEmpty()) {
                m_cachedSurfaces.remove(mapIter);
                m_sizesInPruneOrder.removeLast();
            }

            didRemoveSurface(*surface, false);

            auto purgeableState = surface->setVolatile(false);

            DUMP_POOL_STATISTICS(stream << "IOSurfacePool::takeSurface [" << this << "] - taking surface " << surface.get() << " with size " << size << " color space " << colorSpace << " format " << format << " purgeable state " << purgeableState << "\n" << poolStatistics());
            (void)purgeableState;
            return surface;
        }
    }

    // Some of the in-use surfaces may no longer actually be in-use, but we haven't moved them over yet.
    for (auto surfaceIter = m_inUseSurfaces.begin(); surfaceIter != m_inUseSurfaces.end(); ++surfaceIter) {
        if (!surfaceMatchesParameters(*surfaceIter->get(), size, colorSpace, format, useLosslessCompression))
            continue;
        if (surfaceIter->get()->isInUse())
            continue;

        auto surface = WTF::move(*surfaceIter);
        m_inUseSurfaces.remove(surfaceIter);
        didRemoveSurface(*surface, true);

        surface->setVolatile(false);

        DUMP_POOL_STATISTICS(stream << "IOSurfacePool::takeSurface [" << this << "] - taking surface " << surface.get() << " with size " << size << " color space " << colorSpace << " format " << format << "\n" << poolStatistics());
        return surface;
    }

    DUMP_POOL_STATISTICS(stream << "IOSurfacePool::takeSurface [" << this << "] - failing\n" << poolStatistics());
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
        m_inUseSurfaces.prepend(WTF::move(surface));
        scheduleCollection();
        DUMP_POOL_STATISTICS(stream << "IOSurfacePool::addSurface [" << this << "] - in-use\n" << poolStatistics());
        return;
    }

    insertSurfaceIntoPool(WTF::move(surface));
    DUMP_POOL_STATISTICS(stream << "IOSurfacePool::addSurface [" << this << "]\n" << poolStatistics());
}

void IOSurfacePool::insertSurfaceIntoPool(std::unique_ptr<IOSurface> surface)
{
    IntSize surfaceSize = surface->size();
    auto insertedTuple = m_cachedSurfaces.add(surfaceSize, CachedSurfaceQueue());
    insertedTuple.iterator->value.prepend(WTF::move(surface));
    if (!insertedTuple.isNewEntry)
        m_sizesInPruneOrder.removeLast(surfaceSize);
    m_sizesInPruneOrder.append(surfaceSize);
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

    auto surfaceQueueIter = m_cachedSurfaces.find(m_sizesInPruneOrder.first());
    if (surfaceQueueIter == m_cachedSurfaces.end() || surfaceQueueIter->value.isEmpty()) {
        // Crashes indicate that m_sizesInPruneOrder can contain a value not in m_cachedSurfaces. (rdar://177969354).
        ASSERT_NOT_REACHED();
        m_sizesInPruneOrder.removeAt(0);
        return;
    }

    auto surface = surfaceQueueIter->value.takeLast();
    didRemoveSurface(*surface, false);

    if (surfaceQueueIter->value.isEmpty()) {
        m_cachedSurfaces.remove(surfaceQueueIter);
        m_sizesInPruneOrder.removeAt(0);
    }
}

void IOSurfacePool::evict(size_t additionalSize)
{
    DUMP_POOL_STATISTICS(stream << "IOSurfacePool::evict [" << this << "] - before evict\n" << poolStatistics());

    if (additionalSize >= m_maximumBytesCached) {
        discardAllSurfacesInternal();
        DUMP_POOL_STATISTICS(stream << "IOSurfacePool::evict [" << this << "] - after evict all\n" << poolStatistics());
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

    DUMP_POOL_STATISTICS(stream << "IOSurfacePool::evict [" << this << "] - after evict\n" << poolStatistics());
}

void IOSurfacePool::collectInUseSurfaces()
{
    CachedSurfaceQueue newInUseSurfaces;
    for (CachedSurfaceQueue::iterator surfaceIter = m_inUseSurfaces.begin(); surfaceIter != m_inUseSurfaces.end(); ++surfaceIter) {
        IOSurface* surface = surfaceIter->get();
        if (surface->isInUse()) {
            newInUseSurfaces.append(WTF::move(*surfaceIter));
            continue;
        }

        m_inUseBytesCached -= surface->totalBytes();
        surface->setVolatile(true);
        insertSurfaceIntoPool(WTF::move(*surfaceIter));
    }

    m_inUseSurfaces = WTF::move(newInUseSurfaces);
}

void IOSurfacePool::performCollection(uint64_t token)
{
    {
        Locker locker { m_lock };
        if (token != m_collectionToken)
            return;
        m_collectionToken = 0;
        collectInUseSurfaces();

        if (m_inUseSurfaces.size())
            scheduleCollection();

        DUMP_POOL_STATISTICS(stream << "IOSurfacePool::performCollection [" << this << "]\n" << poolStatistics());
    }
    releaseLayerContents();
}

void IOSurfacePool::scheduleCollection()
{
    if (m_collectionToken)
        return;
    // Tokens are unique across pools, so a cancelled task can never match a later one.
    static std::atomic<uint64_t> lastToken { 0 };
    m_collectionToken = ++lastToken;
    maintenanceQueueSingleton().dispatchAfter(inUseCollectionInterval, [weakThis = ThreadSafeWeakPtr { *this }, token = m_collectionToken] {
        if (auto pool = weakThis.get())
            pool->performCollection(token);
    });
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
    m_cachedSurfaces.clear();
    m_inUseSurfaces.clear();
    m_sizesInPruneOrder.clear();
    m_collectionToken = 0;
    releaseLayerContents();
}

String IOSurfacePool::poolStatistics() const
{
#if ENABLE_IOSURFACE_POOL_STATISTICS
    TextStream stream;
    stream << "Process " << getpid() << " IOSurfacePool [" << this << "] Statistics:\n";

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

        stream << "   " << keyAndSurfaces.key << ": " << keyAndSurfaces.value.size() << " surfaces for " << queueSize / (1024.0 * 1024.0) << " MB (" << queuePurgeableSize / (1024.0 * 1024.0) << " MB (" << (queuePurgeableSize * 100.0) / queueSize << "%) purgeable)\n";
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

    stream << "   TOTAL: " << totalSurfaces << " surfaces for " << totalSize / (1024.0 * 1024.0) << " MB (" << totalPurgeableSize / (1024.0 * 1024.0) << " MB (" << (totalPurgeableSize * 100.0) / totalSize << "%) purgeable)\n";
    return stream.release();
#else
    return emptyString();
#endif
}

}
#endif // HAVE(IOSURFACE)
