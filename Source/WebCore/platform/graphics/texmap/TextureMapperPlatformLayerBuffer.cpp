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

TextureMapperPlatformLayerBuffer::TextureMapperPlatformLayerBuffer(RefPtr<BitmapTexture>&& texture, OptionSet<TextureMapperFlags> flags)
    : m_variant(RGBTexture { 0 })
    , m_texture(WTFMove(texture))
    , m_extraFlags(flags)
    , m_hasManagedTexture(true)
{
}

TextureMapperPlatformLayerBuffer::TextureMapperPlatformLayerBuffer(GLuint textureID, const IntSize& size, OptionSet<TextureMapperFlags> flags, GLint internalFormat)
    : TextureMapperPlatformLayerBuffer({ RGBTexture { textureID } }, size, flags, internalFormat)
{
}

TextureMapperPlatformLayerBuffer::TextureMapperPlatformLayerBuffer(TextureVariant&& variant, const IntSize& size, OptionSet<TextureMapperFlags> flags, GLint internalFormat)
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
    return m_texture && (m_texture->size() == size) && (m_texture->internalFormat() == internalFormat || internalFormat == GL_DONT_CARE);
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

            auto clonedTexture = BitmapTexture::create(m_size, { }, m_internalFormat);
            clonedTexture->copyFromExternalTexture(texture.id);
            return makeUnique<TextureMapperPlatformLayerBuffer>(WTFMove(clonedTexture), m_extraFlags);
        },
        [](const YUVTexture&) -> std::unique_ptr<TextureMapperPlatformLayerBuffer>
        {
            notImplemented();
            return nullptr;
        },
        [](const ExternalOESTexture&) -> std::unique_ptr<TextureMapperPlatformLayerBuffer>
        {
            notImplemented();
            return nullptr;
        });
}

void TextureMapperPlatformLayerBuffer::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    if (m_hasManagedTexture) {
        ASSERT(m_texture);
        textureMapper.drawTexture(m_texture->id(), m_extraFlags | m_texture->colorConvertFlags(), targetRect, modelViewMatrix, opacity);
        return;
    }

    if (m_extraFlags.contains(TextureMapperFlags::ShouldNotBlend)) {
        ASSERT(!m_texture);
        if (m_holePunchClient)
            m_holePunchClient->setVideoRectangle(enclosingIntRect(modelViewMatrix.mapRect(targetRect)));
        textureMapper.drawSolidColor(targetRect, modelViewMatrix, Color::transparentBlack, false);
        return;
    }

#if USE(GSTREAMER_GL)
    if (m_unmanagedBufferDataHolder)
        m_unmanagedBufferDataHolder->waitForCPUSync();
#endif // USE(GSTREAMER_GL)

    WTF::switchOn(m_variant,
        [&](const RGBTexture& texture) {
            ASSERT(texture.id);
            textureMapper.drawTexture(texture.id, m_extraFlags, targetRect, modelViewMatrix, opacity);
        },
        [&](const YUVTexture& texture) {
            switch (texture.numberOfPlanes) {
            case 1:
                ASSERT(texture.yuvPlane[0] == texture.yuvPlane[1] && texture.yuvPlane[1] == texture.yuvPlane[2]);
                ASSERT(texture.yuvPlaneOffset[0] == 2 && texture.yuvPlaneOffset[1] == 1 && !texture.yuvPlaneOffset[2]);
                textureMapper.drawTexturePackedYUV(texture.planes[texture.yuvPlane[0]],
                    texture.yuvToRgbMatrix, m_extraFlags, targetRect, modelViewMatrix, opacity);
                break;
            case 2:
                ASSERT(!texture.yuvPlaneOffset[0]);
                textureMapper.drawTextureSemiPlanarYUV(std::array<GLuint, 2> { texture.planes[texture.yuvPlane[0]], texture.planes[texture.yuvPlane[1]] }, !!texture.yuvPlaneOffset[1],
                    texture.yuvToRgbMatrix, m_extraFlags, targetRect, modelViewMatrix, opacity);
                break;
            case 3:
                ASSERT(!texture.yuvPlaneOffset[0] && !texture.yuvPlaneOffset[1] && !texture.yuvPlaneOffset[2]);
                textureMapper.drawTexturePlanarYUV(std::array<GLuint, 3> { texture.planes[texture.yuvPlane[0]], texture.planes[texture.yuvPlane[1]], texture.planes[texture.yuvPlane[2]] },
                    texture.yuvToRgbMatrix, m_extraFlags, targetRect, modelViewMatrix, opacity, std::nullopt);
                break;
            case 4:
                ASSERT(!texture.yuvPlaneOffset[0] && !texture.yuvPlaneOffset[1] && !texture.yuvPlaneOffset[2]);
                textureMapper.drawTexturePlanarYUV(std::array<GLuint, 3> { texture.planes[texture.yuvPlane[0]], texture.planes[texture.yuvPlane[1]], texture.planes[texture.yuvPlane[2]] },
                    texture.yuvToRgbMatrix, m_extraFlags, targetRect, modelViewMatrix, opacity, texture.planes[texture.yuvPlane[3]]);
                break;
            }
        },
        [&](const ExternalOESTexture& texture) {
            ASSERT(texture.id);
            textureMapper.drawTextureExternalOES(texture.id, m_extraFlags, targetRect, modelViewMatrix, opacity);
        });
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
