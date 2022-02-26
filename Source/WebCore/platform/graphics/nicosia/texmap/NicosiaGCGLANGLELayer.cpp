/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018, 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NicosiaGCGLANGLELayer.h"

#if USE(NICOSIA) && USE(TEXTURE_MAPPER)

#include "ANGLEHeaders.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "TextureMapperGL.h"
#include "TextureMapperPlatformLayerDmabuf.h"
#include "TextureMapperPlatformLayerProxyGL.h"

namespace Nicosia {

using namespace WebCore;

void GCGLANGLELayer::swapBuffersIfNeeded()
{
    if (m_context.layerComposited())
        return;

    m_context.prepareTexture();

    auto& proxy = downcast<Nicosia::ContentLayerTextureMapperImpl>(contentLayer().impl()).proxy();
    auto size = m_context.getInternalFramebufferSize();

    if (m_context.m_compositorTextureBacking) {
        if (m_context.m_compositorTextureBacking->isReleased())
            return;

        auto format = m_context.m_compositorTextureBacking->format();
        auto stride = m_context.m_compositorTextureBacking->stride();
        auto fd = m_context.m_compositorTextureBacking->fd();

        {
            Locker locker { proxy.lock() };
            ASSERT(is<TextureMapperPlatformLayerProxyGL>(proxy));
            downcast<TextureMapperPlatformLayerProxyGL>(proxy).pushNextBuffer(makeUnique<TextureMapperPlatformLayerDmabuf>(size, format, stride, fd));
        }

        m_context.markLayerComposited();
        return;
    }

    // Fallback path, read back texture to main memory
    RefPtr<WebCore::ImageBuffer> imageBuffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return;
    m_context.paintRenderingResultsToCanvas(*imageBuffer.get());

    auto flags = m_context.contextAttributes().alpha ? BitmapTexture::SupportsAlpha : BitmapTexture::NoFlag;
    {
        Locker locker { proxy.lock() };
        ASSERT(is<TextureMapperPlatformLayerProxyGL>(proxy));
        std::unique_ptr<TextureMapperPlatformLayerBuffer> layerBuffer = downcast<TextureMapperPlatformLayerProxyGL>(proxy).getAvailableBuffer(size, m_context.m_internalColorFormat);
        if (!layerBuffer) {
            auto texture = BitmapTextureGL::create(TextureMapperContextAttributes::get(), flags, m_context.m_internalColorFormat);
            layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(WTFMove(texture), flags);
        }

        layerBuffer->textureGL().setPendingContents(ImageBuffer::sinkIntoImage(WTFMove(imageBuffer)));
        downcast<TextureMapperPlatformLayerProxyGL>(proxy).pushNextBuffer(WTFMove(layerBuffer));
    }
    m_context.markLayerComposited();
}

GCGLANGLELayer::GCGLANGLELayer(GraphicsContextGLANGLE& context)
    : m_context(context)
    , m_contentLayer(Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this)))
{
}

GCGLANGLELayer::~GCGLANGLELayer()
{
    downcast<ContentLayerTextureMapperImpl>(m_contentLayer->impl()).invalidateClient();
}

} // namespace Nicosia

#endif // USE(NICOSIA) && USE(TEXTURE_MAPPER)
