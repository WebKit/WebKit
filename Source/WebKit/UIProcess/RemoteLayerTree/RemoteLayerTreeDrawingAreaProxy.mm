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
#import "RemoteLayerTreeCommitBundle.h"
#import "RemoteLayerTreeDrawingAreaProxyMessages.h"
#import "RemotePageDrawingAreaProxy.h"
#import "RemotePageProxy.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "RemoteScrollingCoordinatorTransaction.h"
#import "RemoteScrollingTreeCocoa.h"
#if PLATFORM(IOS_FAMILY) && ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
#import "RemoteScrollingCoordinatorProxyIOS.h"
#endif
#import "WebFrameProxy.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "WindowKind.h"
#import <QuartzCore/CATextLayer.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/AnimationFrameRate.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurfacePool.h>
#import <WebCore/ScrollView.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/MachSendRight.h>
#import <wtf/StdLibExtras.h>
#import <wtf/SystemTracing.h>
#import <wtf/TZoneMallocInlines.h>

@interface _WKSlowFrameHUDLayer : CALayer {
    WeakPtr<WebKit::RemoteLayerTreeDrawingAreaProxy> _drawingArea;
}
- (id)initWithDrawingArea:(WebKit::RemoteLayerTreeDrawingAreaProxy*)drawingArea;
@end

@implementation _WKSlowFrameHUDLayer
- (id)initWithDrawingArea:(WebKit::RemoteLayerTreeDrawingAreaProxy*)drawingArea
{
    self = [super init];
    if (!self)
        return nil;
    _drawingArea = drawingArea;
    return self;
}

- (void)drawInContext:(CGContextRef)cgContext
{
    WebCore::GraphicsContextCG context { cgContext, WebCore::GraphicsContextCG::CGContextFromCALayer };
    if (RefPtr drawingArea = _drawingArea.get())
        drawingArea->drawSlowFrameIndicator(context);
}
@end

namespace WebKit {
using namespace IPC;
using namespace WebCore;

static constexpr size_t kSlowFrameIndicatorWidth = 180;
static constexpr size_t kSlowFrameIndicatorHeight = 40;


WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeDrawingAreaProxy);

RemoteLayerTreeDrawingAreaProxy::RemoteLayerTreeDrawingAreaProxy(WebPageProxy& pageProxy, WebProcessProxy& webProcessProxy)
    : DrawingAreaProxy(pageProxy, webProcessProxy)
    , m_remoteLayerTreeHost(makeUnique<RemoteLayerTreeHost>(*this))
    , m_webPageProxyProcessState(webProcessProxy)
{
    // We don't want to pool surfaces in the UI process.
    // FIXME: We should do this somewhere else.
    IOSurfacePool::sharedPoolSingleton().setPoolSize(0);

    if (protect(pageProxy.preferences())->tiledScrollingIndicatorVisible())
        initializeDebugIndicator();

    if (protect(pageProxy.preferences())->slowFrameIndicatorVisible())
        initializeSlowFrameIndicator();
}

RemoteLayerTreeDrawingAreaProxy::~RemoteLayerTreeDrawingAreaProxy() = default;

std::span<IPC::ReceiverName> RemoteLayerTreeDrawingAreaProxy::messageReceiverNames() const
{
    static std::array<IPC::ReceiverName, 2> names { Messages::DrawingAreaProxy::messageReceiverName(), Messages::RemoteLayerTreeDrawingAreaProxy::messageReceiverName() };
    return { names };
}

void RemoteLayerTreeDrawingAreaProxy::addRemotePageDrawingAreaProxy(RemotePageDrawingAreaProxy& proxy)
{
    m_remotePageProcessState.add(proxy.process().coreProcessIdentifier(), ProcessState(proxy.process()));
}

void RemoteLayerTreeDrawingAreaProxy::removeRemotePageDrawingAreaProxy(RemotePageDrawingAreaProxy& proxy)
{
    ASSERT(m_remotePageProcessState.contains(proxy.process().coreProcessIdentifier()));
    m_remotePageProcessState.remove(proxy.process().coreProcessIdentifier());
}

ProcessState::ProcessState(WebProcessProxy& webProcess)
    : nextLayerTreeTransactionID(TransactionID(TransactionIdentifier(), webProcess.coreProcessIdentifier()).next())
{
}

std::unique_ptr<RemoteLayerTreeHost> RemoteLayerTreeDrawingAreaProxy::detachRemoteLayerTreeHost()
{
    m_remoteLayerTreeHost->detachFromDrawingArea();
    return WTF::move(m_remoteLayerTreeHost);
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

TransactionID RemoteLayerTreeDrawingAreaProxy::nextMainFrameLayerTreeTransactionID() const
{
    for (int i = m_webPageProxyProcessState.pendingCommits.size() - 1; i >= 0; i--) {
        if (m_webPageProxyProcessState.pendingCommits[i].pendingMessage > PendingCommitMessage::NotifyPendingCommitLayerTree)
            return m_webPageProxyProcessState.pendingCommits[i].transactionID;
    }
    return lastCommittedMainFrameLayerTreeTransactionID();
}

TransactionID RemoteLayerTreeDrawingAreaProxy::lastCommittedMainFrameLayerTreeTransactionID() const
{
    return m_webPageProxyProcessState.committedLayerTreeTransactionID.value_or(TransactionID(TransactionIdentifier(), webProcessProxy().coreProcessIdentifier()));
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
    Ref aggregator = CallbackAggregator::create(WTF::move(completionHandler));
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

ProcessState& RemoteLayerTreeDrawingAreaProxy::processStateForConnection(IPC::Connection& connection)
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

const ProcessState& RemoteLayerTreeDrawingAreaProxy::processStateForIdentifier(WebCore::ProcessIdentifier identifier) const
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

void RemoteLayerTreeDrawingAreaProxy::notifyPendingCommitLayerTree(IPC::Connection& connection, std::optional<TransactionID> transactionID)
{
    ProcessState& state = processStateForConnection(connection);
    LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::notifyPendingCommitLayerTree " << transactionID << " old state: " << state.pendingCommits);
    if (transactionID) {
        if (state.pendingCommits.isEmpty()) {
            // The very first commit is initiated by WebContent, all others get
            // started in response to displayDidRefresh.
            MESSAGE_CHECK_BASE(state.nextLayerTreeTransactionID.object() == TransactionIdentifier().next(), connection);
            MESSAGE_CHECK_BASE(state.nextLayerTreeTransactionID == *transactionID, connection);
            state.pendingCommits.insert(0, { *transactionID, PendingCommitMessage::NotifyFlushingLayerTree, CommitDelayState::Pending });
            state.nextLayerTreeTransactionID = transactionID->next();
        } else {
            MESSAGE_CHECK_BASE(state.pendingCommits[0].pendingMessage == PendingCommitMessage::NotifyPendingCommitLayerTree && state.pendingCommits[0].transactionID == *transactionID, connection);
            state.pendingCommits[0].pendingMessage = PendingCommitMessage::NotifyFlushingLayerTree;
        }
    } else {
        // This frame is still pending, it'll be sent when the WebProcess decides it's ready.
        // Use the IntentionallyDeferred state so that we don't think that it's late
        // when displayDidRefresh arrives.
        MESSAGE_CHECK_BASE(state.pendingCommits.size() && state.pendingCommits[0].pendingMessage == PendingCommitMessage::NotifyPendingCommitLayerTree, connection);
        state.pendingCommits[0].delayState = CommitDelayState::IntentionallyDeferred;

        maybePauseDisplayRefreshCallbacks();

#if ENABLE(ASYNC_SCROLLING)
        if (RefPtr page = this->page())
            page->checkedScrollingCoordinatorProxy()->applyScrollingTreeLayerPositionsAfterCommit();
#endif
    }
}

void RemoteLayerTreeDrawingAreaProxy::notifyFlushingLayerTree(IPC::Connection& connection, TransactionID transactionID)
{
    ProcessState& state = processStateForConnection(connection);
    LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::notifyFlushingLayerTree " << transactionID << " old state: " << state.pendingCommits);

    MESSAGE_CHECK_BASE(state.pendingCommits[0].pendingMessage == PendingCommitMessage::NotifyFlushingLayerTree && state.pendingCommits[0].transactionID == transactionID, connection);
    state.pendingCommits[0].pendingMessage = PendingCommitMessage::CommitLayerTree;

    if (state.canSendDisplayDidRefresh(*this) && state.pendingCommits[0].delayState == CommitDelayState::Delayed) {
        LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::notifyFlushingLayerTree - sending missed didRefreshDisplay");
        didRefreshDisplay(&connection);
    }
}

void RemoteLayerTreeDrawingAreaProxy::commitLayerTree(IPC::Connection& connection, const RemoteLayerTreeCommitBundle& bundle, HashMap<ImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>&& handlesMap)
{
    {
        ProcessState& state = processStateForConnection(connection);
        LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree old state: " << state.pendingCommits);
        LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree page data: " << bundle.pageData.description());
        if (bundle.mainFrameData)
            LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree main frame data: " << bundle.mainFrameData->description());
        LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree bundle data: " << bundle.description());
        MESSAGE_CHECK_BASE(state.pendingCommits.size(), connection);
        MESSAGE_CHECK_BASE(state.pendingCommits.last().pendingMessage == PendingCommitMessage::CommitLayerTree, connection);
        MESSAGE_CHECK_BASE(state.pendingCommits.last().transactionID == bundle.transactionID, connection);
        MESSAGE_CHECK_BASE(!state.committedLayerTreeTransactionID || bundle.transactionID == state.committedLayerTreeTransactionID->next(), connection);
    }

    if (bundle.mainFrameData)
        MESSAGE_CHECK_BASE(webProcessProxy().hasConnection(connection), connection);

    // The `sendRights` vector must have __block scope to be captured by
    // the commit handler block below without the need to copy it.
    __block Vector<MachSendRight, 16> sendRights;
    for (auto& transaction : bundle.transactions) {
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

    PendingCommit completedCommit = [&]() {
        ProcessState& state = processStateForConnection(connection);
        state.committedLayerTreeTransactionID = bundle.transactionID;
        return state.pendingCommits.takeLast();
    }();

    RefPtr page = this->page();
    if (!page)
        return;

    if (bundle.mainFrameData) {
        m_activityStateChangeID = bundle.mainFrameData->activityStateChangeID;

        // FIXME(site-isolation): Editor state should be updated for subframes.
        if (bundle.mainFrameData->editorState && page->updateEditorState(EditorState { *bundle.mainFrameData->editorState }, WebPageProxy::ShouldMergeVisualEditorState::Yes))
            page->dispatchDidUpdateEditorState();

        // Process any callbacks for unhiding content early, so that we
        // set the root node during the same CA transaction.
        for (auto& callbackID : bundle.pageData.callbackIDs) {
            if (callbackID == m_replyForUnhidingContent) {
                RELEASE_LOG(RemoteLayerTree, "RemoteLayerTreeDrawingAreaProxy(%" PRIu64 ")::hideContentUntilPendingUpdate completed", identifier().toUInt64());
                m_replyForUnhidingContent = std::nullopt;
                break;
            }
        }

        page->didCommitMainFrameData(*bundle.mainFrameData, bundle.transactionID);

        if (auto milestones = bundle.mainFrameData->newlyReachedPaintingMilestones)
            page->didReachLayoutMilestone(milestones, WallTime::now());
    }

    WeakPtr weakThis { *this };

    for (auto& transaction : bundle.transactions) {
        commitLayerTreeTransaction(connection, CheckedRef { transaction.first }.get(), transaction.second, bundle.mainFrameData, bundle.pageData, bundle.transactionID);
        if (!weakThis)
            return;
    }

    for (auto& callbackID : bundle.pageData.callbackIDs) {
        removeOutstandingPresentationUpdateCallback(connection, callbackID);
        if (auto callback = connection.takeAsyncReplyHandler(callbackID))
            callback(nullptr, nullptr);
    }

    // Keep IOSurface send rights alive until the transaction is commited, otherwise we will
    // prematurely drop the only reference to them, and `inUse` will be wrong for a brief window.
    if (!sendRights.isEmpty())
        [CATransaction addCommitHandler:^{ sendRights.clear(); } forPhase:kCATransactionPhasePostCommit];

    auto duration = MonotonicTime::now() - bundle.startTime;
    if (duration.value() > (1.0 / displayNominalFramesPerSecond().value_or(FullSpeedFramesPerSecond)))
        WTFEmitSignpost(this, WebKitPerformance, "slowFrame");
    m_frameDurations.append(duration);

    if (m_frameDurations.size() > kSlowFrameIndicatorWidth)
        m_frameDurations.removeFirst();

    {
        ProcessState& state = processStateForConnection(connection);
        if (state.canSendDisplayDidRefresh(*this) && completedCommit.delayState == CommitDelayState::Delayed) {
            LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree - sending missed didRefreshDisplay");
            didRefreshDisplay(&connection);
        } else if (!state.pendingCommits.size()) {
            LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree all pending commits received, waiting for display did refresh");
        }
        if (completedCommit.delayState != CommitDelayState::Delayed && state.delayedCommits)
            state.delayedCommits--;
    }

    updateSlowFrameIndicator();
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

void RemoteLayerTreeDrawingAreaProxy::commitLayerTreeTransaction(IPC::Connection& connection, const RemoteLayerTreeTransaction& layerTreeTransaction, const RemoteScrollingCoordinatorTransaction& scrollingTreeTransaction, const std::optional<MainFrameData>& mainFrameData, const PageData& pageData, const TransactionID& transactionID)
{
    TraceScope tracingScope(CommitLayerTreeStart, CommitLayerTreeEnd);

    LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree transaction:" << layerTreeTransaction.description());
    LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::commitLayerTree scrolling tree:" << scrollingTreeTransaction.description());

    RefPtr page = this->page();
    if (!page)
        return;

#if ENABLE(ASYNC_SCROLLING)
    std::optional<RequestedScrollData> requestedScroll;
#endif

    {
        CheckedRef scrollingCoordinatorProxy = *page->scrollingCoordinatorProxy();
        auto commitLayerAndScrollingTrees = [&] {
            if (layerTreeTransaction.hasAnyLayerChanges())
                ++m_countOfTransactionsWithNonEmptyLayerChanges;
            if (m_remoteLayerTreeHost->updateLayerTree(connection, layerTreeTransaction, mainFrameData)) {
                if (!m_replyForUnhidingContent) {
                    if (m_hasDetachedRootLayer)
                        RELEASE_LOG(RemoteLayerTree, "RemoteLayerTreeDrawingAreaProxy(%" PRIu64 ") Unhiding layer tree", identifier().toUInt64());
                    page->setRemoteLayerTreeRootNode(protect(m_remoteLayerTreeHost->rootNode()).get());
                    m_hasDetachedRootLayer = false;
                } else
                    m_remoteLayerTreeHost->detachRootLayer();
            }
#if ENABLE(ASYNC_SCROLLING)
            requestedScroll = scrollingCoordinatorProxy->commitScrollingTreeState(connection, scrollingTreeTransaction, layerTreeTransaction.remoteContextHostedIdentifier());
#endif
        };

        scrollingCoordinatorProxy->willCommitLayerAndScrollingTrees();
        commitLayerAndScrollingTrees();
        scrollingCoordinatorProxy->didCommitLayerAndScrollingTrees();

        page->didCommitLayerTree(layerTreeTransaction, mainFrameData, pageData, transactionID);
        didCommitLayerTree(connection, layerTreeTransaction, scrollingTreeTransaction, mainFrameData, transactionID);

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

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
        if (layerTreeTransaction.changedLayerProperties().size() || layerTreeTransaction.destroyedLayers().size())
            scrollingCoordinatorProxy->updateOverlayRegions(layerTreeTransaction.destroyedLayers());
#endif

        if (m_debugIndicatorLayerTreeHost && mainFrameData) {
            float scale = indicatorScale(layerTreeTransaction.contentsSize());
            scrollingCoordinatorProxy->willCommitLayerAndScrollingTrees();
            bool rootLayerChanged = m_debugIndicatorLayerTreeHost->updateLayerTree(connection, layerTreeTransaction, mainFrameData, scale);
            scrollingCoordinatorProxy->didCommitLayerAndScrollingTrees();
            IntPoint scrollPosition;
#if PLATFORM(MAC)
            scrollPosition = layerTreeTransaction.scrollPosition();
#endif
            updateDebugIndicator(layerTreeTransaction.contentsSize(), rootLayerChanged, scale, scrollPosition);
            protect(m_debugIndicatorLayerTreeHost->rootLayer()).get().name = @"Indicator host root";
        }
    }

    page->layerTreeCommitComplete();
}

void RemoteLayerTreeDrawingAreaProxy::asyncSetLayerContents(WebCore::PlatformLayerIdentifier layerID, RemoteLayerBackingStoreProperties&& properties)
{
    m_remoteLayerTreeHost->asyncSetLayerContents(layerID, WTF::move(properties));
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
    tiledMapLocation = FloatPoint(page->obscuredContentInsets().left(), page->obscuredContentInsets().top());

    tiledMapLocation += FloatSize(indicatorInset, indicatorInset);
    float scale = 1 / page->pageScaleFactor();
    tiledMapLocation.scale(scale);
#endif
    return tiledMapLocation;
}

void RemoteLayerTreeDrawingAreaProxy::updateDebugIndicatorPosition()
{
    if (m_slowFrameIndicatorLayer)
        [m_slowFrameIndicatorLayer setPosition:indicatorLocation()];

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
        [m_tileMapHostLayer addSublayer:protect(m_debugIndicatorLayerTreeHost->rootLayer()).get()];
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

void RemoteLayerTreeDrawingAreaProxy::initializeSlowFrameIndicator()
{
    m_slowFrameIndicatorLayer= adoptNS([[_WKSlowFrameHUDLayer alloc] initWithDrawingArea:this]);
    [m_slowFrameIndicatorLayer setName:@"Slow frame indicator"];
    [m_slowFrameIndicatorLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
    [m_slowFrameIndicatorLayer setAnchorPoint:CGPointZero];
    [m_slowFrameIndicatorLayer setPosition:indicatorLocation()];
    [m_slowFrameIndicatorLayer setBounds:FloatRect(FloatPoint(), FloatSize(kSlowFrameIndicatorWidth, kSlowFrameIndicatorHeight))];
    RetainPtr backgroundColor = adoptCF(CGColorCreateCopyWithAlpha(RetainPtr { CGColorGetConstantColor(kCGColorBlack) }.get(), 0.1));
    [m_slowFrameIndicatorLayer setBackgroundColor:backgroundColor.get()];
}

void RemoteLayerTreeDrawingAreaProxy::updateSlowFrameIndicator()
{
    if (!m_slowFrameIndicatorLayer)
        return;

    // Make sure we're the last sublayer.
    [m_slowFrameIndicatorLayer removeFromSuperlayer];
    RetainPtr rootLayer = m_remoteLayerTreeHost->rootLayer();
    [rootLayer addSublayer:m_slowFrameIndicatorLayer.get()];

    [m_slowFrameIndicatorLayer setNeedsDisplay];
}

void RemoteLayerTreeDrawingAreaProxy::drawSlowFrameIndicator(WebCore::GraphicsContext& context)
{
    context.clearRect(FloatRect(0, 0, kSlowFrameIndicatorWidth, kSlowFrameIndicatorHeight));

    size_t index = kSlowFrameIndicatorWidth - m_frameDurations.size();
    for (auto duration : m_frameDurations) {
        float frameintervals = duration.value() / (1.0 / displayNominalFramesPerSecond().value_or(FullSpeedFramesPerSecond));
        bool slow = frameintervals > 1.0;

        size_t height = std::round(frameintervals * 10);
        height = std::min(kSlowFrameIndicatorHeight, height);

        context.setFillColor(slow ? Color(Color::red) : Color(Color::green).colorWithAlpha(0.5));
        context.fillRect(FloatRect(index, kSlowFrameIndicatorHeight - height, 1, height));
        index++;
    }
}

bool RemoteLayerTreeDrawingAreaProxy::maybePauseDisplayRefreshCallbacks()
{
    if (!m_webPageProxyProcessState.pendingCommits.size() || m_webPageProxyProcessState.pendingCommits[0].delayState != CommitDelayState::IntentionallyDeferred)
        return false;

    for (auto& pair : m_remotePageProcessState) {
        if (!pair.value.pendingCommits.size() || pair.value.pendingCommits[0].delayState != CommitDelayState::IntentionallyDeferred)
            return false;
    }

    pauseDisplayRefreshCallbacks();
    return true;
}

void RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay()
{
    didRefreshDisplay(nullptr);
}

TextStream& operator<<(TextStream& ts, const PendingCommitMessage& state)
{
    if (state == PendingCommitMessage::NotifyPendingCommitLayerTree)
        return ts << "NotifyPendingCommitLayerTree";
    if (state == PendingCommitMessage::NotifyFlushingLayerTree)
        return ts << "NotifyFlushingLayerTree";
    return ts << "CommitLayerTree";
}

TextStream& operator<<(TextStream& ts, const CommitDelayState& state)
{
    switch (state) {
    case CommitDelayState::Pending: return ts << "Pending";
    case CommitDelayState::Delayed: return ts << "Delayed";
    case CommitDelayState::IntentionallyDeferred: return ts << "IntentionallyDeferred";
    }
}

TextStream& operator<<(TextStream& ts, const PendingCommit& pendingCommit)
{
    return ts << "{ " << pendingCommit.transactionID << ", pending(" << pendingCommit.pendingMessage << "), delay(" << pendingCommit.delayState << ") } ";
}

bool RemoteLayerTreeDrawingAreaProxy::allowMultipleCommitLayerTreePending()
{
    if (RefPtr page = this->page())
        return protect(page->preferences())->allowMultipleCommitLayerTreePending();
    return false;
}

bool ProcessState::canSendDisplayDidRefresh(RemoteLayerTreeDrawingAreaProxy& drawingArea)
{
    if (pendingCommits.size() >= 2)
        return false;
    if (pendingCommits.size() == 1)
        return drawingArea.allowMultipleCommitLayerTreePending() && pendingCommits[0].pendingMessage == PendingCommitMessage::CommitLayerTree && delayedCommits >= 4;
    return true;
}

IPC::Error RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay(ProcessState& state, IPC::Connection& connection)
{
    if (!state.canSendDisplayDidRefresh(*this)) {
        ASSERT(state.pendingCommits.size());
        if (state.pendingCommits.last().delayState == CommitDelayState::Pending) {
            state.pendingCommits.last().delayState = CommitDelayState::Delayed;
            LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay still waiting on commit, marked as delayed");
            state.delayedCommits++;
        }
        return IPC::Error::NoError;
    }
    // The very first commit is initiated by WebContent, so don't send a didRefreshDisplay until we've received that.
    if (state.nextLayerTreeTransactionID.object() == TransactionIdentifier().next())
        return IPC::Error::NoError;

    state.pendingCommits.insert(0, { state.nextLayerTreeTransactionID, PendingCommitMessage::NotifyPendingCommitLayerTree, CommitDelayState::Pending });
    state.nextLayerTreeTransactionID.increment();

    LOG_WITH_STREAM(RemoteLayerTree, stream << "RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay new state " << state.pendingCommits);

    if (&state == &m_webPageProxyProcessState) {
        if (RefPtr page = this->page())
            page->checkedScrollingCoordinatorProxy()->sendScrollingTreeNodeUpdate();
    }

    // Waiting for CA to commit is insufficient, because the render server can still be
    // using our backing store. We can improve this by waiting for the render server to commit
    // if we find API to do so, but for now we will make extra buffers if need be.
    return connection.send(Messages::DrawingArea::DisplayDidRefresh(MonotonicTime::now()), identifier());
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
                didRefreshDisplay(state, protect(webProcess.connection()));
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

    static Seconds activityStateUpdateTimeout = [] {
        if (RetainPtr<id> value = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitOverrideActivityStateUpdateTimeout"])
            return Seconds([value doubleValue]);
        return 250_ms;
    }();

    WeakPtr weakThis { *this };
    auto startTime = MonotonicTime::now();

    do {
        IPC::Error error;
        if (!m_webPageProxyProcessState.pendingCommits.size())
            error = didRefreshDisplay(m_webPageProxyProcessState, connection.get());
        else {
            // Only the most recent outstanding frame can be in NotifyPendingCommitLayerTree state
            if (m_webPageProxyProcessState.pendingCommits[0].pendingMessage == PendingCommitMessage::NotifyPendingCommitLayerTree)
                error = connection->waitForAndDispatchImmediately<Messages::RemoteLayerTreeDrawingAreaProxy::NotifyPendingCommitLayerTree>(identifier(), activityStateUpdateTimeout - (MonotonicTime::now() - startTime), IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
            else if (m_webPageProxyProcessState.pendingCommits[0].pendingMessage == PendingCommitMessage::NotifyFlushingLayerTree)
                error = connection->waitForAndDispatchImmediately<Messages::RemoteLayerTreeDrawingAreaProxy::NotifyFlushingLayerTree>(identifier(), activityStateUpdateTimeout - (MonotonicTime::now() - startTime), IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
            else
                error = connection->waitForAndDispatchImmediately<Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree>(identifier(), activityStateUpdateTimeout - (MonotonicTime::now() - startTime), IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
        }

        if (error != IPC::Error::NoError)
            return;
        if (!weakThis || activityStateChangeID <= m_activityStateChangeID)
            return;
    } while (true);
}

void RemoteLayerTreeDrawingAreaProxy::hideContentUntilPendingUpdate()
{
    if (!m_remoteLayerTreeHost)
        return;
    RELEASE_LOG(RemoteLayerTree, "RemoteLayerTreeDrawingAreaProxy(%" PRIu64 ")::hideContentUntilPendingUpdate", identifier().toUInt64());
    m_replyForUnhidingContent = webProcessProxy().sendWithAsyncReply(Messages::DrawingArea::DispatchAfterEnsuringDrawing(), [] () { }, messageSenderDestinationID(), { }, WebProcessProxy::ShouldStartProcessThrottlerActivity::No);
    m_remoteLayerTreeHost->detachRootLayer();
    m_hasDetachedRootLayer = true;
}

void RemoteLayerTreeDrawingAreaProxy::hideContentUntilAnyUpdate()
{
    if (!m_remoteLayerTreeHost)
        return;
    RELEASE_LOG(RemoteLayerTree, "RemoteLayerTreeDrawingAreaProxy(%" PRIu64 ")::hideContentUntilAnyUpdate", identifier().toUInt64());
    m_remoteLayerTreeHost->detachRootLayer();
    m_hasDetachedRootLayer = true;
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

#if ENABLE(THREADED_ANIMATIONS)
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

void RemoteLayerTreeDrawingAreaProxy::updateTimelinesRegistration(WebCore::ProcessIdentifier processIdentifier, const WebCore::AcceleratedTimelinesUpdate& timelinesUpdate, MonotonicTime now)
{
    if (RefPtr page = this->page())
        page->checkedScrollingCoordinatorProxy()->updateTimelinesRegistration(processIdentifier, timelinesUpdate, now);
}

RefPtr<const RemoteAnimationTimeline> RemoteLayerTreeDrawingAreaProxy::timeline(const TimelineID& timelineID) const
{
    if (RefPtr page = this->page())
        return page->checkedScrollingCoordinatorProxy()->timeline(timelineID);
    return nullptr;
}

RefPtr<const RemoteAnimationStack> RemoteLayerTreeDrawingAreaProxy::animationStackForNodeWithIDForTesting(WebCore::PlatformLayerIdentifier layerID) const
{
    return m_remoteLayerTreeHost->animationStackForNodeWithIDForTesting(layerID);
}
#endif // ENABLE(THREADED_ANIMATIONS)

} // namespace WebKit
