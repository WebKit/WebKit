 /*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "RemoteLayerWithRemoteRenderingBackingStoreCollection.h"

#import "ImageBufferBackendHandleSharing.h"
#import "Logging.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerBackingStore.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteRenderingBackendProxy.h"
#import "SwapBuffersDisplayRequirement.h"
#import <wtf/text/TextStream.h>

namespace WebKit {

RemoteLayerWithRemoteRenderingBackingStoreCollection::RemoteLayerWithRemoteRenderingBackingStoreCollection(RemoteLayerTreeContext& layerTreeContext)
    : RemoteLayerBackingStoreCollection(layerTreeContext)
{
}

RemoteRenderingBackendProxy& RemoteLayerWithRemoteRenderingBackingStoreCollection::remoteRenderingBackendProxy()
{
    return layerTreeContext().ensureRemoteRenderingBackendProxy();
}

void RemoteLayerWithRemoteRenderingBackingStoreCollection::prepareBackingStoresForDisplay(RemoteLayerTreeTransaction& transaction)
{
    Vector<RemoteRenderingBackendProxy::LayerPrepareBuffersData> prepareBuffersData;
    prepareBuffersData.reserveInitialCapacity(m_backingStoresNeedingDisplay.computeSize());

    Vector<WeakPtr<RemoteLayerBackingStore>> backingStoreList;
    backingStoreList.reserveInitialCapacity(m_backingStoresNeedingDisplay.computeSize());

    Vector<WeakPtr<RemoteLayerBackingStore>> backingStoresWithNoBuffers;

    for (auto& backingStore : m_backingStoresNeedingDisplay) {
        backingStore.layer()->properties().notePropertiesChanged(LayerChange::BackingStoreChanged);
        transaction.layerPropertiesChanged(*backingStore.layer());

        if (backingStore.performDelegatedLayerDisplay())
            continue;

        if (backingStore.hasNoBuffers()) {
            backingStoresWithNoBuffers.append(backingStore);
            continue;
        }

        prepareBuffersData.append({
            {
                backingStore.bufferForType(RemoteLayerBackingStore::BufferType::Front),
                backingStore.bufferForType(RemoteLayerBackingStore::BufferType::Back),
                backingStore.bufferForType(RemoteLayerBackingStore::BufferType::SecondaryBack)
            },
            backingStore.supportsPartialRepaint(),
            backingStore.hasEmptyDirtyRegion(),
        });
        
        backingStoreList.append(backingStore);
    }

    if (prepareBuffersData.size()) {
        auto& remoteRenderingBackend = layerTreeContext().ensureRemoteRenderingBackendProxy();
        auto swapResult = remoteRenderingBackend.prepareBuffersForDisplay(WTFMove(prepareBuffersData));

        RELEASE_ASSERT(swapResult.size() == backingStoreList.size());
        for (unsigned i = 0; i < swapResult.size(); ++i) {
            auto& backingStoreSwapResult = swapResult[i];
            auto& backingStore = backingStoreList[i];
            backingStore->applySwappedBuffers(WTFMove(backingStoreSwapResult.buffers.front), WTFMove(backingStoreSwapResult.buffers.back), WTFMove(backingStoreSwapResult.buffers.secondaryBack), backingStoreSwapResult.displayRequirement);
        }
    }
    
    for (auto& backingStore : backingStoresWithNoBuffers) {
        backingStore->applySwappedBuffers(nullptr, nullptr, nullptr, SwapBuffersDisplayRequirement::NeedsFullDisplay);
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerWithRemoteRenderingBackingStoreCollection::prepareBackingStoresForDisplay - allocated first buffers for layer " << backingStore->layer()->layerID() << " backing store " << *backingStore);
    }
}

RefPtr<WebCore::ImageBuffer> RemoteLayerWithRemoteRenderingBackingStoreCollection::allocateBufferForBackingStore(const RemoteLayerBackingStore& backingStore)
{
    OptionSet<ImageBufferOptions> options;
    if (backingStore.type() == RemoteLayerBackingStore::Type::IOSurface)
        options.add(WebCore::ImageBufferOptions::Accelerated);
    return remoteRenderingBackendProxy().createImageBuffer(backingStore.size(), WebCore::RenderingPurpose::LayerBacking, backingStore.scale(), backingStore.colorSpace(), backingStore.pixelFormat(), options);
}

bool RemoteLayerWithRemoteRenderingBackingStoreCollection::collectBackingStoreBufferIdentifiersToMarkVolatile(RemoteLayerBackingStore& backingStore, OptionSet<VolatilityMarkingBehavior> markingBehavior, MonotonicTime now, Vector<WebCore::RenderingResourceIdentifier>& identifiers)
{
    ASSERT(!m_inLayerFlush);

    auto collectBuffer = [&](RemoteLayerBackingStore::BufferType bufferType) {
        auto buffer = backingStore.bufferForType(bufferType);
        if (!buffer)
            return;

        if (buffer->volatilityState() != WebCore::VolatilityState::NonVolatile)
            return;

        // Clearing the backend handle in the webcontent process is necessary to have the surface in-use count drop to zero.
        if (auto* backend = buffer->ensureBackendCreated()) {
            auto* sharing = backend->toBackendSharing();
            if (is<ImageBufferBackendHandleSharing>(sharing))
                downcast<ImageBufferBackendHandleSharing>(*sharing).clearBackendHandle();
        }

        identifiers.append(buffer->renderingResourceIdentifier());
    };

    if (markingBehavior.contains(VolatilityMarkingBehavior::ConsiderTimeSinceLastDisplay)) {
        auto timeSinceLastDisplay = now - backingStore.lastDisplayTime();
        if (timeSinceLastDisplay < volatileBackingStoreAgeThreshold) {
            if (timeSinceLastDisplay >= volatileSecondaryBackingStoreAgeThreshold)
                collectBuffer(RemoteLayerBackingStore::BufferType::SecondaryBack);

            return false;
        }
    }

    collectBuffer(RemoteLayerBackingStore::BufferType::SecondaryBack);
    collectBuffer(RemoteLayerBackingStore::BufferType::Back);

    if (!m_reachableBackingStoreInLatestFlush.contains(backingStore) || markingBehavior.contains(VolatilityMarkingBehavior::IgnoreReachability))
        collectBuffer(RemoteLayerBackingStore::BufferType::Front);

    return true;
}

bool RemoteLayerWithRemoteRenderingBackingStoreCollection::collectAllBufferIdentifiersToMarkVolatile(OptionSet<VolatilityMarkingBehavior> liveBackingStoreMarkingBehavior, OptionSet<VolatilityMarkingBehavior> unparentedBackingStoreMarkingBehavior, Vector<WebCore::RenderingResourceIdentifier>& identifiers)
{
    bool completed = true;
    auto now = MonotonicTime::now();

    for (auto& backingStore : m_liveBackingStore)
        completed &= collectBackingStoreBufferIdentifiersToMarkVolatile(backingStore, liveBackingStoreMarkingBehavior, now, identifiers);

    for (auto& backingStore : m_unparentedBackingStore)
        completed &= collectBackingStoreBufferIdentifiersToMarkVolatile(backingStore, unparentedBackingStoreMarkingBehavior, now, identifiers);

    return completed;
}

void RemoteLayerWithRemoteRenderingBackingStoreCollection::markBackingStoreVolatileAfterReachabilityChange(RemoteLayerBackingStore& backingStore)
{
    Vector<WebCore::RenderingResourceIdentifier> identifiers;
    collectBackingStoreBufferIdentifiersToMarkVolatile(backingStore, { }, { }, identifiers);

    if (identifiers.isEmpty())
        return;

    sendMarkBuffersVolatile(WTFMove(identifiers), [weakThis = WeakPtr { *this }](bool succeeded) {
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerWithRemoteRenderingBackingStoreCollection::markBackingStoreVolatileAfterReachabilityChange - succeeded " << succeeded);
        if (!succeeded && weakThis)
            weakThis->scheduleVolatilityTimer();
    });
}

void RemoteLayerWithRemoteRenderingBackingStoreCollection::tryMarkAllBackingStoreVolatile(CompletionHandler<void(bool)>&& completionHandler)
{
    Vector<WebCore::RenderingResourceIdentifier> identifiers;
    bool completed = collectAllBufferIdentifiersToMarkVolatile(VolatilityMarkingBehavior::IgnoreReachability, VolatilityMarkingBehavior::IgnoreReachability, identifiers);

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerWithRemoteRenderingBackingStoreCollection::tryMarkAllBackingStoreVolatile pid " << getpid() << " - live " << m_liveBackingStore.computeSize() << ", unparented " << m_unparentedBackingStore.computeSize() << " " << identifiers);

    if (identifiers.isEmpty()) {
        completionHandler(true);
        return;
    }

    sendMarkBuffersVolatile(WTFMove(identifiers), [completed, completionHandler = WTFMove(completionHandler)](bool succeeded) mutable {
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerWithRemoteRenderingBackingStoreCollection::tryMarkAllBackingStoreVolatile - completed " << completed << ", succeeded " << succeeded);
        completionHandler(completed && succeeded);
    });
}

void RemoteLayerWithRemoteRenderingBackingStoreCollection::markAllBackingStoreVolatileFromTimer()
{
    Vector<WebCore::RenderingResourceIdentifier> identifiers;
    bool completed = collectAllBufferIdentifiersToMarkVolatile(VolatilityMarkingBehavior::ConsiderTimeSinceLastDisplay, { }, identifiers);

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerWithRemoteRenderingBackingStoreCollection::markAllBackingStoreVolatileFromTimer pid " << getpid() << " - live " << m_liveBackingStore.computeSize() << ", unparented " << m_unparentedBackingStore.computeSize() << ", " << identifiers.size() << " buffers to set volatile: " << identifiers);

    if (identifiers.isEmpty()) {
        if (completed)
            m_volatilityTimer.stop();
        return;
    }

    sendMarkBuffersVolatile(WTFMove(identifiers), [completed, weakThis = WeakPtr { *this }](bool succeeded) {
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "sendMarkBuffersVolatile complete - completed " << completed << ", succeeded " << succeeded);
        if (!weakThis)
            return;

        if (completed && succeeded)
            weakThis->m_volatilityTimer.stop();
    });
}

void RemoteLayerWithRemoteRenderingBackingStoreCollection::sendMarkBuffersVolatile(Vector<WebCore::RenderingResourceIdentifier>&& identifiers, CompletionHandler<void(bool)>&& completionHandler)
{
    auto& remoteRenderingBackend = m_layerTreeContext.ensureRemoteRenderingBackendProxy();
    
    remoteRenderingBackend.markSurfacesVolatile(WTFMove(identifiers), [completionHandler = WTFMove(completionHandler)](bool markedAllVolatile) mutable {
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerWithRemoteRenderingBackingStoreCollection::sendMarkBuffersVolatile: marked all volatile " << markedAllVolatile);
        completionHandler(markedAllVolatile);
    });
}

void RemoteLayerWithRemoteRenderingBackingStoreCollection::gpuProcessConnectionWasDestroyed()
{
    for (auto& backingStore : m_liveBackingStore)
        backingStore.setNeedsDisplay();

    for (auto& backingStore : m_unparentedBackingStore)
        backingStore.setNeedsDisplay();
}

} // namespace WebKit
