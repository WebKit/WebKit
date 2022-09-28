/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGL) && USE(LIBGBM)

#include "GBMBufferSwapchain.h"
#include "GraphicsContextGLANGLE.h"
#include <wtf/HashMap.h>

typedef void *EGLImageKHR;

#if USE(NICOSIA)
namespace Nicosia {
class GCGLANGLELayer;
}
#endif

namespace WebCore {

class GraphicsContextGLGBM : public GraphicsContextGLANGLE {
public:
    static RefPtr<GraphicsContextGLGBM> create(WebCore::GraphicsContextGLAttributes&&);
    virtual ~GraphicsContextGLGBM();

    // GraphicsContextGL overrides
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() override;

#if ENABLE(MEDIA_STREAM)
    RefPtr<VideoFrame> paintCompositedResultsToVideoFrame() override;
#endif
#if ENABLE(VIDEO)
    bool copyTextureFromMedia(MediaPlayer&, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY) override;
#endif

    void setContextVisibility(bool) override;
    void prepareForDisplay() override;

    // GraphicsContextGLANGLE overrides
    bool platformInitializeContext() override;
    bool platformInitialize() override;

    void prepareTexture() override;
    bool reshapeDisplayBufferBacking() override;

    struct Swapchain {
        Swapchain() = default;
        Swapchain(GCGLDisplay);
        ~Swapchain();

        GCGLDisplay platformDisplay { nullptr };
        RefPtr<GBMBufferSwapchain> swapchain;
        RefPtr<GBMBufferSwapchain::Buffer> drawBO;
        RefPtr<GBMBufferSwapchain::Buffer> displayBO;

        // Cache for EGLImage objects corresponding to the buffers originating from the swapchain.
        // The swapchain is regenerated (and these EGLImage objects destroyed) upon each buffer reshaping,
        // so we should be fine without managing stale buffers and corresponding EGLImages (which shouldn't occur anyway).
        HashMap<uint32_t, EGLImageKHR, WTF::DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> images;
    };

    Swapchain& swapchain() { return m_swapchain; }

    struct EGLExtensions {
        bool KHR_image_base { false };
        bool KHR_surfaceless_context { false };
        bool EXT_image_dma_buf_import { false };
        bool EXT_image_dma_buf_import_modifiers { false };
        bool ANGLE_power_preference { false };
    };
    const EGLExtensions& eglExtensions() { return m_eglExtensions; }

protected:
    GraphicsContextGLGBM(WebCore::GraphicsContextGLAttributes&&);

private:
    void allocateDrawBufferObject();

    EGLExtensions m_eglExtensions;
    Swapchain m_swapchain;

#if USE(NICOSIA)
    friend class Nicosia::GCGLANGLELayer;
#endif
};

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(LIBGBM)
