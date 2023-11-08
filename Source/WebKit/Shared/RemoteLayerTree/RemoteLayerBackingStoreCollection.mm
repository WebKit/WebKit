 /*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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

#import "ImageBufferShareableBitmapBackend.h"
#import "ImageBufferShareableMappedIOSurfaceBackend.h"
#import "Logging.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerBackingStore.h"
#import "RemoteLayerTreeContext.h"
#import "SwapBuffersDisplayRequirement.h"
#import <WebCore/IOSurfacePool.h>
#import <WebCore/ImageBuffer.h>
#import <wtf/text/TextStream.h>

const Seconds volatilityTimerInterval = 200_ms;

namespace WebKit {

RemoteLayerBackingStoreCollection::RemoteLayerBackingStoreCollection(RemoteLayerTreeContext& layerTreeContext)
    : m_layerTreeContext(layerTreeContext)
    , m_volatilityTimer(*this, &RemoteLayerBackingStoreCollection::volatilityTimerFired)
{
}

RemoteLayerBackingStoreCollection::~RemoteLayerBackingStoreCollection() = default;

void RemoteLayerBackingStoreCollection::prepareBackingStoresForDisplay(RemoteLayerTreeTransaction& transaction)
{
    for (auto& backingStore : m_backingStoresNeedingDisplay) {
        backingStore.prepareToDisplay();
        backingStore.layer()->properties().notePropertiesChanged(LayerChange::BackingStoreChanged);
        transaction.layerPropertiesChanged(*backingStore.layer());
    }
}

bool RemoteLayerBackingStoreCollection::paintReachableBackingStoreContents()
{
    bool anyNonEmptyDirtyRegion = false;
    for (auto& backingStore : m_backingStoresNeedingDisplay) {
        if (!backingStore.hasEmptyDirtyRegion())
            anyNonEmptyDirtyRegion = true;
        backingStore.paintContents();
    }
    return anyNonEmptyDirtyRegion;
}

void RemoteLayerBackingStoreCollection::willFlushLayers()
{
    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "\nRemoteLayerBackingStoreCollection::willFlushLayers()");

    m_inLayerFlush = true;
    m_reachableBackingStoreInLatestFlush.clear();
}

void RemoteLayerBackingStoreCollection::willBuildTransaction()
{
    m_backingStoresNeedingDisplay.clear();
}

void RemoteLayerBackingStoreCollection::willCommitLayerTree(RemoteLayerTreeTransaction& transaction)
{
    ASSERT(m_inLayerFlush);
    Vector<WebCore::PlatformLayerIdentifier> newlyUnreachableLayerIDs;
    for (auto& backingStore : m_liveBackingStore) {
        if (!m_reachableBackingStoreInLatestFlush.contains(backingStore))
            newlyUnreachableLayerIDs.append(backingStore.layer()->layerID());
    }

    transaction.setLayerIDsWithNewlyUnreachableBackingStore(newlyUnreachableLayerIDs);
}

Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> RemoteLayerBackingStoreCollection::didFlushLayers(RemoteLayerTreeTransaction& transaction)
{
    bool needToScheduleVolatilityTimer = false;

    Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> flushers;
    for (auto& layer : transaction.changedLayers()) {
        if (layer->properties().changedProperties & LayerChange::BackingStoreChanged) {
            needToScheduleVolatilityTimer = true;
            if (layer->properties().backingStoreOrProperties.store)
                flushers.appendVector(layer->properties().backingStoreOrProperties.store->takePendingFlushers());
        }

        layer->didCommit();
    }

    m_inLayerFlush = false;

    if (updateUnreachableBackingStores())
        needToScheduleVolatilityTimer = true;

    if (needToScheduleVolatilityTimer)
        scheduleVolatilityTimer();

    return flushers;
}

bool RemoteLayerBackingStoreCollection::updateUnreachableBackingStores()
{
    Vector<WeakPtr<RemoteLayerBackingStore>> newlyUnreachableBackingStore;
    for (auto& backingStore : m_liveBackingStore) {
        if (!m_reachableBackingStoreInLatestFlush.contains(backingStore))
            newlyUnreachableBackingStore.append(backingStore);
    }

    for (auto& backingStore : newlyUnreachableBackingStore)
        backingStoreBecameUnreachable(*backingStore);

    return !newlyUnreachableBackingStore.isEmpty();
}

void RemoteLayerBackingStoreCollection::backingStoreWasCreated(RemoteLayerBackingStore& backingStore)
{
    m_liveBackingStore.add(backingStore);
}

void RemoteLayerBackingStoreCollection::backingStoreWillBeDestroyed(RemoteLayerBackingStore& backingStore)
{
    m_liveBackingStore.remove(backingStore);
    m_unparentedBackingStore.remove(backingStore);
}

bool RemoteLayerBackingStoreCollection::backingStoreWillBeDisplayed(RemoteLayerBackingStore& backingStore)
{
    ASSERT(m_inLayerFlush);
    m_reachableBackingStoreInLatestFlush.add(backingStore);

    auto backingStoreIter = m_unparentedBackingStore.find(backingStore);
    bool wasUnparented = backingStoreIter != m_unparentedBackingStore.end();

    if (backingStore.needsDisplay() || wasUnparented)
        m_backingStoresNeedingDisplay.add(backingStore);

    if (!wasUnparented)
        return false;

    m_liveBackingStore.add(backingStore);
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
                backingStore.setBufferVolatile(RemoteLayerBackingStore::BufferType::SecondaryBack);

            return false;
        }
    }
    
    bool successfullyMadeBackingStoreVolatile = true;

    if (!backingStore.setBufferVolatile(RemoteLayerBackingStore::BufferType::SecondaryBack))
        successfullyMadeBackingStoreVolatile = false;

    if (!backingStore.setBufferVolatile(RemoteLayerBackingStore::BufferType::Back))
        successfullyMadeBackingStoreVolatile = false;

    if (!m_reachableBackingStoreInLatestFlush.contains(backingStore) || markingBehavior.contains(VolatilityMarkingBehavior::IgnoreReachability)) {
        if (!backingStore.setBufferVolatile(RemoteLayerBackingStore::BufferType::Front))
            successfullyMadeBackingStoreVolatile = false;
    }

    return successfullyMadeBackingStoreVolatile;
}

void RemoteLayerBackingStoreCollection::backingStoreBecameUnreachable(RemoteLayerBackingStore& backingStore)
{
    ASSERT(backingStore.layer());

    auto backingStoreIter = m_liveBackingStore.find(backingStore);
    if (backingStoreIter == m_liveBackingStore.end())
        return;

    m_unparentedBackingStore.add(backingStore);
    m_liveBackingStore.remove(backingStoreIter);

    // This will not succeed in marking all buffers as volatile, because the commit unparenting the layer hasn't
    // made it to the UI process yet. The volatility timer will finish marking the remaining buffers later.
    markBackingStoreVolatileAfterReachabilityChange(backingStore);
}

void RemoteLayerBackingStoreCollection::markBackingStoreVolatileAfterReachabilityChange(RemoteLayerBackingStore& backingStore)
{
    markBackingStoreVolatile(backingStore);
}

bool RemoteLayerBackingStoreCollection::markAllBackingStoreVolatile(OptionSet<VolatilityMarkingBehavior> liveBackingStoreMarkingBehavior, OptionSet<VolatilityMarkingBehavior> unparentedBackingStoreMarkingBehavior)
{
    bool successfullyMadeBackingStoreVolatile = true;
    auto now = MonotonicTime::now();

    for (auto& backingStore : m_liveBackingStore)
        successfullyMadeBackingStoreVolatile &= markBackingStoreVolatile(backingStore, liveBackingStoreMarkingBehavior, now);

    for (auto& backingStore : m_unparentedBackingStore)
        successfullyMadeBackingStoreVolatile &= markBackingStoreVolatile(backingStore, unparentedBackingStoreMarkingBehavior, now);

    return successfullyMadeBackingStoreVolatile;
}

void RemoteLayerBackingStoreCollection::tryMarkAllBackingStoreVolatile(CompletionHandler<void(bool)>&& completionHandler)
{
    bool successfullyMadeBackingStoreVolatile = markAllBackingStoreVolatile(VolatilityMarkingBehavior::IgnoreReachability, VolatilityMarkingBehavior::IgnoreReachability);
    completionHandler(successfullyMadeBackingStoreVolatile);
}

void RemoteLayerBackingStoreCollection::markAllBackingStoreVolatileFromTimer()
{
    bool successfullyMadeBackingStoreVolatile = markAllBackingStoreVolatile(VolatilityMarkingBehavior::ConsiderTimeSinceLastDisplay, { });
    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStoreCollection::markAllBackingStoreVolatileFromTimer() - live " << m_liveBackingStore.computeSize() << ", unparented " << m_unparentedBackingStore.computeSize() << "; successfullyMadeBackingStoreVolatile " << successfullyMadeBackingStoreVolatile);

    if (successfullyMadeBackingStoreVolatile)
        m_volatilityTimer.stop();
}

void RemoteLayerBackingStoreCollection::volatilityTimerFired()
{
    markAllBackingStoreVolatileFromTimer();
}

void RemoteLayerBackingStoreCollection::scheduleVolatilityTimer()
{
    if (m_volatilityTimer.isActive())
        return;

    m_volatilityTimer.startRepeating(volatilityTimerInterval);
}

RefPtr<WebCore::ImageBuffer> RemoteLayerBackingStoreCollection::allocateBufferForBackingStore(const RemoteLayerBackingStore& backingStore)
{
    switch (backingStore.type()) {
    case RemoteLayerBackingStore::Type::IOSurface: {
        ImageBufferCreationContext creationContext;
        creationContext.surfacePool = &WebCore::IOSurfacePool::sharedPool();
        return WebCore::ImageBuffer::create<ImageBufferShareableMappedIOSurfaceBackend>(backingStore.size(), backingStore.scale(), backingStore.colorSpace(), backingStore.pixelFormat(), WebCore::RenderingPurpose::LayerBacking, creationContext);
    }
    case RemoteLayerBackingStore::Type::Bitmap:
        return WebCore::ImageBuffer::create<ImageBufferShareableBitmapBackend>(backingStore.size(), backingStore.scale(), backingStore.colorSpace(), backingStore.pixelFormat(), WebCore::RenderingPurpose::LayerBacking, { });
    }
    return nullptr;
}

} // namespace WebKit
