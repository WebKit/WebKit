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

#include "ComputedEffectTiming.h"
#include "WebAnimation.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AnimationEffectTimingReadOnly;

class AnimationEffect : public RefCounted<AnimationEffect> {
public:
    virtual ~AnimationEffect();

    virtual bool isKeyframeEffect() const { return false; }
    AnimationEffectTimingReadOnly* timing() const { return m_timing.get(); }
    ComputedEffectTiming getComputedTiming();
    virtual void apply(RenderStyle&) = 0;
    virtual void invalidate() = 0;
    virtual void animationDidSeek() = 0;
    virtual void animationSuspensionStateDidChange(bool) = 0;

    WebAnimation* animation() const { return m_animation.get(); }
    void setAnimation(WebAnimation* animation) { m_animation = makeWeakPtr(animation); }

    std::optional<Seconds> localTime() const;
    std::optional<Seconds> activeTime() const;
    std::optional<double> iterationProgress() const;
    std::optional<double> currentIteration() const;

    enum class Phase { Before, Active, After, Idle };
    Phase phase() const;

    void timingDidChange();

protected:
    explicit AnimationEffect(Ref<AnimationEffectTimingReadOnly>&&);

private:
    enum class ComputedDirection { Forwards, Reverse };

    std::optional<double> overallProgress() const;
    std::optional<double> simpleIterationProgress() const;
    AnimationEffect::ComputedDirection currentDirection() const;
    std::optional<double> directedProgress() const;
    std::optional<double> transformedProgress() const;

    WeakPtr<WebAnimation> m_animation;
    RefPtr<AnimationEffectTimingReadOnly> m_timing;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_ANIMATION_EFFECT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::AnimationEffect& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
