/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "MomentumEventDispatcher.h"

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)

#include "EventDispatcher.h"
#include "Logging.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/DisplayRefreshMonitor.h>
#include <WebCore/Scrollbar.h>
#include <wtf/SystemTracing.h>

namespace WebKit {

static constexpr Seconds deltaHistoryMaximumAge = 500_ms;
static constexpr Seconds deltaHistoryMaximumInterval = 150_ms;
static constexpr float idealCurveFrameRate = 60;
static constexpr Seconds idealCurveFrameInterval = 1_s / idealCurveFrameRate;

MomentumEventDispatcher::MomentumEventDispatcher(EventDispatcher& dispatcher)
    : m_observerID(DisplayLinkObserverID::generate())
    , m_dispatcher(dispatcher)
{
}

MomentumEventDispatcher::~MomentumEventDispatcher()
{
    stopDisplayLink();
}

bool MomentumEventDispatcher::eventShouldStartSyntheticMomentumPhase(WebCore::PageIdentifier pageIdentifier, const WebWheelEvent& event) const
{
    if (event.momentumPhase() != WebWheelEvent::PhaseBegan)
        return false;

    if (!m_accelerationCurves.contains(pageIdentifier)) {
        RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher not using synthetic momentum phase: no acceleration curve");
        return false;
    }

    if (!event.rawPlatformDelta()) {
        RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher not using synthetic momentum phase: no raw platform delta on start event");
        return false;
    }

    return true;
}

bool MomentumEventDispatcher::handleWheelEvent(WebCore::PageIdentifier pageIdentifier, const WebWheelEvent& event, WebCore::RectEdges<bool> rubberBandableEdges)
{
    m_lastRubberBandableEdges = rubberBandableEdges;
    m_lastIncomingEvent = event;

    bool isMomentumEvent = event.momentumPhase() != WebWheelEvent::PhaseNone;

    if (m_currentGesture.active) {
        bool pageIdentifierChanged = pageIdentifier != m_currentGesture.pageIdentifier;

        // Any incoming wheel event other than a changed event for the current gesture
        // should interrupt the animation; in the usual case, it will be
        // momentumPhase == PhaseEnded that interrupts.
        bool eventShouldInterruptGesture = !isMomentumEvent || event.momentumPhase() != WebWheelEvent::PhaseChanged;

        if (pageIdentifierChanged || eventShouldInterruptGesture)
            didEndMomentumPhase();
    }

    if (event.phase() == WebWheelEvent::PhaseBegan || event.phase() == WebWheelEvent::PhaseChanged) {
        didReceiveScrollEvent(event);

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
        if (auto lastActivePhaseDelta = event.rawPlatformDelta())
            m_lastActivePhaseDelta = *lastActivePhaseDelta;
#endif
    }

    if (event.phase() == WebWheelEvent::PhaseEnded)
        m_lastEndedEventTimestamp = event.ioHIDEventTimestamp();

    if (eventShouldStartSyntheticMomentumPhase(pageIdentifier, event))
        didStartMomentumPhase(pageIdentifier, event);

    bool isMomentumEventDuringSyntheticGesture = isMomentumEvent && m_currentGesture.active;

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
    if (isMomentumEventDuringSyntheticGesture)
        m_currentGesture.accumulatedEventOffset += event.delta();

    auto combinedPhase = (event.phase() << 8) | (event.momentumPhase());
    m_currentLogState.latestEventPhase = combinedPhase;
    m_currentLogState.totalEventOffset += event.delta().height();
    if (!isMomentumEventDuringSyntheticGesture) {
        // Log events that we don't block to the generated offsets log as well,
        // even though we didn't technically generate them, just passed them through.
        m_currentLogState.latestGeneratedPhase = combinedPhase;
        m_currentLogState.totalGeneratedOffset += event.delta().height();
    }
    pushLogEntry();
#endif

    // Consume any normal momentum events while we're inside a synthetic momentum gesture.
    return isMomentumEventDuringSyntheticGesture;
}

static float appKitScrollMultiplierForEvent(const WebWheelEvent& event)
{
    auto delta = event.delta();
    auto unacceleratedDelta = event.unacceleratedScrollingDelta();
    float multiplier = 1;
    
    // FIXME: Can the AppKit multiplier ever differ by axis?
    if (delta.width() && unacceleratedDelta.width())
        multiplier = std::max(multiplier, delta.width() / unacceleratedDelta.width());
    if (delta.height() && unacceleratedDelta.height())
        multiplier = std::max(multiplier, delta.height() / unacceleratedDelta.height());

    return multiplier;
}

void MomentumEventDispatcher::dispatchSyntheticMomentumEvent(WebWheelEvent::Phase phase, WebCore::FloatSize delta)
{
    tracePoint(SyntheticMomentumEvent, static_cast<uint64_t>(phase), std::abs(delta.width()), std::abs(delta.height()));

    ASSERT(m_currentGesture.active);
    ASSERT(m_currentGesture.initiatingEvent);

    auto appKitScrollMultiplier = appKitScrollMultiplierForEvent(*m_currentGesture.initiatingEvent);
    auto appKitAcceleratedDelta = delta * appKitScrollMultiplier;
    auto wheelTicks = appKitAcceleratedDelta / WebCore::Scrollbar::pixelsPerLineStep();
    auto time = WallTime::now();

    // FIXME: Ideally we would stick legitimate rawPlatformDeltas on the event,
    // but currently nothing will consume them, and we'd have to keep track of them separately.
    WebWheelEvent syntheticEvent(
        WebEvent::Wheel,
        m_currentGesture.initiatingEvent->position(),
        m_currentGesture.initiatingEvent->globalPosition(),
        appKitAcceleratedDelta,
        wheelTicks,
        WebWheelEvent::ScrollByPixelWheelEvent,
        m_currentGesture.initiatingEvent->directionInvertedFromDevice(),
        WebWheelEvent::PhaseNone,
        phase,
        true,
        m_currentGesture.initiatingEvent->scrollCount(),
        delta,
        m_lastIncomingEvent->modifiers(),
        time,
        time,
        { });
    m_dispatcher.internalWheelEvent(m_currentGesture.pageIdentifier, syntheticEvent, m_lastRubberBandableEdges, EventDispatcher::WheelEventOrigin::MomentumEventDispatcher);

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
    m_currentLogState.latestGeneratedPhase = phase;
    m_currentLogState.totalGeneratedOffset += appKitAcceleratedDelta.height();
    pushLogEntry();
#endif
}

void MomentumEventDispatcher::didStartMomentumPhase(WebCore::PageIdentifier pageIdentifier, const WebWheelEvent& event)
{
    tracePoint(SyntheticMomentumStart);

    auto momentumStartInterval = event.ioHIDEventTimestamp() - m_lastEndedEventTimestamp;

    m_currentGesture.active = true;
    m_currentGesture.pageIdentifier = pageIdentifier;
    m_currentGesture.initiatingEvent = event;
    m_currentGesture.currentOffset = { };
    m_currentGesture.startTime = MonotonicTime::now() - momentumStartInterval;
    m_currentGesture.accelerationCurve = [&] () -> std::optional<ScrollingAccelerationCurve> {
        auto curveIterator = m_accelerationCurves.find(m_currentGesture.pageIdentifier);
        if (curveIterator == m_accelerationCurves.end())
            return { };
        return curveIterator->value;
    }();

    startDisplayLink();

    // FIXME: The system falls back from the table to just generating deltas
    // directly when the frame interval is within 20fps of idealCurveFrameRate;
    // we should perhaps do the same.
    float idealCurveMultiplier = m_currentGesture.accelerationCurve->frameRate() / idealCurveFrameRate;
    buildOffsetTableWithInitialDelta(*event.rawPlatformDelta() * idealCurveMultiplier);

    dispatchSyntheticMomentumEvent(WebWheelEvent::PhaseBegan, consumeDeltaForCurrentTime());
}

void MomentumEventDispatcher::didEndMomentumPhase()
{
    ASSERT(m_currentGesture.active);

    dispatchSyntheticMomentumEvent(WebWheelEvent::PhaseEnded, { });

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
    RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher saw momentum end phase with total offset %.1f %.1f, duration %f (event offset would have been %.1f %.1f)", m_currentGesture.currentOffset.width(), m_currentGesture.currentOffset.height(), (MonotonicTime::now() - m_currentGesture.startTime).seconds(), m_currentGesture.accumulatedEventOffset.width(), m_currentGesture.accumulatedEventOffset.height());
    m_dispatcher.queue().dispatchAfter(1_s, [this] {
        flushLog();
    });
#endif

    stopDisplayLink();

    m_currentGesture = { };
    tracePoint(SyntheticMomentumEnd);
}

void MomentumEventDispatcher::setScrollingAccelerationCurve(WebCore::PageIdentifier pageIdentifier, std::optional<ScrollingAccelerationCurve> curve)
{
    m_accelerationCurves.set(pageIdentifier, curve);

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
    WTF::TextStream stream(WTF::TextStream::LineMode::SingleLine);
    stream << curve;
    RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher set curve %s", stream.release().utf8().data());
#endif
}

WebCore::PlatformDisplayID MomentumEventDispatcher::displayID() const
{
    ASSERT(m_currentGesture.pageIdentifier);
    auto displayIDIterator = m_displayIDs.find(m_currentGesture.pageIdentifier);
    if (displayIDIterator == m_displayIDs.end())
        return { };
    return displayIDIterator->value;
}

void MomentumEventDispatcher::startDisplayLink()
{
    auto displayID = this->displayID();
    if (!displayID) {
        RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher failed to start display link");
        return;
    }

    // FIXME: Switch down to lower-than-full-speed frame rates for the tail end of the curve.
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StartDisplayLink(m_observerID, displayID, WebCore::FullSpeedFramesPerSecond), 0);
#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
    RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher starting display link for display %d", displayID);
#endif
}

void MomentumEventDispatcher::stopDisplayLink()
{
    auto displayID = this->displayID();
    if (!displayID) {
        RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher failed to stop display link");
        return;
    }

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebProcessProxy::StopDisplayLink(m_observerID, displayID), 0);
#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
    RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher stopping display link for display %d", displayID);
#endif
}

void MomentumEventDispatcher::pageScreenDidChange(WebCore::PageIdentifier pageID, WebCore::PlatformDisplayID displayID)
{
    bool affectsCurrentGesture = (pageID == m_currentGesture.pageIdentifier);
    if (affectsCurrentGesture)
        stopDisplayLink();

    m_displayIDs.set(pageID, displayID);

    if (affectsCurrentGesture)
        startDisplayLink();
}

WebCore::FloatSize MomentumEventDispatcher::consumeDeltaForCurrentTime()
{
    auto animationTime = MonotonicTime::now() - m_currentGesture.startTime;
    auto desiredOffset = offsetAtTime(animationTime);

#if !ENABLE(MOMENTUM_EVENT_DISPATCHER_PREMATURE_ROUNDING)
    // Intentional delta rounding (but at the end!).
    WebCore::FloatSize delta = roundedIntSize(desiredOffset - m_currentGesture.currentOffset);
#else
    WebCore::FloatSize delta = desiredOffset - m_currentGesture.currentOffset;
#endif

    m_currentGesture.currentOffset += delta;

    if (m_currentGesture.initiatingEvent->directionInvertedFromDevice())
        delta.scale(-1);

    return delta;
}

void MomentumEventDispatcher::displayWasRefreshed(WebCore::PlatformDisplayID displayID, const WebCore::DisplayUpdate&)
{
    if (!m_currentGesture.active)
        return;

    if (displayID != this->displayID())
        return;

    dispatchSyntheticMomentumEvent(WebWheelEvent::PhaseChanged, consumeDeltaForCurrentTime());
}

void MomentumEventDispatcher::didReceiveScrollEventWithInterval(WebCore::FloatSize size, Seconds frameInterval)
{
    auto push = [](HistoricalDeltas& deltas, Delta newDelta) {
        bool directionChanged = deltas.size() && (deltas.first().rawPlatformDelta > 0) != (newDelta.rawPlatformDelta > 0);
        if (directionChanged || newDelta.frameInterval > deltaHistoryMaximumAge)
            deltas.clear();

        deltas.prepend(newDelta);
        if (deltas.size() > deltaHistoryQueueSize)
            deltas.removeLast();
    };

    push(m_deltaHistoryX, { size.width(), frameInterval });
    push(m_deltaHistoryY, { size.height(), frameInterval });
}

void MomentumEventDispatcher::didReceiveScrollEvent(const WebWheelEvent& event)
{
    ASSERT(!m_currentGesture.active);

    if (!event.rawPlatformDelta())
        return;

    auto delta = *event.rawPlatformDelta();
    auto time = event.ioHIDEventTimestamp();

    if (!m_lastScrollTimestamp) {
        m_lastScrollTimestamp = time;
        // FIXME: Check that this matches the system (they may go with 10ms for the first frame).
        return;
    }

    auto frameInterval = time - *m_lastScrollTimestamp;
    m_lastScrollTimestamp = time;

    didReceiveScrollEventWithInterval(delta, frameInterval);
}

void MomentumEventDispatcher::buildOffsetTableWithInitialDelta(WebCore::FloatSize initialUnacceleratedDelta)
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER_PREMATURE_ROUNDING)
    m_currentGesture.carryOffset = { };
#endif
    m_currentGesture.offsetTable.clear();

    WebCore::FloatSize accumulatedOffset;
    WebCore::FloatSize unacceleratedDelta = initialUnacceleratedDelta;

    do {
        WebCore::FloatSize acceleratedDelta;
        std::tie(unacceleratedDelta, acceleratedDelta) = computeNextDelta(unacceleratedDelta);

        accumulatedOffset += acceleratedDelta;
        m_currentGesture.offsetTable.append(accumulatedOffset);
    } while (std::abs(unacceleratedDelta.width()) > 0.5 || std::abs(unacceleratedDelta.height()) > 0.5);

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
    RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher built table with %ld frames, initial delta %f %f, distance %f %f (initial delta from last changed event %f %f)", m_currentGesture.offsetTable.size(), initialUnacceleratedDelta.width(), initialUnacceleratedDelta.height(), accumulatedOffset.width(), accumulatedOffset.height(), m_lastActivePhaseDelta.width(), m_lastActivePhaseDelta.height());
#endif
}

static float interpolate(float a, float b, float t)
{
    return a + t * (b - a);
}

WebCore::FloatSize MomentumEventDispatcher::offsetAtTime(Seconds time)
{
    if (!m_currentGesture.offsetTable.size())
        return { };

    float fractionalFrameNumber = time.seconds() / idealCurveFrameInterval.seconds();
    unsigned long lowerFrameNumber = std::min<unsigned long>(m_currentGesture.offsetTable.size() - 1, floor(fractionalFrameNumber));
    unsigned long upperFrameNumber = std::min<unsigned long>(m_currentGesture.offsetTable.size() - 1, lowerFrameNumber + 1);
    float amount = fractionalFrameNumber - lowerFrameNumber;
    
    return {
        -interpolate(m_currentGesture.offsetTable[lowerFrameNumber].width(), m_currentGesture.offsetTable[upperFrameNumber].width(), amount),
        -interpolate(m_currentGesture.offsetTable[lowerFrameNumber].height(), m_currentGesture.offsetTable[upperFrameNumber].height(), amount)
    };
}

static float momentumDecayRate(WebCore::FloatSize delta, Seconds frameInterval)
{
    constexpr float defaultDecay = 0.975f;
    constexpr float tailVelocity = 250.f;
    constexpr float tailDecay = 0.91f;
    auto alpha = defaultDecay;

    if (frameInterval) {
        float initialVelocity = delta.diagonalLength() / frameInterval.seconds();
        float weakMomentum = std::max<float>(0.f, (tailVelocity - initialVelocity) / tailVelocity);
        alpha = defaultDecay - (defaultDecay - tailDecay) * weakMomentum;
    }

    return std::pow(alpha, (frameInterval.seconds() / 0.008f));
}

static constexpr float fromFixedPoint(float value)
{
    return value / 65536.0f;
}

std::pair<WebCore::FloatSize, WebCore::FloatSize> MomentumEventDispatcher::computeNextDelta(WebCore::FloatSize currentUnacceleratedDelta)
{
    WebCore::FloatSize unacceleratedDelta = currentUnacceleratedDelta;

    float decayRate = momentumDecayRate(unacceleratedDelta, idealCurveFrameInterval);
    unacceleratedDelta.scale(decayRate);

    auto quantizedUnacceleratedDelta = unacceleratedDelta;

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_PREMATURE_ROUNDING)
    // Round and carry.
    int32_t quantizedX = std::round(quantizedUnacceleratedDelta.width());
    int32_t quantizedY = std::round(quantizedUnacceleratedDelta.height());

    if (std::abs(quantizedUnacceleratedDelta.width()) < 1 && std::abs(quantizedUnacceleratedDelta.height()) < 1) {
        float deltaXIncludingCarry = quantizedUnacceleratedDelta.width() + m_currentGesture.carryOffset.width();
        float deltaYIncludingCarry = quantizedUnacceleratedDelta.height() + m_currentGesture.carryOffset.height();

        // Intentional truncation.
        quantizedX = deltaXIncludingCarry;
        quantizedY = deltaYIncludingCarry;
        m_currentGesture.carryOffset = { deltaXIncludingCarry - quantizedX, deltaYIncludingCarry - quantizedY };
    }

    quantizedUnacceleratedDelta = { static_cast<float>(quantizedX), static_cast<float>(quantizedY) };
#endif

    // The delta queue operates on pre-acceleration deltas, so insert the new event *before* accelerating.
    didReceiveScrollEventWithInterval(quantizedUnacceleratedDelta, idealCurveFrameInterval);

    auto accelerateAxis = [&] (HistoricalDeltas& deltas, float value) {
        float totalDelta = 0;
        Seconds totalTime;
        unsigned count = 0;

        for (const auto& delta : deltas) {
            totalDelta += std::abs(delta.rawPlatformDelta);
            count++;

            if (delta.frameInterval > deltaHistoryMaximumInterval) {
                totalTime += deltaHistoryMaximumInterval;
                break;
            }

            totalTime += delta.frameInterval;

            if (totalTime >= deltaHistoryMaximumAge)
                break;
        }

        auto averageFrameInterval = std::clamp(totalTime / count, 1_ms, deltaHistoryMaximumInterval);
        float averageFrameIntervalMS = averageFrameInterval.milliseconds();
        float averageDelta = totalDelta / count;

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
        if (!m_currentGesture.didLogInitialQueueState)
            RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher initial historical deltas: average delta %f, average time %fms, event count %d", averageDelta, averageFrameIntervalMS, count);
#endif

        constexpr float velocityGainA = fromFixedPoint(2.f);
        constexpr float velocityGainB = fromFixedPoint(955.f);
        constexpr float velocityConstant = fromFixedPoint(98369.f);
        constexpr float minimumVelocity = fromFixedPoint(1.f);

        float velocity = (velocityGainA * averageFrameIntervalMS * averageFrameIntervalMS - velocityGainB * averageFrameIntervalMS + velocityConstant) * averageDelta;
        velocity = std::max<float>(velocity, minimumVelocity);

        // FIXME: This math is incomplete if we need to support acceleration tables as well.
        float multiplier = m_currentGesture.accelerationCurve->accelerationFactor(velocity) / velocity;
        return value * multiplier;
    };

    WebCore::FloatSize acceleratedDelta(
        accelerateAxis(m_deltaHistoryX, quantizedUnacceleratedDelta.width()),
        accelerateAxis(m_deltaHistoryY, quantizedUnacceleratedDelta.height())
    );

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
    m_currentGesture.didLogInitialQueueState = true;
#endif

    return { unacceleratedDelta, acceleratedDelta };
}

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)

void MomentumEventDispatcher::pushLogEntry()
{
    m_currentLogState.time = MonotonicTime::now();
    m_log.append(m_currentLogState);
}

void MomentumEventDispatcher::flushLog()
{
    if ((MonotonicTime::now() - m_currentLogState.time) < 500_ms)
        return;

    if (m_log.isEmpty())
        return;

    auto startTime = m_log[0].time;
    RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher event log: time,generatedOffset,generatedPhase,eventOffset,eventPhase");
    for (const auto& entry : m_log)
        RELEASE_LOG(ScrollAnimations, "MomentumEventDispatcher event log: %f,%f,%d,%f,%d", (entry.time - startTime).seconds(), entry.totalGeneratedOffset, entry.latestGeneratedPhase, entry.totalEventOffset, entry.latestEventPhase);

    m_log.clear();
    m_currentLogState = { };
}

#endif

} // namespace WebKit

#endif
