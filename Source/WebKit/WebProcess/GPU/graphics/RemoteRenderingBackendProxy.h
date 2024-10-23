/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "GPUProcessConnection.h"
#include "IPCSemaphore.h"
#include "ImageBufferBackendHandle.h"
#include "MarkSurfacesAsVolatileRequestIdentifier.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteDisplayListRecorderMessages.h"
#include "RemoteImageBufferProxy.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteResourceCacheProxy.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include "RenderingBackendIdentifier.h"
#include "RenderingUpdateID.h"
#include "StreamClientConnection.h"
#include "ThreadSafeObjectHeap.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/ImageBufferResourceLimits.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/SharedMemory.h>
#include <WebCore/Timer.h>
#include <span>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

class DestinationColorSpace;
class Filter;
class FloatSize;
class PixelBuffer;

enum class AlphaPremultiplication : uint8_t;
enum class RenderingMode : bool;

}

namespace WebKit {

class WebPage;
class RemoteImageBufferProxy;
class RemoteSerializedImageBufferProxy;
class RemoteSharedResourceCacheProxy;
class RemoteLayerBackingStore;

class RemoteImageBufferProxyFlushState;
class RemoteImageBufferSetProxy;

class RemoteRenderingBackendProxy
    : public IPC::Connection::Client, SerialFunctionDispatcher {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteRenderingBackendProxy);
public:
    static std::unique_ptr<RemoteRenderingBackendProxy> create(WebPage&);
    static std::unique_ptr<RemoteRenderingBackendProxy> create(SerialFunctionDispatcher&);

    ~RemoteRenderingBackendProxy();

    static bool canMapRemoteImageBufferBackendBackingStore();

    RemoteResourceCacheProxy& remoteResourceCacheProxy() { return m_remoteResourceCacheProxy; }

    void transferImageBuffer(std::unique_ptr<RemoteSerializedImageBufferProxy>, WebCore::ImageBuffer&);
    void moveToSerializedBuffer(WebCore::RenderingResourceIdentifier);
    void moveToImageBuffer(WebCore::RenderingResourceIdentifier);

    void createRemoteImageBuffer(WebCore::ImageBuffer&);
    bool isCached(const WebCore::ImageBuffer&) const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

    // Messages to be sent.
    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::ImageBufferPixelFormat, OptionSet<WebCore::ImageBufferOptions>);
    void releaseImageBuffer(WebCore::RenderingResourceIdentifier);
    bool getPixelBufferForImageBuffer(WebCore::RenderingResourceIdentifier, const WebCore::PixelBufferFormat& destinationFormat, const WebCore::IntRect& srcRect, std::span<uint8_t> result);
    void putPixelBufferForImageBuffer(WebCore::RenderingResourceIdentifier, const WebCore::PixelBuffer&, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat);
    RefPtr<WebCore::ShareableBitmap> getShareableBitmap(WebCore::RenderingResourceIdentifier, WebCore::PreserveResolution);
    void cacheNativeImage(WebCore::ShareableBitmap::Handle&&, WebCore::RenderingResourceIdentifier);
    void cacheFont(const WebCore::Font::Attributes&, const WebCore::FontPlatformDataAttributes&, std::optional<WebCore::RenderingResourceIdentifier>);
    void cacheFontCustomPlatformData(Ref<const WebCore::FontCustomPlatformData>&&);
    void cacheDecomposedGlyphs(Ref<WebCore::DecomposedGlyphs>&&);
    void cacheGradient(Ref<WebCore::Gradient>&&);
    void cacheFilter(Ref<WebCore::Filter>&&);
    void releaseAllDrawingResources();
    void releaseAllImageResources();
    void releaseRenderingResource(WebCore::RenderingResourceIdentifier);
    void markSurfacesVolatile(Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>>&&, CompletionHandler<void(bool madeAllVolatile)>&&, bool forcePurge);
    RefPtr<RemoteImageBufferSetProxy> createRemoteImageBufferSet();
    void releaseRemoteImageBufferSet(RemoteImageBufferSetProxy&);
    void getImageBufferResourceLimitsForTesting(CompletionHandler<void(WebCore::ImageBufferResourceLimits)>&&);

#if USE(GRAPHICS_LAYER_WC)
    Function<bool()> flushImageBuffers();
#endif

    std::unique_ptr<RemoteDisplayListRecorderProxy> createDisplayListRecorder(WebCore::RenderingResourceIdentifier, const WebCore::FloatSize&, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::ImageBufferPixelFormat, OptionSet<WebCore::ImageBufferOptions>);

    struct BufferSet {
        RefPtr<WebCore::ImageBuffer> front;
        RefPtr<WebCore::ImageBuffer> back;
        RefPtr<WebCore::ImageBuffer> secondaryBack;
    };

    struct LayerPrepareBuffersData {
        RefPtr<RemoteImageBufferSetProxy> bufferSet;
        WebCore::Region dirtyRegion;
        bool supportsPartialRepaint { true };
        bool hasEmptyDirtyRegion { false };
        bool requiresClearedPixels { true };
    };

#if PLATFORM(COCOA)
    Vector<SwapBuffersDisplayRequirement> prepareImageBufferSetsForDisplay(Vector<LayerPrepareBuffersData>&&);
#endif

    void finalizeRenderingUpdate();
    void didPaintLayers();

    RenderingUpdateID renderingUpdateID() const { return m_renderingUpdateID; }
    unsigned delayedRenderingUpdateCount() const { return m_renderingUpdateID - m_didRenderingUpdateID; }

    RenderingBackendIdentifier renderingBackendIdentifier() const;

    RenderingBackendIdentifier ensureBackendCreated();

    bool isGPUProcessConnectionClosed() const { return !m_connection; }

    void didInitialize(IPC::Semaphore&& wakeUpSemaphore, IPC::Semaphore&& clientWaitSemaphore);

    RefPtr<IPC::StreamClientConnection> connection();

    bool isCurrent() const final;
    Ref<WorkQueue> workQueue() { return m_queue; }

    void didBecomeUnresponsive();

    static constexpr Seconds defaultTimeout = 15_s;
private:
    explicit RemoteRenderingBackendProxy(SerialFunctionDispatcher&);

    template<typename T, typename U, typename V, typename W> auto send(T&& message, ObjectIdentifierGeneric<U, V, W>);
    template<typename T> auto send(T&& message) { return send(std::forward<T>(message), renderingBackendIdentifier()); }
    template<typename T, typename U, typename V, typename W> auto sendSync(T&& message, ObjectIdentifierGeneric<U, V, W>);
    template<typename T> auto sendSync(T&& message) { return sendSync(std::forward<T>(message), renderingBackendIdentifier()); }
    template<typename T, typename C, typename U, typename V, typename W> auto sendWithAsyncReply(T&& message, C&& callback, ObjectIdentifierGeneric<U, V, W>);
    template<typename T, typename C> auto sendWithAsyncReply(T&& message, C&& callback) { return sendWithAsyncReply(std::forward<T>(message), std::forward<C>(callback), renderingBackendIdentifier()); }

    // Connection::Client
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t indexOfObjectFailingDecoding) final { }
    void disconnectGPUProcess();
    void ensureGPUProcessConnection();

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    // Returns std::nullopt if no update is needed or allocation failed.
    // Returns handle if that should be sent to the receiver process.
    std::optional<WebCore::SharedMemory::Handle> updateSharedMemoryForGetPixelBuffer(size_t dataSize);
    void destroyGetPixelBufferSharedMemory();

    // Messages to be received.
    void didCreateImageBufferBackend(ImageBufferBackendHandle&&, WebCore::RenderingResourceIdentifier);
    void didFinalizeRenderingUpdate(RenderingUpdateID didRenderingUpdateID);
    void didMarkLayersAsVolatile(MarkSurfacesAsVolatileRequestIdentifier, Vector<std::pair<RemoteImageBufferSetIdentifier, OptionSet<BufferInSetType>>>, bool didMarkAllLayerAsVolatile);

    // SerialFunctionDispatcher
    void dispatch(Function<void()>&&) final;

    RefPtr<IPC::StreamClientConnection> protectedConnection() const { return m_connection; }

    ThreadSafeWeakPtr<SerialFunctionDispatcher> m_dispatcher;
    WeakPtr<GPUProcessConnection> m_gpuProcessConnection; // Only for main thread operation.
    RefPtr<IPC::StreamClientConnection> m_connection;
    RefPtr<RemoteSharedResourceCacheProxy> m_sharedResourceCache;
    RenderingBackendIdentifier m_identifier { RenderingBackendIdentifier::generate() };
    RemoteResourceCacheProxy m_remoteResourceCacheProxy { *this };
    RefPtr<WebCore::SharedMemory> m_getPixelBufferSharedMemory;
    WebCore::Timer m_destroyGetPixelBufferSharedMemoryTimer { *this, &RemoteRenderingBackendProxy::destroyGetPixelBufferSharedMemory };
    HashMap<MarkSurfacesAsVolatileRequestIdentifier, CompletionHandler<void(bool)>> m_markAsVolatileRequests;
    HashMap<RemoteImageBufferSetIdentifier, WeakPtr<RemoteImageBufferSetProxy>> m_bufferSets;
    Ref<WorkQueue> m_queue;

    RenderingUpdateID m_renderingUpdateID;
    RenderingUpdateID m_didRenderingUpdateID;
    bool m_isResponsive { true };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
