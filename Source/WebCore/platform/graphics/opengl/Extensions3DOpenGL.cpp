/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(GRAPHICS_CONTEXT_3D)

#include "Extensions3DOpenGL.h"

#include "GraphicsContext3D.h"

#if PLATFORM(GTK) || PLATFORM(WIN)
#include "OpenGLShims.h"
#elif USE(OPENGL_ES)
#include <OpenGLES/ES2/glext.h>
#elif USE(OPENGL)
#include <OpenGL/gl.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include "GraphicsContext3DIOS.h"
#endif

namespace WebCore {

Extensions3DOpenGL::Extensions3DOpenGL(GraphicsContext3D* context, bool useIndexedGetString)
    : Extensions3DOpenGLCommon(context, useIndexedGetString)
{
}

Extensions3DOpenGL::~Extensions3DOpenGL() = default;


void Extensions3DOpenGL::blitFramebuffer(long srcX0, long srcY0, long srcX1, long srcY1, long dstX0, long dstY0, long dstX1, long dstY1, unsigned long mask, unsigned long filter)
{
#if PLATFORM(COCOA) && USE(OPENGL_ES)
    UNUSED_PARAM(srcX0);
    UNUSED_PARAM(srcY0);
    UNUSED_PARAM(srcX1);
    UNUSED_PARAM(srcY1);
    UNUSED_PARAM(dstX0);
    UNUSED_PARAM(dstY0);
    UNUSED_PARAM(dstX1);
    UNUSED_PARAM(dstY1);
    UNUSED_PARAM(mask);
    UNUSED_PARAM(filter);
    ::glResolveMultisampleFramebufferAPPLE();
#else
    ::glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
#endif
}

void Extensions3DOpenGL::renderbufferStorageMultisample(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height)
{
    ::glRenderbufferStorageMultisampleEXT(target, samples, internalformat, width, height);
}

Platform3DObject Extensions3DOpenGL::createVertexArrayOES()
{
    m_context->makeContextCurrent();
    GLuint array = 0;
#if PLATFORM(GTK) || PLATFORM(WIN) || (PLATFORM(COCOA) && USE(OPENGL_ES))
    if (isVertexArrayObjectSupported())
        glGenVertexArrays(1, &array);
#elif defined(GL_APPLE_vertex_array_object) && GL_APPLE_vertex_array_object
    glGenVertexArraysAPPLE(1, &array);
#endif
    return array;
}

void Extensions3DOpenGL::deleteVertexArrayOES(Platform3DObject array)
{
    if (!array)
        return;

    m_context->makeContextCurrent();
#if PLATFORM(GTK) || PLATFORM(WIN) || (PLATFORM(COCOA) && USE(OPENGL_ES))
    if (isVertexArrayObjectSupported())
        glDeleteVertexArrays(1, &array);
#elif defined(GL_APPLE_vertex_array_object) && GL_APPLE_vertex_array_object
    glDeleteVertexArraysAPPLE(1, &array);
#endif
}

GC3Dboolean Extensions3DOpenGL::isVertexArrayOES(Platform3DObject array)
{
    if (!array)
        return GL_FALSE;

    m_context->makeContextCurrent();
#if PLATFORM(GTK) || PLATFORM(WIN) || (PLATFORM(COCOA) && USE(OPENGL_ES))
    if (isVertexArrayObjectSupported())
        return glIsVertexArray(array);
#elif defined(GL_APPLE_vertex_array_object) && GL_APPLE_vertex_array_object
    return glIsVertexArrayAPPLE(array);
#endif
    return GL_FALSE;
}

void Extensions3DOpenGL::bindVertexArrayOES(Platform3DObject array)
{
    m_context->makeContextCurrent();
#if PLATFORM(GTK) || PLATFORM(WIN) || (PLATFORM(COCOA) && USE(OPENGL_ES))
    if (isVertexArrayObjectSupported())
        glBindVertexArray(array);
#elif defined(GL_APPLE_vertex_array_object) && GL_APPLE_vertex_array_object
    glBindVertexArrayAPPLE(array);
#else
    UNUSED_PARAM(array);
#endif
}

void Extensions3DOpenGL::insertEventMarkerEXT(const String&)
{
    // FIXME: implement this function and add GL_EXT_debug_marker in supports().
    return;
}

void Extensions3DOpenGL::pushGroupMarkerEXT(const String&)
{
    // FIXME: implement this function and add GL_EXT_debug_marker in supports().
    return;
}

void Extensions3DOpenGL::popGroupMarkerEXT(void)
{
    // FIXME: implement this function and add GL_EXT_debug_marker in supports().
    return;
}

bool Extensions3DOpenGL::supportsExtension(const String& name)
{
    // GL_ANGLE_framebuffer_blit and GL_ANGLE_framebuffer_multisample are "fake". They are implemented using other
    // extensions. In particular GL_EXT_framebuffer_blit and GL_EXT_framebuffer_multisample/GL_APPLE_framebuffer_multisample.
    if (name == "GL_ANGLE_framebuffer_blit")
        return m_availableExtensions.contains("GL_EXT_framebuffer_blit");
    if (name == "GL_ANGLE_framebuffer_multisample")
#if PLATFORM(IOS_FAMILY)
        return m_availableExtensions.contains("GL_APPLE_framebuffer_multisample");
#else
        return m_availableExtensions.contains("GL_EXT_framebuffer_multisample");
#endif

    if (name == "GL_ANGLE_instanced_arrays") {
        return (m_availableExtensions.contains("GL_ARB_instanced_arrays") || m_availableExtensions.contains("GL_EXT_instanced_arrays"))
            && (m_availableExtensions.contains("GL_ARB_draw_instanced") || m_availableExtensions.contains("GL_EXT_draw_instanced"));
    }

    if (name == "GL_EXT_sRGB")
#if PLATFORM(IOS_FAMILY)
        return m_availableExtensions.contains("GL_EXT_sRGB");
#else
        return m_availableExtensions.contains("GL_EXT_texture_sRGB") && (m_availableExtensions.contains("GL_EXT_framebuffer_sRGB") || m_availableExtensions.contains("GL_ARB_framebuffer_sRGB"));
#endif

    if (name == "GL_EXT_frag_depth")
#if PLATFORM(MAC)
        return true;
#else
        return m_availableExtensions.contains("GL_EXT_frag_depth");
#endif

    // Desktop GL always supports GL_OES_rgb8_rgba8.
    if (name == "GL_OES_rgb8_rgba8")
        return true;

    // If GL_ARB_texture_float or GL_OES_texture_float is available then we report
    // GL_OES_texture_half_float, GL_OES_texture_float_linear and GL_OES_texture_half_float_linear as available.
    if (name == "GL_OES_texture_float" || name == "GL_OES_texture_half_float" || name == "GL_OES_texture_float_linear" || name == "GL_OES_texture_half_float_linear")
        return m_availableExtensions.contains("GL_ARB_texture_float") || m_availableExtensions.contains("GL_OES_texture_float");

    // GL_OES_vertex_array_object
    if (name == "GL_OES_vertex_array_object") {
#if (PLATFORM(GTK))
        return m_availableExtensions.contains("GL_ARB_vertex_array_object");
#elif PLATFORM(IOS_FAMILY)
        return m_availableExtensions.contains("GL_OES_vertex_array_object");
#else
        return m_availableExtensions.contains("GL_APPLE_vertex_array_object");
#endif
    }

    // Desktop GL always supports the standard derivative functions
    if (name == "GL_OES_standard_derivatives")
        return true;

    // Desktop GL always supports UNSIGNED_INT indices
    if (name == "GL_OES_element_index_uint")
        return true;
    
    if (name == "GL_EXT_shader_texture_lod")
        return m_availableExtensions.contains("GL_EXT_shader_texture_lod");
    
    if (name == "GL_EXT_texture_filter_anisotropic")
        return m_availableExtensions.contains("GL_EXT_texture_filter_anisotropic");

    if (name == "GL_EXT_draw_buffers") {
#if PLATFORM(IOS_FAMILY)
        return m_availableExtensions.contains(name);
#elif PLATFORM(MAC) || PLATFORM(GTK)
        return m_availableExtensions.contains("GL_ARB_draw_buffers");
#else
        // FIXME: implement support for other platforms.
        return false;
#endif
    }

#if PLATFORM(IOS_FAMILY)
    if (name == "GL_EXT_packed_depth_stencil")
        return m_availableExtensions.contains("GL_OES_packed_depth_stencil");
#endif

    return m_availableExtensions.contains(name);
}

void Extensions3DOpenGL::drawBuffersEXT(GC3Dsizei n, const GC3Denum* bufs)
{
    //  FIXME: implement support for other platforms.
#if PLATFORM(MAC)
    ::glDrawBuffersARB(n, bufs);
#elif PLATFORM(GTK)
    ::glDrawBuffers(n, bufs);
#else
    UNUSED_PARAM(n);
    UNUSED_PARAM(bufs);
#endif
}

void Extensions3DOpenGL::drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount)
{
    m_context->makeContextCurrent();
#if PLATFORM(GTK)
    ::glDrawArraysInstanced(mode, first, count, primcount);
#elif PLATFORM(COCOA)
    ::glDrawArraysInstancedARB(mode, first, count, primcount);
#else
    UNUSED_PARAM(mode);
    UNUSED_PARAM(first);
    UNUSED_PARAM(count);
    UNUSED_PARAM(primcount);
#endif
}

void Extensions3DOpenGL::drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset, GC3Dsizei primcount)
{
    m_context->makeContextCurrent();
#if PLATFORM(GTK)
    ::glDrawElementsInstanced(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)), primcount);
#elif PLATFORM(COCOA)
    ::glDrawElementsInstancedARB(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)), primcount);
#else
    UNUSED_PARAM(mode);
    UNUSED_PARAM(count);
    UNUSED_PARAM(type);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(primcount);
#endif
}

void Extensions3DOpenGL::vertexAttribDivisor(GC3Duint index, GC3Duint divisor)
{
    m_context->makeContextCurrent();
#if PLATFORM(GTK)
    ::glVertexAttribDivisor(index, divisor);
#elif PLATFORM(COCOA)
    ::glVertexAttribDivisorARB(index, divisor);
#else
    UNUSED_PARAM(index);
    UNUSED_PARAM(divisor);
#endif
}

String Extensions3DOpenGL::getExtensions()
{
    ASSERT(!m_useIndexedGetString);
    return String(reinterpret_cast<const char*>(::glGetString(GL_EXTENSIONS)));
}

#if PLATFORM(GTK) || PLATFORM(WIN) || (PLATFORM(COCOA) && USE(OPENGL_ES))
bool Extensions3DOpenGL::isVertexArrayObjectSupported()
{
    static const bool supportsVertexArrayObject = supports("GL_OES_vertex_array_object");
    return supportsVertexArrayObject;
}
#endif

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_3D)
