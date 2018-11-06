/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "AnimationEffect.h"

#include "AnimationEffectTimingReadOnly.h"
#include "FillMode.h"
#include "JSComputedEffectTiming.h"
#include "WebAnimationUtilities.h"

namespace WebCore {

AnimationEffect::AnimationEffect(Ref<AnimationEffectTimingReadOnly>&& timing)
    , m_timing(WTFMove(timing))
{
    m_timing->setEffect(this);
}

AnimationEffect::~AnimationEffect()
{
    m_timing->setEffect(nullptr);
}

void AnimationEffect::timingDidChange()
{
    if (m_animation)
        m_animation->effectTimingPropertiesDidChange();
}

std::optional<Seconds> AnimationEffect::localTime() const
{
    if (m_animation)
        return m_animation->currentTime();
    return std::nullopt;
}

auto AnimationEffect::phase() const -> Phase
{
    // 3.5.5. Animation effect phases and states
    // https://drafts.csswg.org/web-animations-1/#animation-effect-phases-and-states

    bool animationIsBackwards = m_animation && m_animation->playbackRate() < 0;
    auto beforeActiveBoundaryTime = std::max(std::min(m_timing->delay(), m_timing->endTime()), 0_s);
    auto activeAfterBoundaryTime = std::max(std::min(m_timing->delay() + m_timing->activeDuration(), m_timing->endTime()), 0_s);

    // (This should be the last statement, but it's more efficient to cache the local time and return right away if it's not resolved.)
    // Furthermore, it is often convenient to refer to the case when an animation effect is in none of the above phases
    // as being in the idle phase.
    auto effectLocalTime = localTime();
    if (!effectLocalTime)
        return Phase::Idle;

    auto localTimeValue = effectLocalTime.value();

    // An animation effect is in the before phase if the animation effect’s local time is not unresolved and
    // either of the following conditions are met:
    //     1. the local time is less than the before-active boundary time, or
    //     2. the animation direction is ‘backwards’ and the local time is equal to the before-active boundary time.
    if ((localTimeValue + timeEpsilon) < beforeActiveBoundaryTime || (animationIsBackwards && std::abs(localTimeValue.microseconds() - beforeActiveBoundaryTime.microseconds()) < timeEpsilon.microseconds()))
        return Phase::Before;

    // An animation effect is in the after phase if the animation effect’s local time is not unresolved and
    // either of the following conditions are met:
    //     1. the local time is greater than the active-after boundary time, or
    //     2. the animation direction is ‘forwards’ and the local time is equal to the active-after boundary time.
    if ((localTimeValue - timeEpsilon) > activeAfterBoundaryTime || (!animationIsBackwards && std::abs(localTimeValue.microseconds() - activeAfterBoundaryTime.microseconds()) < timeEpsilon.microseconds()))
        return Phase::After;

    // An animation effect is in the active phase if the animation effect’s local time is not unresolved and it is not
    // in either the before phase nor the after phase.
    // (No need to check, we've already established that local time was resolved).
    return Phase::Active;
}

std::optional<Seconds> AnimationEffect::activeTime() const
{
    // 3.8.3.1. Calculating the active time
    // https://drafts.csswg.org/web-animations-1/#calculating-the-active-time

    // The active time is based on the local time and start delay. However, it is only defined
    // when the animation effect should produce an output and hence depends on its fill mode
    // and phase as follows,

    auto effectPhase = phase();

    // If the animation effect is in the before phase, the result depends on the first matching
    // condition from the following,
    if (effectPhase == Phase::Before) {
        // If the fill mode is backwards or both, return the result of evaluating
        // max(local time - start delay, 0).
        if (m_timing->fill() == FillMode::Backwards || m_timing->fill() == FillMode::Both)
            return std::max(localTime().value() - m_timing->delay(), 0_s);
        // Otherwise, return an unresolved time value.
        return std::nullopt;
    }

    // If the animation effect is in the active phase, return the result of evaluating local time - start delay.
    if (effectPhase == Phase::Active)
        return localTime().value() - m_timing->delay();

    // If the animation effect is in the after phase, the result depends on the first matching
    // condition from the following,
    if (effectPhase == Phase::After) {
        // If the fill mode is forwards or both, return the result of evaluating
        // max(min(local time - start delay, active duration), 0).
        if (m_timing->fill() == FillMode::Forwards || m_timing->fill() == FillMode::Both)
            return std::max(std::min(localTime().value() - m_timing->delay(), m_timing->activeDuration()), 0_s);
        // Otherwise, return an unresolved time value.
        return std::nullopt;
    }

    // Otherwise (the local time is unresolved), return an unresolved time value.
    return std::nullopt;
}

std::optional<double> AnimationEffect::overallProgress() const
{
    // 3.8.3.2. Calculating the overall progress
    // https://drafts.csswg.org/web-animations-1/#calculating-the-overall-progress

    // The overall progress describes the number of iterations that have completed (including partial iterations) and is defined as follows:

    // 1. If the active time is unresolved, return unresolved.
    auto effectActiveTime = activeTime();
    if (!effectActiveTime)
        return std::nullopt;

    // 2. Calculate an initial value for overall progress based on the first matching condition from below,
    double overallProgress;

    if (!m_timing->iterationDuration()) {
        // If the iteration duration is zero, if the animation effect is in the before phase, let overall progress be zero,
        // otherwise, let it be equal to the iteration count.
        overallProgress = phase() == Phase::Before ? 0 : m_timing->iterations();
    } else {
        // Otherwise, let overall progress be the result of calculating active time / iteration duration.
        overallProgress = secondsToWebAnimationsAPITime(effectActiveTime.value()) / secondsToWebAnimationsAPITime(m_timing->iterationDuration());
    }

    // 3. Return the result of calculating overall progress + iteration start.
    overallProgress += m_timing->iterationStart();
    return std::abs(overallProgress);
}

std::optional<double> AnimationEffect::simpleIterationProgress() const
{
    // 3.8.3.3. Calculating the simple iteration progress
    // https://drafts.csswg.org/web-animations-1/#calculating-the-simple-iteration-progress

    // The simple iteration progress is a fraction of the progress through the current iteration that
    // ignores transformations to the time introduced by the playback direction or timing functions
    // applied to the effect, and is calculated as follows:

    // 1. If the overall progress is unresolved, return unresolved.
    auto effectOverallProgress = overallProgress();
    if (!effectOverallProgress)
        return std::nullopt;

    // 2. If overall progress is infinity, let the simple iteration progress be iteration start % 1.0,
    // otherwise, let the simple iteration progress be overall progress % 1.0.
    double overallProgressValue = effectOverallProgress.value();
    double simpleIterationProgress = std::isinf(overallProgressValue) ? fmod(m_timing->iterationStart(), 1) : fmod(overallProgressValue, 1);

    // 3. If all of the following conditions are true,
    //
    // the simple iteration progress calculated above is zero, and
    // the animation effect is in the active phase or the after phase, and
    // the active time is equal to the active duration, and
    // the iteration count is not equal to zero.
    // let the simple iteration progress be 1.0.
    auto effectPhase = phase();
    if (!simpleIterationProgress && (effectPhase == Phase::Active || effectPhase == Phase::After) && std::abs(activeTime().value().microseconds() - m_timing->activeDuration().microseconds()) < timeEpsilon.microseconds() && m_timing->iterations())
        return 1;

    return simpleIterationProgress;
}

std::optional<double> AnimationEffect::currentIteration() const
{
    // 3.8.4. Calculating the current iteration
    // https://drafts.csswg.org/web-animations-1/#calculating-the-current-iteration

    // The current iteration can be calculated using the following steps:

    // 1. If the active time is unresolved, return unresolved.
    if (!activeTime())
        return std::nullopt;

    // 2. If the animation effect is in the after phase and the iteration count is infinity, return infinity.
    if (phase() == Phase::After && std::isinf(m_timing->iterations()))
        return std::numeric_limits<double>::infinity();

    // 3. If the simple iteration progress is 1.0, return floor(overall progress) - 1.
    if (simpleIterationProgress().value() == 1)
        return floor(overallProgress().value()) - 1;

    // 4. Otherwise, return floor(overall progress).
    return floor(overallProgress().value());
}

AnimationEffect::ComputedDirection AnimationEffect::currentDirection() const
{
    // 3.9.1. Calculating the directed progress
    // https://drafts.csswg.org/web-animations-1/#calculating-the-directed-progress

    // If playback direction is normal, let the current direction be forwards.
    if (m_timing->direction() == PlaybackDirection::Normal)
        return AnimationEffect::ComputedDirection::Forwards;
    
    // If playback direction is reverse, let the current direction be reverse.
    if (m_timing->direction() == PlaybackDirection::Reverse)
        return AnimationEffect::ComputedDirection::Reverse;
    
    // Otherwise, let d be the current iteration.
    auto d = currentIteration().value();
    // If playback direction is alternate-reverse increment d by 1.
    if (m_timing->direction() == PlaybackDirection::AlternateReverse)
        d++;
    // If d % 2 == 0, let the current direction be forwards, otherwise let the current direction be reverse.
    // If d is infinity, let the current direction be forwards.
    if (std::isinf(d) || !fmod(d, 2))
        return AnimationEffect::ComputedDirection::Forwards;
    return AnimationEffect::ComputedDirection::Reverse;
}

std::optional<double> AnimationEffect::directedProgress() const
{
    // 3.9.1. Calculating the directed progress
    // https://drafts.csswg.org/web-animations-1/#calculating-the-directed-progress

    // The directed progress is calculated from the simple iteration progress using the following steps:

    // 1. If the simple iteration progress is unresolved, return unresolved.
    auto effectSimpleIterationProgress = simpleIterationProgress();
    if (!effectSimpleIterationProgress)
        return std::nullopt;

    // 2. Calculate the current direction (we implement this as a separate method).

    // 3. If the current direction is forwards then return the simple iteration progress.
    if (currentDirection() == AnimationEffect::ComputedDirection::Forwards)
        return effectSimpleIterationProgress.value();

    // Otherwise, return 1.0 - simple iteration progress.
    return 1 - effectSimpleIterationProgress.value();
}

std::optional<double> AnimationEffect::transformedProgress() const
{
    // 3.10.1. Calculating the transformed progress
    // https://drafts.csswg.org/web-animations-1/#calculating-the-transformed-progress

    // The transformed progress is calculated from the directed progress using the following steps:
    //
    // 1. If the directed progress is unresolved, return unresolved.
    auto effectDirectedProgress = directedProgress();
    if (!effectDirectedProgress)
        return std::nullopt;

    auto effectDirectedProgressValue = effectDirectedProgress.value();

    if (auto iterationDuration = m_timing->iterationDuration().seconds()) {
        bool before = false;
        auto* timingFunction = m_timing->timingFunction();
        // 2. Calculate the value of the before flag as follows:
        if (is<StepsTimingFunction>(timingFunction)) {
            // 1. Determine the current direction using the procedure defined in §3.9.1 Calculating the directed progress.
            // 2. If the current direction is forwards, let going forwards be true, otherwise it is false.
            bool goingForwards = currentDirection() == AnimationEffect::ComputedDirection::Forwards;
            // 3. The before flag is set if the animation effect is in the before phase and going forwards is true;
            //    or if the animation effect is in the after phase and going forwards is false.
            auto effectPhase = phase();
            before = (effectPhase == Phase::Before && goingForwards) || (effectPhase == Phase::After && !goingForwards);
        }

        // 3. Return the result of evaluating the animation effect’s timing function passing directed progress as the
        //    input progress value and before flag as the before flag.
        return timingFunction->transformTime(effectDirectedProgressValue, iterationDuration, before);
    }

    return effectDirectedProgressValue;
}

std::optional<double> AnimationEffect::iterationProgress() const
{
    return transformedProgress();
}

ComputedEffectTiming AnimationEffect::getComputedTiming()
{
    ComputedEffectTiming computedTiming;
    computedTiming.delay = m_timing->bindingsDelay();
    computedTiming.endDelay = m_timing->bindingsEndDelay();
    auto fillMode = m_timing->fill();
    computedTiming.fill = fillMode == FillMode::Auto ? FillMode::None : fillMode;
    computedTiming.iterationStart = m_timing->iterationStart();
    computedTiming.iterations = m_timing->iterations();
    computedTiming.duration = secondsToWebAnimationsAPITime(m_timing->iterationDuration());
    computedTiming.direction = m_timing->direction();
    computedTiming.easing = m_timing->easing();
    computedTiming.endTime = secondsToWebAnimationsAPITime(m_timing->endTime());
    computedTiming.activeDuration = secondsToWebAnimationsAPITime(m_timing->activeDuration());
    if (auto effectLocalTime = localTime())
        computedTiming.localTime = secondsToWebAnimationsAPITime(effectLocalTime.value());
    computedTiming.progress = iterationProgress();
    computedTiming.currentIteration = currentIteration();
    return computedTiming;
}

} // namespace WebCore
