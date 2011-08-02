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

#include "cc/CCRenderSurface.h"

#include "GraphicsContext3D.h"
#include "LayerChromium.h"
#include "LayerRendererChromium.h"
#include "LayerTexture.h"
#include "TextStream.h"
#include "cc/CCLayerImpl.h"
#include <wtf/text/CString.h>

namespace WebCore {

CCRenderSurface::CCRenderSurface(CCLayerImpl* owningLayer)
    : m_owningLayer(owningLayer)
    , m_maskLayer(0)
    , m_skipsDraw(false)
    , m_drawOpacity(1)
{
}

CCRenderSurface::~CCRenderSurface()
{
    cleanupResources();
}

void CCRenderSurface::cleanupResources()
{
    if (!m_contentsTexture)
        return;

    ASSERT(layerRenderer());

    m_contentsTexture.clear();
}

LayerRendererChromium* CCRenderSurface::layerRenderer()
{
    ASSERT(m_owningLayer);
    return m_owningLayer->layerRenderer();
}

FloatRect CCRenderSurface::drawableContentRect() const
{
    FloatRect localContentRect(-0.5 * m_contentRect.width(), -0.5 * m_contentRect.height(),
                               m_contentRect.width(), m_contentRect.height());
    FloatRect drawableContentRect = m_drawTransform.mapRect(localContentRect);
    if (m_owningLayer->replicaLayer())
        drawableContentRect.unite(m_replicaDrawTransform.mapRect(localContentRect));

    return drawableContentRect;
}

bool CCRenderSurface::prepareContentsTexture()
{
    IntSize requiredSize(m_contentRect.size());
    TextureManager* textureManager = layerRenderer()->renderSurfaceTextureManager();

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

void CCRenderSurface::releaseContentsTexture()
{
    if (m_skipsDraw || !m_contentsTexture)
        return;
    m_contentsTexture->unreserve();
}


void CCRenderSurface::drawSurface(CCLayerImpl* maskLayer, const TransformationMatrix& drawTransform)
{
    GraphicsContext3D* context3D = layerRenderer()->context();

    int shaderMatrixLocation = -1;
    int shaderAlphaLocation = -1;
    const CCRenderSurface::Program* program = layerRenderer()->renderSurfaceProgram();
    const CCRenderSurface::MaskProgram* maskProgram = layerRenderer()->renderSurfaceMaskProgram();
    ASSERT(program && program->initialized());
    bool useMask = false;
    if (maskLayer && maskLayer->drawsContent()) {
        if (!maskLayer->bounds().isEmpty()) {
            context3D->makeContextCurrent();
            GLC(context3D, context3D->useProgram(maskProgram->program()));
            GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE0));
            GLC(context3D, context3D->uniform1i(maskProgram->fragmentShader().samplerLocation(), 0));
            m_contentsTexture->bindTexture();
            GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE1));
            GLC(context3D, context3D->uniform1i(maskProgram->fragmentShader().maskSamplerLocation(), 1));
            maskLayer->bindContentsTexture();
            GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE0));
            shaderMatrixLocation = maskProgram->vertexShader().matrixLocation();
            shaderAlphaLocation = maskProgram->fragmentShader().alphaLocation();
            useMask = true;
        }
    }

    if (!useMask) {
        GLC(context3D, context3D->useProgram(program->program()));
        m_contentsTexture->bindTexture();
        GLC(context3D, context3D->uniform1i(program->fragmentShader().samplerLocation(), 0));
        shaderMatrixLocation = program->vertexShader().matrixLocation();
        shaderAlphaLocation = program->fragmentShader().alphaLocation();
    }

    LayerChromium::drawTexturedQuad(layerRenderer()->context(), layerRenderer()->projectionMatrix(), drawTransform,
                                        m_contentRect.width(), m_contentRect.height(), m_drawOpacity,
                                        shaderMatrixLocation, shaderAlphaLocation);
}

void CCRenderSurface::draw(const IntRect&)
{
    if (m_skipsDraw || !m_contentsTexture)
        return;
    // FIXME: By using the same RenderSurface for both the content and its reflection,
    // it's currently not possible to apply a separate mask to the reflection layer
    // or correctly handle opacity in reflections (opacity must be applied after drawing
    // both the layer and its reflection). The solution is to introduce yet another RenderSurface
    // to draw the layer and its reflection in. For now we only apply a separate reflection
    // mask if the contents don't have a mask of their own.
    CCLayerImpl* replicaMaskLayer = m_maskLayer;
    if (!m_maskLayer && m_owningLayer->replicaLayer())
        replicaMaskLayer = m_owningLayer->replicaLayer()->maskLayer();

    if (m_owningLayer->parent() && m_owningLayer->parent()->usesLayerScissor())
        layerRenderer()->setScissorToRect(m_scissorRect);
    else
        GLC(layerRenderer()->context(), layerRenderer()->context()->disable(GraphicsContext3D::SCISSOR_TEST));

    // Reflection draws before the layer.
    if (m_owningLayer->replicaLayer())
        drawSurface(replicaMaskLayer, m_replicaDrawTransform);

    drawSurface(m_maskLayer, m_drawTransform);
}

String CCRenderSurface::name() const
{
    return String::format("RenderSurface(id=%i,owner=%s)", m_owningLayer->id(), m_owningLayer->name().utf8().data());
}

static void writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

void CCRenderSurface::dumpSurface(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << name() << "\n";

    writeIndent(ts, indent+1);
    ts << "contentRect: (" << m_contentRect.x() << ", " << m_contentRect.y() << ", " << m_contentRect.width() << ", " << m_contentRect.height() << "\n";
}

int CCRenderSurface::owningLayerId() const
{
    return m_owningLayer ? m_owningLayer->id() : 0;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
