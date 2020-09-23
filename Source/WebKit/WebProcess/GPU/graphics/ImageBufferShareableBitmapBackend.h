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

#if ENABLE(GPU_PROCESS)

#include "ImageBufferBackendHandle.h"
#include <WebCore/PlatformImageBufferBackend.h>
#include <wtf/IsoMalloc.h>

namespace WebCore {
class ImageData;
}

namespace WebKit {

class ShareableBitmap;

class ImageBufferShareableBitmapBackend : public WebCore::PlatformImageBufferBackend {
    WTF_MAKE_ISO_ALLOCATED(ImageBufferShareableBitmapBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferShareableBitmapBackend);
public:
    static std::unique_ptr<ImageBufferShareableBitmapBackend> create(const WebCore::FloatSize& logicalSize, const float resolutionScale, WebCore::ColorSpace, const WebCore::HostWindow*);
    static std::unique_ptr<ImageBufferShareableBitmapBackend> create(const WebCore::FloatSize& logicalSize, const WebCore::IntSize& internalSize, float resolutionScale, WebCore::ColorSpace, ImageBufferBackendHandle);

    ImageBufferShareableBitmapBackend(const WebCore::FloatSize& logicalSize, const WebCore::IntSize& physicalSize, float resolutionScale, WebCore::ColorSpace, RefPtr<ShareableBitmap>&&, std::unique_ptr<WebCore::GraphicsContext>&&);

    ImageBufferBackendHandle createImageBufferBackendHandle() const;

    WebCore::GraphicsContext& context() const override { return *m_context; }

    WebCore::NativeImagePtr copyNativeImage(WebCore::BackingStoreCopy = WebCore::CopyBackingStore) const override;
    RefPtr<WebCore::Image> copyImage(WebCore::BackingStoreCopy = WebCore::CopyBackingStore, WebCore::PreserveResolution = WebCore::PreserveResolution::No) const override;

    Vector<uint8_t> toBGRAData() const override;
    RefPtr<WebCore::ImageData> getImageData(WebCore::AlphaPremultiplication outputFormat, const WebCore::IntRect&) const override;
    void putImageData(WebCore::AlphaPremultiplication inputFormat, const WebCore::ImageData&, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat) override;

private:
    ColorFormat backendColorFormat() const override { return ColorFormat::BGRA; }

    RefPtr<ShareableBitmap> m_bitmap;
    std::unique_ptr<WebCore::GraphicsContext> m_context;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
