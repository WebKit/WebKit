/*
 * Copyright (C) 2020-2021 Apple Inc.  All rights reserved.
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

#if HAVE(IOSURFACE)

#include "ImageBufferCGBackend.h"
#include "IOSurface.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class WEBCORE_EXPORT ImageBufferIOSurfaceBackend : public ImageBufferCGBackend {
    WTF_MAKE_ISO_ALLOCATED(ImageBufferIOSurfaceBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferIOSurfaceBackend);
public:
    static IntSize calculateSafeBackendSize(const Parameters&);
    static unsigned calculateBytesPerRow(const IntSize& backendSize);
    static size_t calculateMemoryCost(const Parameters&);
    static size_t calculateExternalMemoryCost(const Parameters&);
    
    static std::unique_ptr<ImageBufferIOSurfaceBackend> create(const Parameters&, const HostWindow*);
    // FIXME: Rename to createUsingColorSpaceOfGraphicsContext() (or something like that).
    static std::unique_ptr<ImageBufferIOSurfaceBackend> create(const Parameters&, const GraphicsContext&);

    ImageBufferIOSurfaceBackend(const Parameters&, std::unique_ptr<IOSurface>&&);

    IOSurface* surface();

    GraphicsContext& context() const override;
    void flushContext() override;

    IntSize backendSize() const override;
    
    RefPtr<NativeImage> copyNativeImage(BackingStoreCopy = CopyBackingStore) const override;
    RefPtr<NativeImage> sinkIntoNativeImage() override;

    void drawConsuming(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&) override;

    std::optional<PixelBuffer> getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect&) const override;
    void putPixelBuffer(const PixelBuffer&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat) override;

    bool isInUse() const override;
    void releaseGraphicsContext() override;
    VolatilityState setVolatile(bool) override;
    void releaseBufferToPool() override;

    static constexpr RenderingMode renderingMode = RenderingMode::Accelerated;

protected:
    static RetainPtr<CGColorSpaceRef> contextColorSpace(const GraphicsContext&);
    unsigned bytesPerRow() const override;

    // ImageBufferCGBackend overrides.
    RetainPtr<CGImageRef> copyCGImageForEncoding(CFStringRef destinationUTI, PreserveResolution) const final;

    void prepareToDrawIntoContext(GraphicsContext& destinationContext) override;
    void invalidateCachedNativeImage() const;

    std::unique_ptr<IOSurface> m_surface;
    IOSurfaceSeed m_lastSeedWhenDrawingImage { 0 };
    mutable bool m_requiresDrawAfterPutPixelBuffer { false };

    mutable bool m_needsSetupContext { false };
};

} // namespace WebCore

#endif // HAVE(IOSURFACE)
