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

#if USE(NICOSIA) && USE(TEXTURE_MAPPER)

#if USE(COORDINATED_GRAPHICS)
#include "TextureMapperGL.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxy.h"
#endif

#if USE(ANGLE)
#include "ImageBuffer.h"
#endif

#include "GLContext.h"

namespace Nicosia {

using namespace WebCore;

GCGLLayer::GCGLLayer(GraphicsContextGLOpenGL& context)
    : m_context(context)
    , m_contentLayer(Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this)))
{
}

GCGLLayer::GCGLLayer(GraphicsContextGLOpenGL& context, GraphicsContextGLOpenGL::Destination destination)
    : m_context(context)
    , m_contentLayer(Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this)))
{
    switch (destination) {
    case GraphicsContextGLOpenGL::Destination::Offscreen:
        m_glContext = GLContext::createOffscreenContext(&PlatformDisplay::sharedDisplayForCompositing());
        break;
    case GraphicsContextGLOpenGL::Destination::DirectlyToHostWindow:
        ASSERT_NOT_REACHED();
        break;
    }
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
    TextureMapperGL::Flags flags = m_context.contextAttributes().alpha ? TextureMapperGL::ShouldBlend : 0;
#if USE(ANGLE)
    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(textureSize, RenderingMode::Unaccelerated);
    if (!imageBuffer)
        return;

    m_context.paintRenderingResultsToCanvas(imageBuffer.get());
    RefPtr<Image> image = imageBuffer->copyImage(DontCopyBackingStore);
    if (!image)
        return;
#else
    flags |= TextureMapperGL::ShouldFlipTexture;
#endif

    {
        auto& proxy = downcast<Nicosia::ContentLayerTextureMapperImpl>(m_contentLayer->impl()).proxy();
#if USE(ANGLE)
        std::unique_ptr<TextureMapperPlatformLayerBuffer> layerBuffer;
        layerBuffer = proxy.getAvailableBuffer(textureSize, m_context.m_internalColorFormat);
        if (!layerBuffer) {
            auto texture = BitmapTextureGL::create(TextureMapperContextAttributes::get(), flags, m_context.m_internalColorFormat);
            static_cast<BitmapTextureGL&>(texture.get()).setPendingContents(WTFMove(image));
            layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(WTFMove(texture), flags);
        } else
            layerBuffer->textureGL().setPendingContents(WTFMove(image));

        LockHolder holder(proxy.lock());
        proxy.pushNextBuffer(WTFMove(layerBuffer));
#else
        LockHolder holder(proxy.lock());
        proxy.pushNextBuffer(makeUnique<TextureMapperPlatformLayerBuffer>(m_context.m_compositorTexture, textureSize, flags, m_context.m_internalColorFormat));
#endif
    }

    m_context.markLayerComposited();
#endif
}

} // namespace Nicosia

#endif // USE(NICOSIA) && USE(TEXTURE_MAPPER)
