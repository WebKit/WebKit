/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "cc/CCLayerTreeHostImpl.h"

#include "Extensions3D.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "TraceEvent.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCThreadTask.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

PassOwnPtr<CCLayerTreeHostImpl> CCLayerTreeHostImpl::create(const CCSettings& settings, CCLayerTreeHostImplClient* client)
{
    return adoptPtr(new CCLayerTreeHostImpl(settings, client));
}

CCLayerTreeHostImpl::CCLayerTreeHostImpl(const CCSettings& settings, CCLayerTreeHostImplClient* client)
    : m_client(client)
    , m_sourceFrameNumber(-1)
    , m_frameNumber(0)
    , m_settings(settings)
    , m_visible(true)
    , m_haveWheelEventHandlers(false)
    , m_pageScale(1)
    , m_pageScaleDelta(1)
    , m_sentPageScaleDelta(1)
    , m_minPageScale(0)
    , m_maxPageScale(0)
    , m_pinchGestureActive(false)
{
    ASSERT(CCProxy::isImplThread());
}

CCLayerTreeHostImpl::~CCLayerTreeHostImpl()
{
    ASSERT(CCProxy::isImplThread());
    TRACE_EVENT("CCLayerTreeHostImpl::~CCLayerTreeHostImpl()", this, 0);
    if (m_layerRenderer)
        m_layerRenderer->close();
}

void CCLayerTreeHostImpl::beginCommit()
{
}

void CCLayerTreeHostImpl::commitComplete()
{
    // Recompute max scroll position; must be after layer content bounds are
    // updated.
    updateMaxScrollPosition();
}

GraphicsContext3D* CCLayerTreeHostImpl::context()
{
    return m_layerRenderer ? m_layerRenderer->context() : 0;
}

void CCLayerTreeHostImpl::animate(double frameBeginTimeMs)
{
}

void CCLayerTreeHostImpl::drawLayers()
{
    TRACE_EVENT("CCLayerTreeHostImpl::drawLayers", this, 0);
    ASSERT(m_layerRenderer);
    if (m_layerRenderer->rootLayer())
        m_layerRenderer->drawLayers();

    ++m_frameNumber;
}

void CCLayerTreeHostImpl::finishAllRendering()
{
    m_layerRenderer->finish();
}

bool CCLayerTreeHostImpl::isContextLost()
{
    ASSERT(m_layerRenderer);
    return m_layerRenderer->isContextLost();
}

const LayerRendererCapabilities& CCLayerTreeHostImpl::layerRendererCapabilities() const
{
    return m_layerRenderer->capabilities();
}

TextureAllocator* CCLayerTreeHostImpl::contentsTextureAllocator() const
{
    return m_layerRenderer ? m_layerRenderer->contentsTextureAllocator() : 0;
}

void CCLayerTreeHostImpl::swapBuffers()
{
    ASSERT(m_layerRenderer && !isContextLost());
    m_layerRenderer->swapBuffers();
}

void CCLayerTreeHostImpl::onSwapBuffersComplete()
{
    m_client->onSwapBuffersCompleteOnImplThread();
}

void CCLayerTreeHostImpl::readback(void* pixels, const IntRect& rect)
{
    ASSERT(m_layerRenderer && !isContextLost());
    m_layerRenderer->getFramebufferPixels(pixels, rect);
}

static CCLayerImpl* findScrollLayer(CCLayerImpl* layer)
{
    if (!layer)
        return 0;

    if (layer->scrollable())
        return layer;

    for (size_t i = 0; i < layer->children().size(); ++i) {
        CCLayerImpl* found = findScrollLayer(layer->children()[i].get());
        if (found)
            return found;
    }

    return 0;
}

void CCLayerTreeHostImpl::setRootLayer(PassRefPtr<CCLayerImpl> layer)
{
    m_rootLayerImpl = layer;

    // FIXME: Currently, this only finds the first scrollable layer.
    m_scrollLayerImpl = findScrollLayer(m_rootLayerImpl.get());
}

void CCLayerTreeHostImpl::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;

    if (m_layerRenderer)
        m_layerRenderer->setVisible(visible);
}

bool CCLayerTreeHostImpl::initializeLayerRenderer(PassRefPtr<GraphicsContext3D> context)
{
    OwnPtr<LayerRendererChromium> layerRenderer;
    layerRenderer = LayerRendererChromium::create(this, context);

    // If creation failed, and we had asked for accelerated painting, disable accelerated painting
    // and try creating the renderer again.
    if (!layerRenderer && m_settings.acceleratePainting) {
        m_settings.acceleratePainting = false;

        layerRenderer = LayerRendererChromium::create(this, context);
    }

    if (m_layerRenderer)
        m_layerRenderer->close();

    m_layerRenderer = layerRenderer.release();
    return m_layerRenderer;
}

void CCLayerTreeHostImpl::setViewport(const IntSize& viewportSize)
{
    if (viewportSize == m_viewportSize)
        return;

    m_viewportSize = viewportSize;
    updateMaxScrollPosition();
    m_layerRenderer->viewportChanged();
}

void CCLayerTreeHostImpl::setPageScaleFactorAndLimits(float pageScale, float minPageScale, float maxPageScale)
{
    if (!pageScale)
        return;

    if (m_sentPageScaleDelta == 1 && pageScale == m_pageScale && minPageScale == m_minPageScale && maxPageScale == m_maxPageScale)
        return;

    m_minPageScale = minPageScale;
    m_maxPageScale = maxPageScale;

    float pageScaleChange = pageScale / m_pageScale;
    m_pageScale = pageScale;
    m_sentPageScaleDelta = 1;

    // Clamp delta to limits and refresh display matrix.
    setPageScaleDelta(m_pageScaleDelta);
    applyPageScaleDeltaToScrollLayer();

    adjustScrollsForPageScaleChange(pageScaleChange);
}

void CCLayerTreeHostImpl::adjustScrollsForPageScaleChange(float pageScaleChange)
{
    if (pageScaleChange == 1)
        return;

    // We also need to convert impl-side scroll deltas to pageScale space.
    if (m_scrollLayerImpl) {
        IntSize scrollDelta = m_scrollLayerImpl->scrollDelta();
        scrollDelta.scale(pageScaleChange);
        m_scrollLayerImpl->setScrollDelta(scrollDelta);
    }
}

void CCLayerTreeHostImpl::setPageScaleDelta(float delta)
{
    // Clamp to the current min/max limits.
    float finalMagnifyScale = m_pageScale * delta;
    if (m_minPageScale && finalMagnifyScale < m_minPageScale)
        delta = m_minPageScale / m_pageScale;
    else if (m_maxPageScale && finalMagnifyScale > m_maxPageScale)
        delta = m_maxPageScale / m_pageScale;

    if (delta == m_pageScaleDelta)
        return;

    m_pageScaleDelta = delta;

    updateMaxScrollPosition();
    applyPageScaleDeltaToScrollLayer();
}

void CCLayerTreeHostImpl::applyPageScaleDeltaToScrollLayer()
{
    if (m_scrollLayerImpl)
        m_scrollLayerImpl->setPageScaleDelta(m_pageScaleDelta * m_sentPageScaleDelta);
}

void CCLayerTreeHostImpl::updateMaxScrollPosition()
{
    if (!m_scrollLayerImpl || !m_scrollLayerImpl->children().size())
        return;

    FloatSize viewBounds = m_viewportSize;
    viewBounds.scale(1 / m_pageScaleDelta);

    // TODO(aelias): Hardcoding the first child here is weird. Think of
    // a cleaner way to get the contentBounds on the Impl side.
    IntSize maxScroll = m_scrollLayerImpl->children()[0]->contentBounds() - expandedIntSize(viewBounds);
    // The viewport may be larger than the contents in some cases, such as
    // having a vertical scrollbar but no horizontal overflow.
    maxScroll.clampNegativeToZero();

    m_scrollLayerImpl->setMaxScrollPosition(maxScroll);

    // TODO(aelias): Also update sublayers.
}

void CCLayerTreeHostImpl::setZoomAnimatorTransform(const TransformationMatrix& zoom)
{
    if (!m_scrollLayerImpl)
        return;

    m_scrollLayerImpl->setZoomAnimatorTransform(zoom);
}

double CCLayerTreeHostImpl::currentTimeMs() const
{
    return monotonicallyIncreasingTime() * 1000.0;
}

void CCLayerTreeHostImpl::setNeedsRedraw()
{
    m_client->setNeedsRedrawOnImplThread();
}

CCInputHandlerClient::ScrollStatus CCLayerTreeHostImpl::scrollBegin(const IntPoint&)
{
    // TODO: Check for scrollable sublayers.
    if (!m_scrollLayerImpl || !m_scrollLayerImpl->scrollable())
        return ScrollIgnored;

    return ScrollStarted;
}

void CCLayerTreeHostImpl::scrollBy(const IntSize& scrollDelta)
{
    TRACE_EVENT("CCLayerTreeHostImpl::scrollBy", this, 0);
    if (!m_scrollLayerImpl)
        return;

    m_scrollLayerImpl->scrollBy(scrollDelta);
    m_client->setNeedsCommitOnImplThread();
    m_client->setNeedsRedrawOnImplThread();
}

void CCLayerTreeHostImpl::scrollEnd()
{
}

bool CCLayerTreeHostImpl::haveWheelEventHandlers()
{
    return m_haveWheelEventHandlers;
}

void CCLayerTreeHostImpl::pinchGestureBegin()
{
    m_pinchGestureActive = true;
}

void CCLayerTreeHostImpl::pinchGestureUpdate(float magnifyDelta,
                                             const IntPoint& anchor)
{
    TRACE_EVENT("CCLayerTreeHostImpl::pinchGestureUpdate", this, 0);

    if (magnifyDelta == 1.0)
        return;
    if (!m_scrollLayerImpl)
        return;

    // Keep the center-of-pinch anchor specified by (x, y) in a stable
    // position over the course of the magnify.
    FloatPoint prevScaleAnchor(anchor.x() / m_pageScaleDelta, anchor.y() / m_pageScaleDelta);
    setPageScaleDelta(m_pageScaleDelta * magnifyDelta);
    FloatPoint newScaleAnchor(anchor.x() / m_pageScaleDelta, anchor.y() / m_pageScaleDelta);

    FloatSize move = prevScaleAnchor - newScaleAnchor;

    m_scrollLayerImpl->scrollBy(roundedIntSize(move));
    m_client->setNeedsCommitOnImplThread();
    m_client->setNeedsRedrawOnImplThread();
}

void CCLayerTreeHostImpl::pinchGestureEnd()
{
    m_pinchGestureActive = false;

    m_client->setNeedsCommitOnImplThread();
}

PassOwnPtr<CCScrollAndScaleSet> CCLayerTreeHostImpl::processScrollDeltas()
{
    OwnPtr<CCScrollAndScaleSet> scrollInfo = adoptPtr(new CCScrollAndScaleSet());
    bool didMove = m_scrollLayerImpl && (!m_scrollLayerImpl->scrollDelta().isZero() || m_pageScaleDelta != 1.0f);
    if (!didMove || m_pinchGestureActive) {
        m_sentPageScaleDelta = scrollInfo->pageScaleDelta = 1;
        return scrollInfo.release();
    }

    m_sentPageScaleDelta = scrollInfo->pageScaleDelta = m_pageScaleDelta;
    m_pageScale = m_pageScale * m_sentPageScaleDelta;
    setPageScaleDelta(1);

    // FIXME: track scrolls from layers other than the root
    CCLayerTreeHostCommon::ScrollUpdateInfo scroll;
    scroll.layerId = m_scrollLayerImpl->id();
    scroll.scrollDelta = m_scrollLayerImpl->scrollDelta();
    scrollInfo->scrolls.append(scroll);

    m_scrollLayerImpl->setScrollPosition(m_scrollLayerImpl->scrollPosition() + m_scrollLayerImpl->scrollDelta());
    m_scrollLayerImpl->setPosition(m_scrollLayerImpl->position() - m_scrollLayerImpl->scrollDelta());
    m_scrollLayerImpl->setSentScrollDelta(m_scrollLayerImpl->scrollDelta());
    m_scrollLayerImpl->setScrollDelta(IntSize());

    adjustScrollsForPageScaleChange(m_sentPageScaleDelta);

    return scrollInfo.release();
}

}
