/*
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
#include "WebAnimation.h"

#include "AnimationEffect.h"
#include "AnimationPlaybackEvent.h"
#include "AnimationTimeline.h"
#include "Document.h"
#include "EventNames.h"
#include "JSWebAnimation.h"
#include "KeyframeEffect.h"
#include "Microtasks.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

Ref<WebAnimation> WebAnimation::create(Document& document, AnimationEffect* effect, AnimationTimeline* timeline)
{
    auto result = adoptRef(*new WebAnimation(document));

    result->setEffect(effect);

    // FIXME: the spec mandates distinguishing between an omitted timeline parameter
    // and an explicit null or undefined value (webkit.org/b/179065).
    result->setTimeline(timeline ? timeline : &document.timeline());

    return result;
}

WebAnimation::WebAnimation(Document& document)
    : ActiveDOMObject(&document)
    , m_readyPromise(*this, &WebAnimation::readyPromiseResolve)
    , m_finishedPromise(*this, &WebAnimation::finishedPromiseResolve)
{
    suspendIfNeeded();
}

WebAnimation::~WebAnimation()
{
    if (m_timeline)
        m_timeline->removeAnimation(*this);
}

void WebAnimation::setEffect(RefPtr<AnimationEffect>&& effect)
{
    if (effect == m_effect)
        return;

    if (m_effect) {
        m_effect->setAnimation(nullptr);

        // Update the Element to Animation map.
        if (m_timeline && m_effect->isKeyframeEffect()) {
            auto* keyframeEffect = downcast<KeyframeEffect>(m_effect.get());
            auto* target = keyframeEffect->target();
            if (target)
                m_timeline->animationWasRemovedFromElement(*this, *target);
        }
    }

    if (effect) {
        // An animation effect can only be associated with a single animation.
        if (effect->animation())
            effect->animation()->setEffect(nullptr);

        effect->setAnimation(this);

        if (m_timeline && effect->isKeyframeEffect()) {
            auto* keyframeEffect = downcast<KeyframeEffect>(effect.get());
            auto* target = keyframeEffect->target();
            if (target)
                m_timeline->animationWasAddedToElement(*this, *target);
        }
    }

    m_effect = WTFMove(effect);
}

void WebAnimation::setTimeline(RefPtr<AnimationTimeline>&& timeline)
{
    if (timeline == m_timeline)
        return;

    // FIXME: If the animation start time of animation is resolved, make animation’s
    // hold time unresolved (webkit.org/b/178932).

    if (m_timeline)
        m_timeline->removeAnimation(*this);

    if (timeline)
        timeline->addAnimation(*this);

    if (m_effect && m_effect->isKeyframeEffect()) {
        auto* keyframeEffect = downcast<KeyframeEffect>(m_effect.get());
        auto* target = keyframeEffect->target();
        if (target) {
            if (m_timeline)
                m_timeline->animationWasRemovedFromElement(*this, *target);
            if (timeline)
                timeline->animationWasAddedToElement(*this, *target);
        }
    }

    m_timeline = WTFMove(timeline);
}
    
std::optional<double> WebAnimation::bindingsStartTime() const
{
    if (m_startTime)
        return m_startTime->milliseconds();
    return std::nullopt;
}

void WebAnimation::setBindingsStartTime(std::optional<double> startTime)
{
    if (startTime == std::nullopt)
        setStartTime(std::nullopt);
    else
        setStartTime(Seconds::fromMilliseconds(startTime.value()));
}

std::optional<Seconds> WebAnimation::startTime() const
{
    return m_startTime;
}

void WebAnimation::setStartTime(std::optional<Seconds> startTime)
{
    if (startTime == m_startTime)
        return;

    m_startTime = startTime;
    
    if (m_timeline)
        m_timeline->animationTimingModelDidChange();
}

std::optional<double> WebAnimation::bindingsCurrentTime() const
{
    auto time = currentTime();
    if (!time)
        return std::nullopt;
    return time->milliseconds();
}

ExceptionOr<void> WebAnimation::setBindingsCurrentTime(std::optional<double> currentTime)
{
    if (!currentTime)
        return Exception { TypeError };
    setCurrentTime(Seconds::fromMilliseconds(currentTime.value()));
    return { };
}

std::optional<Seconds> WebAnimation::currentTime() const
{
    return currentTime(RespectHoldTime::Yes);
}

std::optional<Seconds> WebAnimation::currentTime(RespectHoldTime respectHoldTime) const
{
    // 3.4.4. The current time of an animation
    // https://drafts.csswg.org/web-animations-1/#the-current-time-of-an-animation

    // The current time is calculated from the first matching condition from below:

    // If the animation's hold time is resolved, the current time is the animation's hold time.
    if (respectHoldTime == RespectHoldTime::Yes && m_holdTime)
        return m_holdTime;

    // If any of the following are true:
    //     1. the animation has no associated timeline, or
    //     2. the associated timeline is inactive, or
    //     3. the animation's start time is unresolved.
    // The current time is an unresolved time value.
    if (!m_timeline || !m_timeline->currentTime() || !m_startTime)
        return std::nullopt;

    // Otherwise, current time = (timeline time - start time) * playback rate
    return (m_timeline->currentTime().value() - m_startTime.value()) * m_playbackRate;
}

void WebAnimation::setCurrentTime(std::optional<Seconds> seekTime)
{
    // FIXME: account for hold time when we support it (webkit.org/b/178932),
    // including situations where playbackRate is 0.

    if (!m_timeline) {
        setStartTime(std::nullopt);
        return;
    }

    auto timelineTime = m_timeline->currentTime();
    if (!timelineTime) {
        setStartTime(std::nullopt);
        return;
    }

    setStartTime(timelineTime.value() - (seekTime.value() / m_playbackRate));
}

void WebAnimation::setPlaybackRate(double newPlaybackRate)
{
    if (m_playbackRate == newPlaybackRate)
        return;

    // 3.5.17.1. Updating the playback rate of an animation
    // Changes to the playback rate trigger a compensatory seek so that that the animation's current time
    // is unaffected by the change to the playback rate.
    auto previousTime = currentTime();
    m_playbackRate = newPlaybackRate;
    if (previousTime)
        setCurrentTime(previousTime);
}

auto WebAnimation::playState() const -> PlayState
{
    // Section 3.5.19. Play states

    // Animation has a pending play task or a pending pause task → pending
    if (pending())
        return PlayState::Pending;

    // The current time of animation is unresolved → idle
    if (!currentTime())
        return PlayState::Idle;

    // The start time of animation is unresolved → paused
    if (!startTime())
        return PlayState::Paused;

    // For animation, animation playback rate > 0 and current time ≥ target effect end; or
    // animation playback rate < 0 and current time ≤ 0 → finished
    if ((m_playbackRate > 0 && currentTime().value() >= effectEndTime()) || (m_playbackRate < 0 && currentTime().value() <= 0_s))
        return PlayState::Finished;

    // Otherwise → running
    return PlayState::Running;
}

Seconds WebAnimation::effectEndTime() const
{
    // The target effect end of an animation is equal to the end time of the animation's target effect.
    // If the animation has no target effect, the target effect end is zero.
    // FIXME: Use the effect's computed end time once we support it (webkit.org/b/179170).
    return m_effect ? m_effect->timing()->duration() : 0_s;
}

void WebAnimation::enqueueAnimationPlaybackEvent(const AtomicString& type, std::optional<Seconds> currentTime, std::optional<Seconds> timelineTime)
{
    auto event = AnimationPlaybackEvent::create(type, currentTime, timelineTime);
    event->setTarget(this);

    if (is<DocumentTimeline>(m_timeline)) {
        // If animation has a document for timing, then append event to its document for timing's pending animation event queue along
        // with its target, animation. If animation is associated with an active timeline that defines a procedure to convert timeline times
        // to origin-relative time, let the scheduled event time be the result of applying that procedure to timeline time. Otherwise, the
        // scheduled event time is an unresolved time value.
        downcast<DocumentTimeline>(*m_timeline).enqueueAnimationPlaybackEvent(WTFMove(event));
    } else {
        // Otherwise, queue a task to dispatch event at animation. The task source for this task is the DOM manipulation task source.
        callOnMainThread([this, pendingActivity = makePendingActivity(*this), event = WTFMove(event)]() {
            if (!m_isStopped)
                this->dispatchEvent(event);
        });
    }
}

void WebAnimation::updateFinishedState(DidSeek didSeek, SynchronouslyNotify synchronouslyNotify)
{
    // 3.4.14. Updating the finished state
    // https://drafts.csswg.org/web-animations-1/#updating-the-finished-state

    // 1. Let the unconstrained current time be the result of calculating the current time substituting an unresolved time value
    // for the hold time if did seek is false. If did seek is true, the unconstrained current time is equal to the current time.
    auto unconstrainedCurrentTime = currentTime(didSeek == DidSeek::Yes ? RespectHoldTime::Yes : RespectHoldTime::No);
    auto endTime = effectEndTime();

    // 2. If all three of the following conditions are true,
    //    - the unconstrained current time is resolved, and
    //    - animation's start time is resolved, and
    //    - animation does not have a pending play task or a pending pause task,
    if (unconstrainedCurrentTime && startTime() && !pending()) {
        // then update animation's hold time based on the first matching condition for animation from below, if any:
        if (m_playbackRate > 0 && unconstrainedCurrentTime >= endTime) {
            // If animation playback rate > 0 and unconstrained current time is greater than or equal to target effect end,
            // If did seek is true, let the hold time be the value of unconstrained current time.
            if (didSeek == DidSeek::Yes)
                m_holdTime = unconstrainedCurrentTime;
            // If did seek is false, let the hold time be the maximum value of previous current time and target effect end. If the previous current time is unresolved, let the hold time be target effect end.
            else if (!m_previousCurrentTime)
                m_holdTime = endTime;
            else
                m_holdTime = std::max(m_previousCurrentTime.value(), endTime);
        } else if (m_playbackRate < 0 && unconstrainedCurrentTime <= 0_s) {
            // If animation playback rate < 0 and unconstrained current time is less than or equal to 0,
            // If did seek is true, let the hold time be the value of unconstrained current time.
            if (didSeek == DidSeek::Yes)
                m_holdTime = unconstrainedCurrentTime;
            // If did seek is false, let the hold time be the minimum value of previous current time and zero. If the previous current time is unresolved, let the hold time be zero.
            else if (!m_previousCurrentTime)
                m_holdTime = 0_s;
            else
                m_holdTime = std::min(m_previousCurrentTime.value(), 0_s);
        } else if (m_playbackRate && m_timeline && m_timeline->currentTime()) {
            // If animation playback rate ≠ 0, and animation is associated with an active timeline,
            // Perform the following steps:
            // 1. If did seek is true and the hold time is resolved, let animation's start time be equal to the result of evaluating timeline time - (hold time / playback rate)
            //    where timeline time is the current time value of timeline associated with animation.
            if (didSeek == DidSeek::Yes && m_holdTime)
                setStartTime(m_timeline->currentTime().value() - (m_holdTime.value() / m_playbackRate));
            // 2. Let the hold time be unresolved.
            m_holdTime = std::nullopt;
        }
    }

    // 3. Set the previous current time of animation be the result of calculating its current time.
    m_previousCurrentTime = currentTime();

    // 4. Let current finished state be true if the play state of animation is finished. Otherwise, let it be false.
    auto currentFinishedState = playState() == PlayState::Finished;

    // 5. If current finished state is true and the current finished promise is not yet resolved, perform the following steps:
    if (currentFinishedState && !m_finishedPromise.isFulfilled()) {
        if (synchronouslyNotify == SynchronouslyNotify::Yes) {
            // If synchronously notify is true, cancel any queued microtask to run the finish notification steps for this animation,
            // and run the finish notification steps immediately.
            m_finishNotificationStepsMicrotaskPending = false;
            finishNotificationSteps();
        } else if (!m_finishNotificationStepsMicrotaskPending) {
            // Otherwise, if synchronously notify is false, queue a microtask to run finish notification steps for animation unless there
            // is already a microtask queued to run those steps for animation.
            m_finishNotificationStepsMicrotaskPending = true;
            scheduleMicrotaskIfNeeded();
        }
    }

    // 6. If current finished state is false and animation's current finished promise is already resolved, set animation's current
    // finished promise to a new (pending) Promise object.
    if (!currentFinishedState && m_finishedPromise.isFulfilled())
        m_finishedPromise.clear();
}

void WebAnimation::scheduleMicrotaskIfNeeded()
{
    if (m_scheduledMicrotask)
        return;

    m_scheduledMicrotask = true;
    MicrotaskQueue::mainThreadQueue().append(std::make_unique<VoidMicrotask>(std::bind(&WebAnimation::performMicrotask, this)));
}

void WebAnimation::performMicrotask()
{
    m_scheduledMicrotask = false;
    if (!m_finishNotificationStepsMicrotaskPending)
        return;

    m_finishNotificationStepsMicrotaskPending = false;
    finishNotificationSteps();
}

void WebAnimation::finishNotificationSteps()
{
    // 3.4.14. Updating the finished state
    // https://drafts.csswg.org/web-animations-1/#finish-notification-steps

    // Let finish notification steps refer to the following procedure:
    // 1. If animation's play state is not equal to finished, abort these steps.
    if (playState() != PlayState::Finished)
        return;

    // 2. Resolve animation's current finished promise object with animation.
    m_finishedPromise.resolve(*this);

    // 3. Create an AnimationPlaybackEvent, finishEvent.
    // 4. Set finishEvent's type attribute to finish.
    // 5. Set finishEvent's currentTime attribute to the current time of animation.
    // 6. Set finishEvent's timelineTime attribute to the current time of the timeline with which animation is associated.
    //    If animation is not associated with a timeline, or the timeline is inactive, let timelineTime be null.
    // 7. If animation has a document for timing, then append finishEvent to its document for timing's pending animation event
    //    queue along with its target, animation. For the scheduled event time, use the result of converting animation's target
    //    effect end to an origin-relative time.
    //    Otherwise, queue a task to dispatch finishEvent at animation. The task source for this task is the DOM manipulation task source.
    enqueueAnimationPlaybackEvent(eventNames().finishEvent, currentTime(), m_timeline ? m_timeline->currentTime() : std::nullopt);
}

Seconds WebAnimation::timeToNextRequiredTick(Seconds timelineTime) const
{
    if (!m_timeline || !m_startTime || !m_effect || !m_playbackRate)
        return Seconds::infinity();

    auto startTime = m_startTime.value();
    auto endTime = startTime + (m_effect->timing()->duration() / m_playbackRate);

    // If we haven't started yet, return the interval until our active start time.
    auto activeStartTime = std::min(startTime, endTime);
    if (timelineTime <= activeStartTime)
        return activeStartTime - timelineTime;

    // If we're in the middle of our active duration, we want to be called as soon as possible.
    auto activeEndTime = std::max(startTime, endTime);
    if (timelineTime <= activeEndTime)
        return 0_ms;

    // If none of the previous cases match, then we're already past our active duration
    // and do not need scheduling.
    return Seconds::infinity();
}

void WebAnimation::resolve(RenderStyle& targetStyle)
{
    if (m_effect && currentTime())
        m_effect->applyAtLocalTime(currentTime().value(), targetStyle);
}

void WebAnimation::acceleratedRunningStateDidChange()
{
    if (is<DocumentTimeline>(m_timeline))
        downcast<DocumentTimeline>(*m_timeline).animationAcceleratedRunningStateDidChange(*this);
}

void WebAnimation::startOrStopAccelerated()
{
    if (m_effect && m_effect->isKeyframeEffect())
        downcast<KeyframeEffect>(*m_effect).startOrStopAccelerated();
}

WebAnimation& WebAnimation::readyPromiseResolve()
{
    return *this;
}

WebAnimation& WebAnimation::finishedPromiseResolve()
{
    return *this;
}

String WebAnimation::description()
{
    return "Animation";
}

const char* WebAnimation::activeDOMObjectName() const
{
    return "Animation";
}

bool WebAnimation::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity();
}

void WebAnimation::stop()
{
    m_isStopped = true;
    removeAllEventListeners();
}

} // namespace WebCore
