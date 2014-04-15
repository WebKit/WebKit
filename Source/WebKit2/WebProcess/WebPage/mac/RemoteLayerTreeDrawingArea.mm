/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteLayerTreeDrawingArea.h"

#import "DrawingAreaProxyMessages.h"
#import "GraphicsLayerCARemote.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreeDrawingAreaProxyMessages.h"
#import "RemoteScrollingCoordinator.h"
#import "RemoteScrollingCoordinatorTransaction.h"
#import "WebPage.h"
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/MainFrame.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/Settings.h>
#import <WebCore/TiledBacking.h>

using namespace WebCore;

namespace WebKit {

RemoteLayerTreeDrawingArea::RemoteLayerTreeDrawingArea(WebPage* webPage, const WebPageCreationParameters&)
    : DrawingArea(DrawingAreaTypeRemoteLayerTree, webPage)
    , m_remoteLayerTreeContext(std::make_unique<RemoteLayerTreeContext>(webPage))
    , m_rootLayer(GraphicsLayer::create(graphicsLayerFactory(), nullptr))
    , m_exposedRect(FloatRect::infiniteRect())
    , m_scrolledExposedRect(FloatRect::infiniteRect())
    , m_layerFlushTimer(this, &RemoteLayerTreeDrawingArea::layerFlushTimerFired)
    , m_isFlushingSuspended(false)
    , m_hasDeferredFlush(false)
    , m_waitingForBackingStoreSwap(false)
    , m_hadFlushDeferredWhileWaitingForBackingStoreSwap(false)
{
    webPage->corePage()->settings().setForceCompositingMode(true);
#if PLATFORM(IOS)
    webPage->corePage()->settings().setDelegatesPageScaling(true);
#endif
}

RemoteLayerTreeDrawingArea::~RemoteLayerTreeDrawingArea()
{
}

void RemoteLayerTreeDrawingArea::setNeedsDisplay()
{
}

void RemoteLayerTreeDrawingArea::setNeedsDisplayInRect(const IntRect&)
{
}

void RemoteLayerTreeDrawingArea::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
}

GraphicsLayerFactory* RemoteLayerTreeDrawingArea::graphicsLayerFactory()
{
    return m_remoteLayerTreeContext.get();
}

void RemoteLayerTreeDrawingArea::setRootCompositingLayer(GraphicsLayer* rootLayer)
{
    Vector<GraphicsLayer *> children;
    if (rootLayer) {
        children.append(rootLayer);
        children.append(m_webPage->pageOverlayController().viewOverlayRootLayer());
    }
    m_rootLayer->setChildren(children);

    m_webPage->mainFrameView()->renderView()->compositor().setDocumentOverlayRootLayer(m_webPage->pageOverlayController().documentOverlayRootLayer());
}

void RemoteLayerTreeDrawingArea::updateGeometry(const IntSize& viewSize, const IntSize& layerPosition)
{
    m_viewSize = viewSize;
    m_webPage->setSize(viewSize);

    scheduleCompositingLayerFlush();

    m_webPage->send(Messages::DrawingAreaProxy::DidUpdateGeometry());
}

bool RemoteLayerTreeDrawingArea::shouldUseTiledBackingForFrameView(const FrameView* frameView)
{
    return frameView && frameView->frame().isMainFrame();
}

void RemoteLayerTreeDrawingArea::updatePreferences(const WebPreferencesStore&)
{
    Settings& settings = m_webPage->corePage()->settings();

    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(true);
    settings.setFixedPositionCreatesStackingContext(true);
}

#if PLATFORM(IOS)
void RemoteLayerTreeDrawingArea::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage->setDeviceScaleFactor(deviceScaleFactor);
}
#endif

void RemoteLayerTreeDrawingArea::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_isFlushingSuspended == isFrozen)
        return;

    m_isFlushingSuspended = isFrozen;

    if (!m_isFlushingSuspended && m_hasDeferredFlush) {
        m_hasDeferredFlush = false;
        scheduleCompositingLayerFlush();
    }
}

void RemoteLayerTreeDrawingArea::forceRepaint()
{
    if (m_isFlushingSuspended)
        return;

    for (Frame* frame = &m_webPage->corePage()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        FrameView* frameView = frame->view();
        if (!frameView || !frameView->tiledBacking())
            continue;

        frameView->tiledBacking()->forceRepaint();
    }

    flushLayers();
}

void RemoteLayerTreeDrawingArea::acceleratedAnimationDidStart(uint64_t layerID, double startTime)
{
    m_remoteLayerTreeContext->animationDidStart(layerID, startTime);
}

void RemoteLayerTreeDrawingArea::setExposedRect(const FloatRect& exposedRect)
{
    m_exposedRect = exposedRect;
    updateScrolledExposedRect();
}

#if PLATFORM(IOS)
void RemoteLayerTreeDrawingArea::setExposedContentRect(const FloatRect& exposedContentRect)
{
    FrameView* frameView = m_webPage->corePage()->mainFrame().view();
    if (!frameView)
        return;

    frameView->setExposedContentRect(enclosingIntRect(exposedContentRect));
    scheduleCompositingLayerFlush();
}
#endif

void RemoteLayerTreeDrawingArea::updateScrolledExposedRect()
{
    FrameView* frameView = m_webPage->corePage()->mainFrame().view();
    if (!frameView)
        return;

    m_scrolledExposedRect = m_exposedRect;

#if !PLATFORM(IOS)
    if (!m_exposedRect.isInfinite()) {
        IntPoint scrollPositionWithOrigin = frameView->scrollPosition() + toIntSize(frameView->scrollOrigin());
        m_scrolledExposedRect.moveBy(scrollPositionWithOrigin);
    }
#endif

    frameView->setExposedRect(m_scrolledExposedRect);
    frameView->adjustTiledBackingCoverage();

    m_webPage->pageOverlayController().didChangeExposedRect();
}

TiledBacking* RemoteLayerTreeDrawingArea::mainFrameTiledBacking() const
{
    FrameView* frameView = m_webPage->corePage()->mainFrame().view();
    return frameView ? frameView->tiledBacking() : 0;
}

void RemoteLayerTreeDrawingArea::scheduleCompositingLayerFlush()
{
    if (m_layerFlushTimer.isActive())
        return;

    m_layerFlushTimer.startOneShot(0);
}

void RemoteLayerTreeDrawingArea::layerFlushTimerFired(WebCore::Timer<RemoteLayerTreeDrawingArea>*)
{
    flushLayers();
}

static void flushBackingStoreChangesInTransaction(RemoteLayerTreeTransaction& transaction)
{
    for (RefPtr<PlatformCALayerRemote> layer : transaction.changedLayers()) {
        if (!layer->properties().changedProperties & RemoteLayerTreeTransaction::BackingStoreChanged)
            return;

        if (RemoteLayerBackingStore* backingStore = layer->properties().backingStore.get())
            backingStore->flush();
    }
}

void RemoteLayerTreeDrawingArea::flushLayers()
{
    if (!m_rootLayer)
        return;

    if (m_isFlushingSuspended) {
        m_hasDeferredFlush = true;
        return;
    }

    if (m_waitingForBackingStoreSwap) {
        m_hadFlushDeferredWhileWaitingForBackingStoreSwap = true;
        return;
    }

    m_webPage->layoutIfNeeded();

    FloatRect visibleRect(FloatPoint(), m_viewSize);
    visibleRect.intersect(m_scrolledExposedRect);
    m_webPage->pageOverlayController().flushPageOverlayLayers(visibleRect);
    m_webPage->corePage()->mainFrame().view()->flushCompositingStateIncludingSubframes();
    m_rootLayer->flushCompositingStateForThisLayerOnly();

    // FIXME: minize these transactions if nothing changed.
    RemoteLayerTreeTransaction layerTransaction;
    m_remoteLayerTreeContext->buildTransaction(layerTransaction, *toGraphicsLayerCARemote(m_rootLayer.get())->platformCALayer());
    m_webPage->willCommitLayerTree(layerTransaction);

    RemoteScrollingCoordinatorTransaction scrollingTransaction;
#if ENABLE(ASYNC_SCROLLING)
    if (m_webPage->scrollingCoordinator())
        toRemoteScrollingCoordinator(m_webPage->scrollingCoordinator())->buildTransaction(scrollingTransaction);
#endif

    // FIXME: Move flushing backing store and sending CommitLayerTree onto a background thread.
    flushBackingStoreChangesInTransaction(layerTransaction);

    m_waitingForBackingStoreSwap = true;
    m_webPage->send(Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree(layerTransaction, scrollingTransaction));

    bool hadAnyChangedBackingStore = false;
    for (auto& layer : layerTransaction.changedLayers()) {
        if (layer->properties().changedProperties & RemoteLayerTreeTransaction::LayerChanges::BackingStoreChanged)
            hadAnyChangedBackingStore = true;
        layer->properties().resetChangedProperties();
    }

    if (hadAnyChangedBackingStore)
        m_remoteLayerTreeContext->backingStoreCollection().schedulePurgeabilityTimer();
}

void RemoteLayerTreeDrawingArea::didUpdate()
{
    // FIXME: This should use a counted replacement for setLayerTreeStateIsFrozen, but
    // the callers of that function are not strictly paired.

    m_waitingForBackingStoreSwap = false;

    if (m_hadFlushDeferredWhileWaitingForBackingStoreSwap) {
        scheduleCompositingLayerFlush();
        m_hadFlushDeferredWhileWaitingForBackingStoreSwap = false;
    }
}

void RemoteLayerTreeDrawingArea::mainFrameContentSizeChanged(const IntSize&)
{
    m_webPage->pageOverlayController().didChangeDocumentSize();
}

} // namespace WebKit
