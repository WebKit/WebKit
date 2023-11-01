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

namespace WebKit {
using namespace IPC;
using namespace WebCore;

RemoteLayerTreeDrawingAreaProxy::RemoteLayerTreeDrawingAreaProxy(WebPageProxy& pageProxy)
    : DrawingAreaProxy(DrawingAreaType::RemoteLayerTree, pageProxy)
    , m_remoteLayerTreeHost(makeUnique<RemoteLayerTreeHost>(*this))
{
    // We don't want to pool surfaces in the UI process.
    // FIXME: We should do this somewhere else.
    IOSurfacePool::sharedPool().setPoolSize(0);

    if (pageProxy.preferences().tiledScrollingIndicatorVisible())
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
    m_remotePageProcessState.add(proxy, ProcessState { });
}

void RemoteLayerTreeDrawingAreaProxy::removeRemotePageDrawingAreaProxy(RemotePageDrawingAreaProxy& proxy)
{
    ASSERT(m_remotePageProcessState.contains(proxy));
    m_remotePageProcessState.remove(proxy);
}

std::unique_ptr<RemoteLayerTreeHost> RemoteLayerTreeDrawingAreaProxy::detachRemoteLayerTreeHost()
{
    m_remoteLayerTreeHost->detachFromDrawingArea();
    return WTFMove(m_remoteLayerTreeHost);
}

void RemoteLayerTreeDrawingAreaProxy::sizeDidChange()
{
    if (!m_webPageProxy->hasRunningProcess())
        return;
    if (auto scrollingCoordinator = m_webPageProxy->scrollingCoordinatorProxy())
        scrollingCoordinator->viewSizeDidChange();

    if (m_isWaitingForDidUpdateGeometry)
        return;
    sendUpdateGeometry();
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

void RemoteLayerTreeDrawingAreaProxy::deviceScaleFactorDidChange()
{
    protectedWebPageProxy()->send(Messages::DrawingArea::SetDeviceScaleFactor(m_webPageProxy->deviceScaleFactor()), m_identifier);
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
    protectedWebPageProxy()->sendWithAsyncReply(Messages::DrawingArea::UpdateGeometry(m_size, false /* flushSynchronously */, MachSendRight()), [weakThis = WeakPtr { this }] {
        if (!weakThis)
            return;
        weakThis->didUpdateGeometry();
    }, m_identifier);
}

RemoteLayerTreeDrawingAreaProxy::ProcessState& RemoteLayerTreeDrawingAreaProxy::processStateForConnection(IPC::Connection& connection)
{
    for (auto pair : m_remotePageProcessState) {
        if (pair.key.process().hasConnection() && pair.key.process().connection() == &connection)
            return pair.value;
    }

    ASSERT(m_webPageProxy->process().hasConnection() && &connection == m_webPageProxy->process().connection());
    return m_webPageProxyProcessState;
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

void RemoteLayerTreeDrawingAreaProxy::commitLayerTree(IPC::Connection& connection, const Vector<std::pair<RemoteLayerTreeTransaction, RemoteScrollingCoordinatorTransaction>>& transactions)
{
    Vector<MachSendRight> sendRights;
    for (auto& transaction : transactions) {
        // commitLayerTreeTransaction consumes the incoming buffers, so we need to grab them first.
        for (auto& [layerID, properties] : transaction.first.changedLayerProperties()) {
            const auto* backingStoreProperties = properties->backingStoreOrProperties.properties.get();
            if (!backingStoreProperties)
                continue;
            if (const auto& backendHandle = backingStoreProperties->bufferHandle()) {
                if (const auto* sendRight = std::get_if<MachSendRight>(&backendHandle.value()))
                    sendRights.append(*sendRight);
            }
        }
    }

    for (auto& transaction : transactions)
        commitLayerTreeTransaction(connection, transaction.first, transaction.second);

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
        ASSERT(layerTreeTransaction.transactionID() == state.lastVisibleTransactionID.next());
        state.transactionIDForPendingCACommit = layerTreeTransaction.transactionID();
        state.activityStateChangeID = layerTreeTransaction.activityStateChangeID();

        didUpdateEditorState = layerTreeTransaction.hasEditorState() && webPageProxy->updateEditorState(layerTreeTransaction.editorState(), WebPageProxy::ShouldMergeVisualEditorState::Yes);
    }

#if ENABLE(ASYNC_SCROLLING)
    std::optional<RequestedScrollData> requestedScroll;
#endif

    auto commitLayerAndScrollingTrees = [&] {
        if (layerTreeTransaction.hasAnyLayerChanges())
            ++m_countOfTransactionsWithNonEmptyLayerChanges;

        if (m_remoteLayerTreeHost->updateLayerTree(layerTreeTransaction)) {
            if (!m_replyForUnhidingContent)
                webPageProxy->setRemoteLayerTreeRootNode(m_remoteLayerTreeHost->rootNode());
            else
                m_remoteLayerTreeHost->detachRootLayer();
        }

#if ENABLE(ASYNC_SCROLLING)
        // FIXME: Making scrolling trees work with site isolation.
        if (layerTreeTransaction.isMainFrameProcessTransaction())
            requestedScroll = webPageProxy->scrollingCoordinatorProxy()->commitScrollingTreeState(scrollingTreeTransaction);
#endif
    };

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

    if (m_debugIndicatorLayerTreeHost) {
        float scale = indicatorScale(layerTreeTransaction.contentsSize());
        bool rootLayerChanged = m_debugIndicatorLayerTreeHost->updateLayerTree(layerTreeTransaction, scale);
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

    if (auto milestones = layerTreeTransaction.newlyReachedPaintingMilestones())
        webPageProxy->didReachLayoutMilestone(milestones);

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
    protectedWebPageProxy()->send(Messages::DrawingArea::AcceleratedAnimationDidStart(layerID, key, startTime), m_identifier);
}

void RemoteLayerTreeDrawingAreaProxy::acceleratedAnimationDidEnd(WebCore::PlatformLayerIdentifier layerID, const String& key)
{
    protectedWebPageProxy()->send(Messages::DrawingArea::AcceleratedAnimationDidEnd(layerID, key), m_identifier);
}

static const float indicatorInset = 10;

FloatPoint RemoteLayerTreeDrawingAreaProxy::indicatorLocation() const
{
    FloatPoint tiledMapLocation;
#if PLATFORM(IOS_FAMILY)
    tiledMapLocation = m_webPageProxy->unobscuredContentRect().location().expandedTo(FloatPoint());
    tiledMapLocation = tiledMapLocation.expandedTo(m_webPageProxy->exposedContentRect().location());

    float absoluteInset = indicatorInset / m_webPageProxy->displayedContentScale();
    tiledMapLocation += FloatSize(absoluteInset, absoluteInset);
#else
    if (auto viewExposedRect = m_webPageProxy->viewExposedRect())
        tiledMapLocation = viewExposedRect->location();

    tiledMapLocation += FloatSize(indicatorInset, indicatorInset);
    float scale = 1 / m_webPageProxy->pageScaleFactor();
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
    IntSize viewSize = m_webPageProxy->viewSize();

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
#if PLATFORM(IOS_FAMILY)
    scaledExposedRect = m_webPageProxy->exposedContentRect();
#else
    if (auto viewExposedRect = m_webPageProxy->viewExposedRect())
        scaledExposedRect = *viewExposedRect;
    float counterScale = 1 / m_webPageProxy->pageScaleFactor();
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

    for (auto pair : m_remotePageProcessState) {
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

    if (m_webPageProxy->process().connection() == &connection)
        m_webPageProxy->scrollingCoordinatorProxy()->sendScrollingTreeNodeDidScroll();

    // Waiting for CA to commit is insufficient, because the render server can still be
    // using our backing store. We can improve this by waiting for the render server to commit
    // if we find API to do so, but for now we will make extra buffers if need be.
    connection.send(Messages::DrawingArea::DisplayDidRefresh(), m_identifier);

#if ASSERT_ENABLED
    state.lastVisibleTransactionID = state.transactionIDForPendingCACommit;
#endif
}

void RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay(IPC::Connection* connection)
{
    if (!m_webPageProxy->hasRunningProcess())
        return;

    if (connection) {
        ProcessState& state = processStateForConnection(*connection);
        didRefreshDisplay(state, *connection);
    } else {
        if (m_webPageProxy->process().hasConnection())
            didRefreshDisplay(m_webPageProxyProcessState, *m_webPageProxy->process().connection());
        for (auto pair : m_remotePageProcessState) {
            if (pair.key.process().hasConnection())
                didRefreshDisplay(pair.value, *pair.key.process().connection());
        }
    }

    if (maybePauseDisplayRefreshCallbacks())
        return;

    m_webPageProxy->didUpdateActivityState();
}

void RemoteLayerTreeDrawingAreaProxy::waitForDidUpdateActivityState(ActivityStateChangeID activityStateChangeID, WebProcessProxy& process)
{
    ASSERT(activityStateChangeID != ActivityStateChangeAsynchronous);

    if (!process.hasConnection() || activityStateChangeID == ActivityStateChangeAsynchronous)
        return;

    ProcessState& state = processStateForConnection(*process.connection());

    // We must send the didUpdate message before blocking on the next commit, otherwise
    // we can be guaranteed that the next commit won't come until after the waitForAndDispatchImmediately times out.
    if (state.commitLayerTreeMessageState == NeedsDisplayDidRefresh)
        didRefreshDisplay(process.connection());

    static Seconds activityStateUpdateTimeout = [] {
        if (id value = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitOverrideActivityStateUpdateTimeout"])
            return Seconds([value doubleValue]);
        return Seconds::fromMilliseconds(250);
    }();

    WeakPtr weakThis { *this };
    auto startTime = MonotonicTime::now();
    while (process.connection()->waitForAndDispatchImmediately<Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree>(m_identifier, activityStateUpdateTimeout - (MonotonicTime::now() - startTime), IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives) == IPC::Error::NoError) {
        if (!weakThis || activityStateChangeID <= state.activityStateChangeID)
            return;

        if (state.commitLayerTreeMessageState == NeedsDisplayDidRefresh)
            didRefreshDisplay(process.connection());
    }
}

void RemoteLayerTreeDrawingAreaProxy::hideContentUntilPendingUpdate()
{
    if (m_replyForUnhidingContent && protectedWebPageProxy()->process().hasConnection()) {
        if (auto replyHandlerToCancel = protectedWebPageProxy()->process().connection()->takeAsyncReplyHandler(m_replyForUnhidingContent))
            replyHandlerToCancel(nullptr);
    }

    m_replyForUnhidingContent = protectedWebPageProxy()->sendWithAsyncReply(Messages::DrawingArea::DispatchAfterEnsuringDrawing(), [weakThis = WeakPtr { this }] {
        if (weakThis) {
            weakThis->protectedWebPageProxy()->setRemoteLayerTreeRootNode(weakThis->m_remoteLayerTreeHost->rootNode());
            weakThis->m_replyForUnhidingContent = AsyncReplyID { };
        }
    }, identifier());
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
    if (m_webPageProxy->windowKind() == WindowKind::InProcessSnapshotting)
        m_remoteLayerTreeHost->mapAllIOSurfaceBackingStore();
}

void RemoteLayerTreeDrawingAreaProxy::minimumSizeForAutoLayoutDidChange()
{
    if (!m_webPageProxy->hasRunningProcess())
        return;

    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::sizeToContentAutoSizeMaximumSizeDidChange()
{
    if (!m_webPageProxy->hasRunningProcess())
        return;

    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

} // namespace WebKit
