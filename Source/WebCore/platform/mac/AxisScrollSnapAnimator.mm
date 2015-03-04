/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "AxisScrollSnapAnimator.h"

#if ENABLE(CSS_SCROLL_SNAP)

namespace WebCore {

const float inertialScrollPredictionFactor = 16.7;
const float snapMagnitudeMax = 25;
const float snapMagnitudeMin = 5;
const float snapThresholdHigh = 1000;
const float snapThresholdLow = 50;
const float glideBoostMultiplier = 3.5;
const float maxTargetWheelDelta = 7;
const float minTargetWheelDelta = 3.5;
const float initialToFinalMomentumFactor = 1.0 / 40.0;

static inline WheelEventStatus toWheelEventStatus(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase)
{
    if (phase == PlatformWheelEventPhaseNone) {
        switch (momentumPhase) {
        case PlatformWheelEventPhaseBegan:
            return WheelEventStatus::InertialScrollBegin;

        case PlatformWheelEventPhaseChanged:
            return WheelEventStatus::InertialScrolling;

        case PlatformWheelEventPhaseEnded:
            return WheelEventStatus::InertialScrollEnd;

        default:
            return WheelEventStatus::Unknown; 
        }
    }
    if (momentumPhase == PlatformWheelEventPhaseNone) {
        switch (phase) {
        case PlatformWheelEventPhaseBegan:
            return WheelEventStatus::UserScrollBegin;

        case PlatformWheelEventPhaseChanged:
            return WheelEventStatus::UserScrolling;

        case PlatformWheelEventPhaseEnded:
            return WheelEventStatus::UserScrollEnd;

        default:
            return WheelEventStatus::Unknown;
        }
    }
    return WheelEventStatus::Unknown;
}

static inline float projectedInertialScrollDistance(float initialWheelDelta)
{
    // FIXME: Experiments with inertial scrolling show a fairly consistent linear relationship between initial wheel delta and total distance scrolled.
    // In the future, we'll want to find a more accurate way of inertial scroll prediction.
    return inertialScrollPredictionFactor * initialWheelDelta;
}

AxisScrollSnapAnimator::AxisScrollSnapAnimator(AxisScrollSnapAnimatorClient* client, const Vector<LayoutUnit>* snapOffsets, ScrollEventAxis axis)
    : m_client(client)
    , m_snapOffsets(snapOffsets)
    , m_axis(axis)
    , m_currentState(ScrollSnapState::DestinationReached)
    , m_initialOffset(0)
    , m_targetOffset(0)
    , m_beginTrackingWheelDeltaOffset(0)
    , m_numWheelDeltasTracked(0)
    , m_glideMagnitude(0)
    , m_glidePhaseShift(0)
    , m_glideInitialWheelDelta(0)
    , m_shouldOverrideWheelEvent(false)
{
}

void AxisScrollSnapAnimator::handleWheelEvent(const PlatformWheelEvent& event)
{
    float wheelDelta = m_axis == ScrollEventAxis::Horizontal ? -event.deltaX() : -event.deltaY();
    WheelEventStatus wheelStatus = toWheelEventStatus(event.phase(), event.momentumPhase());

    switch (wheelStatus) {
    case WheelEventStatus::UserScrollBegin:
    case WheelEventStatus::UserScrolling:
        endScrollSnapAnimation(ScrollSnapState::UserInteraction);
        break;

    case WheelEventStatus::UserScrollEnd:
        beginScrollSnapAnimation(ScrollSnapState::Snapping);
        break;

    case WheelEventStatus::InertialScrollBegin:
        // Begin tracking wheel deltas for glide prediction.
        endScrollSnapAnimation(ScrollSnapState::UserInteraction);
        pushInitialWheelDelta(wheelDelta);
        m_beginTrackingWheelDeltaOffset = m_client->scrollOffsetOnAxis(m_axis);
        break;

    case WheelEventStatus::InertialScrolling:
        // This check for DestinationReached ensures that we don't receive another set of momentum events after ending the last glide.
        if (m_currentState != ScrollSnapState::Gliding && m_currentState != ScrollSnapState::DestinationReached) {
            if (m_numWheelDeltasTracked < wheelDeltaWindowSize)
                pushInitialWheelDelta(wheelDelta);

            if (m_numWheelDeltasTracked == wheelDeltaWindowSize)
                beginScrollSnapAnimation(ScrollSnapState::Gliding);
        }
        break;

    case WheelEventStatus::InertialScrollEnd:
        beginScrollSnapAnimation(ScrollSnapState::Snapping);
        clearInitialWheelDeltaWindow();
        m_shouldOverrideWheelEvent = false;
        break;

    case WheelEventStatus::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

bool AxisScrollSnapAnimator::shouldOverrideWheelEvent(const PlatformWheelEvent& event) const
{
    return m_shouldOverrideWheelEvent && toWheelEventStatus(event.phase(), event.momentumPhase()) == WheelEventStatus::InertialScrolling;
}

void AxisScrollSnapAnimator::scrollSnapAnimationUpdate()
{
    ASSERT(m_currentState == ScrollSnapState::Gliding || m_currentState == ScrollSnapState::Snapping);
    float delta = m_currentState == ScrollSnapState::Snapping ? computeSnapDelta() : computeGlideDelta();
    if (delta)
        m_client->immediateScrollOnAxis(m_axis, delta);
    else
        endScrollSnapAnimation(ScrollSnapState::DestinationReached);
}

void AxisScrollSnapAnimator::beginScrollSnapAnimation(ScrollSnapState newState)
{
    ASSERT(newState == ScrollSnapState::Gliding || newState == ScrollSnapState::Snapping);
    LayoutUnit offset = m_client->scrollOffsetOnAxis(m_axis);
    float initialWheelDelta = newState == ScrollSnapState::Gliding ? averageInitialWheelDelta() : 0;
    LayoutUnit projectedScrollDestination = newState == ScrollSnapState::Gliding ? m_beginTrackingWheelDeltaOffset + LayoutUnit(projectedInertialScrollDistance(initialWheelDelta)) : offset;
    projectedScrollDestination = std::min(std::max(projectedScrollDestination, m_snapOffsets->first()), m_snapOffsets->last());
    m_initialOffset = offset;
    m_targetOffset = closestSnapOffset<LayoutUnit, float>(*m_snapOffsets, projectedScrollDestination, initialWheelDelta);
    if (m_initialOffset == m_targetOffset)
        return;

    m_currentState = newState;
    if (newState == ScrollSnapState::Gliding) {
        m_shouldOverrideWheelEvent = true;
        m_glideInitialWheelDelta = initialWheelDelta;
        bool glideRequiresBoost;
        if (initialWheelDelta > 0)
            glideRequiresBoost = projectedScrollDestination - offset < m_targetOffset - projectedScrollDestination;
        else
            glideRequiresBoost = offset - projectedScrollDestination < projectedScrollDestination - m_targetOffset;

        initializeGlideParameters(glideRequiresBoost);
        clearInitialWheelDeltaWindow();
    }
    m_client->startScrollSnapTimer(m_axis);
}

void AxisScrollSnapAnimator::endScrollSnapAnimation(ScrollSnapState newState)
{
    ASSERT(newState == ScrollSnapState::DestinationReached || newState == ScrollSnapState::UserInteraction);
    if (m_currentState == ScrollSnapState::Gliding)
        clearInitialWheelDeltaWindow();

    m_currentState = newState;
    m_client->stopScrollSnapTimer(m_axis);
}

// Computes the amount to scroll by when performing a "snap" operation, i.e. when a user releases the trackpad without flicking. The snap delta
// is a function of progress t, where t is equal to DISTANCE_TRAVELED / TOTAL_DISTANCE, DISTANCE_TRAVELED is the distance from the initialOffset
// to the current offset, and TOTAL_DISTANCE is the distance from initialOffset to targetOffset. The snapping equation is as follows:
// delta(t) = MAGNITUDE * sin(PI * t). MAGNITUDE indicates the top speed reached near the middle of the animation (t = 0.5), and is a linear
// relationship of the distance traveled, clamped by arbitrary min and max values.
float AxisScrollSnapAnimator::computeSnapDelta() const
{
    LayoutUnit offset = m_client->scrollOffsetOnAxis(m_axis);
    bool canComputeSnap =  (m_initialOffset <= offset && offset < m_targetOffset) || (m_targetOffset < offset && offset <= m_initialOffset);
    if (m_currentState != ScrollSnapState::Snapping || !canComputeSnap)
        return 0;

    int sign = m_initialOffset < m_targetOffset ? 1 : -1;
    float progress = ((float)(offset - m_initialOffset)) / (m_targetOffset - m_initialOffset);
    // Threshold the distance before computing magnitude, so only distances within a certain range are considered.
    float thresholdedDistance = std::min(std::max<float>((m_targetOffset - m_initialOffset) * sign, snapThresholdLow), snapThresholdHigh);
    float magnitude = snapMagnitudeMin + (snapMagnitudeMax - snapMagnitudeMin) * (thresholdedDistance - snapThresholdLow) / (snapThresholdHigh - snapThresholdLow);
    float rawSnapAmount = std::max<float>(1, magnitude * sin(piFloat * progress));
    if ((m_targetOffset < offset && offset - rawSnapAmount < m_targetOffset) || (m_targetOffset > offset && offset + rawSnapAmount > m_targetOffset))
        return m_targetOffset - offset;

    return sign * rawSnapAmount;
}

// Computes the amount to scroll by when performing a "glide" operation, i.e. when a user releases the trackpad with an initial velocity. Here,
// we want the scroll offset to animate directly to the snap point. The snap delta is a function of progress t, where t is equal to
// DISTANCE_TRAVELED / TOTAL_DISTANCE, DISTANCE_TRAVELED is the distance from the initialOffset to the current offset, and TOTAL_DISTANCE is
// the distance from initialOffset to targetOffset.
// The general model of our gliding equation is delta(t) = MAGNITUDE * (1 + cos(PI * t + PHASE_SHIFT)). This was determined after examining the
// momentum velocity curve as a function of progress. To compute MAGNITUDE and PHASE_SHIFT, we use initial velocity V0 and the final velocity VF,
// both as wheel deltas (pixels per timestep). VF should be a small value (< 10) chosen based on the initial velocity and TOTAL_DISTANCE.
// We also enforce the following constraints for the gliding equation:
//   1. delta(0) = V0, since we want the initial velocity of the gliding animation to match the user's scroll velocity. The exception to this is
//      when the glide velocity is not enough to naturally reach the next snap point, and thus requires a boost (see initializeGlideParameters)
//   2. delta(1) = VF, since at t=1, the animation has completed and we want the last wheel delta to match the final velocity VF. Note that this
//      doesn't guarantee that the final velocity will be exactly VF. However, assuming that the initial velocity is much less than TOTAL_DISTANCE,
//      the last wheel delta will be very close, if not the same, as VF.
// For MAGNITUDE = (V0 + VF) / 2 and PHASE_SHIFT = arccos((V0 - VF) / (V0 + VF)), observe that delta(0) and delta(1) evaluate respectively to V0
// and VF. Thus, we can express our gliding equation all in terms of V0, VF and t.
float AxisScrollSnapAnimator::computeGlideDelta() const
{
    LayoutUnit offset = m_client->scrollOffsetOnAxis(m_axis);
    bool canComputeGlide = (m_initialOffset <= offset && offset < m_targetOffset) || (m_targetOffset < offset && offset <= m_initialOffset);
    if (m_currentState != ScrollSnapState::Gliding || !canComputeGlide)
        return 0;

    float progress = ((float)(offset - m_initialOffset)) / (m_targetOffset - m_initialOffset);
    // FIXME: We might want to investigate why -m_glidePhaseShift results in the behavior we want.
    float shift = ceil(m_glideMagnitude * (1 + cos(piFloat * progress - m_glidePhaseShift)));
    shift = m_initialOffset < m_targetOffset ? std::max<float>(shift, 1) : std::min<float>(shift, -1);
    if ((m_initialOffset < m_targetOffset && offset + shift > m_targetOffset) || (m_initialOffset > m_targetOffset && offset + shift < m_targetOffset))
        return m_targetOffset - offset;

    return shift;
}

void AxisScrollSnapAnimator::initializeGlideParameters(bool shouldIncreaseInitialWheelDelta)
{
    // FIXME: Glide boost is a hacky way to speed up natural scrolling velocity. We should find a better way to accomplish this.
    if (shouldIncreaseInitialWheelDelta)
        m_glideInitialWheelDelta *= glideBoostMultiplier;

    // FIXME: There must be a better way to determine a good target delta than multiplying by a factor and clamping to min/max values.
    float targetFinalWheelDelta = initialToFinalMomentumFactor * (m_glideInitialWheelDelta < 0 ? -m_glideInitialWheelDelta : m_glideInitialWheelDelta);
    targetFinalWheelDelta = (m_glideInitialWheelDelta > 0 ? 1 : -1) * std::min(std::max(targetFinalWheelDelta, minTargetWheelDelta), maxTargetWheelDelta);
    m_glideMagnitude = (m_glideInitialWheelDelta + targetFinalWheelDelta) / 2;
    m_glidePhaseShift = acos((m_glideInitialWheelDelta - targetFinalWheelDelta) / (m_glideInitialWheelDelta + targetFinalWheelDelta));
}

void AxisScrollSnapAnimator::pushInitialWheelDelta(float wheelDelta)
{
    if (m_numWheelDeltasTracked < wheelDeltaWindowSize)
        m_wheelDeltaWindow[m_numWheelDeltasTracked++] = wheelDelta;
}

float AxisScrollSnapAnimator::averageInitialWheelDelta() const
{
    if (!m_numWheelDeltasTracked)
        return 0;

    float sum = 0;
    for (int i = 0; i < m_numWheelDeltasTracked; i++)
        sum += m_wheelDeltaWindow[i];

    return sum / m_numWheelDeltasTracked;
}

void AxisScrollSnapAnimator::clearInitialWheelDeltaWindow()
{
    for (int i = 0; i < m_numWheelDeltasTracked; i++)
        m_wheelDeltaWindow[i] = 0;

    m_numWheelDeltasTracked = 0;
}

} // namespace WebCore

#endif // CSS_SCROLL_SNAP
