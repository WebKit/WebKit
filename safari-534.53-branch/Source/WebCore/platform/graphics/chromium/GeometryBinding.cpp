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

#if USE(ACCELERATED_COMPOSITING)

#include "GeometryBinding.h"

#include "GraphicsContext.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"

namespace WebCore {

GeometryBinding::GeometryBinding(GraphicsContext3D* context)
    : m_context(context)
    , m_quadVerticesVbo(0)
    , m_quadElementsVbo(0)
    , m_initialized(false)
{
    // Vertex positions and texture coordinates for the 4 corners of a 1x1 quad.
    float vertices[] = { -0.5f,  0.5f, 0.0f, 0.0f,  1.0f,
                         -0.5f, -0.5f, 0.0f, 0.0f,  0.0f,
                         0.5f, -0.5f, 0.0f, 1.0f,  0.0f,
                         0.5f,  0.5f, 0.0f, 1.0f,  1.0f };
    uint16_t indices[] = { 0, 1, 2, 0, 2, 3, // The two triangles that make up the layer quad.
                           0, 1, 2, 3}; // A line path for drawing the layer border.

    GLC(m_context, m_quadVerticesVbo = m_context->createBuffer());
    GLC(m_context, m_quadElementsVbo = m_context->createBuffer());
    GLC(m_context, m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_quadVerticesVbo));
    GLC(m_context, m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, sizeof(vertices), vertices, GraphicsContext3D::STATIC_DRAW));
    GLC(m_context, m_context->bindBuffer(GraphicsContext3D::ELEMENT_ARRAY_BUFFER, m_quadElementsVbo));
    GLC(m_context, m_context->bufferData(GraphicsContext3D::ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GraphicsContext3D::STATIC_DRAW));

    m_initialized = true;
}

GeometryBinding::~GeometryBinding()
{
    GLC(m_context, m_context->deleteBuffer(m_quadVerticesVbo));
    GLC(m_context, m_context->deleteBuffer(m_quadElementsVbo));
}

void GeometryBinding::prepareForDraw()
{
    GLC(m_context, m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, quadVerticesVbo()));
    GLC(m_context, m_context->bindBuffer(GraphicsContext3D::ELEMENT_ARRAY_BUFFER, quadElementsVbo()));
    unsigned offset = 0;
    GLC(m_context, m_context->vertexAttribPointer(positionAttribLocation(), 3, GraphicsContext3D::FLOAT, false, 5 * sizeof(float), offset));
    offset += 3 * sizeof(float);
    GLC(m_context, m_context->vertexAttribPointer(texCoordAttribLocation(), 2, GraphicsContext3D::FLOAT, false, 5 * sizeof(float), offset));
    GLC(m_context, m_context->enableVertexAttribArray(positionAttribLocation()));
    GLC(m_context, m_context->enableVertexAttribArray(texCoordAttribLocation()));
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
