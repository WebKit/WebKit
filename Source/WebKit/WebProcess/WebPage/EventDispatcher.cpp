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
#include <WebCore/DisplayUpdate.h>
#include <WebCore/HandleUserInputEventResult.h>
#include <WebCore/Page.h>
#include <WebCore/WheelEventTestMonitor.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/SystemTracing.h>

#if ENABLE(ASYNC_SCROLLING)
#include <WebCore/AsyncScrollingCoordinator.h>
#endif

#include <WebCore/DisplayRefreshMonitorManager.h>

#if ENABLE(IOS_TOUCH_EVENTS)
#include <WebCore/RemoteUserInputEventData.h>
#endif

#if ENABLE(SCROLLING_THREAD)
#include <WebCore/ScrollingThread.h>
#include <WebCore/ScrollingTreeNode.h>
#include <WebCore/ThreadedScrollingTree.h>
#endif

namespace WebKit {
using namespace WebCore;

EventDispatcher::EventDispatcher(WebProcess& process)
    : m_process(process)
    , m_queue(WorkQueue::create("com.apple.WebKit.EventDispatcher"_s, WorkQueue::QOS::UserInteractive))
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

void EventDispatcher::ref() const
{
    m_process->ref();
}

void EventDispatcher::deref() const
{
    m_process->deref();
}

#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
void EventDispatcher::addScrollingTreeForPage(WebPage& webPage)
{
    Locker locker { m_scrollingTreesLock };

    ASSERT(webPage.scrollingCoordinator());
    ASSERT(!m_scrollingTrees.contains(webPage.identifier()));

    Ref scrollingCoordinator = downcast<AsyncScrollingCoordinator>(*webPage.scrollingCoordinator());
    RefPtr scrollingTree = dynamicDowncast<ThreadedScrollingTree>(scrollingCoordinator->scrollingTree());
    ASSERT(scrollingTree);
    m_scrollingTrees.set(webPage.identifier(), WTFMove(scrollingTree));
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
        if (RefPtr webPage = WebProcess::singleton().webPage(pageID)) {
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
            scrollingTree->setClientAllowedMainFrameRubberBandableEdges(rubberBandableEdges);

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
void EventDispatcher::gestureEvent(FrameIdentifier frameID, PageIdentifier pageID, const WebGestureEvent& gestureEvent)
{
    RunLoop::main().dispatch([this, frameID, pageID, gestureEvent] mutable {
        dispatchGestureEvent(frameID, pageID, gestureEvent);
    });
}
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
void EventDispatcher::takeQueuedTouchEventsForPage(const WebPage& webPage, UniqueRef<TouchEventQueue>& destinationQueue)
{
    Locker locker { m_touchEventsLock };

    if (auto queue = m_touchEvents.take(webPage.identifier()); queue)
        destinationQueue = makeUniqueRefFromNonNullUniquePtr(WTFMove(queue));
}

void EventDispatcher::touchEvent(PageIdentifier pageID, FrameIdentifier frameID, const WebTouchEvent& touchEvent, CompletionHandler<void(bool, std::optional<RemoteUserInputEventData>)>&& completionHandler)
{
    bool updateListWasEmpty;
    {
        Locker locker { m_touchEventsLock };
        updateListWasEmpty = m_touchEvents.isEmpty();
        auto addResult = m_touchEvents.add(pageID, makeUniqueRef<TouchEventQueue>());
        if (addResult.isNewEntry)
            addResult.iterator->value->append({ frameID, touchEvent, WTFMove(completionHandler) });
        else {
            auto& queuedEvents = addResult.iterator->value;
            ASSERT(!queuedEvents->isEmpty());
            auto& touchEventData = queuedEvents->last();
            // Coalesce touch move events.
            if (touchEvent.type() == WebEventType::TouchMove && touchEventData.event.type() == WebEventType::TouchMove) {
                auto coalescedEvents = Vector<WebTouchEvent> { };
                coalescedEvents.appendVector(queuedEvents->last().event.coalescedEvents());
                coalescedEvents.appendVector(touchEvent.coalescedEvents());

                auto touchEventWithCoalescedEvents = touchEvent;
                touchEventWithCoalescedEvents.setCoalescedEvents(coalescedEvents);

                queuedEvents->last() = { frameID, touchEventWithCoalescedEvents, WTFMove(completionHandler) };
            } else
                queuedEvents->append({ frameID, touchEvent, WTFMove(completionHandler) });
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

    HashMap<PageIdentifier, UniqueRef<TouchEventQueue>> localCopy;
    {
        Locker locker { m_touchEventsLock };
        localCopy.swap(m_touchEvents);
    }

    for (auto& slot : localCopy) {
        if (RefPtr webPage = WebProcess::singleton().webPage(slot.key))
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

    RefPtr webPage = WebProcess::singleton().webPage(pageID);
    if (!webPage)
        return;

    bool handled = false;
    if (webPage->mainFrame()) {
        auto [result, _] = webPage->wheelEvent(webPage->mainFrame()->frameID(), wheelEvent, processingSteps);
        handled = result.wasHandled();
    }

    if (processingSteps.contains(WheelEventProcessingSteps::SynchronousScrolling) && wheelEventOrigin == EventDispatcher::WheelEventOrigin::UIProcess)
        sendDidReceiveEvent(pageID, wheelEvent.type(), handled);
}

#if ENABLE(MAC_GESTURE_EVENTS)
void EventDispatcher::dispatchGestureEvent(FrameIdentifier frameID, PageIdentifier pageID, const WebGestureEvent& gestureEvent)
{
    ASSERT(RunLoop::isMain());

    RefPtr webPage = WebProcess::singleton().webPage(pageID);
    if (!webPage)
        return;

    webPage->gestureEvent(frameID, gestureEvent);
}
#endif

void EventDispatcher::sendDidReceiveEvent(PageIdentifier pageID, WebEventType eventType, bool didHandleEvent)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::DidReceiveEvent(eventType, didHandleEvent, std::nullopt), pageID);
}

void EventDispatcher::notifyScrollingTreesDisplayDidRefresh(PlatformDisplayID displayID)
{
#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
    Locker locker { m_scrollingTreesLock };
    for (auto keyValuePair : m_scrollingTrees)
        keyValuePair.value->displayDidRefresh(displayID);
#endif
}

#if HAVE(DISPLAY_LINK)
void EventDispatcher::displayDidRefresh(PlatformDisplayID displayID, const DisplayUpdate& displayUpdate, bool sendToMainThread)
{
    tracePoint(DisplayRefreshDispatchingToMainThread, displayID, sendToMainThread);

    ASSERT(!RunLoop::isMain());

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    m_momentumEventDispatcher->displayDidRefresh(displayID);
#endif

    notifyScrollingTreesDisplayDidRefresh(displayID);

    if (!sendToMainThread)
        return;

    RunLoop::main().dispatch([displayID, displayUpdate]() {
        DisplayRefreshMonitorManager::sharedManager().displayDidRefresh(displayID, displayUpdate);
    });
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
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StartDisplayLink(m_observerID, displayID, WebCore::FullSpeedFramesPerSecond), 0);
}

void EventDispatcher::stopDisplayDidRefreshCallbacks(WebCore::PlatformDisplayID displayID)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StopDisplayLink(m_observerID, displayID), 0);
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

} // namespace WebKit
