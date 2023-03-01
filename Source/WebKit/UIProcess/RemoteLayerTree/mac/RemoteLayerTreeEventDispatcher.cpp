/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RemoteLayerTreeEventDispatcher.h"

#if PLATFORM(MAC) && ENABLE(SCROLLING_THREAD)

#include "DisplayLink.h"
#include "Logging.h"
#include "NativeWebWheelEvent.h"
#include "RemoteLayerTreeDrawingAreaProxyMac.h"
#include "RemoteScrollingCoordinatorProxyMac.h"
#include "RemoteScrollingTree.h"
#include "WebEventConversion.h"
#include "WebPageProxy.h"
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/ScrollingCoordinatorTypes.h>
#include <WebCore/ScrollingThread.h>
#include <WebCore/WheelEventDeltaFilter.h>

namespace WebKit {
using namespace WebCore;

class RemoteLayerTreeEventDispatcherDisplayLinkClient final : public DisplayLink::Client {
public:
    WTF_MAKE_FAST_ALLOCATED;
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

RemoteLayerTreeEventDispatcher::~RemoteLayerTreeEventDispatcher() = default;

// This must be called to break the cycle between RemoteLayerTreeEventDispatcherDisplayLinkClient and this.
void RemoteLayerTreeEventDispatcher::invalidate()
{
    stopDisplayLinkObserver();
    m_displayLinkClient->invalidate();
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

void RemoteLayerTreeEventDispatcher::wheelEventHysteresisUpdated(PAL::HysteresisState state)
{
    startOrStopDisplayLink();
}

void RemoteLayerTreeEventDispatcher::hasNodeWithAnimatedScrollChanged(bool hasAnimatedScrolls)
{
    startOrStopDisplayLink();
}

void RemoteLayerTreeEventDispatcher::willHandleWheelEvent(const NativeWebWheelEvent& wheelEvent)
{
    ASSERT(isMainRunLoop());
    
    m_wheelEventActivityHysteresis.impulse();
    m_wheelEventsBeingProcessed.append(wheelEvent);

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (wheelEvent.momentumPhase() == WebWheelEvent::PhaseBegan) {
        auto curve = ScrollingAccelerationCurve::fromNativeWheelEvent(wheelEvent);
        m_momentumEventDispatcher->setScrollingAccelerationCurve(m_pageIdentifier, curve);
    }
#else
    UNUSED_PARAM(wheelEvent);
#endif
}

void RemoteLayerTreeEventDispatcher::handleWheelEvent(const NativeWebWheelEvent& nativeWheelEvent, RectEdges<bool> rubberBandableEdges)
{
    ASSERT(isMainRunLoop());

    willHandleWheelEvent(nativeWheelEvent);

    ScrollingThread::dispatch([dispatcher = Ref { *this }, webWheelEvent = WebWheelEvent { nativeWheelEvent }, rubberBandableEdges] {
        auto handlingResult = dispatcher->scrollingThreadHandleWheelEvent(webWheelEvent, rubberBandableEdges);
        RunLoop::main().dispatch([dispatcher, handlingResult] {
            dispatcher->wheelEventWasHandledByScrollingThread(handlingResult);
        });
    });
}

void RemoteLayerTreeEventDispatcher::wheelEventWasHandledByScrollingThread(WheelEventHandlingResult handlingResult)
{
    ASSERT(isMainRunLoop());

    LOG_WITH_STREAM(Scrolling, stream << "RemoteLayerTreeEventDispatcher::wheelEventWasHandledByScrollingThread - result " << handlingResult);

    if (!m_scrollingCoordinator)
        return;

    auto event = m_wheelEventsBeingProcessed.takeFirst();
    m_scrollingCoordinator->continueWheelEventHandling(event, handlingResult);
}

OptionSet<WheelEventProcessingSteps> RemoteLayerTreeEventDispatcher::determineWheelEventProcessing(const WebCore::PlatformWheelEvent& wheelEvent, RectEdges<bool> rubberBandableEdges)
{
    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return { };

    // Replicate the hack in EventDispatcher::internalWheelEvent(). We could pass rubberBandableEdges all the way through the
    // WebProcess and back via the ScrollingTree, but we only ever need to consult it here.
    if (wheelEvent.phase() == PlatformWheelEventPhase::Began)
        scrollingTree->setMainFrameCanRubberBand(rubberBandableEdges);

    return scrollingTree->determineWheelEventProcessing(wheelEvent);
}

WheelEventHandlingResult RemoteLayerTreeEventDispatcher::scrollingThreadHandleWheelEvent(const WebWheelEvent& wheelEvent, RectEdges<bool> rubberBandableEdges)
{
    auto platformWheelEvent = platform(wheelEvent);
    auto processingSteps = determineWheelEventProcessing(platformWheelEvent, rubberBandableEdges);
    if (!processingSteps.contains(WheelEventProcessingSteps::ScrollingThread))
        return WheelEventHandlingResult { processingSteps, false };

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (m_momentumEventDispatcher->handleWheelEvent(m_pageIdentifier, wheelEvent, rubberBandableEdges))
        return WheelEventHandlingResult { processingSteps, true };
#endif

    LOG_WITH_STREAM(Scrolling, stream << "RemoteLayerTreeEventDispatcher::handleWheelEvent on scrolling thread " << platformWheelEvent);
    return internalHandleWheelEvent(platformWheelEvent, processingSteps);
}

WheelEventHandlingResult RemoteLayerTreeEventDispatcher::internalHandleWheelEvent(const PlatformWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps)
{
    ASSERT(ScrollingThread::isCurrentThread());
    ASSERT(processingSteps.contains(WheelEventProcessingSteps::ScrollingThread));

    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return WheelEventHandlingResult::unhandled();

    scrollingTree->willProcessWheelEvent();

    auto filteredEvent = filteredWheelEvent(wheelEvent);
    auto result = scrollingTree->handleWheelEvent(filteredEvent, processingSteps);

    scrollingTree->applyLayerPositions();

    return result;
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

DisplayLink* RemoteLayerTreeEventDispatcher::displayLink() const
{
    ASSERT(isMainRunLoop());

    if (!m_scrollingCoordinator)
        return nullptr;

    auto* drawingArea = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(m_scrollingCoordinator->webPageProxy().drawingArea());
    ASSERT(drawingArea && drawingArea->isRemoteLayerTreeDrawingAreaProxyMac());
    auto* drawingAreaMac = static_cast<RemoteLayerTreeDrawingAreaProxyMac*>(drawingArea);

    return &drawingAreaMac->displayLink();
}

void RemoteLayerTreeEventDispatcher::startOrStopDisplayLink()
{
    if (isMainRunLoop()) {
        startOrStopDisplayLinkOnMainThread();
        return;
    }

    RunLoop::main().dispatch([strongThis = Ref { *this }] {
        strongThis->startOrStopDisplayLinkOnMainThread();
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
    if (m_displayRefreshObserverID)
        return;

    auto* displayLink = this->displayLink();
    if (!displayLink)
        return;

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeEventDispatcher::startDisplayLinkObserver");

    m_displayRefreshObserverID = DisplayLinkObserverID::generate();
    // This display link always runs at the display update frequency (e.g. 120Hz).
    displayLink->addObserver(*m_displayLinkClient, *m_displayRefreshObserverID, displayLink->nominalFramesPerSecond());
}

void RemoteLayerTreeEventDispatcher::stopDisplayLinkObserver()
{
    if (!m_displayRefreshObserverID)
        return;

    auto* displayLink = this->displayLink();
    if (!displayLink)
        return;

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeEventDispatcher::stopDisplayLinkObserver");

    displayLink->removeObserver(*m_displayLinkClient, *m_displayRefreshObserverID);
    m_displayRefreshObserverID = { };
}

void RemoteLayerTreeEventDispatcher::didRefreshDisplay(PlatformDisplayID displayID)
{
    ASSERT(ScrollingThread::isCurrentThread());

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    m_momentumEventDispatcher->displayDidRefresh(displayID);
#endif

    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return;

    scrollingTree->displayDidRefresh(displayID);
}

void RemoteLayerTreeEventDispatcher::mainThreadDisplayDidRefresh(PlatformDisplayID)
{
    scrollingTree()->applyLayerPositions();
}

void RemoteLayerTreeEventDispatcher::windowScreenDidChange(PlatformDisplayID displayID, std::optional<FramesPerSecond> nominalFramesPerSecond)
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    m_momentumEventDispatcher->pageScreenDidChange(m_pageIdentifier, displayID, nominalFramesPerSecond);
#else
    UNUSED_PARAM(displayID);
    UNUSED_PARAM(nominalFramesPerSecond);
#endif
    // FIXME: Restart the displayLink if necessary.
}

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
void RemoteLayerTreeEventDispatcher::handleSyntheticWheelEvent(PageIdentifier pageID, const WebWheelEvent& event, RectEdges<bool> rubberBandableEdges)
{
    ASSERT_UNUSED(pageID, m_pageIdentifier == pageID);

    auto platformWheelEvent = platform(event);
    auto processingSteps = determineWheelEventProcessing(platformWheelEvent, rubberBandableEdges);
    if (!processingSteps.contains(WheelEventProcessingSteps::ScrollingThread))
        return;

    internalHandleWheelEvent(platformWheelEvent, processingSteps);
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
    startOrStopDisplayLink();
}

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
void RemoteLayerTreeEventDispatcher::flushMomentumEventLoggingSoon()
{
    RunLoop::current().dispatchAfter(1_s, [strongThis = Ref { *this }] {
        strongThis->m_momentumEventDispatcher->flushLog();
    });
}
#endif // ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
#endif // ENABLE(MOMENTUM_EVENT_DISPATCHER)

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(SCROLLING_THREAD)
