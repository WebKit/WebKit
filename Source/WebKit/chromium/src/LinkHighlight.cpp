/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "LinkHighlight.h"

#include "Color.h"
#include "Frame.h"
#include "FrameView.h"
#include "LayoutTypes.h"
#include "Node.h"
#include "NonCompositedContentHost.h"
#include "PlatformContextSkia.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WebViewImpl.h"
#include <public/WebAnimationCurve.h>
#include <public/WebFloatAnimationCurve.h>
#include <public/WebFloatPoint.h>
#include <public/WebRect.h>
#include <public/WebSize.h>

using namespace WebCore;

namespace WebKit {

class WebViewImpl;

PassOwnPtr<LinkHighlight> LinkHighlight::create(Node* node, WebViewImpl* owningWebViewImpl)
{
    return adoptPtr(new LinkHighlight(node, owningWebViewImpl));
}

LinkHighlight::LinkHighlight(Node* node, WebViewImpl* owningWebViewImpl)
    : m_contentLayer(adoptPtr(WebContentLayer::create(this)))
    , m_clipLayer(adoptPtr(WebLayer::create()))
    , m_node(node)
    , m_owningWebViewImpl(owningWebViewImpl)
    , m_currentGraphicsLayer(0)
    , m_geometryNeedsUpdate(false)
{
    ASSERT(m_node);
    ASSERT(owningWebViewImpl);

    m_clipLayer->setAnchorPoint(WebFloatPoint());
    m_clipLayer->addChild(m_contentLayer->layer());
    m_contentLayer->layer()->setDrawsContent(false);

    // We don't want to show the highlight until startAnimation is called, even though the highlight
    // layer may be added to the tree immediately.
    m_contentLayer->layer()->setOpacity(0);
    m_contentLayer->layer()->setAnimationDelegate(this);
}

LinkHighlight::~LinkHighlight()
{
    clearGraphicsLayerLinkHighlightPointer();
    releaseResources();
}

WebContentLayer* LinkHighlight::contentLayer()
{
    return m_contentLayer.get();
}

WebLayer* LinkHighlight::clipLayer()
{
    return m_clipLayer.get();
}

void LinkHighlight::releaseResources()
{
    m_node.clear();
}

RenderLayer* LinkHighlight::computeEnclosingCompositingLayer()
{
    if (!m_node || !m_node->renderer())
        return 0;

    RenderLayer* renderLayer = m_node->renderer()->enclosingLayer();

    // Find the nearest enclosing composited layer and attach to it. We may need to cross frame boundaries
    // to find a suitable layer.
    while (renderLayer && !renderLayer->isComposited()) {
        if (!renderLayer->parent()) {
            // See if we've reached the root in an enclosed frame.
            if (renderLayer->renderer()->frame()->ownerRenderer())
                renderLayer = renderLayer->renderer()->frame()->ownerRenderer()->enclosingLayer();
            else
                renderLayer = 0;
        } else
            renderLayer = renderLayer->parent();
    }

    if (!renderLayer || !renderLayer->isComposited())
        return 0;

    m_graphicsLayerOffset = FloatPoint();
    GraphicsLayerChromium* newGraphicsLayer = static_cast<GraphicsLayerChromium*>(renderLayer->backing()->graphicsLayer());
    if (!newGraphicsLayer->drawsContent()) {
        m_graphicsLayerOffset = newGraphicsLayer->position();
        newGraphicsLayer = static_cast<GraphicsLayerChromium*>(m_owningWebViewImpl->nonCompositedContentHost()->topLevelRootLayer());
    }

    if (m_currentGraphicsLayer != newGraphicsLayer) {
        if (m_currentGraphicsLayer)
            clearGraphicsLayerLinkHighlightPointer();

        m_currentGraphicsLayer = newGraphicsLayer;
        m_currentGraphicsLayer->setLinkHighlight(this);
    }

    return renderLayer;
}

bool LinkHighlight::computeHighlightLayerPathAndPosition(RenderLayer* compositingLayer)
{
    if (!m_node || !m_node->renderer())
        return false;

    bool pathHasChanged = false;
    FloatRect boundingRect = m_node->getPixelSnappedRect();

    // FIXME: If we ever use a more sophisticated highlight path, we'll need
    // to devise a way of detecting when it changes.
    if (boundingRect.size() != m_path.boundingRect().size()) {
        FloatSize rectRoundingRadii(3, 3);
        m_path.clear();
        m_path.addRoundedRect(boundingRect, rectRoundingRadii);
        // Always treat the path as being at the origin of this layer.
        m_path.translate(FloatPoint() - boundingRect.location());
        pathHasChanged = true;
    }

    FloatRect nodeBounds = boundingRect;

    // This is a simplified, but basically correct, transformation of the target location, converted
    // from its containing frame view to window coordinates and then back to the containing frame view
    // of the composited layer.
    // FIXME: We also need to transform the target's size in case of scaling. This can be done by also transforming
    //        the full rects in the xToY calls, and transforming both the upper-left and lower right corners
    //        to local coordinates at the end..
    ASSERT(compositingLayer);
    IntPoint targetWindow = m_node->renderer()->frame()->view()->contentsToWindow(enclosingIntRect(nodeBounds).location());
    IntPoint targetCompositorAbsolute = compositingLayer->renderer()->frame()->view()->windowToContents(targetWindow);
    FloatPoint targetCompositorLocal = compositingLayer->renderer()->absoluteToLocal(targetCompositorAbsolute, false, true);

    m_contentLayer->layer()->setBounds(WebSize(enclosingIntRect(nodeBounds).size()));
    m_contentLayer->layer()->setPosition(WebFloatPoint(targetCompositorLocal));

    return pathHasChanged;
}

void LinkHighlight::paintContents(WebCanvas* canvas, const WebRect& webClipRect, WebFloatRect&)
{
    if (!m_node || !m_node->renderer())
        return;

    PlatformContextSkia platformContext(canvas);
    GraphicsContext gc(&platformContext);
    IntRect clipRect(IntPoint(webClipRect.x, webClipRect.y), IntSize(webClipRect.width, webClipRect.height));
    gc.clip(clipRect);
    gc.setFillColor(m_node->renderer()->style()->tapHighlightColor(), ColorSpaceDeviceRGB);
    gc.fillPath(m_path);
}

void LinkHighlight::startHighlightAnimation()
{
    const float startOpacity = 1;
    // FIXME: Should duration be configurable?
    const float duration = 2;

    m_contentLayer->layer()->setOpacity(startOpacity);

    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0, startOpacity));
    curve.add(WebFloatKeyframe(duration / 2, startOpacity));
    // For layout tests we don't fade out.
    curve.add(WebFloatKeyframe(duration, WebKit::layoutTestMode() ? startOpacity : 0));

    m_animation = adoptPtr(WebAnimation::create(curve, WebAnimation::TargetPropertyOpacity));
    m_contentLayer->layer()->setDrawsContent(true);
    m_contentLayer->layer()->addAnimation(m_animation.get());

    invalidate();
    m_owningWebViewImpl->scheduleAnimation();
}

void LinkHighlight::clearGraphicsLayerLinkHighlightPointer()
{
    if (m_currentGraphicsLayer) {
        m_currentGraphicsLayer->setLinkHighlight(0);
        m_currentGraphicsLayer = 0;
    }
}

void LinkHighlight::notifyAnimationStarted(double)
{
}

void LinkHighlight::notifyAnimationFinished(double)
{
    // Since WebViewImpl may hang on to us for a while, make sure we
    // release resources as soon as possible.
    clearGraphicsLayerLinkHighlightPointer();
    releaseResources();
}

void LinkHighlight::updateGeometry()
{
    // To avoid unnecessary updates (e.g. other entities have requested animations from our WebViewImpl),
    // only proceed if we actually requested an update.
    if (!m_geometryNeedsUpdate)
        return;

    m_geometryNeedsUpdate = false;

    RenderLayer* compositingLayer = computeEnclosingCompositingLayer();
    if (compositingLayer && computeHighlightLayerPathAndPosition(compositingLayer)) {
        // We only need to invalidate the layer if the highlight size has changed, otherwise
        // we can just re-position the layer without needing to repaint.
        m_contentLayer->layer()->invalidate();
    }
}

void LinkHighlight::clearCurrentGraphicsLayer()
{
    m_currentGraphicsLayer = 0;
    m_geometryNeedsUpdate = true;
}

void LinkHighlight::invalidate()
{
    // Make sure we update geometry on the next callback from WebViewImpl::layout().
    m_geometryNeedsUpdate = true;
}

WebLayer* LinkHighlight::layer()
{
    return clipLayer();
}

} // namespace WeKit
