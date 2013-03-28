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

#include "AnimationTranslationUtil.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "GraphicsLayerFactory.h"
#include "Image.h"
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#include "PlatformMemoryInstrumentation.h"
#include "ScrollableArea.h"
#include "SkImageFilter.h"
#include "SkMatrix44.h"
#include "SkiaImageFilterBuilder.h"
#include "SystemTime.h"
#include "TransformSkMatrix44Conversions.h"
#include <public/Platform.h>
#include <public/WebAnimation.h>
#include <public/WebCompositorSupport.h>
#include <public/WebContentLayer.h>
#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>
#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebImageLayer.h>
#include <public/WebSize.h>
#include <public/WebSolidColorLayer.h>
#include <wtf/CurrentTime.h>
#include <wtf/HashSet.h>
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/StringExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

using namespace std;
using namespace WebKit;

namespace WebCore {

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient* client)
{
    return factory->createGraphicsLayer(client);
}

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

GraphicsLayerChromium::GraphicsLayerChromium(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_contentsLayer(0)
    , m_contentsLayerId(0)
    , m_linkHighlight(0)
    , m_contentsLayerPurpose(NoContentsLayer)
    , m_inSetChildren(false)
    , m_scrollableArea(0)
{
    m_opaqueRectTrackingContentLayerDelegate = adoptPtr(new OpaqueRectTrackingContentLayerDelegate(this));
    m_layer = adoptPtr(Platform::current()->compositorSupport()->createContentLayer(m_opaqueRectTrackingContentLayerDelegate.get()));
    m_layer->layer()->setDrawsContent(m_drawsContent && m_contentsVisible);
    m_layer->layer()->setScrollClient(this);
    m_layer->setAutomaticallyComputeRasterScale(true);
}

GraphicsLayerChromium::~GraphicsLayerChromium()
{
    willBeDestroyed();
}

void GraphicsLayerChromium::willBeDestroyed()
{
    if (m_linkHighlight) {
        m_linkHighlight->clearCurrentGraphicsLayer();
        m_linkHighlight = 0;
    }

    GraphicsLayer::willBeDestroyed();
}

void GraphicsLayerChromium::setName(const String& inName)
{
    m_nameBase = inName;
    String name = String::format("GraphicsLayer(%p) ", this) + inName;
    GraphicsLayer::setName(name);
    updateNames();
}

void GraphicsLayerChromium::updateNames()
{
    String debugName = "Layer for " + m_nameBase;
    m_layer->layer()->setDebugName(debugName);

    if (m_transformLayer) {
        String debugName = "TransformLayer for " + m_nameBase;
        m_transformLayer->setDebugName(debugName);
    }
    if (WebLayer* contentsLayer = contentsLayerIfRegistered()) {
        String debugName = "ContentsLayer for " + m_nameBase;
        contentsLayer->setDebugName(debugName);
    }
    if (m_linkHighlight) {
        String debugName = "LinkHighlight for " + m_nameBase;
        m_linkHighlight->layer()->setDebugName(debugName);
    }
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
    platformLayer()->removeFromParent();
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
}

void GraphicsLayerChromium::setTransform(const TransformationMatrix& transform)
{
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
    // Note carefully this early-exit is only correct because we also properly call
    // WebLayer::setDrawsContent whenever m_contentsLayer is set to a new layer in setupContentsLayer().
    if (drawsContent == m_drawsContent)
        return;

    GraphicsLayer::setDrawsContent(drawsContent);
    updateLayerIsDrawable();
}

void GraphicsLayerChromium::setContentsVisible(bool contentsVisible)
{
    // Note carefully this early-exit is only correct because we also properly call
    // WebLayer::setDrawsContent whenever m_contentsLayer is set to a new layer in setupContentsLayer().
    if (contentsVisible == m_contentsVisible)
        return;

    GraphicsLayer::setContentsVisible(contentsVisible);
    updateLayerIsDrawable();
}

void GraphicsLayerChromium::setBackgroundColor(const Color& color)
{
    if (color == m_backgroundColor)
        return;

    GraphicsLayer::setBackgroundColor(color);
    updateLayerBackgroundColor();
}

void GraphicsLayerChromium::setContentsOpaque(bool opaque)
{
    GraphicsLayer::setContentsOpaque(opaque);
    m_layer->layer()->setOpaque(m_contentsOpaque);
    m_opaqueRectTrackingContentLayerDelegate->setOpaque(m_contentsOpaque);
}

static bool copyWebCoreFilterOperationsToWebFilterOperations(const FilterOperations& filters, WebFilterOperations& webFilters)
{
    for (size_t i = 0; i < filters.size(); ++i) {
        const FilterOperation& op = *filters.at(i);
        switch (op.getOperationType()) {
        case FilterOperation::REFERENCE:
            return false; // Not supported.
        case FilterOperation::GRAYSCALE:
        case FilterOperation::SEPIA:
        case FilterOperation::SATURATE:
        case FilterOperation::HUE_ROTATE: {
            float amount = static_cast<const BasicColorMatrixFilterOperation*>(&op)->amount();
            switch (op.getOperationType()) {
            case FilterOperation::GRAYSCALE:
                webFilters.append(WebFilterOperation::createGrayscaleFilter(amount));
                break;
            case FilterOperation::SEPIA:
                webFilters.append(WebFilterOperation::createSepiaFilter(amount));
                break;
            case FilterOperation::SATURATE:
                webFilters.append(WebFilterOperation::createSaturateFilter(amount));
                break;
            case FilterOperation::HUE_ROTATE:
                webFilters.append(WebFilterOperation::createHueRotateFilter(amount));
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        }
        case FilterOperation::INVERT:
        case FilterOperation::OPACITY:
        case FilterOperation::BRIGHTNESS:
        case FilterOperation::CONTRAST: {
            float amount = static_cast<const BasicComponentTransferFilterOperation*>(&op)->amount();
            switch (op.getOperationType()) {
            case FilterOperation::INVERT:
                webFilters.append(WebFilterOperation::createInvertFilter(amount));
                break;
            case FilterOperation::OPACITY:
                webFilters.append(WebFilterOperation::createOpacityFilter(amount));
                break;
            case FilterOperation::BRIGHTNESS:
                webFilters.append(WebFilterOperation::createBrightnessFilter(amount));
                break;
            case FilterOperation::CONTRAST:
                webFilters.append(WebFilterOperation::createContrastFilter(amount));
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        }
        case FilterOperation::BLUR: {
            float pixelRadius = static_cast<const BlurFilterOperation*>(&op)->stdDeviation().getFloatValue();
            webFilters.append(WebFilterOperation::createBlurFilter(pixelRadius));
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            const DropShadowFilterOperation& dropShadowOp = *static_cast<const DropShadowFilterOperation*>(&op);
            webFilters.append(WebFilterOperation::createDropShadowFilter(WebPoint(dropShadowOp.x(), dropShadowOp.y()), dropShadowOp.stdDeviation(), dropShadowOp.color().rgb()));
            break;
        }
#if ENABLE(CSS_SHADERS)
        case FilterOperation::CUSTOM:
        case FilterOperation::VALIDATED_CUSTOM:
            return false; // Not supported.
#endif
        case FilterOperation::PASSTHROUGH:
        case FilterOperation::NONE:
            break;
        }
    }
    return true;
}

bool GraphicsLayerChromium::setFilters(const FilterOperations& filters)
{
    // FIXME: For now, we only use SkImageFilters if there is a reference
    // filter in the chain. Once all issues have been ironed out, we should
    // switch all filtering over to this path, and remove setFilters() and
    // WebFilterOperations altogether.
    if (filters.hasReferenceFilter()) {
        if (filters.hasCustomFilter()) {
            // Make sure the filters are removed from the platform layer, as they are
            // going to fallback to software mode.
            m_layer->layer()->setFilter(0);
            GraphicsLayer::setFilters(FilterOperations());
            return false;
        }
        SkiaImageFilterBuilder builder;
        SkAutoTUnref<SkImageFilter> imageFilter(builder.build(filters));
        m_layer->layer()->setFilter(imageFilter);
    } else {
        WebFilterOperations webFilters;
        if (!copyWebCoreFilterOperationsToWebFilterOperations(filters, webFilters)) {
            // Make sure the filters are removed from the platform layer, as they are
            // going to fallback to software mode.
            m_layer->layer()->setFilters(WebFilterOperations());
            GraphicsLayer::setFilters(FilterOperations());
            return false;
        }
        m_layer->layer()->setFilters(webFilters);
    }
    return GraphicsLayer::setFilters(filters);
}

void GraphicsLayerChromium::setBackgroundFilters(const FilterOperations& filters)
{
    WebFilterOperations webFilters;
    if (!copyWebCoreFilterOperationsToWebFilterOperations(filters, webFilters))
        return;
    m_layer->layer()->setBackgroundFilters(webFilters);
}

void GraphicsLayerChromium::setMaskLayer(GraphicsLayer* maskLayer)
{
    if (maskLayer == m_maskLayer)
        return;

    GraphicsLayer::setMaskLayer(maskLayer);

    WebLayer* maskWebLayer = m_maskLayer ? m_maskLayer->platformLayer() : 0;
    m_layer->layer()->setMaskLayer(maskWebLayer);
}

void GraphicsLayerChromium::setBackfaceVisibility(bool visible)
{
    GraphicsLayer::setBackfaceVisibility(visible);
    m_layer->setDoubleSided(m_backfaceVisibility);
}

void GraphicsLayerChromium::setOpacity(float opacity)
{
    float clampedOpacity = max(min(opacity, 1.0f), 0.0f);
    GraphicsLayer::setOpacity(clampedOpacity);
    platformLayer()->setOpacity(opacity);
}

void GraphicsLayerChromium::setReplicatedByLayer(GraphicsLayer* layer)
{
    GraphicsLayerChromium* layerChromium = static_cast<GraphicsLayerChromium*>(layer);
    GraphicsLayer::setReplicatedByLayer(layer);

    WebLayer* webReplicaLayer = layerChromium ? layerChromium->platformLayer() : 0;
    platformLayer()->setReplicaLayer(webReplicaLayer);
}


void GraphicsLayerChromium::setContentsNeedsDisplay()
{
    if (WebLayer* contentsLayer = contentsLayerIfRegistered()) {
        contentsLayer->invalidate();
        addRepaintRect(contentsRect());
    }
}

void GraphicsLayerChromium::setNeedsDisplay()
{
    if (drawsContent()) {
        m_layer->layer()->invalidate();
        addRepaintRect(FloatRect(FloatPoint(), m_size));
        if (m_linkHighlight)
            m_linkHighlight->invalidate();
    }
}

void GraphicsLayerChromium::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (drawsContent()) {
        m_layer->layer()->invalidateRect(rect);
        addRepaintRect(rect);
        if (m_linkHighlight)
            m_linkHighlight->invalidate();
    }
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
    NativeImageSkia* nativeImage = image ? image->nativeImageForCurrentFrame() : 0;
    if (nativeImage) {
        if (m_contentsLayerPurpose != ContentsLayerForImage) {
            m_imageLayer = adoptPtr(Platform::current()->compositorSupport()->createImageLayer());
            registerContentsLayer(m_imageLayer->layer());

            setupContentsLayer(m_imageLayer->layer());
            m_contentsLayerPurpose = ContentsLayerForImage;
            childrenChanged = true;
        }
        m_imageLayer->setBitmap(nativeImage->bitmap());
        m_imageLayer->layer()->setOpaque(image->currentFrameKnownToBeOpaque());
        updateContentsRect();
    } else {
        if (m_imageLayer) {
            childrenChanged = true;

            unregisterContentsLayer(m_imageLayer->layer());
            m_imageLayer.clear();
        }
        // The old contents layer will be removed via updateChildList.
        m_contentsLayer = 0;
    }

    if (childrenChanged)
        updateChildList();
}

void GraphicsLayerChromium::setContentsToSolidColor(const Color& color)
{
    if (color == m_contentsSolidColor)
        return;

    bool childrenChanged = false;

    m_contentsSolidColor = color;

    if (color.isValid() && color.alpha()) {
        if (!m_contentsSolidColorLayer) {
            m_contentsSolidColorLayer = adoptPtr(Platform::current()->compositorSupport()->createSolidColorLayer());
            registerContentsLayer(m_contentsSolidColorLayer->layer());

            setupContentsLayer(m_contentsSolidColorLayer->layer());
            childrenChanged = true;
        }

        m_contentsSolidColorLayer->setBackgroundColor(color.rgb());
        updateContentsRect();
    } else {
        if (m_contentsSolidColorLayer) {
            childrenChanged = true;
            unregisterContentsLayer(m_contentsSolidColorLayer->layer());
            m_contentsSolidColorLayer.clear();
        }
        m_contentsLayer = 0;
    }

    if (childrenChanged)
        updateChildList();

}

static HashSet<int>* s_registeredLayerSet;

void GraphicsLayerChromium::registerContentsLayer(WebLayer* layer)
{
    if (!s_registeredLayerSet)
        s_registeredLayerSet = new HashSet<int>;
    if (s_registeredLayerSet->contains(layer->id()))
        CRASH();
    s_registeredLayerSet->add(layer->id());
}

void GraphicsLayerChromium::unregisterContentsLayer(WebLayer* layer)
{
    ASSERT(s_registeredLayerSet);
    if (!s_registeredLayerSet->contains(layer->id()))
        CRASH();
    s_registeredLayerSet->remove(layer->id());
}

void GraphicsLayerChromium::clearContentsLayerIfUnregistered()
{
    if (!m_contentsLayerId || s_registeredLayerSet->contains(m_contentsLayerId))
        return;

    m_contentsLayer = 0;
    m_contentsLayerId = 0;
}

WebLayer* GraphicsLayerChromium::contentsLayerIfRegistered()
{
    clearContentsLayerIfUnregistered();
    return m_contentsLayer;
}

void GraphicsLayerChromium::setContentsToCanvas(PlatformLayer* layer)
{
    setContentsTo(ContentsLayerForCanvas, layer);
}

void GraphicsLayerChromium::setContentsToMedia(PlatformLayer* layer)
{
    setContentsTo(ContentsLayerForVideo, layer);
}

void GraphicsLayerChromium::setContentsTo(ContentsLayerPurpose purpose, WebLayer* layer)
{
    bool childrenChanged = false;
    if (layer) {
        ASSERT(s_registeredLayerSet);
        if (!s_registeredLayerSet->contains(layer->id()))
            CRASH();
        if (m_contentsLayerId != layer->id()) {
            setupContentsLayer(layer);
            m_contentsLayerPurpose = purpose;
            childrenChanged = true;
        }
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

bool GraphicsLayerChromium::addAnimation(const KeyframeValueList& values, const IntSize& boxSize, const Animation* animation, const String& animationName, double timeOffset)
{
    platformLayer()->setAnimationDelegate(this);

    int animationId = 0;

    if (m_animationIdMap.contains(animationName))
        animationId = m_animationIdMap.get(animationName);

    OwnPtr<WebAnimation> toAdd(createWebAnimation(values, animation, animationId, timeOffset, boxSize));

    if (toAdd) {
        animationId = toAdd->id();
        m_animationIdMap.set(animationName, animationId);

        // Remove any existing animations with the same animation id and target property.
        platformLayer()->removeAnimation(animationId, toAdd->targetProperty());
        return platformLayer()->addAnimation(toAdd.get());
    }

    return false;
}

void GraphicsLayerChromium::pauseAnimation(const String& animationName, double timeOffset)
{
    if (m_animationIdMap.contains(animationName))
        platformLayer()->pauseAnimation(m_animationIdMap.get(animationName), timeOffset);
}

void GraphicsLayerChromium::removeAnimation(const String& animationName)
{
    if (m_animationIdMap.contains(animationName))
        platformLayer()->removeAnimation(m_animationIdMap.get(animationName));
}

void GraphicsLayerChromium::suspendAnimations(double wallClockTime)
{
    // |wallClockTime| is in the wrong time base. Need to convert here.
    // FIXME: find a more reliable way to do this.
    double monotonicTime = wallClockTime + monotonicallyIncreasingTime() - currentTime();
    platformLayer()->suspendAnimations(monotonicTime);
}

void GraphicsLayerChromium::resumeAnimations()
{
    platformLayer()->resumeAnimations(monotonicallyIncreasingTime());
}

void GraphicsLayerChromium::setLinkHighlight(LinkHighlightClient* linkHighlight)
{
    m_linkHighlight = linkHighlight;
    updateChildList();
}

PlatformLayer* GraphicsLayerChromium::platformLayer() const
{
    return m_transformLayer ? m_transformLayer.get() : m_layer->layer();
}

void GraphicsLayerChromium::updateChildList()
{
    WebLayer* childHost = m_transformLayer ? m_transformLayer.get() : m_layer->layer();
    childHost->removeAllChildren();

    clearContentsLayerIfUnregistered();

    if (m_transformLayer) {
        // Add the primary layer first. Even if we have negative z-order children, the primary layer always comes behind.
        childHost->addChild(m_layer->layer());
    } else if (m_contentsLayer) {
        // FIXME: add the contents layer in the correct order with negative z-order children.
        // This does not cause visible rendering issues because currently contents layers are only used
        // for replaced elements that don't have children.
        childHost->addChild(m_contentsLayer);
    }

    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();
    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayerChromium* curChild = static_cast<GraphicsLayerChromium*>(childLayers[i]);

        childHost->addChild(curChild->platformLayer());
    }

    if (m_linkHighlight)
        childHost->addChild(m_linkHighlight->layer());

    if (m_transformLayer && m_contentsLayer) {
        // If we have a transform layer, then the contents layer is parented in the
        // primary layer (which is itself a child of the transform layer).
        m_layer->layer()->removeAllChildren();
        m_layer->layer()->addChild(m_contentsLayer);
    }
}

void GraphicsLayerChromium::updateLayerPosition()
{
    platformLayer()->setPosition(m_position);
}

void GraphicsLayerChromium::updateLayerSize()
{
    IntSize layerSize(m_size.width(), m_size.height());
    if (m_transformLayer) {
        m_transformLayer->setBounds(layerSize);
        m_layer->layer()->setPosition(FloatPoint());
    }

    m_layer->layer()->setBounds(layerSize);

    // Note that we don't resize m_contentsLayer-> It's up the caller to do that.
}

void GraphicsLayerChromium::updateAnchorPoint()
{
    platformLayer()->setAnchorPoint(FloatPoint(m_anchorPoint.x(), m_anchorPoint.y()));
    platformLayer()->setAnchorPointZ(m_anchorPoint.z());
}

void GraphicsLayerChromium::updateTransform()
{
    platformLayer()->setTransform(TransformSkMatrix44Conversions::convert(m_transform));
}

void GraphicsLayerChromium::updateChildrenTransform()
{
    platformLayer()->setSublayerTransform(TransformSkMatrix44Conversions::convert(m_childrenTransform));
}

void GraphicsLayerChromium::updateMasksToBounds()
{
    m_layer->layer()->setMasksToBounds(m_masksToBounds);
}

void GraphicsLayerChromium::updateLayerPreserves3D()
{
    if (m_preserves3D && !m_transformLayer) {
        m_transformLayer = adoptPtr(Platform::current()->compositorSupport()->createLayer());
        m_transformLayer->setPreserves3D(true);
        m_transformLayer->setAnimationDelegate(this);
        m_layer->layer()->transferAnimationsTo(m_transformLayer.get());

        // Copy the position from this layer.
        updateLayerPosition();
        updateLayerSize();
        updateAnchorPoint();
        updateTransform();
        updateChildrenTransform();

        m_layer->layer()->setPosition(FloatPoint::zero());

        m_layer->layer()->setAnchorPoint(FloatPoint(0.5f, 0.5f));
        m_layer->layer()->setTransform(SkMatrix44::I());

        // Set the old layer to opacity of 1. Further down we will set the opacity on the transform layer.
        m_layer->layer()->setOpacity(1);

        // Move this layer to be a child of the transform layer.
        if (parent())
            parent()->platformLayer()->replaceChild(m_layer->layer(), m_transformLayer.get());
        m_transformLayer->addChild(m_layer->layer());

        updateChildList();
    } else if (!m_preserves3D && m_transformLayer) {
        // Replace the transformLayer in the parent with this layer.
        m_layer->layer()->removeFromParent();
        if (parent())
            parent()->platformLayer()->replaceChild(m_transformLayer.get(), m_layer->layer());

        m_layer->layer()->setAnimationDelegate(this);
        m_transformLayer->transferAnimationsTo(m_layer->layer());

        // Release the transform layer.
        m_transformLayer->setAnimationDelegate(0);
        m_transformLayer.clear();

        updateLayerPosition();
        updateLayerSize();
        updateAnchorPoint();
        updateTransform();
        updateChildrenTransform();

        updateChildList();
    }

    m_layer->layer()->setPreserves3D(m_preserves3D);
    platformLayer()->setOpacity(m_opacity);
    updateNames();
}

void GraphicsLayerChromium::updateLayerIsDrawable()
{
    // For the rest of the accelerated compositor code, there is no reason to make a
    // distinction between drawsContent and contentsVisible. So, for m_layer->layer(), these two
    // flags are combined here. m_contentsLayer shouldn't receive the drawsContent flag
    // so it is only given contentsVisible.

    m_layer->layer()->setDrawsContent(m_drawsContent && m_contentsVisible);
    if (WebLayer* contentsLayer = contentsLayerIfRegistered())
        contentsLayer->setDrawsContent(m_contentsVisible);

    if (m_drawsContent) {
        m_layer->layer()->invalidate();
        if (m_linkHighlight)
            m_linkHighlight->invalidate();
    }
}

void GraphicsLayerChromium::updateLayerBackgroundColor()
{
    m_layer->layer()->setBackgroundColor(m_backgroundColor.rgb());
}

void GraphicsLayerChromium::updateContentsVideo()
{
    // FIXME: Implement
}

void GraphicsLayerChromium::updateContentsRect()
{
    WebLayer* contentsLayer = contentsLayerIfRegistered();
    if (!contentsLayer)
        return;

    contentsLayer->setPosition(FloatPoint(m_contentsRect.x(), m_contentsRect.y()));
    contentsLayer->setBounds(IntSize(m_contentsRect.width(), m_contentsRect.height()));
}

void GraphicsLayerChromium::setupContentsLayer(WebLayer* contentsLayer)
{
    m_contentsLayer = contentsLayer;
    m_contentsLayerId = m_contentsLayer->id();

    if (m_contentsLayer) {
        m_contentsLayer->setAnchorPoint(FloatPoint(0, 0));
        m_contentsLayer->setUseParentBackfaceVisibility(true);

        // It is necessary to call setDrawsContent as soon as we receive the new contentsLayer, for
        // the correctness of early exit conditions in setDrawsContent() and setContentsVisible().
        m_contentsLayer->setDrawsContent(m_contentsVisible);

        // Insert the content layer first. Video elements require this, because they have
        // shadow content that must display in front of the video.
        m_layer->layer()->insertChild(m_contentsLayer, 0);
    }
    updateNames();
}

void GraphicsLayerChromium::setAppliesPageScale(bool)
{
}

bool GraphicsLayerChromium::appliesPageScale() const
{
    return false;
}

void GraphicsLayerChromium::paint(GraphicsContext& context, const IntRect& clip)
{
    paintGraphicsLayerContents(context, clip);
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

void GraphicsLayerChromium::didScroll()
{
    if (m_scrollableArea)
        m_scrollableArea->scrollToOffsetWithoutAnimation(IntPoint(m_layer->layer()->scrollPosition()));
}

void GraphicsLayerChromium::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, PlatformMemoryTypes::Layers);
    GraphicsLayer::reportMemoryUsage(memoryObjectInfo);
    info.addMember(m_nameBase, "nameBase");
    info.addMember(m_layer, "layer");
    info.addMember(m_transformLayer, "transformLayer");
    info.addMember(m_imageLayer, "imageLayer");
    info.addMember(m_contentsLayer, "contentsLayer");
    info.addMember(m_linkHighlight, "linkHighlight");
    info.addMember(m_opaqueRectTrackingContentLayerDelegate, "opaqueRectTrackingContentLayerDelegate");
    info.addMember(m_animationIdMap, "animationIdMap");
    info.addMember(m_scrollableArea, "scrollableArea");
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
