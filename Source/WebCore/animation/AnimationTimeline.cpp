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

#include "AnimationTimelinesController.h"
#include "KeyframeEffect.h"
#include "KeyframeEffectStack.h"
#include "StyleResolver.h"
#include "Styleable.h"
#include "WebAnimationUtilities.h"

namespace WebCore {

AnimationTimeline::AnimationTimeline(std::optional<WebAnimationTime> duration)
    : m_duration(duration)
{
}

AnimationTimeline::~AnimationTimeline() = default;

void AnimationTimeline::animationTimingDidChange(WebAnimation& animation)
{
    updateGlobalPosition(animation);

    if (m_animations.add(animation)) {
        auto* timeline = animation.timeline();
        if (timeline && timeline != this)
            timeline->removeAnimation(animation);
        else if (timeline == this) {
            if (auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(animation.effect())) {
                if (auto styleable = keyframeEffect->targetStyleable())
                    styleable->animationWasAdded(animation);
            }
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

void AnimationTimeline::detachFromDocument()
{
    if (CheckedPtr controller = this->controller())
        controller->removeTimeline(*this);

    auto& animationsToRemove = m_animations;
    while (!animationsToRemove.isEmpty())
        animationsToRemove.first()->remove();
}

void AnimationTimeline::suspendAnimations()
{
    for (const auto& animation : m_animations)
        animation->setSuspended(true);
}

void AnimationTimeline::resumeAnimations()
{
    for (const auto& animation : m_animations)
        animation->setSuspended(false);
}

bool AnimationTimeline::animationsAreSuspended() const
{
    return controller() && controller()->animationsAreSuspended();
}

} // namespace WebCore
