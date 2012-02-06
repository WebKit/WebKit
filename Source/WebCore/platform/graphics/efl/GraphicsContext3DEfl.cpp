/*
 * Copyright (C) 2011, 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GraphicsContext3D.h"

#if ENABLE(WEBGL)
#include "Extensions3DOpenGL.h"
#include "GraphicsContext3DPrivate.h"
#include "OpenGLShims.h"
#include "ShaderLang.h"

namespace WebCore {

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attributes, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    bool renderDirectlyToHostWindow = (renderStyle == RenderDirectlyToHostWindow);
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (renderDirectlyToHostWindow)
        return 0;

    OwnPtr<GraphicsContext3DPrivate> priv = GraphicsContext3DPrivate::create();
    if (!priv)
        return 0;

    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(attributes, hostWindow, renderDirectlyToHostWindow));
    context->m_private = priv.release();
    return context.release();
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes attributes, HostWindow* hostWindow, bool renderDirectlyToHostWindow)
    : m_currentWidth(0)
    , m_currentHeight(0)
    , m_attrs(attributes)
    , m_texture(0)
    , m_fbo(0)
    , m_depthStencilBuffer(0)
    , m_boundFBO(0)
    , m_multisampleFBO(0)
    , m_multisampleDepthStencilBuffer(0)
    , m_multisampleColorBuffer(0)
{
    GraphicsContext3DPrivate::addActiveGraphicsContext(this);

    validateAttributes();

    // Create a texture to render into.
    ::glGenTextures(1, &m_texture);
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    ::glBindTexture(GL_TEXTURE_2D, 0);

    // Create an FBO.
    ::glGenFramebuffersEXT(1, &m_fbo);
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

    m_boundFBO = m_fbo;
    if (!m_attrs.antialias && (m_attrs.stencil || m_attrs.depth))
        ::glGenRenderbuffersEXT(1, &m_depthStencilBuffer);

    // Create a multisample FBO.
    if (m_attrs.antialias) {
        ::glGenFramebuffersEXT(1, &m_multisampleFBO);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        m_boundFBO = m_multisampleFBO;
        ::glGenRenderbuffersEXT(1, &m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth)
            ::glGenRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
    }

    // ANGLE initialization.
    ShBuiltInResources ANGLEResources;
    ShInitBuiltInResources(&ANGLEResources);

    getIntegerv(GraphicsContext3D::MAX_VERTEX_ATTRIBS, &ANGLEResources.MaxVertexAttribs);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_UNIFORM_VECTORS, &ANGLEResources.MaxVertexUniformVectors);
    getIntegerv(GraphicsContext3D::MAX_VARYING_VECTORS, &ANGLEResources.MaxVaryingVectors);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxVertexTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxCombinedTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_FRAGMENT_UNIFORM_VECTORS, &ANGLEResources.MaxFragmentUniformVectors);

    // Always set to 1 for OpenGL ES.
    ANGLEResources.MaxDrawBuffers = 1;
    m_compiler.setResources(ANGLEResources);

    ::glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    ::glEnable(GL_POINT_SPRITE);
    ::glClearColor(0, 0, 0, 0);
}

GraphicsContext3D::~GraphicsContext3D()
{
    GraphicsContext3DPrivate::removeActiveGraphicsContext(this);
    if (!m_private->m_context)
        return;

    makeContextCurrent();
    ::glDeleteTextures(1, &m_texture);
    if (m_attrs.antialias) {
        ::glDeleteRenderbuffersEXT(1, &m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth)
            ::glDeleteRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
        ::glDeleteFramebuffersEXT(1, &m_multisampleFBO);
    } else {
        if (m_attrs.stencil || m_attrs.depth)
            ::glDeleteRenderbuffersEXT(1, &m_depthStencilBuffer);
    }
    ::glDeleteFramebuffersEXT(1, &m_fbo);
}

bool GraphicsContext3D::makeContextCurrent()
{
    if (!m_private)
        return false;
    return m_private->makeContextCurrent();
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D() const
{
    return m_private->m_context;
}

bool GraphicsContext3D::isGLES2Compliant() const
{
    return false;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
