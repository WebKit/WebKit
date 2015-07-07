/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef LegacyTileLayerPool_h
#define LegacyTileLayerPool_h

#if PLATFORM(IOS)

#include "IntSize.h"
#include "IntSizeHash.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/OwnPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

@class LegacyTileLayer;

namespace WebCore {

class LegacyTileLayerPool {
    WTF_MAKE_NONCOPYABLE(LegacyTileLayerPool);
public:
    static LegacyTileLayerPool* sharedPool();

    void addLayer(const RetainPtr<LegacyTileLayer>&);
    RetainPtr<LegacyTileLayer> takeLayerWithSize(const IntSize&);

    // The maximum size of all queued layers in bytes.
    unsigned capacity() const { return m_capacity; }
    void setCapacity(unsigned);
    void drain();

    unsigned decayedCapacity() const;

    static unsigned bytesBackingLayerWithPixelSize(const IntSize&);

private:
    LegacyTileLayerPool();

    typedef Deque<RetainPtr<LegacyTileLayer> > LayerList;

    bool canReuseLayerWithSize(const IntSize& size) const { return m_capacity && !size.isEmpty(); }
    void schedulePrune();
    void prune();
    typedef enum { LeaveUnchanged, MarkAsUsed } AccessType;
    LayerList& listOfLayersWithSize(const IntSize&, AccessType = LeaveUnchanged);

    HashMap<IntSize, LayerList> m_reuseLists;
    // Ordered by recent use. The last size is the most recently used.
    Vector<IntSize> m_sizesInPruneOrder;
    unsigned m_totalBytes;
    unsigned m_capacity;
    Mutex m_layerPoolMutex;

    double m_lastAddTime;
    bool m_needsPrune;

    friend NeverDestroyed<LegacyTileLayerPool>;
};

} // namespace WebCore

#endif // PLATFORM(IOS)
#endif // TileLayerPool_h
