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

#import "Logging.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteRenderingBackendProxy.h"
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

void RemoteLayerWithRemoteRenderingBackingStoreCollection::makeFrontBufferNonVolatile(RemoteLayerBackingStore& backingStore)
{
    auto& remoteRenderingBackend = layerTreeContext().ensureRemoteRenderingBackendProxy();
    auto frontBuffer = backingStore.bufferForType(RemoteLayerBackingStore::BufferType::Front);
    if (!frontBuffer)
        return;

    auto result = remoteRenderingBackend.markSurfaceNonVolatile(frontBuffer->renderingResourceIdentifier());
    backingStore.didMakeFrontBufferNonVolatile(result);
}

void RemoteLayerWithRemoteRenderingBackingStoreCollection::swapToValidFrontBuffer(RemoteLayerBackingStore& backingStore)
{
    auto& remoteRenderingBackend = layerTreeContext().ensureRemoteRenderingBackendProxy();

    auto identifiers = RemoteRenderingBackendProxy::BufferSet {
        backingStore.bufferForType(RemoteLayerBackingStore::BufferType::Front),
        backingStore.bufferForType(RemoteLayerBackingStore::BufferType::Back),
        backingStore.bufferForType(RemoteLayerBackingStore::BufferType::SecondaryBack)
    };
    auto swapResult = remoteRenderingBackend.swapToValidFrontBuffer(WTFMove(identifiers));
    backingStore.applySwappedBuffers(WTFMove(swapResult.buffers.front), WTFMove(swapResult.buffers.back), WTFMove(swapResult.buffers.secondaryBack), swapResult.frontBufferWasEmpty);
}

RefPtr<WebCore::ImageBuffer> RemoteLayerWithRemoteRenderingBackingStoreCollection::allocateBufferForBackingStore(const RemoteLayerBackingStore& backingStore)
{
    auto renderingMode = backingStore.type() == RemoteLayerBackingStore::Type::IOSurface ? WebCore::RenderingMode::Accelerated : WebCore::RenderingMode::Unaccelerated;
    return remoteRenderingBackendProxy().createImageBuffer(backingStore.size(), renderingMode, backingStore.scale(), WebCore::DestinationColorSpace::SRGB(), backingStore.pixelFormat());
}

bool RemoteLayerWithRemoteRenderingBackingStoreCollection::collectBackingStoreBufferIdentifiersToMarkVolatile(RemoteLayerBackingStore& backingStore, OptionSet<VolatilityMarkingBehavior> markingBehavior, MonotonicTime now, Vector<WebCore::RenderingResourceIdentifier>& identifiers)
{
    ASSERT(!m_inLayerFlush);

    // FIXME: This doesn't consult RemoteLayerBackingStore::Buffer::isVolatile, so we may redundantly request volatility.
    auto collectBuffer = [&](RemoteLayerBackingStore::BufferType bufferType) {
        auto buffer = backingStore.bufferForType(bufferType);
        if (!buffer)
            return;

        backingStore.willMakeBufferVolatile(bufferType);
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

    if (!m_reachableBackingStoreInLatestFlush.contains(&backingStore) || markingBehavior.contains(VolatilityMarkingBehavior::IgnoreReachability))
        collectBuffer(RemoteLayerBackingStore::BufferType::Front);

    return true;
}

bool RemoteLayerWithRemoteRenderingBackingStoreCollection::collectAllBufferIdentifiersToMarkVolatile(OptionSet<VolatilityMarkingBehavior> liveBackingStoreMarkingBehavior, OptionSet<VolatilityMarkingBehavior> unparentedBackingStoreMarkingBehavior, Vector<WebCore::RenderingResourceIdentifier>& identifiers)
{
    bool completed = true;
    auto now = MonotonicTime::now();

    for (const auto& backingStore : m_liveBackingStore)
        completed &= collectBackingStoreBufferIdentifiersToMarkVolatile(*backingStore, liveBackingStoreMarkingBehavior, now, identifiers);

    for (const auto& backingStore : m_unparentedBackingStore)
        completed &= collectBackingStoreBufferIdentifiersToMarkVolatile(*backingStore, unparentedBackingStoreMarkingBehavior, now, identifiers);

    return completed;
}

void RemoteLayerWithRemoteRenderingBackingStoreCollection::markBackingStoreVolatileAfterReachabilityChange(RemoteLayerBackingStore& backingStore)
{
    Vector<WebCore::RenderingResourceIdentifier> identifiers;
    collectBackingStoreBufferIdentifiersToMarkVolatile(backingStore, { }, { }, identifiers);

    if (identifiers.isEmpty())
        return;

    sendMarkBuffersVolatile(WTFMove(identifiers), [](bool succeeded) {
        LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerWithRemoteRenderingBackingStoreCollection::markBackingStoreVolatileAfterReachabilityChange - succeeded " << succeeded);
    });
}

void RemoteLayerWithRemoteRenderingBackingStoreCollection::tryMarkAllBackingStoreVolatile(CompletionHandler<void(bool)>&& completionHandler)
{
    Vector<WebCore::RenderingResourceIdentifier> identifiers;
    bool completed = collectAllBufferIdentifiersToMarkVolatile(VolatilityMarkingBehavior::IgnoreReachability, VolatilityMarkingBehavior::IgnoreReachability, identifiers);

    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerWithRemoteRenderingBackingStoreCollection::tryMarkAllBackingStoreVolatile pid " << getpid() << " - live " << m_liveBackingStore.size() << ", unparented " << m_unparentedBackingStore.size() << " " << identifiers);

    if (identifiers.isEmpty()) {
        completionHandler(true);
        return;
    }

    sendMarkBuffersVolatile(WTFMove(identifiers), [completed, completionHandler = WTFMove(completionHandler)](bool succeeded) mutable {
        LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerWithRemoteRenderingBackingStoreCollection::tryMarkAllBackingStoreVolatile - completed " << completed << ", succeeded " << succeeded);
        completionHandler(completed && succeeded);
    });
}

void RemoteLayerWithRemoteRenderingBackingStoreCollection::markAllBackingStoreVolatileFromTimer()
{
    Vector<WebCore::RenderingResourceIdentifier> identifiers;
    bool completed = collectAllBufferIdentifiersToMarkVolatile(VolatilityMarkingBehavior::ConsiderTimeSinceLastDisplay, { }, identifiers);

    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerWithRemoteRenderingBackingStoreCollection::markAllBackingStoreVolatileFromTimer pid " << getpid() << " - live " << m_liveBackingStore.size() << ", unparented " << m_unparentedBackingStore.size() << ", " << identifiers.size() << " buffers to set volatile: " << identifiers);

    if (identifiers.isEmpty()) {
        if (completed)
            m_volatilityTimer.stop();
        return;
    }

    sendMarkBuffersVolatile(WTFMove(identifiers), [completed, weakThis = WeakPtr { *this }](bool succeeded) {
        LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "sendMarkBuffersVolatile complete - completed " << completed << ", succeeded " << succeeded);
        if (!weakThis)
            return;

        if (completed && succeeded)
            weakThis->m_volatilityTimer.stop();
    });
}

void RemoteLayerWithRemoteRenderingBackingStoreCollection::sendMarkBuffersVolatile(Vector<WebCore::RenderingResourceIdentifier>&& identifiers, CompletionHandler<void(bool)>&& completionHandler)
{
    auto& remoteRenderingBackend = m_layerTreeContext.ensureRemoteRenderingBackendProxy();
    
    Vector<WebCore::RenderingResourceIdentifier> inUseBufferIdentifiers;
    remoteRenderingBackend.markSurfacesVolatile(WTFMove(identifiers), [&inUseBufferIdentifiers](Vector<WebCore::RenderingResourceIdentifier>&& inUseBuffers) {
        inUseBufferIdentifiers = WTFMove(inUseBuffers);
    });

    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "RemoteLayerWithRemoteRenderingBackingStoreCollection::sendMarkBuffersVolatile: " << inUseBufferIdentifiers.size() << " buffers still in-use");
    // FIXME: inUseBufferIdentifiers will be used to map back to an ImageBuffer and maintain its "isVolatile" flag.
    completionHandler(inUseBufferIdentifiers.isEmpty());
}

} // namespace WebKit
