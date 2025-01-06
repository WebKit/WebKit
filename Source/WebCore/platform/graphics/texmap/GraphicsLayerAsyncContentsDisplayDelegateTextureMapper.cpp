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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsLayerAsyncContentsDisplayDelegateTextureMapper.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedPlatformLayerBufferNativeImage.h"
#include "GraphicsLayer.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "TextureMapperFlags.h"
#include "TextureMapperPlatformLayerProxy.h"

namespace WebCore {

GraphicsLayerAsyncContentsDisplayDelegateTextureMapper::GraphicsLayerAsyncContentsDisplayDelegateTextureMapper(GraphicsLayer& layer)
    : m_proxy(TextureMapperPlatformLayerProxy::create(TextureMapperPlatformLayerProxy::ContentType::OffscreenCanvas))
{
    layer.setContentsToPlatformLayer(m_proxy.ptr(), GraphicsLayer::ContentsLayerPurpose::Canvas);
}

GraphicsLayerAsyncContentsDisplayDelegateTextureMapper::~GraphicsLayerAsyncContentsDisplayDelegateTextureMapper() = default;

bool GraphicsLayerAsyncContentsDisplayDelegateTextureMapper::tryCopyToLayer(ImageBuffer& imageBuffer)
{
    auto image = ImageBuffer::sinkIntoNativeImage(imageBuffer.clone());
    if (!image)
        return false;

    m_proxy->pushNextBuffer(CoordinatedPlatformLayerBufferNativeImage::create(image.releaseNonNull(), nullptr));
    return true;
}

void GraphicsLayerAsyncContentsDisplayDelegateTextureMapper::updateGraphicsLayer(GraphicsLayer& layer)
{
    layer.setContentsToPlatformLayer(m_proxy.ptr(), GraphicsLayer::ContentsLayerPurpose::Canvas);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
