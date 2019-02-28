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

#if USE(COORDINATED_GRAPHICS)

#include "FloatRect.h"
#include "NotImplemented.h"

namespace WebCore {

TextureMapperPlatformLayerBuffer::TextureMapperPlatformLayerBuffer(RefPtr<BitmapTexture>&& texture, TextureMapperGL::Flags flags)
    : m_texture(WTFMove(texture))
    , m_textureID(0)
    , m_extraFlags(flags)
    , m_hasManagedTexture(true)
{
}

TextureMapperPlatformLayerBuffer::TextureMapperPlatformLayerBuffer(GLuint textureID, const IntSize& size, TextureMapperGL::Flags flags, GLint internalFormat)
    : m_textureID(textureID)
    , m_size(size)
    , m_internalFormat(internalFormat)
    , m_extraFlags(flags)
    , m_hasManagedTexture(false)
{
}

bool TextureMapperPlatformLayerBuffer::canReuseWithoutReset(const IntSize& size, GLint internalFormat)
{
    return m_texture && (m_texture->size() == size) && (static_cast<BitmapTextureGL*>(m_texture.get())->internalFormat() == internalFormat || internalFormat == GL_DONT_CARE);
}

std::unique_ptr<TextureMapperPlatformLayerBuffer> TextureMapperPlatformLayerBuffer::clone()
{
    if (m_hasManagedTexture || !m_textureID) {
        notImplemented();
        return nullptr;
    }
    auto texture = BitmapTextureGL::create(TextureMapperContextAttributes::get(), m_internalFormat);
    texture->reset(m_size);
    static_cast<BitmapTextureGL&>(texture.get()).copyFromExternalTexture(m_textureID);
    return std::make_unique<TextureMapperPlatformLayerBuffer>(WTFMove(texture), m_extraFlags);
}

void TextureMapperPlatformLayerBuffer::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    TextureMapperGL& texmapGL = static_cast<TextureMapperGL&>(textureMapper);

    if (m_hasManagedTexture) {
        ASSERT(m_texture);
        BitmapTextureGL* textureGL = static_cast<BitmapTextureGL*>(m_texture.get());
        texmapGL.drawTexture(textureGL->id(), m_extraFlags | textureGL->colorConvertFlags(), textureGL->size(), targetRect, modelViewMatrix, opacity);
        return;
    }

    if (m_extraFlags & TextureMapperGL::ShouldNotBlend) {
        ASSERT(!m_texture);
        if (m_holePunchClient)
            m_holePunchClient->setVideoRectangle(enclosingIntRect(modelViewMatrix.mapRect(targetRect)));
        texmapGL.drawSolidColor(targetRect, modelViewMatrix, Color(0, 0, 0, 0), false);
        return;
    }

    ASSERT(m_textureID);
    texmapGL.drawTexture(m_textureID, m_extraFlags, m_size, targetRect, modelViewMatrix, opacity);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
