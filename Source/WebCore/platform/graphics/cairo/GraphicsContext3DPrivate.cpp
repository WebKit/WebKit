/*
 * Copyright (C) 2011, 2012 Igalia S.L.
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
#include "GraphicsContext3DPrivate.h"

#if ENABLE(WEBGL)

#include "HostWindow.h"
#include "NotImplemented.h"

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER) && USE(TEXTURE_MAPPER_GL)
#include <texmap/TextureMapperGL.h>
#endif

namespace WebCore {

PassOwnPtr<GraphicsContext3DPrivate> GraphicsContext3DPrivate::create(GraphicsContext3D* context, HostWindow* window)
{
    return adoptPtr(new GraphicsContext3DPrivate(context, window));
}

GraphicsContext3DPrivate::GraphicsContext3DPrivate(GraphicsContext3D* context, HostWindow* window)
    : m_context(context)
    , m_window(window)
#if PLATFORM(GTK)
    , m_glContext(GLContext::createOffscreenContext(GLContext::getContextForWidget(m_window->platformPageClient())))
#else
    , m_glContext(GLContext::createOffscreenContext())
#endif
{
}

GraphicsContext3DPrivate::~GraphicsContext3DPrivate()
{
}

bool GraphicsContext3DPrivate::makeContextCurrent()
{
    return m_glContext ? m_glContext->makeContextCurrent() : false;
}

PlatformGraphicsContext3D GraphicsContext3DPrivate::platformContext()
{
    return m_glContext ? m_glContext->platformContext() : 0;
}

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)
void GraphicsContext3DPrivate::paintToTextureMapper(TextureMapper* textureMapper, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, BitmapTexture* mask)
{
    if (!m_glContext)
        return;

    if (textureMapper->accelerationMode() != TextureMapper::OpenGLMode) {
        notImplemented();
        return;
    }

    if (m_context->m_attrs.antialias && m_context->m_boundFBO == m_context->m_multisampleFBO) {
        GLContext* previousActiveContext = GLContext::getCurrent();

        if (previousActiveContext != m_glContext)
            m_context->makeContextCurrent();

        m_context->resolveMultisamplingIfNecessary();

        if (previousActiveContext && previousActiveContext != m_glContext)
            previousActiveContext->makeContextCurrent();
    }

    TextureMapperGL* texmapGL = static_cast<TextureMapperGL*>(textureMapper);
    TextureMapperGL::Flags flags = TextureMapperGL::ShouldFlipTexture | (m_context->m_attrs.alpha ? TextureMapperGL::SupportsBlending : 0);
    IntSize textureSize(m_context->m_currentWidth, m_context->m_currentHeight);
    texmapGL->drawTexture(m_context->m_texture, flags, textureSize, targetRect, matrix, opacity, mask);
}
#endif // USE(ACCELERATED_COMPOSITING)

} // namespace WebCore

#endif // ENABLE_WEBGL
