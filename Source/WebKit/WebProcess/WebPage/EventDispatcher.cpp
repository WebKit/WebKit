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
#include "WebEvent.h"
#include "WebEventConversion.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/Page.h>
#include <WebCore/WheelEventTestMonitor.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/SystemTracing.h>

#if ENABLE(ASYNC_SCROLLING)
#include <WebCore/AsyncScrollingCoordinator.h>
#endif

#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
#include <WebCore/DisplayRefreshMonitorManager.h>
#endif

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
}

#if ENABLE(SCROLLING_THREAD)
void EventDispatcher::addScrollingTreeForPage(WebPage* webPage)
{
    LockHolder locker(m_scrollingTreesMutex);

    ASSERT(webPage->corePage()->scrollingCoordinator());
    ASSERT(!m_scrollingTrees.contains(webPage->identifier()));

    AsyncScrollingCoordinator& scrollingCoordinator = downcast<AsyncScrollingCoordinator>(*webPage->corePage()->scrollingCoordinator());
    m_scrollingTrees.set(webPage->identifier(), downcast<ThreadedScrollingTree>(scrollingCoordinator.scrollingTree()));
}

void EventDispatcher::removeScrollingTreeForPage(WebPage* webPage)
{
    LockHolder locker(m_scrollingTreesMutex);
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

#if ENABLE(SCROLLING_THREAD)
    auto sentToScrollingThread = [&]() {
        LockHolder locker(m_scrollingTreesMutex);

        auto scrollingTree = m_scrollingTrees.get(pageID);
        if (!scrollingTree)
            return false;
        
        // FIXME: It's pretty horrible that we're updating the back/forward state here.
        // WebCore should always know the current state and know when it changes so the
        // scrolling tree can be notified.
        // We only need to do this at the beginning of the gesture.
        if (platformWheelEvent.phase() == PlatformWheelEventPhaseBegan)
            scrollingTree->setMainFrameCanRubberBand({ canRubberBandAtTop, canRubberBandAtRight, canRubberBandAtBottom, canRubberBandAtLeft });

        auto processingSteps = scrollingTree->determineWheelEventProcessing(platformWheelEvent);
        if (!processingSteps.contains(WheelEventProcessingSteps::ScrollingThread))
            return false;

        ScrollingThread::dispatch([scrollingTree, wheelEvent, platformWheelEvent, pageID, protectedThis = makeRef(*this)] {
            auto result = scrollingTree->handleWheelEvent(platformWheelEvent);

            if (result.needsMainThreadProcessing()) {
                protectedThis->dispatchWheelEventViaMainThread(pageID, wheelEvent);
                return;
            }

            protectedThis->sendDidReceiveEvent(pageID, wheelEvent.type(), result.wasHandled);
        });

        return true;
    }();

    if (sentToScrollingThread)
        return;

#else
    UNUSED_PARAM(canRubberBandAtLeft);
    UNUSED_PARAM(canRubberBandAtRight);
    UNUSED_PARAM(canRubberBandAtTop);
    UNUSED_PARAM(canRubberBandAtBottom);
#endif

    dispatchWheelEventViaMainThread(pageID, wheelEvent);
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
    LockHolder locker(&m_touchEventsLock);
    destinationQueue = m_touchEvents.take(webPage.identifier());
}

void EventDispatcher::touchEvent(PageIdentifier pageID, const WebKit::WebTouchEvent& touchEvent, Optional<CallbackID> callbackID)
{
    bool updateListWasEmpty;
    {
        LockHolder locker(&m_touchEventsLock);
        updateListWasEmpty = m_touchEvents.isEmpty();
        auto addResult = m_touchEvents.add(pageID, TouchEventQueue());
        if (addResult.isNewEntry)
            addResult.iterator->value.append({ touchEvent, callbackID });
        else {
            auto& queuedEvents = addResult.iterator->value;
            ASSERT(!queuedEvents.isEmpty());
            auto& lastEventAndCallback = queuedEvents.last();
            // Coalesce touch move events.
            if (touchEvent.type() == WebEvent::TouchMove && lastEventAndCallback.first.type() == WebEvent::TouchMove && !callbackID && !lastEventAndCallback.second)
                queuedEvents.last() = { touchEvent, WTF::nullopt };
            else
                queuedEvents.append({ touchEvent, callbackID });
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
        LockHolder locker(&m_touchEventsLock);
        localCopy.swap(m_touchEvents);
    }

    for (auto& slot : localCopy) {
        if (WebPage* webPage = WebProcess::singleton().webPage(slot.key))
            webPage->dispatchAsynchronousTouchEvents(slot.value);
    }
}
#endif

void EventDispatcher::dispatchWheelEventViaMainThread(WebCore::PageIdentifier pageID, const WebWheelEvent& wheelEvent)
{
    ASSERT(!RunLoop::isMain());
    RunLoop::main().dispatch([protectedThis = makeRef(*this), pageID, wheelEvent]() mutable {
        protectedThis->dispatchWheelEvent(pageID, wheelEvent);
    });
}

void EventDispatcher::dispatchWheelEvent(PageIdentifier pageID, const WebWheelEvent& wheelEvent)
{
    ASSERT(RunLoop::isMain());

    WebPage* webPage = WebProcess::singleton().webPage(pageID);
    if (!webPage)
        return;

    webPage->wheelEvent(wheelEvent);
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

#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
void EventDispatcher::displayWasRefreshed(PlatformDisplayID displayID)
{
#if ENABLE(SCROLLING_THREAD)
    LockHolder locker(m_scrollingTreesMutex);
    for (auto keyValuePair : m_scrollingTrees)
        keyValuePair.value->displayDidRefresh(displayID);
#endif

    RunLoop::main().dispatch([displayID]() {
        DisplayRefreshMonitorManager::sharedManager().displayWasUpdated(displayID);
    });
}
#endif

} // namespace WebKit
