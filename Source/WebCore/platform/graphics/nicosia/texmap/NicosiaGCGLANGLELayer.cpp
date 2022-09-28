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

#include "GraphicsContextGLFallback.h"
#include "GraphicsContextGLGBM.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "TextureMapperGL.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxyDMABuf.h"
#include "TextureMapperPlatformLayerProxyGL.h"

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#endif

namespace Nicosia {

using namespace WebCore;

void GCGLANGLELayer::swapBuffersIfNeeded()
{
    auto& proxy = downcast<Nicosia::ContentLayerTextureMapperImpl>(contentLayer().impl()).proxy();

#if USE(TEXTURE_MAPPER_DMABUF)
    if (is<TextureMapperPlatformLayerProxyDMABuf>(proxy)) {
        RELEASE_ASSERT(m_contextType == ContextType::Gbm);
        auto& swapchain = static_cast<GraphicsContextGLGBM&>(m_context).swapchain();
        auto bo = WTFMove(swapchain.displayBO);
        if (bo) {
            Locker locker { proxy.lock() };

            TextureMapperGL::Flags flags = TextureMapperGL::ShouldFlipTexture;
            if (m_context.contextAttributes().alpha)
                flags |= TextureMapperGL::ShouldBlend;

            downcast<TextureMapperPlatformLayerProxyDMABuf>(proxy).pushDMABuf(
                DMABufObject(reinterpret_cast<uintptr_t>(swapchain.swapchain.get()) + bo->handle()),
                [&](auto&& object) {
                    return bo->createDMABufObject(object.handle);
                }, flags);
        }
        return;
    }
#endif

    {
        // Fallback path, read back texture to main memory
        ASSERT(is<TextureMapperPlatformLayerProxyGL>(proxy));
        RELEASE_ASSERT(m_contextType == ContextType::Fallback);

        auto& context = static_cast<GraphicsContextGLFallback&>(m_context);
        auto size = context.getInternalFramebufferSize();
        RefPtr<WebCore::ImageBuffer> imageBuffer = ImageBuffer::create(size, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
        if (!imageBuffer)
            return;

        context.paintRenderingResultsToCanvas(*imageBuffer.get());

        auto flags = context.contextAttributes().alpha ? BitmapTexture::SupportsAlpha : BitmapTexture::NoFlag;
        auto proxyOperation = [&proxy, size, flags, imageBuffer = WTFMove(imageBuffer)] () mutable {

            Locker locker { proxy.lock() };
            if (!proxy.isActive())
                return;

            std::unique_ptr<TextureMapperPlatformLayerBuffer> layerBuffer = downcast<TextureMapperPlatformLayerProxyGL>(proxy).getAvailableBuffer(size, GL_DONT_CARE);
            if (!layerBuffer) {
                auto texture = BitmapTextureGL::create(TextureMapperContextAttributes::get(), flags, GL_DONT_CARE);
                layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(WTFMove(texture), flags);
            }

            layerBuffer->textureGL().setPendingContents(ImageBuffer::sinkIntoImage(WTFMove(imageBuffer)));
            downcast<TextureMapperPlatformLayerProxyGL>(proxy).pushNextBuffer(WTFMove(layerBuffer));
        };

        downcast<TextureMapperPlatformLayerProxyGL>(proxy).scheduleUpdateOnCompositorThread([proxyOperation] () mutable { proxyOperation(); });
    }
}

GCGLANGLELayer::GCGLANGLELayer(GraphicsContextGLFallback& context)
    : m_contextType(ContextType::Fallback)
    , m_context(context)
    , m_contentLayer(Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this, adoptRef(*new TextureMapperPlatformLayerProxyGL))))
{
}

#if USE(LIBGBM)
GCGLANGLELayer::GCGLANGLELayer(GraphicsContextGLGBM& context)
    : m_contextType(ContextType::Gbm)
    , m_context(context)
    , m_contentLayer(Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this, adoptRef(*new TextureMapperPlatformLayerProxyDMABuf))))
{
}
#endif

GCGLANGLELayer::~GCGLANGLELayer()
{
    downcast<ContentLayerTextureMapperImpl>(m_contentLayer->impl()).invalidateClient();
}

} // namespace Nicosia

#endif // USE(NICOSIA) && USE(TEXTURE_MAPPER)
