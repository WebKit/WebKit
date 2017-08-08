/*
 * Copyright (C) 2011, 2012, 2017 Igalia S.L.
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
#include "TextureMapperGC3DPlatformLayer.h"

#if ENABLE(GRAPHICS_CONTEXT_3D) && USE(TEXTURE_MAPPER)

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#elif USE(OPENGL_ES_2)
#define GL_GLEXT_PROTOTYPES 1
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#include "BitmapTextureGL.h"
#include "GLContext.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxy.h"

namespace WebCore {

TextureMapperGC3DPlatformLayer::TextureMapperGC3DPlatformLayer(GraphicsContext3D& context, GraphicsContext3D::RenderStyle renderStyle)
    : m_context(context)
    , m_renderStyle(renderStyle)
{
    switch (renderStyle) {
    case GraphicsContext3D::RenderOffscreen:
        m_glContext = GLContext::createOffscreenContext(&PlatformDisplay::sharedDisplayForCompositing());
        break;
    case GraphicsContext3D::RenderToCurrentGLContext:
        break;
    case GraphicsContext3D::RenderDirectlyToHostWindow:
        ASSERT_NOT_REACHED();
        break;
    }

#if USE(COORDINATED_GRAPHICS_THREADED)
    if (m_renderStyle == GraphicsContext3D::RenderOffscreen)
        m_platformLayerProxy = adoptRef(new TextureMapperPlatformLayerProxy());
#endif
}

TextureMapperGC3DPlatformLayer::~TextureMapperGC3DPlatformLayer()
{
#if !USE(COORDINATED_GRAPHICS_THREADED)
    if (client())
        client()->platformLayerWillBeDestroyed();
#endif
}

bool TextureMapperGC3DPlatformLayer::makeContextCurrent()
{
    return m_glContext ? m_glContext->makeContextCurrent() : false;
}

PlatformGraphicsContext3D TextureMapperGC3DPlatformLayer::platformContext()
{
    return m_glContext ? m_glContext->platformContext() : GLContext::current()->platformContext();
}

#if USE(COORDINATED_GRAPHICS_THREADED)
RefPtr<TextureMapperPlatformLayerProxy> TextureMapperGC3DPlatformLayer::proxy() const
{
    return m_platformLayerProxy.copyRef();
}

void TextureMapperGC3DPlatformLayer::swapBuffersIfNeeded()
{
    ASSERT(m_renderStyle == GraphicsContext3D::RenderOffscreen);
    if (m_context.layerComposited())
        return;

    m_context.prepareTexture();
    IntSize textureSize(m_context.m_currentWidth, m_context.m_currentHeight);
    TextureMapperGL::Flags flags = TextureMapperGL::ShouldFlipTexture | (m_context.m_attrs.alpha ? TextureMapperGL::ShouldBlend : 0);

    {
        LockHolder holder(m_platformLayerProxy->lock());
        m_platformLayerProxy->pushNextBuffer(std::make_unique<TextureMapperPlatformLayerBuffer>(m_context.m_compositorTexture, textureSize, flags, m_context.m_internalColorFormat));
    }

    m_context.markLayerComposited();
}
#else
void TextureMapperGC3DPlatformLayer::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity)
{
    if (!m_glContext)
        return;

    ASSERT(m_renderStyle == GraphicsContext3D::RenderOffscreen);

    m_context.markLayerComposited();

#if USE(TEXTURE_MAPPER_GL)
    if (m_context.m_attrs.antialias && m_context.m_state.boundFBO == m_context.m_multisampleFBO) {
        GLContext* previousActiveContext = GLContext::current();
        if (previousActiveContext != m_glContext.get())
            m_context.makeContextCurrent();

        m_context.resolveMultisamplingIfNecessary();
        ::glBindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_context.m_state.boundFBO);

        if (previousActiveContext && previousActiveContext != m_glContext.get())
            previousActiveContext->makeContextCurrent();
    }

    TextureMapperGL& texmapGL = static_cast<TextureMapperGL&>(textureMapper);
    TextureMapperGL::Flags flags = TextureMapperGL::ShouldFlipTexture | (m_context.m_attrs.alpha ? TextureMapperGL::ShouldBlend : 0);
    IntSize textureSize(m_context.m_currentWidth, m_context.m_currentHeight);
    texmapGL.drawTexture(m_context.m_texture, flags, textureSize, targetRect, matrix, opacity);
#endif // USE(TEXTURE_MAPPER_GL)
}
#endif // USE(COORDINATED_GRAPHICS_THREADED)

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_3D) && USE(TEXTURE_MAPPER)
