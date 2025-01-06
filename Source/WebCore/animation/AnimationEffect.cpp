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
#include "CSSNumericFactory.h"
#include "CSSNumericValue.h"
#include "CSSPropertyParserConsumer+Easing.h"
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
    timing.delay = secondsToWebAnimationsAPITime(m_timing.specifiedStartDelay);
    timing.endDelay = secondsToWebAnimationsAPITime(m_timing.specifiedEndDelay);
    timing.fill = m_timing.fill;
    timing.iterationStart = m_timing.iterationStart;
    timing.iterations = m_timing.iterations;
    if (auto specifiedDuration = m_timing.specifiedIterationDuration)
        timing.duration = secondsToWebAnimationsAPITime(*specifiedDuration);
    else
        timing.duration = autoAtom();
    timing.direction = m_timing.direction;
    timing.easing = m_timing.timingFunction->cssText();
    return timing;
}

AnimationEffectTiming::ResolutionData AnimationEffect::resolutionData(std::optional<WebAnimationTime> startTime) const
{
    if (!m_animation)
        return { };

    RefPtr animation = m_animation.get();
    RefPtr timeline = animation->timeline();
    return {
        timeline ? timeline->currentTime() : std::nullopt,
        timeline ? timeline->duration() : std::nullopt,
        startTime ? startTime : animation->startTime(),
        animation->currentTime(startTime),
        animation->playbackRate()
    };
}

BasicEffectTiming AnimationEffect::getBasicTiming(std::optional<WebAnimationTime> startTime)
{
    updateComputedTimingPropertiesIfNeeded();
    return m_timing.getBasicTiming(resolutionData(startTime));
}

ComputedEffectTiming AnimationEffect::getBindingsComputedTiming()
{
    if (auto* styleOriginatedAnimation = dynamicDowncast<StyleOriginatedAnimation>(animation()))
        styleOriginatedAnimation->flushPendingStyleChanges();
    return getComputedTiming();
}

ComputedEffectTiming AnimationEffect::getComputedTiming(std::optional<WebAnimationTime> startTime)
{
    updateComputedTimingPropertiesIfNeeded();

    auto data = resolutionData(startTime);
    auto resolvedTiming = m_timing.resolve(data);

    // https://drafts.csswg.org/web-animations-2/#dom-animationeffect-getcomputedtiming
    // The description of the duration attribute of the object needs to indicate that if timing.duration
    // is the string auto, this attribute will return the current calculated value of the intrinsic iteration
    // duration, which may be a expressed as a double representing the duration in milliseconds or a percentage
    // when the effect is associated with a progress-based timeline.
    auto computedDuration = [&]() -> DoubleOrCSSNumericValueOrString {
        auto& duration = m_timing.specifiedIterationDuration ? m_timing.iterationDuration : m_timing.intrinsicIterationDuration;
        if (auto percent = duration.percentage())
            return CSSNumericFactory::percent(*percent);
        ASSERT(duration.time());
        return secondsToWebAnimationsAPITime(*duration.time());
    }();

    ComputedEffectTiming computedTiming;
    computedTiming.delay = secondsToWebAnimationsAPITime(m_timing.specifiedStartDelay);
    computedTiming.endDelay = secondsToWebAnimationsAPITime(m_timing.specifiedEndDelay);
    computedTiming.fill = m_timing.fill == FillMode::Auto ? FillMode::None : m_timing.fill;
    computedTiming.iterationStart = m_timing.iterationStart;
    computedTiming.iterations = m_timing.iterations;
    computedTiming.duration = computedDuration;
    computedTiming.direction = m_timing.direction;
    computedTiming.easing = m_timing.timingFunction->cssText();
    computedTiming.endTime = m_timing.endTime;
    computedTiming.activeDuration = m_timing.activeDuration;
    computedTiming.localTime = data.localTime;
    computedTiming.simpleIterationProgress = resolvedTiming.simpleIterationProgress;
    computedTiming.progress = resolvedTiming.transformedProgress;
    computedTiming.currentIteration = resolvedTiming.currentIteration;
    computedTiming.phase = resolvedTiming.phase;
    computedTiming.before = resolvedTiming.before;
    return computedTiming;
}

ExceptionOr<void> AnimationEffect::bindingsUpdateTiming(Document& document, std::optional<OptionalEffectTiming> timing)
{
    auto retVal = updateTiming(document, timing);
    if (!retVal.hasException() && timing) {
        if (auto* cssAnimation = dynamicDowncast<CSSAnimation>(animation()))
            cssAnimation->effectTimingWasUpdatedUsingBindings(*timing);
    }
    return retVal;
}

ExceptionOr<void> AnimationEffect::updateTiming(Document& document, std::optional<OptionalEffectTiming> timing)
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

    if (auto iterations = timing->iterations) {
        // https://github.com/w3c/csswg-drafts/issues/11343
        if (std::isinf(*iterations)) {
            if (RefPtr animation = m_animation.get()) {
                if (RefPtr timeline = animation->timeline()) {
                    if (timeline->isProgressBased())
                        return Exception { ExceptionCode::TypeError, "The number of iterations cannot be set to Infinity for progress-based animations"_s };
                }
            }
        }
    }

    // 4. If the easing member of input is present but cannot be parsed using the <timing-function> production [CSS-EASING-1], throw a TypeError and abort this procedure.
    if (!timing->easing.isNull()) {
        CSSParserContext parsingContext(document);
        // FIXME: Determine the how calc() and relative units should be resolved and switch to the non-deprecated parsing function.
        auto timingFunctionResult = CSSPropertyParserHelpers::parseEasingFunctionDeprecated(timing->easing, parsingContext);
        if (!timingFunctionResult)
            return Exception { ExceptionCode::TypeError };
        setTimingFunction(WTFMove(timingFunctionResult));
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

    if (auto delay = timing->delay)
        setDelay(Seconds::fromMilliseconds(*delay));

    if (auto endDelay = timing->endDelay)
        setEndDelay(Seconds::fromMilliseconds(*endDelay));

    if (auto fill = timing->fill)
        setFill(*fill);

    if (auto iterationStart = timing->iterationStart)
        setIterationStart(*iterationStart);

    if (auto iterations = timing->iterations)
        setIterations(*iterations);

    if (auto duration = timing->duration) {
        if (auto* durationDouble = std::get_if<double>(&*duration))
            setIterationDuration(Seconds::fromMilliseconds(*durationDouble));
        else
            setIterationDuration(std::nullopt);
    }

    if (auto direction = timing->direction)
        setDirection(*direction);

    if (m_animation)
        m_animation->effectTimingDidChange();

    return { };
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
    m_timingDidMutate = true;

    return { };
}

WebAnimationTime AnimationEffect::delay()
{
    updateComputedTimingPropertiesIfNeeded();
    return m_timing.startDelay;
}

void AnimationEffect::setDelay(const Seconds& delay)
{
    if (m_timing.specifiedStartDelay == delay)
        return;

    m_timing.specifiedStartDelay = delay;
    m_timingDidMutate = true;
}

WebAnimationTime AnimationEffect::endDelay()
{
    updateComputedTimingPropertiesIfNeeded();
    return m_timing.endDelay;
}

void AnimationEffect::setEndDelay(const Seconds& endDelay)
{
    if (m_timing.specifiedEndDelay == endDelay)
        return;

    m_timing.specifiedEndDelay = endDelay;
    m_timingDidMutate = true;
}

void AnimationEffect::setFill(FillMode fill)
{
    if (m_timing.fill == fill)
        return;

    m_timing.fill = fill;
}

WebAnimationTime AnimationEffect::iterationDuration()
{
    updateComputedTimingPropertiesIfNeeded();
    return m_timing.iterationDuration;
}

void AnimationEffect::setIterationDuration(const std::optional<Seconds>& duration)
{
    if (m_timing.specifiedIterationDuration == duration)
        return;

    m_timing.specifiedIterationDuration = duration;
    m_timingDidMutate = true;
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

WebAnimationTime AnimationEffect::activeDuration()
{
    updateComputedTimingPropertiesIfNeeded();
    return m_timing.activeDuration;
}

WebAnimationTime AnimationEffect::endTime()
{
    updateComputedTimingPropertiesIfNeeded();
    return m_timing.endTime;
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

Seconds AnimationEffect::timeToNextTick(const BasicEffectTiming& timing)
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

void AnimationEffect::animationTimelineDidChange(const AnimationTimeline*)
{
    m_timingDidMutate = true;
}

void AnimationEffect::animationPlaybackRateDidChange()
{
    m_timingDidMutate = true;
}

void AnimationEffect::animationProgressBasedTimelineSourceDidChangeMetrics()
{
    m_timingDidMutate = true;
}

void AnimationEffect::animationRangeDidChange()
{
    m_timingDidMutate = true;
}

void AnimationEffect::updateComputedTimingPropertiesIfNeeded()
{
    if (!m_timingDidMutate)
        return;

    m_timingDidMutate = false;

    auto playbackRate = [&] {
        if (m_animation)
            return m_animation->playbackRate();
        return 1.0;
    }();

    auto rangeDuration = [&] -> std::optional<WebAnimationTime> {
        if (!m_animation)
            return std::nullopt;

        RefPtr timeline = m_animation->timeline();
        if (!timeline)
            return std::nullopt;

        if (RefPtr scrollTimeline = dynamicDowncast<ScrollTimeline>(timeline)) {
            auto interval = scrollTimeline->intervalForAttachmentRange(m_animation->range());
            return interval.second - interval.first;
        }

        return timeline->duration();
    }();

    m_timing.updateComputedProperties(rangeDuration, playbackRate);
}

} // namespace WebCore
