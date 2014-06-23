 /*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteLayerBackingStoreCollection.h"

#import "PlatformCALayerRemote.h"
#import "RemoteLayerBackingStore.h"
#import "RemoteLayerTreeContext.h"

const std::chrono::seconds volatileBackingStoreAgeThreshold = 1_s;
const std::chrono::milliseconds volatileSecondaryBackingStoreAgeThreshold = 200_ms;
const std::chrono::milliseconds volatilityTimerInterval = 200_ms;

namespace WebKit {

RemoteLayerBackingStoreCollection::RemoteLayerBackingStoreCollection()
    : m_volatilityTimer(this, &RemoteLayerBackingStoreCollection::volatilityTimerFired)
    , m_inLayerFlush(false)
{
}

void RemoteLayerBackingStoreCollection::willFlushLayers()
{
    m_inLayerFlush = true;
    m_reachableBackingStoreInLatestFlush.clear();
}

void RemoteLayerBackingStoreCollection::willCommitLayerTree(RemoteLayerTreeTransaction& transaction)
{
    ASSERT(m_inLayerFlush);
    Vector<WebCore::GraphicsLayer::PlatformLayerID> newlyUnreachableLayerIDs;
    for (auto& backingStore : m_liveBackingStore) {
        if (!m_reachableBackingStoreInLatestFlush.contains(backingStore))
            newlyUnreachableLayerIDs.append(backingStore->layer()->layerID());
    }

    transaction.setLayerIDsWithNewlyUnreachableBackingStore(newlyUnreachableLayerIDs);
}

void RemoteLayerBackingStoreCollection::didFlushLayers()
{
    m_inLayerFlush = false;

    Vector<RemoteLayerBackingStore*> newlyUnreachableBackingStore;
    for (auto& backingStore : m_liveBackingStore) {
        if (!m_reachableBackingStoreInLatestFlush.contains(backingStore))
            newlyUnreachableBackingStore.append(backingStore);
    }

    for (auto& backingStore : newlyUnreachableBackingStore)
        backingStoreBecameUnreachable(*backingStore);

    if (!newlyUnreachableBackingStore.isEmpty())
        scheduleVolatilityTimer();
}

void RemoteLayerBackingStoreCollection::backingStoreWasCreated(RemoteLayerBackingStore& backingStore)
{
    m_liveBackingStore.add(&backingStore);
}

void RemoteLayerBackingStoreCollection::backingStoreWillBeDestroyed(RemoteLayerBackingStore& backingStore)
{
    m_liveBackingStore.remove(&backingStore);
    m_unparentedBackingStore.remove(&backingStore);
}

void RemoteLayerBackingStoreCollection::backingStoreWillBeDisplayed(RemoteLayerBackingStore& backingStore)
{
    ASSERT(m_inLayerFlush);
    m_reachableBackingStoreInLatestFlush.add(&backingStore);

    auto backingStoreIter = m_unparentedBackingStore.find(&backingStore);
    if (backingStoreIter == m_unparentedBackingStore.end())
        return;
    m_liveBackingStore.add(&backingStore);
    m_unparentedBackingStore.remove(backingStoreIter);
}

bool RemoteLayerBackingStoreCollection::markBackingStoreVolatileImmediately(RemoteLayerBackingStore& backingStore, VolatilityMarkingFlags volatilityMarkingFlags)
{
    ASSERT(!m_inLayerFlush);
    bool successfullyMadeBackingStoreVolatile = true;

    if (!backingStore.setBufferVolatility(RemoteLayerBackingStore::BufferType::SecondaryBack, true))
        successfullyMadeBackingStoreVolatile = false;

    if (!backingStore.setBufferVolatility(RemoteLayerBackingStore::BufferType::Back, true))
        successfullyMadeBackingStoreVolatile = false;

    if (!m_reachableBackingStoreInLatestFlush.contains(&backingStore) || (volatilityMarkingFlags & MarkBuffersIgnoringReachability)) {
        if (!backingStore.setBufferVolatility(RemoteLayerBackingStore::BufferType::Front, true))
            successfullyMadeBackingStoreVolatile = false;
    }

    return successfullyMadeBackingStoreVolatile;
}

bool RemoteLayerBackingStoreCollection::markBackingStoreVolatile(RemoteLayerBackingStore& backingStore, std::chrono::steady_clock::time_point now)
{
    if (now - backingStore.lastDisplayTime() < volatileBackingStoreAgeThreshold) {
        if (now - backingStore.lastDisplayTime() >= volatileSecondaryBackingStoreAgeThreshold)
            backingStore.setBufferVolatility(RemoteLayerBackingStore::BufferType::SecondaryBack, true);

        return false;
    }
    
    return markBackingStoreVolatileImmediately(backingStore);
}

void RemoteLayerBackingStoreCollection::backingStoreBecameUnreachable(RemoteLayerBackingStore& backingStore)
{
    ASSERT(backingStore.layer());

    auto backingStoreIter = m_liveBackingStore.find(&backingStore);
    if (backingStoreIter == m_liveBackingStore.end())
        return;
    m_unparentedBackingStore.add(&backingStore);
    m_liveBackingStore.remove(backingStoreIter);

    // If a layer with backing store is removed from the tree, mark it as having changed backing store, so that
    // on the commit which returns it to the tree, we serialize the backing store (despite possibly not painting).
    backingStore.layer()->properties().notePropertiesChanged(RemoteLayerTreeTransaction::BackingStoreChanged);

    // This will not succeed in marking all buffers as volatile, because the commit unparenting the layer hasn't
    // made it to the UI process yet. The volatility timer will finish marking the remaining buffers later.
    markBackingStoreVolatileImmediately(backingStore);
}

bool RemoteLayerBackingStoreCollection::markAllBackingStoreVolatileImmediatelyIfPossible()
{
    bool successfullyMadeBackingStoreVolatile = true;

    for (const auto& backingStore : m_liveBackingStore)
        successfullyMadeBackingStoreVolatile &= markBackingStoreVolatileImmediately(*backingStore, MarkBuffersIgnoringReachability);

    for (const auto& backingStore : m_unparentedBackingStore)
        successfullyMadeBackingStoreVolatile &= markBackingStoreVolatileImmediately(*backingStore, MarkBuffersIgnoringReachability);

    return successfullyMadeBackingStoreVolatile;
}

void RemoteLayerBackingStoreCollection::volatilityTimerFired(WebCore::Timer<RemoteLayerBackingStoreCollection>&)
{
    bool successfullyMadeBackingStoreVolatile = true;

    auto now = std::chrono::steady_clock::now();
    for (const auto& backingStore : m_liveBackingStore)
        successfullyMadeBackingStoreVolatile &= markBackingStoreVolatile(*backingStore, now);

    for (const auto& backingStore : m_unparentedBackingStore)
        successfullyMadeBackingStoreVolatile &= markBackingStoreVolatileImmediately(*backingStore);

    if (successfullyMadeBackingStoreVolatile)
        m_volatilityTimer.stop();
}

void RemoteLayerBackingStoreCollection::scheduleVolatilityTimer()
{
    if (m_volatilityTimer.isActive())
        return;

    m_volatilityTimer.startRepeating(volatilityTimerInterval);
}

} // namespace WebKit
