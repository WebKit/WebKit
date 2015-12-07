/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "TextureMapperPlatformLayerBuffer.h"

namespace WebCore {

TextureMapperPlatformLayerBuffer::TextureMapperPlatformLayerBuffer(RefPtr<BitmapTexture>&& texture)
    : m_texture(WTF::move(texture))
    , m_textureID(0)
    , m_extraFlags(0)
    , m_hasManagedTexture(true)
{
}

TextureMapperPlatformLayerBuffer::TextureMapperPlatformLayerBuffer(GLuint textureID, const IntSize& size, TextureMapperGL::Flags flags)
    : m_textureID(textureID)
    , m_size(size)
    , m_extraFlags(flags)
    , m_hasManagedTexture(false)
{
}

bool TextureMapperPlatformLayerBuffer::canReuseWithoutReset(const IntSize& size, GC3Dint internalFormat)
{
    return m_texture && (m_texture->size() == size) && (static_cast<BitmapTextureGL*>(m_texture.get())->internalFormat() == internalFormat || internalFormat == GraphicsContext3D::DONT_CARE);
}

void TextureMapperPlatformLayerBuffer::paintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    if (m_hasManagedTexture) {
        ASSERT(m_texture);
        textureMapper->drawTexture(*m_texture, targetRect, modelViewMatrix, opacity);
        return;
    }

    ASSERT(m_textureID);
    TextureMapperGL* texmapGL = static_cast<TextureMapperGL*>(textureMapper);
    texmapGL->drawTexture(m_textureID, m_extraFlags, m_size, targetRect, modelViewMatrix, opacity);
}

} // namespace WebCore
