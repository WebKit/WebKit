/*
 * Copyright (C) 2020-2022 Apple Inc.  All rights reserved.
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
#include "RemoteDisplayListRecorderProxy.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include "RenderingBackendIdentifier.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageBufferBackend.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>

namespace WebKit {

class RemoteRenderingBackendProxy;
class RemoteImageBufferProxyFlushFence;

class RemoteImageBufferProxy : public WebCore::ImageBuffer {
    friend class RemoteSerializedImageBufferProxy;
public:
    template<typename BackendType>
    static RefPtr<RemoteImageBufferProxy> create(const WebCore::FloatSize& size, float resolutionScale, const WebCore::DestinationColorSpace& colorSpace, WebCore::PixelFormat pixelFormat, WebCore::RenderingPurpose purpose, RemoteRenderingBackendProxy& remoteRenderingBackendProxy, bool avoidBackendSizeCheck = false)
    {
        auto parameters = WebCore::ImageBufferBackend::Parameters { size, resolutionScale, colorSpace, pixelFormat, purpose };
        if (!avoidBackendSizeCheck && BackendType::calculateSafeBackendSize(parameters).isEmpty())
            return nullptr;
        auto info = populateBackendInfo<BackendType>(parameters);
        return adoptRef(new RemoteImageBufferProxy(parameters, info, remoteRenderingBackendProxy));
    }

    static RefPtr<RemoteImageBufferProxy> create(const WebCore::ImageBufferBackend::Parameters& parameters, const WebCore::ImageBufferBackend::Info& info, RemoteRenderingBackendProxy& remoteRenderingBackendProxy, std::unique_ptr<WebCore::ImageBufferBackend>&& backend, WebCore::RenderingResourceIdentifier identifier)
    {
        return adoptRef(new RemoteImageBufferProxy(parameters, info, remoteRenderingBackendProxy, WTFMove(backend), identifier));
    }

    ~RemoteImageBufferProxy();

    WebCore::ImageBufferBackend* ensureBackendCreated() const final;

    void clearBackend();
    void backingStoreWillChange();
    std::unique_ptr<WebCore::SerializedImageBuffer> sinkIntoSerializedImageBuffer() final;

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    // Messages
    void didCreateBackend(ImageBufferBackendHandle&&);

private:
    RemoteImageBufferProxy(const WebCore::ImageBufferBackend::Parameters&, const WebCore::ImageBufferBackend::Info&, RemoteRenderingBackendProxy&, std::unique_ptr<WebCore::ImageBufferBackend>&& = nullptr, WebCore::RenderingResourceIdentifier = WebCore::RenderingResourceIdentifier::generate());

    RefPtr<WebCore::NativeImage> copyNativeImage(WebCore::BackingStoreCopy = WebCore::CopyBackingStore) const final;
    RefPtr<WebCore::NativeImage> copyNativeImageForDrawing(WebCore::GraphicsContext&) const final;
    RefPtr<WebCore::NativeImage> sinkIntoNativeImage() final;

    RefPtr<ImageBuffer> sinkIntoBufferForDifferentThread() final;
    RefPtr<ImageBuffer> cloneForDifferentThread() final;

    RefPtr<WebCore::Image> filteredImage(WebCore::Filter&) final;

    void drawConsuming(WebCore::GraphicsContext& destContext, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, const WebCore::ImagePaintingOptions&) final;

    WebCore::GraphicsContext& context() const final;

    RefPtr<WebCore::PixelBuffer> getPixelBuffer(const WebCore::PixelBufferFormat& destinationFormat, const WebCore::IntRect& srcRect, const WebCore::ImageBufferAllocator&) const final;
    void putPixelBuffer(const WebCore::PixelBuffer&, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint = { }, WebCore::AlphaPremultiplication = WebCore::AlphaPremultiplication::Premultiplied) final;

    void convertToLuminanceMask() final;
    void transformToColorSpace(const WebCore::DestinationColorSpace&) final;

    void flushDrawingContext() final;
    bool flushDrawingContextAsync() final;

    std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher> createFlusher() final;
    void prepareForBackingStoreChange();

    void assertDispatcherIsCurrent() const;

    IPC::StreamClientConnection& streamConnection() const;

    RefPtr<RemoteImageBufferProxyFlushFence> m_pendingFlush;
    WeakPtr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
    RemoteDisplayListRecorderProxy m_remoteDisplayList;
    bool m_needsFlush { false };
};

class RemoteSerializedImageBufferProxy : public WebCore::SerializedImageBuffer {
    friend class RemoteRenderingBackendProxy;
public:
    ~RemoteSerializedImageBufferProxy();

    static RefPtr<WebCore::ImageBuffer> sinkIntoImageBuffer(std::unique_ptr<RemoteSerializedImageBufferProxy>, RemoteRenderingBackendProxy&);

    WebCore::RenderingResourceIdentifier renderingResourceIdentifier() { return m_renderingResourceIdentifier; }

    RemoteSerializedImageBufferProxy(const WebCore::ImageBufferBackend::Parameters&, const WebCore::ImageBufferBackend::Info&, const WebCore::RenderingResourceIdentifier&, RemoteRenderingBackendProxy&);

    size_t memoryCost() final
    {
        return m_info.memoryCost;
    }

private:
    RefPtr<WebCore::ImageBuffer> sinkIntoImageBuffer() final
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    bool isRemoteSerializedImageBufferProxy() const final { return true; }

    WebCore::ImageBufferBackend::Parameters m_parameters;
    WebCore::ImageBufferBackend::Info m_info;
    WebCore::RenderingResourceIdentifier m_renderingResourceIdentifier;
    RefPtr<IPC::Connection> m_connection;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteSerializedImageBufferProxy)
    static bool isType(const WebCore::SerializedImageBuffer& buffer) { return buffer.isRemoteSerializedImageBufferProxy(); }
SPECIALIZE_TYPE_TRAITS_END()


#endif // ENABLE(GPU_PROCESS)
