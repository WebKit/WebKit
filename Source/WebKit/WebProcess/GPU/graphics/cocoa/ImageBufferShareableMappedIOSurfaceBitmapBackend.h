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

#if ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)

#include "ImageBufferBackendHandleSharing.h"
#include <WebCore/IOSurface.h>
#include <WebCore/ImageBufferIOSurfaceBackend.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class ProcessIdentity;
}

namespace WebKit {

// ImageBufferBackend for small LayerBacking stores.
class ImageBufferShareableMappedIOSurfaceBitmapBackend final : public WebCore::ImageBufferCGBackend, public ImageBufferBackendHandleSharing {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(ImageBufferShareableMappedIOSurfaceBitmapBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferShareableMappedIOSurfaceBitmapBackend);
public:
    static std::unique_ptr<ImageBufferShareableMappedIOSurfaceBitmapBackend> create(const Parameters&, const WebCore::ImageBufferCreationContext&);
    static size_t calculateMemoryCost(const Parameters& parameters) { return WebCore::ImageBufferIOSurfaceBackend::calculateMemoryCost(parameters); }

    ImageBufferShareableMappedIOSurfaceBitmapBackend(const Parameters&, std::unique_ptr<WebCore::IOSurface>, WebCore::IOSurface::LockAndContext&&, WebCore::IOSurfacePool*);
    ~ImageBufferShareableMappedIOSurfaceBitmapBackend();

    static constexpr WebCore::RenderingMode renderingMode = WebCore::RenderingMode::Accelerated;
    bool canMapBackingStore() const final;

    std::optional<ImageBufferBackendHandle> createBackendHandle(WebCore::SharedMemory::Protection = WebCore::SharedMemory::Protection::ReadWrite) const final;
    WebCore::GraphicsContext& context() final;
private:
    // ImageBufferBackendSharing
    ImageBufferBackendSharing* toBackendSharing() final { return this; }

    // WebCore::ImageBufferCGBackend
    unsigned bytesPerRow() const final;
    RefPtr<WebCore::NativeImage> copyNativeImage() final;
    RefPtr<WebCore::NativeImage> createNativeImageReference() final;
    RefPtr<WebCore::NativeImage> sinkIntoNativeImage() final;
    bool isInUse() const final;
    void releaseGraphicsContext() final;
    bool setVolatile() final;
    WebCore::SetNonVolatileResult setNonVolatile() final;
    WebCore::VolatilityState volatilityState() const final;
    void setVolatilityState(WebCore::VolatilityState) final;
    void transferToNewContext(const WebCore::ImageBufferCreationContext&) final;
    void getPixelBuffer(const WebCore::IntRect&, WebCore::PixelBuffer&) final;
    void putPixelBuffer(const WebCore::PixelBufferSourceView&, const WebCore::IntRect&, const WebCore::IntPoint&, WebCore::AlphaPremultiplication) final;
    void flushContext() final;

    std::unique_ptr<WebCore::IOSurface> m_surface;
    std::optional<WebCore::IOSurface::Locker<WebCore::IOSurface::AccessMode::ReadWrite>> m_lock;
    WebCore::VolatilityState m_volatilityState { WebCore::VolatilityState::NonVolatile };
    RefPtr<WebCore::IOSurfacePool> m_ioSurfacePool;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)
