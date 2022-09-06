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

#if ENABLE(WEBGL) && USE(NICOSIA) && USE(TEXTURE_MAPPER) && !USE(ANGLE)

#if USE(COORDINATED_GRAPHICS)
#include "TextureMapperGL.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxyGL.h"
#endif

#include "GLContext.h"

namespace Nicosia {

using namespace WebCore;

std::unique_ptr<GCGLLayer> GCGLLayer::create(WebCore::GraphicsContextGLOpenGL& context)
{
    if (auto glContext = GLContext::createOffscreenContext(&PlatformDisplay::sharedDisplayForCompositing()))
        return makeUnique<GCGLLayer>(context, WTFMove(glContext));
    return nullptr;
}

GCGLLayer::GCGLLayer(GraphicsContextGLOpenGL& context, std::unique_ptr<WebCore::GLContext>&& glContext)
    : m_context(context)
    , m_glContext(WTFMove(glContext))
    , m_contentLayer(Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this)))
{
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

GCGLContext GCGLLayer::platformContext() const
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

    TextureMapperGL::Flags flags = TextureMapperGL::ShouldFlipTexture;
    if (m_context.contextAttributes().alpha)
        flags |= TextureMapperGL::ShouldBlend;

    {
        auto& proxy = downcast<Nicosia::ContentLayerTextureMapperImpl>(m_contentLayer->impl()).proxy();
        Locker locker { proxy.lock() };
        ASSERT(is<TextureMapperPlatformLayerProxyGL>(proxy));
        downcast<TextureMapperPlatformLayerProxyGL>(proxy).pushNextBuffer(makeUnique<TextureMapperPlatformLayerBuffer>(m_context.m_compositorTexture, textureSize, flags, m_context.m_internalColorFormat));
    }

    m_context.markLayerComposited();
#endif
}

} // namespace Nicosia

#endif // ENABLE(WEBGL) && USE(NICOSIA) && USE(TEXTURE_MAPPER)
