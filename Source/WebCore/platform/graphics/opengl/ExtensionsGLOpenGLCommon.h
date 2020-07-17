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

#include "ExtensionsGL.h"

#include "GraphicsContextGLOpenGL.h"
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class ExtensionsGLOpenGLCommon : public ExtensionsGL {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~ExtensionsGLOpenGLCommon();

    // ExtensionsGL methods.
    bool supports(const String&) override;
    void ensureEnabled(const String&) override;
    bool isEnabled(const String&) override;
    int getGraphicsResetStatusARB() override;

    PlatformGLObject createVertexArrayOES() override = 0;
    void deleteVertexArrayOES(PlatformGLObject) override = 0;
    GCGLboolean isVertexArrayOES(PlatformGLObject) override = 0;
    void bindVertexArrayOES(PlatformGLObject) override = 0;

    void drawBuffersEXT(GCGLsizei, const GCGLenum*) override = 0;

    String getTranslatedShaderSourceANGLE(PlatformGLObject) override;

    // EXT Robustness - uses getGraphicsResetStatusARB()
    void readnPixelsEXT(int x, int y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, void *data) override;
    void getnUniformfvEXT(GCGLuint program, int location, GCGLsizei bufSize, float *params) override;
    void getnUniformivEXT(GCGLuint program, int location, GCGLsizei bufSize, int *params) override;

    bool isNVIDIA() override { return m_isNVIDIA; }
    bool isAMD() override { return m_isAMD; }
    bool isIntel() override { return m_isIntel; }
    bool isImagination() override { return m_isImagination; }
    String vendor() override { return m_vendor; }

    bool requiresBuiltInFunctionEmulation() override { return m_requiresBuiltInFunctionEmulation; }
    bool requiresRestrictedMaximumTextureSize() override { return m_requiresRestrictedMaximumTextureSize; }

    // GL_ANGLE_robust_client_memory
    void getBooleanvRobustANGLE(GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLboolean *data) override;
    void getBufferParameterivRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getFloatvRobustANGLE(GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLfloat *data) override;
    void getFramebufferAttachmentParameterivRobustANGLE(GCGLenum target, GCGLenum attachment, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getIntegervRobustANGLE(GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *data) override;
    void getProgramivRobustANGLE(GCGLuint program, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getRenderbufferParameterivRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getShaderivRobustANGLE(GCGLuint shader, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getTexParameterfvRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLfloat *params) override;
    void getTexParameterivRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getUniformfvRobustANGLE(GCGLuint program, int location, GCGLsizei bufSize, GCGLsizei *length, GCGLfloat *params) override;
    void getUniformivRobustANGLE(GCGLuint program, int location, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getVertexAttribfvRobustANGLE(GCGLuint index, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLfloat *params) override;
    void getVertexAttribivRobustANGLE(GCGLuint index, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getVertexAttribPointervRobustANGLE(GCGLuint index, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, void **pointer) override;
    void readPixelsRobustANGLE(int x, int y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, GCGLsizei *length, GCGLsizei *columns, GCGLsizei *rows, void *pixels) override;
    void texImage2DRobustANGLE(GCGLenum target, int level, int internalformat, GCGLsizei width, GCGLsizei height, int border, GCGLenum format, GCGLenum type, GCGLsizei bufSize, const void *pixels) override;
    void texParameterfvRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, const GCGLfloat *params) override;
    void texParameterivRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, const GCGLint *params) override;
    void texSubImage2DRobustANGLE(GCGLenum target, int level, int xoffset, int yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, const void *pixels) override;
    void compressedTexImage2DRobustANGLE(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, int border, GCGLsizei imageSize, GCGLsizei bufSize, const void* data) override;
    void compressedTexSubImage2DRobustANGLE(GCGLenum target, int level, int xoffset, int yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLsizei bufSize, const void* data) override;
    void compressedTexImage3DRobustANGLE(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLsizei imageSize, GCGLsizei bufSize, const void* data) override;
    void compressedTexSubImage3DRobustANGLE(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLsizei bufSize, const void* data) override;

    void texImage3DRobustANGLE(GCGLenum target, int level, int internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLenum format, GCGLenum type, GCGLsizei bufSize, const void *pixels) override;
    void texSubImage3DRobustANGLE(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLsizei bufSize, const void *pixels) override;
    void getQueryivRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getQueryObjectuivRobustANGLE(GCGLuint id, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLuint *params) override;
    void getBufferPointervRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, void **params) override;
    void getIntegeri_vRobustANGLE(GCGLenum target, GCGLuint index, GCGLsizei bufSize, GCGLsizei *length, int *data) override;
    void getInternalformativRobustANGLE(GCGLenum target, GCGLenum internalformat, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getVertexAttribIivRobustANGLE(GCGLuint index, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getVertexAttribIuivRobustANGLE(GCGLuint index, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLuint *params) override;
    void getUniformuivRobustANGLE(GCGLuint program, int location, GCGLsizei bufSize, GCGLsizei *length, GCGLuint *params) override;
    void getActiveUniformBlockivRobustANGLE(GCGLuint program, GCGLuint uniformBlockIndex, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getInteger64vRobustANGLE(GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint64 *data) override;
    void getInteger64i_vRobustANGLE(GCGLenum target, GCGLuint index, GCGLsizei bufSize, GCGLsizei *length, GCGLint64 *data) override;
    void getBufferParameteri64vRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint64 *params) override;
    void samplerParameterivRobustANGLE(GCGLuint sampler, GCGLenum pname, GCGLsizei bufSize, const GCGLint *param) override;
    void samplerParameterfvRobustANGLE(GCGLuint sampler, GCGLenum pname, GCGLsizei bufSize, const GCGLfloat *param) override;
    void getSamplerParameterivRobustANGLE(GCGLuint sampler, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getSamplerParameterfvRobustANGLE(GCGLuint sampler, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLfloat *params) override;

    void getFramebufferParameterivRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getProgramInterfaceivRobustANGLE(GCGLuint program, GCGLenum programInterface, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getBooleani_vRobustANGLE(GCGLenum target, GCGLuint index, GCGLsizei bufSize, GCGLsizei *length, GCGLboolean *data) override;
    void getMultisamplefvRobustANGLE(GCGLenum pname, GCGLuint index, GCGLsizei bufSize, GCGLsizei *length, GCGLfloat *val) override;
    void getTexLevelParameterivRobustANGLE(GCGLenum target, int level, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getTexLevelParameterfvRobustANGLE(GCGLenum target, int level, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLfloat *params) override;

    void getPointervRobustANGLERobustANGLE(GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, void **params) override;
    void readnPixelsRobustANGLE(int x, int y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, GCGLsizei *length, GCGLsizei *columns, GCGLsizei *rows, void *data, bool readingToPixelBufferObject) override;
    void getnUniformfvRobustANGLE(GCGLuint program, int location, GCGLsizei bufSize, GCGLsizei *length, GCGLfloat *params) override;
    void getnUniformivRobustANGLE(GCGLuint program, int location, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getnUniformuivRobustANGLE(GCGLuint program, int location, GCGLsizei bufSize, GCGLsizei *length, GCGLuint *params) override;
    void texParameterIivRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, const GCGLint *params) override;
    void texParameterIuivRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, const GCGLuint *params) override;
    void getTexParameterIivRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getTexParameterIuivRobustANGLE(GCGLenum target, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLuint *params) override;
    void samplerParameterIivRobustANGLE(GCGLuint sampler, GCGLenum pname, GCGLsizei bufSize, const GCGLint *param) override;
    void samplerParameterIuivRobustANGLE(GCGLuint sampler, GCGLenum pname, GCGLsizei bufSize, const GCGLuint *param) override;
    void getSamplerParameterIivRobustANGLE(GCGLuint sampler, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getSamplerParameterIuivRobustANGLE(GCGLuint sampler, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLuint *params) override;

    void getQueryObjectivRobustANGLE(GCGLuint id, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint *params) override;
    void getQueryObjecti64vRobustANGLE(GCGLuint id, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLint64 *params) override;
    void getQueryObjectui64vRobustANGLE(GCGLuint id, GCGLenum pname, GCGLsizei bufSize, GCGLsizei *length, GCGLuint64 *params) override;

protected:
    friend class ExtensionsGLOpenGLES;
    ExtensionsGLOpenGLCommon(GraphicsContextGLOpenGL*, bool useIndexedGetString);

    virtual bool supportsExtension(const String&) = 0;
    virtual String getExtensions() = 0;

    virtual void initializeAvailableExtensions();
    bool m_initializedAvailableExtensions;
    HashSet<String> m_availableExtensions;

    // Weak pointer back to GraphicsContextGLOpenGL
    GraphicsContextGLOpenGL* m_context;

    bool m_isNVIDIA;
    bool m_isAMD;
    bool m_isIntel;
    bool m_isImagination;
    bool m_requiresBuiltInFunctionEmulation;
    bool m_requiresRestrictedMaximumTextureSize;

    bool m_useIndexedGetString { false };

    String m_vendor;
    String m_renderer;
};

} // namespace WebCore
