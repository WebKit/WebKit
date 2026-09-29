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

#pragma once

#include <wtf/Platform.h>
#if HAVE(IOSURFACE)

#include <WebCore/IOSurface.h>
#include <WebCore/IOSurfacePoolIdentifier.h>
#include <WebCore/IntSize.h>
#include <WebCore/IntSizeHash.h>
#include <WebCore/Timer.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class ColorSpace;

class IOSurfacePool : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<IOSurfacePool> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(IOSurfacePool, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(IOSurfacePool);
    friend class LazyNeverDestroyed<IOSurfacePool>;

public:
    WEBCORE_EXPORT static IOSurfacePool& sharedPoolSingleton();
    WEBCORE_EXPORT static Ref<IOSurfacePool> create();

    WEBCORE_EXPORT ~IOSurfacePool();

    WEBCORE_EXPORT std::unique_ptr<IOSurface> takeSurface(IntSize, const ColorSpace&, IOSurface::Format, UseLosslessCompression);
    WEBCORE_EXPORT void addSurface(std::unique_ptr<IOSurface>&&);

    WEBCORE_EXPORT void discardAllSurfaces();

    WEBCORE_EXPORT void setPoolSize(size_t);

private:
    IOSurfacePool();

    using CachedSurfaceQueue = Deque<std::unique_ptr<IOSurface>>;
    using CachedSurfaceMap = HashMap<IntSize, CachedSurfaceQueue>;

    // Pooled surfaces are marked volatile on insertion, so pool bytes are not charged to the owning
    // process's ledger. That makes a large pool cheap and a high hit rate valuable: every miss mints
    // a fresh non-volatile (charged) surface, while a hit just borrows uncharged bytes back. Measured
    // on apple.com/iphone-duo: 64 MB -> 84% hit rate, owned-unmapped p90/p95 518/587 MB; 256 MB ->
    // 93% and 455/483 MB, despite holding ~166 MB more surfaces.
    static constexpr size_t defaultMaximumBytesCached { 256 * MB };
    // in-use surfaces can't be immediately recycled but may be available soon. We should
    // limit caching in use surfaces so that we don't end up with a pool of surfaces we
    // can't readily recycle.
#if PLATFORM(MAC)
    static constexpr size_t maximumInUseBytes = 0.5 * defaultMaximumBytesCached;
#else
    // 32 MB is chosen for iOS as it is approximately the size of a set of front and back buffer
    // for 4 page tiles of dimensions 1024x1024. This should provide a good trade off of soon to be
    // available surfaces without contributing too much non-volatile footprint during memory pressure situations.
    static constexpr size_t maximumInUseBytes { 32 * MB };
#endif

    bool NODELETE shouldCacheSurface(const IOSurface&) const WTF_REQUIRES_LOCK(m_lock);

    void willAddSurface(IOSurface&, bool inUse) WTF_REQUIRES_LOCK(m_lock);
    void didRemoveSurface(IOSurface&, bool inUse) WTF_REQUIRES_LOCK(m_lock);
    void didUseSurfaceOfSize(IntSize) WTF_REQUIRES_LOCK(m_lock);

    void insertSurfaceIntoPool(std::unique_ptr<IOSurface>) WTF_REQUIRES_LOCK(m_lock);

    void evict(size_t additionalSize) WTF_REQUIRES_LOCK(m_lock);
    void tryEvictInUseSurface() WTF_REQUIRES_LOCK(m_lock);
    void tryEvictOldestCachedSurface() WTF_REQUIRES_LOCK(m_lock);

    void scheduleCollectionTimer() WTF_REQUIRES_LOCK(m_lock);
    void stopCollectionTimer() WTF_REQUIRES_LOCK(m_lock);
    void collectionTimerFired();
    void collectInUseSurfaces() WTF_REQUIRES_LOCK(m_lock);

    void platformGarbageCollectNow();

    void discardAllSurfacesInternal() WTF_REQUIRES_LOCK(m_lock);

    String NODELETE poolStatistics() const WTF_REQUIRES_LOCK(m_lock);

    Lock m_lock;
    RunLoop::Timer m_collectionTimer WTF_GUARDED_BY_LOCK(m_lock);
    CachedSurfaceMap m_cachedSurfaces WTF_GUARDED_BY_LOCK(m_lock);
    CachedSurfaceQueue m_inUseSurfaces WTF_GUARDED_BY_LOCK(m_lock);
    Vector<IntSize> m_sizesInPruneOrder WTF_GUARDED_BY_LOCK(m_lock);

    size_t m_bytesCached WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    size_t m_inUseBytesCached WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    size_t m_maximumBytesCached WTF_GUARDED_BY_LOCK(m_lock) { defaultMaximumBytesCached };
    const IOSurfacePoolIdentifier m_poolIdentifier { IOSurfacePoolIdentifier::generate() };
};

}
#endif // HAVE(IOSURFACE)
