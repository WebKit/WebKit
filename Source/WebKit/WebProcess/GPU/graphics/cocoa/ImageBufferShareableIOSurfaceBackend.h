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

#if HAVE(IOSURFACE)

#include "ImageBufferBackendHandle.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBufferBackend.h>
#include <wtf/IsoMalloc.h>

namespace WebKit {

class ImageBufferShareableIOSurfaceBackend final : public WebCore::ImageBufferBackend {
    WTF_MAKE_ISO_ALLOCATED(ImageBufferShareableIOSurfaceBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferShareableIOSurfaceBackend);
public:
    static std::unique_ptr<ImageBufferShareableIOSurfaceBackend> create(const Parameters&, ImageBufferBackendHandle);

    ImageBufferShareableIOSurfaceBackend(const Parameters& parameters, ImageBufferBackendHandle&& handle)
        : ImageBufferBackend(parameters)
        , m_handle(WTFMove(handle))
    {
    }

    ImageBufferBackendHandle createImageBufferBackendHandle() const;

    WebCore::GraphicsContext& context() const override;
    RefPtr<WebCore::NativeImage> copyNativeImage(WebCore::BackingStoreCopy) const override;
    RefPtr<WebCore::Image> copyImage(WebCore::BackingStoreCopy, WebCore::PreserveResolution) const override;
    void draw(WebCore::GraphicsContext&, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, const WebCore::ImagePaintingOptions&) override;
    void drawPattern(WebCore::GraphicsContext&, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, const WebCore::AffineTransform& patternTransform, const WebCore::FloatPoint& phase, const WebCore::FloatSize& spacing, const WebCore::ImagePaintingOptions&) override;
    String toDataURL(const String& mimeType, Optional<double> quality, WebCore::PreserveResolution) const override;
    Vector<uint8_t> toData(const String& mimeType, Optional<double> quality) const override;
    Vector<uint8_t> toBGRAData() const override;
    RefPtr<WebCore::ImageData> getImageData(WebCore::AlphaPremultiplication outputFormat, const WebCore::IntRect&) const override;
    void putImageData(WebCore::AlphaPremultiplication inputFormat, const WebCore::ImageData&, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat) override;

    static constexpr bool isOriginAtUpperLeftCorner = true;
    static constexpr bool canMapBackingStore = false;
    static constexpr WebCore::RenderingMode renderingMode = WebCore::RenderingMode::Accelerated;

private:
    ImageBufferBackendHandle m_handle;
};

} // namespace WebKit

#endif // HAVE(IOSURFACE)
