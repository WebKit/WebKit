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

#ifndef GeometryBinding_h
#define GeometryBinding_h

#if USE(ACCELERATED_COMPOSITING)

#include "PlatformString.h"

namespace WebCore {

class GraphicsContext3D;

class GeometryBinding {
public:
    explicit GeometryBinding(GraphicsContext3D*);
    ~GeometryBinding();

    bool initialized() const { return m_initialized; }

    GraphicsContext3D* context() const { return m_context; }
    unsigned quadVerticesVbo() const { return m_quadVerticesVbo; }
    unsigned quadElementsVbo() const { return m_quadElementsVbo; }

    void prepareForDraw();

    // All layer shaders share the same attribute locations for the vertex
    // positions and texture coordinates. This allows switching shaders without
    // rebinding attribute arrays.
    static int positionAttribLocation() { return 0; }
    static int texCoordAttribLocation() { return 1; }

private:
    GraphicsContext3D* m_context;
    unsigned m_quadVerticesVbo;
    unsigned m_quadElementsVbo;
    bool m_initialized;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
