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
#include "StyleOriginatedAnimation.h"

#include "Animation.h"
#include "CSSAnimation.h"
#include "CSSTransition.h"
#include "DocumentTimeline.h"
#include "Element.h"
#include "EventNames.h"
#include "KeyframeEffect.h"
#include "Logging.h"
#include "RenderStyle.h"
#include "StyleOriginatedAnimationEvent.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(StyleOriginatedAnimation);

StyleOriginatedAnimation::StyleOriginatedAnimation(const Styleable& styleable, const Animation& backingAnimation)
    : WebAnimation(styleable.element.document())
    , m_owningElement(styleable.element)
    , m_owningPseudoElementIdentifier(styleable.pseudoElementIdentifier)
    , m_backingAnimation(const_cast<Animation&>(backingAnimation))
{
}

StyleOriginatedAnimation::~StyleOriginatedAnimation() = default;

const std::optional<const Styleable> StyleOriginatedAnimation::owningElement() const
{
    if (m_owningElement)
        return Styleable(*m_owningElement, m_owningPseudoElementIdentifier);
    return std::nullopt;
}

void StyleOriginatedAnimation::tick()
{
    LOG_WITH_STREAM(Animations, stream << "StyleOriginatedAnimation::tick for element " << m_owningElement);

    bool wasRelevant = isRelevant();
    
    WebAnimation::tick();
    invalidateDOMEvents(shouldFireDOMEvents());

    // If a style-originated animation transitions from a non-idle state to an idle state, it means it was
    // canceled using the Web Animations API and it should be disassociated from its owner element.
    // From this point on, this animation is like any other animation and should not appear in the
    // maps containing running CSS Transitions and CSS Animations for a given element.
    if (wasRelevant && playState() == WebAnimation::PlayState::Idle)
        disassociateFromOwningElement();
}

bool StyleOriginatedAnimation::canHaveGlobalPosition()
{
    // https://drafts.csswg.org/css-animations-2/#animation-composite-order
    // https://drafts.csswg.org/css-transitions-2/#animation-composite-order
    // CSS Animations and CSS Transitions generated using the markup defined in this specification are not added
    // to the global animation list when they are created. Instead, these animations are appended to the global
    // animation list at the first moment when they transition out of the idle play state after being disassociated
    // from their owning element.
    return !m_owningElement && playState() != WebAnimation::PlayState::Idle;
}

void StyleOriginatedAnimation::disassociateFromOwningElement()
{
    if (!m_owningElement)
        return;

    owningElement()->removeStyleOriginatedAnimationFromListsForOwningElement(*this);
    m_owningElement = nullptr;
}

void StyleOriginatedAnimation::setBackingAnimation(const Animation& backingAnimation)
{
    m_backingAnimation = const_cast<Animation&>(backingAnimation);
    syncPropertiesWithBackingAnimation();
}

void StyleOriginatedAnimation::initialize(const RenderStyle* oldStyle, const RenderStyle& newStyle, const Style::ResolutionContext& resolutionContext)
{
    WebAnimation::initialize();

    // We need to suspend invalidation of the animation's keyframe effect during its creation
    // as it would otherwise trigger invalidation of the document's style and this would be
    // incorrect since it would happen during style invalidation.
    suspendEffectInvalidation();

    ASSERT(m_owningElement);

    Ref effect = KeyframeEffect::create(Ref { *m_owningElement }, m_owningPseudoElementIdentifier);
    setEffect(effect.copyRef());
    setTimeline(&m_owningElement->document().timeline());
    effect->computeStyleOriginatedAnimationBlendingKeyframes(oldStyle, newStyle, resolutionContext);
    syncPropertiesWithBackingAnimation();
    if (backingAnimation().playState() == AnimationPlayState::Playing)
        play();
    else
        pause();

    unsuspendEffectInvalidation();
}

void StyleOriginatedAnimation::syncPropertiesWithBackingAnimation()
{
}

std::optional<WebAnimationTime> StyleOriginatedAnimation::bindingsStartTime() const
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsStartTime();
}

std::optional<WebAnimationTime> StyleOriginatedAnimation::bindingsCurrentTime() const
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsCurrentTime();
}

WebAnimation::PlayState StyleOriginatedAnimation::bindingsPlayState() const
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsPlayState();
}

WebAnimation::ReplaceState StyleOriginatedAnimation::bindingsReplaceState() const
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsReplaceState();
}

bool StyleOriginatedAnimation::bindingsPending() const
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsPending();
}

WebAnimation::ReadyPromise& StyleOriginatedAnimation::bindingsReady()
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsReady();
}

WebAnimation::FinishedPromise& StyleOriginatedAnimation::bindingsFinished()
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsFinished();
}

ExceptionOr<void> StyleOriginatedAnimation::bindingsPlay()
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsPlay();
}

ExceptionOr<void> StyleOriginatedAnimation::bindingsPause()
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsPause();
}

void StyleOriginatedAnimation::flushPendingStyleChanges() const
{
    if (auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(effect())) {
        if (auto* target = keyframeEffect->target())
            target->document().updateStyleIfNeeded();
    }
}

void StyleOriginatedAnimation::setTimeline(RefPtr<AnimationTimeline>&& newTimeline)
{
    if (timeline() && !newTimeline)
        cancel();

    WebAnimation::setTimeline(WTFMove(newTimeline));
}

void StyleOriginatedAnimation::cancel(WebAnimation::Silently silently)
{
    WebAnimationTime cancelationTime = 0_s;

    auto shouldFireEvents = shouldFireDOMEvents();
    if (shouldFireEvents != ShouldFireEvents::No) {
        if (auto* animationEffect = effect()) {
            if (auto activeTime = animationEffect->getBasicTiming().activeTime)
                cancelationTime = *activeTime;
        }
    }

    WebAnimation::cancel(silently);

    invalidateDOMEvents(shouldFireEvents, cancelationTime);
}

void StyleOriginatedAnimation::cancelFromStyle(WebAnimation::Silently silently)
{
    cancel(silently);
    disassociateFromOwningElement();
}

AnimationEffectPhase StyleOriginatedAnimation::phaseWithoutEffect() const
{
    // This shouldn't be called if we actually have an effect.
    ASSERT(!effect());

    auto animationCurrentTime = currentTime();
    if (!animationCurrentTime)
        return AnimationEffectPhase::Idle;

    // Since we don't have an effect, the duration will be zero so the phase is 'before' if the current time is less than zero.
    return *animationCurrentTime < 0_s ? AnimationEffectPhase::Before : AnimationEffectPhase::After;
}

WebAnimationTime StyleOriginatedAnimation::effectTimeAtStart() const
{
    if (auto* effect = this->effect())
        return effect->delay();
    return 0_s;
}

WebAnimationTime StyleOriginatedAnimation::effectTimeAtIteration(double iteration) const
{
    if (auto* effect = this->effect()) {
        auto iterationDuration = effect->iterationDuration();
        // We need not account for delay with progress-based animations as the
        // Web Animations spec does not specify how to account for them.
        if (iterationDuration.percentage())
            return iterationDuration * iteration;
        return effect->delay() + iterationDuration * iteration;
    }
    return 0_s;
}

WebAnimationTime StyleOriginatedAnimation::effectTimeAtEnd() const
{
    if (auto* effect = this->effect())
        return effect->endTime();
    return 0_s;
}

auto StyleOriginatedAnimation::shouldFireDOMEvents() const -> ShouldFireEvents
{
    if (!m_owningElement)
        return ShouldFireEvents::No;

    auto& document = m_owningElement->document();
    if (is<CSSAnimation>(*this)) {
        if (document.hasListenerType(Document::ListenerType::CSSAnimation))
            return ShouldFireEvents::YesForCSSAnimation;
        return ShouldFireEvents::No;
    }
    ASSERT(is<CSSTransition>(*this));
    if (document.hasListenerType(Document::ListenerType::CSSTransition))
        return ShouldFireEvents::YesForCSSTransition;
    return ShouldFireEvents::No;
}

void StyleOriginatedAnimation::invalidateDOMEvents(ShouldFireEvents shouldFireEvents, WebAnimationTime elapsedTime)
{
    if (!m_owningElement)
        return;
    
    auto isPending = pending();
    if (isPending && m_wasPending)
        return;

    double iteration = 0;
    AnimationEffectPhase currentPhase;
    WebAnimationTime intervalStart;
    WebAnimationTime intervalEnd;

    auto* animationEffect = effect();
    if (animationEffect) {
        auto timing = animationEffect->getComputedTiming();
        if (auto computedIteration = timing.currentIteration)
            iteration = *computedIteration;
        currentPhase = timing.phase;
        if (timing.activeDuration.percentage()) {
            // We need not account for delay with progress-based animations as the
            // Web Animations spec does not specify how to account for them.
            auto zero = timing.activeDuration.matchingZero();
            intervalStart = std::max(zero, timing.activeDuration);
            intervalEnd = std::max(zero, std::min(timing.endTime, timing.activeDuration));
        } else {
            auto activeDuration = timing.activeDuration.time()->milliseconds();
            intervalStart = std::max(0_s, Seconds::fromMilliseconds(std::min(-timing.delay, activeDuration)));
            intervalEnd = std::max(0_s, Seconds::fromMilliseconds(std::min(timing.endTime.time()->milliseconds() - timing.delay, activeDuration)));
        }
    } else {
        iteration = 0;
        currentPhase = phaseWithoutEffect();
        intervalStart = 0_s;
        intervalEnd = 0_s;
    }

    bool wasActive = m_previousPhase == AnimationEffectPhase::Active;
    bool wasAfter = m_previousPhase == AnimationEffectPhase::After;
    bool wasBefore = m_previousPhase == AnimationEffectPhase::Before;
    bool wasIdle = m_previousPhase == AnimationEffectPhase::Idle;

    bool isActive = currentPhase == AnimationEffectPhase::Active;
    bool isAfter = currentPhase == AnimationEffectPhase::After;
    bool isBefore = currentPhase == AnimationEffectPhase::Before;
    bool isIdle = currentPhase == AnimationEffectPhase::Idle;

    switch (shouldFireEvents) {
    case ShouldFireEvents::YesForCSSAnimation:
        // https://drafts.csswg.org/css-animations-2/#events
        if ((wasIdle || wasBefore) && isActive)
            enqueueDOMEvent(eventNames().animationstartEvent, intervalStart, effectTimeAtStart());
        else if ((wasIdle || wasBefore) && isAfter) {
            enqueueDOMEvent(eventNames().animationstartEvent, intervalStart, effectTimeAtStart());
            enqueueDOMEvent(eventNames().animationendEvent, intervalEnd, effectTimeAtEnd());
        } else if (wasActive && isBefore)
            enqueueDOMEvent(eventNames().animationendEvent, intervalStart, effectTimeAtEnd());
        else if (wasActive && isActive && m_previousIteration != iteration) {
            auto iterationBoundary = iteration;
            if (m_previousIteration > iteration)
                iterationBoundary++;
            auto elapsedTime = animationEffect ? animationEffect->iterationDuration() * (iterationBoundary - animationEffect->iterationStart()) : zeroTime();
            enqueueDOMEvent(eventNames().animationiterationEvent, elapsedTime, effectTimeAtIteration(iteration));
        } else if (wasActive && isAfter)
            enqueueDOMEvent(eventNames().animationendEvent, intervalEnd, effectTimeAtEnd());
        else if (wasAfter && isActive)
            enqueueDOMEvent(eventNames().animationstartEvent, intervalEnd, effectTimeAtStart());
        else if (wasAfter && isBefore) {
            enqueueDOMEvent(eventNames().animationstartEvent, intervalEnd, effectTimeAtStart());
            enqueueDOMEvent(eventNames().animationendEvent, intervalStart, effectTimeAtEnd());
        } else if ((!wasIdle && !wasAfter) && isIdle)
            enqueueDOMEvent(eventNames().animationcancelEvent, elapsedTime, elapsedTime);
        break;
    case ShouldFireEvents::YesForCSSTransition:
        // https://drafts.csswg.org/css-transitions-2/#transition-events
        if (wasIdle && (isPending || isBefore))
            enqueueDOMEvent(eventNames().transitionrunEvent, intervalStart, effectTimeAtStart());
        else if (wasIdle && isActive) {
            auto scheduledEffectTime = effectTimeAtStart();
            enqueueDOMEvent(eventNames().transitionrunEvent, intervalStart, scheduledEffectTime);
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalStart, scheduledEffectTime);
        } else if (wasIdle && isAfter) {
            enqueueDOMEvent(eventNames().transitionrunEvent, intervalStart, effectTimeAtStart());
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalStart, effectTimeAtStart());
            enqueueDOMEvent(eventNames().transitionendEvent, intervalEnd, effectTimeAtEnd());
        } else if ((m_wasPending || wasBefore) && isActive)
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalStart, effectTimeAtStart());
        else if ((m_wasPending || wasBefore) && isAfter) {
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalStart, effectTimeAtStart());
            enqueueDOMEvent(eventNames().transitionendEvent, intervalEnd, effectTimeAtEnd());
        } else if (wasActive && isAfter)
            enqueueDOMEvent(eventNames().transitionendEvent, intervalEnd, effectTimeAtEnd());
        else if (wasActive && isBefore)
            enqueueDOMEvent(eventNames().transitionendEvent, intervalStart, effectTimeAtEnd());
        else if (wasAfter && isActive)
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalEnd, effectTimeAtStart());
        else if (wasAfter && isBefore) {
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalEnd, effectTimeAtStart());
            enqueueDOMEvent(eventNames().transitionendEvent, intervalStart, effectTimeAtEnd());
        } else if ((!wasIdle && !wasAfter) && isIdle)
            enqueueDOMEvent(eventNames().transitioncancelEvent, elapsedTime, elapsedTime);
        break;
    case ShouldFireEvents::No:
        break;
    }

    m_wasPending = isPending;
    m_previousPhase = currentPhase;
    m_previousIteration = iteration;
}

void StyleOriginatedAnimation::enqueueDOMEvent(const AtomString& eventType, WebAnimationTime elapsedTime, WebAnimationTime scheduledEffectTime)
{
    if (!m_owningElement)
        return;

    auto scheduledTimelineTime = [&]() -> std::optional<Seconds> {
        if (auto* documentTimeline = dynamicDowncast<DocumentTimeline>(timeline())) {
            ASSERT(scheduledEffectTime.time());
            if (auto scheduledAnimationTime = convertAnimationTimeToTimelineTime(*scheduledEffectTime.time()))
                return documentTimeline->convertTimelineTimeToOriginRelativeTime(*scheduledAnimationTime);
        }
        return std::nullopt;
    }();

    auto time = [&]() {
        if (auto seconds = elapsedTime.time())
            return secondsToWebAnimationsAPITime(*seconds) / 1000;
        return 0.0;
    };
    auto event = createEvent(eventType, scheduledTimelineTime, time(), m_owningPseudoElementIdentifier);
    event->setTarget(RefPtr { m_owningElement.get() });
    enqueueAnimationEvent(WTFMove(event));
}

} // namespace WebCore
