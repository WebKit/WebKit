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

#include "cc/CCLayerImpl.h"

#include "GraphicsContext3D.h"
#include "LayerChromium.h" // FIXME: temporary and bad
#include "LayerRendererChromium.h"
#include "RenderSurfaceChromium.h"
#include <wtf/text/WTFString.h>

namespace {
void toGLMatrix(float* flattened, const WebCore::TransformationMatrix& m)
{
    flattened[0] = m.m11();
    flattened[1] = m.m12();
    flattened[2] = m.m13();
    flattened[3] = m.m14();
    flattened[4] = m.m21();
    flattened[5] = m.m22();
    flattened[6] = m.m23();
    flattened[7] = m.m24();
    flattened[8] = m.m31();
    flattened[9] = m.m32();
    flattened[10] = m.m33();
    flattened[11] = m.m34();
    flattened[12] = m.m41();
    flattened[13] = m.m42();
    flattened[14] = m.m43();
    flattened[15] = m.m44();
}
}


namespace WebCore {

CCLayerImpl::CCLayerImpl(LayerChromium* owner)
    : m_owner(owner)
    , m_anchorPoint(0.5, 0.5)
    , m_anchorPointZ(0)
    , m_doubleSided(true)
    , m_masksToBounds(false)
    , m_opacity(1.0)
    , m_preserves3D(false)
#ifndef NDEBUG
    , m_debugID(owner->debugID())
#endif
    , m_targetRenderSurface(0)
    , m_drawDepth(0)
    , m_drawOpacity(0)
    , m_debugBorderColor(0, 0, 0, 0)
    , m_debugBorderWidth(0)
    , m_renderSurface(0)
    , m_layerRenderer(0)
{
}

CCLayerImpl::~CCLayerImpl()
{
}

// These are pseudo-structural hacks until we get real tree syncing up in this piece.
CCLayerImpl* CCLayerImpl::superlayer() const
{
    return m_owner->superlayer() ? m_owner->superlayer()->ccLayerImpl() : 0;
}

CCLayerImpl* CCLayerImpl::maskLayer() const
{
    return m_owner->maskLayer() ? m_owner->maskLayer()->ccLayerImpl() : 0;
}

CCLayerImpl* CCLayerImpl::replicaLayer() const
{
    return m_owner->replicaLayer() ? m_owner->replicaLayer()->ccLayerImpl() : 0;
}

void CCLayerImpl::setLayerRenderer(LayerRendererChromium* renderer)
{
    m_layerRenderer = renderer;
}

RenderSurfaceChromium* CCLayerImpl::createRenderSurface()
{
    m_renderSurface = new RenderSurfaceChromium(this);
    return m_renderSurface.get();
}

bool CCLayerImpl::descendantsDrawsContent()
{
    const Vector<RefPtr<LayerChromium> >& sublayers = m_owner->getSublayers();
    for (size_t i = 0; i < sublayers.size(); ++i) {
        sublayers[i]->createCCLayerImplIfNeeded();
        if (sublayers[i]->ccLayerImpl()->drawsContent() || sublayers[i]->ccLayerImpl()->descendantsDrawsContent())
            return true;
    }
    return false;
}

// These belong on CCLayerImpl, but should be overridden by each type and not defer to the LayerChromium subtypes.
bool CCLayerImpl::drawsContent() const
{
    return m_owner && m_owner->drawsContent();
}

void CCLayerImpl::draw(const IntRect& targetSurfaceRect)
{
    return m_owner->draw(targetSurfaceRect);
}

void CCLayerImpl::updateCompositorResources()
{
    return m_owner->updateCompositorResources();
}

void CCLayerImpl::unreserveContentsTexture()
{
    m_owner->unreserveContentsTexture();
}

void CCLayerImpl::bindContentsTexture()
{
    m_owner->bindContentsTexture();
}

void CCLayerImpl::cleanupResources()
{
    if (renderSurface())
        renderSurface()->cleanupResources();
}

const IntRect CCLayerImpl::getDrawRect() const
{
    // Form the matrix used by the shader to map the corners of the layer's
    // bounds into the view space.
    FloatRect layerRect(-0.5 * bounds().width(), -0.5 * bounds().height(), bounds().width(), bounds().height());
    IntRect mappedRect = enclosingIntRect(drawTransform().mapRect(layerRect));
    return mappedRect;
}

void CCLayerImpl::drawDebugBorder()
{
    static float glMatrix[16];
    if (!debugBorderColor().alpha())
        return;

    ASSERT(layerRenderer());
    const LayerChromium::BorderProgram* program = layerRenderer()->borderProgram();
    ASSERT(program && program->initialized());
    layerRenderer()->useShader(program->program());
    TransformationMatrix renderMatrix = drawTransform();
    renderMatrix.scale3d(bounds().width(), bounds().height(), 1);
    toGLMatrix(&glMatrix[0], layerRenderer()->projectionMatrix() * renderMatrix);
    GraphicsContext3D* context = layerRenderer()->context();
    GLC(context, context->uniformMatrix4fv(program->vertexShader().matrixLocation(), false, &glMatrix[0], 1));

    GLC(context, context->uniform4f(program->fragmentShader().colorLocation(), debugBorderColor().red() / 255.0, debugBorderColor().green() / 255.0, debugBorderColor().blue() / 255.0, 1));

    GLC(context, context->lineWidth(debugBorderWidth()));

    // The indices for the line are stored in the same array as the triangle indices.
    GLC(context, context->drawElements(GraphicsContext3D::LINE_LOOP, 4, GraphicsContext3D::UNSIGNED_SHORT, 6 * sizeof(unsigned short)));
}

void CCLayerImpl::writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

void CCLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "bounds: " << bounds().width() << ", " << bounds().height() << "\n";

    if (m_targetRenderSurface) {
        writeIndent(ts, indent);
        ts << "targetRenderSurface: " << m_targetRenderSurface->name() << "\n";
    }

    writeIndent(ts, indent);
    ts << "drawTransform: ";
    ts << m_drawTransform.m11() << ", " << m_drawTransform.m12() << ", " << m_drawTransform.m13() << ", " << m_drawTransform.m14() << ", ";
    ts << m_drawTransform.m21() << ", " << m_drawTransform.m22() << ", " << m_drawTransform.m23() << ", " << m_drawTransform.m24() << ", ";
    ts << m_drawTransform.m31() << ", " << m_drawTransform.m32() << ", " << m_drawTransform.m33() << ", " << m_drawTransform.m34() << ", ";
    ts << m_drawTransform.m41() << ", " << m_drawTransform.m42() << ", " << m_drawTransform.m43() << ", " << m_drawTransform.m44() << "\n";
}

}

#endif // USE(ACCELERATED_COMPOSITING)
