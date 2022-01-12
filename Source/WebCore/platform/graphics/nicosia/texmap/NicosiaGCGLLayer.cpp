/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
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
#include "NicosiaGCGLLayer.h"

#if ENABLE(WEBGL) && USE(NICOSIA) && USE(TEXTURE_MAPPER)

#if USE(COORDINATED_GRAPHICS)
#include "TextureMapperGL.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxy.h"
#endif

#if USE(ANGLE)
#include "ImageBuffer.h"
#if USE(CAIRO)
#include <cairo.h>
#endif
#endif

#include "GLContext.h"

namespace Nicosia {

using namespace WebCore;

GCGLLayer::GCGLLayer(GraphicsContextGLType& context)
    : m_context(context)
    , m_contentLayer(Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this)))
{
    m_glContext = GLContext::createOffscreenContext(&PlatformDisplay::sharedDisplayForCompositing());
}

GCGLLayer::~GCGLLayer()
{
    downcast<ContentLayerTextureMapperImpl>(m_contentLayer->impl()).invalidateClient();
}

bool GCGLLayer::makeContextCurrent()
{
    ASSERT(m_glContext);
    return m_glContext->makeContextCurrent();
}

PlatformGraphicsContextGL GCGLLayer::platformContext() const
{
    ASSERT(m_glContext);
    return m_glContext->platformContext();
}

void GCGLLayer::swapBuffersIfNeeded()
{
#if USE(COORDINATED_GRAPHICS)
    if (m_context.layerComposited())
        return;

    m_context.prepareTexture();
    IntSize textureSize(m_context.m_currentWidth, m_context.m_currentHeight);
#if USE(ANGLE)
    RefPtr<WebCore::ImageBuffer> imageBuffer = ImageBuffer::create(textureSize, RenderingMode::Unaccelerated, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return;

    m_context.paintRenderingResultsToCanvas(*imageBuffer.get());
#else
    TextureMapperGL::Flags flags = TextureMapperGL::ShouldFlipTexture;
    if (m_context.contextAttributes().alpha)
        flags |= TextureMapperGL::ShouldBlend;
#endif

    {
        auto& proxy = downcast<Nicosia::ContentLayerTextureMapperImpl>(m_contentLayer->impl()).proxy();
#if USE(ANGLE)
        // FIXME: This is duplicated from NicosiaImageBufferPipe, but should be shared somehow.
        auto proxyOperation =
            [this, imageBuffer = WTFMove(imageBuffer)] () mutable {
                auto nativeImage = ImageBuffer::sinkIntoNativeImage(WTFMove(imageBuffer));
                if (!nativeImage)
                    return;

                auto size = nativeImage->size();
                auto flags = nativeImage->hasAlpha() ? BitmapTexture::SupportsAlpha : BitmapTexture::NoFlag;

                auto& proxy = downcast<Nicosia::ContentLayerTextureMapperImpl>(m_contentLayer->impl()).proxy();
                Locker locker { proxy.lock() };

                std::unique_ptr<TextureMapperPlatformLayerBuffer> layerBuffer = proxy.getAvailableBuffer(size, flags);

                if (!layerBuffer) {
                    auto texture = BitmapTextureGL::create(TextureMapperContextAttributes::get());
                    texture->reset(size, flags);
                    layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(WTFMove(texture), nativeImage->hasAlpha() ? TextureMapperGL::ShouldBlend : TextureMapperGL::NoFlag);
                }

#if USE(CAIRO)
                auto* surface = nativeImage->platformImage().get();
                auto* imageData = cairo_image_surface_get_data(surface);
                layerBuffer->textureGL().updateContents(imageData, IntRect(IntPoint(), size), IntPoint(), cairo_image_surface_get_stride(surface));
#else
                notImplemented();
#endif

                proxy.pushNextBuffer(WTFMove(layerBuffer));

                m_context.markLayerComposited();
            };

        proxy.scheduleUpdateOnCompositorThread([proxyOperation] () mutable {
            proxyOperation();
        });

        return;
#else
        Locker locker { proxy.lock() };
        proxy.pushNextBuffer(makeUnique<TextureMapperPlatformLayerBuffer>(m_context.m_compositorTexture, textureSize, flags, m_context.m_internalColorFormat));
#endif
    }

    m_context.markLayerComposited();
#endif
}

} // namespace Nicosia

#endif // ENABLE(WEBGL) && USE(NICOSIA) && USE(TEXTURE_MAPPER)
