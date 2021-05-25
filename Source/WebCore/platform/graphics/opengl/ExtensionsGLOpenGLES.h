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

#include "ExtensionsGLOpenGLCommon.h"

#if USE(OPENGL_ES)

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#if HAVE(OPENGL_ES_3)
#include <GLES3/gl3.h>
#endif // HAVE(OPENGL_ES_3)
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
GL_APICALL GCGLenum GL_APIENTRY glGetGraphicsResetStatusEXT(void);
GL_APICALL void GL_APIENTRY glReadnPixelsEXT(GLint x, GLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, void *data);
GL_APICALL void GL_APIENTRY glGetnUniformfvEXT(GLuint program, GLint location, GCGLsizei bufSize, float *params);
GL_APICALL void GL_APIENTRY glGetnUniformivEXT(GLuint program, GLint location, GCGLsizei bufSize, GLint *params);
#endif
typedef GCGLenum (GL_APIENTRYP PFNGLGETGRAPHICSRESETSTATUSEXTPROC) (void);
typedef void (GL_APIENTRYP PFNGLREADNPIXELSEXTPROC) (GLint x, GLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, void *data);
typedef void (GL_APIENTRYP PFNGLGETNUNIFORMFVEXTPROC) (GLuint program, GLint location, GCGLsizei bufSize, float *params);
typedef void (GL_APIENTRYP PFNGLGETNUNIFORMIVEXTPROC) (GLuint program, GLint location, GCGLsizei bufSize, GLint *params);
#endif

namespace WebCore {

class GraphicsContextGLOpenGL;

class ExtensionsGLOpenGLES : public ExtensionsGLOpenGLCommon {
public:
    // This class only needs to be instantiated by GraphicsContextGLOpenGL implementations.
    ExtensionsGLOpenGLES(GraphicsContextGLOpenGL*, bool useIndexedGetString);
    virtual ~ExtensionsGLOpenGLES();

    virtual void framebufferTexture2DMultisampleIMG(unsigned long target, unsigned long attachment, unsigned long textarget, unsigned texture, int level, unsigned long samples);
    virtual void renderbufferStorageMultisampleIMG(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height);

    // Extension3D methods
    bool isEnabled(const String&) override;
    void renderbufferStorageMultisampleANGLE(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height);

    PlatformGLObject createVertexArrayOES() override;
    void deleteVertexArrayOES(PlatformGLObject) override;
    GCGLboolean isVertexArrayOES(PlatformGLObject) override;
    void bindVertexArrayOES(PlatformGLObject) override;
    void drawBuffersEXT(GCGLSpan<const GCGLenum>) override;

    void drawArraysInstancedANGLE(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount) override;
    void drawElementsInstancedANGLE(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLvoidptr offset, GCGLsizei primcount) override;
    void vertexAttribDivisorANGLE(GCGLuint index, GCGLuint divisor) override;

    // EXT Robustness - reset
    int getGraphicsResetStatusARB() override;

    // EXT Robustness - etc
    void readnPixelsEXT(int x, int y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, void *data) override;
    void getnUniformfvEXT(GCGLuint program, int location, GCGLsizei bufSize, float *params) override;
    void getnUniformivEXT(GCGLuint program, int location, GCGLsizei bufSize, int *params) override;

protected:
    bool supportsExtension(const String&) override;
    String getExtensions() override;

    GCGLenum m_contextResetStatus;

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
};

} // namespace WebCore

#endif // USE(OPENGL_ES)
