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

#include "ImageBufferBackendHandleSharing.h"
#include <WebCore/ImageBuffer.h>
#include <wtf/TZoneMalloc.h>

#if USE(CG)
#include <WebCore/ImageBufferCGBackend.h>
#elif USE(CAIRO)
#include <WebCore/ImageBufferCairoBackend.h>
#elif USE(SKIA)
#include <WebCore/ImageBufferSkiaBackend.h>
#endif

namespace WebCore {
class ProcessIdentity;
class ShareableBitmap;
}

namespace WebKit {

#if USE(CG)
using ImageBufferShareableBitmapBackendBase = WebCore::ImageBufferCGBackend;
#elif USE(CAIRO)
using ImageBufferShareableBitmapBackendBase = WebCore::ImageBufferCairoBackend;
#elif USE(SKIA)
using ImageBufferShareableBitmapBackendBase = WebCore::ImageBufferSkiaBackend;
#endif

class ImageBufferShareableBitmapBackend final : public ImageBufferShareableBitmapBackendBase, public ImageBufferBackendHandleSharing {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(ImageBufferShareableBitmapBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferShareableBitmapBackend);

public:
    static WebCore::IntSize calculateSafeBackendSize(const Parameters&);
    static unsigned calculateBytesPerRow(const Parameters&, const WebCore::IntSize& backendSize);
    static size_t calculateMemoryCost(const Parameters&);

    static std::unique_ptr<ImageBufferShareableBitmapBackend> create(const Parameters&, const WebCore::ImageBufferCreationContext&);
    static std::unique_ptr<ImageBufferShareableBitmapBackend> create(const Parameters&, WebCore::ShareableBitmap::Handle);

    ImageBufferShareableBitmapBackend(const Parameters&, Ref<WebCore::ShareableBitmap>&&, std::unique_ptr<WebCore::GraphicsContext>&&);
    virtual ~ImageBufferShareableBitmapBackend();

    bool canMapBackingStore() const final;
    WebCore::GraphicsContext& context() final { return *m_context; }

    std::optional<ImageBufferBackendHandle> createBackendHandle(WebCore::SharedMemory::Protection = WebCore::SharedMemory::Protection::ReadWrite) const final;
    RefPtr<WebCore::ShareableBitmap> bitmap() const final { return m_bitmap.ptr(); }
#if USE(CAIRO)
    RefPtr<cairo_surface_t> createCairoSurface() final;
#endif
    void transferToNewContext(const WebCore::ImageBufferCreationContext&) final;

    RefPtr<WebCore::NativeImage> copyNativeImage() final;
    RefPtr<WebCore::NativeImage> createNativeImageReference() final;

    void getPixelBuffer(const WebCore::IntRect&, WebCore::PixelBuffer&) final;
    void putPixelBuffer(const WebCore::PixelBuffer&, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat) final;

private:
    unsigned bytesPerRow() const final;
    String debugDescription() const final;

    ImageBufferBackendSharing* toBackendSharing() final { return this; }
    void releaseGraphicsContext() final { /* Do nothing. This is only relevant for IOSurface backends */ }

    Ref<WebCore::ShareableBitmap> m_bitmap;
    std::unique_ptr<WebCore::GraphicsContext> m_context;
};

} // namespace WebKit
