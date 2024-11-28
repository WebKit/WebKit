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
#import "RemoteImageBufferSetProxy.h"
#import "RemoteLayerBackingStore.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerWithInProcessRenderingBackingStore.h"
#import "RemoteLayerWithRemoteRenderingBackingStore.h"
#import "RemoteRenderingBackendProxy.h"
#import "SwapBuffersDisplayRequirement.h"
#import <WebCore/IOSurfacePool.h>
#import <WebCore/ImageBuffer.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/TextStream.h>

const Seconds volatilityTimerInterval = 200_ms;

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerBackingStoreCollection);

RemoteLayerBackingStoreCollection::RemoteLayerBackingStoreCollection(RemoteLayerTreeContext& layerTreeContext)
    : m_layerTreeContext(layerTreeContext)
    , m_volatilityTimer(*this, &RemoteLayerBackingStoreCollection::volatilityTimerFired)
{
}

RemoteLayerBackingStoreCollection::~RemoteLayerBackingStoreCollection() = default;

void RemoteLayerBackingStoreCollection::ref() const
{
    return m_layerTreeContext->ref();
}

void RemoteLayerBackingStoreCollection::deref() const
{
    return m_layerTreeContext->deref();
}

void RemoteLayerBackingStoreCollection::prepareBackingStoresForDisplay(RemoteLayerTreeTransaction& transaction)
{
    Vector<RemoteRenderingBackendProxy::LayerPrepareBuffersData> prepareBuffersData;
    prepareBuffersData.reserveInitialCapacity(m_backingStoresNeedingDisplay.computeSize());

    Vector<WeakPtr<RemoteLayerWithRemoteRenderingBackingStore>> backingStoreList;
    backingStoreList.reserveInitialCapacity(m_backingStoresNeedingDisplay.computeSize());

    auto& remoteRenderingBackend = layerTreeContext().ensureRemoteRenderingBackendProxy();

    for (CheckedRef backingStore : m_backingStoresNeedingDisplay) {
        backingStore->layer().properties().notePropertiesChanged(LayerChange::BackingStoreChanged);
        transaction.layerPropertiesChanged(backingStore->layer());

        if (CheckedPtr remoteBackingStore = dynamicDowncast<RemoteLayerWithRemoteRenderingBackingStore>(backingStore.get())) {
            if (remoteBackingStore->performDelegatedLayerDisplay())
                continue;

            auto bufferSet = remoteBackingStore->protectedBufferSet();
            if (!bufferSet)
                continue;

            if (!remoteBackingStore->hasFrontBuffer() || !remoteBackingStore->supportsPartialRepaint())
                remoteBackingStore->setNeedsDisplay();

            prepareBuffersData.append({
                bufferSet,
                remoteBackingStore->dirtyRegion(),
                remoteBackingStore->supportsPartialRepaint(),
                remoteBackingStore->hasEmptyDirtyRegion(),
                remoteBackingStore->drawingRequiresClearedPixels(),
            });

            backingStoreList.append(*remoteBackingStore);
        }

        backingStore->prepareToDisplay();
    }

    if (prepareBuffersData.size()) {
        auto swapResult = remoteRenderingBackend.prepareImageBufferSetsForDisplay(WTFMove(prepareBuffersData));
        RELEASE_ASSERT(swapResult.size() == backingStoreList.size() || swapResult.isEmpty());
        for (unsigned i = 0; i < swapResult.size(); ++i) {
            auto& backingStoreSwapResult = swapResult[i];
            auto& backingStore = backingStoreList[i];
            if (backingStoreSwapResult == SwapBuffersDisplayRequirement::NeedsFullDisplay)
                backingStore->setNeedsDisplay();
        }
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
            newlyUnreachableLayerIDs.append(backingStore.layer().layerID());
    }

    transaction.setLayerIDsWithNewlyUnreachableBackingStore(newlyUnreachableLayerIDs);
}

Vector<std::unique_ptr<ThreadSafeImageBufferSetFlusher>> RemoteLayerBackingStoreCollection::didFlushLayers(RemoteLayerTreeTransaction& transaction)
{
    bool needToScheduleVolatilityTimer = false;

    Vector<std::unique_ptr<ThreadSafeImageBufferSetFlusher>> flushers;
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

bool RemoteLayerBackingStoreCollection::backingStoreWillBeDisplayedWithRenderingSuppression(RemoteLayerBackingStore& backingStore)
{
    ASSERT(m_inLayerFlush);
    m_reachableBackingStoreInLatestFlush.add(backingStore);

    auto backingStoreIter = m_unparentedBackingStore.find(backingStore);
    bool wasUnparented = backingStoreIter != m_unparentedBackingStore.end();

    ASSERT_UNUSED(wasUnparented, !wasUnparented);
    return false;
}

void RemoteLayerBackingStoreCollection::purgeFrontBufferForTesting(RemoteLayerBackingStore& backingStore)
{
    if (CheckedPtr remoteBackingStore = dynamicDowncast<RemoteLayerWithRemoteRenderingBackingStore>(&backingStore)) {
        if (RefPtr bufferSet = remoteBackingStore->protectedBufferSet()) {
            Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>> identifiers;
            OptionSet<BufferInSetType> bufferTypes { BufferInSetType::Front };
            identifiers.append(std::make_pair(Ref { *bufferSet }, bufferTypes));
            sendMarkBuffersVolatile(WTFMove(identifiers), [](bool) { }, true);
        }
    } else {
        CheckedRef inProcessBackingStore = downcast<RemoteLayerWithInProcessRenderingBackingStore>(backingStore);
        inProcessBackingStore->setBufferVolatile(RemoteLayerBackingStore::BufferType::Front, true);
    }
}

void RemoteLayerBackingStoreCollection::purgeBackBufferForTesting(RemoteLayerBackingStore& backingStore)
{
    if (CheckedPtr remoteBackingStore = dynamicDowncast<RemoteLayerWithRemoteRenderingBackingStore>(&backingStore)) {
        if (RefPtr bufferSet = remoteBackingStore->protectedBufferSet()) {
            Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>> identifiers;
            OptionSet<BufferInSetType> bufferTypes { BufferInSetType::Back, BufferInSetType::SecondaryBack };
            identifiers.append(std::make_pair(Ref { *bufferSet }, bufferTypes));
            sendMarkBuffersVolatile(WTFMove(identifiers), [](bool) { }, true);
        }
    } else {
        CheckedRef inProcessBackingStore = downcast<RemoteLayerWithInProcessRenderingBackingStore>(backingStore);
        inProcessBackingStore->setBufferVolatile(RemoteLayerBackingStore::BufferType::Back, true);
        inProcessBackingStore->setBufferVolatile(RemoteLayerBackingStore::BufferType::SecondaryBack, true);
    }
}

void RemoteLayerBackingStoreCollection::markFrontBufferVolatileForTesting(RemoteLayerBackingStore& backingStore)
{
    if (CheckedPtr remoteBackingStore = dynamicDowncast<RemoteLayerWithRemoteRenderingBackingStore>(&backingStore)) {
        if (RefPtr bufferSet = remoteBackingStore->protectedBufferSet()) {
            Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>> identifiers;
            OptionSet<BufferInSetType> bufferTypes { BufferInSetType::Front };
            identifiers.append(std::make_pair(Ref { *bufferSet }, bufferTypes));
            sendMarkBuffersVolatile(WTFMove(identifiers), [](bool) { });
        }
    } else {
        CheckedRef inProcessBackingStore = downcast<RemoteLayerWithInProcessRenderingBackingStore>(backingStore);
        inProcessBackingStore->setBufferVolatile(RemoteLayerBackingStore::BufferType::Front, false);
    }
}

bool RemoteLayerBackingStoreCollection::markInProcessBackingStoreVolatile(RemoteLayerWithInProcessRenderingBackingStore& backingStore, OptionSet<VolatilityMarkingBehavior> markingBehavior, MonotonicTime now)
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
    if (CheckedPtr remoteBackingStore = dynamicDowncast<RemoteLayerWithRemoteRenderingBackingStore>(&backingStore)) {
        Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>> identifiers;
        collectRemoteRenderingBackingStoreBufferIdentifiersToMarkVolatile(*remoteBackingStore, { }, { }, identifiers);

        if (identifiers.isEmpty())
            return;

        sendMarkBuffersVolatile(WTFMove(identifiers), [weakThis = WeakPtr { *this }](bool succeeded) {
            LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStoreCollection::markBackingStoreVolatileAfterReachabilityChange - succeeded " << succeeded);
            if (!succeeded && weakThis)
                weakThis->scheduleVolatilityTimer();
        });
    } else {
        CheckedRef inProcessBackingStore = downcast<RemoteLayerWithInProcessRenderingBackingStore>(backingStore);
        markInProcessBackingStoreVolatile(inProcessBackingStore);
    }
}

bool RemoteLayerBackingStoreCollection::markAllBackingStoreVolatile(OptionSet<VolatilityMarkingBehavior> liveBackingStoreMarkingBehavior, OptionSet<VolatilityMarkingBehavior> unparentedBackingStoreMarkingBehavior)
{
    bool successfullyMadeBackingStoreVolatile = true;
    auto now = MonotonicTime::now();

    for (CheckedRef backingStore : m_liveBackingStore) {
        if (CheckedPtr inProcessBackingStore = dynamicDowncast<RemoteLayerWithInProcessRenderingBackingStore>(backingStore.get()))
            successfullyMadeBackingStoreVolatile &= markInProcessBackingStoreVolatile(*inProcessBackingStore, liveBackingStoreMarkingBehavior, now);
    }

    for (CheckedRef backingStore : m_unparentedBackingStore) {
        if (CheckedPtr inProcessBackingStore = dynamicDowncast<RemoteLayerWithInProcessRenderingBackingStore>(backingStore.get()))
            successfullyMadeBackingStoreVolatile &= markInProcessBackingStoreVolatile(*inProcessBackingStore, unparentedBackingStoreMarkingBehavior, now);
    }

    return successfullyMadeBackingStoreVolatile;
}

void RemoteLayerBackingStoreCollection::tryMarkAllBackingStoreVolatile(CompletionHandler<void(bool)>&& completionHandler)
{
    bool successfullyMadeBackingStoreVolatile = markAllBackingStoreVolatile(VolatilityMarkingBehavior::IgnoreReachability, VolatilityMarkingBehavior::IgnoreReachability);

    Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>> identifiers;
    bool collectedAllRemoteRenderingBuffers = collectAllRemoteRenderingBufferIdentifiersToMarkVolatile(VolatilityMarkingBehavior::IgnoreReachability, VolatilityMarkingBehavior::IgnoreReachability, identifiers);

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStoreCollection::tryMarkAllBackingStoreVolatile pid " << getpid() << " - live " << m_liveBackingStore.computeSize() << ", unparented " << m_unparentedBackingStore.computeSize() << " " << identifiers);

    if (identifiers.isEmpty()) {
        completionHandler(successfullyMadeBackingStoreVolatile);
        return;
    }

    sendMarkBuffersVolatile(WTFMove(identifiers), [successfullyMadeBackingStoreVolatile, collectedAllRemoteRenderingBuffers, completionHandler = WTFMove(completionHandler)](bool succeeded) mutable {
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStoreCollection::tryMarkAllBackingStoreVolatile - collectedall " << collectedAllRemoteRenderingBuffers << ", succeeded " << succeeded);
        completionHandler(successfullyMadeBackingStoreVolatile && collectedAllRemoteRenderingBuffers && succeeded);
    });
}

void RemoteLayerBackingStoreCollection::markAllBackingStoreVolatileFromTimer()
{
    bool successfullyMadeBackingStoreVolatile = markAllBackingStoreVolatile(VolatilityMarkingBehavior::ConsiderTimeSinceLastDisplay, { });
    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStoreCollection::markAllBackingStoreVolatileFromTimer() - live " << m_liveBackingStore.computeSize() << ", unparented " << m_unparentedBackingStore.computeSize() << "; successfullyMadeBackingStoreVolatile " << successfullyMadeBackingStoreVolatile);

    Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>> identifiers;
    bool collectedAllRemoteRenderingBuffers = collectAllRemoteRenderingBufferIdentifiersToMarkVolatile(VolatilityMarkingBehavior::ConsiderTimeSinceLastDisplay, { }, identifiers);

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStoreCollection::markAllBackingStoreVolatileFromTimer pid " << getpid() << " - live " << m_liveBackingStore.computeSize() << ", unparented " << m_unparentedBackingStore.computeSize() << ", " << identifiers.size() << " buffers to set volatile: " << identifiers);

    if (identifiers.isEmpty()) {
        if (successfullyMadeBackingStoreVolatile && collectedAllRemoteRenderingBuffers)
            m_volatilityTimer.stop();
        return;
    }

    sendMarkBuffersVolatile(WTFMove(identifiers), [successfullyMadeBackingStoreVolatile, collectedAllRemoteRenderingBuffers, weakThis = WeakPtr { *this }](bool succeeded) {
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "sendMarkBuffersVolatile complete - collectedall " << collectedAllRemoteRenderingBuffers << ", succeeded " << succeeded);
        if (!weakThis)
            return;

        if (successfullyMadeBackingStoreVolatile && collectedAllRemoteRenderingBuffers && succeeded)
            weakThis->m_volatilityTimer.stop();
    });
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

void RemoteLayerBackingStoreCollection::gpuProcessConnectionWasDestroyed()
{
    for (CheckedRef backingStore : m_liveBackingStore) {
        if (is<RemoteLayerWithRemoteRenderingBackingStore>(backingStore))
            backingStore->setNeedsDisplay();
    }

    for (CheckedRef backingStore : m_unparentedBackingStore) {
        if (is<RemoteLayerWithRemoteRenderingBackingStore>(backingStore))
            backingStore->setNeedsDisplay();
    }
}

bool RemoteLayerBackingStoreCollection::collectRemoteRenderingBackingStoreBufferIdentifiersToMarkVolatile(RemoteLayerWithRemoteRenderingBackingStore& backingStore, OptionSet<VolatilityMarkingBehavior> markingBehavior, MonotonicTime now, Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>>& identifiers)
{
    ASSERT(!m_inLayerFlush);
    auto bufferSet = backingStore.protectedBufferSet();
    if (!bufferSet)
        return true;

    if (markingBehavior.contains(VolatilityMarkingBehavior::ConsiderTimeSinceLastDisplay)) {
        auto timeSinceLastDisplay = now - backingStore.lastDisplayTime();
        if (timeSinceLastDisplay < volatileBackingStoreAgeThreshold) {
            if (timeSinceLastDisplay >= volatileSecondaryBackingStoreAgeThreshold && !bufferSet->confirmedVolatility().contains(BufferInSetType::SecondaryBack))
                identifiers.append(std::make_pair(Ref { *bufferSet }, BufferInSetType::SecondaryBack));
            return false;
        }
    }

    OptionSet<BufferInSetType> bufferTypes { BufferInSetType::Back, BufferInSetType::SecondaryBack };

    if (!m_reachableBackingStoreInLatestFlush.contains(backingStore) || markingBehavior.contains(VolatilityMarkingBehavior::IgnoreReachability))
        bufferTypes.add(BufferInSetType::Front);

    if (!(bufferTypes - bufferSet->confirmedVolatility()).isEmpty())
        identifiers.append(std::make_pair(Ref { *bufferSet }, bufferTypes));

    return true;
}

bool RemoteLayerBackingStoreCollection::collectAllRemoteRenderingBufferIdentifiersToMarkVolatile(OptionSet<VolatilityMarkingBehavior> liveBackingStoreMarkingBehavior, OptionSet<VolatilityMarkingBehavior> unparentedBackingStoreMarkingBehavior, Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>>& identifiers)
{
    bool completed = true;
    auto now = MonotonicTime::now();

    for (CheckedRef backingStore : m_liveBackingStore) {
        if (CheckedPtr remoteBackingStore = dynamicDowncast<RemoteLayerWithRemoteRenderingBackingStore>(backingStore.get()))
            completed &= collectRemoteRenderingBackingStoreBufferIdentifiersToMarkVolatile(*remoteBackingStore, liveBackingStoreMarkingBehavior, now, identifiers);
    }

    for (CheckedRef backingStore : m_unparentedBackingStore) {
        if (CheckedPtr remoteBackingStore = dynamicDowncast<RemoteLayerWithRemoteRenderingBackingStore>(backingStore.get()))
            completed &= collectRemoteRenderingBackingStoreBufferIdentifiersToMarkVolatile(*remoteBackingStore, unparentedBackingStoreMarkingBehavior, now, identifiers);
    }

    return completed;
}


void RemoteLayerBackingStoreCollection::sendMarkBuffersVolatile(Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>>&& identifiers, CompletionHandler<void(bool)>&& completionHandler, bool forcePurge)
{
    auto& remoteRenderingBackend = m_layerTreeContext->ensureRemoteRenderingBackendProxy();

    remoteRenderingBackend.markSurfacesVolatile(WTFMove(identifiers), [completionHandler = WTFMove(completionHandler)](bool markedAllVolatile) mutable {
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStoreCollection::sendMarkBuffersVolatile: marked all volatile " << markedAllVolatile);
        completionHandler(markedAllVolatile);
    }, forcePurge);
}

RemoteLayerTreeContext& RemoteLayerBackingStoreCollection::layerTreeContext() const
{
    return m_layerTreeContext.get();
}

} // namespace WebKit
