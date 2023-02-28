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


Ref<RemoteLayerTreeEventDispatcher> RemoteLayerTreeEventDispatcher::create(RemoteScrollingCoordinatorProxyMac& scrollingCoordinator, WebCore::PageIdentifier pageIdentifier)
{
    return adoptRef(*new RemoteLayerTreeEventDispatcher(scrollingCoordinator, pageIdentifier));
}

static constexpr Seconds wheelEventHysteresisDuration { 500_ms };

RemoteLayerTreeEventDispatcher::RemoteLayerTreeEventDispatcher(RemoteScrollingCoordinatorProxyMac& scrollingCoordinator, WebCore::PageIdentifier pageIdentifier)
    : m_scrollingCoordinator(WeakPtr { scrollingCoordinator })
    , m_pageIdentifier(pageIdentifier)
    , m_wheelEventDeltaFilter(WheelEventDeltaFilter::create())
    , m_displayLinkClient(makeUnique<RemoteLayerTreeEventDispatcherDisplayLinkClient>(*this))
    , m_wheelEventActivityHysteresis([this](PAL::HysteresisState state) { wheelEventHysteresisUpdated(state); }, wheelEventHysteresisDuration)
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
    ASSERT(isMainRunLoop());
    startOrStopDisplayLink();
}

void RemoteLayerTreeEventDispatcher::hasNodeWithAnimatedScrollChanged(bool hasAnimatedScrolls)
{
    ASSERT(isMainRunLoop());
    startOrStopDisplayLink();
}

void RemoteLayerTreeEventDispatcher::willHandleWheelEvent(const NativeWebWheelEvent& wheelEvent)
{
    ASSERT(isMainRunLoop());
    
    m_wheelEventActivityHysteresis.impulse();
    m_wheelEventsBeingProcessed.append(wheelEvent);
}

void RemoteLayerTreeEventDispatcher::handleWheelEvent(const NativeWebWheelEvent& nativeWheelEvent, RectEdges<bool> rubberBandableEdges)
{
    ASSERT(isMainRunLoop());

    willHandleWheelEvent(nativeWheelEvent);

    ScrollingThread::dispatch([dispatcher = Ref { *this }, platformWheelEvent = platform(nativeWheelEvent), rubberBandableEdges] {
        LOG_WITH_STREAM(Scrolling, stream << "RemoteLayerTreeEventDispatcher::handleWheelEvent on scrolling thread " << platformWheelEvent);
        auto handlingResult = dispatcher->internalHandleWheelEvent(platformWheelEvent, rubberBandableEdges);

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

WheelEventHandlingResult RemoteLayerTreeEventDispatcher::internalHandleWheelEvent(const PlatformWheelEvent& wheelEvent, RectEdges<bool> rubberBandableEdges)
{
    ASSERT(ScrollingThread::isCurrentThread());

    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return WheelEventHandlingResult::unhandled();

    // Replicate the hack in EventDispatcher::internalWheelEvent(). We could pass rubberBandableEdges all the way through the
    // WebProcess and back via the ScrollingTree, but we only ever need to consult it here.
    if (wheelEvent.phase() == PlatformWheelEventPhase::Began)
        scrollingTree->setMainFrameCanRubberBand(rubberBandableEdges);

    auto processingSteps = scrollingTree->determineWheelEventProcessing(wheelEvent);
    LOG_WITH_STREAM(Scrolling, stream << "RemoteLayerTreeEventDispatcher::internalHandleWheelEvent " << wheelEvent << " - steps " << processingSteps);
    if (!processingSteps.contains(WheelEventProcessingSteps::ScrollingThread))
        return WheelEventHandlingResult::unhandled(processingSteps);

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
    auto needsDisplayLink = [&]() {
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

    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return;

    scrollingTree->displayDidRefresh(displayID);
}

void RemoteLayerTreeEventDispatcher::mainThreadDisplayDidRefresh(WebCore::PlatformDisplayID)
{
    scrollingTree()->applyLayerPositions();
}

void RemoteLayerTreeEventDispatcher::windowScreenDidChange(WebCore::PlatformDisplayID, std::optional<WebCore::FramesPerSecond>)
{
    // FIXME: Restart the displayLink if necessary.
}

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(SCROLLING_THREAD)
