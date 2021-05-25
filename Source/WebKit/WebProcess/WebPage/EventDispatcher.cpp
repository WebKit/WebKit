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
#include "WebEventConversion.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebTouchEvent.h"
#include "WebWheelEvent.h"
#include <WebCore/DisplayUpdate.h>
#include <WebCore/Page.h>
#include <WebCore/WheelEventTestMonitor.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/SystemTracing.h>

#if ENABLE(ASYNC_SCROLLING)
#include <WebCore/AsyncScrollingCoordinator.h>
#endif

#include <WebCore/DisplayRefreshMonitorManager.h>

#if ENABLE(SCROLLING_THREAD)
#include <WebCore/ScrollingThread.h>
#include <WebCore/ThreadedScrollingTree.h>
#endif

namespace WebKit {
using namespace WebCore;

Ref<EventDispatcher> EventDispatcher::create()
{
    return adoptRef(*new EventDispatcher);
}

EventDispatcher::EventDispatcher()
    : m_queue(WorkQueue::create("com.apple.WebKit.EventDispatcher", WorkQueue::Type::Serial, WorkQueue::QOS::UserInteractive))
    , m_recentWheelEventDeltaFilter(WheelEventDeltaFilter::create())
{
}

EventDispatcher::~EventDispatcher()
{
    ASSERT_NOT_REACHED();
}

#if ENABLE(SCROLLING_THREAD)
void EventDispatcher::addScrollingTreeForPage(WebPage* webPage)
{
    Locker locker { m_scrollingTreesLock };

    ASSERT(webPage->corePage()->scrollingCoordinator());
    ASSERT(!m_scrollingTrees.contains(webPage->identifier()));

    AsyncScrollingCoordinator& scrollingCoordinator = downcast<AsyncScrollingCoordinator>(*webPage->corePage()->scrollingCoordinator());
    m_scrollingTrees.set(webPage->identifier(), downcast<ThreadedScrollingTree>(scrollingCoordinator.scrollingTree()));
}

void EventDispatcher::removeScrollingTreeForPage(WebPage* webPage)
{
    Locker locker { m_scrollingTreesLock };
    ASSERT(m_scrollingTrees.contains(webPage->identifier()));

    m_scrollingTrees.remove(webPage->identifier());
}
#endif

void EventDispatcher::initializeConnection(IPC::Connection* connection)
{
    connection->addWorkQueueMessageReceiver(Messages::EventDispatcher::messageReceiverName(), m_queue.get(), this);
}

void EventDispatcher::wheelEvent(PageIdentifier pageID, const WebWheelEvent& wheelEvent, bool canRubberBandAtLeft, bool canRubberBandAtRight, bool canRubberBandAtTop, bool canRubberBandAtBottom)
{
#if PLATFORM(COCOA) || ENABLE(SCROLLING_THREAD)
    PlatformWheelEvent platformWheelEvent = platform(wheelEvent);
#endif

#if PLATFORM(COCOA)
    switch (wheelEvent.phase()) {
    case WebWheelEvent::PhaseBegan:
        m_recentWheelEventDeltaFilter->beginFilteringDeltas();
        break;
    case WebWheelEvent::PhaseEnded:
        m_recentWheelEventDeltaFilter->endFilteringDeltas();
        break;
    default:
        break;
    }

    if (m_recentWheelEventDeltaFilter->isFilteringDeltas()) {
        m_recentWheelEventDeltaFilter->updateFromDelta(FloatSize(platformWheelEvent.deltaX(), platformWheelEvent.deltaY()));
        FloatSize filteredDelta = m_recentWheelEventDeltaFilter->filteredDelta();
        platformWheelEvent = platformWheelEvent.copyWithDeltasAndVelocity(filteredDelta.width(), filteredDelta.height(), m_recentWheelEventDeltaFilter->filteredVelocity());
    }
#endif

    auto processingSteps = OptionSet<WebCore::WheelEventProcessingSteps> { WheelEventProcessingSteps::MainThreadForScrolling, WheelEventProcessingSteps::MainThreadForBlockingDOMEventDispatch };
#if ENABLE(SCROLLING_THREAD)
    do {
        Locker locker { m_scrollingTreesLock };

        auto scrollingTree = m_scrollingTrees.get(pageID);
        if (!scrollingTree) {
            dispatchWheelEventViaMainThread(pageID, wheelEvent, processingSteps);
            break;
        }
        
        // FIXME: It's pretty horrible that we're updating the back/forward state here.
        // WebCore should always know the current state and know when it changes so the
        // scrolling tree can be notified.
        // We only need to do this at the beginning of the gesture.
        if (platformWheelEvent.phase() == PlatformWheelEventPhase::Began)
            scrollingTree->setMainFrameCanRubberBand({ canRubberBandAtTop, canRubberBandAtRight, canRubberBandAtBottom, canRubberBandAtLeft });

        auto processingSteps = scrollingTree->determineWheelEventProcessing(platformWheelEvent);

        scrollingTree->willProcessWheelEvent();

        ScrollingThread::dispatch([scrollingTree, wheelEvent, platformWheelEvent, processingSteps, pageID, protectedThis = makeRef(*this)] {
            if (processingSteps.contains(WheelEventProcessingSteps::MainThreadForScrolling)) {
                scrollingTree->willSendEventToMainThread(platformWheelEvent);
                protectedThis->dispatchWheelEventViaMainThread(pageID, wheelEvent, processingSteps);
                scrollingTree->waitForEventToBeProcessedByMainThread(platformWheelEvent);
                return;
            }
        
            auto result = scrollingTree->handleWheelEvent(platformWheelEvent, processingSteps);

            if (result.needsMainThreadProcessing()) {
                protectedThis->dispatchWheelEventViaMainThread(pageID, wheelEvent, result.steps);
                if (result.steps.contains(WheelEventProcessingSteps::MainThreadForScrolling))
                    return;
            }

            // If we scrolled on the scrolling thread (even if we send the event to the main thread for passive event handlers)
            // respond to the UI process that the event was handled.
            protectedThis->sendDidReceiveEvent(pageID, wheelEvent.type(), result.wasHandled);
        });
    } while (false);
#else
    UNUSED_PARAM(canRubberBandAtLeft);
    UNUSED_PARAM(canRubberBandAtRight);
    UNUSED_PARAM(canRubberBandAtTop);
    UNUSED_PARAM(canRubberBandAtBottom);

    dispatchWheelEventViaMainThread(pageID, wheelEvent, processingSteps);
#endif
}

#if ENABLE(MAC_GESTURE_EVENTS)
void EventDispatcher::gestureEvent(PageIdentifier pageID, const WebKit::WebGestureEvent& gestureEvent)
{
    RunLoop::main().dispatch([protectedThis = makeRef(*this), pageID, gestureEvent]() mutable {
        protectedThis->dispatchGestureEvent(pageID, gestureEvent);
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
            if (touchEvent.type() == WebEvent::TouchMove && lastEventAndCallback.first.type() == WebEvent::TouchMove && !completionHandler && !lastEventAndCallback.second)
                queuedEvents.last() = { touchEvent, nullptr };
            else
                queuedEvents.append({ touchEvent, WTFMove(completionHandler) });
        }
    }

    if (updateListWasEmpty) {
        RunLoop::main().dispatch([protectedThis = makeRef(*this)]() mutable {
            protectedThis->dispatchTouchEvents();
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

void EventDispatcher::dispatchWheelEventViaMainThread(WebCore::PageIdentifier pageID, const WebWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps)
{
    ASSERT(!RunLoop::isMain());
    RunLoop::main().dispatch([protectedThis = makeRef(*this), pageID, wheelEvent, steps = processingSteps - WheelEventProcessingSteps::ScrollingThread]() mutable {
        protectedThis->dispatchWheelEvent(pageID, wheelEvent, steps);
    });
}

void EventDispatcher::dispatchWheelEvent(PageIdentifier pageID, const WebWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps)
{
    ASSERT(RunLoop::isMain());

    WebPage* webPage = WebProcess::singleton().webPage(pageID);
    if (!webPage)
        return;

    webPage->wheelEvent(wheelEvent, processingSteps);
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

#if ENABLE(ASYNC_SCROLLING)
void EventDispatcher::sendDidReceiveEvent(PageIdentifier pageID, WebEvent::Type eventType, bool didHandleEvent)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(eventType), didHandleEvent), pageID);
}
#endif

void EventDispatcher::notifyScrollingTreesDisplayWasRefreshed(PlatformDisplayID displayID)
{
#if ENABLE(SCROLLING_THREAD)
    Locker locker { m_scrollingTreesLock };
    for (auto keyValuePair : m_scrollingTrees)
        keyValuePair.value->displayDidRefresh(displayID);
#endif
}

#if HAVE(CVDISPLAYLINK)
void EventDispatcher::displayWasRefreshed(PlatformDisplayID displayID, const DisplayUpdate& displayUpdate, bool sendToMainThread)
{
    tracePoint(DisplayRefreshDispatchingToMainThread, displayID, sendToMainThread);

    ASSERT(!RunLoop::isMain());
    notifyScrollingTreesDisplayWasRefreshed(displayID);

    if (!sendToMainThread)
        return;

    RunLoop::main().dispatch([displayID, displayUpdate]() {
        DisplayRefreshMonitorManager::sharedManager().displayWasUpdated(displayID, displayUpdate);
    });
}
#endif

} // namespace WebKit
