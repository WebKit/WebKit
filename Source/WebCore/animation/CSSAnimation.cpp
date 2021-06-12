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
#include "AnimationEvent.h"
#include "InspectorInstrumentation.h"
#include "RenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSAnimation);

Ref<CSSAnimation> CSSAnimation::create(const Styleable& owningElement, const Animation& backingAnimation, const RenderStyle* oldStyle, const RenderStyle& newStyle, const RenderStyle* parentElementStyle)
{
    auto result = adoptRef(*new CSSAnimation(owningElement, backingAnimation));
    result->initialize(oldStyle, newStyle, parentElementStyle);

    InspectorInstrumentation::didCreateWebAnimation(result.get());

    return result;
}

CSSAnimation::CSSAnimation(const Styleable& element, const Animation& backingAnimation)
    : DeclarativeAnimation(element, backingAnimation)
    , m_animationName(backingAnimation.name().string)
{
}

void CSSAnimation::syncPropertiesWithBackingAnimation()
{
    DeclarativeAnimation::syncPropertiesWithBackingAnimation();

    if (!effect())
        return;

    suspendEffectInvalidation();

    auto& animation = backingAnimation();
    auto* animationEffect = effect();

    if (!m_overriddenProperties.contains(Property::FillMode)) {
        switch (animation.fillMode()) {
        case AnimationFillMode::None:
            animationEffect->setFill(FillMode::None);
            break;
        case AnimationFillMode::Backwards:
            animationEffect->setFill(FillMode::Backwards);
            break;
        case AnimationFillMode::Forwards:
            animationEffect->setFill(FillMode::Forwards);
            break;
        case AnimationFillMode::Both:
            animationEffect->setFill(FillMode::Both);
            break;
        }
    }

    if (!m_overriddenProperties.contains(Property::Direction)) {
        switch (animation.direction()) {
        case Animation::AnimationDirectionNormal:
            animationEffect->setDirection(PlaybackDirection::Normal);
            break;
        case Animation::AnimationDirectionAlternate:
            animationEffect->setDirection(PlaybackDirection::Alternate);
            break;
        case Animation::AnimationDirectionReverse:
            animationEffect->setDirection(PlaybackDirection::Reverse);
            break;
        case Animation::AnimationDirectionAlternateReverse:
            animationEffect->setDirection(PlaybackDirection::AlternateReverse);
            break;
        }
    }

    if (!m_overriddenProperties.contains(Property::IterationCount)) {
        auto iterationCount = animation.iterationCount();
        animationEffect->setIterations(iterationCount == Animation::IterationCountInfinite ? std::numeric_limits<double>::infinity() : iterationCount);
    }

    if (!m_overriddenProperties.contains(Property::Delay))
        animationEffect->setDelay(Seconds(animation.delay()));

    if (!m_overriddenProperties.contains(Property::Duration))
        animationEffect->setIterationDuration(Seconds(animation.duration()));

    animationEffect->updateStaticTimingProperties();
    effectTimingDidChange();

    // Synchronize the play state
    if (!m_overriddenProperties.contains(Property::PlayState)) {
        if (animation.playState() == AnimationPlayState::Playing && playState() == WebAnimation::PlayState::Paused)
            play();
        else if (animation.playState() == AnimationPlayState::Paused && playState() == WebAnimation::PlayState::Running)
            pause();
    }

    unsuspendEffectInvalidation();
}

ExceptionOr<void> CSSAnimation::bindingsPlay()
{
    // https://drafts.csswg.org/css-animations-2/#animations

    // After a successful call to play() or pause() on a CSSAnimation, any subsequent change to the animation-play-state will
    // no longer cause the CSSAnimation to be played or paused.

    auto retVal = DeclarativeAnimation::bindingsPlay();
    if (!retVal.hasException())
        m_overriddenProperties.add(Property::PlayState);
    return retVal;
}

ExceptionOr<void> CSSAnimation::bindingsPause()
{
    // https://drafts.csswg.org/css-animations-2/#animations

    // After a successful call to play() or pause() on a CSSAnimation, any subsequent change to the animation-play-state will
    // no longer cause the CSSAnimation to be played or paused.

    auto retVal = DeclarativeAnimation::bindingsPause();
    if (!retVal.hasException())
        m_overriddenProperties.add(Property::PlayState);
    return retVal;
}

void CSSAnimation::setBindingsEffect(RefPtr<AnimationEffect>&& newEffect)
{
    // https://drafts.csswg.org/css-animations-2/#animations

    // After successfully setting the effect of a CSSAnimation to null or some AnimationEffect other than the original KeyframeEffect,
    // all subsequent changes to animation properties other than animation-name or animation-play-state will not be reflected in that
    // animation. Similarly, any change to matching @keyframes rules will not be reflected in that animation. However, if the last
    // matching @keyframes rule is removed the animation must still be canceled.

    auto* previousEffect = effect();
    DeclarativeAnimation::setBindingsEffect(WTFMove(newEffect));
    if (effect() != previousEffect) {
        m_overriddenProperties.add(Property::Duration);
        m_overriddenProperties.add(Property::TimingFunction);
        m_overriddenProperties.add(Property::IterationCount);
        m_overriddenProperties.add(Property::Direction);
        m_overriddenProperties.add(Property::Delay);
        m_overriddenProperties.add(Property::FillMode);
    }
}

void CSSAnimation::setBindingsStartTime(std::optional<double> startTime)
{
    // https://drafts.csswg.org/css-animations-2/#animations

    // After a successful call to reverse() on a CSSAnimation or after successfully setting the startTime on a CSSAnimation,
    // if, as a result of that call the play state of the CSSAnimation changes to or from the paused play state, any subsequent
    // change to the animation-play-state will no longer cause the CSSAnimation to be played or paused.

    auto previousPlayState = playState();
    DeclarativeAnimation::setBindingsStartTime(startTime);
    auto currentPlayState = playState();
    if (currentPlayState != previousPlayState && (currentPlayState == PlayState::Paused || previousPlayState == PlayState::Paused))
        m_overriddenProperties.add(Property::PlayState);
}

ExceptionOr<void> CSSAnimation::bindingsReverse()
{
    // https://drafts.csswg.org/css-animations-2/#animations

    // After a successful call to reverse() on a CSSAnimation or after successfully setting the startTime on a CSSAnimation,
    // if, as a result of that call the play state of the CSSAnimation changes to or from the paused play state, any subsequent
    // change to the animation-play-state will no longer cause the CSSAnimation to be played or paused.

    auto previousPlayState = playState();
    auto retVal = DeclarativeAnimation::bindingsReverse();
    if (!retVal.hasException()) {
        auto currentPlayState = playState();
        if (currentPlayState != previousPlayState && (currentPlayState == PlayState::Paused || previousPlayState == PlayState::Paused))
            m_overriddenProperties.add(Property::PlayState);
    }
    return retVal;
}

void CSSAnimation::effectTimingWasUpdatedUsingBindings(OptionalEffectTiming timing)
{
    // https://drafts.csswg.org/css-animations-2/#animations

    // After a successful call to updateTiming() on the KeyframeEffect associated with a CSSAnimation, for each property
    // included in the timing parameter, any subsequent change to a corresponding animation property will not be reflected
    // in that animation.

    if (timing.duration)
        m_overriddenProperties.add(Property::Duration);

    if (timing.iterations)
        m_overriddenProperties.add(Property::IterationCount);

    if (timing.delay)
        m_overriddenProperties.add(Property::Delay);

    if (!timing.easing.isNull())
        m_overriddenProperties.add(Property::TimingFunction);

    if (timing.fill)
        m_overriddenProperties.add(Property::FillMode);

    if (timing.direction)
        m_overriddenProperties.add(Property::Direction);
}

void CSSAnimation::effectKeyframesWereSetUsingBindings()
{
    // https://drafts.csswg.org/css-animations-2/#animations

    // After a successful call to setKeyframes() on the KeyframeEffect associated with a CSSAnimation, any subsequent change to
    // matching @keyframes rules or the resolved value of the animation-timing-function property for the target element will not
    // be reflected in that animation.
    m_overriddenProperties.add(Property::TimingFunction);
}

Ref<AnimationEventBase> CSSAnimation::createEvent(const AtomString& eventType, double elapsedTime, const String& pseudoId, std::optional<Seconds> timelineTime)
{
    return AnimationEvent::create(eventType, m_animationName, elapsedTime, pseudoId, timelineTime, this);
}

} // namespace WebCore
