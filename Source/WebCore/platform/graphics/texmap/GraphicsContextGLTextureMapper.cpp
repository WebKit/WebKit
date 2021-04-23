/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "GraphicsContextGLOpenGL.h"

#if ENABLE(WEBGL) && USE(TEXTURE_MAPPER)

#include "GLContext.h"
#include "GraphicsContextGLOpenGLManager.h"
#include "TextureMapperGCGLPlatformLayer.h"
#include <wtf/Deque.h>
#include <wtf/NeverDestroyed.h>

#if USE(ANGLE)
#include "ANGLEHeaders.h"
#elif USE(LIBEPOXY)
#include <epoxy/gl.h>
#elif !USE(OPENGL_ES)
#include "OpenGLShims.h"
#endif

#if USE(ANGLE)
#include "ExtensionsGLANGLE.h"
#else
#include <ANGLE/ShaderLang.h>
#if USE(OPENGL_ES)
#include "ExtensionsGLOpenGLES.h"
#else
#include "ExtensionsGLOpenGL.h"
#endif
#endif

#if USE(NICOSIA)
#if USE(ANGLE)
#include "NicosiaGCGLANGLELayer.h"
#else
#include "NicosiaGCGLLayer.h"
#endif
#endif

namespace WebCore {

RefPtr<GraphicsContextGLOpenGL> GraphicsContextGLOpenGL::create(GraphicsContextGLAttributes attributes, HostWindow* hostWindow)
{
    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
#if !USE(OPENGL_ES) && !USE(LIBEPOXY) && !USE(ANGLE)
        success = initializeOpenGLShims();
#endif
        initialized = true;
    }
    if (!success)
        return nullptr;

    // Make space for the incoming context if we're full.
    GraphicsContextGLOpenGLManager::sharedManager().recycleContextIfNecessary();
    if (GraphicsContextGLOpenGLManager::sharedManager().hasTooManyContexts())
        return nullptr;

    // Create the GraphicsContextGLOpenGL object first in order to establist a current context on this thread.
    auto context = adoptRef(new GraphicsContextGLOpenGL(attributes, hostWindow));

#if USE(LIBEPOXY) && USE(OPENGL_ES) && ENABLE(WEBGL2)
    // Bail if GLES3 was requested but cannot be provided.
    if (attributes.webGLVersion == GraphicsContextGLWebGLVersion::WebGL2 && !epoxy_is_desktop_gl() && epoxy_gl_version() < 30)
        return nullptr;
#endif

    GraphicsContextGLOpenGLManager::sharedManager().addContext(context.get());

    return context;
}

Ref<GraphicsContextGLOpenGL> GraphicsContextGLOpenGL::createForGPUProcess(const GraphicsContextGLAttributes& attributes)
{
    return adoptRef(*new GraphicsContextGLOpenGL(attributes, nullptr));
}

#if USE(ANGLE)
GraphicsContextGLOpenGL::GraphicsContextGLOpenGL(GraphicsContextGLAttributes attributes, HostWindow*, GraphicsContextGLOpenGL* sharedContext)
    : GraphicsContextGL(attributes, sharedContext)
{
    ASSERT_UNUSED(sharedContext, !sharedContext);
#if ENABLE(WEBGL2)
    m_isForWebGL2 = attributes.webGLVersion == GraphicsContextGLWebGLVersion::WebGL2;
#endif
#if USE(NICOSIA)
    m_nicosiaLayer = WTF::makeUnique<Nicosia::GCGLANGLELayer>(*this);
#else
    m_texmapLayer = WTF::makeUnique<TextureMapperGCGLPlatformLayer>(*this);
#endif
    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);

    validateAttributes();
    attributes = contextAttributes(); // They may have changed during validation.

    GLenum textureTarget = drawingBufferTextureTarget();
    // Create a texture to render into.
    gl::GenTextures(1, &m_texture);
    gl::BindTexture(textureTarget, m_texture);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::BindTexture(textureTarget, 0);

    // Create an FBO.
    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_FRAMEBUFFER, m_fbo);

#if USE(COORDINATED_GRAPHICS)
    gl::GenTextures(1, &m_compositorTexture);
    gl::BindTexture(textureTarget, m_compositorTexture);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl::GenTextures(1, &m_intermediateTexture);
    gl::BindTexture(textureTarget, m_intermediateTexture);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl::BindTexture(textureTarget, 0);
#endif

    // Create a multisample FBO.
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attributes.antialias) {
        gl::GenFramebuffers(1, &m_multisampleFBO);
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_multisampleFBO;
        gl::GenRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            gl::GenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else {
        // Bind canvas FBO.
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_fbo;
        if (attributes.stencil || attributes.depth)
            gl::GenRenderbuffers(1, &m_depthStencilBuffer);
    }

    gl::ClearColor(0, 0, 0, 0);
}
#else
GraphicsContextGLOpenGL::GraphicsContextGLOpenGL(GraphicsContextGLAttributes attributes, HostWindow*, GraphicsContextGLOpenGL* sharedContext)
    : GraphicsContextGL(attributes, sharedContext)
{
    ASSERT_UNUSED(sharedContext, !sharedContext);
#if USE(NICOSIA)
    m_nicosiaLayer = makeUnique<Nicosia::GCGLLayer>(*this);
#else
    m_texmapLayer = makeUnique<TextureMapperGCGLPlatformLayer>(*this);
#endif

    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);

    validateAttributes();
    attributes = contextAttributes(); // They may have changed during validation.

    // Create a texture to render into.
    ::glGenTextures(1, &m_texture);
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    ::glBindTexture(GL_TEXTURE_2D, 0);

    // Create an FBO.
    ::glGenFramebuffers(1, &m_fbo);
    ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

#if USE(COORDINATED_GRAPHICS)
    ::glGenTextures(1, &m_compositorTexture);
    ::glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    ::glGenTextures(1, &m_intermediateTexture);
    ::glBindTexture(GL_TEXTURE_2D, m_intermediateTexture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    ::glBindTexture(GL_TEXTURE_2D, 0);
#endif

    // Create a multisample FBO.
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attributes.antialias) {
        ::glGenFramebuffers(1, &m_multisampleFBO);
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_multisampleFBO;
        ::glGenRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            ::glGenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else {
        // Bind canvas FBO.
        glBindFramebuffer(GraphicsContextGLOpenGL::FRAMEBUFFER, m_fbo);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_fbo;
#if USE(OPENGL_ES)
        if (attributes.depth)
            glGenRenderbuffers(1, &m_depthBuffer);
        if (attributes.stencil)
            glGenRenderbuffers(1, &m_stencilBuffer);
#endif
        if (attributes.stencil || attributes.depth)
            glGenRenderbuffers(1, &m_depthStencilBuffer);
    }

#if !USE(OPENGL_ES)
    ::glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    if (GLContext::current()->version() >= 320) {
        m_usingCoreProfile = true;

        // From version 3.2 on we use the OpenGL Core profile, so request that ouput to the shader compiler.
        // OpenGL version 3.2 uses GLSL version 1.50.
        m_compiler = ANGLEWebKitBridge(SH_GLSL_150_CORE_OUTPUT);

        // From version 3.2 on we use the OpenGL Core profile, and we need a VAO for rendering.
        // A VAO could be created and bound by each component using GL rendering (TextureMapper, WebGL, etc). This is
        // a simpler solution: the first GraphicsContextGLOpenGL created on a GLContext will create and bind a VAO for that context.
        GCGLint currentVAO = getInteger(GraphicsContextGLOpenGL::VERTEX_ARRAY_BINDING);
        if (!currentVAO) {
            m_vao = createVertexArray();
            bindVertexArray(m_vao);
        }
    } else {
        // For lower versions request the compatibility output to the shader compiler.
        m_compiler = ANGLEWebKitBridge(SH_GLSL_COMPATIBILITY_OUTPUT);

        // GL_POINT_SPRITE is needed in lower versions.
        ::glEnable(GL_POINT_SPRITE);
    }
#else
    // Adjust the shader specification depending on whether GLES3 (i.e. WebGL2 support) was requested.
#if ENABLE(WEBGL2)
    m_compiler = ANGLEWebKitBridge(SH_ESSL_OUTPUT, (attributes.webGLVersion == GraphicsContextGLWebGLVersion::WebGL2) ? SH_WEBGL2_SPEC : SH_WEBGL_SPEC);
#else
    m_compiler = ANGLEWebKitBridge(SH_ESSL_OUTPUT, SH_WEBGL_SPEC);
#endif
#endif

    // ANGLE initialization.
    ShBuiltInResources ANGLEResources;
    sh::InitBuiltInResources(&ANGLEResources);

    ANGLEResources.MaxVertexAttribs = getInteger(GraphicsContextGLOpenGL::MAX_VERTEX_ATTRIBS);
    ANGLEResources.MaxVertexUniformVectors = getInteger(GraphicsContextGLOpenGL::MAX_VERTEX_UNIFORM_VECTORS);
    ANGLEResources.MaxVaryingVectors = getInteger(GraphicsContextGLOpenGL::MAX_VARYING_VECTORS);
    ANGLEResources.MaxVertexTextureImageUnits = getInteger(GraphicsContextGLOpenGL::MAX_VERTEX_TEXTURE_IMAGE_UNITS);
    ANGLEResources.MaxCombinedTextureImageUnits = getInteger(GraphicsContextGLOpenGL::MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    ANGLEResources.MaxTextureImageUnits = getInteger(GraphicsContextGLOpenGL::MAX_TEXTURE_IMAGE_UNITS);
    ANGLEResources.MaxFragmentUniformVectors = getInteger(GraphicsContextGLOpenGL::MAX_FRAGMENT_UNIFORM_VECTORS);

    // Always set to 1 for OpenGL ES.
    ANGLEResources.MaxDrawBuffers = 1;

    GCGLint range[2] { };
    GCGLint precision = 0;
    getShaderPrecisionFormat(GraphicsContextGLOpenGL::FRAGMENT_SHADER, GraphicsContextGLOpenGL::HIGH_FLOAT, range, &precision);
    ANGLEResources.FragmentPrecisionHigh = (range[0] || range[1] || precision);

    m_compiler.setResources(ANGLEResources);

    ::glClearColor(0, 0, 0, 0);
}
#endif

#if USE(ANGLE)
GraphicsContextGLOpenGL::~GraphicsContextGLOpenGL()
{
    GraphicsContextGLOpenGLManager::sharedManager().removeContext(this);
    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);
    if (m_texture)
        gl::DeleteTextures(1, &m_texture);
#if USE(COORDINATED_GRAPHICS)
    if (m_compositorTexture)
        gl::DeleteTextures(1, &m_compositorTexture);
#endif

    auto attributes = contextAttributes();

    if (attributes.antialias) {
        gl::DeleteRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            gl::DeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        gl::DeleteFramebuffers(1, &m_multisampleFBO);
    } else if (attributes.stencil || attributes.depth) {
#if !USE(ANGLE) && USE(OPENGL_ES)
        if (m_depthBuffer)
            glDeleteRenderbuffers(1, &m_depthBuffer);

        if (m_stencilBuffer)
            glDeleteRenderbuffers(1, &m_stencilBuffer);
#endif
        if (m_depthStencilBuffer)
            gl::DeleteRenderbuffers(1, &m_depthStencilBuffer);
    }
    gl::DeleteFramebuffers(1, &m_fbo);
#if USE(COORDINATED_GRAPHICS)
    gl::DeleteTextures(1, &m_intermediateTexture);
#endif

#if USE(CAIRO)
    if (m_vao)
        deleteVertexArray(m_vao);
#endif
}
#else
GraphicsContextGLOpenGL::~GraphicsContextGLOpenGL()
{
    GraphicsContextGLOpenGLManager::sharedManager().removeContext(this);
    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);
    if (m_texture)
        ::glDeleteTextures(1, &m_texture);
#if USE(COORDINATED_GRAPHICS)
    if (m_compositorTexture)
        ::glDeleteTextures(1, &m_compositorTexture);
#endif

    auto attributes = contextAttributes();

    if (attributes.antialias) {
        ::glDeleteRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            ::glDeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        ::glDeleteFramebuffers(1, &m_multisampleFBO);
    } else if (attributes.stencil || attributes.depth) {
#if USE(OPENGL_ES)
        if (m_depthBuffer)
            glDeleteRenderbuffers(1, &m_depthBuffer);

        if (m_stencilBuffer)
            glDeleteRenderbuffers(1, &m_stencilBuffer);
#endif
        if (m_depthStencilBuffer)
            ::glDeleteRenderbuffers(1, &m_depthStencilBuffer);
    }
    ::glDeleteFramebuffers(1, &m_fbo);
#if USE(COORDINATED_GRAPHICS)
    ::glDeleteTextures(1, &m_intermediateTexture);
#endif

#if USE(CAIRO)
    if (m_vao)
        deleteVertexArray(m_vao);
#endif
}
#endif // USE(ANGLE)

bool GraphicsContextGLOpenGL::makeContextCurrent()
{
#if USE(NICOSIA)
    return m_nicosiaLayer->makeContextCurrent();
#else
    return m_texmapLayer->makeContextCurrent();
#endif
}

void GraphicsContextGLOpenGL::checkGPUStatus()
{
}

bool GraphicsContextGLOpenGL::isGLES2Compliant() const
{
#if USE(ANGLE)
    return m_isForWebGL2;
#elif USE(OPENGL_ES)
    return true;
#else
    return false;
#endif
}

PlatformLayer* GraphicsContextGLOpenGL::platformLayer() const
{
#if USE(NICOSIA)
    return &m_nicosiaLayer->contentLayer();
#else
    return m_texmapLayer.get();
#endif
}

#if USE(ANGLE)
GCGLenum GraphicsContextGLOpenGL::drawingBufferTextureTarget()
{
#if PLATFORM(WIN)
    return GL_TEXTURE_2D;
#else
    return GL_TEXTURE_RECTANGLE_ANGLE;
#endif
}
#endif

#if PLATFORM(GTK) && !USE(ANGLE)
ExtensionsGLOpenGLCommon& GraphicsContextGLOpenGL::getExtensions()
{
    if (!m_extensions) {
#if USE(OPENGL_ES)
        // glGetStringi is not available on GLES2.
        m_extensions = makeUnique<ExtensionsGLOpenGLES>(this,  false);
#else
        // From OpenGL 3.2 on we use the Core profile, and there we must use glGetStringi.
        m_extensions = makeUnique<ExtensionsGLOpenGL>(this, GLContext::current()->version() >= 320);
#endif
    }
    return *m_extensions;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(TEXTURE_MAPPER)
