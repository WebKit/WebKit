/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CanvasLayerChromium.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"

namespace WebCore {

unsigned CanvasLayerChromium::m_shaderProgramId = 0;

CanvasLayerChromium::SharedValues::SharedValues(GraphicsContext3D* context)
    : m_context(context)
    , m_canvasShaderProgram(0)
    , m_shaderSamplerLocation(-1)
    , m_shaderMatrixLocation(-1)
    , m_shaderAlphaLocation(-1)
    , m_initialized(false)
{
    char vertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "attribute vec2 a_texCoord;   \n"
        "uniform mat4 matrix;         \n"
        "varying vec2 v_texCoord;     \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_Position = matrix * a_position; \n"
        "  v_texCoord = a_texCoord;   \n"
        "}                            \n";

    // Canvas layers need to be flipped vertically and their colors shouldn't be
    // swizzled.
    char fragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, vec2(v_texCoord.x, 1.0 - v_texCoord.y)); \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha; \n"
        "}                                                   \n";

    m_canvasShaderProgram = createShaderProgram(m_context, vertexShaderString, fragmentShaderString);
    if (!m_canvasShaderProgram) {
        LOG_ERROR("CanvasLayerChromium: Failed to create shader program");
        return;
    }

    m_shaderSamplerLocation = m_context->getUniformLocation(m_canvasShaderProgram, "s_texture");
    m_shaderMatrixLocation = m_context->getUniformLocation(m_canvasShaderProgram, "matrix");
    m_shaderAlphaLocation = m_context->getUniformLocation(m_canvasShaderProgram, "alpha");
    ASSERT(m_shaderSamplerLocation != -1);
    ASSERT(m_shaderMatrixLocation != -1);
    ASSERT(m_shaderAlphaLocation != -1);

    m_initialized = true;
}

CanvasLayerChromium::SharedValues::~SharedValues()
{
    if (m_canvasShaderProgram)
        GLC(m_context, m_context->deleteProgram(m_canvasShaderProgram));
}

CanvasLayerChromium::CanvasLayerChromium(GraphicsLayerChromium* owner)
    : LayerChromium(owner)
    , m_textureChanged(true)
    , m_textureId(0)
{
}

CanvasLayerChromium::~CanvasLayerChromium()
{
}

void CanvasLayerChromium::draw()
{
    ASSERT(layerRenderer());
    const CanvasLayerChromium::SharedValues* sv = layerRenderer()->canvasLayerSharedValues();
    ASSERT(sv && sv->initialized());
    GraphicsContext3D* context = layerRendererContext();
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textureId));
    layerRenderer()->useShader(sv->canvasShaderProgram());
    GLC(context, context->uniform1i(sv->shaderSamplerLocation(), 0));
    drawTexturedQuad(context, layerRenderer()->projectionMatrix(), drawTransform(),
                     bounds().width(), bounds().height(), drawOpacity(),
                     sv->shaderMatrixLocation(), sv->shaderAlphaLocation());

}

}
#endif // USE(ACCELERATED_COMPOSITING)
