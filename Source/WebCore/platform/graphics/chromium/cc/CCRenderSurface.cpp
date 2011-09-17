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
#include "ManagedTexture.h"
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

    m_contentsTexture.clear();
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

bool CCRenderSurface::prepareContentsTexture(LayerRendererChromium* layerRenderer)
{
    IntSize requiredSize(m_contentRect.size());
    TextureManager* textureManager = layerRenderer->renderSurfaceTextureManager();

    if (!m_contentsTexture)
        m_contentsTexture = ManagedTexture::create(textureManager);

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

void CCRenderSurface::draw(LayerRendererChromium* layerRenderer, const IntRect&)
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
        layerRenderer->setScissorToRect(m_scissorRect);
    else
        GLC(layerRenderer->context(), layerRenderer->context()->disable(GraphicsContext3D::SCISSOR_TEST));

    // Reflection draws before the layer.
    if (m_owningLayer->replicaLayer())
        drawLayer(layerRenderer, replicaMaskLayer, m_replicaDrawTransform);

    drawLayer(layerRenderer, m_maskLayer, m_drawTransform);
}

void CCRenderSurface::drawLayer(LayerRendererChromium* layerRenderer, CCLayerImpl* maskLayer, const TransformationMatrix& drawTransform)
{
    TransformationMatrix renderMatrix = drawTransform;
    // Apply a scaling factor to size the quad from 1x1 to its intended size.
    renderMatrix.scale3d(m_contentRect.width(), m_contentRect.height(), 1);

    TransformationMatrix deviceMatrix = TransformationMatrix(layerRenderer->windowMatrix() * layerRenderer->projectionMatrix() * renderMatrix).to2dTransform();

    // Can only draw surface if device matrix is invertible.
    if (!deviceMatrix.isInvertible())
        return;

    FloatQuad quad = deviceMatrix.mapQuad(layerRenderer->sharedGeometryQuad());
    CCLayerQuad layerQuad = CCLayerQuad(quad);

#if defined(OS_CHROMEOS)
    // FIXME: Disable anti-aliasing to workaround broken driver.
    bool useAA = false;
#else
    // Use anti-aliasing programs only when necessary.
    bool useAA = (!quad.isRectilinear() || !quad.boundingBox().isExpressibleAsIntRect());
#endif

    if (useAA)
        layerQuad.inflateAntiAliasingDistance();

    bool useMask = false;
    if (maskLayer && maskLayer->drawsContent())
        if (!maskLayer->bounds().isEmpty())
            useMask = true;

    if (useMask) {
        if (useAA) {
            const MaskProgramAA* program = layerRenderer->renderSurfaceMaskProgramAA();
            drawSurface(layerRenderer, maskLayer, drawTransform, deviceMatrix, layerQuad, program, program->fragmentShader().maskSamplerLocation(), program->vertexShader().pointLocation(), program->fragmentShader().edgeLocation());
        } else {
            const MaskProgram* program = layerRenderer->renderSurfaceMaskProgram();
            drawSurface(layerRenderer, maskLayer, drawTransform, deviceMatrix, layerQuad, program, program->fragmentShader().maskSamplerLocation(), -1, -1);
        }
    } else {
        if (useAA) {
            const ProgramAA* program = layerRenderer->renderSurfaceProgramAA();
            drawSurface(layerRenderer, maskLayer, drawTransform, deviceMatrix, layerQuad, program, -1, program->vertexShader().pointLocation(), program->fragmentShader().edgeLocation());
        } else {
            const Program* program = layerRenderer->renderSurfaceProgram();
            drawSurface(layerRenderer, maskLayer, drawTransform, deviceMatrix, layerQuad, program, -1, -1, -1);
        }
    }
}

template <class T>
void CCRenderSurface::drawSurface(LayerRendererChromium* layerRenderer, CCLayerImpl* maskLayer, const TransformationMatrix& drawTransform, const TransformationMatrix& deviceTransform, const CCLayerQuad& layerQuad, const T* program, int shaderMaskSamplerLocation, int shaderQuadLocation, int shaderEdgeLocation)
{
    GraphicsContext3D* context3D = layerRenderer->context();

    context3D->makeContextCurrent();
    GLC(context3D, context3D->useProgram(program->program()));

    GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context3D, context3D->uniform1i(program->fragmentShader().samplerLocation(), 0));
    m_contentsTexture->bindTexture(context3D);

    if (shaderMaskSamplerLocation != -1) {
        GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE1));
        GLC(context3D, context3D->uniform1i(shaderMaskSamplerLocation, 1));
        maskLayer->bindContentsTexture(layerRenderer);
        GLC(context3D, context3D->activeTexture(GraphicsContext3D::TEXTURE0));
    }

    if (shaderEdgeLocation != -1) {
        float edge[12];
        layerQuad.toFloatArray(edge);
        GLC(context3D, context3D->uniform3fv(shaderEdgeLocation, edge, 4));
    }

    // Map device space quad to layer space.
    FloatQuad quad = deviceTransform.inverse().mapQuad(layerQuad.floatQuad());

    layerRenderer->drawTexturedQuad(drawTransform, m_contentRect.width(), m_contentRect.height(), m_drawOpacity, quad,
                                    program->vertexShader().matrixLocation(), program->fragmentShader().alphaLocation(), shaderQuadLocation);
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
