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

#pragma once

#include "CallbackID.h"
#include "Connection.h"
#include "WebEvent.h"
#include <WebCore/PageIdentifier.h>
#include <WebCore/WheelEventDeltaFilter.h>
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadingPrimitives.h>

#if ENABLE(MAC_GESTURE_EVENTS)
#include "WebGestureEvent.h"
#endif

namespace WebCore {
class ThreadedScrollingTree;
using PlatformDisplayID = uint32_t;
}

namespace WebKit {

class WebPage;
class WebWheelEvent;

class EventDispatcher : public IPC::Connection::WorkQueueMessageReceiver {
public:
    static Ref<EventDispatcher> create();
    ~EventDispatcher();

#if ENABLE(SCROLLING_THREAD)
    void addScrollingTreeForPage(WebPage*);
    void removeScrollingTreeForPage(WebPage*);
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    using TouchEventQueue = Vector<std::pair<WebTouchEvent, Optional<CallbackID>>, 1>;
    void takeQueuedTouchEventsForPage(const WebPage&, TouchEventQueue&);
#endif

    void initializeConnection(IPC::Connection*);

private:
    EventDispatcher();

    // IPC::Connection::WorkQueueMessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Message handlers
    void wheelEvent(WebCore::PageIdentifier, const WebWheelEvent&, bool canRubberBandAtLeft, bool canRubberBandAtRight, bool canRubberBandAtTop, bool canRubberBandAtBottom);
#if ENABLE(IOS_TOUCH_EVENTS)
    void touchEvent(WebCore::PageIdentifier, const WebTouchEvent&, Optional<CallbackID>);
#endif
#if ENABLE(MAC_GESTURE_EVENTS)
    void gestureEvent(WebCore::PageIdentifier, const WebGestureEvent&);
#endif

    // This is called on the main thread.
    void dispatchWheelEvent(WebCore::PageIdentifier, const WebWheelEvent&);
    void dispatchWheelEventViaMainThread(WebCore::PageIdentifier, const WebWheelEvent&);

#if ENABLE(IOS_TOUCH_EVENTS)
    void dispatchTouchEvents();
#endif
#if ENABLE(MAC_GESTURE_EVENTS)
    void dispatchGestureEvent(WebCore::PageIdentifier, const WebGestureEvent&);
#endif

#if ENABLE(ASYNC_SCROLLING)
    static void sendDidReceiveEvent(WebCore::PageIdentifier, WebEvent::Type, bool didHandleEvent);
#endif

#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    void displayWasRefreshed(WebCore::PlatformDisplayID);
#endif

#if ENABLE(SCROLLING_THREAD)
    void displayDidRefreshOnScrollingThread(WebCore::PlatformDisplayID);
#endif

    Ref<WorkQueue> m_queue;

#if ENABLE(SCROLLING_THREAD)
    Lock m_scrollingTreesMutex;
    HashMap<WebCore::PageIdentifier, RefPtr<WebCore::ThreadedScrollingTree>> m_scrollingTrees;
#endif
    std::unique_ptr<WebCore::WheelEventDeltaFilter> m_recentWheelEventDeltaFilter;
#if ENABLE(IOS_TOUCH_EVENTS)
    Lock m_touchEventsLock;
    HashMap<WebCore::PageIdentifier, TouchEventQueue> m_touchEvents;
#endif
};

} // namespace WebKit
