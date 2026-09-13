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

#include "ImageBufferBackendHandle.h"
#include "RemoteGraphicsContextProxy.h"
#include "RemoteRenderingBackendIdentifier.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageBufferBackend.h>
#include <wtf/Condition.h>
#include <wtf/Identified.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>

namespace IPC {
class Connection;
}

namespace WebKit {

class RemoteRenderingBackendProxy;
class RemoteImageBufferProxyFlushFence;

class RemoteImageBufferProxy final : public WebCore::ImageBuffer {
    WTF_MAKE_TZONE_ALLOCATED(RemoteImageBufferProxy);
    friend class RemoteSerializedImageBufferProxy;
public:
    template<typename BackendType, typename... Args>
    static RefPtr<RemoteImageBufferProxy> create(const WebCore::ImageBuffer::Parameters& parameters, RemoteRenderingBackendProxy& remoteRenderingBackendProxy, Args&&... backendArgs)
    {
        if (BackendType::calculateSafeBackendSize(parameters).isEmpty())
            return nullptr;
        std::unique_ptr<WebCore::ImageBufferBackend> backend = BackendType::create(parameters, std::forward<Args>(backendArgs)...);
        if (!backend)
            return nullptr;
        return adoptRef(new RemoteImageBufferProxy(parameters, WTF::move(backend), remoteRenderingBackendProxy));
    }

    // Used when re-materialising a serialized buffer. The GPU process keeps the
    // existing ImageBuffer, so the backend is built from what the serialized buffer
    // carried: the backing store this process allocated, or nothing at all.
    static Ref<RemoteImageBufferProxy> createForSerializedBuffer(const WebCore::ImageBuffer::Parameters&, std::unique_ptr<WebCore::ImageBufferBackend>&&, RemoteRenderingBackendProxy&);

    ~RemoteImageBufferProxy();
    bool NODELETE isValid() const;

    void disconnect();

    // Non-null iff this process allocated the backing store, in which case the handle
    // is passed to the GPU process with CreateImageBuffer.
    std::optional<ImageBufferBackendHandle> createBackingStoreHandleForGPUProcess() const;

    // Reaching for the sharing interface means someone is about to take the backend
    // handle, so ask the GPU process for it if the backing store is theirs.
    WebCore::ImageBufferBackendSharing* toBackendSharing() final;

    // The backing store this process allocated travels with the serialized buffer;
    // for anything else the GPU process is asked for the handle after unserializing.
    std::optional<ImageBufferBackendHandle> takeBackendHandleForSerialization();

    void backingStoreWillChange();
    std::unique_ptr<WebCore::SerializedImageBuffer> sinkIntoSerializedImageBuffer() final;

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    // Messages
    void didFailToCreateBackend();

    RemoteGraphicsContextIdentifier contextIdentifier() const { return m_context.identifier(); }

    // Sends single-line strokes that have been buffered on the proxy's graphics
    // context into the IPC stream. Call before any cross-buffer read of this
    // image buffer (drawImageBuffer source, clipToImageBuffer source, etc.) so
    // the GPU process sees the up-to-date contents.
    void sendPendingDrawsIfNecessary() const { m_context.sendPendingDrawsIfNecessary(); }
private:
    RemoteImageBufferProxy(Parameters, std::unique_ptr<WebCore::ImageBufferBackend>&&, RemoteRenderingBackendProxy&);

    RefPtr<WebCore::NativeImage> copyNativeImage() const final;
    RefPtr<WebCore::NativeImage> createNativeImageReference() const final;
    bool isRemoteImageBufferProxy() const final { return true; }
    RefPtr<WebCore::NativeImage> sinkIntoNativeImage() final;

    RefPtr<ImageBuffer> sinkIntoBufferForDifferentThread() final;

    RefPtr<WebCore::NativeImage> filteredNativeImage(WebCore::Filter&) final;

    WebCore::GraphicsContext& context() const final;

    RefPtr<WebCore::PixelBuffer> getPixelBuffer(const WebCore::PixelBufferFormat& destinationFormat, const WebCore::IntRect& srcRect, const WebCore::ImageBufferAllocator&) const final;
    void putPixelBuffer(const WebCore::PixelBufferSourceView&, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint = { }, WebCore::AlphaPremultiplication = WebCore::AlphaPremultiplication::Premultiplied) final;

    void convertToLuminanceMask() final;
    void transformToColorSpace(const WebCore::ColorSpace&) final;

    void flushDrawingContext() final;
    bool flushDrawingContextAsync() final;
    std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher> createFlusher() final;

    void prepareForBackingStoreChange();
    std::optional<WebCore::RenderingMode> getEffectiveRenderingModeForTesting() const final;

    void NODELETE assertDispatcherIsCurrent() const;
    template<typename T> void send(T&& message) const;
    template<typename T> auto sendSync(T&& message) const;
    RefPtr<IPC::StreamClientConnection> connection() const;
    void didBecomeUnresponsive() const;

    // Fetches the backing store handle from the GPU process, for a backend that
    // stands in for a backing store this process cannot map.
    void ensureBackendHandle() const;

    RefPtr<RemoteImageBufferProxyFlushFence> m_pendingFlush;
    mutable RemoteGraphicsContextProxy m_context;
    WeakPtr<RemoteRenderingBackendProxy> m_renderingBackend;
};

class RemoteSerializedImageBufferProxy : public WebCore::SerializedImageBuffer, public Identified<RemoteSerializedImageBufferIdentifier> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteSerializedImageBufferProxy);
    friend class RemoteRenderingBackendProxy;
public:
    ~RemoteSerializedImageBufferProxy();

    static RefPtr<WebCore::ImageBuffer> sinkIntoImageBuffer(std::unique_ptr<RemoteSerializedImageBufferProxy>, RemoteRenderingBackendProxy&);

    RemoteSerializedImageBufferProxy(WebCore::ImageBuffer::Parameters, WebCore::RenderingMode, size_t memoryCost, std::optional<ImageBufferBackendHandle>&&, RemoteRenderingBackendProxy&);

    size_t memoryCost() const final
    {
        return m_memoryCost;
    }

    const WebCore::ImageBuffer::Parameters& parameters() const LIFETIME_BOUND { return m_parameters; }
    WebCore::RenderingMode renderingMode() const { return m_renderingMode; }
    // Non-null only for a backing store this process allocated, which is handed to
    // the re-materialised buffer as-is.
    std::optional<ImageBufferBackendHandle> takeBackendHandle() { return std::exchange(m_backendHandle, std::nullopt); }

    std::unique_ptr<WebCore::SerializedImageBuffer> clone() const final;

private:
    RemoteSerializedImageBufferProxy(const WebCore::ImageBuffer::Parameters& parameters, WebCore::RenderingMode renderingMode, size_t memoryCost, std::optional<ImageBufferBackendHandle>&& backendHandle, const RefPtr<IPC::Connection>& connection)
        : m_parameters(parameters)
        , m_renderingMode(renderingMode)
        , m_memoryCost(memoryCost)
        , m_backendHandle(WTF::move(backendHandle))
        , m_connection(connection)
    {
    }

    RefPtr<WebCore::ImageBuffer> sinkIntoImageBuffer() final
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    std::optional<WebCore::ImageBufferTransferHandle> sinkIntoTransferHandle() final;

    bool isRemoteSerializedImageBufferProxy() const final { return true; }

    const WebCore::ImageBuffer::Parameters m_parameters;
    const WebCore::RenderingMode m_renderingMode;
    const size_t m_memoryCost;
    std::optional<ImageBufferBackendHandle> m_backendHandle;
    RefPtr<IPC::Connection> m_connection;
    WeakPtr<RemoteRenderingBackendProxy> m_renderingBackend;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteSerializedImageBufferProxy)
    static bool isType(const WebCore::SerializedImageBuffer& buffer) { return buffer.isRemoteSerializedImageBufferProxy(); }
SPECIALIZE_TYPE_TRAITS_END()


#endif // ENABLE(GPU_PROCESS)
