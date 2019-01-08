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
#include "DeclarativeAnimation.h"

#include "Animation.h"
#include "AnimationEvent.h"
#include "Element.h"
#include "EventNames.h"
#include "KeyframeEffect.h"
#include "PseudoElement.h"
#include "TransitionEvent.h"

namespace WebCore {

DeclarativeAnimation::DeclarativeAnimation(Element& owningElement, const Animation& backingAnimation)
    : WebAnimation(owningElement.document())
    , m_owningElement(&owningElement)
    , m_backingAnimation(const_cast<Animation&>(backingAnimation))
    , m_eventQueue(owningElement)
{
}

DeclarativeAnimation::~DeclarativeAnimation()
{
}

void DeclarativeAnimation::tick()
{
    bool wasRelevant = isRelevant();
    
    WebAnimation::tick();
    invalidateDOMEvents();

    // If a declarative animation transitions from a non-idle state to an idle state, it means it was
    // canceled using the Web Animations API and it should be disassociated from its owner element.
    // From this point on, this animation is like any other animation and should not appear in the
    // maps containing running CSS Transitions and CSS Animations for a given element.
    if (wasRelevant && playState() == WebAnimation::PlayState::Idle) {
        disassociateFromOwningElement();
        m_eventQueue.close();
    }
}

void DeclarativeAnimation::disassociateFromOwningElement()
{
    if (!m_owningElement)
        return;

    if (auto* animationTimeline = timeline())
        animationTimeline->removeDeclarativeAnimationFromListsForOwningElement(*this, *m_owningElement);
    m_owningElement = nullptr;
}

bool DeclarativeAnimation::needsTick() const
{
    return WebAnimation::needsTick() || m_eventQueue.hasPendingEvents();
}

void DeclarativeAnimation::remove()
{
    m_eventQueue.close();
    WebAnimation::remove();
}

void DeclarativeAnimation::setBackingAnimation(const Animation& backingAnimation)
{
    m_backingAnimation = const_cast<Animation&>(backingAnimation);
    syncPropertiesWithBackingAnimation();
}

void DeclarativeAnimation::initialize(const RenderStyle* oldStyle, const RenderStyle& newStyle)
{
    // We need to suspend invalidation of the animation's keyframe effect during its creation
    // as it would otherwise trigger invalidation of the document's style and this would be
    // incorrect since it would happen during style invalidation.
    suspendEffectInvalidation();

    ASSERT(m_owningElement);

    setEffect(KeyframeEffect::create(*m_owningElement));
    setTimeline(&m_owningElement->document().timeline());
    downcast<KeyframeEffect>(effect())->computeDeclarativeAnimationBlendingKeyframes(oldStyle, newStyle);
    syncPropertiesWithBackingAnimation();
    if (backingAnimation().playState() == AnimationPlayState::Playing)
        play();
    else
        pause();

    unsuspendEffectInvalidation();
}

void DeclarativeAnimation::syncPropertiesWithBackingAnimation()
{
}

Optional<double> DeclarativeAnimation::startTime() const
{
    flushPendingStyleChanges();
    return WebAnimation::startTime();
}

void DeclarativeAnimation::setStartTime(Optional<double> startTime)
{
    flushPendingStyleChanges();
    return WebAnimation::setStartTime(startTime);
}

Optional<double> DeclarativeAnimation::bindingsCurrentTime() const
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsCurrentTime();
}

ExceptionOr<void> DeclarativeAnimation::setBindingsCurrentTime(Optional<double> currentTime)
{
    flushPendingStyleChanges();
    return WebAnimation::setBindingsCurrentTime(currentTime);
}

WebAnimation::PlayState DeclarativeAnimation::bindingsPlayState() const
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsPlayState();
}

bool DeclarativeAnimation::bindingsPending() const
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsPending();
}

WebAnimation::ReadyPromise& DeclarativeAnimation::bindingsReady()
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsReady();
}

WebAnimation::FinishedPromise& DeclarativeAnimation::bindingsFinished()
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsFinished();
}

ExceptionOr<void> DeclarativeAnimation::bindingsPlay()
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsPlay();
}

ExceptionOr<void> DeclarativeAnimation::bindingsPause()
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsPause();
}

void DeclarativeAnimation::flushPendingStyleChanges() const
{
    if (auto* animationEffect = effect()) {
        if (is<KeyframeEffect>(animationEffect)) {
            if (auto* target = downcast<KeyframeEffect>(animationEffect)->target())
                target->document().updateStyleIfNeeded();
        }
    }
}

void DeclarativeAnimation::setTimeline(RefPtr<AnimationTimeline>&& newTimeline)
{
    if (timeline() && !newTimeline)
        cancel();

    WebAnimation::setTimeline(WTFMove(newTimeline));
}

void DeclarativeAnimation::cancel()
{
    auto cancelationTime = 0_s;
    if (auto* animationEffect = effect()) {
        if (auto activeTime = animationEffect->getBasicTiming().activeTime)
            cancelationTime = *activeTime;
    }

    WebAnimation::cancel();

    invalidateDOMEvents(cancelationTime);
}

void DeclarativeAnimation::cancelFromStyle()
{
    cancel();
    disassociateFromOwningElement();
}

AnimationEffectPhase DeclarativeAnimation::phaseWithoutEffect() const
{
    // This shouldn't be called if we actually have an effect.
    ASSERT(!effect());

    auto animationCurrentTime = currentTime();
    if (!animationCurrentTime)
        return AnimationEffectPhase::Idle;

    // Since we don't have an effect, the duration will be zero so the phase is 'before' if the current time is less than zero.
    return *animationCurrentTime < 0_s ? AnimationEffectPhase::Before : AnimationEffectPhase::After;
}

void DeclarativeAnimation::invalidateDOMEvents(Seconds elapsedTime)
{
    if (!m_owningElement)
        return;
    
    auto isPending = pending();
    if (isPending && m_wasPending)
        return;

    double iteration;
    AnimationEffectPhase currentPhase;
    Seconds intervalStart;
    Seconds intervalEnd;

    auto* animationEffect = effect();
    if (animationEffect) {
        auto timing = animationEffect->getComputedTiming();
        iteration = timing.currentIteration.valueOr(0);
        currentPhase = timing.phase;
        intervalStart = std::max(0_s, Seconds::fromMilliseconds(std::min(-timing.delay, timing.activeDuration)));
        intervalEnd = std::max(0_s, Seconds::fromMilliseconds(std::min(timing.endTime - timing.delay, timing.activeDuration)));
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

    if (is<CSSAnimation>(this)) {
        // https://drafts.csswg.org/css-animations-2/#events
        if ((wasIdle || wasBefore) && isActive)
            enqueueDOMEvent(eventNames().animationstartEvent, intervalStart);
        else if ((wasIdle || wasBefore) && isAfter) {
            enqueueDOMEvent(eventNames().animationstartEvent, intervalStart);
            enqueueDOMEvent(eventNames().animationendEvent, intervalEnd);
        } else if (wasActive && isBefore)
            enqueueDOMEvent(eventNames().animationendEvent, intervalStart);
        else if (wasActive && isActive && m_previousIteration != iteration) {
            auto iterationBoundary = iteration;
            if (m_previousIteration > iteration)
                iterationBoundary++;
            auto elapsedTime = animationEffect ? animationEffect->iterationDuration() * (iterationBoundary - animationEffect->iterationStart()) : 0_s;
            enqueueDOMEvent(eventNames().animationiterationEvent, elapsedTime);
        } else if (wasActive && isAfter)
            enqueueDOMEvent(eventNames().animationendEvent, intervalEnd);
        else if (wasAfter && isActive)
            enqueueDOMEvent(eventNames().animationstartEvent, intervalEnd);
        else if (wasAfter && isBefore) {
            enqueueDOMEvent(eventNames().animationstartEvent, intervalEnd);
            enqueueDOMEvent(eventNames().animationendEvent, intervalStart);
        } else if ((!wasIdle && !wasAfter) && isIdle)
            enqueueDOMEvent(eventNames().animationcancelEvent, elapsedTime);
    } else if (is<CSSTransition>(this)) {
        // https://drafts.csswg.org/css-transitions-2/#transition-events
        if (wasIdle && (isPending || isBefore))
            enqueueDOMEvent(eventNames().transitionrunEvent, intervalStart);
        else if (wasIdle && isActive) {
            enqueueDOMEvent(eventNames().transitionrunEvent, intervalStart);
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalStart);
        } else if (wasIdle && isAfter) {
            enqueueDOMEvent(eventNames().transitionrunEvent, intervalStart);
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalStart);
            enqueueDOMEvent(eventNames().transitionendEvent, intervalEnd);
        } else if ((m_wasPending || wasBefore) && isActive)
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalStart);
        else if ((m_wasPending || wasBefore) && isAfter) {
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalStart);
            enqueueDOMEvent(eventNames().transitionendEvent, intervalEnd);
        } else if (wasActive && isAfter)
            enqueueDOMEvent(eventNames().transitionendEvent, intervalEnd);
        else if (wasActive && isBefore)
            enqueueDOMEvent(eventNames().transitionendEvent, intervalStart);
        else if (wasAfter && isActive)
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalEnd);
        else if (wasAfter && isBefore) {
            enqueueDOMEvent(eventNames().transitionstartEvent, intervalEnd);
            enqueueDOMEvent(eventNames().transitionendEvent, intervalStart);
        } else if ((!wasIdle && !wasAfter) && isIdle)
            enqueueDOMEvent(eventNames().transitioncancelEvent, elapsedTime);
    }

    m_wasPending = isPending;
    m_previousPhase = currentPhase;
    m_previousIteration = iteration;
}

void DeclarativeAnimation::enqueueDOMEvent(const AtomicString& eventType, Seconds elapsedTime)
{
    ASSERT(m_owningElement);
    auto time = secondsToWebAnimationsAPITime(elapsedTime) / 1000;
    if (is<CSSAnimation>(this))
        m_eventQueue.enqueueEvent(AnimationEvent::create(eventType, downcast<CSSAnimation>(this)->animationName(), time));
    else if (is<CSSTransition>(this))
        m_eventQueue.enqueueEvent(TransitionEvent::create(eventType, downcast<CSSTransition>(this)->transitionProperty(), time, PseudoElement::pseudoElementNameForEvents(m_owningElement->pseudoId())));
}

void DeclarativeAnimation::stop()
{
    m_eventQueue.close();
    WebAnimation::stop();
}

void DeclarativeAnimation::suspend(ReasonForSuspension reason)
{
    m_eventQueue.suspend();
    WebAnimation::suspend(reason);
}

void DeclarativeAnimation::resume()
{
    m_eventQueue.resume();
    WebAnimation::resume();
}

} // namespace WebCore
