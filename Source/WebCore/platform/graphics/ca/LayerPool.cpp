/*
 * Copyright (C) 2012, 2014 Apple Inc. All rights reserved.
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
#include "LayerPool.h"

#include "Logging.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static constexpr Seconds capacityDecayTime { 5_s };

LayerPool::LayerPool()
    : m_maxBytesForPool(48 * 1024 * 1024)
    , m_pruneTimer(*this, &LayerPool::pruneTimerFired)
{
    RELEASE_ASSERT(isMainThread());
    allLayerPools().add(this);
}

LayerPool::~LayerPool()
{
    RELEASE_ASSERT(isMainThread());
    allLayerPools().remove(this);
}

HashSet<LayerPool*>& LayerPool::allLayerPools()
{
    RELEASE_ASSERT(isMainThread());
    static NeverDestroyed<HashSet<LayerPool*>> allLayerPools;
    return allLayerPools.get();
}

unsigned LayerPool::backingStoreBytesForSize(const IntSize& size)
{
    return size.area() * 4;
}

LayerPool::LayerList& LayerPool::listOfLayersWithSize(const IntSize& size, AccessType accessType)
{
    HashMap<IntSize, LayerList>::iterator it = m_reuseLists.find(size);
    if (it == m_reuseLists.end()) {
        it = m_reuseLists.add(size, LayerList()).iterator;
        m_sizesInPruneOrder.append(size);
    } else if (accessType == MarkAsUsed) {
        m_sizesInPruneOrder.remove(m_sizesInPruneOrder.reverseFind(size));
        m_sizesInPruneOrder.append(size);
    }
    return it->value;
}

void LayerPool::addLayer(const RefPtr<PlatformCALayer>& layer)
{
    RELEASE_ASSERT(isMainThread());
    IntSize layerSize(expandedIntSize(layer->bounds().size()));
    if (!canReuseLayerWithSize(layerSize))
        return;

    listOfLayersWithSize(layerSize).prepend(layer);
    m_totalBytes += backingStoreBytesForSize(layerSize);
    
    m_lastAddTime = MonotonicTime::now();
    schedulePrune();
}

RefPtr<PlatformCALayer> LayerPool::takeLayerWithSize(const IntSize& size)
{
    RELEASE_ASSERT(isMainThread());
    if (!canReuseLayerWithSize(size))
        return nullptr;
    LayerList& reuseList = listOfLayersWithSize(size, MarkAsUsed);
    if (reuseList.isEmpty())
        return nullptr;
    m_totalBytes -= backingStoreBytesForSize(size);
    return reuseList.takeFirst();
}
    
unsigned LayerPool::decayedCapacity() const
{
    // Decay to one quarter over capacityDecayTime
    Seconds timeSinceLastAdd = MonotonicTime::now() - m_lastAddTime;
    if (timeSinceLastAdd > capacityDecayTime)
        return m_maxBytesForPool / 4;
    float decayProgess = float(timeSinceLastAdd / capacityDecayTime);
    return m_maxBytesForPool / 4 + m_maxBytesForPool * 3 / 4 * (1 - decayProgess);
}

void LayerPool::schedulePrune()
{
    if (m_pruneTimer.isActive())
        return;
    m_pruneTimer.startOneShot(1_s);
}

void LayerPool::pruneTimerFired()
{
    RELEASE_ASSERT(isMainThread());
    unsigned shrinkTo = decayedCapacity();
    while (m_totalBytes > shrinkTo) {
        RELEASE_ASSERT(!m_sizesInPruneOrder.isEmpty());
        IntSize sizeToDrop = m_sizesInPruneOrder.first();
        auto it = m_reuseLists.find(sizeToDrop);
        RELEASE_ASSERT(it != m_reuseLists.end());
        LayerList& oldestReuseList = it->value;
        if (oldestReuseList.isEmpty()) {
            m_reuseLists.remove(sizeToDrop);
            m_sizesInPruneOrder.remove(0);
            continue;
        }

        ASSERT(m_totalBytes >= backingStoreBytesForSize(sizeToDrop));
        m_totalBytes -= backingStoreBytesForSize(sizeToDrop);
        // The last element in the list is the oldest, hence most likely not to
        // still have a backing store.
        oldestReuseList.removeLast();
    }
    if (MonotonicTime::now() - m_lastAddTime <= capacityDecayTime)
        schedulePrune();
}

void LayerPool::drain()
{
    RELEASE_ASSERT(isMainThread());
    m_reuseLists.clear();
    m_sizesInPruneOrder.clear();
    m_totalBytes = 0;
}

}
