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

#if USE(DIRECT2D)

#include "ImageBufferBackend.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class ImageBufferDirect2DBackend : public ImageBufferBackend {
    WTF_MAKE_ISO_ALLOCATED(ImageBufferDirect2DBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferDirect2DBackend);
public:
    static std::unique_ptr<ImageBufferDirect2DBackend> create(const FloatSize&, float resolutionScale, ColorSpace, const HostWindow*);
    static std::unique_ptr<ImageBufferDirect2DBackend> create(const FloatSize&, const GraphicsContext&);

    GraphicsContext& context() const override;
    void flushContext() override;

    NativeImagePtr copyNativeImage(BackingStoreCopy = CopyBackingStore) const override;
    RefPtr<Image> copyImage(BackingStoreCopy, PreserveResolution) const override;

    RefPtr<Image> sinkIntoImage(PreserveResolution) override;

    String toDataURL(const String& mimeType, Optional<double> quality, PreserveResolution) const override;
    Vector<uint8_t> toData(const String& mimeType, Optional<double> quality) const override;
    Vector<uint8_t> toBGRAData() const override;

    RefPtr<ImageData> getImageData(AlphaPremultiplication outputFormat, const IntRect&) const override;
    void putImageData(AlphaPremultiplication inputFormat, const ImageData&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat) override;

protected:
    ImageBufferDirect2DBackend(const FloatSize& logicalSize, const IntSize& physicalSize, float resolutionScale, ColorSpace);

    std::unique_ptr<PlatformContextDirect2D> m_platformContext;
    std::unique_ptr<GraphicsContext> m_context;
    NativeImagePtr m_bitmap;
};

} // namespace WebCore

#endif // USE(DIRECT2D)
