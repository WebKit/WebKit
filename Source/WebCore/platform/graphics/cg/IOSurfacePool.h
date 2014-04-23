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

#ifndef IOSurfacePool_h
#define IOSurfacePool_h

#include "ColorSpace.h"
#include "IntSize.h"
#include "IntSizeHash.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>

#if USE(IOSURFACE)

namespace WebCore {

class IOSurface;

class IOSurfacePool {
    WTF_MAKE_NONCOPYABLE(IOSurfacePool);
    WTF_MAKE_FAST_ALLOCATED;
    friend class NeverDestroyed<IOSurfacePool>;

public:
    static IOSurfacePool& sharedPool();

    PassRefPtr<IOSurface> takeSurface(IntSize size, ColorSpace colorSpace);
    void addSurface(IOSurface*);

    void discardAllSurfaces();

    void setPoolSize(size_t);

    void showPoolStatistics();

private:
    IOSurfacePool();

    struct CachedSurfaceDetails {
        CachedSurfaceDetails()
            : hasMarkedPurgeable(false)
        { }

        void resetLastUseTime() { lastUseTime = std::chrono::steady_clock::now(); }

        std::chrono::steady_clock::time_point lastUseTime;
        bool hasMarkedPurgeable;
    };

    typedef Deque<RefPtr<IOSurface>> CachedSurfaceQueue;
    typedef HashMap<IntSize, CachedSurfaceQueue> CachedSurfaceMap;
    typedef HashMap<IOSurface*, CachedSurfaceDetails> CachedSurfaceDetailsMap;

    void willAddSurface(IOSurface*, bool inUse);
    void didRemoveSurface(IOSurface*, bool inUse);
    void didUseSurfaceOfSize(IntSize);

    void insertSurfaceIntoPool(IOSurface*);

    void evict(size_t additionalSize);
    void tryEvictInUseSurface();
    void tryEvictOldestCachedSurface();

    void scheduleCollectionTimer();
    void collectionTimerFired(Timer<IOSurfacePool>&);
    void collectInUseSurfaces();
    bool markOlderSurfacesPurgeable();

    void platformGarbageCollectNow();

    Timer<IOSurfacePool> m_collectionTimer;
    CachedSurfaceMap m_cachedSurfaces;
    CachedSurfaceQueue m_inUseSurfaces;
    CachedSurfaceDetailsMap m_surfaceDetails;
    Vector<IntSize> m_sizesInPruneOrder;

    size_t m_bytesCached;
    size_t m_inUseBytesCached;
    size_t m_maximumBytesCached;
};

}
#endif // USE(IOSURFACE)

#endif // IOSurfacePool_h
