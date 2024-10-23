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
    : DrawingAreaProxy(DrawingAreaType::RemoteLayerTree, pageProxy, webProcessProxy)
    , m_remoteLayerTreeHost(makeUnique<RemoteLayerTreeHost>(*this))
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
    if (!protectedWebPageProxy()->hasRunningProcess())
        return;
    if (auto scrollingCoordinator = m_webPageProxy->scrollingCoordinatorProxy())
        scrollingCoordinator->viewSizeDidChange();

    if (m_isWaitingForDidUpdateGeometry)
        return;
    sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::remotePageProcessDidTerminate(WebCore::ProcessIdentifier processIdentifier)
{
    if (!m_remoteLayerTreeHost)
        return;

    if (auto* scrollingCoordinator = m_webPageProxy->scrollingCoordinatorProxy()) {
        scrollingCoordinator->willCommitLayerAndScrollingTrees();
        m_remoteLayerTreeHost->remotePageProcessDidTerminate(processIdentifier);
        scrollingCoordinator->didCommitLayerAndScrollingTrees();
    }
}

void RemoteLayerTreeDrawingAreaProxy::viewWillStartLiveResize()
{
    if (auto scrollingCoordinator = m_webPageProxy->scrollingCoordinatorProxy())
        scrollingCoordinator->viewWillStartLiveResize();
}

void RemoteLayerTreeDrawingAreaProxy::viewWillEndLiveResize()
{
    if (auto scrollingCoordinator = m_webPageProxy->scrollingCoordinatorProxy())
        scrollingCoordinator->viewWillEndLiveResize();
}

void RemoteLayerTreeDrawingAreaProxy::deviceScaleFactorDidChange(CompletionHandler<void()>&& completionHandler)
{
    Ref aggregator = CallbackAggregator::create(WTFMove(completionHandler));
    forEachProcessState([&](ProcessState& state, WebProcessProxy& webProcess) {
        webProcess.sendWithAsyncReply(Messages::DrawingArea::SetDeviceScaleFactor(protectedWebPageProxy()->deviceScaleFactor()), [aggregator] { }, identifier());
    });
}

void RemoteLayerTreeDrawingAreaProxy::didUpdateGeometry()
{
    ASSERT(m_isWaitingForDidUpdateGeometry);

    m_isWaitingForDidUpdateGeometry = false;

    auto webPageProxy = protectedWebPageProxy();
    IntSize minimumSizeForAutoLayout = webPageProxy->minimumSizeForAutoLayout();
    IntSize sizeToContentAutoSizeMaximumSize = webPageProxy->sizeToContentAutoSizeMaximumSize();

    // If the WKView was resized while we were waiting for a DidUpdateGeometry reply from the web process,
    // we need to resend the new size here.
    if (m_lastSentSize != m_size || m_lastSentMinimumSizeForAutoLayout != minimumSizeForAutoLayout || m_lastSentSizeToContentAutoSizeMaximumSize != sizeToContentAutoSizeMaximumSize)
        sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::sendUpdateGeometry()
{
    auto webPageProxy = protectedWebPageProxy();
    m_lastSentMinimumSizeForAutoLayout = webPageProxy->minimumSizeForAutoLayout();
    m_lastSentSizeToContentAutoSizeMaximumSize = webPageProxy->sizeToContentAutoSizeMaximumSize();
    m_lastSentSize = m_size;
    m_isWaitingForDidUpdateGeometry = true;
    sendWithAsyncReply(Messages::DrawingArea::UpdateGeometry(m_size, false /* flushSynchronously */, MachSendRight()), [weakThis = WeakPtr { this }] {
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

    RELEASE_ASSERT(m_webProcessProxy->hasConnection(connection));
    return m_webPageProxyProcessState;
}

void RemoteLayerTreeDrawingAreaProxy::forEachProcessState(Function<void(ProcessState&, WebProcessProxy&)>&& callback)
{
    callback(m_webPageProxyProcessState, m_webProcessProxy);
    for (auto& [key, value] : m_remotePageProcessState) {
        RefPtr webProcess = WebProcessProxy::processForIdentifier(key);
        if (webProcess)
            callback(value, *webProcess);
    }
}

const RemoteLayerTreeDrawingAreaProxy::ProcessState& RemoteLayerTreeDrawingAreaProxy::processStateForIdentifier(WebCore::ProcessIdentifier identifier) const
{
    if (m_webProcessProxy->coreProcessIdentifier() == identifier)
        return m_webPageProxyProcessState;

    auto iter = m_remotePageProcessState.find(identifier);
    RELEASE_ASSERT(iter.get());
    return *iter.values();
}

IPC::Connection& RemoteLayerTreeDrawingAreaProxy::connectionForIdentifier(WebCore::ProcessIdentifier processIdentifier)
{
    RefPtr webProcess = WebProcessProxy::processForIdentifier(processIdentifier);
    RELEASE_ASSERT(webProcess);
    RELEASE_ASSERT(webProcess->hasConnection());
    return webProcess->connection();
}

void RemoteLayerTreeDrawingAreaProxy::commitLayerTreeNotTriggered(IPC::Connection& connection, TransactionID nextCommitTransactionID)
{
    ProcessState& state = processStateForConnection(connection);
    if (nextCommitTransactionID <= state.lastLayerTreeTransactionID) {
        LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTreeNotTriggered nextCommitTransactionID=" << nextCommitTransactionID << ") already obsoleted by m_lastLayerTreeTransactionID=" << state.lastLayerTreeTransactionID);
        return;
    }

    state.commitLayerTreeMessageState = Idle;

    maybePauseDisplayRefreshCallbacks();

#if ENABLE(ASYNC_SCROLLING)
    m_webPageProxy->scrollingCoordinatorProxy()->applyScrollingTreeLayerPositionsAfterCommit();
#endif
}

void RemoteLayerTreeDrawingAreaProxy::willCommitLayerTree(IPC::Connection& connection, TransactionID transactionID)
{
    ProcessState& state = processStateForConnection(connection);
    if (transactionID <= state.lastLayerTreeTransactionID)
        return;

    state.pendingLayerTreeTransactionID = transactionID;
}

void RemoteLayerTreeDrawingAreaProxy::commitLayerTree(IPC::Connection& connection, const Vector<std::pair<RemoteLayerTreeTransaction, RemoteScrollingCoordinatorTransaction>>& transactions, HashMap<RemoteImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>&& handlesMap)
{
    Vector<MachSendRight> sendRights;
    for (auto& transaction : transactions) {
        // commitLayerTreeTransaction consumes the incoming buffers, so we need to grab them first.
        for (auto& [layerID, properties] : transaction.first.changedLayerProperties()) {
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
        commitLayerTreeTransaction(connection, transaction.first, transaction.second);
        if (!weakThis)
            return;
    }

    // Keep IOSurface send rights alive until the transaction is commited, otherwise we will
    // prematurely drop the only reference to them, and `inUse` will be wrong for a brief window.
    if (!sendRights.isEmpty())
        [CATransaction addCommitHandler:[sendRights = WTFMove(sendRights)]() { } forPhase:kCATransactionPhasePostCommit];

    ProcessState& state = processStateForConnection(connection);
    if (std::exchange(state.commitLayerTreeMessageState, NeedsDisplayDidRefresh) == MissedCommit)
        didRefreshDisplay(&connection);

    scheduleDisplayRefreshCallbacks();
}

void RemoteLayerTreeDrawingAreaProxy::commitLayerTreeTransaction(IPC::Connection& connection, const RemoteLayerTreeTransaction& layerTreeTransaction, const RemoteScrollingCoordinatorTransaction& scrollingTreeTransaction)
{
    TraceScope tracingScope(CommitLayerTreeStart, CommitLayerTreeEnd);
    ProcessState& state = processStateForConnection(connection);

    LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree transaction:" << layerTreeTransaction.description());
    LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree scrolling tree:" << scrollingTreeTransaction.description());

    state.lastLayerTreeTransactionID = layerTreeTransaction.transactionID();
    if (state.pendingLayerTreeTransactionID < state.lastLayerTreeTransactionID)
        state.pendingLayerTreeTransactionID = state.lastLayerTreeTransactionID;

    bool didUpdateEditorState { false };
    auto webPageProxy = protectedWebPageProxy();
    if (layerTreeTransaction.isMainFrameProcessTransaction()) {
        ASSERT(layerTreeTransaction.transactionID() == m_lastVisibleTransactionID.next());
        m_transactionIDForPendingCACommit = layerTreeTransaction.transactionID();
        m_activityStateChangeID = layerTreeTransaction.activityStateChangeID();

        // FIXME(site-isolation): Editor state should be updated for subframes.
        didUpdateEditorState = layerTreeTransaction.hasEditorState() && webPageProxy->updateEditorState(layerTreeTransaction.editorState(), WebPageProxy::ShouldMergeVisualEditorState::Yes);
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

    auto commitLayerAndScrollingTrees = [&] {
        if (layerTreeTransaction.hasAnyLayerChanges())
            ++m_countOfTransactionsWithNonEmptyLayerChanges;

        if (m_remoteLayerTreeHost->updateLayerTree(connection, layerTreeTransaction)) {
            if (!m_replyForUnhidingContent)
                webPageProxy->setRemoteLayerTreeRootNode(m_remoteLayerTreeHost->protectedRootNode().get());
            else
                m_remoteLayerTreeHost->detachRootLayer();
        }

#if ENABLE(ASYNC_SCROLLING)
#if PLATFORM(IOS_FAMILY)
        if (!layerTreeTransaction.isMainFrameProcessTransaction()) {
            // TODO: rdar://123104203 Making scrolling trees work with site isolation on iOS.
            return;
        }
#endif
        requestedScroll = webPageProxy->scrollingCoordinatorProxy()->commitScrollingTreeState(connection, scrollingTreeTransaction, layerTreeTransaction.remoteContextHostedIdentifier());
#endif
    };

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    state.acceleratedTimelineTimeOrigin = layerTreeTransaction.acceleratedTimelineTimeOrigin();
    state.animationCurrentTime = MonotonicTime::now();
#endif

    webPageProxy->scrollingCoordinatorProxy()->willCommitLayerAndScrollingTrees();
    commitLayerAndScrollingTrees();
    webPageProxy->scrollingCoordinatorProxy()->didCommitLayerAndScrollingTrees();

    webPageProxy->didCommitLayerTree(layerTreeTransaction);
    didCommitLayerTree(connection, layerTreeTransaction, scrollingTreeTransaction);

#if ENABLE(ASYNC_SCROLLING)
    webPageProxy->scrollingCoordinatorProxy()->applyScrollingTreeLayerPositionsAfterCommit();
#if PLATFORM(IOS_FAMILY)
    webPageProxy->adjustLayersForLayoutViewport(webPageProxy->unobscuredContentRect().location(), webPageProxy->unconstrainedLayoutViewportRect(), webPageProxy->displayedContentScale());
#endif

    // Handle requested scroll position updates from the scrolling tree transaction after didCommitLayerTree()
    // has updated the view size based on the content size.
    if (requestedScroll) {
        auto currentScrollPosition = webPageProxy->scrollingCoordinatorProxy()->currentMainFrameScrollPosition();
        if (auto previousData = std::exchange(requestedScroll->requestedDataBeforeAnimatedScroll, std::nullopt)) {
            auto& [requestType, positionOrDeltaBeforeAnimatedScroll, scrollType, clamping] = *previousData;
            if (requestType != ScrollRequestType::CancelAnimatedScroll)
                currentScrollPosition = RequestedScrollData::computeDestinationPosition(currentScrollPosition, requestType, positionOrDeltaBeforeAnimatedScroll);
        }

        webPageProxy->requestScroll(requestedScroll->destinationPosition(currentScrollPosition), layerTreeTransaction.scrollOrigin(), requestedScroll->animated);
    }
#endif // ENABLE(ASYNC_SCROLLING)

    if (m_debugIndicatorLayerTreeHost && layerTreeTransaction.isMainFrameProcessTransaction()) {
        float scale = indicatorScale(layerTreeTransaction.contentsSize());
        webPageProxy->scrollingCoordinatorProxy()->willCommitLayerAndScrollingTrees();
        bool rootLayerChanged = m_debugIndicatorLayerTreeHost->updateLayerTree(connection, layerTreeTransaction, scale);
        webPageProxy->scrollingCoordinatorProxy()->didCommitLayerAndScrollingTrees();
        IntPoint scrollPosition;
#if PLATFORM(MAC)
        scrollPosition = layerTreeTransaction.scrollPosition();
#endif
        updateDebugIndicator(layerTreeTransaction.contentsSize(), rootLayerChanged, scale, scrollPosition);
        m_debugIndicatorLayerTreeHost->rootLayer().name = @"Indicator host root";
    }

    webPageProxy->layerTreeCommitComplete();

    if (didUpdateEditorState)
        webPageProxy->dispatchDidUpdateEditorState();

    if (layerTreeTransaction.isMainFrameProcessTransaction()) {
        if (auto milestones = layerTreeTransaction.newlyReachedPaintingMilestones())
            webPageProxy->didReachLayoutMilestone(milestones, WallTime::now());
    }

    for (auto& callbackID : layerTreeTransaction.callbackIDs()) {
        if (auto callback = connection.takeAsyncReplyHandler(callbackID))
            callback(nullptr);
    }
}

void RemoteLayerTreeDrawingAreaProxy::asyncSetLayerContents(WebCore::PlatformLayerIdentifier layerID, ImageBufferBackendHandle&& handle, const WebCore::RenderingResourceIdentifier& identifier)
{
    m_remoteLayerTreeHost->asyncSetLayerContents(layerID, WTFMove(handle), identifier);
}

void RemoteLayerTreeDrawingAreaProxy::acceleratedAnimationDidStart(WebCore::PlatformLayerIdentifier layerID, const String& key, MonotonicTime startTime)
{
    Ref connection = connectionForIdentifier(layerID.processIdentifier());
    connection->send(Messages::DrawingArea::AcceleratedAnimationDidStart(layerID, key, startTime), identifier());
}

void RemoteLayerTreeDrawingAreaProxy::acceleratedAnimationDidEnd(WebCore::PlatformLayerIdentifier layerID, const String& key)
{
    Ref connection = connectionForIdentifier(layerID.processIdentifier());
    connection->send(Messages::DrawingArea::AcceleratedAnimationDidEnd(layerID, key), identifier());
}

static const float indicatorInset = 10;

FloatPoint RemoteLayerTreeDrawingAreaProxy::indicatorLocation() const
{
    FloatPoint tiledMapLocation;
    Ref webPageProxy = m_webPageProxy.get();
#if PLATFORM(IOS_FAMILY)
    tiledMapLocation = webPageProxy->unobscuredContentRect().location().expandedTo(FloatPoint());
    tiledMapLocation = tiledMapLocation.expandedTo(webPageProxy->exposedContentRect().location());

    float absoluteInset = indicatorInset / webPageProxy->displayedContentScale();
    tiledMapLocation += FloatSize(absoluteInset, absoluteInset);
#else
    if (auto viewExposedRect = webPageProxy->viewExposedRect())
        tiledMapLocation = viewExposedRect->location();

    tiledMapLocation += FloatSize(indicatorInset, indicatorInset);
    float scale = 1 / webPageProxy->pageScaleFactor();
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
    IntSize viewSize = protectedWebPageProxy()->viewSize();

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
    CALayer *rootLayer = m_remoteLayerTreeHost->rootLayer();
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
    Ref webPageProxy = m_webPageProxy.get();
#if PLATFORM(IOS_FAMILY)
    scaledExposedRect = webPageProxy->exposedContentRect();
#else
    if (auto viewExposedRect = webPageProxy->viewExposedRect())
        scaledExposedRect = *viewExposedRect;
    float counterScale = 1 / webPageProxy->pageScaleFactor();
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

    CGColorSpaceRef colorSpace = sRGBColorSpaceRef();
    {
        const CGFloat components[] = { 1, 1, 1, 0.6 };
        RetainPtr<CGColorRef> color = adoptCF(CGColorCreate(colorSpace, components));
        [m_tileMapHostLayer setBackgroundColor:color.get()];

        const CGFloat borderComponents[] = { 0, 0, 0, 1 };
        RetainPtr<CGColorRef> borderColor = adoptCF(CGColorCreate(colorSpace, borderComponents));
        [m_tileMapHostLayer setBorderColor:borderColor.get()];
    }
    
    m_exposedRectIndicatorLayer = adoptNS([[CALayer alloc] init]);
    [m_exposedRectIndicatorLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
    [m_exposedRectIndicatorLayer setAnchorPoint:CGPointZero];

    {
        const CGFloat components[] = { 0, 1, 0, 1 };
        RetainPtr<CGColorRef> color = adoptCF(CGColorCreate(colorSpace, components));
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

    if (&state == &m_webPageProxyProcessState)
        m_webPageProxy->scrollingCoordinatorProxy()->sendScrollingTreeNodeDidScroll();

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
    if (!protectedWebPageProxy()->hasRunningProcess())
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

    m_webPageProxy->didUpdateActivityState();
}

void RemoteLayerTreeDrawingAreaProxy::waitForDidUpdateActivityState(ActivityStateChangeID activityStateChangeID)
{
    ASSERT(activityStateChangeID != ActivityStateChangeAsynchronous);

    if (!m_webProcessProxy->hasConnection() || activityStateChangeID == ActivityStateChangeAsynchronous)
        return;

    Ref connection = m_webProcessProxy->connection();

    // We must send the didUpdate message before blocking on the next commit, otherwise
    // we can be guaranteed that the next commit won't come until after the waitForAndDispatchImmediately times out.
    if (m_webPageProxyProcessState.commitLayerTreeMessageState == NeedsDisplayDidRefresh)
        didRefreshDisplay(connection.ptr());

    static Seconds activityStateUpdateTimeout = [] {
        if (id value = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitOverrideActivityStateUpdateTimeout"])
            return Seconds([value doubleValue]);
        return Seconds::fromMilliseconds(250);
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
    m_replyForUnhidingContent = protectedWebProcessProxy()->sendWithAsyncReply(Messages::DrawingArea::DispatchAfterEnsuringDrawing(), [] () { }, messageSenderDestinationID(), { }, WebProcessProxy::ShouldStartProcessThrottlerActivity::No);
    m_remoteLayerTreeHost->detachRootLayer();
}

void RemoteLayerTreeDrawingAreaProxy::hideContentUntilAnyUpdate()
{
    m_remoteLayerTreeHost->detachRootLayer();
}

void RemoteLayerTreeDrawingAreaProxy::prepareForAppSuspension()
{
    m_remoteLayerTreeHost->mapAllIOSurfaceBackingStore();
}

bool RemoteLayerTreeDrawingAreaProxy::hasVisibleContent() const
{
    return m_remoteLayerTreeHost->rootLayer();
}

CALayer *RemoteLayerTreeDrawingAreaProxy::layerWithIDForTesting(WebCore::PlatformLayerIdentifier layerID) const
{
    return m_remoteLayerTreeHost->layerWithIDForTesting(layerID);
}

void RemoteLayerTreeDrawingAreaProxy::windowKindDidChange()
{
    if (protectedWebPageProxy()->windowKind() == WindowKind::InProcessSnapshotting)
        m_remoteLayerTreeHost->mapAllIOSurfaceBackingStore();
}

void RemoteLayerTreeDrawingAreaProxy::minimumSizeForAutoLayoutDidChange()
{
    if (!protectedWebPageProxy()->hasRunningProcess())
        return;

    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::sizeToContentAutoSizeMaximumSizeDidChange()
{
    if (!protectedWebPageProxy()->hasRunningProcess())
        return;

    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
void RemoteLayerTreeDrawingAreaProxy::animationsWereAddedToNode(RemoteLayerTreeNode& node)
{
    protectedWebPageProxy()->scrollingCoordinatorProxy()->animationsWereAddedToNode(node);
}

void RemoteLayerTreeDrawingAreaProxy::animationsWereRemovedFromNode(RemoteLayerTreeNode& node)
{
    protectedWebPageProxy()->scrollingCoordinatorProxy()->animationsWereRemovedFromNode(node);
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
