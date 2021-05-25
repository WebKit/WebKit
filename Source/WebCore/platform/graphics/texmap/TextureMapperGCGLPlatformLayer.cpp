/*
 * Copyright (C) 2011, 2012, 2017 Igalia S.L.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "TextureMapperGCGLPlatformLayer.h"

#if ENABLE(WEBGL) && USE(TEXTURE_MAPPER) && !USE(NICOSIA)

#include "ANGLEContext.h"
#include "ANGLEHeaders.h"
#include "BitmapTextureGL.h"
#include "GLContext.h"
#include "TextureMapperGLHeaders.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxy.h"

namespace WebCore {

TextureMapperGCGLPlatformLayer::TextureMapperGCGLPlatformLayer(GraphicsContextGLOpenGL& context)
    : m_context(context)
{
    auto sharingContext = PlatformDisplay::sharedDisplayForCompositing().sharingGLContext()->platformContext();
#if ENABLE(WEBGL2)
    m_glContext = ANGLEContext::createContext(sharingContext, context.contextAttributes().webGLVersion == GraphicsContextGLWebGLVersion::WebGL2);
#else
    m_glContext = ANGLEContext::createContext(sharingContext, false);
#endif

#if USE(COORDINATED_GRAPHICS)
    m_platformLayerProxy = adoptRef(new TextureMapperPlatformLayerProxy());
#endif
}

TextureMapperGCGLPlatformLayer::~TextureMapperGCGLPlatformLayer()
{
#if !USE(COORDINATED_GRAPHICS)
    if (client())
        client()->platformLayerWillBeDestroyed();
#endif
}

bool TextureMapperGCGLPlatformLayer::makeContextCurrent()
{
    ASSERT(m_glContext);
    return m_glContext->makeContextCurrent();
}

PlatformGraphicsContextGL TextureMapperGCGLPlatformLayer::platformContext() const
{
    ASSERT(m_glContext);
    return m_glContext->platformContext();
}

#if USE(COORDINATED_GRAPHICS)
RefPtr<TextureMapperPlatformLayerProxy> TextureMapperGCGLPlatformLayer::proxy() const
{
    return m_platformLayerProxy.copyRef();
}

void TextureMapperGCGLPlatformLayer::swapBuffersIfNeeded()
{
    if (m_context.layerComposited())
        return;

    m_context.prepareTexture();
    IntSize textureSize(m_context.m_currentWidth, m_context.m_currentHeight);
    TextureMapperGL::Flags flags = TextureMapperGL::ShouldFlipTexture | (m_context.m_attrs.alpha ? TextureMapperGL::ShouldBlend : 0);

    {
        Locker locker { m_platformLayerProxy->lock() };
        m_platformLayerProxy->pushNextBuffer(makeUnique<TextureMapperPlatformLayerBuffer>(m_context.m_compositorTexture, textureSize, flags, m_context.m_internalColorFormat));
    }

    m_context.markLayerComposited();
}
#else
void TextureMapperGCGLPlatformLayer::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity)
{
    ASSERT(m_glContext);

    m_context.markLayerComposited();

#if USE(TEXTURE_MAPPER_GL)
    auto attrs = m_context.contextAttributes();
    ASSERT(m_context.m_state.boundReadFBO == m_context.m_state.boundDrawFBO);
    if (attrs.antialias && m_context.m_state.boundDrawFBO == m_context.m_multisampleFBO) {
        GLContext* previousActiveContext = GLContext::current();
        m_context.makeContextCurrent();

        m_context.resolveMultisamplingIfNecessary();
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_context.m_state.boundDrawFBO);

        if (previousActiveContext)
            previousActiveContext->makeContextCurrent();
    }

    TextureMapperGL& texmapGL = static_cast<TextureMapperGL&>(textureMapper);
    TextureMapperGL::Flags flags = TextureMapperGL::ShouldFlipTexture | (attrs.alpha ? TextureMapperGL::ShouldBlend : 0);
    IntSize textureSize(m_context.m_currentWidth, m_context.m_currentHeight);
    texmapGL.drawTexture(m_context.m_texture, flags, textureSize, targetRect, matrix, opacity);
#endif // USE(TEXTURE_MAPPER_GL)
}
#endif // USE(COORDINATED_GRAPHICS)

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(TEXTURE_MAPPER)
