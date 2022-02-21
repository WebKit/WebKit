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

#include "config.h"
#include "TextureMapperPlatformLayerDmabuf.h"

#if USE(ANGLE) && USE(NICOSIA)

#include "GLContextEGL.h"

#if USE(LIBEPOXY)
#include "EpoxyEGL.h"
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#include <unistd.h>

namespace WebCore {

static constexpr uint32_t createFourCC(char a, char b, char c, char d)
{
    return uint32_t(a) | (uint32_t(b) << 8) | (uint32_t(c) << 16) | (uint32_t(d) << 24);
}

static GLint fourCCToGLFormat(uint32_t format)
{
    ASSERT((format == createFourCC('X', 'R', '2', '4')) || (format == createFourCC('A', 'R', '2', '4')));
    return (format == createFourCC('A', 'R', '2', '4')) ? GL_RGBA : GL_RGB;
}

static bool formatHasAlpha(uint32_t format)
{
    ASSERT((format == createFourCC('X', 'R', '2', '4')) || (format == createFourCC('A', 'R', '2', '4')));
    return (format == createFourCC('A', 'R', '2', '4')) ? true : false;
}

TextureMapperPlatformLayerDmabuf::TextureMapperPlatformLayerDmabuf(const IntSize& size, uint32_t format, uint32_t stride, int fd)
    : TextureMapperPlatformLayerBuffer({ RGBTexture { 0 } }, size, TextureMapperGL::ShouldFlipTexture | (formatHasAlpha(format) ? TextureMapperGL::ShouldBlend : 0), fourCCToGLFormat(format))
    , m_format(format)
    , m_stride(stride)
    , m_fd(dup(fd))
{
}

TextureMapperPlatformLayerDmabuf::~TextureMapperPlatformLayerDmabuf()
{
    ASSERT(std::holds_alternative<RGBTexture>(m_variant));
    auto& texture = std::get<RGBTexture>(m_variant);
    if (texture.id)
        glDeleteTextures(1, &texture.id);
    close(m_fd);
}

void TextureMapperPlatformLayerDmabuf::validateTexture()
{
    ASSERT(std::holds_alternative<RGBTexture>(m_variant));
    auto& texture = std::get<RGBTexture>(m_variant);
    if (texture.id)
        return;

    auto* context = GLContext::current();
    ASSERT(context->isEGLContext());

    context->makeContextCurrent();

    auto size = TextureMapperPlatformLayerBuffer::size();
    Vector<EGLAttrib> imageAttributes {
        EGL_WIDTH, size.width(),
        EGL_HEIGHT, size.height(),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(m_format),
        EGL_DMA_BUF_PLANE0_FD_EXT, m_fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLAttrib>(m_stride),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_NONE
    };
    auto image = downcast<GLContextEGL>(*context).createImage(EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)nullptr, imageAttributes);
    if (!image)
        return;

    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    RELEASE_ASSERT(downcast<GLContextEGL>(*context).destroyImage(image));
}

void TextureMapperPlatformLayerDmabuf::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    validateTexture();
    TextureMapperPlatformLayerBuffer::paintToTextureMapper(textureMapper, targetRect, modelViewMatrix, opacity);
}

std::unique_ptr<TextureMapperPlatformLayerBuffer> TextureMapperPlatformLayerDmabuf::clone()
{
    validateTexture();
    return TextureMapperPlatformLayerBuffer::clone();
}

} // namespace WebCore

#endif // USE(ANGLE) && USE(NICOSIA)
