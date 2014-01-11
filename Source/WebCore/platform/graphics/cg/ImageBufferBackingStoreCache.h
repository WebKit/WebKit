/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef ImageBufferBackingStoreCache_h
#define ImageBufferBackingStoreCache_h

#include "ImageBuffer.h"

#include "Timer.h"

#include <wtf/DoublyLinkedList.h>
#include <wtf/HashMap.h>

#if USE(IOSURFACE_CANVAS_BACKING_STORE)

namespace WebCore {

class ImageBufferBackingStoreCache {
    WTF_MAKE_NONCOPYABLE(ImageBufferBackingStoreCache); WTF_MAKE_FAST_ALLOCATED;

public:
    static ImageBufferBackingStoreCache& get();
    
    struct IOSurfaceAndContext {
        IOSurfaceAndContext()
        {
        }

        IOSurfaceAndContext(IOSurfaceRef surface, CGContextRef context)
            : surface(surface)
            , context(context)
        {
        }

        RetainPtr<IOSurfaceRef> surface;
        RetainPtr<CGContextRef> context;
    };

    IOSurfaceAndContext getOrAllocate(IntSize, CGColorSpaceRef, bool needExactSize);
    void deallocate(IOSurfaceRef);

private:
    ImageBufferBackingStoreCache();

    struct IOSurfaceAndContextWithCreationParams : public IOSurfaceAndContext, public DoublyLinkedListNode<IOSurfaceAndContextWithCreationParams> {
        IOSurfaceAndContextWithCreationParams()
        {
        }

        IOSurfaceAndContextWithCreationParams(IOSurfaceRef surface, CGContextRef context, CGColorSpaceRef colorSpace)
            : IOSurfaceAndContext(surface, context)
            , colorSpace(colorSpace)
        {
        }

        IOSurfaceAndContextWithCreationParams* m_prev;
        IOSurfaceAndContextWithCreationParams* m_next;
        RetainPtr<CGColorSpaceRef> colorSpace;
    };
    typedef HashMap<RetainPtr<IOSurfaceRef>, IOSurfaceAndContextWithCreationParams> ActiveSurfaceMap;
    typedef std::pair<int, int> CachedSurfaceKey;
    typedef DoublyLinkedList<IOSurfaceAndContextWithCreationParams> InfoLinkedList;
    typedef HashMap<CachedSurfaceKey, InfoLinkedList> CachedSurfaceMap;

    static CachedSurfaceKey convertSizeToKey(const IntSize& size)
    {
        return std::make_pair(WTF::roundUpToMultipleOf(8, size.width()), WTF::roundUpToMultipleOf(8, size.height()));
    }

    IOSurfaceAndContextWithCreationParams takeFromCache(CachedSurfaceMap::iterator, IOSurfaceAndContextWithCreationParams*);
    void insertIntoCache(IOSurfaceAndContextWithCreationParams&&);

    // If we find an acceptable surface, this function removes it from the cache as
    // well as placing it in the out parameter.
    bool tryTakeFromCache(const IntSize&, CGColorSpaceRef, bool needExactSize, IOSurfaceAndContextWithCreationParams& outInfo);
    bool isAcceptableSurface(const IOSurfaceAndContextWithCreationParams&, const IntSize&, CGColorSpaceRef, bool needExactSize) const;

    void timerFired(DeferrableOneShotTimer<ImageBufferBackingStoreCache>&);
    void schedulePurgeTimer();

    DeferrableOneShotTimer<ImageBufferBackingStoreCache> m_purgeTimer;
    ActiveSurfaceMap m_activeSurfaces;
    CachedSurfaceMap m_cachedSurfaces;
    int m_pixelsCached;
};

}
#endif // IOSURFACE_CANVAS_BACKING_STORE

#endif // ImageBufferBackingStoreCache_h
