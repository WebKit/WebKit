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

#pragma once

#include "ExtensionsGLOpenGLCommon.h"

#include <wtf/text/StringHash.h>

namespace WebCore {

class GraphicsContextGLOpenGL;

class ExtensionsGLOpenGL : public ExtensionsGLOpenGLCommon {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // This class only needs to be instantiated by GraphicsContextGLOpenGL implementations.
    ExtensionsGLOpenGL(GraphicsContextGLOpenGL*, bool useIndexedGetString);
    virtual ~ExtensionsGLOpenGL();

    // ExtensionsGL methods.
    PlatformGLObject createVertexArrayOES() override;
    void deleteVertexArrayOES(PlatformGLObject) override;
    GCGLboolean isVertexArrayOES(PlatformGLObject) override;
    void bindVertexArrayOES(PlatformGLObject) override;
    void drawBuffersEXT(GCGLSpan<const GCGLenum>) override;

    void drawArraysInstancedANGLE(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount) override;
    void drawElementsInstancedANGLE(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLvoidptr offset, GCGLsizei primcount) override;
    void vertexAttribDivisorANGLE(GCGLuint index, GCGLuint divisor) override;

protected:
    bool platformSupportsExtension(const String&) override;
    String getExtensions() override;

private:
#if PLATFORM(GTK) || PLATFORM(WIN)
    bool isVertexArrayObjectSupported();
#endif
};

} // namespace WebCore
