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
#import "WebPreferencesKeys.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DebugPageOverlays.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/InspectorController.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollView.h>
#import <WebCore/Settings.h>
#import <WebCore/TiledBacking.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/SetForScope.h>
#import <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;

RemoteLayerTreeDrawingArea::RemoteLayerTreeDrawingArea(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaType::RemoteLayerTree, parameters.drawingAreaIdentifier, webPage)
    , m_remoteLayerTreeContext(makeUnique<RemoteLayerTreeContext>(webPage))
    , m_rootLayer(GraphicsLayer::create(graphicsLayerFactory(), *this))
    , m_updateRenderingTimer(*this, &RemoteLayerTreeDrawingArea::updateRendering)
{
    webPage.corePage()->settings().setForceCompositingMode(true);
    m_rootLayer->setName(MAKE_STATIC_STRING_IMPL("drawing area root"));

    m_commitQueue = adoptOSObject(dispatch_queue_create("com.apple.WebKit.WebContent.RemoteLayerTreeDrawingArea.CommitQueue", nullptr));

    if (auto viewExposedRect = parameters.viewExposedRect)
        setViewExposedRect(viewExposedRect);
}

RemoteLayerTreeDrawingArea::~RemoteLayerTreeDrawingArea() = default;

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
    triggerRenderingUpdate();
}

void RemoteLayerTreeDrawingArea::updateGeometry(const IntSize& viewSize, bool flushSynchronously, const WTF::MachSendRight&)
{
    m_viewSize = viewSize;
    m_webPage.setSize(viewSize);

    triggerRenderingUpdate();

    send(Messages::DrawingAreaProxy::DidUpdateGeometry());
}

bool RemoteLayerTreeDrawingArea::shouldUseTiledBackingForFrameView(const FrameView& frameView) const
{
    return frameView.frame().isMainFrame() || m_webPage.corePage()->settings().asyncFrameScrollingEnabled();
}

void RemoteLayerTreeDrawingArea::updatePreferences(const WebPreferencesStore& preferences)
{
    Settings& settings = m_webPage.corePage()->settings();

    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(true);

    m_rootLayer->setShowDebugBorder(settings.showDebugBorders());

    m_remoteLayerTreeContext->setUseCGDisplayListsForDOMRendering(preferences.getBoolValueForKey(WebPreferencesKey::useCGDisplayListsForDOMRenderingKey()));
    m_remoteLayerTreeContext->setUseCGDisplayListImageCache(preferences.getBoolValueForKey(WebPreferencesKey::useCGDisplayListImageCacheKey()) && !preferences.getBoolValueForKey(WebPreferencesKey::replayCGDisplayListsIntoBackingStoreKey()));

    DebugPageOverlays::settingsChanged(*m_webPage.corePage());
}

void RemoteLayerTreeDrawingArea::forceRepaintAsync(WebPage& page, CompletionHandler<void()>&& completionHandler)
{
    page.forceRepaintWithoutCallback();
    completionHandler();
}

void RemoteLayerTreeDrawingArea::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage.setDeviceScaleFactor(deviceScaleFactor);
}

DelegatedScrollingMode RemoteLayerTreeDrawingArea::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::DelegatedToNativeScrollView;
}

void RemoteLayerTreeDrawingArea::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_isRenderingSuspended == isFrozen)
        return;

    tracePoint(isFrozen ? LayerTreeFreezeStart : LayerTreeFreezeEnd);

    m_isRenderingSuspended = isFrozen;

    if (!m_isRenderingSuspended && m_hasDeferredRenderingUpdate) {
        m_hasDeferredRenderingUpdate = false;
        startRenderingUpdateTimer();
    }
}

void RemoteLayerTreeDrawingArea::forceRepaint()
{
    if (m_isRenderingSuspended)
        return;

    m_webPage.corePage()->forceRepaintAllFrames();
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

void RemoteLayerTreeDrawingArea::setViewExposedRect(std::optional<WebCore::FloatRect> viewExposedRect)
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
    triggerRenderingUpdate();
}

TiledBacking* RemoteLayerTreeDrawingArea::mainFrameTiledBacking() const
{
    FrameView* frameView = m_webPage.mainFrameView();
    return frameView ? frameView->tiledBacking() : nullptr;
}

void RemoteLayerTreeDrawingArea::startRenderingUpdateTimer()
{
    if (m_updateRenderingTimer.isActive())
        return;
    m_updateRenderingTimer.startOneShot(0_s);
}

void RemoteLayerTreeDrawingArea::triggerRenderingUpdate()
{
    if (m_isRenderingSuspended) {
        m_hasDeferredRenderingUpdate = true;
        return;
    }

    startRenderingUpdateTimer();
}

void RemoteLayerTreeDrawingArea::addCommitHandlers()
{
    if (m_webPage.firstFlushAfterCommit())
        return;

    [CATransaction addCommitHandler:[retainedPage = Ref { m_webPage }] {
        if (Page* corePage = retainedPage->corePage()) {
            if (Frame* coreFrame = retainedPage->mainFrame())
                corePage->inspectorController().willComposite(*coreFrame);
        }
    } forPhase:kCATransactionPhasePreLayout];
    
    [CATransaction addCommitHandler:[retainedPage = Ref { m_webPage }] {
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

    SetForScope change(m_inUpdateRendering, true);
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
    auto commitEncoder = makeUniqueRef<IPC::Encoder>(Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree::name(), m_identifier.toUInt64());
    commitEncoder.get() << WTFMove(message).arguments();

    auto flushers = backingStoreCollection.didFlushLayers(layerTransaction);
    bool haveFlushers = flushers.size();
    RefPtr<BackingStoreFlusher> backingStoreFlusher = BackingStoreFlusher::create(WebProcess::singleton().parentProcessConnection(), WTFMove(commitEncoder), WTFMove(flushers));
    m_pendingBackingStoreFlusher = backingStoreFlusher;

    if (haveFlushers)
        m_webPage.didPaintLayers();

    auto pageID = m_webPage.identifier();
    dispatch_async(m_commitQueue.get(), [backingStoreFlusher = WTFMove(backingStoreFlusher), pageID] {
        backingStoreFlusher->flush();

        MonotonicTime timestamp = MonotonicTime::now();
        RunLoop::main().dispatch([pageID, timestamp] {
            if (WebPage* webPage = WebProcess::singleton().webPage(pageID))
                webPage->didFlushLayerTreeAtTime(timestamp);
        });
    });
}

void RemoteLayerTreeDrawingArea::displayDidRefresh()
{
    // FIXME: This should use a counted replacement for setLayerTreeStateIsFrozen, but
    // the callers of that function are not strictly paired.

    m_waitingForBackingStoreSwap = false;

    if (m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap) {
        triggerRenderingUpdate();
        m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap = false;
    }

    // This empty transaction serves to trigger CA's garbage collection of IOSurfaces. See <rdar://problem/16110687>
    [CATransaction begin];
    [CATransaction commit];

    HashSet<RemoteLayerTreeDisplayRefreshMonitor*> monitorsToNotify = m_displayRefreshMonitors;
    ASSERT(!m_displayRefreshMonitorsToNotify);
    m_displayRefreshMonitorsToNotify = &monitorsToNotify;
    while (!monitorsToNotify.isEmpty())
        monitorsToNotify.takeAny()->triggerDisplayDidRefresh();
    m_displayRefreshMonitorsToNotify = nullptr;
}

void RemoteLayerTreeDrawingArea::mainFrameContentSizeChanged(const IntSize& contentsSize)
{
    m_rootLayer->setSize(contentsSize);
}

void RemoteLayerTreeDrawingArea::tryMarkLayersVolatile(CompletionHandler<void(bool)>&& completionFunction)
{
    m_remoteLayerTreeContext->backingStoreCollection().tryMarkAllBackingStoreVolatile(WTFMove(completionFunction));
}

Ref<RemoteLayerTreeDrawingArea::BackingStoreFlusher> RemoteLayerTreeDrawingArea::BackingStoreFlusher::create(IPC::Connection* connection, UniqueRef<IPC::Encoder>&& encoder, Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> flushers)
{
    return adoptRef(*new RemoteLayerTreeDrawingArea::BackingStoreFlusher(connection, WTFMove(encoder), WTFMove(flushers)));
}

RemoteLayerTreeDrawingArea::BackingStoreFlusher::BackingStoreFlusher(IPC::Connection* connection, UniqueRef<IPC::Encoder>&& encoder, Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> flushers)
    : m_connection(connection)
    , m_commitEncoder(encoder.moveToUniquePtr())
    , m_flushers(WTFMove(flushers))
    , m_hasFlushed(false)
{
}

void RemoteLayerTreeDrawingArea::BackingStoreFlusher::flush()
{
    ASSERT(!m_hasFlushed);

    TraceScope tracingScope(BackingStoreFlushStart, BackingStoreFlushEnd);
    
    for (auto& flusher : m_flushers)
        flusher->flush();
    m_hasFlushed = true;

    ASSERT(m_commitEncoder);
    m_connection->sendMessage(makeUniqueRefFromNonNullUniquePtr(WTFMove(m_commitEncoder)), { });
}

void RemoteLayerTreeDrawingArea::activityStateDidChange(OptionSet<WebCore::ActivityState::Flag>, ActivityStateChangeID activityStateChangeID, CompletionHandler<void()>&& callback)
{
    // FIXME: Should we suspend painting while not visible, like TiledCoreAnimationDrawingArea? Probably.

    if (activityStateChangeID != ActivityStateChangeAsynchronous) {
        m_nextRenderingUpdateRequiresSynchronousImageDecoding = true;
        m_activityStateChangeID = activityStateChangeID;
        startRenderingUpdateTimer();
    }

    // FIXME: We may want to match behavior in TiledCoreAnimationDrawingArea by firing these callbacks after the next compositing flush, rather than immediately after
    // handling an activity state change.
    callback();
}

void RemoteLayerTreeDrawingArea::addTransactionCallbackID(CallbackID callbackID)
{
    // Assume that if someone is listening for this transaction's completion, that they want it to
    // be a "complete" paint (including images that would normally be asynchronously decoding).
    m_nextRenderingUpdateRequiresSynchronousImageDecoding = true;

    m_pendingCallbackIDs.append(static_cast<RemoteLayerTreeTransaction::TransactionCallbackID>(callbackID));
    triggerRenderingUpdate();
}

void RemoteLayerTreeDrawingArea::adoptLayersFromDrawingArea(DrawingArea& oldDrawingArea)
{
    RELEASE_ASSERT(oldDrawingArea.type() == type());

    RemoteLayerTreeDrawingArea& oldRemoteDrawingArea = static_cast<RemoteLayerTreeDrawingArea&>(oldDrawingArea);

    m_remoteLayerTreeContext->adoptLayersFromContext(*oldRemoteDrawingArea.m_remoteLayerTreeContext);
}

} // namespace WebKit
