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

const Seconds volatileBackingStoreAgeThreshold = 1_s;
const Seconds volatileSecondaryBackingStoreAgeThreshold = 200_ms;
const Seconds volatilityTimerInterval = 200_ms;

namespace WebKit {

RemoteLayerBackingStoreCollection::RemoteLayerBackingStoreCollection()
    : m_volatilityTimer(*this, &RemoteLayerBackingStoreCollection::volatilityTimerFired)
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

bool RemoteLayerBackingStoreCollection::backingStoreWillBeDisplayed(RemoteLayerBackingStore& backingStore)
{
    ASSERT(m_inLayerFlush);
    m_reachableBackingStoreInLatestFlush.add(&backingStore);

    auto backingStoreIter = m_unparentedBackingStore.find(&backingStore);
    if (backingStoreIter == m_unparentedBackingStore.end())
        return false;

    m_liveBackingStore.add(&backingStore);
    m_unparentedBackingStore.remove(backingStoreIter);
    return true;
}

bool RemoteLayerBackingStoreCollection::markBackingStoreVolatile(RemoteLayerBackingStore& backingStore, OptionSet<VolatilityMarkingBehavior> markingBehavior, MonotonicTime now)
{
    ASSERT(!m_inLayerFlush);

    if (markingBehavior.contains(VolatilityMarkingBehavior::ConsiderTimeSinceLastDisplay)) {
        auto timeSinceLastDisplay = now - backingStore.lastDisplayTime();
        if (timeSinceLastDisplay < volatileBackingStoreAgeThreshold) {
            if (timeSinceLastDisplay >= volatileSecondaryBackingStoreAgeThreshold)
                backingStore.setBufferVolatility(RemoteLayerBackingStore::BufferType::SecondaryBack, true);

            return false;
        }
    }
    
    bool successfullyMadeBackingStoreVolatile = true;

    if (!backingStore.setBufferVolatility(RemoteLayerBackingStore::BufferType::SecondaryBack, true))
        successfullyMadeBackingStoreVolatile = false;

    if (!backingStore.setBufferVolatility(RemoteLayerBackingStore::BufferType::Back, true))
        successfullyMadeBackingStoreVolatile = false;

    if (!m_reachableBackingStoreInLatestFlush.contains(&backingStore) || markingBehavior.contains(VolatilityMarkingBehavior::IgnoreReachability)) {
        if (!backingStore.setBufferVolatility(RemoteLayerBackingStore::BufferType::Front, true))
            successfullyMadeBackingStoreVolatile = false;
    }

    return successfullyMadeBackingStoreVolatile;
}

void RemoteLayerBackingStoreCollection::backingStoreBecameUnreachable(RemoteLayerBackingStore& backingStore)
{
    ASSERT(backingStore.layer());

    auto backingStoreIter = m_liveBackingStore.find(&backingStore);
    if (backingStoreIter == m_liveBackingStore.end())
        return;

    m_unparentedBackingStore.add(&backingStore);
    m_liveBackingStore.remove(backingStoreIter);

    // This will not succeed in marking all buffers as volatile, because the commit unparenting the layer hasn't
    // made it to the UI process yet. The volatility timer will finish marking the remaining buffers later.
    markBackingStoreVolatile(backingStore);
}

bool RemoteLayerBackingStoreCollection::markAllBackingStoreVolatile(OptionSet<VolatilityMarkingBehavior> liveBackingStoreMarkingBehavior, OptionSet<VolatilityMarkingBehavior> unparentedBackingStoreMarkingBehavior)
{
    bool successfullyMadeBackingStoreVolatile = true;
    auto now = MonotonicTime::now();

    for (const auto& backingStore : m_liveBackingStore)
        successfullyMadeBackingStoreVolatile &= markBackingStoreVolatile(*backingStore, liveBackingStoreMarkingBehavior, now);

    for (const auto& backingStore : m_unparentedBackingStore)
        successfullyMadeBackingStoreVolatile &= markBackingStoreVolatile(*backingStore, unparentedBackingStoreMarkingBehavior, now);

    return successfullyMadeBackingStoreVolatile;
}

void RemoteLayerBackingStoreCollection::tryMarkAllBackingStoreVolatile(CompletionHandler<void(bool)>&& completionHandler)
{
    bool successfullyMadeBackingStoreVolatile = markAllBackingStoreVolatile(VolatilityMarkingBehavior::IgnoreReachability, VolatilityMarkingBehavior::IgnoreReachability);
    completionHandler(successfullyMadeBackingStoreVolatile);
}

void RemoteLayerBackingStoreCollection::volatilityTimerFired()
{
    bool successfullyMadeBackingStoreVolatile = markAllBackingStoreVolatile(VolatilityMarkingBehavior::ConsiderTimeSinceLastDisplay, { });
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
