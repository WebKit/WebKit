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
#import "RemoteImageBufferSetProxy.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeCommitBundle.h"
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
#import <WebCore/FrameInlines.h>
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
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WorkQueue.h>
#import <wtf/text/MakeString.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeDrawingArea);

constexpr FramesPerSecond DefaultPreferredFramesPerSecond = 60;

RemoteLayerTreeDrawingArea::RemoteLayerTreeDrawingArea(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(parameters.drawingAreaIdentifier, webPage)
    , m_remoteLayerTreeContext(RemoteLayerTreeContext::create(webPage))
    , m_updateRenderingTimer(*this, &RemoteLayerTreeDrawingArea::updateRendering)
    , m_commitQueue(WorkQueue::create("com.apple.WebKit.WebContent.RemoteLayerTreeDrawingArea.CommitQueue"_s, WorkQueue::QOS::UserInteractive))
    , m_backingStoreFlusher(BackingStoreFlusher::create(*WebProcess::singleton().parentProcessConnection()))
    , m_scheduleRenderingTimer(*this, &RemoteLayerTreeDrawingArea::scheduleRenderingUpdateTimerFired)
    , m_preferredFramesPerSecond(DefaultPreferredFramesPerSecond)
{
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
    return m_remoteLayerTreeContext.ptr();
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

void RemoteLayerTreeDrawingArea::gpuProcessConnectionWasDestroyed()
{
    m_remoteLayerTreeContext->gpuProcessConnectionWasDestroyed();
}

void RemoteLayerTreeDrawingArea::updateRootLayers()
{
    for (auto& rootLayer : m_rootLayers) {
        Vector<Ref<GraphicsLayer>> children;
        if (rootLayer.contentLayer) {
            children.append(Ref { *rootLayer.contentLayer });
            if (rootLayer.viewOverlayRootLayer)
                children.append(Ref { *rootLayer.viewOverlayRootLayer });
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
    ASSERT(Frame::isRootFrameIdentifier(frameID));
    auto layer = GraphicsLayer::create(graphicsLayerFactory(), *this);
    layer->setName(makeString("drawing area root "_s, frameID));
    m_rootLayers.append(RootLayerInfo {
        WTFMove(layer),
        nullptr,
        nullptr,
        frameID
    });
}

void RemoteLayerTreeDrawingArea::removeRootFrame(WebCore::FrameIdentifier frameID)
{
    auto count = m_rootLayers.removeAllMatching([frameID] (const auto& layer) {
        return layer.frameID == frameID;
    });
    ASSERT_UNUSED(count, count == 1);
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
    return frameView.frame().isMainFrame()
        || m_webPage->corePage()->settings().asyncFrameScrollingEnabled();
}

void RemoteLayerTreeDrawingArea::updatePreferences(const WebPreferencesStore& preferences)
{
    Ref page = *m_webPage->corePage();
    Settings& settings = page->settings();

    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(true);

    for (auto& rootLayer : m_rootLayers)
        rootLayer.layer->setShowDebugBorder(settings.showDebugBorders());

    m_remoteLayerTreeContext->setUseDynamicContentScalingDisplayListsForDOMRendering(preferences.getBoolValueForKey(WebPreferencesKey::useCGDisplayListsForDOMRenderingKey()));

    DebugPageOverlays::settingsChanged(page);
}

void RemoteLayerTreeDrawingArea::setDeviceScaleFactor(float deviceScaleFactor, CompletionHandler<void()>&& completionHandler)
{
    Ref { m_webPage.get() }->setDeviceScaleFactor(deviceScaleFactor);
    completionHandler();
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

void RemoteLayerTreeDrawingArea::updateRenderingWithForcedRepaint()
{
    if (m_isRenderingSuspended)
        return;

    protectedWebPage()->protectedCorePage()->forceRepaintAllFrames();
    updateRendering();
}

void RemoteLayerTreeDrawingArea::updateRenderingWithForcedRepaintAsync(WebPage& page, CompletionHandler<void()>&& completionHandler)
{
    updateRenderingWithForcedRepaint();
    completionHandler();
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

    if (RefPtr frameView = protectedWebPage()->localMainFrameView())
        frameView->setViewExposedRect(m_viewExposedRect);
}

WebCore::FloatRect RemoteLayerTreeDrawingArea::exposedContentRect() const
{
    RefPtr frameView = protectedWebPage()->localMainFrameView();
    if (!frameView)
        return FloatRect();

    return frameView->exposedContentRect();
}

void RemoteLayerTreeDrawingArea::setExposedContentRect(const FloatRect& exposedContentRect)
{
    RefPtr frameView = protectedWebPage()->localMainFrameView();
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
    if (!m_updateStartTime)
        m_updateStartTime = MonotonicTime::now();
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

    if (!m_updateStartTime)
        m_updateStartTime = MonotonicTime::now();

    Ref webPage = m_webPage.get();
    if (!webPage->hasRootFrames())
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
        if (RefPtr layer = rootLayer.viewOverlayRootLayer)
            layer->flushCompositingState(visibleRect);
    }

    Ref backingStoreCollection = m_remoteLayerTreeContext->backingStoreCollection();
    backingStoreCollection->willFlushLayers();

    // FIXME: Minimize these transactions if nothing changed.
    auto transactionID = takeNextTransactionID();
    send(Messages::RemoteLayerTreeDrawingAreaProxy::NotifyPendingCommitLayerTree(transactionID));

    auto transactions = WTF::map(m_rootLayers, [&](RootLayerInfo& rootLayer) -> RemoteLayerTreeCommitBundle::RootFrameData {
        backingStoreCollection->willBuildTransaction();
        rootLayer.layer->flushCompositingStateForThisLayerOnly();

        RemoteLayerTreeTransaction layerTransaction;

        RefPtr layer = downcast<GraphicsLayerCARemote>(rootLayer.layer.get()).platformCALayer();
        m_remoteLayerTreeContext->buildTransaction(layerTransaction, *layer, rootLayer.frameID);

        webPage->willCommitLayerTree(layerTransaction, rootLayer.frameID);

        m_waitingForBackingStoreSwap = true;

        RemoteScrollingCoordinatorTransaction scrollingTransaction;
#if ENABLE(ASYNC_SCROLLING)
        if (webPage->scrollingCoordinator())
            scrollingTransaction = downcast<RemoteScrollingCoordinator>(*webPage->protectedScrollingCoordinator()).buildTransaction(rootLayer.frameID);
        scrollingTransaction.setFrameIdentifier(rootLayer.frameID);
#endif

        return { WTFMove(layerTransaction), WTFMove(scrollingTransaction) };
    });

    for (auto& transaction : transactions)
        backingStoreCollection->willCommitLayerTree(CheckedRef { transaction.first });

    RemoteLayerTreeCommitBundle bundle { WTFMove(transactions), { WTFMove(m_pendingCallbackIDs), webPage->protectedCorePage()->renderTreeSize() }, std::nullopt, transactionID };

    if (webPage->localMainFrame()) {
        bundle.mainFrameData = MainFrameData { };
        auto& mainFrameData = *bundle.mainFrameData;
        mainFrameData.newlyReachedPaintingMilestones = std::exchange(m_pendingNewlyReachedPaintingMilestones, { });
        mainFrameData.activityStateChangeID = std::exchange(m_activityStateChangeID, ActivityStateChangeAsynchronous);

        webPage->willCommitMainFrameData(mainFrameData, transactionID);
        willCommitMainFrameData(mainFrameData);
    }
    bundle.startTime = *std::exchange(m_updateStartTime, std::nullopt);

    auto commitEncoder = makeUniqueRef<IPC::Encoder>(Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree::name(), m_identifier.toUInt64());
    commitEncoder.get() << bundle;

    Vector<std::unique_ptr<ThreadSafeImageBufferSetFlusher>> flushers;
    for (auto& transaction : bundle.transactions)
        flushers.appendVector(backingStoreCollection->didFlushLayers(CheckedRef { transaction.first }));

    OptionSet<WebPage::DidUpdateRenderingFlags> didUpdateRenderingFlags;
    if (flushers.size())
        didUpdateRenderingFlags.add(WebPage::DidUpdateRenderingFlags::PaintedLayers);
    webPage->didUpdateRendering(didUpdateRenderingFlags);

    m_backingStoreFlusher->markHasPendingFlush();

    auto pageID = webPage->identifier();
    m_commitQueue->dispatch([backingStoreFlusher = m_backingStoreFlusher, commitEncoder = WTFMove(commitEncoder), flushers = WTFMove(flushers), pageID] () mutable {
        bool flushSucceeded = backingStoreFlusher->flush(WTFMove(commitEncoder), WTFMove(flushers));

        RunLoop::mainSingleton().dispatch([pageID, flushSucceeded] () mutable {
            if (RefPtr webPage = WebProcess::singleton().webPage(pageID)) {
                if (RefPtr drawingArea = dynamicDowncast<RemoteLayerTreeDrawingArea>(webPage->drawingArea())) {
                    drawingArea->didCompleteRenderingUpdateDisplayFlush(flushSucceeded);
                    drawingArea->didCompleteRenderingFrame();
                }
            }
        });
    });
}

void RemoteLayerTreeDrawingArea::didCompleteRenderingUpdateDisplayFlush(bool flushSucceeded)
{
    protectedWebPage()->didFlushLayerTreeAtTime(MonotonicTime::now(), flushSucceeded);
    didCompleteRenderingUpdateDisplay();
}

void RemoteLayerTreeDrawingArea::displayDidRefresh(MonotonicTime start)
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
        m_updateStartTime = start;
        triggerRenderingUpdate();
        m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap = false;
        m_isScheduled = false;
    } else if (wasWaitingForBackingStoreSwap && m_updateRenderingTimer.isActive())
        m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap = true;
    else
        send(Messages::RemoteLayerTreeDrawingAreaProxy::NotifyPendingCommitLayerTree(std::nullopt));
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

Ref<RemoteLayerTreeDrawingArea::BackingStoreFlusher> RemoteLayerTreeDrawingArea::BackingStoreFlusher::create(Ref<IPC::Connection>&& connection)
{
    return adoptRef(*new RemoteLayerTreeDrawingArea::BackingStoreFlusher(WTFMove(connection)));
}

RemoteLayerTreeDrawingArea::BackingStoreFlusher::BackingStoreFlusher(Ref<IPC::Connection>&& connection)
    : m_connection(WTFMove(connection))
{
}

bool RemoteLayerTreeDrawingArea::BackingStoreFlusher::flush(UniqueRef<IPC::Encoder>&& commitEncoder, Vector<std::unique_ptr<ThreadSafeImageBufferSetFlusher>>&& flushers)
{
    ASSERT(m_pendingFlushes);

    TraceScope tracingScope(BackingStoreFlushStart, BackingStoreFlushEnd);
    bool flushSucceeded = true;
    HashMap<ImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>> handles;
    for (auto& flusher : flushers) {
        flushSucceeded = flusher->flushAndCollectHandles(handles);
        if (!flushSucceeded)
            break;
    }
    // FIXME: Currently we send the transaction even if the flush timed out.
    commitEncoder.get() << WTFMove(handles);

    m_pendingFlushes--;

    m_connection->sendMessage(WTFMove(commitEncoder), { });
    return flushSucceeded;
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
    m_remoteLayerTreeContext->adoptLayersFromContext(downcast<RemoteLayerTreeDrawingArea>(oldDrawingArea).m_remoteLayerTreeContext);
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
    RefPtr page = m_webPage->corePage();
    auto preferredFramesPerSecond = page->preferredRenderingUpdateFramesPerSecond();

    if (preferredFramesPerSecond && preferredFramesPerSecond != m_preferredFramesPerSecond)
        setPreferredFramesPerSecond(*preferredFramesPerSecond);

    m_preferredFramesPerSecond = preferredFramesPerSecond;
    m_preferredRenderingUpdateInterval = page->preferredRenderingUpdateInterval();
}

} // namespace WebKit
