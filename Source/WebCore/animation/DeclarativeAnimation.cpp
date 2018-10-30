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
#include "AnimationEffectTimingReadOnly.h"
#include "AnimationEvent.h"
#include "Element.h"
#include "EventNames.h"
#include "KeyframeEffectReadOnly.h"
#include "PseudoElement.h"
#include "TransitionEvent.h"

namespace WebCore {

DeclarativeAnimation::DeclarativeAnimation(Element& target, const Animation& backingAnimation)
    : WebAnimation(target.document())
    , m_target(target)
    , m_backingAnimation(const_cast<Animation&>(backingAnimation))
    , m_eventQueue(target)
{
}

DeclarativeAnimation::~DeclarativeAnimation()
{
}

void DeclarativeAnimation::tick()
{
    WebAnimation::tick();
    invalidateDOMEvents();
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

void DeclarativeAnimation::initialize(const Element& target, const RenderStyle* oldStyle, const RenderStyle& newStyle)
{
    // We need to suspend invalidation of the animation's keyframe effect during its creation
    // as it would otherwise trigger invalidation of the document's style and this would be
    // incorrect since it would happen during style invalidation.
    suspendEffectInvalidation();

    setEffect(KeyframeEffectReadOnly::create(target));
    setTimeline(&target.document().timeline());
    downcast<KeyframeEffectReadOnly>(effect())->computeDeclarativeAnimationBlendingKeyframes(oldStyle, newStyle);
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

std::optional<double> DeclarativeAnimation::startTime() const
{
    flushPendingStyleChanges();
    return WebAnimation::startTime();
}

void DeclarativeAnimation::setStartTime(std::optional<double> startTime)
{
    flushPendingStyleChanges();
    return WebAnimation::setStartTime(startTime);
}

std::optional<double> DeclarativeAnimation::bindingsCurrentTime() const
{
    flushPendingStyleChanges();
    return WebAnimation::bindingsCurrentTime();
}

ExceptionOr<void> DeclarativeAnimation::setBindingsCurrentTime(std::optional<double> currentTime)
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
        if (is<KeyframeEffectReadOnly>(animationEffect)) {
            if (auto* target = downcast<KeyframeEffectReadOnly>(animationEffect)->target())
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
    if (auto animationEffect = effect())
        cancelationTime = animationEffect->activeTime().value_or(0_s);

    WebAnimation::cancel();

    invalidateDOMEvents(cancelationTime);
}

AnimationEffectReadOnly::Phase DeclarativeAnimation::phaseWithoutEffect() const
{
    // This shouldn't be called if we actually have an effect.
    ASSERT(!effect());

    auto animationCurrentTime = currentTime();
    if (!animationCurrentTime)
        return AnimationEffectReadOnly::Phase::Idle;

    // Since we don't have an effect, the duration will be zero so the phase is 'before' if the current time is less than zero.
    return animationCurrentTime.value() < 0_s ? AnimationEffectReadOnly::Phase::Before : AnimationEffectReadOnly::Phase::After;
}

void DeclarativeAnimation::invalidateDOMEvents(Seconds elapsedTime)
{
    auto* animationEffect = effect();

    auto isPending = pending();
    if (isPending && m_wasPending)
        return;

    auto iteration = animationEffect ? animationEffect->currentIteration().value_or(0) : 0;
    auto currentPhase = animationEffect ? animationEffect->phase() : phaseWithoutEffect();

    bool wasActive = m_previousPhase == AnimationEffectReadOnly::Phase::Active;
    bool wasAfter = m_previousPhase == AnimationEffectReadOnly::Phase::After;
    bool wasBefore = m_previousPhase == AnimationEffectReadOnly::Phase::Before;
    bool wasIdle = m_previousPhase == AnimationEffectReadOnly::Phase::Idle;

    bool isActive = currentPhase == AnimationEffectReadOnly::Phase::Active;
    bool isAfter = currentPhase == AnimationEffectReadOnly::Phase::After;
    bool isBefore = currentPhase == AnimationEffectReadOnly::Phase::Before;
    bool isIdle = currentPhase == AnimationEffectReadOnly::Phase::Idle;

    auto* effectTiming = animationEffect ? animationEffect->timing() : nullptr;
    auto intervalStart = effectTiming ? std::max(0_s, std::min(-effectTiming->delay(), effectTiming->activeDuration())) : 0_s;
    auto intervalEnd = effectTiming ? std::max(0_s, std::min(effectTiming->endTime() - effectTiming->delay(), effectTiming->activeDuration())) : 0_s;

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
            auto elapsedTime = effectTiming ? effectTiming->iterationDuration() * (iterationBoundary - effectTiming->iterationStart()) : 0_s;
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
    auto time = secondsToWebAnimationsAPITime(elapsedTime) / 1000;
    if (is<CSSAnimation>(this))
        m_eventQueue.enqueueEvent(AnimationEvent::create(eventType, downcast<CSSAnimation>(this)->animationName(), time));
    else if (is<CSSTransition>(this))
        m_eventQueue.enqueueEvent(TransitionEvent::create(eventType, downcast<CSSTransition>(this)->transitionProperty(), time, PseudoElement::pseudoElementNameForEvents(m_target.pseudoId())));
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
