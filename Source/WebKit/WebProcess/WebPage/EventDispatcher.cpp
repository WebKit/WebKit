/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
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
#include "EventDispatcher.h"

#include "EventDispatcherMessages.h"
#include "MomentumEventDispatcher.h"
#include "WebEventConversion.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include "WebTouchEvent.h"
#include "WebWheelEvent.h"
#include <WebCore/Chrome.h>
#include <WebCore/DisplayUpdate.h>
#include <WebCore/Page.h>
#include <WebCore/WheelEventTestMonitor.h>
#include <wtf/MainThread.h>
#include <wtf/SystemTracing.h>

#if ENABLE(ASYNC_SCROLLING)
#include <WebCore/AsyncScrollingCoordinator.h>
#endif

#include <WebCore/DisplayRefreshMonitorManager.h>

#if ENABLE(SCROLLING_THREAD)
#include <WebCore/ScrollingThread.h>
#include <WebCore/ScrollingTreeNode.h>
#include <WebCore/ThreadedScrollingTree.h>
#endif

namespace WebKit {
using namespace WebCore;

EventDispatcher::EventDispatcher()
    : m_queue(WorkQueue::create("com.apple.WebKit.EventDispatcher", WorkQueue::QOS::UserInteractive))
    , m_recentWheelEventDeltaFilter(WheelEventDeltaFilter::create())
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    , m_momentumEventDispatcher(WTF::makeUnique<MomentumEventDispatcher>(*this))
    , m_observerID(DisplayLinkObserverID::generate())
#endif
{
}

EventDispatcher::~EventDispatcher()
{
    ASSERT_NOT_REACHED();
}

#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
void EventDispatcher::addScrollingTreeForPage(WebPage& webPage)
{
    Locker locker { m_scrollingTreesLock };

    ASSERT(webPage.scrollingCoordinator());
    ASSERT(!m_scrollingTrees.contains(webPage.identifier()));

    auto& scrollingCoordinator = downcast<AsyncScrollingCoordinator>(*webPage.scrollingCoordinator());
    auto* scrollingTree = dynamicDowncast<ThreadedScrollingTree>(scrollingCoordinator.scrollingTree());
    ASSERT(scrollingTree);
    m_scrollingTrees.set(webPage.identifier(), scrollingTree);
}

void EventDispatcher::removeScrollingTreeForPage(WebPage& webPage)
{
    Locker locker { m_scrollingTreesLock };
    ASSERT(m_scrollingTrees.contains(webPage.identifier()));

    m_scrollingTrees.remove(webPage.identifier());
}
#endif

void EventDispatcher::initializeConnection(IPC::Connection& connection)
{
    connection.addMessageReceiver(m_queue.get(), *this, Messages::EventDispatcher::messageReceiverName());
}

void EventDispatcher::internalWheelEvent(PageIdentifier pageID, const WebWheelEvent& wheelEvent, RectEdges<bool> rubberBandableEdges, WheelEventOrigin wheelEventOrigin)
{
    auto processingSteps = OptionSet<WebCore::WheelEventProcessingSteps> { WheelEventProcessingSteps::SynchronousScrolling, WheelEventProcessingSteps::BlockingDOMEventDispatch };

    ensureOnMainRunLoop([pageID] {
        if (auto* webPage = WebProcess::singleton().webPage(pageID)) {
            if (auto* corePage = webPage->corePage()) {
                if (auto* keyboardScrollingAnimator = corePage->currentKeyboardScrollingAnimator())
                    keyboardScrollingAnimator->stopScrollingImmediately();
            }
        }
    });
    
#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
    do {
        auto platformWheelEvent = platform(wheelEvent);
#if PLATFORM(COCOA)
        m_recentWheelEventDeltaFilter->updateFromEvent(platformWheelEvent);
        if (WheelEventDeltaFilter::shouldApplyFilteringForEvent(platformWheelEvent))
            platformWheelEvent = m_recentWheelEventDeltaFilter->eventCopyWithFilteredDeltas(platformWheelEvent);
        else if (WheelEventDeltaFilter::shouldIncludeVelocityForEvent(platformWheelEvent))
            platformWheelEvent = m_recentWheelEventDeltaFilter->eventCopyWithVelocity(platformWheelEvent);
#endif

        Locker locker { m_scrollingTreesLock };
        auto scrollingTree = m_scrollingTrees.get(pageID);
        if (!scrollingTree) {
            dispatchWheelEventViaMainThread(pageID, wheelEvent, processingSteps, wheelEventOrigin);
            break;
        }
        
        // FIXME: It's pretty horrible that we're updating the back/forward state here.
        // WebCore should always know the current state and know when it changes so the
        // scrolling tree can be notified.
        // We only need to do this at the beginning of the gesture.
        if (platformWheelEvent.phase() == PlatformWheelEventPhase::Began)
            scrollingTree->setMainFrameCanRubberBand(rubberBandableEdges);

        auto processingSteps = scrollingTree->determineWheelEventProcessing(platformWheelEvent);
        bool useMainThreadForScrolling = processingSteps.contains(WheelEventProcessingSteps::SynchronousScrolling);

#if !PLATFORM(COCOA)
        // Deliver continuing scroll gestures directly to the scrolling thread until the end.
        if ((platformWheelEvent.phase() == PlatformWheelEventPhase::Changed || platformWheelEvent.phase() == PlatformWheelEventPhase::Ended)
            && scrollingTree->isUserScrollInProgressAtEventLocation(platformWheelEvent))
            useMainThreadForScrolling = false;
#endif

        scrollingTree->willProcessWheelEvent();

        ScrollingThread::dispatch([scrollingTree, wheelEvent, platformWheelEvent, processingSteps, useMainThreadForScrolling, pageID, wheelEventOrigin, this] {
            if (useMainThreadForScrolling) {
                scrollingTree->willSendEventToMainThread(platformWheelEvent);
                dispatchWheelEventViaMainThread(pageID, wheelEvent, processingSteps, wheelEventOrigin);
                scrollingTree->waitForEventToBeProcessedByMainThread(platformWheelEvent);
                return;
            }

            auto result = scrollingTree->handleWheelEvent(platformWheelEvent, processingSteps);

            if (result.needsMainThreadProcessing()) {
                dispatchWheelEventViaMainThread(pageID, wheelEvent, result.steps, wheelEventOrigin);
                if (result.steps.contains(WheelEventProcessingSteps::SynchronousScrolling))
                    return;
            }

            // If we scrolled on the scrolling thread (even if we send the event to the main thread for passive event handlers)
            // respond to the UI process that the event was handled.
            if (wheelEventOrigin == WheelEventOrigin::UIProcess)
                sendDidReceiveEvent(pageID, wheelEvent.type(), result.wasHandled);
        });
    } while (false);
#else
    UNUSED_PARAM(rubberBandableEdges);

    dispatchWheelEventViaMainThread(pageID, wheelEvent, processingSteps, wheelEventOrigin);
#endif
}

void EventDispatcher::wheelEvent(PageIdentifier pageID, const WebWheelEvent& wheelEvent, RectEdges<bool> rubberBandableEdges)
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (m_momentumEventDispatcher->handleWheelEvent(pageID, wheelEvent, rubberBandableEdges)) {
        sendDidReceiveEvent(pageID, wheelEvent.type(), true);
        return;
    }
#endif
    internalWheelEvent(pageID, wheelEvent, rubberBandableEdges, WheelEventOrigin::UIProcess);
}

#if ENABLE(MAC_GESTURE_EVENTS)
void EventDispatcher::gestureEvent(PageIdentifier pageID, const WebKit::WebGestureEvent& gestureEvent)
{
    RunLoop::main().dispatch([this, pageID, gestureEvent] {
        dispatchGestureEvent(pageID, gestureEvent);
    });
}
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
void EventDispatcher::takeQueuedTouchEventsForPage(const WebPage& webPage, TouchEventQueue& destinationQueue)
{
    Locker locker { m_touchEventsLock };
    destinationQueue = m_touchEvents.take(webPage.identifier());
}

void EventDispatcher::touchEventWithoutCallback(PageIdentifier pageID, const WebTouchEvent& touchEvent)
{
    this->touchEvent(pageID, touchEvent, nullptr);
}

void EventDispatcher::touchEvent(PageIdentifier pageID, const WebTouchEvent& touchEvent, CompletionHandler<void(bool)>&& completionHandler)
{
    bool updateListWasEmpty;
    {
        Locker locker { m_touchEventsLock };
        updateListWasEmpty = m_touchEvents.isEmpty();
        auto addResult = m_touchEvents.add(pageID, TouchEventQueue());
        if (addResult.isNewEntry)
            addResult.iterator->value.append({ touchEvent, WTFMove(completionHandler) });
        else {
            auto& queuedEvents = addResult.iterator->value;
            ASSERT(!queuedEvents.isEmpty());
            auto& lastEventAndCallback = queuedEvents.last();
            // Coalesce touch move events.
            if (touchEvent.type() == WebEventType::TouchMove && lastEventAndCallback.first.type() == WebEventType::TouchMove && !completionHandler && !lastEventAndCallback.second)
                queuedEvents.last() = { touchEvent, nullptr };
            else
                queuedEvents.append({ touchEvent, WTFMove(completionHandler) });
        }
    }

    if (updateListWasEmpty) {
        RunLoop::main().dispatch([this] {
            dispatchTouchEvents();
        });
    }
}

void EventDispatcher::dispatchTouchEvents()
{
    TraceScope traceScope(DispatchTouchEventsStart, DispatchTouchEventsEnd);

    HashMap<PageIdentifier, TouchEventQueue> localCopy;
    {
        Locker locker { m_touchEventsLock };
        localCopy.swap(m_touchEvents);
    }

    for (auto& slot : localCopy) {
        if (auto* webPage = WebProcess::singleton().webPage(slot.key))
            webPage->dispatchAsynchronousTouchEvents(WTFMove(slot.value));
    }
}
#endif

void EventDispatcher::dispatchWheelEventViaMainThread(WebCore::PageIdentifier pageID, const WebWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps, WheelEventOrigin wheelEventOrigin)
{
    ASSERT(!RunLoop::isMain());
    RunLoop::main().dispatch([this, pageID, wheelEvent, wheelEventOrigin, steps = processingSteps - WheelEventProcessingSteps::AsyncScrolling] {
        dispatchWheelEvent(pageID, wheelEvent, steps, wheelEventOrigin);
    });
}

void EventDispatcher::dispatchWheelEvent(PageIdentifier pageID, const WebWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps, WheelEventOrigin wheelEventOrigin)
{
    ASSERT(RunLoop::isMain());

    WebPage* webPage = WebProcess::singleton().webPage(pageID);
    if (!webPage)
        return;

    bool handled = webPage->wheelEvent(wheelEvent, processingSteps, wheelEventOrigin);

    if (processingSteps.contains(WheelEventProcessingSteps::SynchronousScrolling) && wheelEventOrigin == EventDispatcher::WheelEventOrigin::UIProcess)
        sendDidReceiveEvent(pageID, wheelEvent.type(), handled);
}

#if ENABLE(MAC_GESTURE_EVENTS)
void EventDispatcher::dispatchGestureEvent(PageIdentifier pageID, const WebGestureEvent& gestureEvent)
{
    ASSERT(RunLoop::isMain());

    WebPage* webPage = WebProcess::singleton().webPage(pageID);
    if (!webPage)
        return;

    webPage->gestureEvent(gestureEvent);
}
#endif

void EventDispatcher::sendDidReceiveEvent(PageIdentifier pageID, WebEventType eventType, bool didHandleEvent)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::DidReceiveEvent(eventType, didHandleEvent), pageID);
}

void EventDispatcher::notifyScrollingTreesDisplayDidRefresh(PlatformDisplayID displayID)
{
#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
    Locker locker { m_scrollingTreesLock };
    for (auto keyValuePair : m_scrollingTrees)
        keyValuePair.value->displayDidRefresh(displayID);
#endif
}

#if PLATFORM(COCOA)
void EventDispatcher::displayDidRefresh(PlatformDisplayID displayID, const DisplayUpdate& displayUpdate, bool wantsFullSpeedUpdates, bool anyObserverWantsCallback)
{
    assertIsCurrent(m_queue.get());
    tracePoint(DisplayRefreshDispatchingToMainThread, displayID, anyObserverWantsCallback);

    if (wantsFullSpeedUpdates) {
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
        m_momentumEventDispatcher->displayDidRefresh(displayID);
#endif
    }

    notifyAsyncRenderingRefreshObserversDisplayDidRefresh(displayID, displayUpdate);

    if (!anyObserverWantsCallback)
        return;

#if PLATFORM(MAC)
    notifyScrollingTreesDisplayDidRefresh(displayID);
    RunLoop::main().dispatch([displayID, displayUpdate]() {
        DisplayRefreshMonitorManager::sharedManager().displayDidRefresh(displayID, displayUpdate);
    });
#endif
}
#endif

void EventDispatcher::pageScreenDidChange(PageIdentifier pageID, PlatformDisplayID displayID, std::optional<unsigned> nominalFramesPerSecond)
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    m_momentumEventDispatcher->pageScreenDidChange(pageID, displayID, nominalFramesPerSecond);
#else
    UNUSED_PARAM(pageID);
    UNUSED_PARAM(displayID);
#endif

#if PLATFORM(COCOA)
    dispatchToMainRunLoop([this, pageID] () WTF_REQUIRES_CAPABILITY(mainRunLoop) {
        updateAsyncRenderingRefreshObservers(pageID);
    });
#endif
}

void EventDispatcher::renderingUpdateFrequencyChanged(PageIdentifier pageIdentifier)
{
    assertIsMainRunLoop();
#if PLATFORM(COCOA)
    updateAsyncRenderingRefreshObservers(pageIdentifier);
#endif
}

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
void EventDispatcher::setScrollingAccelerationCurve(PageIdentifier pageID, std::optional<ScrollingAccelerationCurve> curve)
{
    m_momentumEventDispatcher->setScrollingAccelerationCurve(pageID, curve);
}

void EventDispatcher::handleSyntheticWheelEvent(WebCore::PageIdentifier pageIdentifier, const WebWheelEvent& event, WebCore::RectEdges<bool> rubberBandableEdges)
{
    internalWheelEvent(pageIdentifier, event, rubberBandableEdges, WheelEventOrigin::MomentumEventDispatcher);
}

void EventDispatcher::startDisplayDidRefreshCallbacks(WebCore::PlatformDisplayID displayID)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StartDisplayLink(m_observerID, displayID, WebCore::FullSpeedFramesPerSecond, false), 0);
}

void EventDispatcher::stopDisplayDidRefreshCallbacks(WebCore::PlatformDisplayID displayID)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StopDisplayLink(m_observerID, displayID, false), 0);
}

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
void EventDispatcher::flushMomentumEventLoggingSoon()
{
    // FIXME: Nothing keeps this alive.
    queue().dispatchAfter(1_s, [this] {
        m_momentumEventDispatcher->flushLog();
    });
}
#endif
#endif // ENABLE(MOMENTUM_EVENT_DISPATCHER)

#if PLATFORM(COCOA)
EventDispatcher::PageObservers::PageObservers()
    : m_observerID(DisplayLinkObserverID::generate())
{ }

void EventDispatcher::updateAsyncRenderingRefreshObservers(PageIdentifier pageIdentifier)
{
    assertIsMainRunLoop();
    Locker locker { m_pageObserversLock };
    auto it = m_pageObservers.find(pageIdentifier);

    // All observers for this Page might have been removed before
    // this task ran.
    if (it == m_pageObservers.end())
        return;

    std::optional<PlatformDisplayID> displayID;
    std::optional<FramesPerSecond> framesPerSecond;
    std::optional<Seconds> updateInterval;
    if (auto* webPage = WebProcess::singleton().webPage(pageIdentifier)) {
        displayID = webPage->corePage()->chrome().displayID();
        framesPerSecond = webPage->corePage()->preferredRenderingUpdateFramesPerSecond();
        if (!framesPerSecond)
            updateInterval = webPage->corePage()->preferredRenderingUpdateInterval();
    }

    if (it->value.m_displayID == displayID && it->value.m_framesPerSecond == framesPerSecond)
        return;

    if (it->value.m_displayID) {
        if (it->value.m_framesPerSecond)
            WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StopDisplayLink(it->value.m_observerID, *it->value.m_displayID, false), 0);
        if (it->value.m_updateInterval) {
            dispatchToQueue([this, pageIdentifier] () WTF_REQUIRES_CAPABILITY(m_queue.get()) {
                stopTimerForPage(pageIdentifier);
            });
        }
    }

    it->value.m_displayID = displayID;
    it->value.m_framesPerSecond = framesPerSecond;
    it->value.m_updateInterval = updateInterval;

    if (!displayID)
        return;

    if (framesPerSecond)
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StartDisplayLink(it->value.m_observerID, *displayID, *framesPerSecond, true), 0);
    else if (updateInterval) {
        dispatchToQueue([this, pageIdentifier] () WTF_REQUIRES_CAPABILITY(m_queue.get()) {
            startTimerForPage(pageIdentifier);
        });
    }
}

void EventDispatcher::startTimerForPage(PageIdentifier pageIdentifier)
{
    assertIsCurrent(m_queue.get());
    Locker locker { m_pageObserversLock };
    auto it = m_pageObservers.find(pageIdentifier);

    if (it == m_pageObservers.end())
        return;

    if (!it->value.m_displayID || !it->value.m_updateInterval)
        return;

    it->value.m_timer = makeUnique<Timer>([this, pageIdentifier] {
        assertIsCurrent(m_queue.get());
        Locker locker { m_pageObserversLock };
        auto it = m_pageObservers.find(pageIdentifier);

        if (it == m_pageObservers.end())
            return;

        if (!it->value.m_displayID || !it->value.m_updateInterval)
            return;

        notifyPageObserversDisplayDidRefresh(it->value);
    });
    it->value.m_timer->startRepeating(*it->value.m_updateInterval);
}

void EventDispatcher::stopTimerForPage(PageIdentifier pageIdentifier)
{
    assertIsCurrent(m_queue.get());

    Locker locker { m_pageObserversLock };
    auto it = m_pageObservers.find(pageIdentifier);

    if (it == m_pageObservers.end())
        return;

    if (it->value.m_timer)
        it->value.m_timer->stop();
    it->value.m_timer = nullptr;
}

void EventDispatcher::notifyAsyncRenderingRefreshObserversDisplayDidRefresh(WebCore::PlatformDisplayID displayID, const WebCore::DisplayUpdate& displayUpdate)
{
    assertIsCurrent(m_queue.get());
    Locker locker { m_pageObserversLock };

    for (auto& pageObserver : m_pageObservers) {
        if (!pageObserver.value.m_displayID || !pageObserver.value.m_framesPerSecond || *pageObserver.value.m_displayID != displayID)
            continue;
        if (!displayUpdate.relevantForUpdateFrequency(*pageObserver.value.m_framesPerSecond))
            continue;

        notifyPageObserversDisplayDidRefresh(pageObserver.value);
    }
}

void EventDispatcher::notifyPageObserversDisplayDidRefresh(PageObservers& pageObservers)
{
    assertIsCurrent(m_queue.get());
    for (auto& ob : pageObservers.m_observers) {
        auto observer = ob.m_observer;
        ob.m_dispatcher.dispatch([observer] () {
            observer->displayDidRefresh();
        });
    }
}

void EventDispatcher::addAsyncRenderingRefreshObserver(PageIdentifier pageIdentifier, FunctionDispatcher& dispatcher, AsyncRenderingRefreshObserver& observer)
{
    Locker locker { m_pageObserversLock };

    auto addResult = m_pageObservers.add(pageIdentifier, PageObservers());

    ASSERT(!addResult.iterator->value.m_observers.containsIf([&] (auto& observerAndDispatcher) {
        return observerAndDispatcher.m_observer.ptr() == &observer;
    }));
    addResult.iterator->value.m_observers.append({ dispatcher, observer });

    if (addResult.isNewEntry) {
        dispatchToMainRunLoop([this, pageIdentifier] () WTF_REQUIRES_CAPABILITY(mainRunLoop) {
            updateAsyncRenderingRefreshObservers(pageIdentifier);
        });
    }
}

void EventDispatcher::removeAsyncRenderingRefreshObserver(PageIdentifier pageIdentifier, AsyncRenderingRefreshObserver& observer)
{
    Locker locker { m_pageObserversLock };
    auto it = m_pageObservers.find(pageIdentifier);
    RELEASE_ASSERT(it != m_pageObservers.end());

#if ASSERT_ENABLED
    unsigned removedCount =
#endif
    it->value.m_observers.removeFirstMatching([&] (auto& observerAndDispatcher) {
        return observerAndDispatcher.m_observer.ptr() == &observer;
    });
    ASSERT(removedCount == 1);

    if (it->value.m_observers.isEmpty()) {
        if (it->value.m_displayID)
            WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StopDisplayLink(it->value.m_observerID, *it->value.m_displayID, false), 0);
        m_pageObservers.remove(it);
    }
}
#endif


} // namespace WebKit
