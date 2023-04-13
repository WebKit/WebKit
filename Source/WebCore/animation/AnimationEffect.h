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

#include "AnimationEffect.h"
#include "AnimationEffectPhase.h"
#include "BasicEffectTiming.h"
#include "ComputedEffectTiming.h"
#include "ExceptionOr.h"
#include "FillMode.h"
#include "KeyframeEffectOptions.h"
#include "OptionalEffectTiming.h"
#include "PlaybackDirection.h"
#include "TimingFunction.h"
#include "WebAnimation.h"
#include "WebAnimationUtilities.h"
#include <variant>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Seconds.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AnimationEffect : public RefCounted<AnimationEffect>, public CanMakeWeakPtr<AnimationEffect> {
    WTF_MAKE_ISO_ALLOCATED(AnimationEffect);
public:
    virtual ~AnimationEffect();

    virtual bool isCustomEffect() const { return false; }
    virtual bool isKeyframeEffect() const { return false; }

    EffectTiming getBindingsTiming() const;
    EffectTiming getTiming() const;
    BasicEffectTiming getBasicTiming(std::optional<Seconds> = std::nullopt) const;
    ComputedEffectTiming getBindingsComputedTiming() const;
    ComputedEffectTiming getComputedTiming(std::optional<Seconds> = std::nullopt) const;
    ExceptionOr<void> bindingsUpdateTiming(std::optional<OptionalEffectTiming>);
    ExceptionOr<void> updateTiming(std::optional<OptionalEffectTiming>);

    virtual void animationDidTick() { };
    virtual void animationDidChangeTimingProperties() { };
    virtual void animationWasCanceled() { };
    virtual void animationSuspensionStateDidChange(bool) { };
    virtual void animationTimelineDidChange(AnimationTimeline*) { };
    virtual void animationDidFinish() { };

    WebAnimation* animation() const { return m_animation.get(); }
    virtual void setAnimation(WebAnimation*);

    Seconds delay() const { return m_delay; }
    void setDelay(const Seconds&);

    Seconds endDelay() const { return m_endDelay; }
    void setEndDelay(const Seconds&);

    FillMode fill() const { return m_fill; }
    void setFill(FillMode);

    double iterationStart() const { return m_iterationStart; }
    ExceptionOr<void> setIterationStart(double);

    double iterations() const { return m_iterations; }
    ExceptionOr<void> setIterations(double);

    Seconds iterationDuration() const { return m_iterationDuration; }
    void setIterationDuration(const Seconds&);

    PlaybackDirection direction() const { return m_direction; }
    void setDirection(PlaybackDirection);

    TimingFunction* timingFunction() const { return m_timingFunction.get(); }
    void setTimingFunction(const RefPtr<TimingFunction>&);

    Seconds activeDuration() const { return m_activeDuration; }
    Seconds endTime() const { return m_endTime; }

    void updateStaticTimingProperties();

    virtual Seconds timeToNextTick(const BasicEffectTiming&) const;

    virtual bool preventsAnimationReadiness() const { return false; }

protected:
    explicit AnimationEffect();

    virtual bool ticksContinouslyWhileActive() const { return false; }
    virtual std::optional<double> progressUntilNextStep(double) const;

private:
    enum class ComputedDirection : uint8_t { Forwards, Reverse };

    FillMode m_fill { FillMode::Auto };
    PlaybackDirection m_direction { PlaybackDirection::Normal };

    WeakPtr<WebAnimation, WeakPtrImplWithEventTargetData> m_animation;
    RefPtr<TimingFunction> m_timingFunction;

    double m_iterationStart { 0 };
    double m_iterations { 1 };

    Seconds m_delay { 0_s };
    Seconds m_endDelay { 0_s };
    Seconds m_iterationDuration { 0_s };
    Seconds m_activeDuration { 0_s };
    Seconds m_endTime { 0_s };
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_ANIMATION_EFFECT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::AnimationEffect& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
