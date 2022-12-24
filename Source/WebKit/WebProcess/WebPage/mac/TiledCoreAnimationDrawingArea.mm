/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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
#import "TiledCoreAnimationDrawingArea.h"

#if PLATFORM(MAC)

#import "DisplayRefreshMonitorMac.h"
#import "DrawingAreaProxyMessages.h"
#import "EventDispatcher.h"
#import "LayerHostingContext.h"
#import "LayerTreeContext.h"
#import "Logging.h"
#import "ViewGestureControllerMessages.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebPageCreationParameters.h"
#import "WebPageProxyMessages.h"
#import "WebPreferencesKeys.h"
#import "WebPreferencesStore.h"
#import "WebProcess.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DebugPageOverlays.h>
#import <WebCore/DestinationColorSpace.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/Page.h>
#import <WebCore/PlatformCAAnimationCocoa.h>
#import <WebCore/RenderView.h>
#import <WebCore/RunLoopObserver.h>
#import <WebCore/ScrollbarTheme.h>
#import <WebCore/Settings.h>
#import <WebCore/TiledBacking.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <WebCore/WindowEventLoop.h>
#import <wtf/MachSendRight.h>
#import <wtf/MainThread.h>
#import <wtf/SystemTracing.h>

#if ENABLE(ASYNC_SCROLLING)
#import <WebCore/AsyncScrollingCoordinator.h>
#import <WebCore/ScrollingThread.h>
#import <WebCore/ScrollingTree.h>
#endif

namespace WebKit {
using namespace WebCore;

TiledCoreAnimationDrawingArea::TiledCoreAnimationDrawingArea(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaType::TiledCoreAnimation, parameters.drawingAreaIdentifier, webPage)
    , m_isPaintingSuspended(!(parameters.activityState & ActivityState::IsVisible))
{
    m_webPage.corePage()->settings().setForceCompositingMode(true);

    m_hostingLayer = [CALayer layer];
    [m_hostingLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
    [m_hostingLayer setFrame:m_webPage.bounds()];
    [m_hostingLayer setOpaque:YES];
    [m_hostingLayer setGeometryFlipped:YES];

    m_renderingUpdateRunLoopObserver = makeUnique<RunLoopObserver>(static_cast<CFIndex>(RunLoopObserver::WellKnownRunLoopOrders::RenderingUpdate), [this]() {
        this->renderingUpdateRunLoopCallback();
    });

    m_postRenderingUpdateRunLoopObserver = makeUnique<RunLoopObserver>(static_cast<CFIndex>(RunLoopObserver::WellKnownRunLoopOrders::PostRenderingUpdate), [this]() {
        this->postRenderingUpdateRunLoopCallback();
    });

    updateLayerHostingContext();
    
    setColorSpace(parameters.colorSpace);

    if (auto viewExposedRect = parameters.viewExposedRect)
        setViewExposedRect(viewExposedRect);

    if (!parameters.isProcessSwap)
        sendEnterAcceleratedCompositingModeIfNeeded();
}

TiledCoreAnimationDrawingArea::~TiledCoreAnimationDrawingArea()
{
    invalidateRenderingUpdateRunLoopObserver();
    invalidatePostRenderingUpdateRunLoopObserver();
    for (auto& callback : m_nextActivityStateChangeCallbacks)
        callback();
}

void TiledCoreAnimationDrawingArea::sendDidFirstLayerFlushIfNeeded()
{
    if (!m_rootLayer)
        return;

    if (!m_needsSendDidFirstLayerFlush)
        return;
    m_needsSendDidFirstLayerFlush = false;

    // Let the first commit complete before sending.
    [CATransaction addCommitHandler:[this, weakThis = WeakPtr { *this }] {
        if (!weakThis || !m_layerHostingContext)
            return;
        LayerTreeContext layerTreeContext;
        layerTreeContext.contextID = m_layerHostingContext->contextID();
        send(Messages::DrawingAreaProxy::DidFirstLayerFlush(0, layerTreeContext));
    } forPhase:kCATransactionPhasePostCommit];
}

void TiledCoreAnimationDrawingArea::sendEnterAcceleratedCompositingModeIfNeeded()
{
    if (!m_needsSendEnterAcceleratedCompositingMode)
        return;
    m_needsSendEnterAcceleratedCompositingMode = false;

    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_layerHostingContext->contextID();
    send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, layerTreeContext));
}

void TiledCoreAnimationDrawingArea::registerScrollingTree()
{
    WebProcess::singleton().eventDispatcher().addScrollingTreeForPage(m_webPage);
}

void TiledCoreAnimationDrawingArea::unregisterScrollingTree()
{
    WebProcess::singleton().eventDispatcher().removeScrollingTreeForPage(m_webPage);
}

void TiledCoreAnimationDrawingArea::setNeedsDisplay()
{
}

void TiledCoreAnimationDrawingArea::setNeedsDisplayInRect(const IntRect& rect)
{
}

void TiledCoreAnimationDrawingArea::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    CALayer *rootLayer = graphicsLayer ? graphicsLayer->platformLayer() : nil;

    if (m_layerTreeStateIsFrozen) {
        m_pendingRootLayer = rootLayer;
        return;
    }

    m_pendingRootLayer = nullptr;
    setRootCompositingLayer(rootLayer);
}

void TiledCoreAnimationDrawingArea::forceRepaint()
{
    if (m_layerTreeStateIsFrozen)
        return;

    m_webPage.corePage()->forceRepaintAllFrames();
    updateRendering();
    [CATransaction flush];
    [CATransaction synchronize];
}

void TiledCoreAnimationDrawingArea::forceRepaintAsync(WebPage& page, CompletionHandler<void()>&& completionHandler)
{
    if (m_layerTreeStateIsFrozen) {
        page.forceRepaintWithoutCallback();
        return completionHandler();
    }

    dispatchAfterEnsuringUpdatedScrollPosition([this, weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis)
            return completionHandler();
        m_webPage.drawingArea()->forceRepaint();
        completionHandler();
    });
}

void TiledCoreAnimationDrawingArea::setLayerTreeStateIsFrozen(bool layerTreeStateIsFrozen)
{
    if (m_layerTreeStateIsFrozen == layerTreeStateIsFrozen)
        return;

    tracePoint(layerTreeStateIsFrozen ? LayerTreeFreezeStart : LayerTreeFreezeEnd);

    m_layerTreeStateIsFrozen = layerTreeStateIsFrozen;

    if (m_layerTreeStateIsFrozen) {
        invalidateRenderingUpdateRunLoopObserver();
        invalidatePostRenderingUpdateRunLoopObserver();
    } else {
        // Immediate flush as any delay in unfreezing can result in flashes.
        scheduleRenderingUpdateRunLoopObserver();
    }
}

bool TiledCoreAnimationDrawingArea::layerTreeStateIsFrozen() const
{
    return m_layerTreeStateIsFrozen;
}

void TiledCoreAnimationDrawingArea::triggerRenderingUpdate()
{
    if (m_layerTreeStateIsFrozen)
        return;

    scheduleRenderingUpdateRunLoopObserver();
}

void TiledCoreAnimationDrawingArea::updatePreferences(const WebPreferencesStore& store)
{
    Settings& settings = m_webPage.corePage()->settings();

    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(true);

    DebugPageOverlays::settingsChanged(*m_webPage.corePage());

    bool showTiledScrollingIndicator = settings.showTiledScrollingIndicator();
    if (showTiledScrollingIndicator == !!m_debugInfoLayer)
        return;

    updateDebugInfoLayer(showTiledScrollingIndicator);
}

void TiledCoreAnimationDrawingArea::updateRootLayers()
{
    if (!m_rootLayer) {
        [m_hostingLayer setSublayers:@[ ]];
        return;
    }

    [m_hostingLayer setSublayers:m_viewOverlayRootLayer ? @[ m_rootLayer.get(), m_viewOverlayRootLayer->platformLayer() ] : @[ m_rootLayer.get() ]];
    
    if (m_debugInfoLayer)
        [m_hostingLayer addSublayer:m_debugInfoLayer.get()];
}

void TiledCoreAnimationDrawingArea::attachViewOverlayGraphicsLayer(GraphicsLayer* viewOverlayRootLayer)
{
    m_viewOverlayRootLayer = viewOverlayRootLayer;
    updateRootLayers();
    triggerRenderingUpdate();
}

void TiledCoreAnimationDrawingArea::mainFrameContentSizeChanged(const IntSize& size)
{
}

void TiledCoreAnimationDrawingArea::setShouldScaleViewToFitDocument(bool shouldScaleView)
{
    if (m_shouldScaleViewToFitDocument == shouldScaleView)
        return;

    m_shouldScaleViewToFitDocument = shouldScaleView;
    triggerRenderingUpdate();
}

void TiledCoreAnimationDrawingArea::scaleViewToFitDocumentIfNeeded()
{
    const int maximumDocumentWidthForScaling = 1440;
    const float minimumViewScale = 0.1;

    if (!m_shouldScaleViewToFitDocument)
        return;

    LOG(Resize, "TiledCoreAnimationDrawingArea %p scaleViewToFitDocumentIfNeeded", this);
    m_webPage.layoutIfNeeded();

    if (!m_webPage.mainFrameView() || !m_webPage.mainFrameView()->renderView())
        return;

    int viewWidth = m_webPage.size().width();
    int documentWidth = m_webPage.mainFrameView()->renderView()->unscaledDocumentRect().width();

    bool documentWidthChanged = m_lastDocumentSizeForScaleToFit.width() != documentWidth;
    bool viewWidthChanged = m_lastViewSizeForScaleToFit.width() != viewWidth;

    LOG(Resize, "  documentWidthChanged=%d, viewWidthChanged=%d", documentWidthChanged, viewWidthChanged);

    if (!documentWidthChanged && !viewWidthChanged)
        return;

    // The view is now bigger than the document, so we'll re-evaluate whether we have to scale.
    if (m_isScalingViewToFitDocument && viewWidth >= m_lastDocumentSizeForScaleToFit.width())
        m_isScalingViewToFitDocument = false;

    // Our current understanding of the document width is still up to date, and we're in scaling mode.
    // Update the viewScale without doing an extra layout to re-determine the document width.
    if (m_isScalingViewToFitDocument) {
        if (!documentWidthChanged) {
            m_lastViewSizeForScaleToFit = m_webPage.size();
            float viewScale = (float)viewWidth / (float)m_lastDocumentSizeForScaleToFit.width();
            if (viewScale < minimumViewScale) {
                viewScale = minimumViewScale;
                documentWidth = std::ceil(viewWidth / viewScale);
            }
            IntSize fixedLayoutSize(documentWidth, std::ceil((m_webPage.size().height() - m_webPage.corePage()->topContentInset()) / viewScale));
            m_webPage.setFixedLayoutSize(fixedLayoutSize);
            m_webPage.scaleView(viewScale);

            LOG(Resize, "  using fixed layout at %dx%d. document width %d unchanged, scaled to %.4f to fit view width %d", fixedLayoutSize.width(), fixedLayoutSize.height(), documentWidth, viewScale, viewWidth);
            return;
        }
    
        IntSize fixedLayoutSize = m_webPage.fixedLayoutSize();
        if (documentWidth > fixedLayoutSize.width()) {
            LOG(Resize, "  page laid out wider than fixed layout width. Not attempting to re-scale");
            return;
        }
    }

    LOG(Resize, "  doing unconstrained layout");

    // Lay out at the view size.
    m_webPage.setUseFixedLayout(false);
    m_webPage.layoutIfNeeded();

    if (!m_webPage.mainFrameView() || !m_webPage.mainFrameView()->renderView())
        return;

    IntSize documentSize = m_webPage.mainFrameView()->renderView()->unscaledDocumentRect().size();
    m_lastViewSizeForScaleToFit = m_webPage.size();
    m_lastDocumentSizeForScaleToFit = documentSize;

    documentWidth = documentSize.width();

    float viewScale = 1;

    LOG(Resize, "  unscaled document size %dx%d. need to scale down: %d", documentSize.width(), documentSize.height(), documentWidth && documentWidth < maximumDocumentWidthForScaling && viewWidth < documentWidth);

    // Avoid scaling down documents that don't fit in a certain width, to allow
    // sites that want horizontal scrollbars to continue to have them.
    if (documentWidth && documentWidth < maximumDocumentWidthForScaling && viewWidth < documentWidth) {
        // If the document doesn't fit in the view, scale it down but lay out at the view size.
        m_isScalingViewToFitDocument = true;
        m_webPage.setUseFixedLayout(true);
        viewScale = (float)viewWidth / (float)documentWidth;
        if (viewScale < minimumViewScale) {
            viewScale = minimumViewScale;
            documentWidth = std::ceil(viewWidth / viewScale);
        }
        IntSize fixedLayoutSize(documentWidth, std::ceil((m_webPage.size().height() - m_webPage.corePage()->topContentInset()) / viewScale));
        m_webPage.setFixedLayoutSize(fixedLayoutSize);

        LOG(Resize, "  using fixed layout at %dx%d. document width %d, scaled to %.4f to fit view width %d", fixedLayoutSize.width(), fixedLayoutSize.height(), documentWidth, viewScale, viewWidth);
    }

    m_webPage.scaleView(viewScale);
}

void TiledCoreAnimationDrawingArea::dispatchAfterEnsuringUpdatedScrollPosition(WTF::Function<void ()>&& function)
{
#if ENABLE(ASYNC_SCROLLING)
    if (!m_webPage.corePage()->scrollingCoordinator()) {
        function();
        return;
    }

    m_webPage.corePage()->scrollingCoordinator()->commitTreeStateIfNeeded();

    if (!m_layerTreeStateIsFrozen) {
        invalidateRenderingUpdateRunLoopObserver();
        invalidatePostRenderingUpdateRunLoopObserver();
    }

    ScrollingThread::dispatchBarrier([this, retainedPage = Ref { m_webPage }, function = WTFMove(function)] {
        // It is possible for the drawing area to be destroyed before the bound block is invoked.
        if (!retainedPage->drawingArea())
            return;

        function();

        if (!m_layerTreeStateIsFrozen)
            scheduleRenderingUpdateRunLoopObserver();
    });
#else
    function();
#endif
}

void TiledCoreAnimationDrawingArea::sendPendingNewlyReachedPaintingMilestones()
{
    if (!m_pendingNewlyReachedPaintingMilestones)
        return;

    m_webPage.send(Messages::WebPageProxy::DidReachLayoutMilestone(std::exchange(m_pendingNewlyReachedPaintingMilestones, { })));
}

void TiledCoreAnimationDrawingArea::addTransactionCallbackID(CallbackID callbackID)
{
    m_pendingCallbackIDs.append(callbackID);
    triggerRenderingUpdate();
}

void TiledCoreAnimationDrawingArea::didCompleteRenderingUpdateDisplay()
{
    m_haveRegisteredHandlersForNextCommit = false;

    sendPendingNewlyReachedPaintingMilestones();
    DrawingArea::didCompleteRenderingUpdateDisplay();
    
    schedulePostRenderingUpdateRunLoopObserver();
}

void TiledCoreAnimationDrawingArea::addCommitHandlers()
{
    if (m_haveRegisteredHandlersForNextCommit)
        return;

    [CATransaction addCommitHandler:[retainedPage = Ref { m_webPage }] {
        if (auto* drawingArea = dynamicDowncast<TiledCoreAnimationDrawingArea>(retainedPage->drawingArea()))
            drawingArea->willStartRenderingUpdateDisplay();
    } forPhase:kCATransactionPhasePreLayout];

    [CATransaction addCommitHandler:[retainedPage = Ref { m_webPage }] {
        if (auto* drawingArea = dynamicDowncast<TiledCoreAnimationDrawingArea>(retainedPage->drawingArea()))
            drawingArea->didCompleteRenderingUpdateDisplay();
    } forPhase:kCATransactionPhasePostCommit];
    
    m_haveRegisteredHandlersForNextCommit = true;
}

void TiledCoreAnimationDrawingArea::updateRendering(UpdateRenderingType flushType)
{
    if (layerTreeStateIsFrozen())
        return;

    @autoreleasepool {
        scaleViewToFitDocumentIfNeeded();

        m_webPage.updateRendering();
        m_webPage.flushPendingThemeColorChange();
        m_webPage.flushPendingPageExtendedBackgroundColorChange();
        m_webPage.flushPendingSampledPageTopColorChange();
        m_webPage.flushPendingEditorStateUpdate();
        m_webPage.flushPendingIntrinsicContentSizeUpdate();

        if (m_pendingRootLayer) {
            setRootCompositingLayer(m_pendingRootLayer.get());
            m_pendingRootLayer = nullptr;
        }

        FloatRect visibleRect = [m_hostingLayer frame];
        if (auto exposedRect = m_webPage.mainFrameView()->viewExposedRect())
            visibleRect.intersect(*exposedRect);

        // Because our view-relative overlay root layer is not attached to the main GraphicsLayer tree, we need to flush it manually.
        if (m_viewOverlayRootLayer)
            m_viewOverlayRootLayer->flushCompositingState(visibleRect);

        addCommitHandlers();

        OptionSet<FinalizeRenderingUpdateFlags> flags;
        if (flushType == UpdateRenderingType::Normal)
            flags.add(FinalizeRenderingUpdateFlags::ApplyScrollingTreeLayerPositions);

        m_webPage.finalizeRenderingUpdate(flags);

        // If we have an active transient zoom, we want the zoom to win over any changes
        // that WebCore makes to the relevant layers, so re-apply our changes after flushing.
        if (m_transientZoomScale != 1)
            applyTransientZoomToLayers(m_transientZoomScale, m_transientZoomOrigin);

        if (!m_pendingCallbackIDs.isEmpty()) {
            send(Messages::DrawingAreaProxy::DispatchPresentationCallbacksAfterFlushingLayers(m_pendingCallbackIDs));
            m_pendingCallbackIDs.clear();
        }

        sendDidFirstLayerFlushIfNeeded();
        m_webPage.didUpdateRendering();
        handleActivityStateChangeCallbacksIfNeeded();
        invalidateRenderingUpdateRunLoopObserver();
    }
}

void TiledCoreAnimationDrawingArea::handleActivityStateChangeCallbacks()
{
    if (!m_shouldHandleActivityStateChangeCallbacks)
        return;
    m_shouldHandleActivityStateChangeCallbacks = false;

    if (m_activityStateChangeID != ActivityStateChangeAsynchronous)
        m_webPage.send(Messages::WebPageProxy::DidUpdateActivityState());

    for (auto& callback : std::exchange(m_nextActivityStateChangeCallbacks, { }))
        callback();

    m_activityStateChangeID = ActivityStateChangeAsynchronous;
}

void TiledCoreAnimationDrawingArea::handleActivityStateChangeCallbacksIfNeeded()
{
    if (!m_shouldHandleActivityStateChangeCallbacks)
        return;

    // If there is no active transaction, likely there is no layer change or change is committed,
    // perform the callbacks immediately, which may unblock UI process.
    if (![CATransaction currentState]) {
        handleActivityStateChangeCallbacks();
        return;
    }

    [CATransaction addCommitHandler:[weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;

        Ref protectedPage = weakThis->m_webPage;
        auto* drawingArea = static_cast<TiledCoreAnimationDrawingArea*>(protectedPage->drawingArea());
        ASSERT(weakThis.get() == drawingArea);
        if (drawingArea != weakThis.get())
            return;

        drawingArea->handleActivityStateChangeCallbacks();
    } forPhase:kCATransactionPhasePostCommit];
}

void TiledCoreAnimationDrawingArea::activityStateDidChange(OptionSet<ActivityState::Flag> changed, ActivityStateChangeID activityStateChangeID, CompletionHandler<void()>&& nextActivityStateChangeCallback)
{
    m_nextActivityStateChangeCallbacks.append(WTFMove(nextActivityStateChangeCallback));
    m_activityStateChangeID = std::max(m_activityStateChangeID, activityStateChangeID);

    if (changed & ActivityState::IsVisible) {
        if (m_webPage.isVisible())
            resumePainting();
        else
            suspendPainting();
    }

    if (m_activityStateChangeID != ActivityStateChangeAsynchronous || !m_nextActivityStateChangeCallbacks.isEmpty()) {
        m_shouldHandleActivityStateChangeCallbacks = true;
        triggerRenderingUpdate();
    }
}

void TiledCoreAnimationDrawingArea::suspendPainting()
{
    ASSERT(!m_isPaintingSuspended);
    m_isPaintingSuspended = true;

    // This is a signal to media frameworks; it does not actively pause anything.
    [m_hostingLayer setValue:@YES forKey:@"NSCAViewRenderPaused"];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"NSCAViewRenderDidPauseNotification" object:nil userInfo:@{ @"layer": m_hostingLayer.get() }];
}

void TiledCoreAnimationDrawingArea::resumePainting()
{
    if (!m_isPaintingSuspended) {
        // FIXME: We can get a call to resumePainting when painting is not suspended.
        // This happens when sending a synchronous message to create a new page. See <rdar://problem/8976531>.
        return;
    }
    m_isPaintingSuspended = false;

    [m_hostingLayer setValue:@NO forKey:@"NSCAViewRenderPaused"];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"NSCAViewRenderDidResumeNotification" object:nil userInfo:@{ @"layer": m_hostingLayer.get() }];
}

void TiledCoreAnimationDrawingArea::setViewExposedRect(std::optional<FloatRect> viewExposedRect)
{
    m_viewExposedRect = viewExposedRect;

    if (FrameView* frameView = m_webPage.mainFrameView())
        frameView->setViewExposedRect(m_viewExposedRect);
}

FloatRect TiledCoreAnimationDrawingArea::exposedContentRect() const
{
    ASSERT_NOT_REACHED();
    return { };
}

void TiledCoreAnimationDrawingArea::setExposedContentRect(const FloatRect&)
{
    ASSERT_NOT_REACHED();
}

void TiledCoreAnimationDrawingArea::updateGeometry(const IntSize& viewSize, bool flushSynchronously, const WTF::MachSendRight& fencePort)
{
    m_inUpdateGeometry = true;

    IntSize size = viewSize;
    IntSize contentSize = IntSize(-1, -1);

    if (!m_webPage.minimumSizeForAutoLayout().width() || m_webPage.autoSizingShouldExpandToViewHeight() || (!m_webPage.sizeToContentAutoSizeMaximumSize().width() && !m_webPage.sizeToContentAutoSizeMaximumSize().height()))
        m_webPage.setSize(size);

    FrameView* frameView = m_webPage.mainFrameView();

    if (m_webPage.autoSizingShouldExpandToViewHeight() && frameView)
        frameView->setAutoSizeFixedMinimumHeight(viewSize.height());

    m_webPage.layoutIfNeeded();

    if (frameView && (m_webPage.minimumSizeForAutoLayout().width() || (m_webPage.sizeToContentAutoSizeMaximumSize().width() && m_webPage.sizeToContentAutoSizeMaximumSize().height()))) {
        contentSize = frameView->autoSizingIntrinsicContentSize();
        size = contentSize;
    }

    updateRendering();

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [m_hostingLayer setFrame:CGRectMake(0, 0, viewSize.width(), viewSize.height())];

    [CATransaction commit];

    if (flushSynchronously)
        [CATransaction flush];

    send(Messages::DrawingAreaProxy::DidUpdateGeometry());

    m_inUpdateGeometry = false;

    m_layerHostingContext->setFencePort(fencePort.sendRight());
}

void TiledCoreAnimationDrawingArea::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage.setDeviceScaleFactor(deviceScaleFactor);
}

void TiledCoreAnimationDrawingArea::setLayerHostingMode(LayerHostingMode)
{
    updateLayerHostingContext();

    // Finally, inform the UIProcess that the context has changed.
    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_layerHostingContext->contextID();
    send(Messages::DrawingAreaProxy::UpdateAcceleratedCompositingMode(0, layerTreeContext));
}

void TiledCoreAnimationDrawingArea::setColorSpace(std::optional<WebCore::DestinationColorSpace> colorSpace)
{
    m_layerHostingContext->setColorSpace(colorSpace ? colorSpace->platformColorSpace() : nullptr);
}

std::optional<WebCore::DestinationColorSpace> TiledCoreAnimationDrawingArea::displayColorSpace() const
{
    return DestinationColorSpace { m_layerHostingContext->colorSpace() };
}

RefPtr<WebCore::DisplayRefreshMonitor> TiledCoreAnimationDrawingArea::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    return DisplayRefreshMonitorMac::create(displayID);
}

void TiledCoreAnimationDrawingArea::updateLayerHostingContext()
{
    RetainPtr<CGColorSpaceRef> colorSpace;

    // Invalidate the old context.
    if (m_layerHostingContext) {
        colorSpace = m_layerHostingContext->colorSpace();
        m_layerHostingContext->invalidate();
        m_layerHostingContext = nullptr;
    }

    // Create a new context and set it up.
    switch (m_webPage.layerHostingMode()) {
    case LayerHostingMode::InProcess:
        m_layerHostingContext = LayerHostingContext::createForPort(WebProcess::singleton().compositingRenderServerPort());
        break;
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    case LayerHostingMode::OutOfProcess:
        m_layerHostingContext = LayerHostingContext::createForExternalHostingProcess();
        break;
#endif
    }

    if (m_rootLayer)
        m_layerHostingContext->setRootLayer(m_hostingLayer.get());

    if (colorSpace)
        m_layerHostingContext->setColorSpace(colorSpace.get());
}

void TiledCoreAnimationDrawingArea::setRootCompositingLayer(CALayer *layer)
{
    ASSERT(!m_layerTreeStateIsFrozen);

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    bool hadRootLayer = !!m_rootLayer;
    m_rootLayer = layer;

    updateRootLayers();

    if (hadRootLayer != !!layer)
        m_layerHostingContext->setRootLayer(layer ? m_hostingLayer.get() : nil);

    updateDebugInfoLayer(layer && m_webPage.corePage()->settings().showTiledScrollingIndicator());

    [CATransaction commit];
}

TiledBacking* TiledCoreAnimationDrawingArea::mainFrameTiledBacking() const
{
    FrameView* frameView = m_webPage.mainFrameView();
    return frameView ? frameView->tiledBacking() : nullptr;
}

void TiledCoreAnimationDrawingArea::updateDebugInfoLayer(bool showLayer)
{
    if (m_debugInfoLayer) {
        [m_debugInfoLayer removeFromSuperlayer];
        m_debugInfoLayer = nil;
    }
    
    if (showLayer) {
        if (auto* tiledBacking = mainFrameTiledBacking()) {
            if (PlatformCALayer* indicatorLayer = tiledBacking->tiledScrollingIndicatorLayer())
                m_debugInfoLayer = indicatorLayer->platformLayer();
        }

        if (m_debugInfoLayer) {
            [m_debugInfoLayer setName:@"Debug Info"];
            [m_hostingLayer addSublayer:m_debugInfoLayer.get()];
        }
    }
}

bool TiledCoreAnimationDrawingArea::shouldUseTiledBackingForFrameView(const FrameView& frameView) const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    return (localFrame && localFrame->isMainFrame())
        || m_webPage.corePage()->settings().asyncFrameScrollingEnabled();
}

PlatformCALayer* TiledCoreAnimationDrawingArea::layerForTransientZoom() const
{
    auto* scaledLayer = dynamicDowncast<GraphicsLayerCA>(m_webPage.mainFrameView()->graphicsLayerForPageScale());
    if (!scaledLayer)
        return nullptr;

    return scaledLayer->platformCALayer();
}

PlatformCALayer* TiledCoreAnimationDrawingArea::shadowLayerForTransientZoom() const
{
    auto* shadowLayer = dynamicDowncast<GraphicsLayerCA>(m_webPage.mainFrameView()->graphicsLayerForTransientZoomShadow());
    if (!shadowLayer)
        return nullptr;

    return shadowLayer->platformCALayer();
}
    
static FloatPoint shadowLayerPositionForFrame(FrameView& frameView, FloatPoint origin)
{
    // FIXME: correct for b-t documents?
    FloatPoint position = frameView.positionForRootContentLayer();
    return position + origin.expandedTo(FloatPoint());
}

static FloatRect shadowLayerBoundsForFrame(FrameView& frameView, float transientScale)
{
    FloatRect clipLayerFrame(frameView.renderView()->documentRect());
    FloatRect shadowLayerFrame = clipLayerFrame;
    
    shadowLayerFrame.scale(transientScale / frameView.frame().page()->pageScaleFactor());
    shadowLayerFrame.intersect(clipLayerFrame);
    
    return shadowLayerFrame;
}

void TiledCoreAnimationDrawingArea::applyTransientZoomToLayers(double scale, FloatPoint origin)
{
    // FIXME: Scrollbars should stay in-place and change height while zooming.

    if (!m_hostingLayer)
        return;

    TransformationMatrix transform;
    transform.translate(origin.x(), origin.y());
    transform.scale(scale);

    PlatformCALayer* zoomLayer = layerForTransientZoom();
    zoomLayer->setTransform(transform);
    zoomLayer->setAnchorPoint(FloatPoint3D());
    zoomLayer->setPosition(FloatPoint3D());
    
    if (PlatformCALayer* shadowLayer = shadowLayerForTransientZoom()) {
        FrameView& frameView = *m_webPage.mainFrameView();
        shadowLayer->setBounds(shadowLayerBoundsForFrame(frameView, scale));
        shadowLayer->setPosition(shadowLayerPositionForFrame(frameView, origin));
    }

    m_transientZoomScale = scale;
    m_transientZoomOrigin = origin;
}

void TiledCoreAnimationDrawingArea::adjustTransientZoom(double scale, FloatPoint origin)
{
    scale *= m_webPage.viewScaleFactor();

    applyTransientZoomToLayers(scale, origin);

    double currentPageScale = m_webPage.totalScaleFactor();
    if (scale > currentPageScale)
        return;

    auto* frameView = m_webPage.mainFrameView();
    FloatRect tileCoverageRect = frameView->visibleContentRectIncludingScrollbars();
    tileCoverageRect.moveBy(-origin);
    tileCoverageRect.scale(currentPageScale / scale);
    
    if (auto* tiledBacking = mainFrameTiledBacking())
        tiledBacking->prepopulateRect(tileCoverageRect);
}

static RetainPtr<CABasicAnimation> transientZoomSnapAnimationForKeyPath(ASCIILiteral keyPath)
{
    const float transientZoomSnapBackDuration = 0.25;

    RetainPtr<CABasicAnimation> animation = [CABasicAnimation animationWithKeyPath:keyPath.createNSString().get()];
    [animation setDuration:transientZoomSnapBackDuration];
    [animation setFillMode:kCAFillModeForwards];
    [animation setRemovedOnCompletion:false];
    [animation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut]];

    return animation;
}

void TiledCoreAnimationDrawingArea::commitTransientZoom(double scale, FloatPoint origin)
{
    scale *= m_webPage.viewScaleFactor();

    FrameView& frameView = *m_webPage.mainFrameView();
    FloatRect visibleContentRect = frameView.visibleContentRectIncludingScrollbars();

    FloatPoint constrainedOrigin = visibleContentRect.location();
    constrainedOrigin.moveBy(-origin);

    IntSize scaledTotalContentsSize = frameView.totalContentsSize();
    scaledTotalContentsSize.scale(scale / m_webPage.totalScaleFactor());

    // Scaling may have exposed the overhang area, so we need to constrain the final
    // layer position exactly like scrolling will once it's committed, to ensure that
    // scrolling doesn't make the view jump.
    constrainedOrigin = ScrollableArea::constrainScrollPositionForOverhang(roundedIntRect(visibleContentRect), scaledTotalContentsSize, roundedIntPoint(constrainedOrigin), frameView.scrollOrigin(), frameView.headerHeight(), frameView.footerHeight());
    constrainedOrigin.moveBy(-visibleContentRect.location());
    constrainedOrigin = -constrainedOrigin;

    if (m_transientZoomScale == scale && roundedIntPoint(m_transientZoomOrigin) == roundedIntPoint(constrainedOrigin)) {
        // We're already at the right scale and position, so we don't need to animate.
        applyTransientZoomToPage(scale, origin);
        return;
    }

    TransformationMatrix transform;
    transform.translate(constrainedOrigin.x(), constrainedOrigin.y());
    transform.scale(scale);

    RetainPtr<CABasicAnimation> renderViewAnimationCA = transientZoomSnapAnimationForKeyPath("transform"_s);
    auto renderViewAnimation = PlatformCAAnimationCocoa::create(renderViewAnimationCA.get());
    renderViewAnimation->setToValue(transform);

    RetainPtr<CALayer> shadowCALayer;
    if (PlatformCALayer* shadowLayer = shadowLayerForTransientZoom())
        shadowCALayer = shadowLayer->platformLayer();

    RefPtr<PlatformCALayer> zoomLayer = layerForTransientZoom();
    RefPtr<WebPage> page = &m_webPage;

    [CATransaction begin];
    [CATransaction setCompletionBlock:[zoomLayer, shadowCALayer, page, scale, origin] () {
        zoomLayer->removeAnimationForKey("transientZoomCommit"_s);
        if (shadowCALayer)
            [shadowCALayer removeAllAnimations];

        if (TiledCoreAnimationDrawingArea* drawingArea = static_cast<TiledCoreAnimationDrawingArea*>(page->drawingArea()))
            drawingArea->applyTransientZoomToPage(scale, origin);
    }];

    zoomLayer->addAnimationForKey("transientZoomCommit"_s, renderViewAnimation.get());

    if (shadowCALayer) {
        FloatRect shadowBounds = shadowLayerBoundsForFrame(frameView, scale);
        RetainPtr<CGPathRef> shadowPath = adoptCF(CGPathCreateWithRect(shadowBounds, NULL));

        RetainPtr<CABasicAnimation> shadowBoundsAnimation = transientZoomSnapAnimationForKeyPath("bounds"_s);
        [shadowBoundsAnimation setToValue:[NSValue valueWithRect:shadowBounds]];
        RetainPtr<CABasicAnimation> shadowPositionAnimation = transientZoomSnapAnimationForKeyPath("position"_s);
        [shadowPositionAnimation setToValue:[NSValue valueWithPoint:shadowLayerPositionForFrame(frameView, constrainedOrigin)]];
        RetainPtr<CABasicAnimation> shadowPathAnimation = transientZoomSnapAnimationForKeyPath("shadowPath"_s);
        [shadowPathAnimation setToValue:(__bridge id)shadowPath.get()];

        [shadowCALayer addAnimation:shadowBoundsAnimation.get() forKey:@"transientZoomCommitShadowBounds"];
        [shadowCALayer addAnimation:shadowPositionAnimation.get() forKey:@"transientZoomCommitShadowPosition"];
        [shadowCALayer addAnimation:shadowPathAnimation.get() forKey:@"transientZoomCommitShadowPath"];
    }

    [CATransaction commit];
}

void TiledCoreAnimationDrawingArea::applyTransientZoomToPage(double scale, FloatPoint origin)
{
    // If the page scale is already the target scale, setPageScaleFactor() will short-circuit
    // and not apply the transform, so we can't depend on it to do so.
    TransformationMatrix finalTransform;
    finalTransform.scale(scale);
    layerForTransientZoom()->setTransform(finalTransform);
    
    FrameView& frameView = *m_webPage.mainFrameView();

    if (PlatformCALayer* shadowLayer = shadowLayerForTransientZoom()) {
        shadowLayer->setBounds(shadowLayerBoundsForFrame(frameView, 1));
        shadowLayer->setPosition(shadowLayerPositionForFrame(frameView, FloatPoint()));
    }

    FloatPoint unscrolledOrigin(origin);
    FloatRect unobscuredContentRect = frameView.unobscuredContentRectIncludingScrollbars();
    unscrolledOrigin.moveBy(-unobscuredContentRect.location());
    m_webPage.scalePage(scale / m_webPage.viewScaleFactor(), roundedIntPoint(-unscrolledOrigin));
    m_transientZoomScale = 1;
    updateRendering(UpdateRenderingType::TransientZoom);
}

void TiledCoreAnimationDrawingArea::addFence(const MachSendRight& fencePort)
{
    m_layerHostingContext->setFencePort(fencePort.sendRight());
}

void TiledCoreAnimationDrawingArea::scheduleRenderingUpdateRunLoopObserver()
{
    if (m_renderingUpdateRunLoopObserver->isScheduled())
        return;

    tracePoint(RenderingUpdateRunLoopObserverStart);
    
    m_renderingUpdateRunLoopObserver->schedule();

    // Avoid running any more tasks before the runloop observer fires.
    WebCore::WindowEventLoop::breakToAllowRenderingUpdate();
}

void TiledCoreAnimationDrawingArea::invalidateRenderingUpdateRunLoopObserver()
{
    if (!m_renderingUpdateRunLoopObserver->isScheduled())
        return;

    tracePoint(RenderingUpdateRunLoopObserverEnd, 1);

    m_renderingUpdateRunLoopObserver->invalidate();
}

void TiledCoreAnimationDrawingArea::renderingUpdateRunLoopCallback()
{
    tracePoint(RenderingUpdateRunLoopObserverEnd, 0);

    updateRendering();
}

void TiledCoreAnimationDrawingArea::schedulePostRenderingUpdateRunLoopObserver()
{
    if (m_postRenderingUpdateRunLoopObserver->isScheduled())
        return;

    m_postRenderingUpdateRunLoopObserver->schedule();
}

void TiledCoreAnimationDrawingArea::invalidatePostRenderingUpdateRunLoopObserver()
{
    if (!m_postRenderingUpdateRunLoopObserver->isScheduled())
        return;

    m_postRenderingUpdateRunLoopObserver->invalidate();
}

void TiledCoreAnimationDrawingArea::postRenderingUpdateRunLoopCallback()
{
    didCompleteRenderingFrame();
    invalidatePostRenderingUpdateRunLoopObserver();
}

} // namespace WebKit

#endif // PLATFORM(MAC)
