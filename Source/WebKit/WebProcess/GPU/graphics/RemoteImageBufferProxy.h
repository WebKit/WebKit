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
#include <WebCore/ImageBuffer.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>

namespace WebKit {

class RemoteRenderingBackendProxy;
class RemoteImageBufferProxyFlushState;

class RemoteImageBufferProxy : public WebCore::ImageBuffer {
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

    ~RemoteImageBufferProxy();

    DisplayListRecorderFlushIdentifier lastSentFlushIdentifier() const { return m_sentFlushIdentifier; }

    WebCore::ImageBufferBackend* ensureBackendCreated() const final;

    void clearBackend();
    void backingStoreWillChange();
    void didCreateImageBufferBackend(ImageBufferBackendHandle&&);

private:
    RemoteImageBufferProxy(const WebCore::ImageBufferBackend::Parameters&, const WebCore::ImageBufferBackend::Info&, RemoteRenderingBackendProxy&);

    bool hasPendingFlush() const;

    void waitForDidFlushWithTimeout();

    RefPtr<WebCore::NativeImage> copyNativeImage(WebCore::BackingStoreCopy = WebCore::CopyBackingStore) const final;
    RefPtr<WebCore::NativeImage> copyNativeImageForDrawing(WebCore::BackingStoreCopy = WebCore::CopyBackingStore) const final;
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
    
    bool prefersPreparationForDisplay() final { return true; }
    
    void flushContext() final;
    void flushDrawingContext() final;
    bool flushDrawingContextAsync() final;

    std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher> createFlusher() final;
    void prepareForBackingStoreChange();

    void assertDispatcherIsCurrent() const;

    DisplayListRecorderFlushIdentifier m_sentFlushIdentifier;
    Ref<RemoteImageBufferProxyFlushState> m_flushState;
    WeakPtr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
    RemoteDisplayListRecorderProxy m_remoteDisplayList;
    bool m_needsFlush { false };
};

class RemoteImageBufferProxyFlushState : public ThreadSafeRefCounted<RemoteImageBufferProxyFlushState> {
    WTF_MAKE_NONCOPYABLE(RemoteImageBufferProxyFlushState);
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteImageBufferProxyFlushState() = default;
    void waitForDidFlushOnSecondaryThread(DisplayListRecorderFlushIdentifier);
    void markCompletedFlush(DisplayListRecorderFlushIdentifier);
    void cancel();
    DisplayListRecorderFlushIdentifier identifierForCompletedFlush() const;

private:
    mutable Lock m_lock;
    Condition m_condition;
    DisplayListRecorderFlushIdentifier m_identifier WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
