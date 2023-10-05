/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#import "MessageSenderInlines.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreeDrawingAreaProxyMessages.h"
#import "RemoteScrollingCoordinator.h"
#import "RemoteScrollingCoordinatorTransaction.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebPageCreationParameters.h"
#import "WebPageProxyMessages.h"
#import "WebPreferencesKeys.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DebugPageOverlays.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollView.h>
#import <WebCore/Settings.h>
#import <WebCore/TiledBacking.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/SetForScope.h>
#import <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;

constexpr FramesPerSecond DefaultPreferredFramesPerSecond = 60;

RemoteLayerTreeDrawingArea::RemoteLayerTreeDrawingArea(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaType::RemoteLayerTree, parameters.drawingAreaIdentifier, webPage)
    , m_remoteLayerTreeContext(makeUnique<RemoteLayerTreeContext>(webPage))
    , m_updateRenderingTimer(*this, &RemoteLayerTreeDrawingArea::updateRendering)
    , m_scheduleRenderingTimer(*this, &RemoteLayerTreeDrawingArea::scheduleRenderingUpdateTimerFired)
    , m_preferredFramesPerSecond(DefaultPreferredFramesPerSecond)
{
    webPage.corePage()->settings().setForceCompositingMode(true);

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
    ASSERT_NOT_REACHED();
    return nullptr;
}

void RemoteLayerTreeDrawingArea::setPreferredFramesPerSecond(FramesPerSecond preferredFramesPerSecond)
{
    send(Messages::RemoteLayerTreeDrawingAreaProxy::SetPreferredFramesPerSecond(preferredFramesPerSecond));
}

void RemoteLayerTreeDrawingArea::updateRootLayers()
{
    for (auto& rootLayer : m_rootLayers) {
        Vector<Ref<GraphicsLayer>> children;
        if (rootLayer.contentLayer) {
            children.append(*rootLayer.contentLayer);
            if (rootLayer.viewOverlayRootLayer)
                children.append(*rootLayer.viewOverlayRootLayer);
        }
        rootLayer.layer->setChildren(WTFMove(children));
    }
}

void RemoteLayerTreeDrawingArea::attachViewOverlayGraphicsLayer(WebCore::FrameIdentifier mainFrameID, GraphicsLayer* viewOverlayRootLayer)
{
    if (auto* layerInfo = rootLayerInfoWithFrameIdentifier(mainFrameID)) {
        layerInfo->viewOverlayRootLayer = viewOverlayRootLayer;
        updateRootLayers();
    }
}

void RemoteLayerTreeDrawingArea::addRootFrame(WebCore::FrameIdentifier frameID)
{
    auto layer = GraphicsLayer::create(graphicsLayerFactory(), *this);
    layer->setName(makeString("drawing area root "_s, frameID));
    m_rootLayers.append(RootLayerInfo {
        WTFMove(layer),
        nullptr,
        nullptr,
        frameID
    });
}

void RemoteLayerTreeDrawingArea::setRootCompositingLayer(WebCore::Frame& frame, GraphicsLayer* rootGraphicsLayer)
{
    for (auto& rootLayer : m_rootLayers) {
        if (rootLayer.frameID == frame.frameID())
            rootLayer.contentLayer = rootGraphicsLayer;
    }
    updateRootLayers();
    triggerRenderingUpdate();
}

void RemoteLayerTreeDrawingArea::updateGeometry(const IntSize& viewSize, bool flushSynchronously, const WTF::MachSendRight&, CompletionHandler<void()>&& completionHandler)
{
    IntSize size = viewSize;
    IntSize contentSize = IntSize(-1, -1);

    Ref webPage = m_webPage.get();
    if (!webPage->minimumSizeForAutoLayout().width() || webPage->autoSizingShouldExpandToViewHeight() || (!webPage->sizeToContentAutoSizeMaximumSize().width() && !webPage->sizeToContentAutoSizeMaximumSize().height()))
        webPage->setSize(size);

    RefPtr frameView = webPage->localMainFrameView();

    if (webPage->autoSizingShouldExpandToViewHeight() && frameView)
        frameView->setAutoSizeFixedMinimumHeight(viewSize.height());

    webPage->layoutIfNeeded();

    if (frameView && (webPage->minimumSizeForAutoLayout().width() || (webPage->sizeToContentAutoSizeMaximumSize().width() && webPage->sizeToContentAutoSizeMaximumSize().height()))) {
        contentSize = frameView->autoSizingIntrinsicContentSize();
        size = contentSize;
    }

    triggerRenderingUpdate();
    completionHandler();
}

bool RemoteLayerTreeDrawingArea::shouldUseTiledBackingForFrameView(const LocalFrameView& frameView) const
{
    return frameView.frame().isRootFrame()
        || m_webPage->corePage()->settings().asyncFrameScrollingEnabled();
}

void RemoteLayerTreeDrawingArea::updatePreferences(const WebPreferencesStore& preferences)
{
    auto& page = *m_webPage->corePage();
    Settings& settings = page.settings();

    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(true);

    for (auto& rootLayer : m_rootLayers)
        rootLayer.layer->setShowDebugBorder(settings.showDebugBorders());

    m_remoteLayerTreeContext->setUseDynamicContentScalingDisplayListsForDOMRendering(preferences.getBoolValueForKey(WebPreferencesKey::useCGDisplayListsForDOMRenderingKey()));

    DebugPageOverlays::settingsChanged(page);
}

void RemoteLayerTreeDrawingArea::forceRepaintAsync(WebPage& page, CompletionHandler<void()>&& completionHandler)
{
    page.forceRepaintWithoutCallback();
    completionHandler();
}

void RemoteLayerTreeDrawingArea::setDeviceScaleFactor(float deviceScaleFactor)
{
    Ref { m_webPage.get() }->setDeviceScaleFactor(deviceScaleFactor);
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

    m_webPage->corePage()->forceRepaintAllFrames();
    updateRendering();
}

void RemoteLayerTreeDrawingArea::acceleratedAnimationDidStart(WebCore::PlatformLayerIdentifier layerID, const String& key, MonotonicTime startTime)
{
    m_remoteLayerTreeContext->animationDidStart(layerID, key, startTime);
}

void RemoteLayerTreeDrawingArea::acceleratedAnimationDidEnd(WebCore::PlatformLayerIdentifier layerID, const String& key)
{
    m_remoteLayerTreeContext->animationDidEnd(layerID, key);
}

void RemoteLayerTreeDrawingArea::setViewExposedRect(std::optional<WebCore::FloatRect> viewExposedRect)
{
    m_viewExposedRect = viewExposedRect;

    if (RefPtr frameView = m_webPage->localMainFrameView())
        frameView->setViewExposedRect(m_viewExposedRect);
}

WebCore::FloatRect RemoteLayerTreeDrawingArea::exposedContentRect() const
{
    RefPtr frameView = m_webPage->localMainFrameView();
    if (!frameView)
        return FloatRect();

    return frameView->exposedContentRect();
}

void RemoteLayerTreeDrawingArea::setExposedContentRect(const FloatRect& exposedContentRect)
{
    RefPtr frameView = m_webPage->localMainFrameView();
    if (!frameView)
        return;
    if (frameView->exposedContentRect() == exposedContentRect)
        return;

    frameView->setExposedContentRect(exposedContentRect);
    triggerRenderingUpdate();
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

void RemoteLayerTreeDrawingArea::setNextRenderingUpdateRequiresSynchronousImageDecoding()
{
    m_remoteLayerTreeContext->setNextRenderingUpdateRequiresSynchronousImageDecoding();
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

    Ref webPage = m_webPage.get();
    if (auto* page = webPage->corePage(); page && !page->rootFrames().computeSize())
        return;

    scaleViewToFitDocumentIfNeeded();

    SetForScope change(m_inUpdateRendering, true);
    webPage->updateRendering();
    webPage->flushPendingIntrinsicContentSizeUpdate();

    auto size = webPage->size();
    FloatRect visibleRect(FloatPoint(), size);
    if (RefPtr mainFrameView = webPage->localMainFrameView()) {
        if (auto exposedRect = mainFrameView->viewExposedRect())
            visibleRect.intersect(*exposedRect);
    }

    OptionSet<FinalizeRenderingUpdateFlags> flags;
    if (m_remoteLayerTreeContext->nextRenderingUpdateRequiresSynchronousImageDecoding())
        flags.add(FinalizeRenderingUpdateFlags::InvalidateImagesWithAsyncDecodes);

    webPage->finalizeRenderingUpdate(flags);

    willStartRenderingUpdateDisplay();

    // Because our view-relative overlay root layer is not attached to the FrameView's GraphicsLayer tree, we need to flush it manually.
    for (auto& rootLayer : m_rootLayers) {
        if (rootLayer.viewOverlayRootLayer)
            rootLayer.viewOverlayRootLayer->flushCompositingState(visibleRect);
    }

    RELEASE_ASSERT(!m_pendingBackingStoreFlusher || m_pendingBackingStoreFlusher->hasFlushed());

    RemoteLayerBackingStoreCollection& backingStoreCollection = m_remoteLayerTreeContext->backingStoreCollection();
    backingStoreCollection.willFlushLayers();

    // FIXME: Minimize these transactions if nothing changed.
    auto transactionID = takeNextTransactionID();
    auto transactions = WTF::map(m_rootLayers, [&](auto& rootLayer) -> std::pair<RemoteLayerTreeTransaction, RemoteScrollingCoordinatorTransaction> {
        rootLayer.layer->flushCompositingStateForThisLayerOnly();

        RemoteLayerTreeTransaction layerTransaction;
        layerTransaction.setTransactionID(transactionID);
        layerTransaction.setCallbackIDs(WTFMove(m_pendingCallbackIDs));

        m_remoteLayerTreeContext->buildTransaction(layerTransaction, *downcast<GraphicsLayerCARemote>(rootLayer.layer.get()).platformCALayer(), rootLayer.frameID);

        backingStoreCollection.willCommitLayerTree(layerTransaction);

        // FIXME: Investigate whether this needs to be done multiple times in a page with multiple root frames. <rdar://116202678>
        webPage->willCommitLayerTree(layerTransaction, rootLayer.frameID);

        layerTransaction.setNewlyReachedPaintingMilestones(std::exchange(m_pendingNewlyReachedPaintingMilestones, { }));
        layerTransaction.setActivityStateChangeID(std::exchange(m_activityStateChangeID, ActivityStateChangeAsynchronous));

        willCommitLayerTree(layerTransaction);

        m_waitingForBackingStoreSwap = true;

        send(Messages::RemoteLayerTreeDrawingAreaProxy::WillCommitLayerTree(layerTransaction.transactionID()));

        RemoteScrollingCoordinatorTransaction scrollingTransaction;
#if ENABLE(ASYNC_SCROLLING)
        if (webPage->scrollingCoordinator())
            scrollingTransaction = downcast<RemoteScrollingCoordinator>(*webPage->scrollingCoordinator()).buildTransaction();
#endif
        return { WTFMove(layerTransaction), WTFMove(scrollingTransaction) };
    });

    Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree message(transactions);
    auto commitEncoder = makeUniqueRef<IPC::Encoder>(Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree::name(), m_identifier.toUInt64());
    commitEncoder.get() << WTFMove(message).arguments();

    Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> flushers;
    for (auto& transaction : transactions)
        flushers.appendVector(backingStoreCollection.didFlushLayers(transaction.first));
    bool haveFlushers = flushers.size();
    RefPtr<BackingStoreFlusher> backingStoreFlusher = BackingStoreFlusher::create(RefPtr { WebProcess::singleton().parentProcessConnection() }, WTFMove(commitEncoder), WTFMove(flushers));
    m_pendingBackingStoreFlusher = backingStoreFlusher;

    if (haveFlushers)
        webPage->didPaintLayers();

    auto pageID = webPage->identifier();
    dispatch_async(m_commitQueue.get(), makeBlockPtr([backingStoreFlusher = WTFMove(backingStoreFlusher), pageID] () mutable {
        backingStoreFlusher->flush();

        RunLoop::main().dispatch([pageID] () mutable {
            if (auto* webPage = WebProcess::singleton().webPage(pageID)) {
                if (auto* drawingArea = dynamicDowncast<RemoteLayerTreeDrawingArea>(webPage->drawingArea())) {
                    drawingArea->didCompleteRenderingUpdateDisplay();
                    drawingArea->didCompleteRenderingFrame();
                }
            }
        });
    }).get());
}

void RemoteLayerTreeDrawingArea::didCompleteRenderingUpdateDisplay()
{
    m_webPage->didFlushLayerTreeAtTime(MonotonicTime::now());
    DrawingArea::didCompleteRenderingUpdateDisplay();
}

void RemoteLayerTreeDrawingArea::displayDidRefresh()
{
    // FIXME: This should use a counted replacement for setLayerTreeStateIsFrozen, but
    // the callers of that function are not strictly paired.

    auto wasWaitingForBackingStoreSwap = std::exchange(m_waitingForBackingStoreSwap, false);

    if (!WebProcess::singleton().shouldUseRemoteRenderingFor(WebCore::RenderingPurpose::DOM)) {
        // This empty transaction serves to trigger CA's garbage collection of IOSurfaces. See <rdar://problem/16110687>
        [CATransaction begin];
        [CATransaction commit];
    }

    if (m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap || (m_isScheduled && !m_scheduleRenderingTimer.isActive())) {
        triggerRenderingUpdate();
        m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap = false;
        m_isScheduled = false;
    } else if (wasWaitingForBackingStoreSwap && m_updateRenderingTimer.isActive())
        m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap = true;
    else
        send(Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTreeNotTriggered(nextTransactionID()));
}

auto RemoteLayerTreeDrawingArea::rootLayerInfoWithFrameIdentifier(WebCore::FrameIdentifier frameID) -> RootLayerInfo*
{
    auto index = m_rootLayers.findIf([&] (const auto& layer) {
        return layer.frameID == frameID;
    });
    if (index == WTF::notFound) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    return &m_rootLayers[index];
}

void RemoteLayerTreeDrawingArea::mainFrameContentSizeChanged(WebCore::FrameIdentifier frameID, const IntSize& contentsSize)
{
    if (auto* layerInfo = rootLayerInfoWithFrameIdentifier(frameID))
        layerInfo->layer->setSize(contentsSize);
}

void RemoteLayerTreeDrawingArea::tryMarkLayersVolatile(CompletionHandler<void(bool)>&& completionFunction)
{
    m_remoteLayerTreeContext->backingStoreCollection().tryMarkAllBackingStoreVolatile(WTFMove(completionFunction));
}

Ref<RemoteLayerTreeDrawingArea::BackingStoreFlusher> RemoteLayerTreeDrawingArea::BackingStoreFlusher::create(RefPtr<IPC::Connection>&& connection, UniqueRef<IPC::Encoder>&& encoder, Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> flushers)
{
    return adoptRef(*new RemoteLayerTreeDrawingArea::BackingStoreFlusher(WTFMove(connection), WTFMove(encoder), WTFMove(flushers)));
}

RemoteLayerTreeDrawingArea::BackingStoreFlusher::BackingStoreFlusher(RefPtr<IPC::Connection>&& connection, UniqueRef<IPC::Encoder>&& encoder, Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> flushers)
    : m_connection(WTFMove(connection))
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

void RemoteLayerTreeDrawingArea::activityStateDidChange(OptionSet<WebCore::ActivityState>, ActivityStateChangeID activityStateChangeID, CompletionHandler<void()>&& callback)
{
    // FIXME: Should we suspend painting while not visible, like TiledCoreAnimationDrawingArea? Probably.

    if (activityStateChangeID != ActivityStateChangeAsynchronous) {
        m_remoteLayerTreeContext->setNextRenderingUpdateRequiresSynchronousImageDecoding();
        m_activityStateChangeID = activityStateChangeID;
        startRenderingUpdateTimer();
    }

    // FIXME: We may want to match behavior in TiledCoreAnimationDrawingArea by firing these callbacks after the next compositing flush, rather than immediately after
    // handling an activity state change.
    callback();
}

void RemoteLayerTreeDrawingArea::dispatchAfterEnsuringDrawing(IPC::AsyncReplyID callbackID)
{
    // Assume that if someone is listening for this transaction's completion, that they want it to
    // be a "complete" paint (including images that would normally be asynchronously decoding).
    m_remoteLayerTreeContext->setNextRenderingUpdateRequiresSynchronousImageDecoding();

    m_pendingCallbackIDs.append(callbackID);
    triggerRenderingUpdate();
}

void RemoteLayerTreeDrawingArea::adoptLayersFromDrawingArea(DrawingArea& oldDrawingArea)
{
    RELEASE_ASSERT(oldDrawingArea.type() == type());

    RemoteLayerTreeDrawingArea& oldRemoteDrawingArea = static_cast<RemoteLayerTreeDrawingArea&>(oldDrawingArea);

    m_remoteLayerTreeContext->adoptLayersFromContext(*oldRemoteDrawingArea.m_remoteLayerTreeContext);
}

void RemoteLayerTreeDrawingArea::scheduleRenderingUpdateTimerFired()
{
    triggerRenderingUpdate();
    m_isScheduled = false;
}

bool RemoteLayerTreeDrawingArea::scheduleRenderingUpdate()
{
    if (m_isScheduled)
        return true;

    tracePoint(RemoteLayerTreeScheduleRenderingUpdate, m_waitingForBackingStoreSwap);

    m_isScheduled = true;

    if (m_preferredFramesPerSecond) {
        if (displayDidRefreshIsPending())
            return true;

        callOnMainRunLoop([self = WeakPtr { this }] () {
            if (self) {
                self->m_isScheduled = false;
                self->triggerRenderingUpdate();
            }
        });
    } else
        m_scheduleRenderingTimer.startOneShot(m_preferredRenderingUpdateInterval);

    return true;
}

void RemoteLayerTreeDrawingArea::renderingUpdateFramesPerSecondChanged()
{
    auto* page = m_webPage->corePage();
    auto preferredFramesPerSecond = page->preferredRenderingUpdateFramesPerSecond();

    if (preferredFramesPerSecond && preferredFramesPerSecond != m_preferredFramesPerSecond)
        setPreferredFramesPerSecond(*preferredFramesPerSecond);

    m_preferredFramesPerSecond = preferredFramesPerSecond;
    m_preferredRenderingUpdateInterval = page->preferredRenderingUpdateInterval();
}

} // namespace WebKit
