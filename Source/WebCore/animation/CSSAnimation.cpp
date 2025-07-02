/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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

#include "AnimationEffect.h"
#include "CSSAnimationEvent.h"
#include "DocumentTimeline.h"
#include "InspectorInstrumentation.h"
#include "KeyframeEffect.h"
#include "RenderStyle.h"
#include "StyleOriginatedTimelinesController.h"
#include "ViewTimeline.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSAnimation);

Ref<CSSAnimation> CSSAnimation::create(const Styleable& owningElement, const Animation& backingAnimation, const RenderStyle* oldStyle, const RenderStyle& newStyle, const Style::ResolutionContext& resolutionContext)
{
    auto result = adoptRef(*new CSSAnimation(owningElement, backingAnimation));
    result->initialize(oldStyle, newStyle, resolutionContext);

    InspectorInstrumentation::didCreateWebAnimation(result.get());

    return result;
}

CSSAnimation::CSSAnimation(const Styleable& element, const Animation& backingAnimation)
    : StyleOriginatedAnimation(element, backingAnimation)
    , m_animationName(backingAnimation.name().name)
{
}

void CSSAnimation::syncPropertiesWithBackingAnimation()
{
    StyleOriginatedAnimation::syncPropertiesWithBackingAnimation();

    // If we have been disassociated from our original owning element,
    // we should no longer sync any of the `animation-*` CSS properties.
    if (!owningElement())
        return;

    if (!effect())
        return;

    suspendEffectInvalidation();

    // https://drafts.csswg.org/css-animations-2/#animation-timeline
    // When multiple animation-* properties are set simultaneously, animation-timeline
    // is updated first, so e.g. a change to animation-play-state applies to the
    // simultaneously-applied timeline specified in animation-timeline.
    syncStyleOriginatedTimeline();

    Ref animation = backingAnimation();
    RefPtr animationEffect = effect();

    if (!m_overriddenProperties.contains(Property::FillMode)) {
        switch (animation->fillMode()) {
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
        switch (animation->direction()) {
        case Animation::Direction::Normal:
            animationEffect->setDirection(PlaybackDirection::Normal);
            break;
        case Animation::Direction::Alternate:
            animationEffect->setDirection(PlaybackDirection::Alternate);
            break;
        case Animation::Direction::Reverse:
            animationEffect->setDirection(PlaybackDirection::Reverse);
            break;
        case Animation::Direction::AlternateReverse:
            animationEffect->setDirection(PlaybackDirection::AlternateReverse);
            break;
        }
    }

    if (!m_overriddenProperties.contains(Property::IterationCount)) {
        auto iterationCount = animation->iterationCount();
        animationEffect->setIterations(iterationCount == Animation::IterationCountInfinite ? std::numeric_limits<double>::infinity() : iterationCount);
    }

    if (!m_overriddenProperties.contains(Property::Delay))
        animationEffect->setDelay(Seconds(animation->delay()));

    if (!m_overriddenProperties.contains(Property::Duration)) {
        if (auto duration = animation->duration())
            animationEffect->setIterationDuration(Seconds(*duration));
        else
            animationEffect->setIterationDuration(std::nullopt);
    }

    if (!m_overriddenProperties.contains(Property::CompositeOperation)) {
        if (auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(animationEffect.get()))
            keyframeEffect->setComposite(animation->compositeOperation());
    }

    if (!m_overriddenProperties.contains(Property::RangeStart))
        setRangeStart(animation->range().start);
    if (!m_overriddenProperties.contains(Property::RangeEnd))
        setRangeEnd(animation->range().end);

    effectTimingDidChange();

    // Synchronize the play state
    if (!m_overriddenProperties.contains(Property::PlayState)) {
        auto styleOriginatedPlayState = animation->playState();
        if (m_lastStyleOriginatedPlayState != styleOriginatedPlayState) {
            if (styleOriginatedPlayState == AnimationPlayState::Playing && playState() == WebAnimation::PlayState::Paused)
                play();
            else if (styleOriginatedPlayState == AnimationPlayState::Paused && playState() == WebAnimation::PlayState::Running)
                pause();
        }
        m_lastStyleOriginatedPlayState = styleOriginatedPlayState;
    }

    unsuspendEffectInvalidation();
}

void CSSAnimation::syncStyleOriginatedTimeline()
{
    if (m_overriddenProperties.contains(Property::Timeline) || !effect())
        return;

    suspendEffectInvalidation();

    ASSERT(owningElement());
    Ref document = owningElement()->element.document();
    auto& timeline = backingAnimation().timeline();
    WTF::switchOn(timeline,
        [&] (Animation::TimelineKeyword keyword) {
            setTimeline(keyword == Animation::TimelineKeyword::None ? nullptr : RefPtr { document->existingTimeline() });
        }, [&] (const AtomString&) {
            CheckedRef styleOriginatedTimelinesController = document->ensureStyleOriginatedTimelinesController();
            styleOriginatedTimelinesController->attachAnimation(*this);
        }, [&] (const Animation::AnonymousScrollTimeline& anonymousScrollTimeline) {
            auto scrollTimeline = ScrollTimeline::create(anonymousScrollTimeline.scroller, anonymousScrollTimeline.axis);
            scrollTimeline->setSource(*owningElement());
            setTimeline(WTFMove(scrollTimeline));
        }, [&] (const Animation::AnonymousViewTimeline& anonymousViewTimeline) {
            auto viewTimeline = ViewTimeline::create(nullAtom(), anonymousViewTimeline.axis, anonymousViewTimeline.insets);
            viewTimeline->setSubject(*owningElement());
            setTimeline(WTFMove(viewTimeline));
        }
    );

    // If we're not dealing with a named timeline, we should make sure we have no
    // pending attachment operation for this timeline name.
    if (!std::holds_alternative<AtomString>(timeline)) {
        CheckedRef styleOriginatedTimelinesController = document->ensureStyleOriginatedTimelinesController();
        styleOriginatedTimelinesController->removePendingOperationsForCSSAnimation(*this);
    }

    unsuspendEffectInvalidation();
}

AnimationTimeline* CSSAnimation::bindingsTimeline() const
{
    flushPendingStyleChanges();
    return StyleOriginatedAnimation::bindingsTimeline();
}

void CSSAnimation::setBindingsTimeline(RefPtr<AnimationTimeline>&& timeline)
{
    m_overriddenProperties.add(Property::Timeline);
    StyleOriginatedAnimation::setBindingsTimeline(WTFMove(timeline));
}

void CSSAnimation::setBindingsRangeStart(TimelineRangeValue&& range)
{
    m_overriddenProperties.add(Property::RangeStart);
    StyleOriginatedAnimation::setBindingsRangeStart(WTFMove(range));
}

void CSSAnimation::setBindingsRangeEnd(TimelineRangeValue&& range)
{
    m_overriddenProperties.add(Property::RangeEnd);
    StyleOriginatedAnimation::setBindingsRangeEnd(WTFMove(range));
}

ExceptionOr<void> CSSAnimation::bindingsPlay()
{
    // https://drafts.csswg.org/css-animations-2/#animations

    // After a successful call to play() or pause() on a CSSAnimation, any subsequent change to the animation-play-state will
    // no longer cause the CSSAnimation to be played or paused.

    auto retVal = StyleOriginatedAnimation::bindingsPlay();
    if (!retVal.hasException())
        m_overriddenProperties.add(Property::PlayState);
    return retVal;
}

ExceptionOr<void> CSSAnimation::bindingsPause()
{
    // https://drafts.csswg.org/css-animations-2/#animations

    // After a successful call to play() or pause() on a CSSAnimation, any subsequent change to the animation-play-state will
    // no longer cause the CSSAnimation to be played or paused.

    auto retVal = StyleOriginatedAnimation::bindingsPause();
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

    RefPtr previousEffect = effect();
    StyleOriginatedAnimation::setBindingsEffect(WTFMove(newEffect));
    if (effect() != previousEffect.get()) {
        m_overriddenProperties.add(Property::Duration);
        m_overriddenProperties.add(Property::TimingFunction);
        m_overriddenProperties.add(Property::IterationCount);
        m_overriddenProperties.add(Property::Direction);
        m_overriddenProperties.add(Property::Delay);
        m_overriddenProperties.add(Property::FillMode);
        m_overriddenProperties.add(Property::Keyframes);
        m_overriddenProperties.add(Property::CompositeOperation);
    }
}

ExceptionOr<void> CSSAnimation::setBindingsStartTime(const std::optional<WebAnimationTime>& startTime)
{
    // https://drafts.csswg.org/css-animations-2/#animations

    // After a successful call to reverse() on a CSSAnimation or after successfully setting the startTime on a CSSAnimation,
    // if, as a result of that call the play state of the CSSAnimation changes to or from the paused play state, any subsequent
    // change to the animation-play-state will no longer cause the CSSAnimation to be played or paused.

    auto previousPlayState = playState();
    auto result = StyleOriginatedAnimation::setBindingsStartTime(startTime);
    if (result.hasException())
        return result.releaseException();
    auto currentPlayState = playState();
    if (currentPlayState != previousPlayState && (currentPlayState == PlayState::Paused || previousPlayState == PlayState::Paused))
        m_overriddenProperties.add(Property::PlayState);

    return { };
}

ExceptionOr<void> CSSAnimation::bindingsReverse()
{
    // https://drafts.csswg.org/css-animations-2/#animations

    // After a successful call to reverse() on a CSSAnimation or after successfully setting the startTime on a CSSAnimation,
    // if, as a result of that call the play state of the CSSAnimation changes to or from the paused play state, any subsequent
    // change to the animation-play-state will no longer cause the CSSAnimation to be played or paused.

    auto previousPlayState = playState();
    auto retVal = StyleOriginatedAnimation::bindingsReverse();
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
    m_overriddenProperties.add(Property::Keyframes);
    m_overriddenProperties.add(Property::TimingFunction);
}

void CSSAnimation::effectCompositeOperationWasSetUsingBindings()
{
    m_overriddenProperties.add(Property::CompositeOperation);
}

void CSSAnimation::keyframesRuleDidChange()
{
    if (m_overriddenProperties.contains(Property::Keyframes))
        return;

    RefPtr keyframeEffect = dynamicDowncast<KeyframeEffect>(effect());
    if (!keyframeEffect)
        return;

    auto owningElement = this->owningElement();
    if (!owningElement)
        return;

    keyframeEffect->keyframesRuleDidChange();
    owningElement->keyframesRuleDidChange();
}

void CSSAnimation::updateKeyframesIfNeeded(const RenderStyle* oldStyle, const RenderStyle& newStyle, const Style::ResolutionContext& resolutionContext)
{
    if (m_overriddenProperties.contains(Property::Keyframes))
        return;

    RefPtr keyframeEffect = dynamicDowncast<KeyframeEffect>(effect());
    if (!keyframeEffect)
        return;

    if (keyframeEffect->blendingKeyframes().isEmpty())
        keyframeEffect->computeStyleOriginatedAnimationBlendingKeyframes(oldStyle, newStyle, resolutionContext);
}

Ref<StyleOriginatedAnimationEvent> CSSAnimation::createEvent(const AtomString& eventType, std::optional<Seconds> scheduledTime, double elapsedTime, const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    return CSSAnimationEvent::create(eventType, this, scheduledTime, elapsedTime, pseudoElementIdentifier, m_animationName);
}

} // namespace WebCore
