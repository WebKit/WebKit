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

#pragma once

#include <WebCore/AnimationEffectTiming.h>
#include <WebCore/BasicEffectTiming.h>
#include <WebCore/ComputedEffectTiming.h>
#include <WebCore/FillMode.h>
#include <WebCore/KeyframeEffectOptions.h>
#include <WebCore/OptionalEffectTiming.h>
#include <WebCore/PlaybackDirection.h>
#include <WebCore/TimingFunction.h>
#include <WebCore/WebAnimation.h>
#include <WebCore/WebAnimationUtilities.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Seconds.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AnimationEffect : public RefCountedAndCanMakeWeakPtr<AnimationEffect> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(AnimationEffect);
public:
    virtual ~AnimationEffect();

    virtual bool isCustomEffect() const { return false; }
    virtual bool isKeyframeEffect() const { return false; }

    EffectTiming getBindingsTiming() const;
    BasicEffectTiming getBasicTiming();
    ComputedEffectTiming getBindingsComputedTiming();
    ComputedEffectTiming getComputedTiming(UseCachedCurrentTime = UseCachedCurrentTime::Yes, EndpointInclusiveActiveInterval = EndpointInclusiveActiveInterval::No);
    ExceptionOr<void> bindingsUpdateTiming(Document&, std::optional<OptionalEffectTiming>);
    ExceptionOr<void> updateTiming(Document&, std::optional<OptionalEffectTiming>);

    virtual void animationDidTick() { };
    virtual void animationDidChangeTimingProperties() { };
    virtual void animationWasCanceled() { };
    virtual void animationSuspensionStateDidChange(bool) { };
    virtual void animationTimelineDidChange(const AnimationTimeline*);
    virtual void animationDidFinish() { };
    virtual void animationPlaybackRateDidChange();
    virtual void animationProgressBasedTimelineSourceDidChangeMetrics(const TimelineRange&);
    void animationRangeDidChange();

    AnimationEffectTiming timing() const { return m_timing; }

    WebAnimation* animation() const { return m_animation.get(); }
    virtual void setAnimation(WebAnimation*);

    WebAnimationTime delay();
    Seconds specifiedDelay() const { return m_timing.specifiedStartDelay; }
    void setDelay(const Seconds&);

    WebAnimationTime endDelay();
    Seconds specifiedEndDelay() const { return m_timing.specifiedEndDelay; }
    void setEndDelay(const Seconds&);

    FillMode fill() const { return m_timing.fill; }
    void setFill(FillMode);

    double iterationStart() const { return m_timing.iterationStart; }
    ExceptionOr<void> setIterationStart(double);

    double iterations() const { return m_timing.iterations; }
    ExceptionOr<void> setIterations(double);

    WebAnimationTime iterationDuration();
    std::optional<Seconds> specifiedIterationDuration() const { return m_timing.specifiedIterationDuration; }
    void setIterationDuration(const std::optional<Seconds>&);

    PlaybackDirection direction() const { return m_timing.direction; }
    void setDirection(PlaybackDirection);

    TimingFunction* timingFunction() const { return m_timing.timingFunction.get(); }
    void setTimingFunction(const RefPtr<TimingFunction>&);

    WebAnimationTime activeDuration();
    WebAnimationTime endTime();

    virtual Seconds timeToNextTick(const BasicEffectTiming&);

    virtual bool preventsAnimationReadiness() const { return false; }

protected:
    explicit AnimationEffect();

    virtual bool ticksContinuouslyWhileActive() const { return false; }
    virtual std::optional<double> progressUntilNextStep(double) const;

private:
    AnimationEffectTiming::ResolutionData resolutionData(UseCachedCurrentTime = UseCachedCurrentTime::Yes, EndpointInclusiveActiveInterval = EndpointInclusiveActiveInterval::No) const;
    void updateComputedTimingPropertiesIfNeeded();

    AnimationEffectTiming m_timing;
    WeakPtr<WebAnimation, WeakPtrImplWithEventTargetData> m_animation;
    bool m_timingDidMutate { false };
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_ANIMATION_EFFECT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::AnimationEffect& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
