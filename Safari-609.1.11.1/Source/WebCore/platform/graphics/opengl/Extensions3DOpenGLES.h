/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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

#pragma once

#include "Extensions3DOpenGLCommon.h"

#if USE(OPENGL_ES)

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#ifndef GL_EXT_robustness
/* reuse GL_NO_ERROR */
#define GL_GUILTY_CONTEXT_RESET_EXT 0x8253
#define GL_INNOCENT_CONTEXT_RESET_EXT 0x8254
#define GL_UNKNOWN_CONTEXT_RESET_EXT 0x8255
#define GL_CONTEXT_ROBUST_ACCESS_EXT 0x90F3
#define GL_RESET_NOTIFICATION_STRATEGY_EXT 0x8256
#define GL_LOSE_CONTEXT_ON_RESET_EXT 0x8252
#define GL_NO_RESET_NOTIFICATION_EXT 0x8261
#endif

#ifndef GL_EXT_robustness
#define GL_EXT_robustness 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL GC3Denum GL_APIENTRY glGetGraphicsResetStatusEXT(void);
GL_APICALL void GL_APIENTRY glReadnPixelsEXT(GLint x, GLint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, GC3Dsizei bufSize, void *data);
GL_APICALL void GL_APIENTRY glGetnUniformfvEXT(GLuint program, GLint location, GC3Dsizei bufSize, float *params);
GL_APICALL void GL_APIENTRY glGetnUniformivEXT(GLuint program, GLint location, GC3Dsizei bufSize, GLint *params);
#endif
typedef GC3Denum (GL_APIENTRYP PFNGLGETGRAPHICSRESETSTATUSEXTPROC) (void);
typedef void (GL_APIENTRYP PFNGLREADNPIXELSEXTPROC) (GLint x, GLint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, GC3Dsizei bufSize, void *data);
typedef void (GL_APIENTRYP PFNGLGETNUNIFORMFVEXTPROC) (GLuint program, GLint location, GC3Dsizei bufSize, float *params);
typedef void (GL_APIENTRYP PFNGLGETNUNIFORMIVEXTPROC) (GLuint program, GLint location, GC3Dsizei bufSize, GLint *params);
#endif

namespace WebCore {

class Extensions3DOpenGLES : public Extensions3DOpenGLCommon {
public:
    // This class only needs to be instantiated by GraphicsContext3D implementations.
    Extensions3DOpenGLES(GraphicsContext3D*, bool useIndexedGetString);
    virtual ~Extensions3DOpenGLES();

    virtual void framebufferTexture2DMultisampleIMG(unsigned long target, unsigned long attachment, unsigned long textarget, unsigned int texture, int level, unsigned long samples);
    virtual void renderbufferStorageMultisampleIMG(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height);

    // Extension3D methods
    bool isEnabled(const String&) override;
    void blitFramebuffer(long srcX0, long srcY0, long srcX1, long srcY1, long dstX0, long dstY0, long dstX1, long dstY1, unsigned long mask, unsigned long filter) override;
    void renderbufferStorageMultisample(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height) override;
    void insertEventMarkerEXT(const String&) override;
    void pushGroupMarkerEXT(const String&) override;
    void popGroupMarkerEXT(void) override;

    Platform3DObject createVertexArrayOES() override;
    void deleteVertexArrayOES(Platform3DObject) override;
    GC3Dboolean isVertexArrayOES(Platform3DObject) override;
    void bindVertexArrayOES(Platform3DObject) override;
    void drawBuffersEXT(GC3Dsizei, const GC3Denum*) override;

    void drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount) override;
    void drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset, GC3Dsizei primcount) override;
    void vertexAttribDivisor(GC3Duint index, GC3Duint divisor) override;

    // EXT Robustness - reset
    int getGraphicsResetStatusARB() override;

    // EXT Robustness - etc
    void readnPixelsEXT(int x, int y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, GC3Dsizei bufSize, void *data) override;
    void getnUniformfvEXT(GC3Duint program, int location, GC3Dsizei bufSize, float *params) override;
    void getnUniformivEXT(GC3Duint program, int location, GC3Dsizei bufSize, int *params) override;

protected:
    bool supportsExtension(const String&) override;
    String getExtensions() override;

    GC3Denum m_contextResetStatus;

    bool m_supportsOESvertexArrayObject;
    bool m_supportsIMGMultisampledRenderToTexture;
    bool m_supportsANGLEinstancedArrays;

    PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC m_glFramebufferTexture2DMultisampleIMG;
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC m_glRenderbufferStorageMultisampleIMG;
    PFNGLBINDVERTEXARRAYOESPROC m_glBindVertexArrayOES;
    PFNGLDELETEVERTEXARRAYSOESPROC m_glDeleteVertexArraysOES;
    PFNGLGENVERTEXARRAYSOESPROC m_glGenVertexArraysOES;
    PFNGLISVERTEXARRAYOESPROC m_glIsVertexArrayOES;
    PFNGLGETGRAPHICSRESETSTATUSEXTPROC m_glGetGraphicsResetStatusEXT;
    PFNGLREADNPIXELSEXTPROC m_glReadnPixelsEXT;
    PFNGLGETNUNIFORMFVEXTPROC m_glGetnUniformfvEXT;
    PFNGLGETNUNIFORMIVEXTPROC m_glGetnUniformivEXT;
    PFNGLVERTEXATTRIBDIVISORANGLEPROC m_glVertexAttribDivisorANGLE;
    PFNGLDRAWARRAYSINSTANCEDANGLEPROC m_glDrawArraysInstancedANGLE;
    PFNGLDRAWELEMENTSINSTANCEDANGLEPROC m_glDrawElementsInstancedANGLE;

    std::unique_ptr<GraphicsContext3D::ContextLostCallback> m_contextLostCallback;
};

} // namespace WebCore

#endif // USE(OPENGL_ES)
