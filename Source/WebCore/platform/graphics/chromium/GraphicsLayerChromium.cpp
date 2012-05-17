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

#include "AnimationIdVendor.h"
#include "Canvas2DLayerChromium.h"
#include "ContentLayerChromium.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "Image.h"
#include "ImageLayerChromium.h"
#include "LayerChromium.h"
#include "LinkHighlight.h"
#include "PlatformString.h"
#include "SkMatrix44.h"
#include "SystemTime.h"

#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebSize.h>
#include <public/WebTransformationMatrix.h>
#include <wtf/CurrentTime.h>
#include <wtf/StringExtras.h>
#include <wtf/text/CString.h>

using namespace std;

using WebKit::WebContentLayer;
using WebKit::WebLayer;

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
    , m_pageScaleChanged(false)
{
    m_layer = WebContentLayer(ContentLayerChromium::create(this));

    updateDebugIndicators();
}

GraphicsLayerChromium::~GraphicsLayerChromium()
{
    // Do cleanup while we can still safely call methods on the derived class.
    willBeDestroyed();
}

void GraphicsLayerChromium::willBeDestroyed()
{
    if (!m_layer.isNull()) {
        m_layer.clearClient();
        m_layer.unwrap<LayerChromium>()->clearRenderSurface();
        m_layer.unwrap<LayerChromium>()->setLayerAnimationDelegate(0);
    }

    if (!m_contentsLayer.isNull()) {
        m_contentsLayer.unwrap<LayerChromium>()->clearRenderSurface();
        m_contentsLayer.unwrap<LayerChromium>()->setLayerAnimationDelegate(0);
    }

    if (!m_transformLayer.isNull()) {
        m_transformLayer.unwrap<LayerChromium>()->clearRenderSurface();
        m_transformLayer.unwrap<LayerChromium>()->setLayerAnimationDelegate(0);
    }

    if (m_linkHighlight)
        m_linkHighlight.clear();

    GraphicsLayer::willBeDestroyed();
}

void GraphicsLayerChromium::setName(const String& inName)
{
    m_nameBase = inName;
    String name = String::format("GraphicsLayerChromium(%p) GraphicsLayer(%p) ", m_layer.unwrap<LayerChromium>(), this) + inName;
    GraphicsLayer::setName(name);
    updateNames();
}

void GraphicsLayerChromium::updateNames()
{
    if (!m_layer.isNull())
        m_layer.unwrap<LayerChromium>()->setDebugName("Layer for " + m_nameBase);
    if (!m_transformLayer.isNull())
        m_transformLayer.unwrap<LayerChromium>()->setDebugName("TransformLayer for " + m_nameBase);
    if (!m_contentsLayer.isNull())
        m_contentsLayer.unwrap<LayerChromium>()->setDebugName("ContentsLayer for " + m_nameBase);
    if (m_linkHighlight)
        m_linkHighlight->contentLayer()->setDebugName("LinkHighlight for " + m_nameBase);
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
    layerForParent().removeFromParent();
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
    // We are receiving negative sizes here that cause assertions to fail in the compositor. Clamp them to 0 to
    // avoid those assertions.
    // FIXME: This should be an ASSERT instead, as negative sizes should not exist in WebCore.
    FloatSize clampedSize = size;
    if (clampedSize.width() < 0 || clampedSize.height() < 0)
        clampedSize = FloatSize();

    if (clampedSize == m_size)
        return;

    GraphicsLayer::setSize(clampedSize);
    updateLayerSize();

    if (m_pageScaleChanged && !m_layer.isNull())
        m_layer.invalidate();
    m_pageScaleChanged = false;
}

void GraphicsLayerChromium::setTransform(const TransformationMatrix& transform)
{
    // Call this method first to assign contents scale to our layer so the painter can apply the scale transform.
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
    // Note carefully this early-exit is only correct because we also properly initialize
    // LayerChromium::m_isDrawable whenever m_contentsLayer is set to a new layer in setupContentsLayer().
    if (drawsContent == m_drawsContent)
        return;

    GraphicsLayer::setDrawsContent(drawsContent);
    updateLayerIsDrawable();
}

void GraphicsLayerChromium::setContentsVisible(bool contentsVisible)
{
    // Note carefully this early-exit is only correct because we also properly initialize
    // LayerChromium::m_isDrawable whenever m_contentsLayer is set to a new layer in setupContentsLayer().
    if (contentsVisible == m_contentsVisible)
        return;

    GraphicsLayer::setContentsVisible(contentsVisible);
    updateLayerIsDrawable();
}

void GraphicsLayerChromium::setBackgroundColor(const Color& color)
{
    GraphicsLayer::setBackgroundColor(color.rgb());

    m_contentsLayerHasBackgroundColor = true;
    updateLayerBackgroundColor();
}

void GraphicsLayerChromium::clearBackgroundColor()
{
    GraphicsLayer::clearBackgroundColor();
    m_contentsLayer.setBackgroundColor(static_cast<RGBA32>(0));
}

void GraphicsLayerChromium::setContentsOpaque(bool opaque)
{
    GraphicsLayer::setContentsOpaque(opaque);
    m_layer.setOpaque(m_contentsOpaque);
}

bool GraphicsLayerChromium::setFilters(const FilterOperations& filters)
{
    m_layer.unwrap<LayerChromium>()->setFilters(filters);
    return GraphicsLayer::setFilters(filters);
}

void GraphicsLayerChromium::setMaskLayer(GraphicsLayer* maskLayer)
{
    if (maskLayer == m_maskLayer)
        return;

    GraphicsLayer::setMaskLayer(maskLayer);

    LayerChromium* maskLayerChromium = m_maskLayer ? m_maskLayer->platformLayer() : 0;
    m_layer.unwrap<LayerChromium>()->setMaskLayer(maskLayerChromium);
}

void GraphicsLayerChromium::setBackfaceVisibility(bool visible)
{
    GraphicsLayer::setBackfaceVisibility(visible);
    m_layer.setDoubleSided(m_backfaceVisibility);
}

void GraphicsLayerChromium::setOpacity(float opacity)
{
    float clampedOpacity = max(min(opacity, 1.0f), 0.0f);
    GraphicsLayer::setOpacity(clampedOpacity);
    primaryLayer().setOpacity(opacity);
}

void GraphicsLayerChromium::setReplicatedByLayer(GraphicsLayer* layer)
{
    GraphicsLayerChromium* layerChromium = static_cast<GraphicsLayerChromium*>(layer);
    GraphicsLayer::setReplicatedByLayer(layer);
    LayerChromium* replicaLayer = layerChromium ? layerChromium->primaryLayer().unwrap<LayerChromium>() : 0;
    primaryLayer().unwrap<LayerChromium>()->setReplicaLayer(replicaLayer);
}


void GraphicsLayerChromium::setContentsNeedsDisplay()
{
    if (!m_contentsLayer.isNull())
        m_contentsLayer.invalidate();
}

void GraphicsLayerChromium::setNeedsDisplay()
{
    if (drawsContent())
        m_layer.invalidate();
}

void GraphicsLayerChromium::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (drawsContent())
        m_layer.invalidateRect(rect);
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
        if (m_contentsLayer.isNull() || m_contentsLayerPurpose != ContentsLayerForImage) {
            RefPtr<ImageLayerChromium> imageLayer = ImageLayerChromium::create();
            setupContentsLayer(imageLayer.get());
            m_contentsLayerPurpose = ContentsLayerForImage;
            childrenChanged = true;
        }
        ImageLayerChromium* imageLayer = static_cast<ImageLayerChromium*>(m_contentsLayer.unwrap<LayerChromium>());
        imageLayer->setContents(image);
        imageLayer->setOpaque(image->isBitmapImage() && !image->currentFrameHasAlpha());
        updateContentsRect();
    } else {
        if (!m_contentsLayer.isNull()) {
            childrenChanged = true;

            // The old contents layer will be removed via updateChildList.
            m_contentsLayer.reset();
        }
    }

    if (childrenChanged)
        updateChildList();
}

void GraphicsLayerChromium::setContentsToCanvas(PlatformLayer* platformLayer)
{
    bool childrenChanged = false;
    if (platformLayer) {
        if (m_contentsLayer.unwrap<LayerChromium>() != platformLayer) {
            setupContentsLayer(platformLayer);
            m_contentsLayerPurpose = ContentsLayerForCanvas;
            childrenChanged = true;
        }
        updateContentsRect();
    } else {
        if (!m_contentsLayer.isNull()) {
            childrenChanged = true;

            // The old contents layer will be removed via updateChildList.
            m_contentsLayer.reset();
        }
    }

    if (childrenChanged)
        updateChildList();
}

bool GraphicsLayerChromium::addAnimation(const KeyframeValueList& values, const IntSize& boxSize, const Animation* animation, const String& animationName, double timeOffset)
{
    primaryLayer().unwrap<LayerChromium>()->setLayerAnimationDelegate(this);
    return primaryLayer().unwrap<LayerChromium>()->addAnimation(values, boxSize, animation, mapAnimationNameToId(animationName), AnimationIdVendor::getNextGroupId(), timeOffset);
}

void GraphicsLayerChromium::pauseAnimation(const String& animationName, double timeOffset)
{
    primaryLayer().unwrap<LayerChromium>()->pauseAnimation(mapAnimationNameToId(animationName), timeOffset);
}

void GraphicsLayerChromium::removeAnimation(const String& animationName)
{
    primaryLayer().unwrap<LayerChromium>()->removeAnimation(mapAnimationNameToId(animationName));
}

void GraphicsLayerChromium::suspendAnimations(double wallClockTime)
{
    // |wallClockTime| is in the wrong time base. Need to convert here.
    // FIXME: find a more reliable way to do this.
    double monotonicTime = wallClockTime + monotonicallyIncreasingTime() - currentTime();
    primaryLayer().unwrap<LayerChromium>()->suspendAnimations(monotonicTime);
}

void GraphicsLayerChromium::resumeAnimations()
{
    primaryLayer().unwrap<LayerChromium>()->resumeAnimations(monotonicallyIncreasingTime());
}

void GraphicsLayerChromium::addLinkHighlight(const Path& path)
{
    m_linkHighlight = LinkHighlight::create(this, path, AnimationIdVendor::LinkHighlightAnimationId, AnimationIdVendor::getNextGroupId());
    updateChildList();
}

void GraphicsLayerChromium::didFinishLinkHighlight()
{
    if (m_linkHighlight)
        m_linkHighlight->contentLayer()->removeFromParent();

    m_linkHighlight.clear();
}

void GraphicsLayerChromium::setContentsToMedia(PlatformLayer* layer)
{
    bool childrenChanged = false;
    if (layer) {
        if (m_contentsLayer.isNull() || m_contentsLayerPurpose != ContentsLayerForVideo) {
            setupContentsLayer(layer);
            m_contentsLayerPurpose = ContentsLayerForVideo;
            childrenChanged = true;
        }
        updateContentsRect();
    } else {
        if (!m_contentsLayer.isNull()) {
            childrenChanged = true;

            // The old contents layer will be removed via updateChildList.
            m_contentsLayer.reset();
        }
    }

    if (childrenChanged)
        updateChildList();
}

WebLayer GraphicsLayerChromium::hostLayerForChildren() const
{
    return m_transformLayer.isNull() ? m_layer :  m_transformLayer;
}

WebLayer GraphicsLayerChromium::layerForParent() const
{
    return m_transformLayer.isNull() ? m_layer :  m_transformLayer;
}

PlatformLayer* GraphicsLayerChromium::platformLayer() const
{
    return primaryLayer().unwrap<LayerChromium>();
}

void GraphicsLayerChromium::setDebugBackgroundColor(const Color& color)
{
    if (color.isValid())
        m_layer.setBackgroundColor(color.rgb());
    else
        m_layer.setBackgroundColor(static_cast<RGBA32>(0));
}

void GraphicsLayerChromium::setDebugBorder(const Color& color, float borderWidth)
{
    if (color.isValid()) {
        m_layer.setDebugBorderColor(color.rgb());
        m_layer.setDebugBorderWidth(borderWidth);
    } else {
        m_layer.setDebugBorderColor(static_cast<RGBA32>(0));
        m_layer.setDebugBorderWidth(0);
    }
}

void GraphicsLayerChromium::updateChildList()
{
    Vector<RefPtr<LayerChromium> > newChildren;

    if (!m_transformLayer.isNull()) {
        // Add the primary layer first. Even if we have negative z-order children, the primary layer always comes behind.
        newChildren.append(m_layer.unwrap<LayerChromium>());
    } else if (!m_contentsLayer.isNull()) {
        // FIXME: add the contents layer in the correct order with negative z-order children.
        // This does not cause visible rendering issues because currently contents layers are only used
        // for replaced elements that don't have children.
        newChildren.append(m_contentsLayer.unwrap<LayerChromium>());
    }

    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayerChromium* curChild = static_cast<GraphicsLayerChromium*>(childLayers[i]);

        LayerChromium* childLayer = curChild->layerForParent().unwrap<LayerChromium>();
        newChildren.append(childLayer);
    }

    if (m_linkHighlight)
        newChildren.append(m_linkHighlight->contentLayer());

    for (size_t i = 0; i < newChildren.size(); ++i)
        newChildren[i]->removeFromParent();

    if (!m_transformLayer.isNull()) {
        m_transformLayer.unwrap<LayerChromium>()->setChildren(newChildren);

        if (!m_contentsLayer.isNull()) {
            // If we have a transform layer, then the contents layer is parented in the
            // primary layer (which is itself a child of the transform layer).
            m_layer.removeAllChildren();
            m_layer.addChild(m_contentsLayer);
        }
    } else
        m_layer.unwrap<LayerChromium>()->setChildren(newChildren);
}

void GraphicsLayerChromium::updateLayerPosition()
{
    // Position is offset on the layer by the layer anchor point.
    FloatPoint layerPosition(m_position.x() + m_anchorPoint.x() * m_size.width(),
                             m_position.y() + m_anchorPoint.y() * m_size.height());

    primaryLayer().setPosition(layerPosition);
}

void GraphicsLayerChromium::updateLayerSize()
{
    IntSize layerSize(m_size.width(), m_size.height());
    if (!m_transformLayer.isNull()) {
        m_transformLayer.setBounds(layerSize);
        // The anchor of the contents layer is always at 0.5, 0.5, so the position is center-relative.
        FloatPoint centerPoint(m_size.width() / 2, m_size.height() / 2);
        m_layer.setPosition(centerPoint);
    }

    m_layer.setBounds(layerSize);

    // Note that we don't resize m_contentsLayer. It's up the caller to do that.

    // If we've changed the bounds, we need to recalculate the position
    // of the layer, taking anchor point into account.
    updateLayerPosition();
}

void GraphicsLayerChromium::updateAnchorPoint()
{
    primaryLayer().setAnchorPoint(FloatPoint(m_anchorPoint.x(), m_anchorPoint.y()));
    primaryLayer().setAnchorPointZ(m_anchorPoint.z());

    updateLayerPosition();
}

void GraphicsLayerChromium::updateTransform()
{
    primaryLayer().setTransform(WebKit::WebTransformationMatrix(m_transform));
}

void GraphicsLayerChromium::updateChildrenTransform()
{
    primaryLayer().setSublayerTransform(WebKit::WebTransformationMatrix(m_childrenTransform));
}

void GraphicsLayerChromium::updateMasksToBounds()
{
    m_layer.setMasksToBounds(m_masksToBounds);
    updateDebugIndicators();
}

void GraphicsLayerChromium::updateLayerPreserves3D()
{
    if (m_preserves3D && m_transformLayer.isNull()) {
        // Create the transform layer.
        m_transformLayer = WebLayer::create();
        m_transformLayer.setPreserves3D(true);
        m_transformLayer.unwrap<LayerChromium>()->setLayerAnimationDelegate(this);
        m_transformLayer.unwrap<LayerChromium>()->setLayerAnimationController(m_layer.unwrap<LayerChromium>()->releaseLayerAnimationController());

        // Copy the position from this layer.
        updateLayerPosition();
        updateLayerSize();
        updateAnchorPoint();
        updateTransform();
        updateChildrenTransform();

        m_layer.setPosition(FloatPoint(m_size.width() / 2.0f, m_size.height() / 2.0f));

        m_layer.setAnchorPoint(FloatPoint(0.5f, 0.5f));
        m_layer.setTransform(SkMatrix44());

        // Set the old layer to opacity of 1. Further down we will set the opacity on the transform layer.
        m_layer.setOpacity(1);

        m_layer.setContentsScale(contentsScale());

        // Move this layer to be a child of the transform layer.
        if (!m_layer.parent().isNull())
            m_layer.parent().replaceChild(m_layer, m_transformLayer);
        m_transformLayer.addChild(m_layer);

        updateChildList();
    } else if (!m_preserves3D && !m_transformLayer.isNull()) {
        // Relace the transformLayer in the parent with this layer.
        m_layer.removeFromParent();
        if (!m_transformLayer.parent().isNull())
            m_transformLayer.parent().replaceChild(m_transformLayer, m_layer);

        m_layer.unwrap<LayerChromium>()->setLayerAnimationDelegate(this);
        m_layer.unwrap<LayerChromium>()->setLayerAnimationController(m_transformLayer.unwrap<LayerChromium>()->releaseLayerAnimationController());

        // Release the transform layer.
        m_transformLayer.unwrap<LayerChromium>()->setLayerAnimationDelegate(0);
        m_transformLayer.reset();

        updateLayerPosition();
        updateLayerSize();
        updateAnchorPoint();
        updateTransform();
        updateChildrenTransform();

        updateChildList();
    }

    m_layer.setPreserves3D(m_preserves3D);
    primaryLayer().setOpacity(m_opacity);
    updateNames();
}

void GraphicsLayerChromium::updateLayerIsDrawable()
{
    // For the rest of the accelerated compositor code, there is no reason to make a
    // distinction between drawsContent and contentsVisible. So, for m_layer, these two
    // flags are combined here. m_contentsLayer shouldn't receive the drawsContent flag
    // so it is only given contentsVisible.

    m_layer.setDrawsContent(m_drawsContent && m_contentsVisible);

    if (!m_contentsLayer.isNull())
        m_contentsLayer.setDrawsContent(m_contentsVisible);

    if (m_drawsContent)
        m_layer.invalidate();

    updateDebugIndicators();
}

void GraphicsLayerChromium::updateLayerBackgroundColor()
{
    if (m_contentsLayer.isNull())
        return;

    // We never create the contents layer just for background color yet.
    if (m_backgroundColorSet)
        m_contentsLayer.setBackgroundColor(m_backgroundColor.rgb());
    else
        m_contentsLayer.setBackgroundColor(static_cast<RGBA32>(0));
}

void GraphicsLayerChromium::updateContentsVideo()
{
    // FIXME: Implement
}

void GraphicsLayerChromium::updateContentsRect()
{
    if (m_contentsLayer.isNull())
        return;

    m_contentsLayer.setPosition(FloatPoint(m_contentsRect.x(), m_contentsRect.y()));
    m_contentsLayer.setBounds(IntSize(m_contentsRect.width(), m_contentsRect.height()));
}

void GraphicsLayerChromium::updateContentsScale()
{
    // If page scale is already applied then there's no need to apply it again.
    if (appliesPageScale() || m_layer.isNull())
        return;

    m_layer.setContentsScale(contentsScale());
}

void GraphicsLayerChromium::setupContentsLayer(LayerChromium* contentsLayer)
{
    if (contentsLayer == m_contentsLayer.unwrap<LayerChromium>())
        return;

    if (!m_contentsLayer.isNull()) {
        m_contentsLayer.removeFromParent();
        m_contentsLayer.reset();
    }

    if (contentsLayer) {
        m_contentsLayer = WebLayer(contentsLayer);

        m_contentsLayer.setAnchorPoint(FloatPoint(0, 0));

        // It is necessary to call setDrawsContent as soon as we receive the new contentsLayer, for
        // the correctness of early exit conditions in setDrawsContent() and setContentsVisible().
        m_contentsLayer.setDrawsContent(m_contentsVisible);

        // Insert the content layer first. Video elements require this, because they have
        // shadow content that must display in front of the video.
        m_layer.insertChild(m_contentsLayer, 0);

        if (showDebugBorders()) {
            m_contentsLayer.setDebugBorderColor(Color(0, 0, 128, 180).rgb());
            m_contentsLayer.setDebugBorderWidth(1);
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

void GraphicsLayerChromium::deviceOrPageScaleFactorChanged()
{
    updateContentsScale();
    // Invalidations are clamped to the layer's bounds but we receive the scale changed notification before receiving
    // the new layer bounds. When the scale changes, we really want to invalidate the post-scale layer bounds, so we
    // remember that the scale has changed and then invalidate the full layer bounds when we receive the new size.
    m_pageScaleChanged = true;
}

void GraphicsLayerChromium::paintContents(GraphicsContext& context, const IntRect& clip)
{
    paintGraphicsLayerContents(context, clip);
}

int GraphicsLayerChromium::mapAnimationNameToId(const String& animationName)
{
    if (animationName.isEmpty())
        return 0;

    if (!m_animationIdMap.contains(animationName))
        m_animationIdMap.add(animationName, AnimationIdVendor::getNextAnimationId());

    return m_animationIdMap.find(animationName)->second;
}

void GraphicsLayerChromium::notifyAnimationStarted(double startTime)
{
    if (m_client)
        m_client->notifyAnimationStarted(this, startTime);
}

void GraphicsLayerChromium::notifyAnimationFinished(double)
{
    // Do nothing.
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
