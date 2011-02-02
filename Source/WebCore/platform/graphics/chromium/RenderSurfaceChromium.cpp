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
    , m_maskShaderProgram(0)
    , m_shaderSamplerLocation(-1)
    , m_shaderMatrixLocation(-1)
    , m_shaderAlphaLocation(-1)
    , m_maskShaderSamplerLocation(-1)
    , m_maskShaderMaskSamplerLocation(-1)
    , m_maskShaderMatrixLocation(-1)
    , m_maskShaderAlphaLocation(-1)
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
    char fragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha; \n"
        "}                                                   \n";
    char fragmentShaderWithMaskString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform sampler2D s_mask;                           \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  vec4 maskColor = texture2D(s_mask, v_texCoord);   \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha * maskColor.w; \n"
        "}                                                   \n";

    m_shaderProgram = LayerChromium::createShaderProgram(m_context, vertexShaderString, fragmentShaderString);
    m_maskShaderProgram = LayerChromium::createShaderProgram(m_context, vertexShaderString, fragmentShaderWithMaskString);
    if (!m_shaderProgram || !m_maskShaderProgram) {
        LOG_ERROR("RenderSurfaceChromium: Failed to create shader program");
        return;
    }

    GLC(m_context, m_shaderSamplerLocation = m_context->getUniformLocation(m_shaderProgram, "s_texture"));
    GLC(m_context, m_shaderMatrixLocation = m_context->getUniformLocation(m_shaderProgram, "matrix"));
    GLC(m_context, m_shaderAlphaLocation = m_context->getUniformLocation(m_shaderProgram, "alpha"));

    GLC(m_context, m_maskShaderSamplerLocation = m_context->getUniformLocation(m_maskShaderProgram, "s_texture"));
    GLC(m_context, m_maskShaderMaskSamplerLocation = m_context->getUniformLocation(m_maskShaderProgram, "s_mask"));
    GLC(m_context, m_maskShaderMatrixLocation = m_context->getUniformLocation(m_maskShaderProgram, "matrix"));
    GLC(m_context, m_maskShaderAlphaLocation = m_context->getUniformLocation(m_maskShaderProgram, "alpha"));

    if (m_shaderSamplerLocation == -1 || m_shaderMatrixLocation == -1 || m_shaderAlphaLocation == -1
        || m_maskShaderSamplerLocation == -1 || m_maskShaderMaskSamplerLocation == -1 || m_maskShaderMatrixLocation == -1 || m_maskShaderAlphaLocation == -1) {
        LOG_ERROR("Failed to initialize render surface shaders.");
        return;
    }

    GLC(m_context, m_context->useProgram(m_shaderProgram));
    GLC(m_context, m_context->uniform1i(m_shaderSamplerLocation, 0));
    GLC(m_context, m_context->useProgram(m_maskShaderProgram));
    GLC(m_context, m_context->uniform1i(m_maskShaderSamplerLocation, 0));
    GLC(m_context, m_context->uniform1i(m_maskShaderMaskSamplerLocation, 1));
    GLC(m_context, m_context->useProgram(0));
    m_initialized = true;
}

RenderSurfaceChromium::SharedValues::~SharedValues()
{
    if (m_shaderProgram)
        GLC(m_context, m_context->deleteProgram(m_shaderProgram));
    if (m_maskShaderProgram)
        GLC(m_context, m_context->deleteProgram(m_maskShaderProgram));
}

RenderSurfaceChromium::RenderSurfaceChromium(LayerChromium* owningLayer)
    : m_owningLayer(owningLayer)
    , m_maskLayer(0)
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

FloatRect RenderSurfaceChromium::drawableContentRect() const
{
    FloatRect localContentRect(-0.5 * m_contentRect.width(), -0.5 * m_contentRect.height(),
                               m_contentRect.width(), m_contentRect.height());
    FloatRect drawableContentRect = m_drawTransform.mapRect(localContentRect);
    if (m_owningLayer->replicaLayer())
        drawableContentRect.unite(m_replicaDrawTransform.mapRect(localContentRect));

    return drawableContentRect;
}

bool RenderSurfaceChromium::prepareContentsTexture()
{
    IntSize requiredSize(m_contentRect.size());
    TextureManager* textureManager = layerRenderer()->textureManager();

    if (!m_contentsTexture)
        m_contentsTexture = LayerTexture::create(layerRenderer()->context(), textureManager);

    if (m_contentsTexture->isReserved())
        return true;

    if (!m_contentsTexture->reserve(requiredSize, GraphicsContext3D::RGBA)) {
        m_skipsDraw = true;
        return false;
    }

    m_skipsDraw = false;
    return true;
}

void RenderSurfaceChromium::drawSurface(LayerChromium* maskLayer, const TransformationMatrix& drawTransform)
{
    GraphicsContext3D* context3D = layerRenderer()->context();

    int shaderMatrixLocation = -1;
    int shaderAlphaLocation = -1;
    const RenderSurfaceChromium::SharedValues* sv = layerRenderer()->renderSurfaceSharedValues();
    ASSERT(sv && sv->initialized());
    bool useMask = false;
    if (maskLayer && maskLayer->drawsContent()) {
        maskLayer->updateContentsIfDirty();
        if (!maskLayer->bounds().isEmpty()) {
            context3D->makeContextCurrent();
            layerRenderer()->useShader(sv->maskShaderProgram());
            GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE0));
            m_contentsTexture->bindTexture();
            GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE1));
            maskLayer->bindContentsTexture();
            GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE0));
            shaderMatrixLocation = sv->maskShaderMatrixLocation();
            shaderAlphaLocation = sv->maskShaderAlphaLocation();
            useMask = true;
        }
    }

    if (!useMask) {
        layerRenderer()->useShader(sv->shaderProgram());
        m_contentsTexture->bindTexture();
        shaderMatrixLocation = sv->shaderMatrixLocation();
        shaderAlphaLocation = sv->shaderAlphaLocation();
    }
    
    LayerChromium::drawTexturedQuad(layerRenderer()->context(), layerRenderer()->projectionMatrix(), drawTransform,
                                        m_contentRect.width(), m_contentRect.height(), m_drawOpacity,
                                        shaderMatrixLocation, shaderAlphaLocation);

    m_contentsTexture->unreserve();

    if (maskLayer)
        maskLayer->unreserveContentsTexture();
}

void RenderSurfaceChromium::draw()
{
    if (m_skipsDraw || !m_contentsTexture)
        return;
    // FIXME: By using the same RenderSurface for both the content and its reflection,
    // it's currently not possible to apply a separate mask to the reflection layer
    // or correctly handle opacity in reflections (opacity must be applied after drawing
    // both the layer and its reflection). The solution is to introduce yet another RenderSurface
    // to draw the layer and its reflection in. For now we only apply a separate reflection
    // mask if the contents don't have a mask of their own.
    LayerChromium* replicaMaskLayer = m_maskLayer;
    if (!m_maskLayer && m_owningLayer->replicaLayer())
        replicaMaskLayer = m_owningLayer->replicaLayer()->maskLayer();

    layerRenderer()->setScissorToRect(m_scissorRect);

    // Reflection draws before the layer.
    if (m_owningLayer->replicaLayer()) 
        drawSurface(replicaMaskLayer, m_replicaDrawTransform);

    drawSurface(m_maskLayer, m_drawTransform);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
