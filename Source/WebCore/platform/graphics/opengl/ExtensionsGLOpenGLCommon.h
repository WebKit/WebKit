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

#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class GraphicsContextGLOpenGL;

class ExtensionsGLOpenGLCommon : public ExtensionsGL {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~ExtensionsGLOpenGLCommon();

    // ExtensionsGL methods.
    bool supports(const String&) override;
    void ensureEnabled(const String&) override;
    bool isEnabled(const String&) override;
    int getGraphicsResetStatusARB() override;

    // GL_OES_vertex_array_object
    virtual PlatformGLObject createVertexArrayOES() = 0;
    virtual void deleteVertexArrayOES(PlatformGLObject) = 0;
    virtual GCGLboolean isVertexArrayOES(PlatformGLObject) = 0;
    virtual void bindVertexArrayOES(PlatformGLObject) = 0;

    void drawBuffersEXT(GCGLSpan<const GCGLenum>) override = 0;

    String getTranslatedShaderSourceANGLE(PlatformGLObject) override;

    virtual void drawArraysInstancedANGLE(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount) = 0;
    virtual void drawElementsInstancedANGLE(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLvoidptr offset, GCGLsizei primcount) = 0;
    virtual void vertexAttribDivisorANGLE(GCGLuint index, GCGLuint divisor) = 0;

    // EXT Robustness - uses getGraphicsResetStatusARB()
    void readnPixelsEXT(int x, int y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, void *data) override;
    void getnUniformfvEXT(GCGLuint program, int location, GCGLsizei bufSize, float *params) override;
    void getnUniformivEXT(GCGLuint program, int location, GCGLsizei bufSize, int *params) override;

    bool isNVIDIA() const { return m_isNVIDIA; }
    bool isAMD() const { return m_isAMD; }
    bool isIntel() const { return m_isIntel; }
    bool isImagination() const { return m_isImagination; }
    String vendor() const { return m_vendor; }

    bool requiresBuiltInFunctionEmulation() const { return m_requiresBuiltInFunctionEmulation; }
    bool requiresRestrictedMaximumTextureSize() const { return m_requiresRestrictedMaximumTextureSize; }

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
