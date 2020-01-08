/*
 * Copyright (C) 2019 Google Inc. All rights reserved.
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
#include <wtf/text/StringHash.h>

namespace WebCore {

class ExtensionsGLANGLE : public ExtensionsGL {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // This class only needs to be instantiated by GraphicsContextGLOpenGL implementations.
    ExtensionsGLANGLE(GraphicsContextGLOpenGL*, bool useIndexedGetString = false);
    virtual ~ExtensionsGLANGLE();

    // ExtensionsGL methods.
    bool supports(const String&) override;
    void ensureEnabled(const String&) override;
    bool isEnabled(const String&) override;
    int getGraphicsResetStatusARB() override;

    PlatformGLObject createVertexArrayOES() override;
    void deleteVertexArrayOES(PlatformGLObject) override;
    GCGLboolean isVertexArrayOES(PlatformGLObject) override;
    void bindVertexArrayOES(PlatformGLObject) override;
    void insertEventMarkerEXT(const String&) override;
    void pushGroupMarkerEXT(const String&) override;
    void popGroupMarkerEXT(void) override;
    void drawBuffersEXT(GCGLsizei, const GCGLenum*) override;

    String getTranslatedShaderSourceANGLE(PlatformGLObject) override;

    void blitFramebuffer(long srcX0, long srcY0, long srcX1, long srcY1, long dstX0, long dstY0, long dstX1, long dstY1, unsigned long mask, unsigned long filter) override;
    void renderbufferStorageMultisample(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height) override;

    void drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount) override;
    void drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset, GCGLsizei primcount) override;
    void vertexAttribDivisor(GCGLuint index, GCGLuint divisor) override;

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

private:
    bool supportsExtension(const WTF::String&);
    String getExtensions();

    virtual void initializeAvailableExtensions();
    bool m_initializedAvailableExtensions;
    HashSet<String> m_availableExtensions;
    HashSet<String> m_requestableExtensions;
    HashSet<String> m_enabledExtensions;

    // Weak pointer back to GraphicsContextGLOpenGL.
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
