/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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

#if ENABLE(GRAPHICS_CONTEXT_GL) && USE(TEXTURE_MAPPER)

#include "GraphicsContextGLOpenGLPrivate.h"
#include "TextureMapperGCGLPlatformLayer.h"
#include <ANGLE/ShaderLang.h>
#include <wtf/Deque.h>
#include <wtf/NeverDestroyed.h>

#if USE(ANGLE)
#define EGL_EGL_PROTOTYPES 0
// Skip the inclusion of ANGLE's explicit context entry points for now.
#define GL_ANGLE_explicit_context
#define GL_ANGLE_explicit_context_gles1
typedef void* GLeglContext;
#include <ANGLE/egl.h>
#include <ANGLE/eglext.h>
#include <ANGLE/eglext_angle.h>
#include <ANGLE/entry_points_egl.h>
#include <ANGLE/entry_points_gles_2_0_autogen.h>
#include <ANGLE/entry_points_gles_ext_autogen.h>
#include <ANGLE/gl2ext.h>
#include <ANGLE/gl2ext_angle.h>
#if defined(Above)
#undef Above
#endif
#if defined(Below)
#undef Below
#endif
#if defined(None)
#undef None
#endif
#elif USE(LIBEPOXY)
#include <epoxy/gl.h>
#elif !USE(OPENGL_ES)
#include "OpenGLShims.h"
#endif

#if USE(ANGLE)
#include "ExtensionsGLANGLE.h"
#elif USE(OPENGL_ES)
#include "ExtensionsGLOpenGLES.h"
#else
#include "ExtensionsGLOpenGL.h"
#endif

#if USE(NICOSIA)
#if USE(ANGLE)
#include "NicosiaGCGLANGLELayer.h"
#else
#include "NicosiaGCGLLayer.h"
#endif
#endif

namespace WebCore {

static const size_t MaxActiveContexts = 16;
static Deque<GraphicsContextGLOpenGL*, MaxActiveContexts>& activeContexts()
{
    static NeverDestroyed<Deque<GraphicsContextGLOpenGL*, MaxActiveContexts>> s_activeContexts;
    return s_activeContexts;
}

RefPtr<GraphicsContextGLOpenGL> GraphicsContextGLOpenGL::create(GraphicsContextGLAttributes attributes, HostWindow* hostWindow, GraphicsContextGLOpenGL::Destination destination)
{
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (destination == Destination::DirectlyToHostWindow)
        return nullptr;

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

    auto& contexts = activeContexts();
    if (contexts.size() >= MaxActiveContexts)
        contexts.first()->recycleContext();

    // Calling recycleContext() above should have lead to the graphics context being
    // destroyed and thus removed from the active contexts list.
    if (contexts.size() >= MaxActiveContexts)
        return nullptr;

    // Create the GraphicsContextGLOpenGL object first in order to establist a current context on this thread.
    auto context = adoptRef(new GraphicsContextGLOpenGL(attributes, hostWindow, destination));

#if USE(LIBEPOXY) && USE(OPENGL_ES)
    // Bail if GLES3 was requested but cannot be provided.
    if (attributes.isWebGL2 && !epoxy_is_desktop_gl() && epoxy_gl_version() < 30)
        return nullptr;
#endif

    contexts.append(context.get());
    return context;
}

#if USE(ANGLE)
GraphicsContextGLOpenGL::GraphicsContextGLOpenGL(GraphicsContextGLAttributes attributes, HostWindow*, GraphicsContextGLOpenGL::Destination destination, GraphicsContextGLOpenGL* sharedContext)
    : GraphicsContextGL(attributes, destination, sharedContext)
{
    ASSERT_UNUSED(sharedContext, !sharedContext);
#if USE(NICOSIA)
    m_nicosiaLayer = WTF::makeUnique<Nicosia::GCGLANGLELayer>(*this, destination);
#else
    m_texmapLayer = WTF::makeUnique<TextureMapperGCGLPlatformLayer>(*this, destination);
#endif
    makeContextCurrent();

    validateAttributes();
    attributes = contextAttributes(); // They may have changed during validation.

    if (destination == Destination::Offscreen) {
        // Create a texture to render into.
        gl::GenTextures(1, &m_texture);
        gl::BindTexture(GL_TEXTURE_RECTANGLE_ANGLE, m_texture);
        gl::TexParameterf(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexParameterf(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::TexParameteri(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl::BindTexture(GL_TEXTURE_RECTANGLE_ANGLE, 0);

        // Create an FBO.
        gl::GenFramebuffers(1, &m_fbo);
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_fbo);

#if USE(COORDINATED_GRAPHICS)
        gl::GenTextures(1, &m_compositorTexture);
        gl::BindTexture(GL_TEXTURE_RECTANGLE_ANGLE, m_compositorTexture);
        gl::TexParameterf(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexParameterf(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::TexParameteri(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        gl::GenTextures(1, &m_intermediateTexture);
        gl::BindTexture(GL_TEXTURE_RECTANGLE_ANGLE, m_intermediateTexture);
        gl::TexParameterf(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexParameterf(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::TexParameteri(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        gl::BindTexture(GL_TEXTURE_RECTANGLE_ANGLE, 0);
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
#if USE(OPENGL_ES)
            if (attributes.depth)
                gl::GenRenderbuffers(1, &m_depthBuffer);
            if (attributes.stencil)
                gl::GenRenderbuffers(1, &m_stencilBuffer);
#endif
            if (attributes.stencil || attributes.depth)
                gl::GenRenderbuffers(1, &m_depthStencilBuffer);
        }
    }

    gl::ClearColor(0, 0, 0, 0);
}
#else
GraphicsContextGLOpenGL::GraphicsContextGLOpenGL(GraphicsContextGLAttributes attributes, HostWindow*, GraphicsContextGLOpenGL::Destination destination, GraphicsContextGLOpenGL* sharedContext)
    : GraphicsContextGL(attributes, destination, sharedContext)
{
    ASSERT_UNUSED(sharedContext, !sharedContext);
#if USE(NICOSIA)
    m_nicosiaLayer = makeUnique<Nicosia::GCGLLayer>(*this, destination);
#else
    m_texmapLayer = makeUnique<TextureMapperGCGLPlatformLayer>(*this, destination);
#endif

    makeContextCurrent();

    validateAttributes();
    attributes = contextAttributes(); // They may have changed during validation.

    if (destination == Destination::Offscreen) {
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
        GCGLint currentVAO = 0;
        getIntegerv(GraphicsContextGLOpenGL::VERTEX_ARRAY_BINDING, &currentVAO);
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
    m_compiler = ANGLEWebKitBridge(SH_ESSL_OUTPUT, attributes.isWebGL2 ? SH_WEBGL2_SPEC : SH_WEBGL_SPEC);
#endif

    // ANGLE initialization.
    ShBuiltInResources ANGLEResources;
    sh::InitBuiltInResources(&ANGLEResources);

    getIntegerv(GraphicsContextGLOpenGL::MAX_VERTEX_ATTRIBS, &ANGLEResources.MaxVertexAttribs);
    getIntegerv(GraphicsContextGLOpenGL::MAX_VERTEX_UNIFORM_VECTORS, &ANGLEResources.MaxVertexUniformVectors);
    getIntegerv(GraphicsContextGLOpenGL::MAX_VARYING_VECTORS, &ANGLEResources.MaxVaryingVectors);
    getIntegerv(GraphicsContextGLOpenGL::MAX_VERTEX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxVertexTextureImageUnits);
    getIntegerv(GraphicsContextGLOpenGL::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxCombinedTextureImageUnits);
    getIntegerv(GraphicsContextGLOpenGL::MAX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxTextureImageUnits);
    getIntegerv(GraphicsContextGLOpenGL::MAX_FRAGMENT_UNIFORM_VECTORS, &ANGLEResources.MaxFragmentUniformVectors);

    // Always set to 1 for OpenGL ES.
    ANGLEResources.MaxDrawBuffers = 1;

    GCGLint range[2], precision;
    getShaderPrecisionFormat(GraphicsContextGLOpenGL::FRAGMENT_SHADER, GraphicsContextGLOpenGL::HIGH_FLOAT, range, &precision);
    ANGLEResources.FragmentPrecisionHigh = (range[0] || range[1] || precision);

    m_compiler.setResources(ANGLEResources);

    ::glClearColor(0, 0, 0, 0);
}
#endif

#if USE(ANGLE)
GraphicsContextGLOpenGL::~GraphicsContextGLOpenGL()
{
    makeContextCurrent();
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
#if USE(OPENGL_ES)
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

    auto* activeContext = activeContexts().takeLast([this](auto* it) {
        return it == this;
    });
    ASSERT_UNUSED(activeContext, !!activeContext);
}
#else
GraphicsContextGLOpenGL::~GraphicsContextGLOpenGL()
{
    makeContextCurrent();
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

    auto* activeContext = activeContexts().takeLast([this](auto* it) {
        return it == this;
    });
    ASSERT_UNUSED(activeContext, !!activeContext);
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

PlatformGraphicsContextGL GraphicsContextGLOpenGL::platformGraphicsContextGL() const
{
#if USE(NICOSIA)
    return m_nicosiaLayer->platformContext();
#else
    return m_texmapLayer->platformContext();
#endif
}

PlatformGLObject GraphicsContextGLOpenGL::platformTexture() const
{
    return m_texture;
}

bool GraphicsContextGLOpenGL::isGLES2Compliant() const
{
#if USE(OPENGL_ES)
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

#if PLATFORM(GTK) && !USE(ANGLE)
ExtensionsGL& GraphicsContextGLOpenGL::getExtensions()
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

#endif // ENABLE(GRAPHICS_CONTEXT_GL) && USE(TEXTURE_MAPPER)
