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

#include "GraphicsLayerSkia.h"

#include "FloatConversion.h"
#include "FloatRect.h"
#include "Image.h"
#include "LayerSkia.h"
#include "PlatformString.h"
#include "SystemTime.h"
#include <wtf/CurrentTime.h>
#include <wtf/StringExtras.h>
#include <wtf/text/CString.h>

using namespace std;

namespace WebCore {

static void setLayerBorderColor(LayerSkia& layer, const Color& color)
{
    layer.setBorderColor(color);
}

static void clearBorderColor(LayerSkia& layer)
{
    layer.setBorderColor(0);
}

static void setLayerBackgroundColor(LayerSkia& layer, const Color& color)
{
    layer.setBackgroundColor(color);
}

static void clearLayerBackgroundColor(LayerSkia& layer)
{
    layer.setBackgroundColor(0);
}

GraphicsLayer::CompositingCoordinatesOrientation GraphicsLayer::compositingCoordinatesOrientation()
{
    return CompositingCoordinatesBottomUp;
}

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return new GraphicsLayerSkia(client);
}

GraphicsLayerSkia::GraphicsLayerSkia(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_contentsLayerPurpose(NoContentsLayer)
    , m_contentsLayerHasBackgroundColor(false)
{
    m_layer = LayerSkia::create(LayerSkia::Layer, this);

    updateDebugIndicators();
}

GraphicsLayerSkia::~GraphicsLayerSkia()
{
    // Clean up the Skia layer.
    if (m_layer)
        m_layer->removeFromSuperlayer();

    if (m_transformLayer)
        m_transformLayer->removeFromSuperlayer();
}

void GraphicsLayerSkia::setName(const String& inName)
{
    String name = String::format("GraphicsLayerSkia(%p) GraphicsLayer(%p) ", m_layer.get(), this) + inName;
    GraphicsLayer::setName(name);
}

NativeLayer GraphicsLayerSkia::nativeLayer() const
{
    return m_layer.get();
}

bool GraphicsLayerSkia::setChildren(const Vector<GraphicsLayer*>& children)
{
    bool childrenChanged = GraphicsLayer::setChildren(children);
    // FIXME: GraphicsLayer::setChildren calls addChild() for each sublayer, which
    // will end up calling updateSublayerList() N times.
    if (childrenChanged)
        updateSublayerList();

    return childrenChanged;
}

void GraphicsLayerSkia::addChild(GraphicsLayer* childLayer)
{
    GraphicsLayer::addChild(childLayer);
    updateSublayerList();
}

void GraphicsLayerSkia::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    GraphicsLayer::addChildAtIndex(childLayer, index);
    updateSublayerList();
}

void GraphicsLayerSkia::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(childLayer, sibling);
    updateSublayerList();
}

void GraphicsLayerSkia::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer *sibling)
{
    GraphicsLayer::addChildAbove(childLayer, sibling);
    updateSublayerList();
}

bool GraphicsLayerSkia::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
        updateSublayerList();
        return true;
    }
    return false;
}

void GraphicsLayerSkia::removeFromParent()
{
    GraphicsLayer::removeFromParent();
    layerForSuperlayer()->removeFromSuperlayer();
}

void GraphicsLayerSkia::setPosition(const FloatPoint& point)
{
    GraphicsLayer::setPosition(point);
    updateLayerPosition();
}

void GraphicsLayerSkia::setAnchorPoint(const FloatPoint3D& point)
{
    if (point == m_anchorPoint)
        return;

    GraphicsLayer::setAnchorPoint(point);
    updateAnchorPoint();
}

void GraphicsLayerSkia::setSize(const FloatSize& size)
{
    if (size == m_size)
        return;

    GraphicsLayer::setSize(size);
    updateLayerSize();
}

void GraphicsLayerSkia::setTransform(const TransformationMatrix& transform)
{
    if (transform == m_transform)
        return;

    GraphicsLayer::setTransform(transform);
    updateTransform();
}

void GraphicsLayerSkia::setChildrenTransform(const TransformationMatrix& transform)
{
    if (transform == m_childrenTransform)
        return;

    GraphicsLayer::setChildrenTransform(transform);
    updateChildrenTransform();
}

void GraphicsLayerSkia::setPreserves3D(bool preserves3D)
{
    if (preserves3D == m_preserves3D)
        return;

    GraphicsLayer::setPreserves3D(preserves3D);
    updateLayerPreserves3D();
}

void GraphicsLayerSkia::setMasksToBounds(bool masksToBounds)
{
    if (masksToBounds == m_masksToBounds)
        return;

    GraphicsLayer::setMasksToBounds(masksToBounds);
    updateMasksToBounds();
}

void GraphicsLayerSkia::setDrawsContent(bool drawsContent)
{
    if (drawsContent == m_drawsContent)
        return;

    GraphicsLayer::setDrawsContent(drawsContent);
    updateLayerDrawsContent();
}

void GraphicsLayerSkia::setBackgroundColor(const Color& color)
{
    if (m_backgroundColorSet && m_backgroundColor == color)
        return;

    GraphicsLayer::setBackgroundColor(color);

    m_contentsLayerHasBackgroundColor = true;
    updateLayerBackgroundColor();
}

void GraphicsLayerSkia::clearBackgroundColor()
{
    if (!m_backgroundColorSet)
        return;

    GraphicsLayer::clearBackgroundColor();
    clearLayerBackgroundColor(*m_contentsLayer);
}

void GraphicsLayerSkia::setContentsOpaque(bool opaque)
{
    if (m_contentsOpaque == opaque)
        return;

    GraphicsLayer::setContentsOpaque(opaque);
    updateContentsOpaque();
}

void GraphicsLayerSkia::setBackfaceVisibility(bool visible)
{
    if (m_backfaceVisibility == visible)
        return;

    GraphicsLayer::setBackfaceVisibility(visible);
    updateBackfaceVisibility();
}

void GraphicsLayerSkia::setOpacity(float opacity)
{
    float clampedOpacity = max(min(opacity, 1.0f), 0.0f);

    if (m_opacity == clampedOpacity)
        return;

    GraphicsLayer::setOpacity(clampedOpacity);
    primaryLayer()->setOpacity(opacity);
}

void GraphicsLayerSkia::setNeedsDisplay()
{
    if (drawsContent())
        m_layer->setNeedsDisplay();
}

void GraphicsLayerSkia::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (drawsContent())
        m_layer->setNeedsDisplay(rect);
}

void GraphicsLayerSkia::setContentsRect(const IntRect& rect)
{
    if (rect == m_contentsRect)
        return;

    GraphicsLayer::setContentsRect(rect);
    updateContentsRect();
}

void GraphicsLayerSkia::setContentsToImage(Image* image)
{
    // FIXME: Implement
}

void GraphicsLayerSkia::setContentsToVideo(PlatformLayer* videoLayer)
{
    // FIXME: Implement
}

void GraphicsLayerSkia::setGeometryOrientation(CompositingCoordinatesOrientation orientation)
{
    if (orientation == m_geometryOrientation)
        return;

    GraphicsLayer::setGeometryOrientation(orientation);
    updateGeometryOrientation();
}

PlatformLayer* GraphicsLayerSkia::hostLayerForSublayers() const
{
    return m_transformLayer ? m_transformLayer.get() : m_layer.get();
}

PlatformLayer* GraphicsLayerSkia::layerForSuperlayer() const
{
    return m_transformLayer ? m_transformLayer.get() : m_layer.get();
}

PlatformLayer* GraphicsLayerSkia::platformLayer() const
{
    return primaryLayer();
}

void GraphicsLayerSkia::setDebugBackgroundColor(const Color& color)
{
    if (color.isValid())
        setLayerBackgroundColor(*m_layer, color);
    else
        clearLayerBackgroundColor(*m_layer);
}

void GraphicsLayerSkia::setDebugBorder(const Color& color, float borderWidth)
{
    if (color.isValid()) {
        setLayerBorderColor(*m_layer, color);
        m_layer->setBorderWidth(borderWidth);
    } else {
        clearBorderColor(*m_layer);
        m_layer->setBorderWidth(0);
    }
}

void GraphicsLayerSkia::updateSublayerList()
{
    Vector<RefPtr<LayerSkia> > newSublayers;

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
        GraphicsLayerSkia* curChild = static_cast<GraphicsLayerSkia*>(childLayers[i]);

        LayerSkia* childLayer = curChild->layerForSuperlayer();
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

void GraphicsLayerSkia::updateLayerPosition()
{
    // Position is offset on the layer by the layer anchor point.
    SkPoint layerPosition;
    layerPosition.set(m_position.x() + m_anchorPoint.x() * m_size.width(),
                      m_position.y() + m_anchorPoint.y() * m_size.height());

    primaryLayer()->setPosition(layerPosition);
}

void GraphicsLayerSkia::updateLayerSize()
{
    SkIRect rect;
    rect.set(0, 0, m_size.width(), m_size.height());
    if (m_transformLayer) {
        m_transformLayer->setBounds(rect);
        // The anchor of the contents layer is always at 0.5, 0.5, so the position is center-relative.
        SkPoint centerPoint;
        centerPoint.set(m_size.width() / 2.0f, m_size.height() / 2.0f);
        m_layer->setPosition(centerPoint);
    }

    m_layer->setBounds(rect);

    // Note that we don't resize m_contentsLayer. It's up the caller to do that.

    // If we've changed the bounds, we need to recalculate the position
    // of the layer, taking anchor point into account.
    updateLayerPosition();
}

void GraphicsLayerSkia::updateAnchorPoint()
{
    SkPoint anchorPoint;
    anchorPoint.set(m_anchorPoint.x(), m_anchorPoint.y());
    primaryLayer()->setAnchorPoint(anchorPoint);
    primaryLayer()->setAnchorPointZ(m_anchorPoint.z());
    updateLayerPosition();
}

void GraphicsLayerSkia::updateTransform()
{
    primaryLayer()->setTransform(m_transform.toAffineTransform());
}

void GraphicsLayerSkia::updateChildrenTransform()
{
    primaryLayer()->setSublayerTransform(m_childrenTransform);
}

void GraphicsLayerSkia::updateMasksToBounds()
{
    m_layer->setMasksToBounds(m_masksToBounds);
    updateDebugIndicators();
}

void GraphicsLayerSkia::updateContentsOpaque()
{
    m_layer->setOpaque(m_contentsOpaque);
}

void GraphicsLayerSkia::updateBackfaceVisibility()
{
    m_layer->setDoubleSided(m_backfaceVisibility);
}

void GraphicsLayerSkia::updateLayerPreserves3D()
{
    // FIXME: implement
}

void GraphicsLayerSkia::updateLayerDrawsContent()
{
    if (m_drawsContent)
        m_layer->setNeedsDisplay();

    updateDebugIndicators();
}

void GraphicsLayerSkia::updateLayerBackgroundColor()
{
    if (!m_contentsLayer)
        return;

    // We never create the contents layer just for background color yet.
    if (m_backgroundColorSet)
        setLayerBackgroundColor(*m_contentsLayer, m_backgroundColor);
    else
        clearLayerBackgroundColor(*m_contentsLayer);
}

void GraphicsLayerSkia::updateContentsImage()
{
    // FIXME: Implement
}

void GraphicsLayerSkia::updateContentsVideo()
{
    // FIXME: Implement
}

void GraphicsLayerSkia::updateContentsRect()
{
    if (!m_contentsLayer)
        return;

    SkPoint point;
    point.set(m_contentsRect.x(), m_contentsRect.y());
    SkIRect rect;
    rect.set(0, 0, m_contentsRect.width(), m_contentsRect.height());

    m_contentsLayer->setPosition(point);
    m_contentsLayer->setBounds(rect);
}

void GraphicsLayerSkia::updateGeometryOrientation()
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

void GraphicsLayerSkia::setupContentsLayer(LayerSkia* contentsLayer)
{
    if (contentsLayer == m_contentsLayer)
        return;

    if (m_contentsLayer) {
        m_contentsLayer->removeFromSuperlayer();
        m_contentsLayer = 0;
    }

    if (contentsLayer) {
        m_contentsLayer = contentsLayer;

        SkPoint anchorPoint;
        anchorPoint.set(0.0f, 0.0f);
        m_contentsLayer->setAnchorPoint(anchorPoint);

        // Insert the content layer first. Video elements require this, because they have
        // shadow content that must display in front of the video.
        m_layer->insertSublayer(m_contentsLayer.get(), 0);

        updateContentsRect();

        if (showDebugBorders()) {
            setLayerBorderColor(*m_contentsLayer, Color(0, 0, 128, 180));
            m_contentsLayer->setBorderWidth(1.0f);
        }
    }
    updateDebugIndicators();
}

// This function simply mimics the operation of GraphicsLayerCA
void GraphicsLayerSkia::updateOpacityOnLayer()
{
    primaryLayer()->setOpacity(m_opacity);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
