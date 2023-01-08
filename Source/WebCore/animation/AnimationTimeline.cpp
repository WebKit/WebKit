/*
 * Copyright (C) Canon Inc. 2016
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "AnimationTimeline.h"

#include "KeyframeEffect.h"
#include "KeyframeEffectStack.h"
#include "StyleResolver.h"
#include "Styleable.h"
#include "WebAnimationUtilities.h"

namespace WebCore {

AnimationTimeline::AnimationTimeline() = default;
AnimationTimeline::~AnimationTimeline() = default;

void AnimationTimeline::forgetAnimation(WebAnimation* animation)
{
    m_allAnimations.removeFirst(animation);
}

void AnimationTimeline::animationTimingDidChange(WebAnimation& animation)
{
    updateGlobalPosition(animation);

    if (m_animations.add(animation)) {
        m_allAnimations.append(animation);
        auto* timeline = animation.timeline();
        if (timeline && timeline != this)
            timeline->removeAnimation(animation);
        else if (timeline == this && is<KeyframeEffect>(animation.effect())) {
            if (auto styleable = downcast<KeyframeEffect>(animation.effect())->targetStyleable())
                styleable->animationWasAdded(animation);
        }
    }
}

void AnimationTimeline::updateGlobalPosition(WebAnimation& animation)
{
    static uint64_t s_globalPosition = 0;
    if (!animation.globalPosition() && animation.canHaveGlobalPosition())
        animation.setGlobalPosition(++s_globalPosition);
}

void AnimationTimeline::removeAnimation(WebAnimation& animation)
{
    ASSERT(!animation.timeline() || animation.timeline() == this);
    m_animations.remove(animation);
    if (auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(animation.effect())) {
        if (auto styleable = keyframeEffect->targetStyleable()) {
            styleable->animationWasRemoved(animation);
            styleable->ensureKeyframeEffectStack().removeEffect(*keyframeEffect);
        }
    }
}

std::optional<double> AnimationTimeline::bindingsCurrentTime()
{
    auto time = currentTime();
    if (!time)
        return std::nullopt;
    return secondsToWebAnimationsAPITime(*time);
}

} // namespace WebCore
