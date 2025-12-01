/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeEventDispatcher.h"

#if PLATFORM(MAC)

#import "DisplayLink.h"
#import "Logging.h"
#import "NativeWebWheelEvent.h"
#import "RemoteLayerTreeDrawingAreaProxyMac.h"
#import "RemoteLayerTreeNode.h"
#import "RemoteScrollingCoordinatorProxyMac.h"
#import "RemoteScrollingTree.h"
#import "WebEventConversion.h"
#import "WebPageProxy.h"
#import <QuartzCore/CALayer.h>
#import <WebCore/PlatformWheelEvent.h>
#import <WebCore/ScrollingCoordinatorTypes.h>
#import <WebCore/ScrollingNodeID.h>
#import <WebCore/ScrollingThread.h>
#import <WebCore/WheelEventDeltaFilter.h>
#import <wtf/SystemTracing.h>
#import <wtf/TZoneMallocInlines.h>

#if ENABLE(THREADED_ANIMATIONS)
#import "RemoteMonotonicTimeline.h"
#endif

namespace WebKit {
using namespace WebCore;

class RemoteLayerTreeEventDispatcherDisplayLinkClient final : public DisplayLink::Client {
public:
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RemoteLayerTreeEventDispatcherDisplayLinkClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteLayerTreeEventDispatcherDisplayLinkClient);
public:
    explicit RemoteLayerTreeEventDispatcherDisplayLinkClient(RemoteLayerTreeEventDispatcher& eventDispatcher)
        : m_eventDispatcher(&eventDispatcher)
    {
    }
    
    void invalidate()
    {
        Locker locker(m_eventDispatcherLock);
        m_eventDispatcher = nullptr;
    }

private:
    // This is called on the display link callback thread.
    void displayLinkFired(PlatformDisplayID displayID, DisplayUpdate, bool wantsFullSpeedUpdates, bool anyObserverWantsCallback) override
    {
        RefPtr<RemoteLayerTreeEventDispatcher> eventDispatcher;
        {
            Locker locker(m_eventDispatcherLock);
            eventDispatcher = m_eventDispatcher;
        }

        if (!eventDispatcher)
            return;

        if (!eventDispatcher->scrollingTreeWasRecentlyActive())
            return;

        ScrollingThread::dispatch([dispatcher = Ref { *eventDispatcher }, displayID] {
            dispatcher->didRefreshDisplay(displayID);
        });
    }
    
    Lock m_eventDispatcherLock;
    RefPtr<RemoteLayerTreeEventDispatcher> m_eventDispatcher WTF_GUARDED_BY_LOCK(m_eventDispatcherLock);
};


Ref<RemoteLayerTreeEventDispatcher> RemoteLayerTreeEventDispatcher::create(RemoteScrollingCoordinatorProxyMac& scrollingCoordinator, PageIdentifier pageIdentifier)
{
    return adoptRef(*new RemoteLayerTreeEventDispatcher(scrollingCoordinator, pageIdentifier));
}

static constexpr Seconds wheelEventHysteresisDuration { 500_ms };

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeEventDispatcher);

RemoteLayerTreeEventDispatcher::RemoteLayerTreeEventDispatcher(RemoteScrollingCoordinatorProxyMac& scrollingCoordinator, PageIdentifier pageIdentifier)
    : m_scrollingCoordinator(WeakPtr { scrollingCoordinator })
    , m_pageIdentifier(pageIdentifier)
    , m_wheelEventDeltaFilter(WheelEventDeltaFilter::create())
    , m_displayLinkClient(makeUnique<RemoteLayerTreeEventDispatcherDisplayLinkClient>(*this))
    , m_wheelEventActivityHysteresis([this](PAL::HysteresisState state) { wheelEventHysteresisUpdated(state); }, wheelEventHysteresisDuration)
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    , m_momentumEventDispatcher(WTF::makeUnique<MomentumEventDispatcher>(*this))
#endif
{
}

RemoteLayerTreeEventDispatcher::~RemoteLayerTreeEventDispatcher()
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    ASSERT(!m_momentumEventDispatcher);
#endif
    ASSERT(!m_displayRefreshObserverID);
}

// This must be called to break the cycle between RemoteLayerTreeEventDispatcherDisplayLinkClient and this.
void RemoteLayerTreeEventDispatcher::invalidate()
{
    checkedDisplayLinkClient()->invalidate();

    removeDisplayLinkClient();

    {
        Locker locker { m_scrollingTreeLock };
        m_scrollingTree = nullptr;
    }

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    m_momentumEventDispatcher = nullptr;
#endif

    m_displayLinkClient = nullptr;
}

void RemoteLayerTreeEventDispatcher::setScrollingTree(RefPtr<RemoteScrollingTree>&& scrollingTree)
{
    ASSERT(isMainRunLoop());

    Locker locker { m_scrollingTreeLock };
    m_scrollingTree = WTFMove(scrollingTree);
}

RefPtr<RemoteScrollingTree> RemoteLayerTreeEventDispatcher::scrollingTree()
{
    RefPtr<RemoteScrollingTree> result;
    {
        Locker locker { m_scrollingTreeLock };
        result = m_scrollingTree;
    }
    
    return result;
}

CheckedPtr<RemoteLayerTreeEventDispatcherDisplayLinkClient> RemoteLayerTreeEventDispatcher::checkedDisplayLinkClient()
{
    return m_displayLinkClient.get();
}

void RemoteLayerTreeEventDispatcher::wheelEventHysteresisUpdated(PAL::HysteresisState state)
{
    startOrStopDisplayLink();
}

void RemoteLayerTreeEventDispatcher::hasNodeWithAnimatedScrollChanged(bool hasAnimatedScrolls)
{
    startOrStopDisplayLink();
}

void RemoteLayerTreeEventDispatcher::cacheWheelEventScrollingAccelerationCurve(const NativeWebWheelEvent& wheelEvent)
{
    ASSERT(isMainRunLoop());

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (wheelEvent.momentumPhase() != WebWheelEvent::Phase::Began)
        return;

    auto curve = ScrollingAccelerationCurve::fromNativeWheelEvent(wheelEvent).or_else([pageIdentifier = m_pageIdentifier] {
        auto curve = ScrollingAccelerationCurve::fallbackCurve();
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [&curve, pageIdentifier] {
            UNUSED_VARIABLE(pageIdentifier);
            if (curve)
                LOG_WITH_STREAM(ScrollAnimations, stream << "RemoteLayerTreeEventDispatcher::cacheWheelEventScrollingAccelerationCurve - using fallback acceleration curve " << *curve << " for page " << pageIdentifier);
        });
        return curve;
    });
    m_momentumEventDispatcher->setScrollingAccelerationCurve(m_pageIdentifier, WTFMove(curve));
#endif
}

void RemoteLayerTreeEventDispatcher::willHandleWheelEvent(const WebWheelEvent& wheelEvent)
{
    ASSERT(isMainRunLoop());
    
    m_wheelEventActivityHysteresis.impulse();
    m_wheelEventsBeingProcessed.append(wheelEvent);
}

void RemoteLayerTreeEventDispatcher::handleWheelEvent(const WebWheelEvent& wheelEvent, RectEdges<WebCore::RubberBandingBehavior> rubberBandableEdges)
{
    ASSERT(isMainRunLoop());

    auto scrollingTree = this->scrollingTree();
    if (scrollingTree && scrollingTree->scrollingPerformanceTestingEnabled()) {
        if (wheelEvent.phase() == WebWheelEvent::Phase::Began)
            startFingerDownSignpostInterval();

        if (wheelEvent.phase() == WebWheelEvent::Phase::Ended)
            endFingerDownSignpostInterval();
    }

    willHandleWheelEvent(wheelEvent);

    ScrollingThread::dispatch([dispatcher = Ref { *this }, wheelEvent, rubberBandableEdges] {
        dispatcher->scrollingThreadHandleWheelEvent(wheelEvent, rubberBandableEdges);
    });
}

void RemoteLayerTreeEventDispatcher::scrollingThreadHandleWheelEvent(const WebWheelEvent& webWheelEvent, RectEdges<WebCore::RubberBandingBehavior> rubberBandableEdges)
{
    ASSERT(ScrollingThread::isCurrentThread());
    
    auto continueEventHandlingOnMainThread = [protectedThis = Ref { *this }](WheelEventHandlingResult handlingResult) {
        RunLoop::mainSingleton().dispatch([protectedThis, handlingResult] {
            protectedThis->continueWheelEventHandling(handlingResult);
        });
    };

    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return;

    auto locker = RemoteLayerTreeHitTestLocker { *scrollingTree };

    auto platformWheelEvent = platform(webWheelEvent);
    auto processingSteps = determineWheelEventProcessing(platformWheelEvent, rubberBandableEdges);

    bool needSynchronousScrolling = processingSteps.contains(WheelEventProcessingSteps::SynchronousScrolling);
    if (needSynchronousScrolling) {
        scrollingTree->willSendEventForDefaultHandling(platformWheelEvent);
        continueEventHandlingOnMainThread(WheelEventHandlingResult::unhandled(processingSteps));
        // Wait for the web process to respond to the event, preventing the next event from being handled until we get a response or time out,
        // allowing us to know if this gesture should be come non-blocking.
        scrollingTree->waitForEventDefaultHandlingCompletion(platformWheelEvent);
        return;
    }

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (m_momentumEventDispatcher->handleWheelEvent(m_pageIdentifier, webWheelEvent, rubberBandableEdges)) {
        continueEventHandlingOnMainThread(WheelEventHandlingResult::handled(processingSteps));
        return;
    }
#endif

    auto handlingResult = internalHandleWheelEvent(platformWheelEvent, processingSteps);
    continueEventHandlingOnMainThread(handlingResult);
}

void RemoteLayerTreeEventDispatcher::continueWheelEventHandling(WheelEventHandlingResult handlingResult)
{
    ASSERT(isMainRunLoop());
    CheckedPtr scrollingCoordinator = m_scrollingCoordinator.get();
    if (!scrollingCoordinator)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "RemoteLayerTreeEventDispatcher::continueWheelEventHandling - result " << handlingResult);

    auto event = m_wheelEventsBeingProcessed.takeFirst();
    scrollingCoordinator->continueWheelEventHandling(event, handlingResult);
}

OptionSet<WheelEventProcessingSteps> RemoteLayerTreeEventDispatcher::determineWheelEventProcessing(const PlatformWheelEvent& wheelEvent, RectEdges<WebCore::RubberBandingBehavior> rubberBandableEdges)
{
    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return { };

    // Replicate the hack in EventDispatcher::internalWheelEvent(). We could pass rubberBandableEdges all the way through the
    // WebProcess and back via the ScrollingTree, but we only ever need to consult it here.
    if (wheelEvent.phase() == PlatformWheelEventPhase::Began)
        scrollingTree->setClientAllowedMainFrameRubberBandableEdges(rubberBandableEdges);

    return scrollingTree->determineWheelEventProcessing(wheelEvent);
}

WheelEventHandlingResult RemoteLayerTreeEventDispatcher::internalHandleWheelEvent(const PlatformWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps)
{
    ASSERT(ScrollingThread::isCurrentThread());
    ASSERT(processingSteps.contains(WheelEventProcessingSteps::AsyncScrolling));

    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return WheelEventHandlingResult::unhandled();

    scrollingTree->willProcessWheelEvent();

    auto filteredEvent = filteredWheelEvent(wheelEvent);
    return scrollingTree->handleWheelEvent(filteredEvent, processingSteps);
}

void RemoteLayerTreeEventDispatcher::wheelEventHandlingCompleted(const PlatformWheelEvent& wheelEvent, std::optional<ScrollingNodeID> scrollingNodeID, std::optional<WheelScrollGestureState> gestureState, bool wasHandled)
{
    ASSERT(isMainRunLoop());

    LOG_WITH_STREAM(WheelEvents, stream << "RemoteLayerTreeEventDispatcher::wheelEventHandlingCompleted " << wheelEvent << " - sending event to scrolling thread, node " << 0 << " gestureState " << gestureState);

    if (auto scrollingTree = this->scrollingTree())
        scrollingTree->receivedEventAfterDefaultHandling(wheelEvent, gestureState);

    ScrollingThread::dispatch([protectedThis = Ref { *this }, wheelEvent, scrollingNodeID, gestureState, wasHandled] {
        auto scrollingTree = protectedThis->scrollingTree();
        if (!scrollingTree)
            return;

        auto result = scrollingTree->handleWheelEventAfterDefaultHandling(wheelEvent, scrollingNodeID, gestureState);
        RunLoop::mainSingleton().dispatch([protectedThis, wasHandled, result]() {
            if (CheckedPtr scrollingCoordinator = protectedThis->m_scrollingCoordinator.get())
                scrollingCoordinator->protectedWebPageProxy()->wheelEventHandlingCompleted(wasHandled || result.wasHandled);
        });

    });
}

PlatformWheelEvent RemoteLayerTreeEventDispatcher::filteredWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    m_wheelEventDeltaFilter->updateFromEvent(wheelEvent);

    auto filteredEvent = wheelEvent;
    if (WheelEventDeltaFilter::shouldApplyFilteringForEvent(wheelEvent))
        filteredEvent = m_wheelEventDeltaFilter->eventCopyWithFilteredDeltas(wheelEvent);
    else if (WheelEventDeltaFilter::shouldIncludeVelocityForEvent(wheelEvent))
        filteredEvent = m_wheelEventDeltaFilter->eventCopyWithVelocity(wheelEvent);

    return filteredEvent;
}

RemoteLayerTreeDrawingAreaProxyMac& RemoteLayerTreeEventDispatcher::drawingAreaMac() const
{
    return *downcast<RemoteLayerTreeDrawingAreaProxyMac>(CheckedRef { *m_scrollingCoordinator }->webPageProxy().drawingArea());
}

Ref<RemoteLayerTreeDrawingAreaProxyMac> RemoteLayerTreeEventDispatcher::protectedDrawingAreaMac() const
{
    return drawingAreaMac();
}

DisplayLink* RemoteLayerTreeEventDispatcher::displayLink() const
{
    ASSERT(isMainRunLoop());

    if (!m_scrollingCoordinator)
        return nullptr;

    return &protectedDrawingAreaMac()->displayLink();
}

DisplayLink* RemoteLayerTreeEventDispatcher::existingDisplayLink() const
{
    ASSERT(isMainRunLoop());

    if (!m_scrollingCoordinator)
        return nullptr;

    return protectedDrawingAreaMac()->existingDisplayLink();
}

void RemoteLayerTreeEventDispatcher::startOrStopDisplayLink()
{
    if (isMainRunLoop()) {
        startOrStopDisplayLinkOnMainThread();
        return;
    }

    RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }] {
        protectedThis->startOrStopDisplayLinkOnMainThread();
    });
}

void RemoteLayerTreeEventDispatcher::startOrStopDisplayLinkOnMainThread()
{
    ASSERT(isMainRunLoop());

    auto needsDisplayLink = [&]() {
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
        if (m_momentumEventDispatcherNeedsDisplayLink)
            return true;
#endif
        if (m_wheelEventActivityHysteresis.state() == PAL::HysteresisState::Started)
            return true;

#if ENABLE(THREADED_ANIMATIONS)
        {
            Locker lock { m_animationLock };
            if (m_monotonicTimelineRegistry && !m_monotonicTimelineRegistry->isEmpty())
                return true;
        }
#endif

        auto scrollingTree = this->scrollingTree();
        return scrollingTree && scrollingTree->hasNodeWithActiveScrollAnimations();
    }();

    if (needsDisplayLink)
        startDisplayLinkObserver();
    else
        stopDisplayLinkObserver();
}

void RemoteLayerTreeEventDispatcher::startDisplayLinkObserver()
{
    ASSERT(m_displayLinkClient);
    if (m_displayRefreshObserverID)
        return;

    auto* displayLink = this->displayLink();
    if (!displayLink)
        return;

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeEventDispatcher::startDisplayLinkObserver");

    m_displayRefreshObserverID = DisplayLinkObserverID::generate();
    // This display link always runs at the display update frequency (e.g. 120Hz).
    displayLink->addObserver(*checkedDisplayLinkClient(), *m_displayRefreshObserverID, displayLink->nominalFramesPerSecond());
}

void RemoteLayerTreeEventDispatcher::stopDisplayLinkObserver()
{
    if (!m_displayRefreshObserverID)
        return;

    auto* displayLink = existingDisplayLink();
    if (!displayLink)
        return;

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeEventDispatcher::stopDisplayLinkObserver");

    displayLink->removeObserver(*checkedDisplayLinkClient(), *m_displayRefreshObserverID);
    m_displayRefreshObserverID = { };
}

void RemoteLayerTreeEventDispatcher::removeDisplayLinkClient()
{
    auto* displayLink = existingDisplayLink();
    if (!displayLink)
        return;

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeEventDispatcher::removeDisplayLinkClient");
    displayLink->removeClient(*checkedDisplayLinkClient());
    m_displayRefreshObserverID = { };
}

void RemoteLayerTreeEventDispatcher::didRefreshDisplay(PlatformDisplayID displayID)
{
    auto traceScope = TraceScope { ScrollingThreadDisplayDidRefreshStart, ScrollingThreadDisplayDidRefreshEnd, displayID };

    ASSERT(ScrollingThread::isCurrentThread());

    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return;

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    {
        // Make sure the lock is held for the handleSyntheticWheelEvent callback.
        auto locker = RemoteLayerTreeHitTestLocker { *scrollingTree };
        m_momentumEventDispatcher->displayDidRefresh(displayID);
    }
#endif

    Locker locker { m_scrollingTreeLock };

    auto now = MonotonicTime::now();
    m_lastDisplayDidRefreshTime = now;

    scrollingTree->displayDidRefresh(displayID);

    if (m_state != SynchronizationState::Idle) {
        scrollingTree->tryToApplyLayerPositions();
#if ENABLE(THREADED_ANIMATIONS)
        updateAnimations();
#endif
    }

    switch (m_state) {
    case SynchronizationState::Idle: {
        m_state = SynchronizationState::WaitingForRenderingUpdate;
        constexpr auto maxStartRenderingUpdateDelay = 1_ms;
        scheduleDelayedRenderingUpdateDetectionTimer(maxStartRenderingUpdateDelay);
        break;
    }
    case SynchronizationState::WaitingForRenderingUpdate:
    case SynchronizationState::InRenderingUpdate:
    case SynchronizationState::Desynchronized:
        break;
    }
}

void RemoteLayerTreeEventDispatcher::scheduleDelayedRenderingUpdateDetectionTimer(Seconds delay)
{
    ASSERT(ScrollingThread::isCurrentThread());

    if (!m_delayedRenderingUpdateDetectionTimer)
        m_delayedRenderingUpdateDetectionTimer = makeUnique<RunLoop::Timer>(RunLoop::currentSingleton(), "RemoteLayerTreeEventDispatcher::DelayedRenderingUpdateDetectionTimer"_s, [weakThis = ThreadSafeWeakPtr { *this }] {
            auto strongThis = weakThis.get();
            if (strongThis)
                strongThis->delayedRenderingUpdateDetectionTimerFired();
        });

    m_delayedRenderingUpdateDetectionTimer->startOneShot(delay);
}

void RemoteLayerTreeEventDispatcher::delayedRenderingUpdateDetectionTimerFired()
{
    ASSERT(ScrollingThread::isCurrentThread());

    if (auto scrollingTree = this->scrollingTree())
        scrollingTree->tryToApplyLayerPositions();
#if ENABLE(THREADED_ANIMATIONS)
    updateAnimations();
#endif
}

void RemoteLayerTreeEventDispatcher::waitForRenderingUpdateCompletionOrTimeout()
{
    ASSERT(ScrollingThread::isCurrentThread());
    ASSERT(m_scrollingTreeLock.isLocked());

    if (m_delayedRenderingUpdateDetectionTimer)
        m_delayedRenderingUpdateDetectionTimer->stop();

    auto currentTime = MonotonicTime::now();
    RefPtr scrollingTree = m_scrollingTree;
    auto estimatedNextDisplayRefreshTime = std::max(m_lastDisplayDidRefreshTime + scrollingTree->frameDuration(), currentTime);
    auto timeoutTime = std::min(currentTime + scrollingTree->maxAllowableRenderingUpdateDurationForSynchronization(), estimatedNextDisplayRefreshTime);

    constexpr auto maximumTimeoutDelay = 32_ms;
    auto maximumTimeoutTime = currentTime + maximumTimeoutDelay;
    if (timeoutTime > maximumTimeoutTime) {
        RELEASE_LOG_ERROR(DisplayLink, "%p - [webPageID=%" PRIu64 "] RemoteLayerTreeEventDispatcher::waitForRenderingUpdateCompletionOrTimeout - bad timeout %.2fms into the future (frame duration %.2fms)", this, m_pageIdentifier.toUInt64(),
            (timeoutTime - currentTime).milliseconds(), scrollingTree->frameDuration().milliseconds());
        timeoutTime = maximumTimeoutTime;
    }

    bool becameIdle = m_stateCondition.waitUntil(m_scrollingTreeLock, timeoutTime, [&] {
        assertIsHeld(m_scrollingTreeLock);
        return m_state == SynchronizationState::Idle;
    });

    ASSERT(m_scrollingTreeLock.isLocked());

    if (!becameIdle) {
        m_state = SynchronizationState::Desynchronized;
        // At this point we know the main thread is taking too long in the rendering update,
        // so we give up trying to sync with the main thread and update layers here on the scrolling thread.
        // Dispatch to allow for the scrolling thread to handle any outstanding wheel events before we commit layers.
        ScrollingThread::dispatch([protectedThis = Ref { *this }]() {
            if (auto scrollingTree = protectedThis->scrollingTree())
                scrollingTree->tryToApplyLayerPositions();
#if ENABLE(THREADED_ANIMATIONS)
            protectedThis->updateAnimations();
#endif
        });
        tracePoint(ScrollingThreadRenderUpdateSyncEnd, 1);
    } else
        tracePoint(ScrollingThreadRenderUpdateSyncEnd);
}

bool RemoteLayerTreeEventDispatcher::scrollingTreeWasRecentlyActive()
{
    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return false;

    if (scrollingTree->hasRecentActivity())
        return true;

#if ENABLE(THREADED_ANIMATIONS)
    Locker lock { m_animationLock };
    return !m_animationStacks.isEmpty();
#else
    return false;
#endif
}

void RemoteLayerTreeEventDispatcher::mainThreadDisplayDidRefresh(PlatformDisplayID)
{
    if (!scrollingTreeWasRecentlyActive())
        return;

    tracePoint(ScrollingThreadRenderUpdateSyncStart);

    // Wait for the scrolling thread to acquire m_scrollingTreeLock. This ensures that any pending wheel events are processed.
    BinarySemaphore semaphore;
    ScrollingThread::dispatch([protectedThis = Ref { *this }, &semaphore]() {
        Locker treeLocker { protectedThis->m_scrollingTreeLock };
        semaphore.signal();
        protectedThis->waitForRenderingUpdateCompletionOrTimeout();
    });
    semaphore.wait();

    Locker locker { m_scrollingTreeLock };
    m_state = SynchronizationState::InRenderingUpdate;
}

void RemoteLayerTreeEventDispatcher::renderingUpdateComplete()
{
    ASSERT(isMainRunLoop());

    Locker locker { m_scrollingTreeLock };

    if (m_state == SynchronizationState::InRenderingUpdate)
        m_stateCondition.notifyOne();

    m_state = SynchronizationState::Idle;
}

#if ENABLE(THREADED_ANIMATIONS)
void RemoteLayerTreeEventDispatcher::lockForAnimationChanges()
{
    ASSERT(isMainRunLoop());
    m_animationLock.lock();
}

void RemoteLayerTreeEventDispatcher::unlockForAnimationChanges()
{
    ASSERT(isMainRunLoop());
    m_animationLock.unlock();
    startOrStopDisplayLink();
}

void RemoteLayerTreeEventDispatcher::animationsWereAddedToNode(RemoteLayerTreeNode& node)
{
    ASSERT(isMainRunLoop());
    assertIsHeld(m_animationLock);
    auto animationStack = node.takeAnimationStack();
    ASSERT(animationStack);
    m_animationStacks.set(node.layerID(), animationStack.releaseNonNull());
}

void RemoteLayerTreeEventDispatcher::animationsWereRemovedFromNode(RemoteLayerTreeNode& node)
{
    ASSERT(isMainRunLoop());
    assertIsHeld(m_animationLock);
    if (auto animationStack = m_animationStacks.take(node.layerID()))
        animationStack->clear(node.protectedLayer().get());
}

void RemoteLayerTreeEventDispatcher::updateTimelineRegistration(WebCore::ProcessIdentifier processIdentifier, const HashSet<Ref<WebCore::AcceleratedTimeline>>& timelineRepresentations, MonotonicTime now)
{
    assertIsHeld(m_animationLock);
    if (auto scrollingTree = this->scrollingTree())
        scrollingTree->updateTimelineRegistration(processIdentifier, timelineRepresentations);
    if (!m_monotonicTimelineRegistry)
        m_monotonicTimelineRegistry = makeUnique<RemoteMonotonicTimelineRegistry>();
    m_monotonicTimelineRegistry->update(processIdentifier, timelineRepresentations, now);
    if (m_monotonicTimelineRegistry->isEmpty())
        m_monotonicTimelineRegistry = nullptr;
}

RefPtr<const RemoteAnimationTimeline> RemoteLayerTreeEventDispatcher::timeline(const TimelineID& timelineID)
{
    assertIsHeld(m_animationLock);
    if (m_monotonicTimelineRegistry) {
        if (RefPtr timeline = m_monotonicTimelineRegistry->get(timelineID))
            return timeline;
    }
    if (auto scrollingTree = this->scrollingTree())
        return scrollingTree->timeline(timelineID);
    return nullptr;
}

void RemoteLayerTreeEventDispatcher::updateAnimations()
{
    ASSERT(!isMainRunLoop());
    Locker lock { m_animationLock };

    // FIXME: Rather than using 'now' at the point this is called, we
    // should probably be using the timestamp of the (next?) display
    // link update or vblank refresh.
    if (m_monotonicTimelineRegistry)
        m_monotonicTimelineRegistry->advanceCurrentTime(MonotonicTime::now());

    auto animationStacks = std::exchange(m_animationStacks, { });
    for (auto [layerID, currentAnimationStack] : animationStacks) {
        Ref animationStack = currentAnimationStack;
        animationStack->applyEffectsFromScrollingThread();

        // We can clear the effect stack if it's empty, but the previous
        // call to applyEffects() is important so that the base values
        // were re-applied.
        if (!animationStack->isEmpty())
            m_animationStacks.set(layerID, WTFMove(animationStack));
    }
}

RefPtr<const RemoteAnimationStack> RemoteLayerTreeEventDispatcher::animationStackForNodeWithIDForTesting(WebCore::PlatformLayerIdentifier layerID) const
{
    assertIsHeld(m_animationLock);

    auto it = m_animationStacks.find(layerID);
    if (it != m_animationStacks.end())
        return it->value.ptr();
    return nullptr;
}
#endif

void RemoteLayerTreeEventDispatcher::windowScreenWillChange()
{
    removeDisplayLinkClient();
}

void RemoteLayerTreeEventDispatcher::windowScreenDidChange(PlatformDisplayID displayID, std::optional<FramesPerSecond> nominalFramesPerSecond)
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    m_momentumEventDispatcher->pageScreenDidChange(m_pageIdentifier, displayID, nominalFramesPerSecond);
#else
    UNUSED_PARAM(displayID);
    UNUSED_PARAM(nominalFramesPerSecond);
#endif
    if (auto scrollingTree = this->scrollingTree())
        scrollingTree->windowScreenDidChange(displayID, nominalFramesPerSecond);
    
    // FIXME: Restart the displayLink if necessary.
}

void RemoteLayerTreeEventDispatcher::startFingerDownSignpostInterval()
{
    if (!m_fingerDownIntervalIsActive) {
        WTFBeginSignpostAlways(nullptr, ScrollingPerformanceTestFingerDownInterval, "isAnimation=YES;");
        m_fingerDownIntervalIsActive = true;
    }
}

void RemoteLayerTreeEventDispatcher::endFingerDownSignpostInterval()
{
    if (m_fingerDownIntervalIsActive) {
        WTFEndSignpostAlways(nullptr, ScrollingPerformanceTestFingerDownInterval, "isAnimation=YES;");
        m_fingerDownIntervalIsActive = false;
    }
}

void RemoteLayerTreeEventDispatcher::startMomentumSignpostInterval()
{
    if (!m_momentumIntervalIsActive) {
        WTFBeginSignpostAlways(nullptr, ScrollingPerformanceTestMomentumInterval, "isAnimation=YES;");
        m_momentumIntervalIsActive = true;
    }
}

void RemoteLayerTreeEventDispatcher::endMomentumSignpostInterval()
{
    if (m_momentumIntervalIsActive) {
        WTFEndSignpostAlways(nullptr, ScrollingPerformanceTestMomentumInterval, "isAnimation=YES;");
        m_momentumIntervalIsActive = false;
    }
}

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
void RemoteLayerTreeEventDispatcher::handleSyntheticWheelEvent(PageIdentifier pageID, const WebWheelEvent& event, RectEdges<WebCore::RubberBandingBehavior> rubberBandableEdges)
{
    ASSERT_UNUSED(pageID, m_pageIdentifier == pageID);

    auto scrollingTree = this->scrollingTree();
    if (scrollingTree && scrollingTree->scrollingPerformanceTestingEnabled()) {
        if (event.momentumPhase() == WebWheelEvent::Phase::Began)
            startMomentumSignpostInterval();

        if (m_momentumIntervalIsActive && (!std::abs(event.delta().height()) || event.momentumPhase() == WebWheelEvent::Phase::Ended))
            endMomentumSignpostInterval();
    }

    auto platformWheelEvent = platform(event);
    auto processingSteps = determineWheelEventProcessing(platformWheelEvent, rubberBandableEdges);
    if (!processingSteps.contains(WheelEventProcessingSteps::AsyncScrolling))
        return;

    internalHandleWheelEvent(platformWheelEvent, processingSteps);
}

void RemoteLayerTreeEventDispatcher::didStartRubberbanding()
{
    endFingerDownSignpostInterval();
    endMomentumSignpostInterval();
}

void RemoteLayerTreeEventDispatcher::startDisplayDidRefreshCallbacks(PlatformDisplayID)
{
    ASSERT(!m_momentumEventDispatcherNeedsDisplayLink);
    m_momentumEventDispatcherNeedsDisplayLink = true;
    startOrStopDisplayLink();
}

void RemoteLayerTreeEventDispatcher::stopDisplayDidRefreshCallbacks(PlatformDisplayID)
{
    ASSERT(m_momentumEventDispatcherNeedsDisplayLink);
    m_momentumEventDispatcherNeedsDisplayLink = false;
    if (m_momentumEventDispatcher)
        startOrStopDisplayLink();
}

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
void RemoteLayerTreeEventDispatcher::flushMomentumEventLoggingSoon()
{
    RunLoop::currentSingleton().dispatchAfter(1_s, [protectedThis = Ref { *this }] {
        protectedThis->m_momentumEventDispatcher->flushLog();
    });
}
#endif // ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
#endif // ENABLE(MOMENTUM_EVENT_DISPATCHER)

} // namespace WebKit

#endif // PLATFORM(MAC)
