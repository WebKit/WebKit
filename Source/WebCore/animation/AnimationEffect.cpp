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

#include "CSSAnimation.h"
#include "CommonAtomStrings.h"
#include "FillMode.h"
#include "JSComputedEffectTiming.h"
#include "ScriptExecutionContext.h"
#include "WebAnimation.h"
#include "WebAnimationUtilities.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(AnimationEffect);

AnimationEffect::AnimationEffect() = default;

AnimationEffect::~AnimationEffect() = default;

void AnimationEffect::setAnimation(WebAnimation* animation)
{
    if (m_animation == animation)
        return;

    m_animation = animation;
    if (animation)
        animation->updateRelevance();
}

EffectTiming AnimationEffect::getBindingsTiming() const
{
    if (auto* styleOriginatedAnimation = dynamicDowncast<StyleOriginatedAnimation>(animation()))
        styleOriginatedAnimation->flushPendingStyleChanges();

    EffectTiming timing;
    timing.delay = secondsToWebAnimationsAPITime(m_timing.delay);
    timing.endDelay = secondsToWebAnimationsAPITime(m_timing.endDelay);
    timing.fill = m_timing.fill;
    timing.iterationStart = m_timing.iterationStart;
    timing.iterations = m_timing.iterations;
    if (m_timing.iterationDuration == 0_s)
        timing.duration = autoAtom();
    else
        timing.duration = secondsToWebAnimationsAPITime(m_timing.iterationDuration);
    timing.direction = m_timing.direction;
    timing.easing = m_timing.timingFunction->cssText();
    return timing;
}

std::optional<Seconds> AnimationEffect::localTime(std::optional<Seconds> startTime) const
{
    // 4.5.4. Local time
    // https://drafts.csswg.org/web-animations-1/#local-time-section

    // The local time of an animation effect at a given moment is based on the first matching condition from the following:
    // If the animation effect is associated with an animation, the local time is the current time of the animation.
    // Otherwise, the local time is unresolved.
    if (m_animation)
        return m_animation->currentTime(startTime);
    return std::nullopt;
}

double AnimationEffect::playbackRate() const
{
    return m_animation ? m_animation->playbackRate() : 1;
}

BasicEffectTiming AnimationEffect::getBasicTiming(std::optional<Seconds> startTime) const
{
    return m_timing.getBasicTiming(localTime(startTime), playbackRate());
}

ComputedEffectTiming AnimationEffect::getBindingsComputedTiming() const
{
    if (auto* styleOriginatedAnimation = dynamicDowncast<StyleOriginatedAnimation>(animation()))
        styleOriginatedAnimation->flushPendingStyleChanges();
    return getComputedTiming();
}

ComputedEffectTiming AnimationEffect::getComputedTiming(std::optional<Seconds> startTime) const
{
    auto localTime = this->localTime(startTime);
    auto resolvedTiming = m_timing.resolve(localTime, playbackRate());

    ComputedEffectTiming computedTiming;
    computedTiming.delay = secondsToWebAnimationsAPITime(m_timing.delay);
    computedTiming.endDelay = secondsToWebAnimationsAPITime(m_timing.endDelay);
    computedTiming.fill = m_timing.fill == FillMode::Auto ? FillMode::None : m_timing.fill;
    computedTiming.iterationStart = m_timing.iterationStart;
    computedTiming.iterations = m_timing.iterations;
    computedTiming.duration = secondsToWebAnimationsAPITime(m_timing.iterationDuration);
    computedTiming.direction = m_timing.direction;
    computedTiming.easing = m_timing.timingFunction->cssText();
    computedTiming.endTime = secondsToWebAnimationsAPITime(m_timing.endTime);
    computedTiming.activeDuration = secondsToWebAnimationsAPITime(m_timing.activeDuration);
    if (localTime)
        computedTiming.localTime = secondsToWebAnimationsAPITime(*localTime);
    computedTiming.simpleIterationProgress = resolvedTiming.simpleIterationProgress;
    computedTiming.progress = resolvedTiming.transformedProgress;
    computedTiming.currentIteration = resolvedTiming.currentIteration;
    computedTiming.phase = resolvedTiming.phase;
    return computedTiming;
}

ExceptionOr<void> AnimationEffect::bindingsUpdateTiming(std::optional<OptionalEffectTiming> timing)
{
    auto retVal = updateTiming(timing);
    if (!retVal.hasException() && timing) {
        if (auto* cssAnimation = dynamicDowncast<CSSAnimation>(animation()))
            cssAnimation->effectTimingWasUpdatedUsingBindings(*timing);
    }
    return retVal;
}

ExceptionOr<void> AnimationEffect::updateTiming(std::optional<OptionalEffectTiming> timing)
{
    // 6.5.4. Updating the timing of an AnimationEffect
    // https://drafts.csswg.org/web-animations/#updating-animationeffect-timing

    // To update the timing properties of an animation effect, effect, from an EffectTiming or OptionalEffectTiming object, input, perform the following steps:
    if (!timing)
        return { };

    // 1. If the iterationStart member of input is present and less than zero, throw a TypeError and abort this procedure.
    if (timing->iterationStart) {
        if (timing->iterationStart.value() < 0)
            return Exception { ExceptionCode::TypeError };
    }

    // 2. If the iterations member of input is present, and less than zero or is the value NaN, throw a TypeError and abort this procedure.
    if (timing->iterations) {
        if (timing->iterations.value() < 0 || std::isnan(timing->iterations.value()))
            return Exception { ExceptionCode::TypeError };
    }

    // 3. If the duration member of input is present, and less than zero or is the value NaN, throw a TypeError and abort this procedure.
    // FIXME: should it not throw an exception on a string other than "auto"?
    if (timing->duration) {
        if (std::holds_alternative<double>(timing->duration.value())) {
            auto durationAsDouble = std::get<double>(timing->duration.value());
            if (durationAsDouble < 0 || std::isnan(durationAsDouble))
                return Exception { ExceptionCode::TypeError };
        } else {
            if (std::get<String>(timing->duration.value()) != autoAtom())
                return Exception { ExceptionCode::TypeError };
        }
    }

    // 4. If the easing member of input is present but cannot be parsed using the <timing-function> production [CSS-EASING-1], throw a TypeError and abort this procedure.
    if (!timing->easing.isNull()) {
        auto timingFunctionResult = TimingFunction::createFromCSSText(timing->easing);
        if (timingFunctionResult.hasException())
            return timingFunctionResult.releaseException();
        m_timing.timingFunction = timingFunctionResult.returnValue();
    }

    // 5. Assign each member present in input to the corresponding timing property of effect as follows:
    //
    //    delay → start delay
    //    endDelay → end delay
    //    fill → fill mode
    //    iterationStart → iteration start
    //    iterations → iteration count
    //    duration → iteration duration
    //    direction → playback direction
    //    easing → timing function

    if (timing->delay)
        m_timing.delay = Seconds::fromMilliseconds(timing->delay.value());

    if (timing->endDelay)
        m_timing.endDelay = Seconds::fromMilliseconds(timing->endDelay.value());

    if (timing->fill)
        m_timing.fill = timing->fill.value();

    if (timing->iterationStart)
        m_timing.iterationStart = timing->iterationStart.value();

    if (timing->iterations)
        m_timing.iterations = timing->iterations.value();

    if (timing->duration)
        m_timing.iterationDuration = std::holds_alternative<double>(timing->duration.value()) ? Seconds::fromMilliseconds(std::get<double>(timing->duration.value())) : 0_s;

    if (timing->direction)
        m_timing.direction = timing->direction.value();

    updateStaticTimingProperties();

    if (m_animation)
        m_animation->effectTimingDidChange();

    return { };
}

void AnimationEffect::updateStaticTimingProperties()
{
    m_timing.updateComputedProperties();
}

ExceptionOr<void> AnimationEffect::setIterationStart(double iterationStart)
{
    // https://drafts.csswg.org/web-animations-1/#dom-animationeffecttiming-iterationstart
    // If an attempt is made to set this attribute to a value less than zero, a TypeError must
    // be thrown and the value of the iterationStart attribute left unchanged.
    if (iterationStart < 0)
        return Exception { ExceptionCode::TypeError };

    if (m_timing.iterationStart == iterationStart)
        return { };

    m_timing.iterationStart = iterationStart;

    return { };
}

ExceptionOr<void> AnimationEffect::setIterations(double iterations)
{
    // https://drafts.csswg.org/web-animations-1/#dom-animationeffecttiming-iterations
    // If an attempt is made to set this attribute to a value less than zero or a NaN value, a
    // TypeError must be thrown and the value of the iterations attribute left unchanged.
    if (iterations < 0 || std::isnan(iterations))
        return Exception { ExceptionCode::TypeError };

    if (m_timing.iterations == iterations)
        return { };
        
    m_timing.iterations = iterations;

    return { };
}

void AnimationEffect::setDelay(const Seconds& delay)
{
    if (m_timing.delay == delay)
        return;

    m_timing.delay = delay;
}

void AnimationEffect::setEndDelay(const Seconds& endDelay)
{
    if (m_timing.endDelay == endDelay)
        return;

    m_timing.endDelay = endDelay;
}

void AnimationEffect::setFill(FillMode fill)
{
    if (m_timing.fill == fill)
        return;

    m_timing.fill = fill;
}

void AnimationEffect::setIterationDuration(const Seconds& duration)
{
    if (m_timing.iterationDuration == duration)
        return;

    m_timing.iterationDuration = duration;
}

void AnimationEffect::setDirection(PlaybackDirection direction)
{
    if (m_timing.direction == direction)
        return;

    m_timing.direction = direction;
}

void AnimationEffect::setTimingFunction(const RefPtr<TimingFunction>& timingFunction)
{
    m_timing.timingFunction = timingFunction;
}

std::optional<double> AnimationEffect::progressUntilNextStep(double iterationProgress) const
{
    RefPtr stepsTimingFunction = dynamicDowncast<StepsTimingFunction>(m_timing.timingFunction);
    if (!stepsTimingFunction)
        return std::nullopt;

    auto numberOfSteps = stepsTimingFunction->numberOfSteps();
    auto nextStepProgress = ceil(iterationProgress * numberOfSteps) / numberOfSteps;
    return nextStepProgress - iterationProgress;
}

Seconds AnimationEffect::timeToNextTick(const BasicEffectTiming& timing) const
{
    switch (timing.phase) {
    case AnimationEffectPhase::Before:
        // The effect is in its "before" phase, in this case we can wait until it enters its "active" phase.
        return delay() - *timing.localTime;
    case AnimationEffectPhase::Active: {
        if (!ticksContinuouslyWhileActive())
            return endTime() - *timing.localTime;
        if (auto iterationProgress = getComputedTiming().simpleIterationProgress) {
            // In case we're in a range that uses a steps() timing function, we can compute the time until the next step starts.
            if (auto progressUntilNextStep = this->progressUntilNextStep(*iterationProgress))
                return iterationDuration() * *progressUntilNextStep;
        }
        // Other effects that continuously tick in the "active" phase will need to update their animated
        // progress at the immediate next opportunity.
        return 0_s;
    }
    case AnimationEffectPhase::After:
        // The effect is in its after phase, which means it will no longer update its progress, so it doens't need a tick.
        return Seconds::infinity();
    case AnimationEffectPhase::Idle:
        ASSERT_NOT_REACHED();
        return Seconds::infinity();
    }

    ASSERT_NOT_REACHED();
    return Seconds::infinity();
}

} // namespace WebCore
