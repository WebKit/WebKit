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
#include "NicosiaGC3DLayer.h"

#if USE(NICOSIA) && USE(TEXTURE_MAPPER)

#if USE(COORDINATED_GRAPHICS)
#include "TextureMapperGL.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxy.h"
#endif

#include "GLContext.h"

namespace Nicosia {

using namespace WebCore;

GC3DLayer::GC3DLayer(GraphicsContext3D& context, GraphicsContext3D::RenderStyle renderStyle)
    : m_context(context)
    , m_contentLayer(Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this)))
{
    switch (renderStyle) {
    case GraphicsContext3D::RenderOffscreen:
        m_glContext = GLContext::createOffscreenContext(&PlatformDisplay::sharedDisplayForCompositing());
        break;
    case GraphicsContext3D::RenderDirectlyToHostWindow:
        ASSERT_NOT_REACHED();
        break;
    }
}

GC3DLayer::~GC3DLayer()
{
    downcast<ContentLayerTextureMapperImpl>(m_contentLayer->impl()).invalidateClient();
}

bool GC3DLayer::makeContextCurrent()
{
    ASSERT(m_glContext);
    return m_glContext->makeContextCurrent();
}

PlatformGraphicsContext3D GC3DLayer::platformContext()
{
    ASSERT(m_glContext);
    return m_glContext->platformContext();
}

void GC3DLayer::swapBuffersIfNeeded()
{
#if USE(COORDINATED_GRAPHICS)
    if (m_context.layerComposited())
        return;

    m_context.prepareTexture();
    IntSize textureSize(m_context.m_currentWidth, m_context.m_currentHeight);
    TextureMapperGL::Flags flags = TextureMapperGL::ShouldFlipTexture | (m_context.m_attrs.alpha ? TextureMapperGL::ShouldBlend : 0);

    {
        auto& proxy = downcast<Nicosia::ContentLayerTextureMapperImpl>(m_contentLayer->impl()).proxy();

        LockHolder holder(proxy.lock());
        proxy.pushNextBuffer(std::make_unique<TextureMapperPlatformLayerBuffer>(m_context.m_compositorTexture, textureSize, flags, m_context.m_internalColorFormat));
    }

    m_context.markLayerComposited();
#endif
}

} // namespace Nicosia

#endif // USE(NICOSIA) && USE(TEXTURE_MAPPER)
