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

#include "DisplayLinkObserverID.h"
#include "MessageReceiver.h"
#include "MomentumEventDispatcher.h"
#include "WebEvent.h"
#include <WebCore/AnimationFrameRate.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/RectEdges.h>
#include <WebCore/ScrollingCoordinatorTypes.h>
#include <WebCore/Timer.h>
#include <WebCore/WheelEventDeltaFilter.h>
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/WorkQueue.h>

#if ENABLE(MAC_GESTURE_EVENTS)
#include "WebGestureEvent.h"
#endif

namespace WebCore {
struct DisplayUpdate;
class ThreadedScrollingTree;
using PlatformDisplayID = uint32_t;
}

namespace WebKit {

class MomentumEventDispatcher;
class ScrollingAccelerationCurve;
class WebPage;
class WebWheelEvent;

#if ENABLE(IOS_TOUCH_EVENTS)
class WebTouchEvent;
#endif

class EventDispatcher final :
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    public MomentumEventDispatcher::Client,
#endif
    private IPC::MessageReceiver {
public:
    EventDispatcher();
    ~EventDispatcher();

    enum class WheelEventOrigin : bool { UIProcess, MomentumEventDispatcher };

    WorkQueue& queue() { return m_queue.get(); }

#if ENABLE(SCROLLING_THREAD)
    void addScrollingTreeForPage(WebPage&);
    void removeScrollingTreeForPage(WebPage&);
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    using TouchEventQueue = Vector<std::pair<WebTouchEvent, CompletionHandler<void(bool)>>, 1>;
    void takeQueuedTouchEventsForPage(const WebPage&, TouchEventQueue&);
#endif

    void initializeConnection(IPC::Connection&);

    void notifyScrollingTreesDisplayDidRefresh(WebCore::PlatformDisplayID);

#if HAVE(CVDISPLAYLINK)
    // Observes displayDidRefresh at the same rate that the WebCore::Page's
    // RenderingUpdateScheduler does, but without blocking on the main thread.
    // Automatically adjusts to throttling rate changes, and the page moving
    // to a different screen.
    class AsyncRenderingRefreshObserver : public ThreadSafeRefCounted< AsyncRenderingRefreshObserver> {
    public:
        // Dispatched on the FunctionDispatcher passed when the observer
        // is added.
        virtual void displayDidRefresh() = 0;

        virtual ~AsyncRenderingRefreshObserver() = default;
    };
    void addAsyncRenderingRefreshObserver(WebCore::PageIdentifier, FunctionDispatcher&, AsyncRenderingRefreshObserver&);
    void removeAsyncRenderingRefreshObserver(WebCore::PageIdentifier, AsyncRenderingRefreshObserver&);
#endif

    void renderingUpdateFrequencyChanged(WebCore::PageIdentifier);

private:
    // IPC::MessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Message handlers
    void wheelEvent(WebCore::PageIdentifier, const WebWheelEvent&, WebCore::RectEdges<bool> rubberBandableEdges);
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    void setScrollingAccelerationCurve(WebCore::PageIdentifier, std::optional<ScrollingAccelerationCurve>);
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    void touchEvent(WebCore::PageIdentifier, const WebTouchEvent&, CompletionHandler<void(bool)>&&);
    void touchEventWithoutCallback(WebCore::PageIdentifier, const WebTouchEvent&);
#endif
#if ENABLE(MAC_GESTURE_EVENTS)
    void gestureEvent(WebCore::PageIdentifier, const WebGestureEvent&);
#endif

    // This is called on the main thread.
    void dispatchWheelEvent(WebCore::PageIdentifier, const WebWheelEvent&, OptionSet<WebCore::WheelEventProcessingSteps>, WheelEventOrigin);
    void dispatchWheelEventViaMainThread(WebCore::PageIdentifier, const WebWheelEvent&, OptionSet<WebCore::WheelEventProcessingSteps>, WheelEventOrigin);

    void internalWheelEvent(WebCore::PageIdentifier, const WebWheelEvent&, WebCore::RectEdges<bool> rubberBandableEdges, WheelEventOrigin);

#if ENABLE(IOS_TOUCH_EVENTS)
    void dispatchTouchEvents();
#endif
#if ENABLE(MAC_GESTURE_EVENTS)
    void dispatchGestureEvent(WebCore::PageIdentifier, const WebGestureEvent&);
#endif

    static void sendDidReceiveEvent(WebCore::PageIdentifier, WebEventType, bool didHandleEvent);

#if PLATFORM(MAC)
    void displayDidRefresh(WebCore::PlatformDisplayID, const WebCore::DisplayUpdate&, bool wantsFullSpeedUpdates, bool anyObserverWantsCallback);
#endif

#if ENABLE(SCROLLING_THREAD)
    void displayDidRefreshOnScrollingThread(WebCore::PlatformDisplayID);
#endif

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    // EventDispatcher::Client
    void handleSyntheticWheelEvent(WebCore::PageIdentifier, const WebWheelEvent&, WebCore::RectEdges<bool> rubberBandableEdges) override;
    void startDisplayDidRefreshCallbacks(WebCore::PlatformDisplayID) override;
    void stopDisplayDidRefreshCallbacks(WebCore::PlatformDisplayID) override;

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
    void flushMomentumEventLoggingSoon() override;
#endif
#endif

    void pageScreenDidChange(WebCore::PageIdentifier, WebCore::PlatformDisplayID, std::optional<unsigned> nominalFramesPerSecond);

    template <typename T>
    void dispatchToQueue(T&& function)
    {
        m_queue->dispatch([this, function = WTFMove(function)] () {
            assertIsCurrent(m_queue.get());
            function();
        });
    }

    template <typename T>
    static void dispatchToMainRunLoop(T&& function)
    {
        RunLoop::main().dispatch([function = WTFMove(function)] () {
            assertIsMainRunLoop();
            function();
        });
    }

    Ref<WorkQueue> m_queue;

#if HAVE(CVDISPLAYLINK)
    struct ObserverAndDispatcher {
        FunctionDispatcher& m_dispatcher;
        Ref<AsyncRenderingRefreshObserver> m_observer;
    };

    struct PageObservers {
        PageObservers();
        PageObservers(PageObservers&&) = default;
        PageObservers& operator=(PageObservers&&) = default;

        DisplayLinkObserverID m_observerID;
        std::optional<WebCore::PlatformDisplayID> m_displayID;
        std::optional<WebCore::FramesPerSecond> m_framesPerSecond;
        std::optional<Seconds> m_updateInterval;
        Vector<ObserverAndDispatcher> m_observers;

        std::unique_ptr<WebCore::Timer> m_timer;
    };
    Lock m_pageObserversLock;
    HashMap<WebCore::PageIdentifier, PageObservers> m_pageObservers WTF_GUARDED_BY_LOCK(m_pageObserversLock);

    void updateAsyncRenderingRefreshObservers(WebCore::PageIdentifier) WTF_REQUIRES_CAPABILITY(mainRunLoop);
    void notifyAsyncRenderingRefreshObserversDisplayDidRefresh(WebCore::PlatformDisplayID, const WebCore::DisplayUpdate&) WTF_REQUIRES_CAPABILITY(m_queue.get());
    void notifyPageObserversDisplayDidRefresh(PageObservers&) WTF_REQUIRES_LOCK(m_pageObserversLock) WTF_REQUIRES_CAPABILITY(m_queue.get());
    void startTimerForPage(WebCore::PageIdentifier) WTF_REQUIRES_CAPABILITY(m_queue.get());
    void stopTimerForPage(WebCore::PageIdentifier) WTF_REQUIRES_CAPABILITY(m_queue.get());
#endif

#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
    Lock m_scrollingTreesLock;
    HashMap<WebCore::PageIdentifier, RefPtr<WebCore::ThreadedScrollingTree>> m_scrollingTrees WTF_GUARDED_BY_LOCK(m_scrollingTreesLock);
#endif
    std::unique_ptr<WebCore::WheelEventDeltaFilter> m_recentWheelEventDeltaFilter;
#if ENABLE(IOS_TOUCH_EVENTS)
    Lock m_touchEventsLock;
    HashMap<WebCore::PageIdentifier, TouchEventQueue> m_touchEvents WTF_GUARDED_BY_LOCK(m_touchEventsLock);
#endif

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    std::unique_ptr<MomentumEventDispatcher> m_momentumEventDispatcher;
    DisplayLinkObserverID m_observerID;
#endif
};

} // namespace WebKit
