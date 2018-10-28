/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "CSSAnimation.h"

#include "Animation.h"
#include "Element.h"

namespace WebCore {

Ref<CSSAnimation> CSSAnimation::create(Element& target, const Animation& backingAnimation, const RenderStyle* oldStyle, const RenderStyle& newStyle)
{
    auto result = adoptRef(*new CSSAnimation(target, backingAnimation, newStyle));
    result->initialize(target, oldStyle, newStyle);
    return result;
}

CSSAnimation::CSSAnimation(Element& element, const Animation& backingAnimation, const RenderStyle& unanimatedStyle)
    : DeclarativeAnimation(element, backingAnimation)
    , m_animationName(backingAnimation.name())
    , m_unanimatedStyle(RenderStyle::clonePtr(unanimatedStyle))
{
}

void CSSAnimation::syncPropertiesWithBackingAnimation()
{
    DeclarativeAnimation::syncPropertiesWithBackingAnimation();

    if (!effect())
        return;

    suspendEffectInvalidation();

    auto& animation = backingAnimation();
    auto* timing = effect()->timing();

    switch (animation.fillMode()) {
    case AnimationFillMode::None:
        timing->setFill(FillMode::None);
        break;
    case AnimationFillMode::Backwards:
        timing->setFill(FillMode::Backwards);
        break;
    case AnimationFillMode::Forwards:
        timing->setFill(FillMode::Forwards);
        break;
    case AnimationFillMode::Both:
        timing->setFill(FillMode::Both);
        break;
    }

    switch (animation.direction()) {
    case Animation::AnimationDirectionNormal:
        timing->setDirection(PlaybackDirection::Normal);
        break;
    case Animation::AnimationDirectionAlternate:
        timing->setDirection(PlaybackDirection::Alternate);
        break;
    case Animation::AnimationDirectionReverse:
        timing->setDirection(PlaybackDirection::Reverse);
        break;
    case Animation::AnimationDirectionAlternateReverse:
        timing->setDirection(PlaybackDirection::AlternateReverse);
        break;
    }

    auto iterationCount = animation.iterationCount();
    timing->setIterations(iterationCount == Animation::IterationCountInfinite ? std::numeric_limits<double>::infinity() : iterationCount);

    timing->setDelay(Seconds(animation.delay()));
    timing->setIterationDuration(Seconds(animation.duration()));

    // Synchronize the play state
    if (animation.playState() == AnimationPlayState::Playing && playState() == WebAnimation::PlayState::Paused) {
        if (!m_stickyPaused)
            play();
    } else if (animation.playState() == AnimationPlayState::Paused && playState() == WebAnimation::PlayState::Running)
        pause();

    unsuspendEffectInvalidation();
}

ExceptionOr<void> CSSAnimation::bindingsPlay()
{
    m_stickyPaused = false;
    return DeclarativeAnimation::bindingsPlay();
}

ExceptionOr<void> CSSAnimation::bindingsPause()
{
    m_stickyPaused = true;
    return DeclarativeAnimation::bindingsPause();
}

} // namespace WebCore
