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
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <gst/allocators/gstfdmemory.h>
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

    EGLint stride = WTF::roundUpToMultipleOf(128, m_size.width());
    Vector<EGLAttrib> attributes {
        EGL_WIDTH, m_size.width(),
        EGL_HEIGHT, m_size.height(),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(m_fourcc),
        EGL_DMA_BUF_PLANE0_FD_EXT, gst_fd_memory_get_fd(memory),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
        EGL_NONE
    };

    auto& display = PlatformDisplay::sharedDisplay();
    auto image = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    if (!image) [[unlikely]]
        return;

    OptionSet<BitmapTexture::Flags> textureFlags { BitmapTexture::Flags::ExternalOESRenderTarget };
    if (m_flags.contains(TextureMapperFlags::ShouldBlend))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
    auto texture = BitmapTexturePool::singleton().createTextureForImage(image, textureFlags);
    textureMapper.drawTextureExternalOESYUV(texture->id(), m_flags, targetRect, modelViewMatrix, opacity);
    display.destroyEGLImage(image);
#endif // USE(GSTREAMER) && USE(GBM)
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
