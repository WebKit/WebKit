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
#include <public/Platform.h>
#include <public/WebAnimationCurve.h>
#include <public/WebCompositorSupport.h>
#include <public/WebFloatAnimationCurve.h>
#include <public/WebFloatPoint.h>
#include <public/WebRect.h>
#include <public/WebSize.h>
#include <public/WebTransformationMatrix.h>
#include <wtf/CurrentTime.h>

using namespace WebCore;

namespace WebKit {

class WebViewImpl;

PassOwnPtr<LinkHighlight> LinkHighlight::create(Node* node, WebViewImpl* owningWebViewImpl)
{
    return adoptPtr(new LinkHighlight(node, owningWebViewImpl));
}

LinkHighlight::LinkHighlight(Node* node, WebViewImpl* owningWebViewImpl)
    : m_node(node)
    , m_owningWebViewImpl(owningWebViewImpl)
    , m_currentGraphicsLayer(0)
    , m_geometryNeedsUpdate(false)
    , m_isAnimating(false)
    , m_startTime(monotonicallyIncreasingTime())
{
    ASSERT(m_node);
    ASSERT(owningWebViewImpl);
    WebCompositorSupport* compositorSupport = Platform::current()->compositorSupport();
    m_contentLayer = adoptPtr(compositorSupport->createContentLayer(this));
    m_clipLayer = adoptPtr(compositorSupport->createLayer());
    m_clipLayer->setAnchorPoint(WebFloatPoint());
    m_clipLayer->addChild(m_contentLayer->layer());
    m_contentLayer->layer()->setAnimationDelegate(this);
    m_contentLayer->layer()->setDrawsContent(true);
    m_contentLayer->layer()->setOpacity(1);
    m_geometryNeedsUpdate = true;
    updateGeometry();
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

    GraphicsLayerChromium* newGraphicsLayer = static_cast<GraphicsLayerChromium*>(renderLayer->backing()->graphicsLayer());
    m_clipLayer->setSublayerTransform(WebTransformationMatrix());
    if (!newGraphicsLayer->drawsContent()) {
        m_clipLayer->setSublayerTransform(WebTransformationMatrix(newGraphicsLayer->transform()));
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

static void convertTargetSpaceQuadToCompositedLayer(const FloatQuad& targetSpaceQuad, RenderObject* targetRenderer, RenderObject* compositedRenderer, FloatQuad& compositedSpaceQuad)
{
    ASSERT(targetRenderer);
    ASSERT(compositedRenderer);

    for (unsigned i = 0; i < 4; ++i) {
        IntPoint point;
        switch (i) {
        case 0: point = roundedIntPoint(targetSpaceQuad.p1()); break;
        case 1: point = roundedIntPoint(targetSpaceQuad.p2()); break;
        case 2: point = roundedIntPoint(targetSpaceQuad.p3()); break;
        case 3: point = roundedIntPoint(targetSpaceQuad.p4()); break;
        }

        point = targetRenderer->frame()->view()->contentsToWindow(point);
        point = compositedRenderer->frame()->view()->windowToContents(point);
        FloatPoint floatPoint = compositedRenderer->absoluteToLocal(point, UseTransforms);

        switch (i) {
        case 0: compositedSpaceQuad.setP1(floatPoint); break;
        case 1: compositedSpaceQuad.setP2(floatPoint); break;
        case 2: compositedSpaceQuad.setP3(floatPoint); break;
        case 3: compositedSpaceQuad.setP4(floatPoint); break;
        }
    }
}

static void addQuadToPath(const FloatQuad& quad, Path& path)
{
    // FIXME: Make this create rounded quad-paths, just like the axis-aligned case.
    path.moveTo(quad.p1());
    path.addLineTo(quad.p2());
    path.addLineTo(quad.p3());
    path.addLineTo(quad.p4());
    path.closeSubpath();
}

bool LinkHighlight::computeHighlightLayerPathAndPosition(RenderLayer* compositingLayer)
{
    if (!m_node || !m_node->renderer())
        return false;

    ASSERT(compositingLayer);

    // Get quads for node in absolute coordinates.
    Vector<FloatQuad> quads;
    m_node->renderer()->absoluteQuads(quads);
    ASSERT(quads.size());

    Path newPath;
    for (unsigned quadIndex = 0; quadIndex < quads.size(); ++quadIndex) {

        FloatQuad transformedQuad;

        // Transform node quads in target absolute coords to local coordinates in the compositor layer.
        convertTargetSpaceQuadToCompositedLayer(quads[quadIndex], m_node->renderer(), compositingLayer->renderer(), transformedQuad);

        // FIXME: for now, we'll only use rounded paths if we have a single node quad. The reason for this is that
        // we may sometimes get a chain of adjacent boxes (e.g. for text nodes) which end up looking like sausage
        // links: these should ideally be merged into a single rect before creating the path, but that's
        // another CL.
        if (quads.size() == 1 && transformedQuad.isRectilinear()) {
            FloatSize rectRoundingRadii(3, 3);
            newPath.addRoundedRect(transformedQuad.boundingBox(), rectRoundingRadii);
        } else
            addQuadToPath(transformedQuad, newPath);
    }

    FloatRect boundingRect = newPath.boundingRect();
    newPath.translate(FloatPoint() - boundingRect.location());

    bool pathHasChanged = !m_path.platformPath() || !(*newPath.platformPath() == *m_path.platformPath());
    if (pathHasChanged) {
        m_path = newPath;
        m_contentLayer->layer()->setBounds(enclosingIntRect(boundingRect).size());
    }

    m_contentLayer->layer()->setPosition(boundingRect.location());

    return pathHasChanged;
}

void LinkHighlight::paintContents(WebCanvas* canvas, const WebRect& webClipRect, bool, WebFloatRect&)
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

void LinkHighlight::startHighlightAnimationIfNeeded()
{
    if (m_isAnimating)
        return;

    m_isAnimating = true;
    const float startOpacity = 1;
    // FIXME: Should duration be configurable?
    const float fadeDuration = 0.1f;
    const float minPreFadeDuration = 0.1f;

    m_contentLayer->layer()->setOpacity(startOpacity);

    WebCompositorSupport* compositorSupport = Platform::current()->compositorSupport();

    OwnPtr<WebFloatAnimationCurve> curve = adoptPtr(compositorSupport->createFloatAnimationCurve());

    curve->add(WebFloatKeyframe(0, startOpacity));
    // Make sure we have displayed for at least minPreFadeDuration before starting to fade out.
    float extraDurationRequired = std::max(0.f, minPreFadeDuration - static_cast<float>(monotonicallyIncreasingTime() - m_startTime));
    if (extraDurationRequired)
        curve->add(WebFloatKeyframe(extraDurationRequired, startOpacity));
    // For layout tests we don't fade out.
    curve->add(WebFloatKeyframe(fadeDuration + extraDurationRequired, WebKit::layoutTestMode() ? startOpacity : 0));

    m_animation = adoptPtr(compositorSupport->createAnimation(*curve, WebAnimation::TargetPropertyOpacity));

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

        if (m_currentGraphicsLayer)
            m_currentGraphicsLayer->addRepaintRect(FloatRect(layer()->position().x, layer()->position().y, layer()->bounds().width, layer()->bounds().height));
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
