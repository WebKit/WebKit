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

#if USE(SKIA)
#include "ColorSpaceSkia.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkImage.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferExternalOES> CoordinatedPlatformLayerBufferExternalOES::create(unsigned textureID, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
{
    return makeUnique<CoordinatedPlatformLayerBufferExternalOES>(textureID, size, flags, WTF::move(fence));
}

#if USE(GSTREAMER) && USE(GBM)
std::unique_ptr<CoordinatedPlatformLayerBufferExternalOES> CoordinatedPlatformLayerBufferExternalOES::create(GRefPtr<GstBuffer>&& buffer, uint32_t fourcc, CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace yuvColorSpace, SampleRange sampleRange, const IntSize& size, OptionSet<TextureMapperFlags> flags)
{
    return makeUnique<CoordinatedPlatformLayerBufferExternalOES>(WTF::move(buffer), fourcc, yuvColorSpace, sampleRange, size, flags);
}
#endif

CoordinatedPlatformLayerBufferExternalOES::CoordinatedPlatformLayerBufferExternalOES(unsigned textureID, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::ExternalOES, size, flags, WTF::move(fence))
    , m_textureID(textureID)
{
}

#if USE(GSTREAMER) && USE(GBM)
CoordinatedPlatformLayerBufferExternalOES::CoordinatedPlatformLayerBufferExternalOES(GRefPtr<GstBuffer>&& buffer, uint32_t fourcc, CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace yuvColorSpace, SampleRange sampleRange, const IntSize& size, OptionSet<TextureMapperFlags> flags)
    : CoordinatedPlatformLayerBuffer(Type::ExternalOES, size, flags, nullptr)
    , m_fourcc(fourcc)
    , m_yuvColorSpace(yuvColorSpace)
    , m_sampleRange(sampleRange)
    , m_buffer(WTF::move(buffer))
{
}
#endif

CoordinatedPlatformLayerBufferExternalOES::~CoordinatedPlatformLayerBufferExternalOES() = default;

#if USE(GSTREAMER) && USE(GBM)
RefPtr<BitmapTexture> CoordinatedPlatformLayerBufferExternalOES::createExternalOESTexture()
{
    if (m_externalOESTexture)
        return m_externalOESTexture;

    auto memory = gst_buffer_peek_memory(m_buffer.get(), 0);
    if (!gst_is_fd_memory(memory)) [[unlikely]]
        return nullptr;

    int fd = gst_fd_memory_get_fd(memory);

    // Use stride and plane offsets from GstVideoMeta if available. The Qualcomm decoder
    // populates these from the C2HandleGBM with the exact values for the allocated GBM buffer,
    // including the correct UV plane offset (which accounts for slice height alignment).
    // Providing explicit plane 1 attributes avoids the EGL driver needing to consult the
    // GBM metadata buffer (meta_buffer_fd) to locate the UV plane, which it cannot access
    // when importing via EGL_LINUX_DMA_BUF_EXT, causing intermittent green frames.
    EGLint stride = WTF::roundUpToMultipleOf(128, m_size.width());
    EGLint uvStride = stride;
    EGLAttrib uvOffset = static_cast<EGLAttrib>(stride) * m_size.height();
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

    std::array<EGLAttrib, 6> plane1Attributes {
        EGL_DMA_BUF_PLANE1_FD_EXT, static_cast<EGLAttrib>(fd),
        EGL_DMA_BUF_PLANE1_OFFSET_EXT, uvOffset,
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

    auto colorSpaceHint = [&]() -> std::optional<EGLAttrib> {
        switch (m_yuvColorSpace) {
        case CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt601:
            return EGL_ITU_REC601_EXT;
        case CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt709:
            return EGL_ITU_REC709_EXT;
        case CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Bt2020:
            return EGL_ITU_REC2020_EXT;
        case CoordinatedPlatformLayerBufferYUV::YuvToRgbColorSpace::Smpte240M:
            // EGL has no token for it.
            return std::nullopt;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    auto attributesWithHints = attributes;
    if (colorSpaceHint) {
        attributesWithHints.appendList<EGLAttrib>({
            EGL_YUV_COLOR_SPACE_HINT_EXT, *colorSpaceHint,
            EGL_SAMPLE_RANGE_HINT_EXT, m_sampleRange == SampleRange::Full ? EGL_YUV_FULL_RANGE_EXT : EGL_YUV_NARROW_RANGE_EXT,
        });
    }
    attributesWithHints.append(EGL_NONE);
    attributes.append(EGL_NONE);

    auto image = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributesWithHints);
    if (!image && colorSpaceHint) [[unlikely]]
        image = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);

    if (!image) [[unlikely]] {
        LOG_ERROR("Cannot create EGLImage from the Qualcomm decoder dma-buf -- video will not render.");
        return nullptr;
    }

    OptionSet<BitmapTexture::Flags> textureFlags { BitmapTexture::Flags::ExternalOESRenderTarget };
    if (m_flags.contains(TextureMapperFlags::ShouldBlend))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
    m_externalOESTexture = BitmapTexturePool::singleton().createTextureForImage(image, m_size, textureFlags);
    display.destroyEGLImage(image);
    return m_externalOESTexture;
}
#endif // USE(GSTREAMER) && USE(GBM)

void CoordinatedPlatformLayerBufferExternalOES::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    waitForContentsIfNeeded();
    if (m_textureID) {
        textureMapper.drawTextureExternalOES(m_textureID, m_flags, targetRect, modelViewMatrix, opacity);
        return;
    }

#if USE(GSTREAMER) && USE(GBM)
    if (auto texture = createExternalOESTexture())
        textureMapper.drawTextureExternalOESYUV(texture->id(), m_flags, targetRect, modelViewMatrix, opacity);
#endif // USE(GSTREAMER) && USE(GBM)
}

#if USE(SKIA)
sk_sp<SkImage> CoordinatedPlatformLayerBufferExternalOES::skiaImage()
{
    waitForContentsIfNeeded();

    auto textureID = m_textureID;

#if USE(GSTREAMER) && USE(GBM)
    // The Qualcomm decoder gives us a GBM FD instead of a texture, so import it here.
    if (!textureID && m_buffer) {
        if (auto texture = createExternalOESTexture())
            textureID = texture->id();
    }
#endif // USE(GSTREAMER) && USE(GBM)

    if (!textureID)
        return nullptr;

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    ASSERT(grContext);
    GrGLTextureInfo externalTexture;
    externalTexture.fTarget = GL_TEXTURE_EXTERNAL_OES;
    externalTexture.fID = textureID;
    externalTexture.fFormat = GL_RGBA8;
    auto backendTexture = GrBackendTextures::MakeGL(m_size.width(), m_size.height(), skgpu::Mipmapped::kNo, externalTexture);
    auto origin = m_flags.contains(TextureMapperFlags::ShouldFlipTexture) ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin;
    auto alphaType = m_flags.contains(TextureMapperFlags::ShouldBlend) ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
    return SkImages::BorrowTextureFrom(grContext, backendTexture, origin, kRGBA_8888_SkColorType, alphaType, sRGBColorSpaceSingleton());
}
#endif

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
