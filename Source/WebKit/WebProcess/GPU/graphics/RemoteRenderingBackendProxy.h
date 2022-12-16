/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "RemoteRenderingBackendCreationParameters.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteResourceCacheProxy.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include "RenderingBackendIdentifier.h"
#include "RenderingUpdateID.h"
#include "SharedMemory.h"
#include "StreamClientConnection.h"
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/Timer.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/Span.h>
#include <wtf/WeakPtr.h>

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

class RemoteImageBufferProxyFlushState;

class RemoteRenderingBackendProxy
    : public IPC::Connection::Client {
public:
    static std::unique_ptr<RemoteRenderingBackendProxy> create(WebPage&);
    static std::unique_ptr<RemoteRenderingBackendProxy> create(const RemoteRenderingBackendCreationParameters&, SerialFunctionDispatcher&);

    ~RemoteRenderingBackendProxy();

    RemoteResourceCacheProxy& remoteResourceCacheProxy() { return m_remoteResourceCacheProxy; }

    void transferImageBuffer(std::unique_ptr<RemoteSerializedImageBufferProxy>, WebCore::ImageBuffer&);
    void moveToSerializedBuffer(WebCore::RenderingResourceIdentifier, RemoteSerializedImageBufferWriteReference&&);
    void moveToImageBuffer(RemoteSerializedImageBufferWriteReference&&, WebCore::RenderingResourceIdentifier);

    void createRemoteImageBuffer(WebCore::ImageBuffer&);
    bool isCached(const WebCore::ImageBuffer&) const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Messages to be sent.
    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::PixelFormat, bool avoidBackendSizeCheck = false);
    bool getPixelBufferForImageBuffer(WebCore::RenderingResourceIdentifier, const WebCore::PixelBufferFormat& destinationFormat, const WebCore::IntRect& srcRect, Span<uint8_t> result);
    void putPixelBufferForImageBuffer(WebCore::RenderingResourceIdentifier, const WebCore::PixelBuffer&, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat);
    RefPtr<ShareableBitmap> getShareableBitmap(WebCore::RenderingResourceIdentifier, WebCore::PreserveResolution);
    RefPtr<WebCore::Image> getFilteredImage(WebCore::RenderingResourceIdentifier, WebCore::Filter&);
    void cacheNativeImage(const ShareableBitmapHandle&, WebCore::RenderingResourceIdentifier);
    void cacheFont(Ref<WebCore::Font>&&);
    void cacheDecomposedGlyphs(Ref<WebCore::DecomposedGlyphs>&&);
    void releaseAllRemoteResources();
    void releaseRemoteResource(WebCore::RenderingResourceIdentifier);
    void markSurfacesVolatile(Vector<WebCore::RenderingResourceIdentifier>&&, CompletionHandler<void(bool madeAllVolatile)>&&);

    struct BufferSet {
        RefPtr<WebCore::ImageBuffer> front;
        RefPtr<WebCore::ImageBuffer> back;
        RefPtr<WebCore::ImageBuffer> secondaryBack;
    };
    
    struct LayerPrepareBuffersData {
        BufferSet buffers;
        bool supportsPartialRepaint { true };
        bool hasEmptyDirtyRegion { false };
    };
    
    struct SwapBuffersResult {
        BufferSet buffers;
        SwapBuffersDisplayRequirement displayRequirement;
    };

    Vector<SwapBuffersResult> prepareBuffersForDisplay(const Vector<LayerPrepareBuffersData>&);

    void finalizeRenderingUpdate();
    void didPaintLayers();

    RenderingUpdateID renderingUpdateID() const { return m_renderingUpdateID; }
    unsigned delayedRenderingUpdateCount() const { return m_renderingUpdateID - m_didRenderingUpdateID; }

    enum class DidReceiveBackendCreationResult : bool {
        ReceivedAnyResponse,
        TimeoutOrIPCFailure
    };
    DidReceiveBackendCreationResult waitForDidCreateImageBufferBackend();
    bool waitForDidFlush();

    RenderingBackendIdentifier renderingBackendIdentifier() const;

    RenderingBackendIdentifier ensureBackendCreated();

    bool isGPUProcessConnectionClosed() const { return !m_streamConnection; }

    void didInitialize(IPC::Semaphore&& wakeUpSemaphore, IPC::Semaphore&& clientWaitSemaphore);

    IPC::StreamClientConnection& streamConnection();
    IPC::Connection* connection() { return m_connection.get(); }

    template<typename T, typename C>
    void sendToStreamWithAsyncReply(T&& message, C&& completionHandler)
    {
        streamConnection().sendWithAsyncReply(WTFMove(message), WTFMove(completionHandler), renderingBackendIdentifier(), Seconds::infinity());
    }

    SerialFunctionDispatcher& dispatcher() { return m_dispatcher; }

    void addPendingFlush(RemoteImageBufferProxyFlushState&, DisplayListRecorderFlushIdentifier);

private:
    explicit RemoteRenderingBackendProxy(const RemoteRenderingBackendCreationParameters&, SerialFunctionDispatcher&);

    template<typename T>
    void send(T&& message)
    {
        streamConnection().send(WTFMove(message), renderingBackendIdentifier(), Seconds::infinity());

    }

    template<typename T>
    auto sendSync(T&& message, IPC::Timeout timeout = Seconds::infinity())
    {
        return streamConnection().sendSync(WTFMove(message), renderingBackendIdentifier(), timeout);
    }

    // Connection::Client
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) final { }
    void disconnectGPUProcess();
    void ensureGPUProcessConnection();

    // Returns std::nullopt if no update is needed or allocation failed.
    // Returns handle if that should be sent to the receiver process.
    std::optional<SharedMemory::Handle> updateSharedMemoryForGetPixelBuffer(size_t dataSize);
    void destroyGetPixelBufferSharedMemory();

    // Messages to be received.
    void didCreateImageBufferBackend(ImageBufferBackendHandle&&, WebCore::RenderingResourceIdentifier);
    void didFlush(DisplayListRecorderFlushIdentifier);
    void didFinalizeRenderingUpdate(RenderingUpdateID didRenderingUpdateID);
    void didMarkLayersAsVolatile(MarkSurfacesAsVolatileRequestIdentifier, const Vector<WebCore::RenderingResourceIdentifier>& markedVolatileBufferIdentifiers, bool didMarkAllLayerAsVolatile);

    RefPtr<IPC::Connection> m_connection;
    RefPtr<IPC::StreamClientConnection> m_streamConnection;
    RemoteRenderingBackendCreationParameters m_parameters;
    RemoteResourceCacheProxy m_remoteResourceCacheProxy { *this };
    RefPtr<SharedMemory> m_getPixelBufferSharedMemory;
    WebCore::Timer m_destroyGetPixelBufferSharedMemoryTimer { *this, &RemoteRenderingBackendProxy::destroyGetPixelBufferSharedMemory };
    HashMap<MarkSurfacesAsVolatileRequestIdentifier, CompletionHandler<void(bool)>> m_markAsVolatileRequests;
    SerialFunctionDispatcher& m_dispatcher;

    RenderingUpdateID m_renderingUpdateID;
    RenderingUpdateID m_didRenderingUpdateID;
    HashMap<DisplayListRecorderFlushIdentifier, Ref<RemoteImageBufferProxyFlushState>> m_pendingFlushes;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
