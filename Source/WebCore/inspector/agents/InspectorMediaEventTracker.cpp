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

#include "config.h"
#include "InspectorMediaEventTracker.h"

#if ENABLE(VIDEO)

#include "DocumentFullscreen.h"
#include "DocumentInlines.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FrameInlines.h"
#include "FrameTree.h"
#include "HTMLMediaElement.h"
#include "HTMLVideoElement.h"
#include "LocalFrame.h"
#include "Node.h"
#include "VideoPlaybackQuality.h"
#include <wtf/Borrow.h>
#include <wtf/Function.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

static void forEachInspectedNodeEventName(Node& node, const Function<void(const AtomString&)>& callback)
{
#if ENABLE(FULLSCREEN_API)
    if (isAnyOf<Document, HTMLMediaElement>(node))
        callback(eventNames().webkitfullscreenchangeEvent);
#endif // ENABLE(FULLSCREEN_API)

    if (is<HTMLMediaElement>(node)) {
        callback(eventNames().abortEvent);
        callback(eventNames().canplayEvent);
        callback(eventNames().canplaythroughEvent);
        callback(eventNames().emptiedEvent);
        callback(eventNames().endedEvent);
        callback(eventNames().loadeddataEvent);
        callback(eventNames().loadedmetadataEvent);
        callback(eventNames().loadstartEvent);
        callback(eventNames().pauseEvent);
        callback(eventNames().playEvent);
        callback(eventNames().playingEvent);
        callback(eventNames().seekedEvent);
        callback(eventNames().seekingEvent);
        callback(eventNames().stalledEvent);
        callback(eventNames().suspendEvent);
        callback(eventNames().waitingEvent);
    }
}

class EventFiredCallback final : public EventListener {
public:
    NODELETE static Ref<EventFiredCallback> create(InspectorMediaEventTracker& tracker)
    {
        return adoptRef(*new EventFiredCallback(tracker));
    }

    void handleEvent(ScriptExecutionContext&, Event& event) final
    {
        if (CheckedPtr tracker = m_tracker)
            tracker->handleEvent(event);
    }

private:
    EventFiredCallback(InspectorMediaEventTracker& tracker)
        : EventListener(EventListener::CPPEventListenerType)
        , m_tracker(tracker)
    {
    }

    const WeakPtr<InspectorMediaEventTracker> m_tracker;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorMediaEventTracker);

InspectorMediaEventTracker::InspectorMediaEventTracker(Client& client)
    : m_client(client)
    , m_mediaMetricsTimer(*this, &InspectorMediaEventTracker::mediaMetricsTimerFired)
{
}

InspectorMediaEventTracker::~InspectorMediaEventTracker() = default;

bool InspectorMediaEventTracker::isInLocalFrameTree(Node& node)
{
    for (RefPtr<Frame> frame = node.document().frame(); frame; frame = frame->tree().parent()) {
        if (!is<LocalFrame>(*frame))
            return false;
    }
    return true;
}

void InspectorMediaEventTracker::addEventListenersToNode(Node& node)
{
    if (!m_eventFiredCallback)
        m_eventFiredCallback = EventFiredCallback::create(*this);

    m_instrumentedNodes.add(node);

    forEachInspectedNodeEventName(node, [node = Ref { node }, eventFiredCallback = Ref { *m_eventFiredCallback }](const AtomString& eventName) {
        node->addEventListener(eventName, eventFiredCallback.copyRef());
    });

    if (is<HTMLMediaElement>(node)) {
        if (!m_mediaMetricsTimer.isActive())
            m_mediaMetricsTimer.start(0_s, 1_s / 15.);
    }
}

void InspectorMediaEventTracker::reset(RemoveEventListeners removeEventListeners)
{
    RefPtr eventFiredCallback = std::exchange(m_eventFiredCallback, nullptr);
    if (eventFiredCallback && removeEventListeners == RemoveEventListeners::Yes) {
        for (Ref node : m_instrumentedNodes) {
            forEachInspectedNodeEventName(node, [&](const AtomString& eventName) {
                node->removeEventListener(eventName, *eventFiredCallback, { });
            });
        }
    }

    m_instrumentedNodes.clear();
    m_dispatchedEvents.clear();

    m_mediaMetricsTimer.stop();
    m_mediaMetrics.clear();
}

void InspectorMediaEventTracker::eventDidResetAfterDispatch(const Event& event)
{
    m_dispatchedEvents.remove(&event);
}

void InspectorMediaEventTracker::handleEvent(Event& event)
{
    RefPtr currentTarget = dynamicDowncast<Node>(event.currentTarget());
    if (!currentTarget || !m_instrumentedNodes.contains(*currentTarget))
        return;

    RefPtr node = dynamicDowncast<Node>(event.target());
    if (!node || m_dispatchedEvents.contains(&event))
        return;

    auto nodeId = m_client->pushNodePathForMediaEvent(*node);
    if (!nodeId)
        return;

    m_dispatchedEvents.add(&event);

    RefPtr<JSON::Object> data = JSON::Object::create();

#if ENABLE(FULLSCREEN_API)
    if (event.type() == eventNames().webkitfullscreenchangeEvent || event.type() == eventNames().fullscreenchangeEvent)
        data->setBoolean("enabled"_s, !!protect(protect(node->document())->fullscreen())->fullscreenElement());
#endif // ENABLE(FULLSCREEN_API)

    m_client->mediaEventDidFire(nodeId, event.type(), data->size() ? WTF::move(data) : nullptr);
}

void InspectorMediaEventTracker::mediaMetricsTimerFired()
{
    const Vector<Ref<HTMLMediaElement>> mediaElements = [&] {
        Vector<Ref<HTMLMediaElement>> mediaElements;
        for (Ref node : m_instrumentedNodes) {
            if (RefPtr mediaElement = dynamicDowncast<HTMLMediaElement>(node))
                mediaElements.append(mediaElement.releaseNonNull());
        }
        return mediaElements;
    }();


    if (mediaElements.isEmpty()) {
        m_mediaMetricsTimer.stop();
        m_mediaMetrics.clear();
        return;
    }

    for (auto& mediaElement : mediaElements) {
        if (!is<HTMLVideoElement>(mediaElement) || !mediaElement->isPlaying())
            continue;

        auto videoPlaybackQuality = mediaElement->getVideoPlaybackQuality();
        unsigned displayCompositedVideoFrames = videoPlaybackQuality->displayCompositedVideoFrames();

        auto iterator = m_mediaMetrics.find(mediaElement);
        if (iterator == m_mediaMetrics.end()) {
            m_mediaMetrics.set(mediaElement.get(), MediaMetrics { displayCompositedVideoFrames });
            continue;
        }

        bool isPowerEfficient = displayCompositedVideoFrames > iterator->value.displayCompositedFrames;
        if (iterator->value.isPowerEfficient != isPowerEfficient) {
            iterator->value.isPowerEfficient = isPowerEfficient;

            if (auto nodeId = m_client->pushNodePathForMediaEvent(mediaElement.get()))
                m_client->powerEfficientPlaybackStateDidChange(nodeId, isPowerEfficient);
        }

        iterator->value.displayCompositedFrames = displayCompositedVideoFrames;
    }
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
