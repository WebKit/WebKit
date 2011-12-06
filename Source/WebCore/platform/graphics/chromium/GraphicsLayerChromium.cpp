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

#include "Canvas2DLayerChromium.h"
#include "ContentLayerChromium.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "Image.h"
#include "ImageLayerChromium.h"
#include "LayerChromium.h"
#include "PlatformString.h"
#include "SystemTime.h"

#include <wtf/CurrentTime.h>
#include <wtf/StringExtras.h>
#include <wtf/text/CString.h>

using namespace std;

namespace WebCore {

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return adoptPtr(new GraphicsLayerChromium(client));
}

GraphicsLayerChromium::GraphicsLayerChromium(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_contentsLayerPurpose(NoContentsLayer)
    , m_contentsLayerHasBackgroundColor(false)
    , m_inSetChildren(false)
{
    m_layer = ContentLayerChromium::create(this);

    updateDebugIndicators();
}

GraphicsLayerChromium::~GraphicsLayerChromium()
{
    if (m_layer) {
        m_layer->setDelegate(0);
        m_layer->clearRenderSurface();
    }
    if (m_contentsLayer) {
        m_contentsLayer->setDelegate(0);
        m_contentsLayer->clearRenderSurface();
    }
    if (m_transformLayer) {
        m_transformLayer->setDelegate(0);
        m_transformLayer->clearRenderSurface();
    }
}

void GraphicsLayerChromium::setName(const String& inName)
{
    m_nameBase = inName;
    String name = String::format("GraphicsLayerChromium(%p) GraphicsLayer(%p) ", m_layer.get(), this) + inName;
    GraphicsLayer::setName(name);
    updateNames();
}

void GraphicsLayerChromium::updateNames()
{
    if (m_layer)
        m_layer->setName("Layer for " + m_nameBase);
    if (m_transformLayer)
        m_transformLayer->setName("TransformLayer for " + m_nameBase);
    if (m_contentsLayer)
        m_contentsLayer->setName("ContentsLayer for " + m_nameBase);
}

bool GraphicsLayerChromium::setChildren(const Vector<GraphicsLayer*>& children)
{
    m_inSetChildren = true;
    bool childrenChanged = GraphicsLayer::setChildren(children);

    if (childrenChanged)
        updateChildList();
    m_inSetChildren = false;

    return childrenChanged;
}

void GraphicsLayerChromium::addChild(GraphicsLayer* childLayer)
{
    GraphicsLayer::addChild(childLayer);
    if (!m_inSetChildren) 
        updateChildList();
}

void GraphicsLayerChromium::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    GraphicsLayer::addChildAtIndex(childLayer, index);
    updateChildList();
}

void GraphicsLayerChromium::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(childLayer, sibling);
    updateChildList();
}

void GraphicsLayerChromium::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer *sibling)
{
    GraphicsLayer::addChildAbove(childLayer, sibling);
    updateChildList();
}

bool GraphicsLayerChromium::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
        updateChildList();
        return true;
    }
    return false;
}

void GraphicsLayerChromium::removeFromParent()
{
    GraphicsLayer::removeFromParent();
    layerForParent()->removeFromParent();
}

void GraphicsLayerChromium::setPosition(const FloatPoint& point)
{
    GraphicsLayer::setPosition(point);
    updateLayerPosition();
}

void GraphicsLayerChromium::setAnchorPoint(const FloatPoint3D& point)
{
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
    // Call this method first to assign contents scale to LayerChromium so the painter can apply the scale transform.
    updateContentsScale();

    GraphicsLayer::setTransform(transform);
    updateTransform();
}

void GraphicsLayerChromium::setChildrenTransform(const TransformationMatrix& transform)
{
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
    GraphicsLayer::setBackgroundColor(color);

    m_contentsLayerHasBackgroundColor = true;
    updateLayerBackgroundColor();
}

void GraphicsLayerChromium::clearBackgroundColor()
{
    GraphicsLayer::clearBackgroundColor();
    m_contentsLayer->setBackgroundColor(static_cast<RGBA32>(0));
}

void GraphicsLayerChromium::setContentsOpaque(bool opaque)
{
    GraphicsLayer::setContentsOpaque(opaque);
    updateContentsOpaque();
}

void GraphicsLayerChromium::setMaskLayer(GraphicsLayer* maskLayer)
{
    if (maskLayer == m_maskLayer)
        return;

    GraphicsLayer::setMaskLayer(maskLayer);

    LayerChromium* maskLayerChromium = m_maskLayer ? m_maskLayer->platformLayer() : 0;
    if (maskLayerChromium)
        maskLayerChromium->setIsMask(true);
    m_layer->setMaskLayer(maskLayerChromium);
}

void GraphicsLayerChromium::setBackfaceVisibility(bool visible)
{
    GraphicsLayer::setBackfaceVisibility(visible);
    updateBackfaceVisibility();
}

void GraphicsLayerChromium::setOpacity(float opacity)
{
    float clampedOpacity = max(min(opacity, 1.0f), 0.0f);
    GraphicsLayer::setOpacity(clampedOpacity);
    primaryLayer()->setOpacity(opacity);
}

void GraphicsLayerChromium::setReplicatedByLayer(GraphicsLayer* layer)
{
    GraphicsLayerChromium* layerChromium = static_cast<GraphicsLayerChromium*>(layer);
    GraphicsLayer::setReplicatedByLayer(layer);
    LayerChromium* replicaLayer = layerChromium ? layerChromium->primaryLayer() : 0;
    primaryLayer()->setReplicaLayer(replicaLayer);
}


void GraphicsLayerChromium::setContentsNeedsDisplay()
{
    if (m_contentsLayer) {
        m_contentsLayer->setNeedsDisplay();
        m_contentsLayer->contentChanged();
    }
}

void GraphicsLayerChromium::setNeedsDisplay()
{
    if (drawsContent())
        m_layer->setNeedsDisplay();
}

void GraphicsLayerChromium::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (drawsContent())
        m_layer->setNeedsDisplayRect(rect);
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
    bool childrenChanged = false;
    if (image) {
        if (!m_contentsLayer.get() || m_contentsLayerPurpose != ContentsLayerForImage) {
            RefPtr<ImageLayerChromium> imageLayer = ImageLayerChromium::create(this);
            setupContentsLayer(imageLayer.get());
            m_contentsLayer = imageLayer;
            m_contentsLayerPurpose = ContentsLayerForImage;
            childrenChanged = true;
        }
        ImageLayerChromium* imageLayer = static_cast<ImageLayerChromium*>(m_contentsLayer.get());
        imageLayer->setContents(image);
        imageLayer->setOpaque(image->isBitmapImage() && !image->currentFrameHasAlpha());
        updateContentsRect();
    } else {
        if (m_contentsLayer) {
            childrenChanged = true;

            // The old contents layer will be removed via updateChildList.
            m_contentsLayer = 0;
        }
    }

    if (childrenChanged)
        updateChildList();
}

void GraphicsLayerChromium::setContentsToCanvas(PlatformLayer* platformLayer)
{
    bool childrenChanged = false;
    if (platformLayer) {
        platformLayer->setDelegate(this);
        if (m_contentsLayer.get() != platformLayer) {
            setupContentsLayer(platformLayer);
            m_contentsLayer = platformLayer;
            m_contentsLayerPurpose = ContentsLayerForCanvas;
            childrenChanged = true;
        }
        m_contentsLayer->setNeedsDisplay();
        updateContentsRect();
    } else {
        if (m_contentsLayer) {
            childrenChanged = true;

            // The old contents layer will be removed via updateChildList.
            m_contentsLayer = 0;
        }
    }

    if (childrenChanged)
        updateChildList();
}

void GraphicsLayerChromium::setContentsToMedia(PlatformLayer* layer)
{
    bool childrenChanged = false;
    if (layer) {
        if (!m_contentsLayer.get() || m_contentsLayerPurpose != ContentsLayerForVideo) {
            setupContentsLayer(layer);
            m_contentsLayer = layer;
            m_contentsLayerPurpose = ContentsLayerForVideo;
            childrenChanged = true;
        }
        layer->setDelegate(this);
        layer->setNeedsDisplay();
        updateContentsRect();
    } else {
        if (m_contentsLayer) {
            childrenChanged = true;
  
            // The old contents layer will be removed via updateChildList.
            m_contentsLayer = 0;
        }
    }
  
    if (childrenChanged)
        updateChildList();
}

PlatformLayer* GraphicsLayerChromium::hostLayerForChildren() const
{
    return m_transformLayer ? m_transformLayer.get() : m_layer.get();
}

PlatformLayer* GraphicsLayerChromium::layerForParent() const
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
        m_layer->setBackgroundColor(color);
    else
        m_layer->setBackgroundColor(static_cast<RGBA32>(0));
}

void GraphicsLayerChromium::setDebugBorder(const Color& color, float borderWidth)
{
    if (color.isValid()) {
        m_layer->setDebugBorderColor(color);
        m_layer->setDebugBorderWidth(borderWidth);
    } else {
        m_layer->setDebugBorderColor(static_cast<RGBA32>(0));
        m_layer->setDebugBorderWidth(0);
    }
}

void GraphicsLayerChromium::updateChildList()
{
    Vector<RefPtr<LayerChromium> > newChildren;

    if (m_transformLayer) {
        // Add the primary layer first. Even if we have negative z-order children, the primary layer always comes behind.
        newChildren.append(m_layer.get());
    } else if (m_contentsLayer) {
        // FIXME: add the contents layer in the correct order with negative z-order children.
        // This does not cause visible rendering issues because currently contents layers are only used
        // for replaced elements that don't have children.
        newChildren.append(m_contentsLayer.get());
    }

    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayerChromium* curChild = static_cast<GraphicsLayerChromium*>(childLayers[i]);

        LayerChromium* childLayer = curChild->layerForParent();
        newChildren.append(childLayer);
    }

    for (size_t i = 0; i < newChildren.size(); ++i)
        newChildren[i]->removeFromParent();

    if (m_transformLayer) {
        m_transformLayer->setChildren(newChildren);

        if (m_contentsLayer) {
            // If we have a transform layer, then the contents layer is parented in the
            // primary layer (which is itself a child of the transform layer).
            m_layer->removeAllChildren();
            m_layer->addChild(m_contentsLayer);
        }
    } else
        m_layer->setChildren(newChildren);
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
    if (m_preserves3D && !m_transformLayer) {
        // Create the transform layer.
        m_transformLayer = LayerChromium::create(this);
        m_transformLayer->setPreserves3D(true);

        // Copy the position from this layer.
        updateLayerPosition();
        updateLayerSize();
        updateAnchorPoint();
        updateTransform();
        updateChildrenTransform();

        m_layer->setPosition(FloatPoint(m_size.width() / 2.0f, m_size.height() / 2.0f));

        m_layer->setAnchorPoint(FloatPoint(0.5f, 0.5f));
        TransformationMatrix identity;
        m_layer->setTransform(identity);
        
        // Set the old layer to opacity of 1. Further down we will set the opacity on the transform layer.
        m_layer->setOpacity(1);

        m_layer->setContentsScale(contentsScale());

        // Move this layer to be a child of the transform layer.
        if (m_layer->parent())
            m_layer->parent()->replaceChild(m_layer.get(), m_transformLayer.get());
        m_transformLayer->addChild(m_layer.get());

        updateChildList();
    } else if (!m_preserves3D && m_transformLayer) {
        // Relace the transformLayer in the parent with this layer.
        m_layer->removeFromParent();
        if (m_transformLayer->parent())
            m_transformLayer->parent()->replaceChild(m_transformLayer.get(), m_layer.get());

        // Release the transform layer.
        m_transformLayer = 0;

        updateLayerPosition();
        updateLayerSize();
        updateAnchorPoint();
        updateTransform();
        updateChildrenTransform();

        updateChildList();
    }

    m_layer->setPreserves3D(m_preserves3D);
    updateOpacityOnLayer();
    updateNames();
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
        m_contentsLayer->setBackgroundColor(m_backgroundColor);
    else
        m_contentsLayer->setBackgroundColor(static_cast<RGBA32>(0));
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

void GraphicsLayerChromium::updateContentsScale()
{
    // If page scale is already applied then there's no need to apply it again.
    if (appliesPageScale() || !m_layer)
        return;

    m_layer->setContentsScale(contentsScale());
}

void GraphicsLayerChromium::setupContentsLayer(LayerChromium* contentsLayer)
{
    if (contentsLayer == m_contentsLayer)
        return;

    if (m_contentsLayer) {
        m_contentsLayer->removeFromParent();
        m_contentsLayer = 0;
    }

    if (contentsLayer) {
        m_contentsLayer = contentsLayer;

        m_contentsLayer->setAnchorPoint(FloatPoint(0, 0));

        // Insert the content layer first. Video elements require this, because they have
        // shadow content that must display in front of the video.
        m_layer->insertChild(m_contentsLayer.get(), 0);

        updateContentsRect();

        if (showDebugBorders()) {
            m_contentsLayer->setDebugBorderColor(Color(0, 0, 128, 180));
            m_contentsLayer->setDebugBorderWidth(1);
        }
    }
    updateDebugIndicators();
    updateNames();
}

float GraphicsLayerChromium::contentsScale() const
{
    if (!appliesPageScale())
        return pageScaleFactor() * deviceScaleFactor();
    return 1;
}

// This function simply mimics the operation of GraphicsLayerCA
void GraphicsLayerChromium::updateOpacityOnLayer()
{
    primaryLayer()->setOpacity(m_opacity);
}

void GraphicsLayerChromium::deviceOrPageScaleFactorChanged()
{
    updateContentsScale();
    if (m_layer)
        m_layer->pageScaleChanged();
}

bool GraphicsLayerChromium::drawsContent() const
{
    return GraphicsLayer::drawsContent();
}

void GraphicsLayerChromium::paintContents(GraphicsContext& context, const IntRect& clip)
{
    paintGraphicsLayerContents(context, clip);
}

void GraphicsLayerChromium::notifySyncRequired()
{
    if (m_client)
        m_client->notifySyncRequired(this);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
