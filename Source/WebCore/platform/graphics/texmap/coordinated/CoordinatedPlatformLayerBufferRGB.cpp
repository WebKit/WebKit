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
#include "CoordinatedPlatformLayerBufferRGB.h"

#if USE(COORDINATED_GRAPHICS)
#include "BitmapTexture.h"
#include "TextureMapper.h"

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferRGB> CoordinatedPlatformLayerBufferRGB::create(Ref<BitmapTexture>&& texture, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
{
    return makeUnique<CoordinatedPlatformLayerBufferRGB>(WTFMove(texture), flags, WTFMove(fence));
}

std::unique_ptr<CoordinatedPlatformLayerBufferRGB> CoordinatedPlatformLayerBufferRGB::create(unsigned textureID, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
{
    return makeUnique<CoordinatedPlatformLayerBufferRGB>(textureID, size, flags, WTFMove(fence));
}

CoordinatedPlatformLayerBufferRGB::CoordinatedPlatformLayerBufferRGB(Ref<BitmapTexture>&& texture, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::RGB, texture->size(), flags, WTFMove(fence))
    , m_texture(WTFMove(texture))
{
}

CoordinatedPlatformLayerBufferRGB::CoordinatedPlatformLayerBufferRGB(unsigned textureID, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
    : CoordinatedPlatformLayerBuffer(Type::RGB, size, flags, WTFMove(fence))
    , m_textureID(textureID)
{
}

CoordinatedPlatformLayerBufferRGB::~CoordinatedPlatformLayerBufferRGB() = default;

void CoordinatedPlatformLayerBufferRGB::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    waitForContentsIfNeeded();

    if (m_texture)
        textureMapper.drawTexture(m_texture->id(), m_flags | m_texture->colorConvertFlags(), targetRect, modelViewMatrix, opacity);
    else
        textureMapper.drawTexture(m_textureID, m_flags, targetRect, modelViewMatrix, opacity);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
