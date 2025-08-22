/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeDrawingAreaProxy.h"

#import "DrawingAreaMessages.h"
#import "DrawingAreaProxyMessages.h"
#import "LayerProperties.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "ProcessThrottler.h"
#import "RemoteLayerTreeDrawingAreaProxyMessages.h"
#import "RemotePageDrawingAreaProxy.h"
#import "RemotePageProxy.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "RemoteScrollingCoordinatorTransaction.h"
#import "RemoteScrollingTreeCocoa.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "WindowKind.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/AnimationFrameRate.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurfacePool.h>
#import <WebCore/ScrollView.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/MachSendRight.h>
#import <wtf/SystemTracing.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace IPC;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeDrawingAreaProxy);

RemoteLayerTreeDrawingAreaProxy::RemoteLayerTreeDrawingAreaProxy(WebPageProxy& pageProxy, WebProcessProxy& webProcessProxy)
    : DrawingAreaProxy(pageProxy, webProcessProxy)
    , m_remoteLayerTreeHost(makeUnique<RemoteLayerTreeHost>(*this))
#if ASSERT_ENABLED
    , m_lastVisibleTransactionID(TransactionIdentifier(), webProcessProxy.coreProcessIdentifier())
#endif
    , m_transactionIDForPendingCACommit(TransactionIdentifier(), webProcessProxy.coreProcessIdentifier())
{
    // We don't want to pool surfaces in the UI process.
    // FIXME: We should do this somewhere else.
    IOSurfacePool::sharedPoolSingleton().setPoolSize(0);

    if (pageProxy.protectedPreferences()->tiledScrollingIndicatorVisible())
        initializeDebugIndicator();
}

RemoteLayerTreeDrawingAreaProxy::~RemoteLayerTreeDrawingAreaProxy() = default;

std::span<IPC::ReceiverName> RemoteLayerTreeDrawingAreaProxy::messageReceiverNames() const
{
    static std::array<IPC::ReceiverName, 2> names { Messages::DrawingAreaProxy::messageReceiverName(), Messages::RemoteLayerTreeDrawingAreaProxy::messageReceiverName() };
    return { names };
}

void RemoteLayerTreeDrawingAreaProxy::addRemotePageDrawingAreaProxy(RemotePageDrawingAreaProxy& proxy)
{
    m_remotePageProcessState.add(proxy.process().coreProcessIdentifier(), ProcessState { });
}

void RemoteLayerTreeDrawingAreaProxy::removeRemotePageDrawingAreaProxy(RemotePageDrawingAreaProxy& proxy)
{
    ASSERT(m_remotePageProcessState.contains(proxy.process().coreProcessIdentifier()));
    m_remotePageProcessState.remove(proxy.process().coreProcessIdentifier());
}

std::unique_ptr<RemoteLayerTreeHost> RemoteLayerTreeDrawingAreaProxy::detachRemoteLayerTreeHost()
{
    m_remoteLayerTreeHost->detachFromDrawingArea();
    return WTFMove(m_remoteLayerTreeHost);
}

void RemoteLayerTreeDrawingAreaProxy::sizeDidChange()
{
    RefPtr page = this->page();
    if (!page || !page->hasRunningProcess())
        return;
    if (CheckedPtr scrollingCoordinator = page->scrollingCoordinatorProxy())
        scrollingCoordinator->viewSizeDidChange();

    if (m_isWaitingForDidUpdateGeometry)
        return;
    sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::remotePageProcessDidTerminate(WebCore::ProcessIdentifier processIdentifier)
{
    if (!m_remoteLayerTreeHost)
        return;

    if (CheckedPtr scrollingCoordinator = page() ? page()->scrollingCoordinatorProxy() : nullptr) {
        scrollingCoordinator->willCommitLayerAndScrollingTrees();
        m_remoteLayerTreeHost->remotePageProcessDidTerminate(processIdentifier);
        scrollingCoordinator->didCommitLayerAndScrollingTrees();
    }
}

void RemoteLayerTreeDrawingAreaProxy::viewWillStartLiveResize()
{
    if (CheckedPtr scrollingCoordinator = page() ? page()->scrollingCoordinatorProxy() : nullptr)
        scrollingCoordinator->viewWillStartLiveResize();
}

void RemoteLayerTreeDrawingAreaProxy::viewWillEndLiveResize()
{
    if (CheckedPtr scrollingCoordinator = page() ? page()->scrollingCoordinatorProxy() : nullptr)
        scrollingCoordinator->viewWillEndLiveResize();
}

void RemoteLayerTreeDrawingAreaProxy::deviceScaleFactorDidChange(CompletionHandler<void()>&& completionHandler)
{
    Ref aggregator = CallbackAggregator::create(WTFMove(completionHandler));
    forEachProcessState([&](ProcessState& state, WebProcessProxy& webProcess) {
        if (RefPtr page = this->page())
            webProcess.sendWithAsyncReply(Messages::DrawingArea::SetDeviceScaleFactor(page->deviceScaleFactor()), [aggregator] { }, identifier());
    });
}

void RemoteLayerTreeDrawingAreaProxy::didUpdateGeometry()
{
    ASSERT(m_isWaitingForDidUpdateGeometry);

    m_isWaitingForDidUpdateGeometry = false;

    RefPtr page = this->page();
    if (!page)
        return;

    IntSize minimumSizeForAutoLayout = page->minimumSizeForAutoLayout();
    IntSize sizeToContentAutoSizeMaximumSize = page->sizeToContentAutoSizeMaximumSize();

    // If the WKView was resized while we were waiting for a DidUpdateGeometry reply from the web process,
    // we need to resend the new size here.
    if (m_lastSentSize != size() || m_lastSentMinimumSizeForAutoLayout != minimumSizeForAutoLayout || m_lastSentSizeToContentAutoSizeMaximumSize != sizeToContentAutoSizeMaximumSize)
        sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::sendUpdateGeometry()
{
    RefPtr page = this->page();
    if (!page)
        return;

    m_lastSentMinimumSizeForAutoLayout = page->minimumSizeForAutoLayout();
    m_lastSentSizeToContentAutoSizeMaximumSize = page->sizeToContentAutoSizeMaximumSize();
    m_lastSentSize = size();

    dispatchSetObscuredContentInsets();

    m_isWaitingForDidUpdateGeometry = true;
    sendWithAsyncReply(Messages::DrawingArea::UpdateGeometry(size(), false /* flushSynchronously */, MachSendRight()), [weakThis = WeakPtr { this }] {
        if (!weakThis)
            return;
        weakThis->didUpdateGeometry();
    });
}

RemoteLayerTreeDrawingAreaProxy::ProcessState& RemoteLayerTreeDrawingAreaProxy::processStateForConnection(IPC::Connection& connection)
{
    for (auto& [key, value] : m_remotePageProcessState) {
        RefPtr webProcess = WebProcessProxy::processForIdentifier(key);
        if (webProcess && webProcess->hasConnection(connection))
            return value;
    }

    RELEASE_ASSERT(webProcessProxy().hasConnection(connection));
    return m_webPageProxyProcessState;
}

void RemoteLayerTreeDrawingAreaProxy::forEachProcessState(NOESCAPE Function<void(ProcessState&, WebProcessProxy&)>&& callback)
{
    callback(m_webPageProxyProcessState, webProcessProxy());
    for (auto& [key, value] : m_remotePageProcessState) {
        RefPtr webProcess = WebProcessProxy::processForIdentifier(key);
        if (webProcess)
            callback(value, *webProcess);
    }
}

const RemoteLayerTreeDrawingAreaProxy::ProcessState& RemoteLayerTreeDrawingAreaProxy::processStateForIdentifier(WebCore::ProcessIdentifier identifier) const
{
    if (webProcessProxy().coreProcessIdentifier() == identifier)
        return m_webPageProxyProcessState;

    auto iter = m_remotePageProcessState.find(identifier);
    RELEASE_ASSERT(iter.get());
    return *iter.values();
}

IPC::Connection* RemoteLayerTreeDrawingAreaProxy::connectionForIdentifier(WebCore::ProcessIdentifier processIdentifier)
{
    RefPtr webProcess = WebProcessProxy::processForIdentifier(processIdentifier);
    if (webProcess && webProcess->hasConnection())
        return &webProcess->connection();
    return nullptr;
}

void RemoteLayerTreeDrawingAreaProxy::commitLayerTreeNotTriggered(IPC::Connection& connection, TransactionID nextCommitTransactionID)
{
    ProcessState& state = processStateForConnection(connection);
    if (state.lastLayerTreeTransactionID && nextCommitTransactionID.lessThanOrEqualSameProcess(*state.lastLayerTreeTransactionID)) {
        LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTreeNotTriggered nextCommitTransactionID=" << nextCommitTransactionID << ") already obsoleted by m_lastLayerTreeTransactionID=" << state.lastLayerTreeTransactionID);
        return;
    }

    state.commitLayerTreeMessageState = Idle;

    maybePauseDisplayRefreshCallbacks();

#if ENABLE(ASYNC_SCROLLING)
    if (RefPtr page = this->page())
        page->checkedScrollingCoordinatorProxy()->applyScrollingTreeLayerPositionsAfterCommit();
#endif
}

void RemoteLayerTreeDrawingAreaProxy::willCommitLayerTree(IPC::Connection& connection, TransactionID transactionID)
{
    ProcessState& state = processStateForConnection(connection);
    if (state.lastLayerTreeTransactionID && transactionID.lessThanOrEqualSameProcess(*state.lastLayerTreeTransactionID))
        return;

    state.pendingLayerTreeTransactionID = transactionID;
}

void RemoteLayerTreeDrawingAreaProxy::commitLayerTree(IPC::Connection& connection, const Vector<std::pair<RemoteLayerTreeTransaction, RemoteScrollingCoordinatorTransaction>>& transactions, HashMap<RemoteImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>&& handlesMap)
{
    // The `sendRights` vector must have __block scope to be captured by
    // the commit handler block below without the need to copy it.
    __block Vector<MachSendRight, 16> sendRights;
    for (auto& transaction : transactions) {
        // commitLayerTreeTransaction consumes the incoming buffers, so we need to grab them first.
        for (auto& [layerID, properties] : CheckedRef { transaction.first }->changedLayerProperties()) {
            auto* backingStoreProperties = properties->backingStoreOrProperties.properties.get();
            if (!backingStoreProperties)
                continue;
            if (backingStoreProperties->bufferSetIdentifier()) {
                auto iter = handlesMap.find(*backingStoreProperties->bufferSetIdentifier());
                if (iter != handlesMap.end())
                    backingStoreProperties->setBackendHandle(*iter->value);
            }
            if (const auto& backendHandle = backingStoreProperties->bufferHandle()) {
                if (const auto* sendRight = std::get_if<MachSendRight>(&backendHandle.value()))
                    sendRights.append(*sendRight);
            }
        }
    }

    WeakPtr weakThis { *this };

    for (auto& transaction : transactions) {
        commitLayerTreeTransaction(connection, CheckedRef { transaction.first }.get(), transaction.second);
        if (!weakThis)
            return;
    }

    // Keep IOSurface send rights alive until the transaction is commited, otherwise we will
    // prematurely drop the only reference to them, and `inUse` will be wrong for a brief window.
    if (!sendRights.isEmpty())
        [CATransaction addCommitHandler:^{ sendRights.clear(); } forPhase:kCATransactionPhasePostCommit];

    ProcessState& state = processStateForConnection(connection);
    if (std::exchange(state.commitLayerTreeMessageState, NeedsDisplayDidRefresh) == MissedCommit)
        didRefreshDisplay(&connection);

    scheduleDisplayRefreshCallbacks();
}

#if ENABLE(TOUCH_EVENT_REGIONS)
WebCore::TrackingType RemoteLayerTreeDrawingAreaProxy::eventTrackingTypeForPoint(WebCore::EventTrackingRegions::EventType eventType, IntPoint location)
{
    FloatPoint localLocation = location;
    if (auto* eventRegion = eventRegionForPoint(remoteLayerTreeHost().rootLayer(), localLocation))
        return eventRegion->eventTrackingTypeForPoint(eventType, roundedIntPoint(localLocation));
    return WebCore::TrackingType::NotTracking;
}
#endif

void RemoteLayerTreeDrawingAreaProxy::commitLayerTreeTransaction(IPC::Connection& connection, const RemoteLayerTreeTransaction& layerTreeTransaction, const RemoteScrollingCoordinatorTransaction& scrollingTreeTransaction)
{
    TraceScope tracingScope(CommitLayerTreeStart, CommitLayerTreeEnd);
    ProcessState& state = processStateForConnection(connection);

    LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree transaction:" << layerTreeTransaction.description());
    LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree scrolling tree:" << scrollingTreeTransaction.description());

    RefPtr page = this->page();
    if (!page)
        return;

    state.lastLayerTreeTransactionID = layerTreeTransaction.transactionID();
    if (!state.pendingLayerTreeTransactionID || state.pendingLayerTreeTransactionID->lessThanSameProcess(layerTreeTransaction.transactionID()))
        state.pendingLayerTreeTransactionID = state.lastLayerTreeTransactionID;

    bool didUpdateEditorState { false };
    if (layerTreeTransaction.isMainFrameProcessTransaction()) {
        ASSERT(layerTreeTransaction.transactionID() == m_lastVisibleTransactionID.next());
        m_transactionIDForPendingCACommit = layerTreeTransaction.transactionID();
        m_activityStateChangeID = layerTreeTransaction.activityStateChangeID();

        // FIXME(site-isolation): Editor state should be updated for subframes.
        didUpdateEditorState = layerTreeTransaction.hasEditorState() && page->updateEditorState(EditorState { layerTreeTransaction.editorState() }, WebPageProxy::ShouldMergeVisualEditorState::Yes);
    }

#if ENABLE(ASYNC_SCROLLING)
    std::optional<RequestedScrollData> requestedScroll;
#endif

    // Process any callbacks for unhiding content early, so that we
    // set the root node during the same CA transaction.
    if (layerTreeTransaction.isMainFrameProcessTransaction()) {
        for (auto& callbackID : layerTreeTransaction.callbackIDs()) {
            if (callbackID == m_replyForUnhidingContent) {
                m_replyForUnhidingContent = std::nullopt;
                break;
            }
        }
    }

    CheckedRef scrollingCoordinatorProxy = *page->scrollingCoordinatorProxy();
    auto commitLayerAndScrollingTrees = [&] {
        if (layerTreeTransaction.hasAnyLayerChanges())
            ++m_countOfTransactionsWithNonEmptyLayerChanges;

        if (m_remoteLayerTreeHost->updateLayerTree(connection, layerTreeTransaction)) {
            if (!m_replyForUnhidingContent)
                page->setRemoteLayerTreeRootNode(m_remoteLayerTreeHost->protectedRootNode().get());
            else
                m_remoteLayerTreeHost->detachRootLayer();
        }
#if ENABLE(ASYNC_SCROLLING)
        requestedScroll = scrollingCoordinatorProxy->commitScrollingTreeState(connection, scrollingTreeTransaction, layerTreeTransaction.remoteContextHostedIdentifier());
#endif
    };

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    state.acceleratedTimelineTimeOrigin = layerTreeTransaction.acceleratedTimelineTimeOrigin();
    state.animationCurrentTime = MonotonicTime::now();
#endif

    scrollingCoordinatorProxy->willCommitLayerAndScrollingTrees();
    commitLayerAndScrollingTrees();
    scrollingCoordinatorProxy->didCommitLayerAndScrollingTrees();

    page->didCommitLayerTree(layerTreeTransaction);
    didCommitLayerTree(connection, layerTreeTransaction, scrollingTreeTransaction);

#if ENABLE(ASYNC_SCROLLING)
    scrollingCoordinatorProxy->applyScrollingTreeLayerPositionsAfterCommit();
#if PLATFORM(IOS_FAMILY)
    page->adjustLayersForLayoutViewport(page->unobscuredContentRect().location(), page->unconstrainedLayoutViewportRect(), page->displayedContentScale());
#endif

    // Handle requested scroll position updates from the scrolling tree transaction after didCommitLayerTree()
    // has updated the view size based on the content size.
    if (requestedScroll) {
        auto currentScrollPosition = scrollingCoordinatorProxy->currentMainFrameScrollPosition();
        if (auto previousData = std::exchange(requestedScroll->requestedDataBeforeAnimatedScroll, std::nullopt)) {
            auto& [requestType, positionOrDeltaBeforeAnimatedScroll, scrollType, clamping] = *previousData;
            if (requestType != ScrollRequestType::CancelAnimatedScroll)
                currentScrollPosition = RequestedScrollData::computeDestinationPosition(currentScrollPosition, requestType, positionOrDeltaBeforeAnimatedScroll);
        }

        page->requestScroll(requestedScroll->destinationPosition(currentScrollPosition), layerTreeTransaction.scrollOrigin(), requestedScroll->animated);
    }
#endif // ENABLE(ASYNC_SCROLLING)

    if (m_debugIndicatorLayerTreeHost && layerTreeTransaction.isMainFrameProcessTransaction()) {
        float scale = indicatorScale(layerTreeTransaction.contentsSize());
        scrollingCoordinatorProxy->willCommitLayerAndScrollingTrees();
        bool rootLayerChanged = m_debugIndicatorLayerTreeHost->updateLayerTree(connection, layerTreeTransaction, scale);
        scrollingCoordinatorProxy->didCommitLayerAndScrollingTrees();
        IntPoint scrollPosition;
#if PLATFORM(MAC)
        scrollPosition = layerTreeTransaction.scrollPosition();
#endif
        updateDebugIndicator(layerTreeTransaction.contentsSize(), rootLayerChanged, scale, scrollPosition);
        m_debugIndicatorLayerTreeHost->rootLayer().name = @"Indicator host root";
    }

    page->layerTreeCommitComplete();

    if (didUpdateEditorState)
        page->dispatchDidUpdateEditorState();

    if (layerTreeTransaction.isMainFrameProcessTransaction()) {
        if (auto milestones = layerTreeTransaction.newlyReachedPaintingMilestones())
            page->didReachLayoutMilestone(milestones, WallTime::now());
    }

    for (auto& callbackID : layerTreeTransaction.callbackIDs()) {
        removeOutstandingPresentationUpdateCallback(connection, callbackID);
        if (auto callback = connection.takeAsyncReplyHandler(callbackID))
            callback(nullptr, nullptr);
    }
}

void RemoteLayerTreeDrawingAreaProxy::asyncSetLayerContents(WebCore::PlatformLayerIdentifier layerID, RemoteLayerBackingStoreProperties&& properties)
{
    m_remoteLayerTreeHost->asyncSetLayerContents(layerID, WTFMove(properties));
}

void RemoteLayerTreeDrawingAreaProxy::acceleratedAnimationDidStart(WebCore::PlatformLayerIdentifier layerID, const String& key, MonotonicTime startTime)
{
    if (RefPtr connection = connectionForIdentifier(layerID.processIdentifier()))
        connection->send(Messages::DrawingArea::AcceleratedAnimationDidStart(layerID, key, startTime), identifier());
}

void RemoteLayerTreeDrawingAreaProxy::acceleratedAnimationDidEnd(WebCore::PlatformLayerIdentifier layerID, const String& key)
{
    if (RefPtr connection = connectionForIdentifier(layerID.processIdentifier()))
        connection->send(Messages::DrawingArea::AcceleratedAnimationDidEnd(layerID, key), identifier());
}

static const float indicatorInset = 10;

FloatPoint RemoteLayerTreeDrawingAreaProxy::indicatorLocation() const
{
    FloatPoint tiledMapLocation;
    RefPtr page = this->page();
    if (!page)
        return { };

#if PLATFORM(IOS_FAMILY)
    tiledMapLocation = page->unobscuredContentRect().location().expandedTo(FloatPoint());
    tiledMapLocation = tiledMapLocation.expandedTo(page->exposedContentRect().location());

    float absoluteInset = indicatorInset / page->displayedContentScale();
    tiledMapLocation += FloatSize(absoluteInset, absoluteInset);
#else
    if (auto viewExposedRect = page->viewExposedRect())
        tiledMapLocation = viewExposedRect->location();

    tiledMapLocation += FloatSize(indicatorInset, indicatorInset);
    float scale = 1 / page->pageScaleFactor();
    tiledMapLocation.scale(scale);
#endif
    return tiledMapLocation;
}

void RemoteLayerTreeDrawingAreaProxy::updateDebugIndicatorPosition()
{
    if (!m_tileMapHostLayer)
        return;

    [m_tileMapHostLayer setPosition:indicatorLocation()];
}

float RemoteLayerTreeDrawingAreaProxy::indicatorScale(IntSize contentsSize) const
{
    // Pick a good scale.
    RefPtr page = this->page();
    if (!page)
        return 1;

    IntSize viewSize = page->viewSize();

    float scale = 1;
    if (!contentsSize.isEmpty()) {
        float widthScale = std::min<float>((viewSize.width() - 2 * indicatorInset) / contentsSize.width(), 0.05);
        scale = std::min(widthScale, static_cast<float>(viewSize.height() - 2 * indicatorInset) / contentsSize.height());
    }
    
    return scale;
}

void RemoteLayerTreeDrawingAreaProxy::updateDebugIndicator()
{
    // FIXME: we should also update live information during scale.
    updateDebugIndicatorPosition();
}

void RemoteLayerTreeDrawingAreaProxy::updateDebugIndicator(IntSize contentsSize, bool rootLayerChanged, float scale, const IntPoint& scrollPosition)
{
    // Make sure we're the last sublayer.
    RetainPtr rootLayer = m_remoteLayerTreeHost->rootLayer();
    [m_tileMapHostLayer removeFromSuperlayer];
    [rootLayer addSublayer:m_tileMapHostLayer.get()];

    [m_tileMapHostLayer setBounds:FloatRect(FloatPoint(), contentsSize)];
    [m_tileMapHostLayer setPosition:indicatorLocation()];
    [m_tileMapHostLayer setTransform:CATransform3DMakeScale(scale, scale, 1)];

    if (rootLayerChanged) {
        [m_tileMapHostLayer setSublayers:@[]];
        [m_tileMapHostLayer addSublayer:m_debugIndicatorLayerTreeHost->rootLayer()];
        [m_tileMapHostLayer addSublayer:m_exposedRectIndicatorLayer.get()];
    }
    
    const float indicatorBorderWidth = 1;
    float counterScaledBorder = indicatorBorderWidth / scale;

    [m_exposedRectIndicatorLayer setBorderWidth:counterScaledBorder];

    FloatRect scaledExposedRect;
    RefPtr page = this->page();
    if (!page)
        return;

#if PLATFORM(IOS_FAMILY)
    scaledExposedRect = page->exposedContentRect();
#else
    if (auto viewExposedRect = page->viewExposedRect())
        scaledExposedRect = *viewExposedRect;
    float counterScale = 1 / page->pageScaleFactor();
    scaledExposedRect.scale(counterScale);
#endif
    [m_exposedRectIndicatorLayer setPosition:scaledExposedRect.location()];
    [m_exposedRectIndicatorLayer setBounds:FloatRect(FloatPoint(), scaledExposedRect.size())];
}

void RemoteLayerTreeDrawingAreaProxy::initializeDebugIndicator()
{
    m_debugIndicatorLayerTreeHost = makeUnique<RemoteLayerTreeHost>(*this);
    m_debugIndicatorLayerTreeHost->setIsDebugLayerTreeHost(true);

    m_tileMapHostLayer = adoptNS([[CALayer alloc] init]);
    [m_tileMapHostLayer setName:@"Tile map host"];
    [m_tileMapHostLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
    [m_tileMapHostLayer setAnchorPoint:CGPointZero];
    [m_tileMapHostLayer setOpacity:0.8];
    [m_tileMapHostLayer setMasksToBounds:YES];
    [m_tileMapHostLayer setBorderWidth:2];

    RetainPtr colorSpace = sRGBColorSpaceSingleton();
    {
        const CGFloat components[] = { 1, 1, 1, 0.6 };
        RetainPtr<CGColorRef> color = adoptCF(CGColorCreate(colorSpace.get(), components));
        [m_tileMapHostLayer setBackgroundColor:color.get()];

        const CGFloat borderComponents[] = { 0, 0, 0, 1 };
        RetainPtr<CGColorRef> borderColor = adoptCF(CGColorCreate(colorSpace.get(), borderComponents));
        [m_tileMapHostLayer setBorderColor:borderColor.get()];
    }
    
    m_exposedRectIndicatorLayer = adoptNS([[CALayer alloc] init]);
    [m_exposedRectIndicatorLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
    [m_exposedRectIndicatorLayer setAnchorPoint:CGPointZero];

    {
        const CGFloat components[] = { 0, 1, 0, 1 };
        RetainPtr<CGColorRef> color = adoptCF(CGColorCreate(colorSpace.get(), components));
        [m_exposedRectIndicatorLayer setBorderColor:color.get()];
    }
}

bool RemoteLayerTreeDrawingAreaProxy::maybePauseDisplayRefreshCallbacks()
{
    if (m_webPageProxyProcessState.commitLayerTreeMessageState == NeedsDisplayDidRefresh || m_webPageProxyProcessState.commitLayerTreeMessageState == CommitLayerTreePending)
        return false;

    for (auto& pair : m_remotePageProcessState) {
        if (pair.value.commitLayerTreeMessageState == NeedsDisplayDidRefresh || pair.value.commitLayerTreeMessageState == CommitLayerTreePending)
            return false;
    }

    pauseDisplayRefreshCallbacks();
    return true;
}

void RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay()
{
    didRefreshDisplay(nullptr);
}

void RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay(ProcessState& state, IPC::Connection& connection)
{
    if (state.commitLayerTreeMessageState != NeedsDisplayDidRefresh) {
        if (state.commitLayerTreeMessageState == CommitLayerTreePending)
            state.commitLayerTreeMessageState = MissedCommit;
        return;
    }

    state.commitLayerTreeMessageState = CommitLayerTreePending;

    if (&state == &m_webPageProxyProcessState) {
        if (RefPtr page = this->page())
            page->checkedScrollingCoordinatorProxy()->sendScrollingTreeNodeUpdate();
    }

    // Waiting for CA to commit is insufficient, because the render server can still be
    // using our backing store. We can improve this by waiting for the render server to commit
    // if we find API to do so, but for now we will make extra buffers if need be.
    connection.send(Messages::DrawingArea::DisplayDidRefresh(), identifier());

#if ASSERT_ENABLED
    if (&state == &m_webPageProxyProcessState)
        m_lastVisibleTransactionID = m_transactionIDForPendingCACommit;
#endif
}

void RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay(IPC::Connection* connection)
{
    RefPtr page = this->page();
    if (!page || !page->hasRunningProcess())
        return;

    if (connection) {
        ProcessState& state = processStateForConnection(*connection);
        didRefreshDisplay(state, *connection);
    } else {
        forEachProcessState([&](ProcessState& state, WebProcessProxy& webProcess) {
            if (webProcess.hasConnection())
                didRefreshDisplay(state, webProcess.protectedConnection());
        });
    }

    if (maybePauseDisplayRefreshCallbacks())
        return;

    if (RefPtr page = this->page())
        page->didUpdateActivityState();
}

void RemoteLayerTreeDrawingAreaProxy::waitForDidUpdateActivityState(ActivityStateChangeID activityStateChangeID)
{
    ASSERT(activityStateChangeID != ActivityStateChangeAsynchronous);

    if (!webProcessProxy().hasConnection() || activityStateChangeID == ActivityStateChangeAsynchronous)
        return;

    Ref connection = webProcessProxy().connection();

    // We must send the didUpdate message before blocking on the next commit, otherwise
    // we can be guaranteed that the next commit won't come until after the waitForAndDispatchImmediately times out.
    if (m_webPageProxyProcessState.commitLayerTreeMessageState == NeedsDisplayDidRefresh)
        didRefreshDisplay(connection.ptr());

    static Seconds activityStateUpdateTimeout = [] {
        if (RetainPtr<id> value = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitOverrideActivityStateUpdateTimeout"])
            return Seconds([value doubleValue]);
        return 250_ms;
    }();

    WeakPtr weakThis { *this };
    auto startTime = MonotonicTime::now();
    while (connection->waitForAndDispatchImmediately<Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree>(identifier(), activityStateUpdateTimeout - (MonotonicTime::now() - startTime), IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives) == IPC::Error::NoError) {
        if (!weakThis || activityStateChangeID <= m_activityStateChangeID)
            return;

        if (m_webPageProxyProcessState.commitLayerTreeMessageState == NeedsDisplayDidRefresh)
            didRefreshDisplay(connection.ptr());
    }
}

void RemoteLayerTreeDrawingAreaProxy::hideContentUntilPendingUpdate()
{
    m_replyForUnhidingContent = webProcessProxy().sendWithAsyncReply(Messages::DrawingArea::DispatchAfterEnsuringDrawing(), [] () { }, messageSenderDestinationID(), { }, WebProcessProxy::ShouldStartProcessThrottlerActivity::No);
    m_remoteLayerTreeHost->detachRootLayer();
}

void RemoteLayerTreeDrawingAreaProxy::hideContentUntilAnyUpdate()
{
    m_remoteLayerTreeHost->detachRootLayer();
}

bool RemoteLayerTreeDrawingAreaProxy::hasVisibleContent() const
{
    return m_remoteLayerTreeHost->rootLayer();
}

CALayer *RemoteLayerTreeDrawingAreaProxy::layerWithIDForTesting(WebCore::PlatformLayerIdentifier layerID) const
{
    return m_remoteLayerTreeHost->layerWithIDForTesting(layerID);
}

void RemoteLayerTreeDrawingAreaProxy::minimumSizeForAutoLayoutDidChange()
{
    RefPtr page = this->page();
    if (!page || !page->hasRunningProcess())
        return;

    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::sizeToContentAutoSizeMaximumSizeDidChange()
{
    RefPtr page = this->page();
    if (!page || !page->hasRunningProcess())
        return;

    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
void RemoteLayerTreeDrawingAreaProxy::animationsWereAddedToNode(RemoteLayerTreeNode& node)
{
    if (RefPtr page = this->page())
        page->checkedScrollingCoordinatorProxy()->animationsWereAddedToNode(node);
}

void RemoteLayerTreeDrawingAreaProxy::animationsWereRemovedFromNode(RemoteLayerTreeNode& node)
{
    if (RefPtr page = this->page())
        page->checkedScrollingCoordinatorProxy()->animationsWereRemovedFromNode(node);
}

Seconds RemoteLayerTreeDrawingAreaProxy::acceleratedTimelineTimeOrigin(WebCore::ProcessIdentifier processIdentifier) const
{
    const auto& state = processStateForIdentifier(processIdentifier);
    return state.acceleratedTimelineTimeOrigin;
}

MonotonicTime RemoteLayerTreeDrawingAreaProxy::animationCurrentTime(WebCore::ProcessIdentifier processIdentifier) const
{
    const auto& state = processStateForIdentifier(processIdentifier);
    return state.animationCurrentTime;
}

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)

} // namespace WebKit
