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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebKit {

// Represents the number of wheel events we can hold in the queue before we start pushing them preemptively.
constexpr unsigned wheelEventQueueSizeThreshold = 10;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebWheelEventCoalescer);

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
#if PLATFORM(COCOA) || PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    if (a.hasPreciseScrollingDeltas() != b.hasPreciseScrollingDeltas())
        return false;
#endif

    return true;
}

Ref<WebWheelEvent> WebWheelEventCoalescer::coalesce(const WebWheelEvent& a, const WebWheelEvent& b)
{
    ASSERT(canCoalesce(a, b));

    auto mergedDelta = a.delta() + b.delta();
    auto mergedWheelTicks = a.wheelTicks() + b.wheelTicks();

#if PLATFORM(COCOA)
    auto mergedUnacceleratedScrollingDelta = a.unacceleratedScrollingDelta() + b.unacceleratedScrollingDelta();
    std::optional<WebCore::FloatSize> mergedRawPlatformScrollingDelta;
    if (a.rawPlatformDelta() && b.rawPlatformDelta())
        mergedRawPlatformScrollingDelta = a.rawPlatformDelta().value() + b.rawPlatformDelta().value();

    auto wheelData = WebWheelEventData {
        .position = b.position(),
        .globalPosition = b.globalPosition(),
        .delta = mergedDelta,
        .wheelTicks = mergedWheelTicks,
        .granularity = b.granularity(),
        .directionInvertedFromDevice = b.directionInvertedFromDevice(),
        .phase = b.phase(),
        .momentumPhase = b.momentumPhase(),
        .hasPreciseScrollingDeltas = b.hasPreciseScrollingDeltas(),
        .scrollCount = b.scrollCount(),
        .unacceleratedScrollingDelta = mergedUnacceleratedScrollingDelta,
        .ioHIDEventTimestamp = b.ioHIDEventTimestamp(),
        .rawPlatformDelta = mergedRawPlatformScrollingDelta,
        .momentumEndType = b.momentumEndType(),
        .inputSource = b.inputSource(),
    };
#elif PLATFORM(GTK) || USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    auto wheelData = WebWheelEventData {
        .position = b.position(),
        .globalPosition = b.globalPosition(),
        .delta = mergedDelta,
        .wheelTicks = mergedWheelTicks,
        .granularity = b.granularity(),
        .phase = b.phase(),
        .momentumPhase = b.momentumPhase(),
        .hasPreciseScrollingDeltas = b.hasPreciseScrollingDeltas(),
    };
#else
    auto wheelData = WebWheelEventData {
        .position = b.position(),
        .globalPosition = b.globalPosition(),
        .delta = mergedDelta,
        .wheelTicks = mergedWheelTicks,
        .granularity = b.granularity(),
    };
#endif
    return WebWheelEvent::create({ WebEventType::Wheel, b.modifiers(), b.timestamp() }, WTF::move(wheelData));
}

bool WebWheelEventCoalescer::shouldDispatchEventNow(const WebWheelEvent& event) const
{
#if PLATFORM(GTK)
    // Don't queue events representing a non-trivial scrolling phase to
    // avoid having them trapped in the queue, potentially preventing a
    // scrolling session to beginning or end correctly.
    // This is only needed by platforms whose WebWheelEvent has this phase
    // information (Cocoa and GTK+) but Cocoa was fine without it.
    if (event.phase() == WebWheelEvent::Phase::None
        || event.phase() == WebWheelEvent::Phase::Changed
        || event.momentumPhase() == WebWheelEvent::Phase::None
        || event.momentumPhase() == WebWheelEvent::Phase::Changed)
        return true;
#else
    UNUSED_PARAM(event);
#endif

    return m_wheelEventQueue.size() >= wheelEventQueueSizeThreshold;
}

RefPtr<WebWheelEvent> WebWheelEventCoalescer::nextEventToDispatch()
{
    if (m_wheelEventQueue.isEmpty())
        return nullptr;

    Ref coalescedNativeEvent = m_wheelEventQueue.takeFirst();

    auto coalescedSequence = makeUnique<CoalescedEventSequence>();
    coalescedSequence->append(coalescedNativeEvent);

    RefPtr<WebWheelEvent> coalescedWebEvent = coalescedNativeEvent.ptr();

    while (!m_wheelEventQueue.isEmpty() && canCoalesce(*coalescedWebEvent, m_wheelEventQueue.first())) {
        Ref firstEvent = m_wheelEventQueue.takeFirst();
        coalescedSequence->append(firstEvent);
        coalescedWebEvent = coalesce(*coalescedWebEvent, firstEvent);
    }

#if !LOG_DISABLED
    if (coalescedSequence->size() > 1) {
        auto platformEvents = WTF::map(*coalescedSequence, [](auto& event) { return platform(event.get()); });
        LOG_WITH_STREAM(WheelEvents, stream << "WebWheelEventCoalescer::wheelEventWithCoalescing coalesced " << platformEvents << " into " << platform(*coalescedWebEvent));
    }
#endif

    m_eventsBeingProcessed.append(WTF::move(coalescedSequence));
    return coalescedWebEvent;
}

bool WebWheelEventCoalescer::shouldDispatchEvent(Ref<NativeWebWheelEvent>&& event)
{
    LOG_WITH_STREAM(WheelEvents, stream << "WebWheelEventCoalescer::shouldDispatchEvent " << platform(event.get()) << " (" << m_wheelEventQueue.size() << " events in the queue, " << m_eventsBeingProcessed.size() << " event sequences being processed)");

    m_wheelEventQueue.append(WTF::move(event));

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

RefPtr<NativeWebWheelEvent> WebWheelEventCoalescer::takeOldestEventBeingProcessed()
{
    if (m_eventsBeingProcessed.isEmpty())
        return nullptr;

    auto oldestSequence = m_eventsBeingProcessed.takeFirst();
    return oldestSequence->last().ptr();
}

void WebWheelEventCoalescer::clear()
{
    m_wheelEventQueue.clear();
    m_eventsBeingProcessed.clear();
}

} // namespace WebKit
