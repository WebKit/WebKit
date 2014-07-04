/*
 * Copyright (C) 2011, 2014 Apple Inc. All rights reserved.
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
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

#if ENABLE(ASYNC_SCROLLING)
#include <WebCore/AsyncScrollingCoordinator.h>
#include <WebCore/ScrollingThread.h>
#include <WebCore/ThreadedScrollingTree.h>
#endif

using namespace WebCore;

namespace WebKit {

PassRefPtr<EventDispatcher> EventDispatcher::create()
{
    return adoptRef(new EventDispatcher);
}

EventDispatcher::EventDispatcher()
    : m_queue(WorkQueue::create("com.apple.WebKit.EventDispatcher", WorkQueue::QOS::UserInteractive))
    , m_recentWheelEventDeltaTracker(std::make_unique<WheelEventDeltaTracker>())
#if ENABLE(IOS_TOUCH_EVENTS)
    , m_touchEventsLock(SPINLOCK_INITIALIZER)
#endif
{
}

EventDispatcher::~EventDispatcher()
{
}

#if ENABLE(ASYNC_SCROLLING)
void EventDispatcher::addScrollingTreeForPage(WebPage* webPage)
{
    MutexLocker locker(m_scrollingTreesMutex);

    ASSERT(webPage->corePage()->scrollingCoordinator());
    ASSERT(!m_scrollingTrees.contains(webPage->pageID()));

    AsyncScrollingCoordinator* scrollingCoordinator = toAsyncScrollingCoordinator(webPage->corePage()->scrollingCoordinator());
    m_scrollingTrees.set(webPage->pageID(), toThreadedScrollingTree(scrollingCoordinator->scrollingTree()));
}

void EventDispatcher::removeScrollingTreeForPage(WebPage* webPage)
{
    MutexLocker locker(m_scrollingTreesMutex);
    ASSERT(m_scrollingTrees.contains(webPage->pageID()));

    m_scrollingTrees.remove(webPage->pageID());
}
#endif

void EventDispatcher::initializeConnection(IPC::Connection* connection)
{
    connection->addWorkQueueMessageReceiver(Messages::EventDispatcher::messageReceiverName(), m_queue.get(), this);
}

void EventDispatcher::wheelEvent(uint64_t pageID, const WebWheelEvent& wheelEvent, bool canRubberBandAtLeft, bool canRubberBandAtRight, bool canRubberBandAtTop, bool canRubberBandAtBottom)
{
    PlatformWheelEvent platformWheelEvent = platform(wheelEvent);

#if PLATFORM(COCOA)
    switch (wheelEvent.phase()) {
    case PlatformWheelEventPhaseBegan:
        m_recentWheelEventDeltaTracker->beginTrackingDeltas();
        break;
    case PlatformWheelEventPhaseEnded:
        m_recentWheelEventDeltaTracker->endTrackingDeltas();
        break;
    default:
        break;
    }

    if (m_recentWheelEventDeltaTracker->isTrackingDeltas()) {
        m_recentWheelEventDeltaTracker->recordWheelEventDelta(platformWheelEvent);

        DominantScrollGestureDirection dominantDirection = DominantScrollGestureDirection::None;
        dominantDirection = m_recentWheelEventDeltaTracker->dominantScrollGestureDirection();

        // Workaround for scrolling issues <rdar://problem/14758615>.
        if (dominantDirection == DominantScrollGestureDirection::Vertical && platformWheelEvent.deltaX())
            platformWheelEvent = platformWheelEvent.copyIgnoringHorizontalDelta();
        else if (dominantDirection == DominantScrollGestureDirection::Horizontal && platformWheelEvent.deltaY())
            platformWheelEvent = platformWheelEvent.copyIgnoringVerticalDelta();
    }
#endif

#if ENABLE(ASYNC_SCROLLING)
    MutexLocker locker(m_scrollingTreesMutex);
    if (ThreadedScrollingTree* scrollingTree = m_scrollingTrees.get(pageID)) {
        // FIXME: It's pretty horrible that we're updating the back/forward state here.
        // WebCore should always know the current state and know when it changes so the
        // scrolling tree can be notified.
        // We only need to do this at the beginning of the gesture.
        if (platformWheelEvent.phase() == PlatformWheelEventPhaseBegan)
            ScrollingThread::dispatch(bind(&ThreadedScrollingTree::setCanRubberBandState, scrollingTree, canRubberBandAtLeft, canRubberBandAtRight, canRubberBandAtTop, canRubberBandAtBottom));

        ScrollingTree::EventResult result = scrollingTree->tryToHandleWheelEvent(platformWheelEvent);
        if (result == ScrollingTree::DidHandleEvent || result == ScrollingTree::DidNotHandleEvent) {
            sendDidReceiveEvent(pageID, wheelEvent, result == ScrollingTree::DidHandleEvent);
            return;
        }
    }
#else
    UNUSED_PARAM(canRubberBandAtLeft);
    UNUSED_PARAM(canRubberBandAtRight);
    UNUSED_PARAM(canRubberBandAtTop);
    UNUSED_PARAM(canRubberBandAtBottom);
#endif

    RunLoop::main().dispatch(bind(&EventDispatcher::dispatchWheelEvent, this, pageID, wheelEvent));
}

#if ENABLE(IOS_TOUCH_EVENTS)
void EventDispatcher::clearQueuedTouchEventsForPage(const WebPage& webPage)
{
    SpinLockHolder locker(&m_touchEventsLock);
    m_touchEvents.remove(webPage.pageID());
}

void EventDispatcher::getQueuedTouchEventsForPage(const WebPage& webPage, TouchEventQueue& destinationQueue)
{
    SpinLockHolder locker(&m_touchEventsLock);
    destinationQueue = m_touchEvents.take(webPage.pageID());
}

void EventDispatcher::touchEvent(uint64_t pageID, const WebKit::WebTouchEvent& touchEvent)
{
    bool updateListWasEmpty;
    {
        SpinLockHolder locker(&m_touchEventsLock);
        updateListWasEmpty = m_touchEvents.isEmpty();
        auto addResult = m_touchEvents.add(pageID, TouchEventQueue());
        if (addResult.isNewEntry)
            addResult.iterator->value.append(touchEvent);
        else {
            TouchEventQueue& queuedEvents = addResult.iterator->value;
            ASSERT(!queuedEvents.isEmpty());
            const WebTouchEvent& lastTouchEvent = queuedEvents.last();

            // Coalesce touch move events.
            WebEvent::Type type = lastTouchEvent.type();
            if (type == WebEvent::TouchMove)
                queuedEvents.last() = touchEvent;
            else
                queuedEvents.append(touchEvent);
        }
    }

    if (updateListWasEmpty)
        RunLoop::main().dispatch(bind(&EventDispatcher::dispatchTouchEvents, this));
}

void EventDispatcher::dispatchTouchEvents()
{
    HashMap<uint64_t, TouchEventQueue> localCopy;
    {
        SpinLockHolder locker(&m_touchEventsLock);
        localCopy.swap(m_touchEvents);
    }

    for (auto& slot : localCopy) {
        if (WebPage* webPage = WebProcess::shared().webPage(slot.key))
            webPage->dispatchAsynchronousTouchEvents(slot.value);
    }
}
#endif

void EventDispatcher::dispatchWheelEvent(uint64_t pageID, const WebWheelEvent& wheelEvent)
{
    ASSERT(RunLoop::isMain());

    WebPage* webPage = WebProcess::shared().webPage(pageID);
    if (!webPage)
        return;

    webPage->wheelEvent(wheelEvent);
}

#if ENABLE(ASYNC_SCROLLING)
void EventDispatcher::sendDidReceiveEvent(uint64_t pageID, const WebEvent& event, bool didHandleEvent)
{
    WebProcess::shared().parentProcessConnection()->send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(event.type()), didHandleEvent), pageID);
}
#endif

} // namespace WebKit
