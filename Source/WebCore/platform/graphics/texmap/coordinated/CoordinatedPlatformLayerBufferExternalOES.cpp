/*
 * Copyright (C) 2015, 2024 Igalia S.L.
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

#include "config.h"
#include "CoordinatedPlatformLayerBufferExternalOES.h"

#if USE(COORDINATED_GRAPHICS)
#include "BitmapTexturePool.h"
#include "PlatformDisplay.h"
#include "TextureMapper.h"
#include <wtf/MathExtras.h>

#if USE(GSTREAMER) && USE(GBM)
#include <drm_fourcc.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <gst/allocators/gstfdmemory.h>
#include <gst/video/gstvideometa.h>
#endif

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferExternalOES> CoordinatedPlatformLayerBufferExternalOES::create(unsigned textureID, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
{
    return makeUnique<CoordinatedPlatformLayerBufferExternalOES>(textureID, size, flags, WTF::move(fence));
}

#if USE(GSTREAMER) && USE(GBM)
std::unique_ptr<CoordinatedPlatformLayerBufferExternalOES> CoordinatedPlatformLayerBufferExternalOES::create(GRefPtr<GstBuffer>&& buffer, uint32_t fourcc, const IntSize& size, OptionSet<TextureMapperFlags> flags)
{
    return makeUnique<CoordinatedPlatformLayerBufferExternalOES>(WTF::move(buffer), fourcc, size, flags);
}
#endif

CoordinatedPlatformLayerBufferExternalOES::CoordinatedPlatformLayerBufferExternalOES(unsigned textureID, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::ExternalOES, size, flags, WTF::move(fence))
    , m_textureID(textureID)
{
}

#if USE(GSTREAMER) && USE(GBM)
CoordinatedPlatformLayerBufferExternalOES::CoordinatedPlatformLayerBufferExternalOES(GRefPtr<GstBuffer>&& buffer, uint32_t fourcc, const IntSize& size, OptionSet<TextureMapperFlags> flags)
    : CoordinatedPlatformLayerBuffer(Type::ExternalOES, size, flags, nullptr)
    , m_fourcc(fourcc)
    , m_buffer(WTF::move(buffer))
{
}
#endif

CoordinatedPlatformLayerBufferExternalOES::~CoordinatedPlatformLayerBufferExternalOES() = default;

void CoordinatedPlatformLayerBufferExternalOES::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    waitForContentsIfNeeded();
    if (m_textureID) {
        textureMapper.drawTextureExternalOES(m_textureID, m_flags, targetRect, modelViewMatrix, opacity);
        return;
    }

#if USE(GSTREAMER) && USE(GBM)
    auto memory = gst_buffer_peek_memory(m_buffer.get(), 0);
    if (!gst_is_fd_memory(memory)) [[unlikely]]
        return;

    int fd = gst_fd_memory_get_fd(memory);

    // Use stride and plane offsets from GstVideoMeta if available. The Qualcomm decoder
    // populates these from the C2HandleGBM with the exact values for the allocated GBM buffer,
    // including the correct UV plane offset (which accounts for slice height alignment).
    // Providing explicit plane 1 attributes avoids the EGL driver needing to consult the
    // GBM metadata buffer (meta_buffer_fd) to locate the UV plane, which it cannot access
    // when importing via EGL_LINUX_DMA_BUF_EXT, causing intermittent green frames.
    EGLint stride = WTF::roundUpToMultipleOf(128, m_size.width());
    EGLint uvStride = stride;
    std::optional<EGLAttrib> uvOffset;
    if (const auto videoMeta = gst_buffer_get_video_meta(m_buffer.get()); videoMeta && videoMeta->n_planes >= 2) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN; // GLib port
        stride = videoMeta->stride[0];
        uvStride = videoMeta->stride[1];
        uvOffset = static_cast<EGLAttrib>(videoMeta->offset[1]);
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END; // GLib port
    }

    Vector<EGLAttrib> attributes {
        EGL_WIDTH, m_size.width(),
        EGL_HEIGHT, m_size.height(),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(m_fourcc),
        EGL_DMA_BUF_PLANE0_FD_EXT, fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
    };

    // Specify DRM_FORMAT_MOD_LINEAR to tell the driver the buffer uses a linear layout.
    // Without this, the Qualcomm Adreno driver tries to query the GBM metadata buffer
    // to determine the buffer format (linear vs UBWC/compressed), which fails because
    // meta_buffer_fd is unavailable through the standard DMA-BUF import path.
    auto& display = PlatformDisplay::sharedDisplay();
    if (display.eglExtensions().EXT_image_dma_buf_import_modifiers) {
        std::array<EGLAttrib, 4> plane0Modifier {
            EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, static_cast<EGLAttrib>(DRM_FORMAT_MOD_LINEAR >> 32),
            EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, static_cast<EGLAttrib>(DRM_FORMAT_MOD_LINEAR & 0xffffffff),
        };
        attributes.append(std::span<const EGLAttrib> { plane0Modifier });
    }

    if (uvOffset) {
        std::array<EGLAttrib, 6> plane1Attributes {
            EGL_DMA_BUF_PLANE1_FD_EXT, static_cast<EGLAttrib>(fd),
            EGL_DMA_BUF_PLANE1_OFFSET_EXT, *uvOffset,
            EGL_DMA_BUF_PLANE1_PITCH_EXT, static_cast<EGLAttrib>(uvStride),
        };
        attributes.append(std::span<const EGLAttrib> { plane1Attributes });
        if (display.eglExtensions().EXT_image_dma_buf_import_modifiers) {
            std::array<EGLAttrib, 4> plane1Modifier {
                EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT, static_cast<EGLAttrib>(DRM_FORMAT_MOD_LINEAR >> 32),
                EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT, static_cast<EGLAttrib>(DRM_FORMAT_MOD_LINEAR & 0xffffffff),
            };
            attributes.append(std::span<const EGLAttrib> { plane1Modifier });
        }
    }

    attributes.append(EGL_NONE);

    auto image = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    if (!image) [[unlikely]]
        return;

    OptionSet<BitmapTexture::Flags> textureFlags { BitmapTexture::Flags::ExternalOESRenderTarget };
    if (m_flags.contains(TextureMapperFlags::ShouldBlend))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
    auto texture = BitmapTexturePool::singleton().createTextureForImage(image, m_size, textureFlags);
    textureMapper.drawTextureExternalOESYUV(texture->id(), m_flags, targetRect, modelViewMatrix, opacity);
    display.destroyEGLImage(image);
#endif // USE(GSTREAMER) && USE(GBM)
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
