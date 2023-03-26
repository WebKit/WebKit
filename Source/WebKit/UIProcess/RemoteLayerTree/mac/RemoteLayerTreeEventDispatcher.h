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

#pragma once

#if PLATFORM(MAC) && ENABLE(SCROLLING_THREAD)

#include "DisplayLinkObserverID.h"
#include "MomentumEventDispatcher.h"
#include "NativeWebWheelEvent.h"
#include <pal/HysteresisActivity.h>
#include <wtf/Deque.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
class PlatformWheelEvent;
class WheelEventDeltaFilter;
struct WheelEventHandlingResult;
};

namespace WebKit {

class DisplayLink;
class NativeWebWheelEvent;
class RemoteScrollingCoordinatorProxyMac;
class RemoteScrollingTree;
class RemoteLayerTreeEventDispatcherDisplayLinkClient;

// This class exists to act as a threadsafe DisplayLink::Client client, allowing RemoteScrollingCoordinatorProxyMac to
// be main-thread only. It's the UI-process analogue of WebPage/EventDispatcher.
class RemoteLayerTreeEventDispatcher : public ThreadSafeRefCounted<RemoteLayerTreeEventDispatcher>, public MomentumEventDispatcher::Client {
    WTF_MAKE_FAST_ALLOCATED();
    friend class RemoteLayerTreeEventDispatcherDisplayLinkClient;
public:
    static Ref<RemoteLayerTreeEventDispatcher> create(RemoteScrollingCoordinatorProxyMac&, WebCore::PageIdentifier);

    explicit RemoteLayerTreeEventDispatcher(RemoteScrollingCoordinatorProxyMac&, WebCore::PageIdentifier);
    ~RemoteLayerTreeEventDispatcher();
    
    void invalidate();
    
    void cacheWheelEventScrollingAccelerationCurve(const NativeWebWheelEvent&);

    void handleWheelEvent(const WebWheelEvent&, WebCore::RectEdges<bool> rubberBandableEdges);

    void setScrollingTree(RefPtr<RemoteScrollingTree>&&);

    void didRefreshDisplay(WebCore::PlatformDisplayID);
    void mainThreadDisplayDidRefresh(WebCore::PlatformDisplayID);

    void hasNodeWithAnimatedScrollChanged(bool hasAnimatedScrolls);

    void windowScreenDidChange(WebCore::PlatformDisplayID, std::optional<WebCore::FramesPerSecond>);

private:
    OptionSet<WheelEventProcessingSteps> determineWheelEventProcessing(const WebCore::PlatformWheelEvent&, WebCore::RectEdges<bool> rubberBandableEdges);

    WebCore::WheelEventHandlingResult scrollingThreadHandleWheelEvent(const WebWheelEvent&, WebCore::RectEdges<bool> rubberBandableEdges);
    WebCore::WheelEventHandlingResult internalHandleWheelEvent(const WebCore::PlatformWheelEvent&, OptionSet<WheelEventProcessingSteps>);

    WebCore::PlatformWheelEvent filteredWheelEvent(const WebCore::PlatformWheelEvent&);

    void wheelEventHysteresisUpdated(PAL::HysteresisState);

    void willHandleWheelEvent(const WebWheelEvent&);
    void wheelEventWasHandledByScrollingThread(WheelEventHandlingResult);

    DisplayLink* displayLink() const;

    void startDisplayLinkObserver();
    void stopDisplayLinkObserver();

    void startOrStopDisplayLink();
    void startOrStopDisplayLinkOnMainThread();

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    void handleSyntheticWheelEvent(WebCore::PageIdentifier, const WebWheelEvent&, WebCore::RectEdges<bool> rubberBandableEdges);
    void startDisplayDidRefreshCallbacks(WebCore::PlatformDisplayID);
    void stopDisplayDidRefreshCallbacks(WebCore::PlatformDisplayID);
#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
    void flushMomentumEventLoggingSoon();
#endif
#endif

    RefPtr<RemoteScrollingTree> scrollingTree();

    Lock m_scrollingTreeLock;
    RefPtr<RemoteScrollingTree> m_scrollingTree WTF_GUARDED_BY_LOCK(m_scrollingTreeLock);

    Deque<WebWheelEvent, 2> m_wheelEventsBeingProcessed; // FIXME: Remove

    WeakPtr<RemoteScrollingCoordinatorProxyMac> m_scrollingCoordinator;
    WebCore::PageIdentifier m_pageIdentifier;

    std::unique_ptr<WebCore::WheelEventDeltaFilter> m_wheelEventDeltaFilter;
    std::unique_ptr<RemoteLayerTreeEventDispatcherDisplayLinkClient> m_displayLinkClient;
    std::optional<DisplayLinkObserverID> m_displayRefreshObserverID;
    PAL::HysteresisActivity m_wheelEventActivityHysteresis;

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    std::unique_ptr<MomentumEventDispatcher> m_momentumEventDispatcher;
    bool m_momentumEventDispatcherNeedsDisplayLink { false };
#endif
};

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(SCROLLING_THREAD)
