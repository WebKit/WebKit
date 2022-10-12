/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "WebWheelEventCoalescer.h"

#include "Logging.h"
#include "NativeWebWheelEvent.h"
#include "WebEventConversion.h"
#include <wtf/text/TextStream.h>

namespace WebKit {

// Represents the number of wheel events we can hold in the queue before we start pushing them preemptively.
constexpr unsigned wheelEventQueueSizeThreshold = 10;

#if !LOG_DISABLED
static WTF::TextStream& operator<<(WTF::TextStream& ts, const WebWheelEvent& wheelEvent)
{
    ts << platform(wheelEvent);
    return ts;
}
#endif

bool WebWheelEventCoalescer::canCoalesce(const WebWheelEvent& a, const WebWheelEvent& b)
{
    if (a.position() != b.position())
        return false;
    if (a.globalPosition() != b.globalPosition())
        return false;
    if (a.modifiers() != b.modifiers())
        return false;
    if (a.granularity() != b.granularity())
        return false;
#if PLATFORM(COCOA)
    if (a.phase() != b.phase())
        return false;
    if (a.momentumPhase() != b.momentumPhase())
        return false;
#endif
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE)
    if (a.hasPreciseScrollingDeltas() != b.hasPreciseScrollingDeltas())
        return false;
#endif

    return true;
}

WebWheelEvent WebWheelEventCoalescer::coalesce(const WebWheelEvent& a, const WebWheelEvent& b)
{
    ASSERT(canCoalesce(a, b));

    auto mergedDelta = a.delta() + b.delta();
    auto mergedWheelTicks = a.wheelTicks() + b.wheelTicks();

#if PLATFORM(COCOA)
    auto mergedUnacceleratedScrollingDelta = a.unacceleratedScrollingDelta() + b.unacceleratedScrollingDelta();
    std::optional<WebCore::FloatSize> mergedRawPlatformScrollingDelta;
    if (a.rawPlatformDelta() && b.rawPlatformDelta())
        mergedRawPlatformScrollingDelta = a.rawPlatformDelta().value() + b.rawPlatformDelta().value();

    return WebWheelEvent({ WebEvent::Wheel, b.modifiers(), b.timestamp() }, b.position(), b.globalPosition(), mergedDelta, mergedWheelTicks, b.granularity(), b.directionInvertedFromDevice(), b.phase(), b.momentumPhase(), b.hasPreciseScrollingDeltas(), b.scrollCount(), mergedUnacceleratedScrollingDelta, b.ioHIDEventTimestamp(), mergedRawPlatformScrollingDelta, b.momentumEndType());
#elif PLATFORM(GTK) || USE(LIBWPE)
    return WebWheelEvent({ WebEvent::Wheel, b.modifiers(), b.timestamp() }, b.position(), b.globalPosition(), mergedDelta, mergedWheelTicks, b.granularity(), b.phase(), b.momentumPhase(), b.hasPreciseScrollingDeltas());
#else
    return WebWheelEvent({ WebEvent::Wheel, b.modifiers(), b.timestamp() }, b.position(), b.globalPosition(), mergedDelta, mergedWheelTicks, b.granularity());
#endif
}

bool WebWheelEventCoalescer::shouldDispatchEventNow(const WebWheelEvent& event) const
{
#if PLATFORM(GTK)
    // Don't queue events representing a non-trivial scrolling phase to
    // avoid having them trapped in the queue, potentially preventing a
    // scrolling session to beginning or end correctly.
    // This is only needed by platforms whose WebWheelEvent has this phase
    // information (Cocoa and GTK+) but Cocoa was fine without it.
    if (event.phase() == WebWheelEvent::Phase::PhaseNone
        || event.phase() == WebWheelEvent::Phase::PhaseChanged
        || event.momentumPhase() == WebWheelEvent::Phase::PhaseNone
        || event.momentumPhase() == WebWheelEvent::Phase::PhaseChanged)
        return true;
#else
    UNUSED_PARAM(event);
#endif

    return m_wheelEventQueue.size() >= wheelEventQueueSizeThreshold;
}

std::optional<WebWheelEvent> WebWheelEventCoalescer::nextEventToDispatch()
{
    if (m_wheelEventQueue.isEmpty())
        return std::nullopt;

    auto coalescedEvent = m_wheelEventQueue.takeFirst();

    auto coalescedSequence = makeUnique<CoalescedEventSequence>();
    coalescedSequence->append(coalescedEvent);

    WebWheelEvent coalescedWebEvent = coalescedEvent;

    while (!m_wheelEventQueue.isEmpty() && canCoalesce(coalescedWebEvent, m_wheelEventQueue.first())) {
        auto firstEvent = m_wheelEventQueue.takeFirst();
        coalescedSequence->append(firstEvent);
        coalescedWebEvent = coalesce(coalescedWebEvent, firstEvent);
    }

#if !LOG_DISABLED
    if (coalescedSequence->size() > 1)
        LOG_WITH_STREAM(WheelEvents, stream << "WebWheelEventCoalescer::wheelEventWithCoalescing coalesced " << *coalescedSequence << " into " << coalescedWebEvent);
#endif

    m_eventsBeingProcessed.append(WTFMove(coalescedSequence));
    return coalescedWebEvent;
}

bool WebWheelEventCoalescer::shouldDispatchEvent(const NativeWebWheelEvent& event)
{
    LOG_WITH_STREAM(WheelEvents, stream << "WebWheelEventCoalescer::shouldDispatchEvent " << event << " (" << m_wheelEventQueue.size() << " events in the queue, " << m_eventsBeingProcessed.size() << " event sequences being processed)");

    m_wheelEventQueue.append(event);

    if (!m_eventsBeingProcessed.isEmpty()) {
        if (!shouldDispatchEventNow(m_wheelEventQueue.last())) {
            LOG_WITH_STREAM(WheelEvents, stream << "WebWheelEventCoalescer::shouldDispatchEvent -  " << m_wheelEventQueue.size() << " events queued; not dispatching");
            return false;
        }
        // The queue has too many wheel events, so push a new event.
        // FIXME: This logic is confusing, and possibly not necessary.
    }

    return true;
}

NativeWebWheelEvent WebWheelEventCoalescer::takeOldestEventBeingProcessed()
{
    ASSERT(hasEventsBeingProcessed());
    auto oldestSequence = m_eventsBeingProcessed.takeFirst();
    return oldestSequence->last();
}

void WebWheelEventCoalescer::clear()
{
    m_wheelEventQueue.clear();
    m_eventsBeingProcessed.clear();
}

} // namespace WebKit
