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

    if (m_webPageProxy.preferences().tiledScrollingIndicatorVisible())
        initializeDebugIndicator();
}

RemoteLayerTreeDrawingAreaProxy::~RemoteLayerTreeDrawingAreaProxy() = default;

std::span<IPC::ReceiverName> RemoteLayerTreeDrawingAreaProxy::messageReceiverNames() const
{
    static std::array<IPC::ReceiverName, 2> names { Messages::DrawingAreaProxy::messageReceiverName(), Messages::RemoteLayerTreeDrawingAreaProxy::messageReceiverName() };
    return { names };
}

std::unique_ptr<RemoteLayerTreeHost> RemoteLayerTreeDrawingAreaProxy::detachRemoteLayerTreeHost()
{
    m_remoteLayerTreeHost->detachFromDrawingArea();
    return WTFMove(m_remoteLayerTreeHost);
}

void RemoteLayerTreeDrawingAreaProxy::sizeDidChange()
{
    if (!m_webPageProxy.hasRunningProcess())
        return;
    if (auto scrollingCoordinator = m_webPageProxy.scrollingCoordinatorProxy())
        scrollingCoordinator->viewSizeDidChange();

    if (m_isWaitingForDidUpdateGeometry)
        return;
    sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::viewWillStartLiveResize()
{
    if (auto scrollingCoordinator = m_webPageProxy.scrollingCoordinatorProxy())
        scrollingCoordinator->viewWillStartLiveResize();
}

void RemoteLayerTreeDrawingAreaProxy::viewWillEndLiveResize()
{
    if (auto scrollingCoordinator = m_webPageProxy.scrollingCoordinatorProxy())
        scrollingCoordinator->viewWillEndLiveResize();
}

void RemoteLayerTreeDrawingAreaProxy::deviceScaleFactorDidChange()
{
    m_webPageProxy.send(Messages::DrawingArea::SetDeviceScaleFactor(m_webPageProxy.deviceScaleFactor()), m_identifier);
}

void RemoteLayerTreeDrawingAreaProxy::didUpdateGeometry()
{
    ASSERT(m_isWaitingForDidUpdateGeometry);

    m_isWaitingForDidUpdateGeometry = false;

    IntSize minimumSizeForAutoLayout = m_webPageProxy.minimumSizeForAutoLayout();
    IntSize sizeToContentAutoSizeMaximumSize = m_webPageProxy.sizeToContentAutoSizeMaximumSize();

    // If the WKView was resized while we were waiting for a DidUpdateGeometry reply from the web process,
    // we need to resend the new size here.
    if (m_lastSentSize != m_size || m_lastSentMinimumSizeForAutoLayout != minimumSizeForAutoLayout || m_lastSentSizeToContentAutoSizeMaximumSize != sizeToContentAutoSizeMaximumSize)
        sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::sendUpdateGeometry()
{
    m_lastSentMinimumSizeForAutoLayout = m_webPageProxy.minimumSizeForAutoLayout();
    m_lastSentSizeToContentAutoSizeMaximumSize = m_webPageProxy.sizeToContentAutoSizeMaximumSize();
    m_lastSentSize = m_size;
    m_isWaitingForDidUpdateGeometry = true;
    m_webPageProxy.sendWithAsyncReply(Messages::DrawingArea::UpdateGeometry(m_size, false /* flushSynchronously */, MachSendRight()), [weakThis = WeakPtr { this }] {
        if (!weakThis)
            return;
        weakThis->didUpdateGeometry();
    }, m_identifier);
}

const char* RemoteLayerTreeDrawingAreaProxy::messageStateDescription()
{
    switch (m_commitLayerTreeMessageState) {
    case CommitLayerTreePending: return "Pend";
    case NeedsDisplayDidRefresh: return "Need";
    case MissedCommit: return "Miss";
    case Idle: return "Idle";
    default: return "????";
    }
}

void RemoteLayerTreeDrawingAreaProxy::commitLayerTreeNotTriggered(TransactionID nextCommitTransactionID)
{
    if (nextCommitTransactionID <= m_lastLayerTreeTransactionID) {
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeNotTriggered(nextCommitTransactionID=" << nextCommitTransactionID << ") - already obsoleted!");
        return;
    }

    ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeNotTriggered(nextCommitTransactionID=" << nextCommitTransactionID << ") -> m_commitLayerTreeMessageState=Idle, pauseDisplayRefreshCallbacks");
    m_commitLayerTreeMessageState = Idle;
    pauseDisplayRefreshCallbacks();
#if ENABLE(ASYNC_SCROLLING)
    m_webPageProxy.scrollingCoordinatorProxy()->applyScrollingTreeLayerPositionsAfterCommit();
#endif
}

void RemoteLayerTreeDrawingAreaProxy::willCommitLayerTree(TransactionID transactionID)
{
    if (transactionID <= m_lastLayerTreeTransactionID) {
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::willCommitLayerTree(transactionID=" << transactionID << ") - already obsoleted!");
        return;
    }

    ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::willCommitLayerTree(transactionID=" << transactionID << ")");
    m_pendingLayerTreeTransactionID = transactionID;
}

void RemoteLayerTreeDrawingAreaProxy::commitLayerTree(IPC::Connection& connection, const Vector<std::pair<RemoteLayerTreeTransaction, RemoteScrollingCoordinatorTransaction>>& transactions)
{
    constexpr auto shouldAddTransactionForId = [](auto id) -> bool {
        static constexpr unsigned behaviorChange = 32u;
        return (id / behaviorChange) & 0x1u;
    };
    auto id = transactions.isEmpty() ? 1u : transactions.first().first.transactionID().toUInt64();
    bool addTransaction = shouldAddTransactionForId(id) && false;
    bool addTransactionChanged = (shouldAddTransactionForId(id - 1u) != addTransaction) && false;

    if (addTransactionChanged)
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTree - CATransaction changed          =============================");

    if (addTransaction) {
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTree - before [CATransaction begin]   vvvvvvvvvvvvvvvvvvvvvvvvvvvvv");
        [CATransaction begin];
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTree - after  [CATransaction begin]");
    } else
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTree - WITHOUT [CATransaction begin]  v...v...v...v...v...v...v...v");

    for (auto& transaction : transactions)
        commitLayerTreeTransaction(connection, transaction.first, transaction.second);

    if (addTransaction) {
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTree - before [CATransaction commit]");
        [CATransaction commit];
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTree - after  [CATransaction commit]  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
    } else
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTree - WITHOUT [CATransaction commit] ^```^```^```^```^```^```^```^");
}

void RemoteLayerTreeDrawingAreaProxy::commitLayerTreeTransaction(IPC::Connection& connection, const RemoteLayerTreeTransaction& layerTreeTransaction, const RemoteScrollingCoordinatorTransaction& scrollingTreeTransaction)
{
    ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeTransaction(transaction's ID=" << layerTreeTransaction.transactionID() << ")...");
#if !defined(NDEBUG) || !LOG_DISABLED
    ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << "]::commitLayerTreeTransaction transaction: " << layerTreeTransaction.description());
    ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << "]::commitLayerTreeTransaction scrolling tree: " << scrollingTreeTransaction.description());
#endif
    TraceScope tracingScope(CommitLayerTreeStart, CommitLayerTreeEnd);

    LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree transaction:" << layerTreeTransaction.description());
    LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree scrolling tree:" << scrollingTreeTransaction.description());

    m_lastLayerTreeTransactionID = layerTreeTransaction.transactionID();
    ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeTransaction(transaction's ID=" << layerTreeTransaction.transactionID() << ") - m_pendingLayerTreeTransactionID was " << m_pendingLayerTreeTransactionID << " -> set lastTrID=" << m_lastLayerTreeTransactionID);
    if (m_pendingLayerTreeTransactionID < m_lastLayerTreeTransactionID) {
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeTransaction(transaction's ID=" << layerTreeTransaction.transactionID() << ") - m_pendingLayerTreeTransactionID was " << m_pendingLayerTreeTransactionID << " too low (reordered willCommit?) -> increased to lastTrID=" << m_lastLayerTreeTransactionID);
        m_pendingLayerTreeTransactionID = m_lastLayerTreeTransactionID;
    }

    bool didUpdateEditorState { false };
    if (layerTreeTransaction.isMainFrameProcessTransaction()) {
        ASSERT(layerTreeTransaction.transactionID() == m_lastVisibleTransactionID.next());
        m_transactionIDForPendingCACommit = layerTreeTransaction.transactionID();
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeTransaction(transaction's ID=" << layerTreeTransaction.transactionID() << ") - layerTreeTransaction.isMainFrameProcessTransaction -> m_activityStateChangeID=" << m_activityStateChangeID << " (was " << layerTreeTransaction.activityStateChangeID() << ")");
        m_activityStateChangeID = layerTreeTransaction.activityStateChangeID();
        
        didUpdateEditorState = layerTreeTransaction.hasEditorState() && m_webPageProxy.updateEditorState(layerTreeTransaction.editorState(), WebPageProxy::ShouldMergeVisualEditorState::Yes);
    }

#if ENABLE(ASYNC_SCROLLING)
    std::optional<RequestedScrollData> requestedScroll;
#endif

    auto commitLayerAndScrollingTrees = [&] {
        if (layerTreeTransaction.hasAnyLayerChanges())
            ++m_countOfTransactionsWithNonEmptyLayerChanges;

        if (m_remoteLayerTreeHost->updateLayerTree(layerTreeTransaction)) {
            if (layerTreeTransaction.transactionID() >= m_transactionIDForUnhidingContent)
                m_webPageProxy.setRemoteLayerTreeRootNode(m_remoteLayerTreeHost->rootNode());
            else
                m_remoteLayerTreeHost->detachRootLayer();
        }

#if ENABLE(ASYNC_SCROLLING)
        // FIXME: Making scrolling trees work with site isolation.
        if (layerTreeTransaction.isMainFrameProcessTransaction())
            requestedScroll = m_webPageProxy.scrollingCoordinatorProxy()->commitScrollingTreeState(scrollingTreeTransaction);
#endif
    };

#if ENABLE(ASYNC_SCROLLING)
    bool needAtomicCATransaction = scrollingTreeTransaction.hasChanges();
    if (needAtomicCATransaction)
        [CATransaction begin];
#endif
    m_webPageProxy.scrollingCoordinatorProxy()->willCommitLayerAndScrollingTrees();
    commitLayerAndScrollingTrees();
    m_webPageProxy.scrollingCoordinatorProxy()->didCommitLayerAndScrollingTrees();

    m_webPageProxy.didCommitLayerTree(layerTreeTransaction);
    didCommitLayerTree(connection, layerTreeTransaction, scrollingTreeTransaction);

#if ENABLE(ASYNC_SCROLLING)
    m_webPageProxy.scrollingCoordinatorProxy()->applyScrollingTreeLayerPositionsAfterCommit();
#if PLATFORM(IOS_FAMILY)
    m_webPageProxy.adjustLayersForLayoutViewport(m_webPageProxy.unobscuredContentRect().location(), m_webPageProxy.unconstrainedLayoutViewportRect(), m_webPageProxy.displayedContentScale());
#endif

    // Handle requested scroll position updates from the scrolling tree transaction after didCommitLayerTree()
    // has updated the view size based on the content size.
    if (requestedScroll) {
        auto previousScrollPosition = m_webPageProxy.scrollingCoordinatorProxy()->currentMainFrameScrollPosition();
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeTransaction(transaction's ID=" << layerTreeTransaction.transactionID() << ") - mattwoodrow> previousScrollPosition=" << previousScrollPosition);
        if (auto previousData = std::exchange(requestedScroll->requestedDataBeforeAnimatedScroll, std::nullopt)) {
            auto& [type, positionOrDeltaBeforeAnimatedScroll, scrollType, clamping] = *previousData;
            previousScrollPosition = type == ScrollRequestType::DeltaUpdate ? (m_webPageProxy.scrollingCoordinatorProxy()->currentMainFrameScrollPosition() + std::get<FloatSize>(positionOrDeltaBeforeAnimatedScroll)) : std::get<FloatPoint>(positionOrDeltaBeforeAnimatedScroll);
            ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeTransaction(transaction's ID=" << layerTreeTransaction.transactionID() << ") - mattwoodrow> previous type=" << type << " -> previousScrollPosition=" << previousScrollPosition);
        }

        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeTransaction(transaction's ID=" << layerTreeTransaction.transactionID() << ") - mattwoodrow> -> requestScroll(destinationPosition(prev)=" << requestedScroll->destinationPosition(previousScrollPosition) << ", scrollOrigin=" << layerTreeTransaction.scrollOrigin() << ")");
        m_webPageProxy.requestScroll(requestedScroll->destinationPosition(previousScrollPosition), layerTreeTransaction.scrollOrigin(), requestedScroll->animated);

    }

    if (needAtomicCATransaction)
        [CATransaction commit];
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

    m_webPageProxy.layerTreeCommitComplete();

    if (!layerTreeTransaction.isMainFrameProcessTransaction()) {
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeTransaction(transaction's ID=" << layerTreeTransaction.transactionID() << ") -> send DrawingArea::DisplayDidRefresh...");
        connection.send(Messages::DrawingArea::DisplayDidRefresh(), m_identifier);
    } else if (std::exchange(m_commitLayerTreeMessageState, NeedsDisplayDidRefresh) == MissedCommit) {
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeTransaction(transaction's ID=" << layerTreeTransaction.transactionID() << ") -> didRefreshDisplay...");
        didRefreshDisplay();
    }

    ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::commitLayerTreeTransaction(transaction's ID=" << layerTreeTransaction.transactionID() << ") -> scheduleDisplayRefreshCallbacks");
    scheduleDisplayRefreshCallbacks();

    if (didUpdateEditorState)
        m_webPageProxy.dispatchDidUpdateEditorState();

    if (auto milestones = layerTreeTransaction.newlyReachedPaintingMilestones())
        m_webPageProxy.didReachLayoutMilestone(milestones);

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
    m_webPageProxy.send(Messages::DrawingArea::AcceleratedAnimationDidStart(layerID, key, startTime), m_identifier);
}

void RemoteLayerTreeDrawingAreaProxy::acceleratedAnimationDidEnd(WebCore::PlatformLayerIdentifier layerID, const String& key)
{
    m_webPageProxy.send(Messages::DrawingArea::AcceleratedAnimationDidEnd(layerID, key), m_identifier);
}

static const float indicatorInset = 10;

FloatPoint RemoteLayerTreeDrawingAreaProxy::indicatorLocation() const
{
    FloatPoint tiledMapLocation;
#if PLATFORM(IOS_FAMILY)
    tiledMapLocation = m_webPageProxy.unobscuredContentRect().location().expandedTo(FloatPoint());
    tiledMapLocation = tiledMapLocation.expandedTo(m_webPageProxy.exposedContentRect().location());

    float absoluteInset = indicatorInset / m_webPageProxy.displayedContentScale();
    tiledMapLocation += FloatSize(absoluteInset, absoluteInset);
#else
    if (auto viewExposedRect = m_webPageProxy.viewExposedRect())
        tiledMapLocation = viewExposedRect->location();

    tiledMapLocation += FloatSize(indicatorInset, indicatorInset);
    float scale = 1 / m_webPageProxy.pageScaleFactor();
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
    IntSize viewSize = m_webPageProxy.viewSize();

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
    scaledExposedRect = m_webPageProxy.exposedContentRect();
#else
    if (auto viewExposedRect = m_webPageProxy.viewExposedRect())
        scaledExposedRect = *viewExposedRect;
    float counterScale = 1 / m_webPageProxy.pageScaleFactor();
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

void RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay()
{
    if (!m_webPageProxy.hasRunningProcess())
        return;

    if (m_commitLayerTreeMessageState != NeedsDisplayDidRefresh) {
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::didRefreshDisplay -> pauseDisplayRefreshCallbacks");
        if (m_commitLayerTreeMessageState == CommitLayerTreePending)
            m_commitLayerTreeMessageState = MissedCommit;
        pauseDisplayRefreshCallbacks();
        return;
    }
    ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::didRefreshDisplay -> state=Pend, send DrawingArea::DisplayDidRefresh");
    
    m_commitLayerTreeMessageState = CommitLayerTreePending;

    m_webPageProxy.scrollingCoordinatorProxy()->sendScrollingTreeNodeDidScroll();

    // Waiting for CA to commit is insufficient, because the render server can still be
    // using our backing store. We can improve this by waiting for the render server to commit
    // if we find API to do so, but for now we will make extra buffers if need be.
    m_webPageProxy.send(Messages::DrawingArea::DisplayDidRefresh(), m_identifier);

#if ASSERT_ENABLED
    m_lastVisibleTransactionID = m_transactionIDForPendingCACommit;
#endif

    m_webPageProxy.didUpdateActivityState();
}

void RemoteLayerTreeDrawingAreaProxy::waitForDidUpdateActivityState(ActivityStateChangeID activityStateChangeID, WebProcessProxy& process)
{
    ASSERT(activityStateChangeID != ActivityStateChangeAsynchronous);

    // We must send the didUpdate message before blocking on the next commit, otherwise
    // we can be guaranteed that the next commit won't come until after the waitForAndDispatchImmediately times out.
    if (m_commitLayerTreeMessageState == NeedsDisplayDidRefresh) {
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::waitForDidUpdateActivityState(activityStateChangeID=" << activityStateChangeID << ") -> didRefreshDisplay");
        didRefreshDisplay();
    } else
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::waitForDidUpdateActivityState(activityStateChangeID=" << activityStateChangeID << ")");

    static Seconds activityStateUpdateTimeout = [] {
        if (id value = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitOverrideActivityStateUpdateTimeout"])
            return Seconds([value doubleValue]);
        return Seconds::fromMilliseconds(250);
    }();

    WeakPtr weakThis { *this };
    auto startTime = MonotonicTime::now();
    ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::waitForDidUpdateActivityState(activityStateChangeID=" << activityStateChangeID << ") -> waitForAndDispatchImmediately<Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree> start looping...");
    while (true) {
        auto error = process.connection()->waitForAndDispatchImmediately<Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree>(m_identifier, activityStateUpdateTimeout - (MonotonicTime::now() - startTime), IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
        if (error != IPC::Error::NoError) {
            ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::waitForDidUpdateActivityState(activityStateChangeID=" << activityStateChangeID << ") - waitForAndDispatchImmediately ended with error=" << errorAsString(error));
            return;
        }
        if (!weakThis) {
            ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::waitForDidUpdateActivityState(activityStateChangeID=" << activityStateChangeID << ") - ended because !weakThis");
            return;
        }
        if (activityStateChangeID == ActivityStateChangeAsynchronous) {
            ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::waitForDidUpdateActivityState(activityStateChangeID=" << activityStateChangeID << ") - ended because activityStateChangeID == ActivityStateChangeAsynchronous");
            return;
        }
        if (activityStateChangeID <= m_activityStateChangeID) {
            ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::waitForDidUpdateActivityState(activityStateChangeID=" << activityStateChangeID << ") - ended because activityStateChangeID <= m_activityStateChangeID=" << m_activityStateChangeID);
            return;
        }
        ALWAYS_LOG_WITH_STREAM(stream << "RemoteLayerTreeDrawingAreaProxy[pgid=" << m_webPageProxy.webPageID() << " state=" << messageStateDescription() << " lastTrID=" << m_lastLayerTreeTransactionID << "]::waitForDidUpdateActivityState(activityStateChangeID=" << activityStateChangeID << ") -> waitForAndDispatchImmediately<Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree> continue looping...");
    }
}

void RemoteLayerTreeDrawingAreaProxy::hideContentUntilPendingUpdate()
{
    m_transactionIDForUnhidingContent = nextLayerTreeTransactionID();
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
    if (m_webPageProxy.windowKind() == WindowKind::InProcessSnapshotting)
        m_remoteLayerTreeHost->mapAllIOSurfaceBackingStore();
}

void RemoteLayerTreeDrawingAreaProxy::minimumSizeForAutoLayoutDidChange()
{
    if (!m_webPageProxy.hasRunningProcess())
        return;

    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void RemoteLayerTreeDrawingAreaProxy::sizeToContentAutoSizeMaximumSizeDidChange()
{
    if (!m_webPageProxy.hasRunningProcess())
        return;

    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

} // namespace WebKit
