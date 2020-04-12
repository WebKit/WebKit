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
    , m_variant(RGBTexture { 0 })
    , m_extraFlags(flags)
    , m_hasManagedTexture(true)
{
}

TextureMapperPlatformLayerBuffer::TextureMapperPlatformLayerBuffer(GLuint textureID, const IntSize& size, TextureMapperGL::Flags flags, GLint internalFormat)
    : TextureMapperPlatformLayerBuffer({ RGBTexture { textureID } }, size, flags, internalFormat)
{
}

TextureMapperPlatformLayerBuffer::TextureMapperPlatformLayerBuffer(TextureVariant&& variant, const IntSize& size, TextureMapperGL::Flags flags, GLint internalFormat)
    : m_variant(WTFMove(variant))
    , m_size(size)
    , m_internalFormat(internalFormat)
    , m_extraFlags(flags)
    , m_hasManagedTexture(false)
{
}

TextureMapperPlatformLayerBuffer::~TextureMapperPlatformLayerBuffer()
{
}

bool TextureMapperPlatformLayerBuffer::canReuseWithoutReset(const IntSize& size, GLint internalFormat)
{
    return m_texture && (m_texture->size() == size) && (static_cast<BitmapTextureGL*>(m_texture.get())->internalFormat() == internalFormat || internalFormat == GL_DONT_CARE);
}

std::unique_ptr<TextureMapperPlatformLayerBuffer> TextureMapperPlatformLayerBuffer::clone()
{
    if (m_hasManagedTexture) {
        notImplemented();
        return nullptr;
    }

    return WTF::switchOn(m_variant,
        [&](const RGBTexture& texture) mutable -> std::unique_ptr<TextureMapperPlatformLayerBuffer> {
            if (!texture.id) {
                notImplemented();
                return nullptr;
            }

            auto clonedTexture = BitmapTextureGL::create(TextureMapperContextAttributes::get(), m_internalFormat);
            clonedTexture->reset(m_size);
            static_cast<BitmapTextureGL&>(clonedTexture.get()).copyFromExternalTexture(texture.id);
            return makeUnique<TextureMapperPlatformLayerBuffer>(WTFMove(clonedTexture), m_extraFlags);
        },
        [](const YUVTexture&)
        {
            notImplemented();
            return nullptr;
        },
        [](const ExternalOESTexture&)
        {
            notImplemented();
            return nullptr;
        });
}

void TextureMapperPlatformLayerBuffer::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    TextureMapperGL& texmapGL = static_cast<TextureMapperGL&>(textureMapper);

    if (m_hasManagedTexture) {
        ASSERT(m_texture);
        BitmapTextureGL* textureGL = static_cast<BitmapTextureGL*>(m_texture.get());
#if USE(ANGLE)
        textureGL->updatePendingContents(IntRect(IntPoint(), textureGL->contentSize()), IntPoint());
#endif
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

#if USE(GSTREAMER_GL)
    if (m_unmanagedBufferDataHolder)
        m_unmanagedBufferDataHolder->waitForCPUSync();
#endif // USE(GSTREAMER_GL)

    WTF::switchOn(m_variant,
        [&](const RGBTexture& texture) {
            ASSERT(texture.id);
            texmapGL.drawTexture(texture.id, m_extraFlags, m_size, targetRect, modelViewMatrix, opacity);
        },
        [&](const YUVTexture& texture) {
            switch (texture.numberOfPlanes) {
            case 1:
                ASSERT(texture.yuvPlane[0] == texture.yuvPlane[1] && texture.yuvPlane[1] == texture.yuvPlane[2]);
                ASSERT(texture.yuvPlaneOffset[0] == 2 && texture.yuvPlaneOffset[1] == 1 && !texture.yuvPlaneOffset[2]);
                texmapGL.drawTexturePackedYUV(texture.planes[texture.yuvPlane[0]],
                    texture.yuvToRgbMatrix, m_extraFlags, m_size, targetRect, modelViewMatrix, opacity);
                break;
            case 2:
                ASSERT(!texture.yuvPlaneOffset[0]);
                texmapGL.drawTextureSemiPlanarYUV(std::array<GLuint, 2> { texture.planes[texture.yuvPlane[0]], texture.planes[texture.yuvPlane[1]] }, !!texture.yuvPlaneOffset[1],
                    texture.yuvToRgbMatrix, m_extraFlags, m_size, targetRect, modelViewMatrix, opacity);
                break;
            case 3:
                ASSERT(!texture.yuvPlaneOffset[0] && !texture.yuvPlaneOffset[1] && !texture.yuvPlaneOffset[2]);
                texmapGL.drawTexturePlanarYUV(std::array<GLuint, 3> { texture.planes[texture.yuvPlane[0]], texture.planes[texture.yuvPlane[1]], texture.planes[texture.yuvPlane[2]] },
                    texture.yuvToRgbMatrix, m_extraFlags, m_size, targetRect, modelViewMatrix, opacity);
                break;
            }
        },
        [&](const ExternalOESTexture& texture) {
            ASSERT(texture.id);
            texmapGL.drawTextureExternalOES(texture.id, m_extraFlags, m_size, targetRect, modelViewMatrix, opacity);
        });
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
