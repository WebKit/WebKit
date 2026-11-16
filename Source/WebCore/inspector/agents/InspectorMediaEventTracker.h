/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO)

#include "EventTarget.h"
#include "Timer.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/HashSet.h>
#include <wtf/JSONValues.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class Event;
class EventListener;
class HTMLMediaElement;
class Node;

class InspectorMediaEventTracker final : public CanMakeWeakPtr<InspectorMediaEventTracker>, public CanMakeCheckedPtr<InspectorMediaEventTracker> {
    WTF_MAKE_NONCOPYABLE(InspectorMediaEventTracker);
    WTF_MAKE_TZONE_ALLOCATED(InspectorMediaEventTracker);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorMediaEventTracker);
public:
    class Client : public AbstractCanMakeCheckedPtr {
    public:
        virtual ~Client() = default;

        virtual Inspector::Protocol::DOM::NodeId pushNodePathForMediaEvent(Node&) = 0;
        virtual void mediaEventDidFire(Inspector::Protocol::DOM::NodeId, const String& eventName, RefPtr<JSON::Object>&& data) = 0;
        virtual void powerEfficientPlaybackStateDidChange(Inspector::Protocol::DOM::NodeId, bool isPowerEfficient) = 0;
    };

    explicit InspectorMediaEventTracker(Client&);
    ~InspectorMediaEventTracker();

    static bool isInLocalFrameTree(Node&);

    void addEventListenersToNode(Node&);
    void eventDidResetAfterDispatch(const Event&);

    enum class RemoveEventListeners : bool { No, Yes };
    void reset(RemoveEventListeners);

private:
    friend class EventFiredCallback;
    void handleEvent(Event&);
    void mediaMetricsTimerFired();

    const CheckedRef<Client> m_client;
    RefPtr<EventListener> m_eventFiredCallback;
    WeakHashSet<Node, WeakPtrImplWithEventTargetData> m_instrumentedNodes;
    HashSet<const Event*> m_dispatchedEvents;

    Timer m_mediaMetricsTimer;
    struct MediaMetrics {
        unsigned displayCompositedFrames { 0 };
        bool isPowerEfficient { false };
    };
    WeakHashMap<HTMLMediaElement, MediaMetrics> m_mediaMetrics;
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
