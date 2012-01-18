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
#include "LayerChromium.h"
#include "LayerRendererChromium.h"
#include "cc/CCDebugBorderDrawQuad.h"
#include "cc/CCLayerSorter.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

CCLayerImpl::CCLayerImpl(int id)
    : m_parent(0)
    , m_layerId(id)
    , m_anchorPoint(0.5, 0.5)
    , m_anchorPointZ(0)
    , m_scrollable(false)
    , m_doubleSided(true)
    , m_layerPropertyChanged(false)
    , m_masksToBounds(false)
    , m_opaque(false)
    , m_opacity(1.0)
    , m_preserves3D(false)
    , m_usesLayerClipping(false)
    , m_isNonCompositedContent(false)
    , m_drawsContent(false)
    , m_pageScaleDelta(1)
    , m_targetRenderSurface(0)
    , m_drawDepth(0)
    , m_drawOpacity(0)
    , m_debugBorderColor(0, 0, 0, 0)
    , m_debugBorderWidth(0)
{
    ASSERT(CCProxy::isImplThread());
}

CCLayerImpl::~CCLayerImpl()
{
    ASSERT(CCProxy::isImplThread());
}

void CCLayerImpl::addChild(PassRefPtr<CCLayerImpl> child)
{
    child->setParent(this);
    m_children.append(child);
}

void CCLayerImpl::removeFromParent()
{
    if (!m_parent)
        return;
    for (size_t i = 0; i < m_parent->m_children.size(); ++i) {
        if (m_parent->m_children[i].get() == this) {
            m_parent->m_children.remove(i);
            break;
        }
    }
    m_parent = 0;
}

void CCLayerImpl::removeAllChildren()
{
    while (m_children.size())
        m_children[0]->removeFromParent();
}

void CCLayerImpl::clearChildList()
{
    m_children.clear();
}

void CCLayerImpl::createRenderSurface()
{
    ASSERT(!m_renderSurface);
    m_renderSurface = adoptPtr(new CCRenderSurface(this));
}

bool CCLayerImpl::descendantDrawsContent()
{
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i]->drawsContent() || m_children[i]->descendantDrawsContent())
            return true;
    }
    return false;
}

void CCLayerImpl::draw(LayerRendererChromium*)
{
    ASSERT_NOT_REACHED();
}

PassOwnPtr<CCSharedQuadState> CCLayerImpl::createSharedQuadState() const
{
    IntRect layerClipRect;
    if (usesLayerClipping())
        layerClipRect = clipRect();
    return CCSharedQuadState::create(quadTransform(), drawTransform(), visibleLayerRect(), layerClipRect, drawOpacity(), opaque());
}

void CCLayerImpl::appendQuads(CCQuadList& quadList, const CCSharedQuadState* sharedQuadState)
{
}

void CCLayerImpl::appendDebugBorderQuad(CCQuadList& quadList, const CCSharedQuadState* sharedQuadState) const
{
    if (!hasDebugBorders())
        return;

    IntRect layerRect(IntPoint(), bounds());
    quadList.append(CCDebugBorderDrawQuad::create(sharedQuadState, layerRect, debugBorderColor(), debugBorderWidth()));
}

void CCLayerImpl::bindContentsTexture(LayerRendererChromium*)
{
    ASSERT_NOT_REACHED();
}

void CCLayerImpl::scrollBy(const IntSize& scroll)
{
    IntSize newDelta = m_scrollDelta + scroll;
    IntSize minDelta = -toSize(m_scrollPosition);
    IntSize maxDelta = m_maxScrollPosition - toSize(m_scrollPosition);
    // Clamp newDelta so that position + delta stays within scroll bounds.
    m_scrollDelta = newDelta.expandedTo(minDelta).shrunkTo(maxDelta);
    noteLayerPropertyChangedForSubtree();
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

TransformationMatrix CCLayerImpl::quadTransform() const
{
    TransformationMatrix quadTransformation = drawTransform();

    float offsetX = -0.5 * bounds().width();
    float offsetY = -0.5 * bounds().height();
    quadTransformation.translate(offsetX, offsetY);

    return quadTransformation;
}

void CCLayerImpl::writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

void CCLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "layer ID: " << m_layerId << "\n";

    writeIndent(ts, indent);
    ts << "bounds: " << bounds().width() << ", " << bounds().height() << "\n";

    if (m_targetRenderSurface) {
        writeIndent(ts, indent);
        ts << "targetRenderSurface: " << m_targetRenderSurface->name() << "\n";
    }

    writeIndent(ts, indent);
    ts << "drawTransform: ";
    ts << m_drawTransform.m11() << ", " << m_drawTransform.m12() << ", " << m_drawTransform.m13() << ", " << m_drawTransform.m14() << "  //  ";
    ts << m_drawTransform.m21() << ", " << m_drawTransform.m22() << ", " << m_drawTransform.m23() << ", " << m_drawTransform.m24() << "  //  ";
    ts << m_drawTransform.m31() << ", " << m_drawTransform.m32() << ", " << m_drawTransform.m33() << ", " << m_drawTransform.m34() << "  //  ";
    ts << m_drawTransform.m41() << ", " << m_drawTransform.m42() << ", " << m_drawTransform.m43() << ", " << m_drawTransform.m44() << "\n";

    writeIndent(ts, indent);
    ts << "drawsContent: " << (m_drawsContent ? "yes" : "no") << "\n";
}

void sortLayers(Vector<RefPtr<CCLayerImpl> >::iterator first, Vector<RefPtr<CCLayerImpl> >::iterator end, CCLayerSorter* layerSorter)
{
    TRACE_EVENT("LayerRendererChromium::sortLayers", 0, 0);
    layerSorter->sort(first, end);
}

String CCLayerImpl::layerTreeAsText() const
{
    TextStream ts;
    dumpLayer(ts, 0);
    return ts.release();
}

void CCLayerImpl::dumpLayer(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << layerTypeAsString() << "(" << m_name << ")\n";
    dumpLayerProperties(ts, indent+2);
    if (m_replicaLayer) {
        writeIndent(ts, indent+2);
        ts << "Replica:\n";
        m_replicaLayer->dumpLayer(ts, indent+3);
    }
    if (m_maskLayer) {
        writeIndent(ts, indent+2);
        ts << "Mask:\n";
        m_maskLayer->dumpLayer(ts, indent+3);
    }
    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->dumpLayer(ts, indent+1);
}

void CCLayerImpl::noteLayerPropertyChangedForSubtree()
{
    m_layerPropertyChanged = true;
    noteLayerPropertyChangedForDescendants();
}

void CCLayerImpl::noteLayerPropertyChangedForDescendants()
{
    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::resetAllChangeTrackingForSubtree()
{
    m_layerPropertyChanged = false;
    m_updateRect = FloatRect();

    if (m_renderSurface)
        m_renderSurface->resetPropertyChangedFlag();

    if (m_maskLayer)
        m_maskLayer->resetAllChangeTrackingForSubtree();

    if (m_replicaLayer)
        m_replicaLayer->resetAllChangeTrackingForSubtree(); // also resets the replica mask, if it exists.

    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->resetAllChangeTrackingForSubtree();
}

void CCLayerImpl::setBounds(const IntSize& bounds)
{
    if (m_bounds == bounds)
        return;

    m_bounds = bounds;

    if (masksToBounds())
        noteLayerPropertyChangedForSubtree();
    else
        m_layerPropertyChanged = true;
}

void CCLayerImpl::setMaskLayer(PassRefPtr<CCLayerImpl> maskLayer)
{
    if (m_maskLayer == maskLayer)
        return;

    m_maskLayer = maskLayer;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setReplicaLayer(PassRefPtr<CCLayerImpl> replicaLayer)
{
    if (m_replicaLayer == replicaLayer)
        return;

    m_replicaLayer = replicaLayer;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setDrawsContent(bool drawsContent)
{
    if (m_drawsContent == drawsContent)
        return;

    m_drawsContent = drawsContent;
    m_layerPropertyChanged = true;
}

void CCLayerImpl::setAnchorPoint(const FloatPoint& anchorPoint)
{
    if (m_anchorPoint == anchorPoint)
        return;

    m_anchorPoint = anchorPoint;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setAnchorPointZ(float anchorPointZ)
{
    if (m_anchorPointZ == anchorPointZ)
        return;

    m_anchorPointZ = anchorPointZ;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setBackgroundColor(const Color& backgroundColor)
{
    if (m_backgroundColor == backgroundColor)
        return;

    m_backgroundColor = backgroundColor;
    m_layerPropertyChanged = true;
}

void CCLayerImpl::setMasksToBounds(bool masksToBounds)
{
    if (m_masksToBounds == masksToBounds)
        return;

    m_masksToBounds = masksToBounds;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setOpaque(bool opaque)
{
    if (m_opaque == opaque)
        return;

    m_opaque = opaque;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setOpacity(float opacity)
{
    if (m_opacity == opacity)
        return;

    m_opacity = opacity;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setPosition(const FloatPoint& position)
{
    if (m_position == position)
        return;

    m_position = position;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setPreserves3D(bool preserves3D)
{
    if (m_preserves3D == preserves3D)
        return;

    m_preserves3D = preserves3D;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setSublayerTransform(const TransformationMatrix& sublayerTransform)
{
    if (m_sublayerTransform == sublayerTransform)
        return;

    m_sublayerTransform = sublayerTransform;
    // sublayer transform does not affect the current layer; it affects only its children.
    noteLayerPropertyChangedForDescendants();
}

void CCLayerImpl::setTransform(const TransformationMatrix& transform)
{
    if (m_transform == transform)
        return;

    m_transform = transform;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setDebugBorderColor(Color debugBorderColor)
{
    if (m_debugBorderColor == debugBorderColor)
        return;

    m_debugBorderColor = debugBorderColor;
    m_layerPropertyChanged = true;
}

void CCLayerImpl::setDebugBorderWidth(float debugBorderWidth)
{
    if (m_debugBorderWidth == debugBorderWidth)
        return;

    m_debugBorderWidth = debugBorderWidth;
    m_layerPropertyChanged = true;
}

bool CCLayerImpl::hasDebugBorders() const
{
    return debugBorderColor().alpha() && debugBorderWidth() > 0;
}

void CCLayerImpl::setContentBounds(const IntSize& contentBounds)
{
    if (m_contentBounds == contentBounds)
        return;

    m_contentBounds = contentBounds;
    m_layerPropertyChanged = true;
}

void CCLayerImpl::setScrollPosition(const IntPoint& scrollPosition)
{
    if (m_scrollPosition == scrollPosition)
        return;

    m_scrollPosition = scrollPosition;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setScrollDelta(const IntSize& scrollDelta)
{
    if (m_scrollDelta == scrollDelta)
        return;

    m_scrollDelta = scrollDelta;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setPageScaleDelta(float pageScaleDelta)
{
    if (m_pageScaleDelta == pageScaleDelta)
        return;

    m_pageScaleDelta = pageScaleDelta;
    noteLayerPropertyChangedForSubtree();
}

void CCLayerImpl::setDoubleSided(bool doubleSided)
{
    if (m_doubleSided == doubleSided)
        return;

    m_doubleSided = doubleSided;
    noteLayerPropertyChangedForSubtree();
}

}


#endif // USE(ACCELERATED_COMPOSITING)
