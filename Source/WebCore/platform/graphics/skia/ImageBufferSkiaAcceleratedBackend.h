/*
 * Copyright (C) 2024, 2025 Igalia S.L.
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

#if USE(SKIA)
#include "ImageBuffer.h"
#include "ImageBufferSkiaSurfaceBackend.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPictureRecorder.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/TZoneMalloc.h>

namespace WebCore {

class GLFence;
class GraphicsContextSkia;
class SkiaSwitchableCanvas;

class ImageBufferSkiaAcceleratedBackend final : public ImageBufferSkiaSurfaceBackend {
    WTF_MAKE_TZONE_ALLOCATED(ImageBufferSkiaAcceleratedBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferSkiaAcceleratedBackend);
public:
    static std::unique_ptr<ImageBufferSkiaAcceleratedBackend> create(const Parameters&, const ImageBufferCreationContext&);
    static std::unique_ptr<ImageBufferSkiaAcceleratedBackend> create(const Parameters&, const ImageBufferCreationContext&, sk_sp<SkSurface>&&);
    ~ImageBufferSkiaAcceleratedBackend();

    static constexpr RenderingMode renderingMode = RenderingMode::Accelerated;

private:
    ImageBufferSkiaAcceleratedBackend(const Parameters&, sk_sp<SkSurface>&&);

    GraphicsContext& context() final;
    void flushContext() final;
    void prepareForDisplay() final;

    RefPtr<NativeImage> copyNativeImage() final;
    RefPtr<NativeImage> createNativeImageReference() final;

    void getPixelBuffer(const IntRect&, PixelBuffer&) final;
    void putPixelBuffer(const PixelBufferSourceView&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat) final;

    std::unique_ptr<GLFence> flushCanvasRecordingContextIfNeeded();
    void ensureCanvasRecordingContext();

#if USE(COORDINATED_GRAPHICS)
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() const final;

    RefPtr<GraphicsLayerContentsDisplayDelegate> m_layerContentsDisplayDelegate;
#endif

    bool m_hasActiveRecording : 1 { false };
    SkPictureRecorder m_pictureRecorder;
    std::unique_ptr<SkiaSwitchableCanvas> m_switchableCanvas;
    std::unique_ptr<GraphicsContextSkia> m_canvasRecordingContext;
};

} // namespace WebCore

#endif // USE(SKIA)
