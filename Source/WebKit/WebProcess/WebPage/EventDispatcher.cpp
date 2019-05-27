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
#include <WebCore/WheelEventTestTrigger.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

#if ENABLE(ASYNC_SCROLLING)
#include <WebCore/AsyncScrollingCoordinator.h>
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

#if ENABLE(ASYNC_SCROLLING)
void EventDispatcher::addScrollingTreeForPage(WebPage* webPage)
{
    LockHolder locker(m_scrollingTreesMutex);

    ASSERT(webPage->corePage()->scrollingCoordinator());
    ASSERT(!m_scrollingTrees.contains(webPage->pageID()));

    AsyncScrollingCoordinator& scrollingCoordinator = downcast<AsyncScrollingCoordinator>(*webPage->corePage()->scrollingCoordinator());
    m_scrollingTrees.set(webPage->pageID(), downcast<ThreadedScrollingTree>(scrollingCoordinator.scrollingTree()));
}

void EventDispatcher::removeScrollingTreeForPage(WebPage* webPage)
{
    LockHolder locker(m_scrollingTreesMutex);
    ASSERT(m_scrollingTrees.contains(webPage->pageID()));

    m_scrollingTrees.remove(webPage->pageID());
}
#endif

void EventDispatcher::initializeConnection(IPC::Connection* connection)
{
    connection->addWorkQueueMessageReceiver(Messages::EventDispatcher::messageReceiverName(), m_queue.get(), this);
}

void EventDispatcher::wheelEvent(PageIdentifier pageID, const WebWheelEvent& wheelEvent, bool canRubberBandAtLeft, bool canRubberBandAtRight, bool canRubberBandAtTop, bool canRubberBandAtBottom)
{
#if PLATFORM(COCOA) || ENABLE(ASYNC_SCROLLING)
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

#if ENABLE(ASYNC_SCROLLING)
    LockHolder locker(m_scrollingTreesMutex);
    if (RefPtr<ThreadedScrollingTree> scrollingTree = m_scrollingTrees.get(pageID)) {
        // FIXME: It's pretty horrible that we're updating the back/forward state here.
        // WebCore should always know the current state and know when it changes so the
        // scrolling tree can be notified.
        // We only need to do this at the beginning of the gesture.
        if (platformWheelEvent.phase() == PlatformWheelEventPhaseBegan)
            scrollingTree->setCanRubberBandState(canRubberBandAtLeft, canRubberBandAtRight, canRubberBandAtTop, canRubberBandAtBottom);

        ScrollingEventResult result = scrollingTree->tryToHandleWheelEvent(platformWheelEvent);

        if (result == ScrollingEventResult::DidHandleEvent || result == ScrollingEventResult::DidNotHandleEvent) {
            sendDidReceiveEvent(pageID, wheelEvent, result == ScrollingEventResult::DidHandleEvent);
            return;
        }
    }
#else
    UNUSED_PARAM(canRubberBandAtLeft);
    UNUSED_PARAM(canRubberBandAtRight);
    UNUSED_PARAM(canRubberBandAtTop);
    UNUSED_PARAM(canRubberBandAtBottom);
#endif

    RunLoop::main().dispatch([protectedThis = makeRef(*this), pageID, wheelEvent]() mutable {
        protectedThis->dispatchWheelEvent(pageID, wheelEvent);
    }); 
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
void EventDispatcher::clearQueuedTouchEventsForPage(const WebPage& webPage)
{
    LockHolder locker(&m_touchEventsLock);
    m_touchEvents.remove(webPage.pageID());
}

void EventDispatcher::getQueuedTouchEventsForPage(const WebPage& webPage, TouchEventQueue& destinationQueue)
{
    LockHolder locker(&m_touchEventsLock);
    destinationQueue = m_touchEvents.take(webPage.pageID());
}

void EventDispatcher::touchEvent(PageIdentifier pageID, const WebKit::WebTouchEvent& touchEvent)
{
    bool updateListWasEmpty;
    {
        LockHolder locker(&m_touchEventsLock);
        updateListWasEmpty = m_touchEvents.isEmpty();
        auto addResult = m_touchEvents.add(pageID, TouchEventQueue());
        if (addResult.isNewEntry)
            addResult.iterator->value.append(touchEvent);
        else {
            TouchEventQueue& queuedEvents = addResult.iterator->value;
            ASSERT(!queuedEvents.isEmpty());
            const WebTouchEvent& lastTouchEvent = queuedEvents.last();

            // Coalesce touch move events.
            if (touchEvent.type() == WebEvent::TouchMove && lastTouchEvent.type() == WebEvent::TouchMove)
                queuedEvents.last() = touchEvent;
            else
                queuedEvents.append(touchEvent);
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
void EventDispatcher::sendDidReceiveEvent(PageIdentifier pageID, const WebEvent& event, bool didHandleEvent)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(event.type()), didHandleEvent), pageID);
}
#endif

} // namespace WebKit
