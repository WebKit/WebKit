/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "RenderSurfaceChromium.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "LayerTexture.h"

namespace WebCore {

RenderSurfaceChromium::SharedValues::SharedValues(GraphicsContext3D* context)
    : m_context(context)
    , m_shaderProgram(0)
    , m_shaderSamplerLocation(-1)
    , m_shaderMatrixLocation(-1)
    , m_shaderAlphaLocation(-1)
    , m_initialized(false)
{
    // The following program composites layers whose contents are the results of a previous
    // render operation and therefore doesn't perform any color swizzling. It is used
    // in scrolling and for compositing offscreen textures.
    char renderSurfaceVertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "attribute vec2 a_texCoord;   \n"
        "uniform mat4 matrix;         \n"
        "varying vec2 v_texCoord;     \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_Position = matrix * a_position; \n"
        "  v_texCoord = a_texCoord;   \n"
        "}                            \n";
    char renderSurfaceFragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha; \n"
        "}                                                   \n";

    m_shaderProgram = LayerChromium::createShaderProgram(m_context, renderSurfaceVertexShaderString, renderSurfaceFragmentShaderString);
    if (!m_shaderProgram) {
        LOG_ERROR("RenderSurfaceChromium: Failed to create shader program");
        return;
    }

    GLC(m_context, m_shaderSamplerLocation = m_context->getUniformLocation(m_shaderProgram, "s_texture"));
    GLC(m_context, m_shaderMatrixLocation = m_context->getUniformLocation(m_shaderProgram, "matrix"));
    GLC(m_context, m_shaderAlphaLocation = m_context->getUniformLocation(m_shaderProgram, "alpha"));
    if (m_shaderSamplerLocation == -1 || m_shaderMatrixLocation == -1 || m_shaderAlphaLocation == -1) {
        LOG_ERROR("Failed to initialize texture layer shader.");
        return;
    }
    m_initialized = true;
}

RenderSurfaceChromium::SharedValues::~SharedValues()
{
    if (m_shaderProgram)
        GLC(m_context, m_context->deleteProgram(m_shaderProgram));
}

RenderSurfaceChromium::RenderSurfaceChromium(LayerChromium* owningLayer)
    : m_owningLayer(owningLayer)
    , m_skipsDraw(false)
{
}

RenderSurfaceChromium::~RenderSurfaceChromium()
{
    cleanupResources();
}

void RenderSurfaceChromium::cleanupResources()
{
    if (!m_contentsTexture)
        return;

    ASSERT(layerRenderer());

    m_contentsTexture.clear();
}

LayerRendererChromium* RenderSurfaceChromium::layerRenderer()
{
    ASSERT(m_owningLayer);
    return m_owningLayer->layerRenderer();
}

bool RenderSurfaceChromium::prepareContentsTexture()
{
    IntSize requiredSize(m_contentRect.size());
    TextureManager* textureManager = layerRenderer()->textureManager();

    if (!m_contentsTexture)
        m_contentsTexture = LayerTexture::create(layerRenderer()->context(), textureManager);

    if (!m_contentsTexture->reserve(requiredSize, GraphicsContext3D::RGBA)) {
        m_skipsDraw = true;
        return false;
    }

    m_skipsDraw = false;
    return true;
}

void RenderSurfaceChromium::draw()
{
    if (m_skipsDraw || !m_contentsTexture)
        return;

    m_contentsTexture->bindTexture();

    const RenderSurfaceChromium::SharedValues* sv = layerRenderer()->renderSurfaceSharedValues();
    ASSERT(sv && sv->initialized());

    layerRenderer()->useShader(sv->shaderProgram());
    layerRenderer()->setScissorToRect(m_scissorRect);

    LayerChromium::drawTexturedQuad(layerRenderer()->context(), layerRenderer()->projectionMatrix(), m_drawTransform,
                                    m_contentRect.width(), m_contentRect.height(), m_drawOpacity,
                                    sv->shaderMatrixLocation(), sv->shaderAlphaLocation());

    m_contentsTexture->unreserve();
}

}
#endif // USE(ACCELERATED_COMPOSITING)
