/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2014 Collabora Ltd. All rights reserved.
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

#if USE(OPENGL_ES)
#include "ExtensionsGLOpenGLES.h"

#if ENABLE(WEBGL)
#include "GraphicsContextGLOpenGL.h"
#include "NotImplemented.h"

#if USE(LIBEPOXY)
#include "EpoxyEGL.h"
#else
#include <EGL/egl.h>
#endif

namespace WebCore {

ExtensionsGLOpenGLES::ExtensionsGLOpenGLES(GraphicsContextGLOpenGL* context, bool useIndexedGetString)
    : ExtensionsGLOpenGLCommon(context, useIndexedGetString)
    , m_contextResetStatus(GL_NO_ERROR)
    , m_supportsOESvertexArrayObject(false)
    , m_supportsIMGMultisampledRenderToTexture(false)
    , m_supportsANGLEinstancedArrays(false)
    , m_glFramebufferTexture2DMultisampleIMG(0)
    , m_glRenderbufferStorageMultisampleIMG(0)
    , m_glBindVertexArrayOES(0)
    , m_glDeleteVertexArraysOES(0)
    , m_glGenVertexArraysOES(0)
    , m_glIsVertexArrayOES(0)
    , m_glGetGraphicsResetStatusEXT(0)
    , m_glReadnPixelsEXT(0)
    , m_glGetnUniformfvEXT(0)
    , m_glGetnUniformivEXT(0)
    , m_glVertexAttribDivisorANGLE(nullptr)
    , m_glDrawArraysInstancedANGLE(nullptr)
    , m_glDrawElementsInstancedANGLE(nullptr)
{
}

ExtensionsGLOpenGLES::~ExtensionsGLOpenGLES() = default;

bool ExtensionsGLOpenGLES::isEnabled(const String& name)
{
    // Return false immediately if the extension is not supported by the drivers.
    bool enabled = ExtensionsGLOpenGLCommon::isEnabled(name);
    if (!enabled)
        return false;

    // For GL_EXT_robustness, check that the context supports robust access.
    if (name == "GL_EXT_robustness")
        return m_context->getInteger(ExtensionsGL::CONTEXT_ROBUST_ACCESS) == GL_TRUE;

    return true;
}

void ExtensionsGLOpenGLES::framebufferTexture2DMultisampleIMG(unsigned long target, unsigned long attachment, unsigned long textarget, unsigned texture, int level, unsigned long samples)
{
    if (!m_context->makeContextCurrent())
        return;
    if (m_glFramebufferTexture2DMultisampleIMG)
        m_glFramebufferTexture2DMultisampleIMG(target, attachment, textarget, texture, level, samples);
    else
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLES::renderbufferStorageMultisampleIMG(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height)
{
    if (!m_context->makeContextCurrent())
        return;
    if (m_glRenderbufferStorageMultisampleIMG)
        m_glRenderbufferStorageMultisampleIMG(target, samples, internalformat, width, height);
    else
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLES::renderbufferStorageMultisampleANGLE(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!m_context->makeContextCurrent())
        return;
    if (m_glRenderbufferStorageMultisampleIMG)
        renderbufferStorageMultisampleIMG(target, samples, internalformat, width, height);
    else
        notImplemented();
}

PlatformGLObject ExtensionsGLOpenGLES::createVertexArrayOES()
{
    if (m_glGenVertexArraysOES) {
        if (!m_context->makeContextCurrent())
            return 0;
        GLuint array = 0;
        m_glGenVertexArraysOES(1, &array);
        return array;
    }

    m_context->synthesizeGLError(GL_INVALID_OPERATION);
    return 0;
}

void ExtensionsGLOpenGLES::deleteVertexArrayOES(PlatformGLObject array)
{
    if (!array)
        return;
    if (m_glDeleteVertexArraysOES) {
        if (!m_context->makeContextCurrent())
            return;
        m_glDeleteVertexArraysOES(1, &array);
        return;
    }
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

GCGLboolean ExtensionsGLOpenGLES::isVertexArrayOES(PlatformGLObject array)
{
    if (!array)
        return GL_FALSE;
    if (m_glIsVertexArrayOES) {
        if (!m_context->makeContextCurrent())
            return GL_FALSE;
        return m_glIsVertexArrayOES(array);
    }
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
    return false;
}

void ExtensionsGLOpenGLES::bindVertexArrayOES(PlatformGLObject array)
{
    if (m_glBindVertexArrayOES) {
        if (!m_context->makeContextCurrent())
            return;
        m_glBindVertexArrayOES(array);
    } else
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLES::drawBuffersEXT(GCGLSpan<const GCGLenum> /* bufs */)
{
    // FIXME: implement the support.
    notImplemented();
}

int ExtensionsGLOpenGLES::getGraphicsResetStatusARB()
{
    // FIXME: This does not call getGraphicsResetStatusARB, but instead getGraphicsResetStatusEXT.
    // The return codes from the two extensions are identical and their purpose is the same, so it
    // may be best to rename getGraphicsResetStatusARB() to getGraphicsResetStatus().
    if (m_contextResetStatus != GL_NO_ERROR)
        return m_contextResetStatus;
    if (m_glGetGraphicsResetStatusEXT) {
        int reasonForReset = UNKNOWN_CONTEXT_RESET_ARB;
        if (m_context->makeContextCurrent())
            reasonForReset = m_glGetGraphicsResetStatusEXT();
        if (reasonForReset != GL_NO_ERROR)
            m_contextResetStatus = reasonForReset;
        return reasonForReset;
    }

    m_context->synthesizeGLError(GL_INVALID_OPERATION);
    return false;
}

void ExtensionsGLOpenGLES::readnPixelsEXT(int x, int y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, void *data)
{
    if (m_glReadnPixelsEXT) {
        if (!m_context->makeContextCurrent())
            return;

        // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
        // all previous rendering calls should be done before reading pixels.
        ::glFlush();

        // FIXME: If non-BlackBerry platforms use this, they will need to implement
        // their anti-aliasing code here.
        m_glReadnPixelsEXT(x, y, width, height, format, type, bufSize, data);
        return;
    }

    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLES::getnUniformfvEXT(GCGLuint program, int location, GCGLsizei bufSize, float *params)
{
    if (m_glGetnUniformfvEXT) {
        if (!m_context->makeContextCurrent())
            return;

        m_glGetnUniformfvEXT(program, location, bufSize, params);
        return;
    }

    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLES::getnUniformivEXT(GCGLuint program, int location, GCGLsizei bufSize, int *params)
{
    if (m_glGetnUniformivEXT) {
        if (!m_context->makeContextCurrent())
            return;

        m_glGetnUniformivEXT(program, location, bufSize, params);
        return;
    }

    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLES::drawArraysInstancedANGLE(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount)
{
    if (!m_glDrawArraysInstancedANGLE) {
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
        return;
    }

    if (!m_context->makeContextCurrent())
        return;

    m_glDrawArraysInstancedANGLE(mode, first, count, primcount);
}

void ExtensionsGLOpenGLES::drawElementsInstancedANGLE(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLvoidptr offset, GCGLsizei primcount)
{
    if (!m_glDrawElementsInstancedANGLE) {
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
        return;
    }

    if (!m_context->makeContextCurrent())
        return;

    m_glDrawElementsInstancedANGLE(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)), primcount);
}

void ExtensionsGLOpenGLES::vertexAttribDivisorANGLE(GCGLuint index, GCGLuint divisor)
{
    if (!m_glVertexAttribDivisorANGLE) {
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
        return;
    }

    if (!m_context->makeContextCurrent())
        return;

    m_glVertexAttribDivisorANGLE(index, divisor);
}

bool ExtensionsGLOpenGLES::supportsExtension(const String& name)
{
    if (name == "GL_ANGLE_instanced_arrays") {
        auto majorVersion = []() {
            GLint version = 0;
            ::glGetIntegerv(GL_MAJOR_VERSION, &version);
            return version;
        };

        if (m_availableExtensions.contains(name)) {
            m_glVertexAttribDivisorANGLE = reinterpret_cast<PFNGLVERTEXATTRIBDIVISORANGLEPROC>(eglGetProcAddress("glVertexAttribDivisorANGLE"));
            m_glDrawArraysInstancedANGLE = reinterpret_cast<PFNGLDRAWARRAYSINSTANCEDANGLEPROC >(eglGetProcAddress("glDrawArraysInstancedANGLE"));
            m_glDrawElementsInstancedANGLE = reinterpret_cast<PFNGLDRAWELEMENTSINSTANCEDANGLEPROC >(eglGetProcAddress("glDrawElementsInstancedANGLE"));
            m_supportsANGLEinstancedArrays = true;
        } else if (majorVersion() >= 3 || (m_availableExtensions.contains("GL_EXT_instanced_arrays") && m_availableExtensions.contains("GL_EXT_draw_instanced"))) {
            m_glVertexAttribDivisorANGLE = ::glVertexAttribDivisor;
            m_glDrawArraysInstancedANGLE = ::glDrawArraysInstanced;
            m_glDrawElementsInstancedANGLE = ::glDrawElementsInstanced;
            m_supportsANGLEinstancedArrays = true;
        }
        return m_supportsANGLEinstancedArrays;
    }
    
    if (m_availableExtensions.contains(name)) {
        if (!m_supportsOESvertexArrayObject && name == "GL_OES_vertex_array_object") {
            m_glBindVertexArrayOES = reinterpret_cast<PFNGLBINDVERTEXARRAYOESPROC>(eglGetProcAddress("glBindVertexArrayOES"));
            m_glGenVertexArraysOES = reinterpret_cast<PFNGLGENVERTEXARRAYSOESPROC>(eglGetProcAddress("glGenVertexArraysOES"));
            m_glDeleteVertexArraysOES = reinterpret_cast<PFNGLDELETEVERTEXARRAYSOESPROC>(eglGetProcAddress("glDeleteVertexArraysOES"));
            m_glIsVertexArrayOES = reinterpret_cast<PFNGLISVERTEXARRAYOESPROC>(eglGetProcAddress("glIsVertexArrayOES"));
            m_supportsOESvertexArrayObject = true;
        } else if (!m_supportsIMGMultisampledRenderToTexture && name == "GL_IMG_multisampled_render_to_texture") {
            m_glFramebufferTexture2DMultisampleIMG = reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC>(eglGetProcAddress("glFramebufferTexture2DMultisampleIMG"));
            m_glRenderbufferStorageMultisampleIMG = reinterpret_cast<PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC>(eglGetProcAddress("glRenderbufferStorageMultisampleIMG"));
            m_supportsIMGMultisampledRenderToTexture = true;
        } else if (!m_glGetGraphicsResetStatusEXT && name == "GL_EXT_robustness") {
            m_glGetGraphicsResetStatusEXT = reinterpret_cast<PFNGLGETGRAPHICSRESETSTATUSEXTPROC>(eglGetProcAddress("glGetGraphicsResetStatusEXT"));
            m_glReadnPixelsEXT = reinterpret_cast<PFNGLREADNPIXELSEXTPROC>(eglGetProcAddress("glReadnPixelsEXT"));
            m_glGetnUniformfvEXT = reinterpret_cast<PFNGLGETNUNIFORMFVEXTPROC>(eglGetProcAddress("glGetnUniformfvEXT"));
            m_glGetnUniformivEXT = reinterpret_cast<PFNGLGETNUNIFORMIVEXTPROC>(eglGetProcAddress("glGetnUniformivEXT"));
        } else if (name == "GL_EXT_draw_buffers") {
            // FIXME: implement the support.
            return false;
        }
        return true;
    }

    return false;
}

String ExtensionsGLOpenGLES::getExtensions()
{
    return String(reinterpret_cast<const char*>(::glGetString(GL_EXTENSIONS)));
}

} // namespace WebCore

#endif // ENABLE(WEBGL)

#endif // USE(OPENGL_ES)
