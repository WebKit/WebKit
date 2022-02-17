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
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteDisplayListRecorderMessages.h"
#include "RemoteRenderingBackendCreationParameters.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteResourceCacheProxy.h"
#include "RenderingBackendIdentifier.h"
#include "RenderingUpdateID.h"
#include "SharedMemory.h"
#include "StreamClientConnection.h"
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/Timer.h>
#include <wtf/Deque.h>
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

class RemoteRenderingBackendProxy
    : private IPC::MessageReceiver
    , public GPUProcessConnection::Client {
public:
    static std::unique_ptr<RemoteRenderingBackendProxy> create(WebPage&);

    ~RemoteRenderingBackendProxy();

    using GPUProcessConnection::Client::weakPtrFactory;
    using WeakValueType = GPUProcessConnection::Client::WeakValueType;

    RemoteResourceCacheProxy& remoteResourceCacheProxy() { return m_remoteResourceCacheProxy; }

    void createRemoteImageBuffer(WebCore::ImageBuffer&);
    bool isCached(const WebCore::ImageBuffer&) const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Messages to be sent.
    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::RenderingMode, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::PixelFormat);
    bool getPixelBufferForImageBuffer(WebCore::RenderingResourceIdentifier, const WebCore::PixelBufferFormat& destinationFormat, const WebCore::IntRect& srcRect, Span<uint8_t> result);
    void putPixelBufferForImageBuffer(WebCore::RenderingResourceIdentifier, const WebCore::PixelBuffer&, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat);
    String getDataURLForImageBuffer(const String& mimeType, std::optional<double> quality, WebCore::PreserveResolution, WebCore::RenderingResourceIdentifier);
    Vector<uint8_t> getDataForImageBuffer(const String& mimeType, std::optional<double> quality, WebCore::RenderingResourceIdentifier);
    RefPtr<ShareableBitmap> getShareableBitmap(WebCore::RenderingResourceIdentifier, WebCore::PreserveResolution);
    RefPtr<WebCore::Image> getFilteredImage(WebCore::RenderingResourceIdentifier, WebCore::Filter&);
    void cacheNativeImage(const ShareableBitmap::Handle&, WebCore::RenderingResourceIdentifier);
    void cacheFont(Ref<WebCore::Font>&&);
    void deleteAllFonts();
    void releaseRemoteResource(WebCore::RenderingResourceIdentifier);
    void markSurfacesVolatile(Vector<WebCore::RenderingResourceIdentifier>&&, CompletionHandler<void(Vector<WebCore::RenderingResourceIdentifier>&& inUseBufferIdentifiers)>&&);

    WebCore::VolatilityState markSurfaceNonVolatile(WebCore::RenderingResourceIdentifier);

    struct BufferSet {
        RefPtr<WebCore::ImageBuffer> front;
        RefPtr<WebCore::ImageBuffer> back;
        RefPtr<WebCore::ImageBuffer> secondaryBack;
    };
    
    struct SwapBuffersResult {
        BufferSet buffers;
        bool frontBufferWasEmpty { false };
    };
    SwapBuffersResult swapToValidFrontBuffer(const BufferSet&);

    void finalizeRenderingUpdate();
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

    bool isGPUProcessConnectionClosed() const { return !m_gpuProcessConnection; }

    void didCreateWakeUpSemaphoreForDisplayListStream(IPC::Semaphore&&);

    template<typename T, typename U>
    void sendToStream(T&& message, ObjectIdentifier<U> identifier)
    {
        // FIXME: We should consider making the send timeout finite.
        streamConnection().send(WTFMove(message), identifier, Seconds::infinity());
    }

    template<typename T>
    void sendToStream(T&& message)
    {
        sendToStream(WTFMove(message), renderingBackendIdentifier());
    }

private:
    explicit RemoteRenderingBackendProxy(WebPage&);

    IPC::StreamClientConnection& streamConnection();

    template<typename T>
    auto sendSyncToStream(T&& message, typename T::Reply&& reply, IPC::Timeout timeout = { Seconds::infinity() })
    {
        return streamConnection().sendSync(WTFMove(message), WTFMove(reply), renderingBackendIdentifier(), timeout);
    }

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    GPUProcessConnection& ensureGPUProcessConnection();
    IPC::Connection& gpuProcessConnection();

    // Returns std::nullopt if no update is needed or allocation failed.
    // Returns handle if that should be sent to the receiver process.
    std::optional<SharedMemory::Handle> updateSharedMemoryForGetPixelBuffer(size_t dataSize);
    void destroyGetPixelBufferSharedMemory();

    // Messages to be received.
    void didCreateImageBufferBackend(ImageBufferBackendHandle, WebCore::RenderingResourceIdentifier);
    void didFlush(WebCore::GraphicsContextFlushIdentifier, WebCore::RenderingResourceIdentifier);
    void didFinalizeRenderingUpdate(RenderingUpdateID didRenderingUpdateID);

    std::unique_ptr<IPC::StreamClientConnection> m_streamConnection;
    RemoteRenderingBackendCreationParameters m_parameters;
    WeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    RemoteResourceCacheProxy m_remoteResourceCacheProxy { *this };
    RefPtr<SharedMemory> m_getPixelBufferSharedMemory;
    WebCore::Timer m_destroyGetPixelBufferSharedMemoryTimer { *this, &RemoteRenderingBackendProxy::destroyGetPixelBufferSharedMemory };

    RenderingUpdateID m_renderingUpdateID;
    RenderingUpdateID m_didRenderingUpdateID;

    bool m_needsWakeUpSemaphoreForDisplayListStream { true };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
