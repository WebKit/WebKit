/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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

#if USE(CG)

#include "ImageBufferCGBackend.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class ImageBufferCGBitmapBackend : public ImageBufferCGBackend {
    WTF_MAKE_ISO_ALLOCATED(ImageBufferCGBitmapBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferCGBitmapBackend);
public:
    static std::unique_ptr<ImageBufferCGBitmapBackend> create(const FloatSize&, float resolutionScale, ColorSpace, CGColorSpaceRef, const HostWindow*);
    static std::unique_ptr<ImageBufferCGBitmapBackend> create(const FloatSize&, float resolutionScale, ColorSpace, const HostWindow*);
    static std::unique_ptr<ImageBufferCGBitmapBackend> create(const FloatSize&, const GraphicsContext&);

    GraphicsContext& context() const override;

    NativeImagePtr copyNativeImage(BackingStoreCopy = CopyBackingStore) const override;

    Vector<uint8_t> toBGRAData() const override;

    RefPtr<ImageData> getImageData(AlphaPremultiplication outputFormat, const IntRect&) const override;
    void putImageData(AlphaPremultiplication inputFormat, const ImageData&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat) override;

private:
    ImageBufferCGBitmapBackend(const FloatSize& logicalSize, const IntSize& physicalSize, float resolutionScale, ColorSpace, void* data, RetainPtr<CGDataProviderRef>&&, std::unique_ptr<GraphicsContext>&&);

#if PLATFORM(IOS_FAMILY)
    ColorFormat backendColorFormat() const override { return ColorFormat::BGRA; }
#endif

    void* m_data;
    RetainPtr<CGDataProviderRef> m_dataProvider;
    std::unique_ptr<GraphicsContext> m_context;
};

} // namespace WebCore

#endif // USE(CG)
