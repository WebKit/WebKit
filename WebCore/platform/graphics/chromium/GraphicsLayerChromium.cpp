/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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


/** FIXME
 * This file borrows code heavily from platform/graphics/win/GraphicsLayerCACF.cpp
 * (and hence it includes both copyrights)
 * Ideally the common code (mostly the code that keeps track of the layer hierarchy)
 * should be kept separate and shared between platforms. It would be a well worthwhile
 * effort once the Windows implementation (binaries and headers) of CoreAnimation is
 * checked in to the WebKit repository. Until then only Apple can make this happen.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayerChromium.h"

#include "FloatConversion.h"
#include "FloatRect.h"
#include "Image.h"
#include "LayerChromium.h"
#include "PlatformString.h"
#include "SystemTime.h"
#include <wtf/CurrentTime.h>
#include <wtf/StringExtras.h>
#include <wtf/text/CString.h>

using namespace std;

namespace WebCore {

static void setLayerBorderColor(LayerChromium& layer, const Color& color)
{
    layer.setBorderColor(color);
}

static void clearBorderColor(LayerChromium& layer)
{
    layer.setBorderColor(0);
}

static void setLayerBackgroundColor(LayerChromium& layer, const Color& color)
{
    layer.setBackgroundColor(color);
}

static void clearLayerBackgroundColor(LayerChromium& layer)
{
    layer.setBackgroundColor(0);
}

GraphicsLayer::CompositingCoordinatesOrientation GraphicsLayer::compositingCoordinatesOrientation()
{
    return CompositingCoordinatesBottomUp;
}

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return new GraphicsLayerChromium(client);
}

GraphicsLayerChromium::GraphicsLayerChromium(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_contentsLayerPurpose(NoContentsLayer)
    , m_contentsLayerHasBackgroundColor(false)
{
    m_layer = LayerChromium::create(LayerChromium::Layer, this);

    updateDebugIndicators();
}

GraphicsLayerChromium::~GraphicsLayerChromium()
{
    // Clean up the Skia layer.
    if (m_layer)
        m_layer->removeFromSuperlayer();

    if (m_transformLayer)
        m_transformLayer->removeFromSuperlayer();
}

void GraphicsLayerChromium::setName(const String& inName)
{
    String name = String::format("GraphicsLayerChromium(%p) GraphicsLayer(%p) ", m_layer.get(), this) + inName;
    GraphicsLayer::setName(name);
}

NativeLayer GraphicsLayerChromium::nativeLayer() const
{
    return m_layer.get();
}

bool GraphicsLayerChromium::setChildren(const Vector<GraphicsLayer*>& children)
{
    bool childrenChanged = GraphicsLayer::setChildren(children);
    // FIXME: GraphicsLayer::setChildren calls addChild() for each sublayer, which
    // will end up calling updateSublayerList() N times.
    if (childrenChanged)
        updateSublayerList();

    return childrenChanged;
}

void GraphicsLayerChromium::addChild(GraphicsLayer* childLayer)
{
    GraphicsLayer::addChild(childLayer);
    updateSublayerList();
}

void GraphicsLayerChromium::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    GraphicsLayer::addChildAtIndex(childLayer, index);
    updateSublayerList();
}

void GraphicsLayerChromium::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(childLayer, sibling);
    updateSublayerList();
}

void GraphicsLayerChromium::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer *sibling)
{
    GraphicsLayer::addChildAbove(childLayer, sibling);
    updateSublayerList();
}

bool GraphicsLayerChromium::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
        updateSublayerList();
        return true;
    }
    return false;
}

void GraphicsLayerChromium::removeFromParent()
{
    GraphicsLayer::removeFromParent();
    layerForSuperlayer()->removeFromSuperlayer();
}

void GraphicsLayerChromium::setPosition(const FloatPoint& point)
{
    GraphicsLayer::setPosition(point);
    updateLayerPosition();
}

void GraphicsLayerChromium::setAnchorPoint(const FloatPoint3D& point)
{
    if (point == m_anchorPoint)
        return;

    GraphicsLayer::setAnchorPoint(point);
    updateAnchorPoint();
}

void GraphicsLayerChromium::setSize(const FloatSize& size)
{
    if (size == m_size)
        return;

    GraphicsLayer::setSize(size);
    updateLayerSize();
}

void GraphicsLayerChromium::setTransform(const TransformationMatrix& transform)
{
    if (transform == m_transform)
        return;

    GraphicsLayer::setTransform(transform);
    updateTransform();
}

void GraphicsLayerChromium::setChildrenTransform(const TransformationMatrix& transform)
{
    if (transform == m_childrenTransform)
        return;

    GraphicsLayer::setChildrenTransform(transform);
    updateChildrenTransform();
}

void GraphicsLayerChromium::setPreserves3D(bool preserves3D)
{
    if (preserves3D == m_preserves3D)
        return;

    GraphicsLayer::setPreserves3D(preserves3D);
    updateLayerPreserves3D();
}

void GraphicsLayerChromium::setMasksToBounds(bool masksToBounds)
{
    if (masksToBounds == m_masksToBounds)
        return;

    GraphicsLayer::setMasksToBounds(masksToBounds);
    updateMasksToBounds();
}

void GraphicsLayerChromium::setDrawsContent(bool drawsContent)
{
    if (drawsContent == m_drawsContent)
        return;

    GraphicsLayer::setDrawsContent(drawsContent);
    updateLayerDrawsContent();
}

void GraphicsLayerChromium::setBackgroundColor(const Color& color)
{
    if (m_backgroundColorSet && m_backgroundColor == color)
        return;

    GraphicsLayer::setBackgroundColor(color);

    m_contentsLayerHasBackgroundColor = true;
    updateLayerBackgroundColor();
}

void GraphicsLayerChromium::clearBackgroundColor()
{
    if (!m_backgroundColorSet)
        return;

    GraphicsLayer::clearBackgroundColor();
    clearLayerBackgroundColor(*m_contentsLayer);
}

void GraphicsLayerChromium::setContentsOpaque(bool opaque)
{
    if (m_contentsOpaque == opaque)
        return;

    GraphicsLayer::setContentsOpaque(opaque);
    updateContentsOpaque();
}

void GraphicsLayerChromium::setBackfaceVisibility(bool visible)
{
    if (m_backfaceVisibility == visible)
        return;

    GraphicsLayer::setBackfaceVisibility(visible);
    updateBackfaceVisibility();
}

void GraphicsLayerChromium::setOpacity(float opacity)
{
    float clampedOpacity = max(min(opacity, 1.0f), 0.0f);

    if (m_opacity == clampedOpacity)
        return;

    GraphicsLayer::setOpacity(clampedOpacity);
    primaryLayer()->setOpacity(opacity);
}

void GraphicsLayerChromium::setNeedsDisplay()
{
    if (drawsContent())
        m_layer->setNeedsDisplay();
}

void GraphicsLayerChromium::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (drawsContent())
        m_layer->setNeedsDisplay(rect);
}

void GraphicsLayerChromium::setContentsRect(const IntRect& rect)
{
    if (rect == m_contentsRect)
        return;

    GraphicsLayer::setContentsRect(rect);
    updateContentsRect();
}

void GraphicsLayerChromium::setContentsToImage(Image* image)
{
    // FIXME: Implement
}

void GraphicsLayerChromium::setContentsToVideo(PlatformLayer* videoLayer)
{
    // FIXME: Implement
}

void GraphicsLayerChromium::setGeometryOrientation(CompositingCoordinatesOrientation orientation)
{
    if (orientation == m_geometryOrientation)
        return;

    GraphicsLayer::setGeometryOrientation(orientation);
    updateGeometryOrientation();
}

PlatformLayer* GraphicsLayerChromium::hostLayerForSublayers() const
{
    return m_transformLayer ? m_transformLayer.get() : m_layer.get();
}

PlatformLayer* GraphicsLayerChromium::layerForSuperlayer() const
{
    return m_transformLayer ? m_transformLayer.get() : m_layer.get();
}

PlatformLayer* GraphicsLayerChromium::platformLayer() const
{
    return primaryLayer();
}

void GraphicsLayerChromium::setDebugBackgroundColor(const Color& color)
{
    if (color.isValid())
        setLayerBackgroundColor(*m_layer, color);
    else
        clearLayerBackgroundColor(*m_layer);
}

void GraphicsLayerChromium::setDebugBorder(const Color& color, float borderWidth)
{
    if (color.isValid()) {
        setLayerBorderColor(*m_layer, color);
        m_layer->setBorderWidth(borderWidth);
    } else {
        clearBorderColor(*m_layer);
        m_layer->setBorderWidth(0);
    }
}

void GraphicsLayerChromium::updateSublayerList()
{
    Vector<RefPtr<LayerChromium> > newSublayers;

    if (m_transformLayer) {
        // Add the primary layer first. Even if we have negative z-order children, the primary layer always comes behind.
        newSublayers.append(m_layer.get());
    } else if (m_contentsLayer) {
        // FIXME: add the contents layer in the correct order with negative z-order children.
        // This does not cause visible rendering issues because currently contents layers are only used
        // for replaced elements that don't have children.
        newSublayers.append(m_contentsLayer.get());
    }

    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayerChromium* curChild = static_cast<GraphicsLayerChromium*>(childLayers[i]);

        LayerChromium* childLayer = curChild->layerForSuperlayer();
        newSublayers.append(childLayer);
    }

    for (size_t i = 0; i < newSublayers.size(); ++i)
        newSublayers[i]->removeFromSuperlayer();

    if (m_transformLayer) {
        m_transformLayer->setSublayers(newSublayers);

        if (m_contentsLayer) {
            // If we have a transform layer, then the contents layer is parented in the
            // primary layer (which is itself a child of the transform layer).
            m_layer->removeAllSublayers();
            m_layer->addSublayer(m_contentsLayer);
        }
    } else
        m_layer->setSublayers(newSublayers);
}

void GraphicsLayerChromium::updateLayerPosition()
{
    // Position is offset on the layer by the layer anchor point.
    FloatPoint layerPosition(m_position.x() + m_anchorPoint.x() * m_size.width(),
                             m_position.y() + m_anchorPoint.y() * m_size.height());

    primaryLayer()->setPosition(layerPosition);
}

void GraphicsLayerChromium::updateLayerSize()
{
    IntSize layerSize(m_size.width(), m_size.height());
    if (m_transformLayer) {
        m_transformLayer->setBounds(layerSize);
        // The anchor of the contents layer is always at 0.5, 0.5, so the position is center-relative.
        FloatPoint centerPoint(m_size.width() / 2, m_size.height() / 2);
        m_layer->setPosition(centerPoint);
    }

    m_layer->setBounds(layerSize);

    // Note that we don't resize m_contentsLayer. It's up the caller to do that.

    // If we've changed the bounds, we need to recalculate the position
    // of the layer, taking anchor point into account.
    updateLayerPosition();
}

void GraphicsLayerChromium::updateAnchorPoint()
{
    primaryLayer()->setAnchorPoint(FloatPoint(m_anchorPoint.x(), m_anchorPoint.y()));
    primaryLayer()->setAnchorPointZ(m_anchorPoint.z());
    updateLayerPosition();
}

void GraphicsLayerChromium::updateTransform()
{
    primaryLayer()->setTransform(m_transform);
}

void GraphicsLayerChromium::updateChildrenTransform()
{
    primaryLayer()->setSublayerTransform(m_childrenTransform);
}

void GraphicsLayerChromium::updateMasksToBounds()
{
    m_layer->setMasksToBounds(m_masksToBounds);
    updateDebugIndicators();
}

void GraphicsLayerChromium::updateContentsOpaque()
{
    m_layer->setOpaque(m_contentsOpaque);
}

void GraphicsLayerChromium::updateBackfaceVisibility()
{
    m_layer->setDoubleSided(m_backfaceVisibility);
}

void GraphicsLayerChromium::updateLayerPreserves3D()
{
    // FIXME: implement
}

void GraphicsLayerChromium::updateLayerDrawsContent()
{
    if (m_drawsContent)
        m_layer->setNeedsDisplay();

    updateDebugIndicators();
}

void GraphicsLayerChromium::updateLayerBackgroundColor()
{
    if (!m_contentsLayer)
        return;

    // We never create the contents layer just for background color yet.
    if (m_backgroundColorSet)
        setLayerBackgroundColor(*m_contentsLayer, m_backgroundColor);
    else
        clearLayerBackgroundColor(*m_contentsLayer);
}

void GraphicsLayerChromium::updateContentsImage()
{
    // FIXME: Implement
}

void GraphicsLayerChromium::updateContentsVideo()
{
    // FIXME: Implement
}

void GraphicsLayerChromium::updateContentsRect()
{
    if (!m_contentsLayer)
        return;

    m_contentsLayer->setPosition(FloatPoint(m_contentsRect.x(), m_contentsRect.y()));
    m_contentsLayer->setBounds(IntSize(m_contentsRect.width(), m_contentsRect.height()));
}

void GraphicsLayerChromium::updateGeometryOrientation()
{
    switch (geometryOrientation()) {
    case CompositingCoordinatesTopDown:
        m_layer->setGeometryFlipped(false);
        break;

    case CompositingCoordinatesBottomUp:
        m_layer->setGeometryFlipped(true);
        break;
    }
    // Geometry orientation is mapped onto children transform in older QuartzCores,
    // so is handled via setGeometryOrientation().
}

void GraphicsLayerChromium::setupContentsLayer(LayerChromium* contentsLayer)
{
    if (contentsLayer == m_contentsLayer)
        return;

    if (m_contentsLayer) {
        m_contentsLayer->removeFromSuperlayer();
        m_contentsLayer = 0;
    }

    if (contentsLayer) {
        m_contentsLayer = contentsLayer;

        m_contentsLayer->setAnchorPoint(FloatPoint(0, 0));

        // Insert the content layer first. Video elements require this, because they have
        // shadow content that must display in front of the video.
        m_layer->insertSublayer(m_contentsLayer.get(), 0);

        updateContentsRect();

        if (showDebugBorders()) {
            setLayerBorderColor(*m_contentsLayer, Color(0, 0, 128, 180));
            m_contentsLayer->setBorderWidth(1);
        }
    }
    updateDebugIndicators();
}

// This function simply mimics the operation of GraphicsLayerCA
void GraphicsLayerChromium::updateOpacityOnLayer()
{
    primaryLayer()->setOpacity(m_opacity);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
