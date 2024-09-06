/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "CoordinatedPlatformLayerBufferDMABuf.h"

#if USE(COORDINATED_GRAPHICS) && USE(GBM)
#include "BitmapTexture.h"
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "DMABufBuffer.h"
#include "PlatformDisplay.h"
#include "TextureMapper.h"
#include <drm_fourcc.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> CoordinatedPlatformLayerBufferDMABuf::create(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
{
    return makeUnique<CoordinatedPlatformLayerBufferDMABuf>(WTFMove(dmabuf), flags, WTFMove(fence));
}

CoordinatedPlatformLayerBufferDMABuf::CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&& dmabuf, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::DMABuf, dmabuf->attributes().size, flags, WTFMove(fence))
    , m_dmabuf(WTFMove(dmabuf))
{
}

CoordinatedPlatformLayerBufferDMABuf::~CoordinatedPlatformLayerBufferDMABuf() = default;

std::unique_ptr<CoordinatedPlatformLayerBuffer> CoordinatedPlatformLayerBufferDMABuf::importDMABuf(TextureMapper& textureMapper) const
{
    auto& display = PlatformDisplay::sharedDisplay();
    auto image = [&](const DMABufBuffer::Attributes& buffer) -> EGLImage {
        Vector<EGLAttrib> attributes = {
            EGL_WIDTH, buffer.size.width(),
            EGL_HEIGHT, buffer.size.height(),
            EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(buffer.fourcc)
        };

#define ADD_PLANE_ATTRIBUTES(planeIndex) { \
    std::array<EGLAttrib, 6> planeAttributes { \
        EGL_DMA_BUF_PLANE##planeIndex##_FD_EXT, buffer.fds[planeIndex].value(), \
        EGL_DMA_BUF_PLANE##planeIndex##_OFFSET_EXT, static_cast<EGLAttrib>(buffer.offsets[planeIndex]), \
        EGL_DMA_BUF_PLANE##planeIndex##_PITCH_EXT, static_cast<EGLAttrib>(buffer.strides[planeIndex]) \
    }; \
    attributes.append(std::span<const EGLAttrib> { planeAttributes }); \
    if (buffer.modifier != DRM_FORMAT_MOD_INVALID && display.eglExtensions().EXT_image_dma_buf_import_modifiers) { \
        std::array<EGLAttrib, 4> modifierAttributes { \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_HI_EXT, static_cast<EGLAttrib>(buffer.modifier >> 32), \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_LO_EXT, static_cast<EGLAttrib>(buffer.modifier & 0xffffffff) \
        }; \
        attributes.append(std::span<const EGLAttrib> { modifierAttributes }); \
    } \
    }

        auto planeCount = buffer.fds.size();
        if (planeCount > 0)
            ADD_PLANE_ATTRIBUTES(0);
        if (planeCount > 1)
            ADD_PLANE_ATTRIBUTES(1);
        if (planeCount > 2)
            ADD_PLANE_ATTRIBUTES(2);
        if (planeCount > 3)
            ADD_PLANE_ATTRIBUTES(3);

#undef ADD_PLANE_ATTRIBUTES

        attributes.append(EGL_NONE);

        return display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    }(m_dmabuf->attributes());

    if (!image)
        return nullptr;

    OptionSet<BitmapTexture::Flags> textureFlags;
    if (m_flags.contains(TextureMapperFlags::ShouldBlend))
        textureFlags.add(BitmapTexture::Flags::SupportsAlpha);
    auto texture = textureMapper.acquireTextureFromPool(m_size, textureFlags);
    glBindTexture(GL_TEXTURE_2D, texture->id());
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    display.destroyEGLImage(image);

    return CoordinatedPlatformLayerBufferRGB::create(WTFMove(texture), m_flags, nullptr);
}

void CoordinatedPlatformLayerBufferDMABuf::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    waitForContentsIfNeeded();

    if (!m_dmabuf->buffer())
        m_dmabuf->setBuffer(importDMABuf(textureMapper));

    if (auto* buffer = m_dmabuf->buffer())
        buffer->paintToTextureMapper(textureMapper, targetRect, modelViewMatrix, opacity);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(GBM)
