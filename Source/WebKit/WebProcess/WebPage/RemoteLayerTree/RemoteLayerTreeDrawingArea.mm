/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeDisplayRefreshMonitor.h"
#import "RemoteLayerTreeDrawingAreaProxyMessages.h"
#import "RemoteScrollingCoordinator.h"
#import "RemoteScrollingCoordinatorTransaction.h"
#import "WebPage.h"
#import "WebPageCreationParameters.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DebugPageOverlays.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/InspectorController.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/Settings.h>
#import <WebCore/TiledBacking.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/SetForScope.h>
#import <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;

RemoteLayerTreeDrawingArea::RemoteLayerTreeDrawingArea(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaTypeRemoteLayerTree, parameters.drawingAreaIdentifier, webPage)
    , m_remoteLayerTreeContext(makeUnique<RemoteLayerTreeContext>(webPage))
    , m_rootLayer(GraphicsLayer::create(graphicsLayerFactory(), *this))
    , m_updateRenderingTimer(*this, &RemoteLayerTreeDrawingArea::updateRendering)
{
    webPage.corePage()->settings().setForceCompositingMode(true);
    m_rootLayer->setName("drawing area root");

    m_commitQueue = dispatch_queue_create("com.apple.WebKit.WebContent.RemoteLayerTreeDrawingArea.CommitQueue", nullptr);

    // In order to ensure that we get a unique DisplayRefreshMonitor per-DrawingArea (necessary because DisplayRefreshMonitor
    // is driven by this class), give each page a unique DisplayID derived from WebPage's unique ID.
    // FIXME: While using the high end of the range of DisplayIDs makes a collision with real, non-RemoteLayerTreeDrawingArea
    // DisplayIDs less likely, it is not entirely safe to have a RemoteLayerTreeDrawingArea and TiledCoreAnimationDrawingArea
    // coeexist in the same process.
    webPage.windowScreenDidChange(std::numeric_limits<uint32_t>::max() - webPage.identifier().toUInt64(), WTF::nullopt);

    if (auto viewExposedRect = parameters.viewExposedRect)
        setViewExposedRect(viewExposedRect);
}

RemoteLayerTreeDrawingArea::~RemoteLayerTreeDrawingArea()
{
    dispatch_release(m_commitQueue);
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

RefPtr<DisplayRefreshMonitor> RemoteLayerTreeDrawingArea::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    auto monitor = RemoteLayerTreeDisplayRefreshMonitor::create(displayID, *this);
    m_displayRefreshMonitors.add(monitor.ptr());
    return WTFMove(monitor);
}

void RemoteLayerTreeDrawingArea::willDestroyDisplayRefreshMonitor(DisplayRefreshMonitor* monitor)
{
    auto remoteMonitor = static_cast<RemoteLayerTreeDisplayRefreshMonitor*>(monitor);
    m_displayRefreshMonitors.remove(remoteMonitor);

    if (m_displayRefreshMonitorsToNotify)
        m_displayRefreshMonitorsToNotify->remove(remoteMonitor);
}

void RemoteLayerTreeDrawingArea::adoptDisplayRefreshMonitorsFromDrawingArea(DrawingArea& drawingArea)
{
    if (is<RemoteLayerTreeDrawingArea>(drawingArea)) {
        auto& otherDrawingArea = downcast<RemoteLayerTreeDrawingArea>(drawingArea);
        m_displayRefreshMonitors = WTFMove(otherDrawingArea.m_displayRefreshMonitors);
        for (auto* monitor : m_displayRefreshMonitors)
            monitor->updateDrawingArea(*this);
    }
}

void RemoteLayerTreeDrawingArea::setPreferredFramesPerSecond(FramesPerSecond preferredFramesPerSecond)
{
    send(Messages::RemoteLayerTreeDrawingAreaProxy::SetPreferredFramesPerSecond(preferredFramesPerSecond));
}

void RemoteLayerTreeDrawingArea::updateRootLayers()
{
    Vector<Ref<GraphicsLayer>> children;
    if (m_contentLayer) {
        children.append(*m_contentLayer);
        if (m_viewOverlayRootLayer)
            children.append(*m_viewOverlayRootLayer);
    }

    m_rootLayer->setChildren(WTFMove(children));
}

void RemoteLayerTreeDrawingArea::attachViewOverlayGraphicsLayer(GraphicsLayer* viewOverlayRootLayer)
{
    m_viewOverlayRootLayer = viewOverlayRootLayer;
    updateRootLayers();
}

void RemoteLayerTreeDrawingArea::setRootCompositingLayer(GraphicsLayer* rootLayer)
{
    m_contentLayer = rootLayer;
    updateRootLayers();
    scheduleRenderingUpdate();
}

void RemoteLayerTreeDrawingArea::updateGeometry(const IntSize& viewSize, bool flushSynchronously, const WTF::MachSendRight&)
{
    m_viewSize = viewSize;
    m_webPage.setSize(viewSize);

    scheduleRenderingUpdate();

    send(Messages::DrawingAreaProxy::DidUpdateGeometry());
}

bool RemoteLayerTreeDrawingArea::shouldUseTiledBackingForFrameView(const FrameView& frameView) const
{
    return frameView.frame().isMainFrame() || m_webPage.corePage()->settings().asyncFrameScrollingEnabled();
}

void RemoteLayerTreeDrawingArea::updatePreferences(const WebPreferencesStore&)
{
    Settings& settings = m_webPage.corePage()->settings();

    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(true);

    m_rootLayer->setShowDebugBorder(settings.showDebugBorders());

    DebugPageOverlays::settingsChanged(*m_webPage.corePage());
}

#if PLATFORM(IOS_FAMILY)
void RemoteLayerTreeDrawingArea::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage.setDeviceScaleFactor(deviceScaleFactor);
}
#endif

void RemoteLayerTreeDrawingArea::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_isRenderingSuspended == isFrozen)
        return;

    tracePoint(isFrozen ? LayerTreeFreezeStart : LayerTreeFreezeEnd);

    m_isRenderingSuspended = isFrozen;

    if (!m_isRenderingSuspended && m_hasDeferredRenderingUpdate) {
        m_hasDeferredRenderingUpdate = false;
        scheduleImmediateRenderingUpdate();
    }
}

void RemoteLayerTreeDrawingArea::forceRepaint()
{
    if (m_isRenderingSuspended)
        return;

    for (Frame* frame = &m_webPage.corePage()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        FrameView* frameView = frame->view();
        if (!frameView || !frameView->tiledBacking())
            continue;

        frameView->tiledBacking()->forceRepaint();
    }

    updateRendering();
}

void RemoteLayerTreeDrawingArea::acceleratedAnimationDidStart(uint64_t layerID, const String& key, MonotonicTime startTime)
{
    m_remoteLayerTreeContext->animationDidStart(layerID, key, startTime);
}

void RemoteLayerTreeDrawingArea::acceleratedAnimationDidEnd(uint64_t layerID, const String& key)
{
    m_remoteLayerTreeContext->animationDidEnd(layerID, key);
}

void RemoteLayerTreeDrawingArea::setViewExposedRect(Optional<WebCore::FloatRect> viewExposedRect)
{
    m_viewExposedRect = viewExposedRect;

    if (FrameView* frameView = m_webPage.mainFrameView())
        frameView->setViewExposedRect(m_viewExposedRect);
}

WebCore::FloatRect RemoteLayerTreeDrawingArea::exposedContentRect() const
{
    FrameView* frameView = m_webPage.mainFrameView();
    if (!frameView)
        return FloatRect();

    return frameView->exposedContentRect();
}

void RemoteLayerTreeDrawingArea::setExposedContentRect(const FloatRect& exposedContentRect)
{
    FrameView* frameView = m_webPage.mainFrameView();
    if (!frameView)
        return;
    if (frameView->exposedContentRect() == exposedContentRect)
        return;

    frameView->setExposedContentRect(exposedContentRect);
    scheduleRenderingUpdate();
}

TiledBacking* RemoteLayerTreeDrawingArea::mainFrameTiledBacking() const
{
    FrameView* frameView = m_webPage.mainFrameView();
    return frameView ? frameView->tiledBacking() : nullptr;
}

void RemoteLayerTreeDrawingArea::scheduleImmediateRenderingUpdate()
{
    m_updateRenderingTimer.startOneShot(0_s);
}

void RemoteLayerTreeDrawingArea::scheduleRenderingUpdate()
{
    if (m_isRenderingSuspended) {
        m_hasDeferredRenderingUpdate = true;
        return;
    }

    if (m_updateRenderingTimer.isActive())
        return;
    scheduleImmediateRenderingUpdate();
}

void RemoteLayerTreeDrawingArea::addCommitHandlers()
{
    if (m_webPage.firstFlushAfterCommit())
        return;

    [CATransaction addCommitHandler:[retainedPage = makeRefPtr(&m_webPage)] {
        if (Page* corePage = retainedPage->corePage()) {
            if (Frame* coreFrame = retainedPage->mainFrame())
                corePage->inspectorController().willComposite(*coreFrame);
        }
    } forPhase:kCATransactionPhasePreCommit];
    
    [CATransaction addCommitHandler:[retainedPage = makeRefPtr(&m_webPage)] {
        if (Page* corePage = retainedPage->corePage()) {
            if (Frame* coreFrame = retainedPage->mainFrame())
                corePage->inspectorController().didComposite(*coreFrame);
        }
        retainedPage->setFirstFlushAfterCommit(false);
    } forPhase:kCATransactionPhasePostCommit];
    
    m_webPage.setFirstFlushAfterCommit(true);
}

void RemoteLayerTreeDrawingArea::updateRendering()
{
    if (m_isRenderingSuspended) {
        m_hasDeferredRenderingUpdate = true;
        return;
    }

    if (m_waitingForBackingStoreSwap) {
        m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap = true;
        return;
    }

    // This function is not reentrant, e.g. a rAF callback may force repaint.
    if (m_inUpdateRendering)
        return;

    SetForScope<bool> change(m_inUpdateRendering, true);
    m_webPage.updateRendering();

    FloatRect visibleRect(FloatPoint(), m_viewSize);
    if (auto exposedRect = m_webPage.mainFrameView()->viewExposedRect())
        visibleRect.intersect(*exposedRect);

    addCommitHandlers();

    OptionSet<FinalizeRenderingUpdateFlags> flags;
    if (m_nextRenderingUpdateRequiresSynchronousImageDecoding)
        flags.add(FinalizeRenderingUpdateFlags::InvalidateImagesWithAsyncDecodes);

    m_webPage.finalizeRenderingUpdate(flags);

    // Because our view-relative overlay root layer is not attached to the FrameView's GraphicsLayer tree, we need to flush it manually.
    if (m_viewOverlayRootLayer)
        m_viewOverlayRootLayer->flushCompositingState(visibleRect);

    RELEASE_ASSERT(!m_pendingBackingStoreFlusher || m_pendingBackingStoreFlusher->hasFlushed());

    RemoteLayerBackingStoreCollection& backingStoreCollection = m_remoteLayerTreeContext->backingStoreCollection();
    backingStoreCollection.willFlushLayers();

    m_rootLayer->flushCompositingStateForThisLayerOnly();

    // FIXME: Minimize these transactions if nothing changed.
    RemoteLayerTreeTransaction layerTransaction;
    layerTransaction.setTransactionID(takeNextTransactionID());
    layerTransaction.setCallbackIDs(WTFMove(m_pendingCallbackIDs));

    m_remoteLayerTreeContext->setNextRenderingUpdateRequiresSynchronousImageDecoding(m_nextRenderingUpdateRequiresSynchronousImageDecoding);
    m_remoteLayerTreeContext->buildTransaction(layerTransaction, *downcast<GraphicsLayerCARemote>(m_rootLayer.get()).platformCALayer());
    m_remoteLayerTreeContext->setNextRenderingUpdateRequiresSynchronousImageDecoding(false);

    backingStoreCollection.willCommitLayerTree(layerTransaction);
    m_webPage.willCommitLayerTree(layerTransaction);

    layerTransaction.setNewlyReachedPaintingMilestones(m_pendingNewlyReachedPaintingMilestones);
    m_pendingNewlyReachedPaintingMilestones = { };

    layerTransaction.setActivityStateChangeID(m_activityStateChangeID);
    m_activityStateChangeID = ActivityStateChangeAsynchronous;

    RemoteScrollingCoordinatorTransaction scrollingTransaction;
#if ENABLE(ASYNC_SCROLLING)
    if (m_webPage.scrollingCoordinator())
        downcast<RemoteScrollingCoordinator>(*m_webPage.scrollingCoordinator()).buildTransaction(scrollingTransaction);
#endif

    m_nextRenderingUpdateRequiresSynchronousImageDecoding = false;
    m_waitingForBackingStoreSwap = true;

    send(Messages::RemoteLayerTreeDrawingAreaProxy::WillCommitLayerTree(layerTransaction.transactionID()));

    Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree message(layerTransaction, scrollingTransaction);
    auto commitEncoder = makeUnique<IPC::Encoder>(Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree::name(), m_identifier.toUInt64());
    commitEncoder->encode(message.arguments());

    // FIXME: Move all backing store flushing management to RemoteLayerBackingStoreCollection.
    bool hadAnyChangedBackingStore = false;
    Vector<RetainPtr<CGContextRef>> contextsToFlush;
    for (auto& layer : layerTransaction.changedLayers()) {
        if (layer->properties().changedProperties & RemoteLayerTreeTransaction::BackingStoreChanged) {
            hadAnyChangedBackingStore = true;
            if (layer->properties().backingStore) {
                if (auto contextPendingFlush = layer->properties().backingStore->takeFrontContextPendingFlush())
                    contextsToFlush.append(contextPendingFlush);
            }
        }

        layer->didCommit();
    }

    backingStoreCollection.didFlushLayers();

    if (hadAnyChangedBackingStore)
        backingStoreCollection.scheduleVolatilityTimer();

    RefPtr<BackingStoreFlusher> backingStoreFlusher = BackingStoreFlusher::create(WebProcess::singleton().parentProcessConnection(), WTFMove(commitEncoder), WTFMove(contextsToFlush));
    m_pendingBackingStoreFlusher = backingStoreFlusher;

    auto pageID = m_webPage.identifier();
    dispatch_async(m_commitQueue, [backingStoreFlusher = WTFMove(backingStoreFlusher), pageID] {
        backingStoreFlusher->flush();

        MonotonicTime timestamp = MonotonicTime::now();
        dispatch_async(dispatch_get_main_queue(), [pageID, timestamp] {
            if (WebPage* webPage = WebProcess::singleton().webPage(pageID))
                webPage->didFlushLayerTreeAtTime(timestamp);
        });
    });
}

void RemoteLayerTreeDrawingArea::didUpdate()
{
    // FIXME: This should use a counted replacement for setLayerTreeStateIsFrozen, but
    // the callers of that function are not strictly paired.

    m_waitingForBackingStoreSwap = false;

    if (m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap) {
        scheduleRenderingUpdate();
        m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap = false;
    }

    // This empty transaction serves to trigger CA's garbage collection of IOSurfaces. See <rdar://problem/16110687>
    [CATransaction begin];
    [CATransaction commit];

    HashSet<RemoteLayerTreeDisplayRefreshMonitor*> monitorsToNotify = m_displayRefreshMonitors;
    ASSERT(!m_displayRefreshMonitorsToNotify);
    m_displayRefreshMonitorsToNotify = &monitorsToNotify;
    while (!monitorsToNotify.isEmpty())
        monitorsToNotify.takeAny()->didUpdateLayers();
    m_displayRefreshMonitorsToNotify = nullptr;
}

void RemoteLayerTreeDrawingArea::mainFrameContentSizeChanged(const IntSize& contentsSize)
{
    m_rootLayer->setSize(contentsSize);
}

bool RemoteLayerTreeDrawingArea::markLayersVolatileImmediatelyIfPossible()
{
    return m_remoteLayerTreeContext->backingStoreCollection().markAllBackingStoreVolatileImmediatelyIfPossible();
}

Ref<RemoteLayerTreeDrawingArea::BackingStoreFlusher> RemoteLayerTreeDrawingArea::BackingStoreFlusher::create(IPC::Connection* connection, std::unique_ptr<IPC::Encoder> encoder, Vector<RetainPtr<CGContextRef>> contextsToFlush)
{
    return adoptRef(*new RemoteLayerTreeDrawingArea::BackingStoreFlusher(connection, WTFMove(encoder), WTFMove(contextsToFlush)));
}

RemoteLayerTreeDrawingArea::BackingStoreFlusher::BackingStoreFlusher(IPC::Connection* connection, std::unique_ptr<IPC::Encoder> encoder, Vector<RetainPtr<CGContextRef>> contextsToFlush)
    : m_connection(connection)
    , m_commitEncoder(WTFMove(encoder))
    , m_contextsToFlush(WTFMove(contextsToFlush))
    , m_hasFlushed(false)
{
}

void RemoteLayerTreeDrawingArea::BackingStoreFlusher::flush()
{
    ASSERT(!m_hasFlushed);

    TraceScope tracingScope(BackingStoreFlushStart, BackingStoreFlushEnd);
    
    for (auto& context : m_contextsToFlush)
        CGContextFlush(context.get());
    m_hasFlushed = true;

    m_connection->sendMessage(WTFMove(m_commitEncoder), { });
}

void RemoteLayerTreeDrawingArea::activityStateDidChange(OptionSet<WebCore::ActivityState::Flag>, ActivityStateChangeID activityStateChangeID, const Vector<CallbackID>& callbackIDs)
{
    // FIXME: Should we suspend painting while not visible, like TiledCoreAnimationDrawingArea? Probably.

    if (activityStateChangeID != ActivityStateChangeAsynchronous) {
        m_nextRenderingUpdateRequiresSynchronousImageDecoding = true;
        m_activityStateChangeID = activityStateChangeID;
        scheduleImmediateRenderingUpdate();
    }

    // FIXME: We may want to match behavior in TiledCoreAnimationDrawingArea by firing these callbacks after the next compositing flush, rather than immediately after
    // handling an activity state change.
    for (auto& callbackID : callbackIDs)
        m_webPage.send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void RemoteLayerTreeDrawingArea::addTransactionCallbackID(CallbackID callbackID)
{
    // Assume that if someone is listening for this transaction's completion, that they want it to
    // be a "complete" paint (including images that would normally be asynchronously decoding).
    m_nextRenderingUpdateRequiresSynchronousImageDecoding = true;

    m_pendingCallbackIDs.append(static_cast<RemoteLayerTreeTransaction::TransactionCallbackID>(callbackID));
    scheduleRenderingUpdate();
}

void RemoteLayerTreeDrawingArea::adoptLayersFromDrawingArea(DrawingArea& oldDrawingArea)
{
    RELEASE_ASSERT(oldDrawingArea.type() == type());

    RemoteLayerTreeDrawingArea& oldRemoteDrawingArea = static_cast<RemoteLayerTreeDrawingArea&>(oldDrawingArea);

    m_remoteLayerTreeContext->adoptLayersFromContext(*oldRemoteDrawingArea.m_remoteLayerTreeContext);
}

} // namespace WebKit
