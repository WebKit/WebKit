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
#include "Extensions3DOpenGLES.h"

#if ENABLE(GRAPHICS_CONTEXT_3D)
#include "GraphicsContext3D.h"
#include "NotImplemented.h"

#if USE(LIBEPOXY)
#include "EpoxyEGL.h"
#else
#include <EGL/egl.h>
#endif

namespace WebCore {

Extensions3DOpenGLES::Extensions3DOpenGLES(GraphicsContext3D* context, bool useIndexedGetString)
    : Extensions3DOpenGLCommon(context, useIndexedGetString)
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

Extensions3DOpenGLES::~Extensions3DOpenGLES() = default;

bool Extensions3DOpenGLES::isEnabled(const String& name)
{
    // Return false immediately if the extension is not supported by the drivers.
    bool enabled = Extensions3DOpenGLCommon::isEnabled(name);
    if (!enabled)
        return false;

    // For GL_EXT_robustness, check that the context supports robust access.
    if (name == "GL_EXT_robustness") {
        GLint robustAccess = GL_FALSE;
        m_context->getIntegerv(Extensions3D::CONTEXT_ROBUST_ACCESS, &robustAccess);
        return robustAccess == GL_TRUE;
    }

    return true;
}

void Extensions3DOpenGLES::framebufferTexture2DMultisampleIMG(unsigned long target, unsigned long attachment, unsigned long textarget, unsigned int texture, int level, unsigned long samples)
{
    if (m_glFramebufferTexture2DMultisampleIMG)
        m_glFramebufferTexture2DMultisampleIMG(target, attachment, textarget, texture, level, samples);
    else
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void Extensions3DOpenGLES::renderbufferStorageMultisampleIMG(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height)
{
    if (m_glRenderbufferStorageMultisampleIMG)
        m_glRenderbufferStorageMultisampleIMG(target, samples, internalformat, width, height);
    else
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void Extensions3DOpenGLES::blitFramebuffer(long /* srcX0 */, long /* srcY0 */, long /* srcX1 */, long /* srcY1 */, long /* dstX0 */, long /* dstY0 */, long /* dstX1 */, long /* dstY1 */, unsigned long /* mask */, unsigned long /* filter */)
{
    notImplemented();
}

void Extensions3DOpenGLES::renderbufferStorageMultisample(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height)
{
    if (m_glRenderbufferStorageMultisampleIMG)
        renderbufferStorageMultisampleIMG(target, samples, internalformat, width, height);
    else
        notImplemented();
}

void Extensions3DOpenGLES::insertEventMarkerEXT(const String&)
{
    notImplemented();
}

void Extensions3DOpenGLES::pushGroupMarkerEXT(const String&)
{
    notImplemented();
}

void Extensions3DOpenGLES::popGroupMarkerEXT(void)
{
    notImplemented();
}

Platform3DObject Extensions3DOpenGLES::createVertexArrayOES()
{
    m_context->makeContextCurrent();
    if (m_glGenVertexArraysOES) {
        GLuint array = 0;
        m_glGenVertexArraysOES(1, &array);
        return array;
    }

    m_context->synthesizeGLError(GL_INVALID_OPERATION);
    return 0;
}

void Extensions3DOpenGLES::deleteVertexArrayOES(Platform3DObject array)
{
    if (!array)
        return;

    m_context->makeContextCurrent();
    if (m_glDeleteVertexArraysOES)
        m_glDeleteVertexArraysOES(1, &array);
    else
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

GC3Dboolean Extensions3DOpenGLES::isVertexArrayOES(Platform3DObject array)
{
    if (!array)
        return GL_FALSE;

    m_context->makeContextCurrent();
    if (m_glIsVertexArrayOES)
        return m_glIsVertexArrayOES(array);

    m_context->synthesizeGLError(GL_INVALID_OPERATION);
    return false;
}

void Extensions3DOpenGLES::bindVertexArrayOES(Platform3DObject array)
{
    if (!array)
        return;

    m_context->makeContextCurrent();
    if (m_glBindVertexArrayOES)
        m_glBindVertexArrayOES(array);
    else
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void Extensions3DOpenGLES::drawBuffersEXT(GC3Dsizei /* n */, const GC3Denum* /* bufs */)
{
    // FIXME: implement the support.
    notImplemented();
}

int Extensions3DOpenGLES::getGraphicsResetStatusARB()
{
    // FIXME: This does not call getGraphicsResetStatusARB, but instead getGraphicsResetStatusEXT.
    // The return codes from the two extensions are identical and their purpose is the same, so it
    // may be best to rename getGraphicsResetStatusARB() to getGraphicsResetStatus().
    if (m_contextResetStatus != GL_NO_ERROR)
        return m_contextResetStatus;
    if (m_glGetGraphicsResetStatusEXT) {
        m_context->makeContextCurrent();
        int reasonForReset = m_glGetGraphicsResetStatusEXT();
        if (reasonForReset != GL_NO_ERROR) {
            ASSERT(m_contextLostCallback);
            if (m_contextLostCallback)
                m_contextLostCallback->onContextLost();
            m_contextResetStatus = reasonForReset;
        }
        return reasonForReset;
    }

    m_context->synthesizeGLError(GL_INVALID_OPERATION);
    return false;
}

void Extensions3DOpenGLES::readnPixelsEXT(int x, int y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, GC3Dsizei bufSize, void *data)
{
    if (m_glReadnPixelsEXT) {
        m_context->makeContextCurrent();
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

void Extensions3DOpenGLES::getnUniformfvEXT(GC3Duint program, int location, GC3Dsizei bufSize, float *params)
{
    if (m_glGetnUniformfvEXT) {
        m_context->makeContextCurrent();
        m_glGetnUniformfvEXT(program, location, bufSize, params);
        return;
    }

    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void Extensions3DOpenGLES::getnUniformivEXT(GC3Duint program, int location, GC3Dsizei bufSize, int *params)
{
    if (m_glGetnUniformivEXT) {
        m_context->makeContextCurrent();
        m_glGetnUniformivEXT(program, location, bufSize, params);
        return;
    }

    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void Extensions3DOpenGLES::drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount)
{
    if (!m_glDrawArraysInstancedANGLE) {
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
        return;
    }

    m_context->makeContextCurrent();
    m_glDrawArraysInstancedANGLE(mode, first, count, primcount);
}

void Extensions3DOpenGLES::drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset, GC3Dsizei primcount)
{
    if (!m_glDrawElementsInstancedANGLE) {
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
        return;
    }

    m_context->makeContextCurrent();
    m_glDrawElementsInstancedANGLE(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)), primcount);
}

void Extensions3DOpenGLES::vertexAttribDivisor(GC3Duint index, GC3Duint divisor)
{
    if (!m_glVertexAttribDivisorANGLE) {
        m_context->synthesizeGLError(GL_INVALID_OPERATION);
        return;
    }

    m_context->makeContextCurrent();
    m_glVertexAttribDivisorANGLE(index, divisor);
}

bool Extensions3DOpenGLES::supportsExtension(const String& name)
{
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
        } else if (!m_supportsANGLEinstancedArrays && name == "GL_ANGLE_instanced_arrays") {
            m_glVertexAttribDivisorANGLE = reinterpret_cast<PFNGLVERTEXATTRIBDIVISORANGLEPROC>(eglGetProcAddress("glVertexAttribDivisorANGLE"));
            m_glDrawArraysInstancedANGLE = reinterpret_cast<PFNGLDRAWARRAYSINSTANCEDANGLEPROC >(eglGetProcAddress("glDrawArraysInstancedANGLE"));
            m_glDrawElementsInstancedANGLE = reinterpret_cast<PFNGLDRAWELEMENTSINSTANCEDANGLEPROC >(eglGetProcAddress("glDrawElementsInstancedANGLE"));
            m_supportsANGLEinstancedArrays = true;
        } else if (name == "GL_EXT_draw_buffers") {
            // FIXME: implement the support.
            return false;
        }
        return true;
    }

    return false;
}

String Extensions3DOpenGLES::getExtensions()
{
    return String(reinterpret_cast<const char*>(::glGetString(GL_EXTENSIONS)));
}

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_3D)

#endif // USE(OPENGL_ES)
