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
    // 3.4.3. Setting the target effect of an animation
    // https://drafts.csswg.org/web-animations-1/#setting-the-target-effect

    // 2. If new effect is the same object as old effect, abort this procedure.
    if (effect == m_effect)
        return;

    // 3. If new effect is null and old effect is not null, run the procedure to reset an animation's pending tasks on animation.
    if (!effect && m_effect)
        resetPendingTasks();

    if (m_effect) {
        m_effect->setAnimation(nullptr);

        // Update the Element to Animation map.
        if (m_timeline && is<KeyframeEffect>(m_effect)) {
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

        if (m_timeline && is<KeyframeEffect>(effect)) {
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
    // 3.4.1. Setting the timeline of an animation
    // https://drafts.csswg.org/web-animations-1/#setting-the-timeline

    // 2. If new timeline is the same object as old timeline, abort this procedure.
    if (timeline == m_timeline)
        return;

    // 4. If the animation start time of animation is resolved, make animation’s hold time unresolved.
    if (startTime())
        m_holdTime = std::nullopt;

    if (m_timeline)
        m_timeline->removeAnimation(*this);

    if (timeline)
        timeline->addAnimation(*this);

    if (is<KeyframeEffect>(m_effect)) {
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

    updatePendingTasks();

    // 5. Run the procedure to update an animation’s finished state for animation with the did seek flag set to false,
    // and the synchronously notify flag set to false.
    updateFinishedState(DidSeek::No, SynchronouslyNotify::No);
}
    
std::optional<double> WebAnimation::bindingsStartTime() const
{
    if (!m_startTime)
        return std::nullopt;
    return m_startTime->milliseconds();
}

void WebAnimation::setBindingsStartTime(std::optional<double> startTime)
{
    if (!startTime)
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
        return setCurrentTime(std::nullopt);
    return setCurrentTime(Seconds::fromMilliseconds(currentTime.value()));
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

ExceptionOr<void> WebAnimation::silentlySetCurrentTime(std::optional<Seconds> seekTime)
{
    // 3.4.5. Setting the current time of an animation
    // https://drafts.csswg.org/web-animations-1/#setting-the-current-time-of-an-animation

    // 1. If seek time is an unresolved time value, then perform the following steps.
    if (!seekTime) {
        // 1. If the current time is resolved, then throw a TypeError.
        if (currentTime())
            return Exception { TypeError };
        // 2. Abort these steps.
        return { };
    }

    // 2. Update either animation's hold time or start time as follows:
    // If any of the following conditions are true:
    //     - animation's hold time is resolved, or
    //     - animation's start time is unresolved, or
    //     - animation has no associated timeline or the associated timeline is inactive, or
    //     - animation's playback rate is 0,
    // Set animation's hold time to seek time.
    // Otherwise, set animation's start time to the result of evaluating timeline time - (seek time / playback rate)
    // where timeline time is the current time value of timeline associated with animation.
    if (m_holdTime || !startTime() || !m_timeline || !m_timeline->currentTime() || !m_playbackRate)
        m_holdTime = seekTime;
    else
        setStartTime(m_timeline->currentTime().value() - (seekTime.value() / m_playbackRate));

    // 3. If animation has no associated timeline or the associated timeline is inactive, make animation's start time unresolved.
    if (!m_timeline || !m_timeline->currentTime())
        setStartTime(std::nullopt);

    // 4. Make animation's previous current time unresolved.
    m_previousCurrentTime = std::nullopt;

    return { };
}

ExceptionOr<void> WebAnimation::setCurrentTime(std::optional<Seconds> seekTime)
{
    // 3.4.5. Setting the current time of an animation
    // https://drafts.csswg.org/web-animations-1/#setting-the-current-time-of-an-animation

    // 1. Run the steps to silently set the current time of animation to seek time.
    auto silentResult = silentlySetCurrentTime(seekTime);
    if (silentResult.hasException())
        return silentResult.releaseException();

    // 2. If animation has a pending pause task, synchronously complete the pause operation by performing the following steps:
    if (hasPendingPauseTask()) {
        // 1. Set animation's hold time to seek time.
        m_holdTime = seekTime;
        // 2. Make animation's start time unresolved.
        setStartTime(std::nullopt);
        // 3. Cancel the pending pause task.
        setTimeToRunPendingPauseTask(TimeToRunPendingTask::NotScheduled);
        // 4. Resolve animation's current ready promise with animation.
        m_readyPromise.resolve(*this);
    }

    // 3. Run the procedure to update an animation's finished state for animation with the did seek flag set to true, and the synchronously notify flag set to false.
    updateFinishedState(DidSeek::Yes, SynchronouslyNotify::No);

    return { };
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

void WebAnimation::cancel()
{
    // 3.4.16. Canceling an animation
    // https://drafts.csswg.org/web-animations-1/#canceling-an-animation-section
    //
    // An animation can be canceled which causes the current time to become unresolved hence removing any effects caused by the target effect.
    //
    // The procedure to cancel an animation for animation is as follows:
    //
    // 1. If animation's play state is not idle, perform the following steps:
    if (playState() != PlayState::Idle) {
        // 1. Run the procedure to reset an animation's pending tasks on animation.
        resetPendingTasks();

        // 2. Reject the current finished promise with a DOMException named "AbortError".
        m_finishedPromise.reject(Exception { AbortError });

        // 3. Let current finished promise be a new (pending) Promise object.
        m_finishedPromise.clear();

        // 4. Create an AnimationPlaybackEvent, cancelEvent.
        // 5. Set cancelEvent's type attribute to cancel.
        // 6. Set cancelEvent's currentTime to null.
        // 7. Let timeline time be the current time of the timeline with which animation is associated. If animation is not associated with an
        //    active timeline, let timeline time be n unresolved time value.
        // 8. Set cancelEvent's timelineTime to timeline time. If timeline time is unresolved, set it to null.
        // 9. If animation has a document for timing, then append cancelEvent to its document for timing's pending animation event queue along
        //    with its target, animation. If animation is associated with an active timeline that defines a procedure to convert timeline times
        //    to origin-relative time, let the scheduled event time be the result of applying that procedure to timeline time. Otherwise, the
        //    scheduled event time is an unresolved time value.
        // Otherwise, queue a task to dispatch cancelEvent at animation. The task source for this task is the DOM manipulation task source.
        enqueueAnimationPlaybackEvent(eventNames().cancelEvent, std::nullopt, m_timeline ? m_timeline->currentTime() : std::nullopt);
    }

    // 2. Make animation's hold time unresolved.
    m_holdTime = std::nullopt;

    // 3. Make animation's start time unresolved.
    setStartTime(std::nullopt);
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

void WebAnimation::resetPendingTasks()
{
    // The procedure to reset an animation's pending tasks for animation is as follows:
    // https://drafts.csswg.org/web-animations-1/#reset-an-animations-pending-tasks
    //
    // 1. If animation does not have a pending play task or a pending pause task, abort this procedure.
    if (!pending())
        return;

    // 2. If animation has a pending play task, cancel that task.
    if (hasPendingPlayTask())
        setTimeToRunPendingPlayTask(TimeToRunPendingTask::NotScheduled);

    // 3. If animation has a pending pause task, cancel that task.
    if (hasPendingPauseTask())
        setTimeToRunPendingPauseTask(TimeToRunPendingTask::NotScheduled);

    // 4. Reject animation's current ready promise with a DOMException named "AbortError".
    m_readyPromise.reject(Exception { AbortError });

    // 5. Let animation's current ready promise be the result of creating a new resolved Promise object.
    m_readyPromise.clear();
    m_readyPromise.resolve(*this);
}

ExceptionOr<void> WebAnimation::finish()
{
    // 3.4.15. Finishing an animation
    // https://drafts.csswg.org/web-animations-1/#finishing-an-animation-section

    // An animation can be advanced to the natural end of its current playback direction by using the procedure to finish an animation for animation defined below:
    //
    // 1. If animation playback rate is zero, or if animation playback rate > 0 and target effect end is infinity, throw an InvalidStateError and abort these steps.
    if (!m_playbackRate || (m_playbackRate > 0 && effectEndTime() == Seconds::infinity()))
        return Exception { InvalidStateError };

    // 2. Set limit as follows:
    // If animation playback rate > 0, let limit be target effect end.
    // Otherwise, let limit be zero.
    auto limit = m_playbackRate > 0 ? effectEndTime() : 0_s;

    // 3. Silently set the current time to limit.
    silentlySetCurrentTime(limit);

    // 4. If animation's start time is unresolved and animation has an associated active timeline, let the start time be the result of
    //    evaluating timeline time - (limit / playback rate) where timeline time is the current time value of the associated timeline.
    if (!startTime() && m_timeline && m_timeline->currentTime())
        setStartTime(m_timeline->currentTime().value() - (limit / m_playbackRate));

    // 5. If there is a pending pause task and start time is resolved,
    if (hasPendingPauseTask() && startTime()) {
        // 1. Let the hold time be unresolved.
        m_holdTime = std::nullopt;
        // 2. Cancel the pending pause task.
        setTimeToRunPendingPauseTask(TimeToRunPendingTask::NotScheduled);
        // 3. Resolve the current ready promise of animation with animation.
        m_readyPromise.resolve(*this);
    }

    // 6. If there is a pending play task and start time is resolved, cancel that task and resolve the current ready promise of animation with animation.
    if (hasPendingPlayTask() && startTime()) {
        setTimeToRunPendingPlayTask(TimeToRunPendingTask::NotScheduled);
        m_readyPromise.resolve(*this);
    }

    // 7. Run the procedure to update an animation's finished state animation with the did seek flag set to true, and the synchronously notify flag set to true.
    updateFinishedState(DidSeek::Yes, SynchronouslyNotify::Yes);

    return { };
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

ExceptionOr<void> WebAnimation::play()
{
    return play(AutoRewind::Yes);
}

ExceptionOr<void> WebAnimation::play(AutoRewind autoRewind)
{
    // 3.4.10. Playing an animation
    // https://drafts.csswg.org/web-animations-1/#play-an-animation

    auto localTime = currentTime();
    auto endTime = effectEndTime();

    // 1. Let aborted pause be a boolean flag that is true if animation has a pending pause task, and false otherwise.
    bool abortedPause = hasPendingPauseTask();

    // 2. Let has pending ready promise be a boolean flag that is initially false.
    bool hasPendingReadyPromise = false;

    // 3. Perform the steps corresponding to the first matching condition from the following, if any:
    if (m_playbackRate > 0 && autoRewind == AutoRewind::Yes && (!localTime || localTime.value() < 0_s || localTime.value() >= endTime)) {
        // If animation playback rate > 0, the auto-rewind flag is true and either animation's:
        //     - current time is unresolved, or
        //     - current time < zero, or
        //     - current time ≥ target effect end,
        // Set animation's hold time to zero.
        m_holdTime = 0_s;
    } else if (m_playbackRate < 0 && autoRewind == AutoRewind::Yes && (!localTime || localTime.value() <= 0_s || localTime.value() > endTime)) {
        // If animation playback rate < 0, the auto-rewind flag is true and either animation's:
        //     - current time is unresolved, or
        //     - current time ≤ zero, or
        //     - current time > target effect end
        // If target effect end is positive infinity, throw an InvalidStateError and abort these steps.
        if (endTime == Seconds::infinity())
            return Exception { InvalidStateError };
        m_holdTime = endTime;
    } else if (!m_playbackRate && !localTime) {
        // If animation playback rate = 0 and animation's current time is unresolved,
        // Set animation's hold time to zero.
        m_holdTime = 0_s;
    }

    // 4. If animation has a pending play task or a pending pause task,
    if (hasPendingPauseTask()) {
        // 1. Cancel that task.
        setTimeToRunPendingPlayTask(TimeToRunPendingTask::NotScheduled);
        // 2. Set has pending ready promise to true.
        hasPendingReadyPromise = true;
    }

    // 5. If animation's hold time is unresolved and aborted pause is false, abort this procedure.
    if (!m_holdTime && !abortedPause)
        return { };

    // 6. If animation's hold time is resolved, let its start time be unresolved.
    if (m_holdTime)
        setStartTime(std::nullopt);

    // 7. If has pending ready promise is false, let animation's current ready promise be a new (pending) Promise object.
    if (!hasPendingReadyPromise)
        m_readyPromise.clear();

    // 8. Schedule a task to run as soon as animation is ready.
    setTimeToRunPendingPlayTask(TimeToRunPendingTask::WhenReady);

    // 9. Run the procedure to update an animation's finished state for animation with the did seek flag set to false, and the synchronously notify flag set to false.
    updateFinishedState(DidSeek::No, SynchronouslyNotify::No);

    return { };
}

void WebAnimation::setTimeToRunPendingPlayTask(TimeToRunPendingTask timeToRunPendingTask)
{
    m_timeToRunPendingPlayTask = timeToRunPendingTask;
    updatePendingTasks();
}

void WebAnimation::runPendingPlayTask()
{
    // 3.4.10. Playing an animation, step 8.
    // https://drafts.csswg.org/web-animations-1/#play-an-animation

    m_timeToRunPendingPlayTask = TimeToRunPendingTask::NotScheduled;

    // 1. Let ready time be the time value of the timeline associated with animation at the moment when animation became ready.
    auto readyTime = m_timeline->currentTime();

    // 2. If animation's start time is unresolved, perform the following steps:
    if (!startTime()) {
        // 1. Let new start time be the result of evaluating ready time - hold time / animation playback rate for animation.
        // If the animation playback rate is zero, let new start time be simply ready time.
        auto newStartTime = readyTime.value();
        if (m_playbackRate)
            newStartTime -= m_holdTime.value() / m_playbackRate;
        // 2. If animation's playback rate is not 0, make animation's hold time unresolved.
        if (m_playbackRate)
            m_holdTime = std::nullopt;
        // 3. Set the animation start time of animation to new start time.
        setStartTime(newStartTime);
    }

    // 3. Resolve animation's current ready promise with animation.
    m_readyPromise.resolve(*this);

    // 4. Run the procedure to update an animation's finished state for animation with the did seek flag set to false, and the synchronously notify flag set to false.
    updateFinishedState(DidSeek::No, SynchronouslyNotify::No);
}

ExceptionOr<void> WebAnimation::pause()
{
    // 3.4.11. Pausing an animation
    // https://drafts.csswg.org/web-animations-1/#pause-an-animation

    // 1. If animation has a pending pause task, abort these steps.
    if (hasPendingPauseTask())
        return { };

    // 2. If the play state of animation is paused, abort these steps.
    if (playState() == PlayState::Paused)
        return { };

    auto localTime = currentTime();

    // 3. If the animation's current time is unresolved, perform the steps according to the first matching condition from below:
    if (!localTime) {
        if (m_playbackRate >= 0) {
            // If animation's playback rate is ≥ 0, let animation's hold time be zero.
            m_holdTime = 0_s;
        } else if (effectEndTime() == Seconds::infinity()) {
            // Otherwise, if target effect end for animation is positive infinity, throw an InvalidStateError and abort these steps.
            return Exception { InvalidStateError };
        } else {
            // Otherwise, let animation's hold time be target effect end.
            m_holdTime = effectEndTime();
        }
    }

    // 4. Let has pending ready promise be a boolean flag that is initially false.
    bool hasPendingReadyPromise = false;

    // 5. If animation has a pending play task, cancel that task and let has pending ready promise be true.
    if (hasPendingPlayTask()) {
        setTimeToRunPendingPlayTask(TimeToRunPendingTask::NotScheduled);
        hasPendingReadyPromise = true;
    }

    // 6. If has pending ready promise is false, set animation's current ready promise to a new (pending) Promise object.
    if (!hasPendingReadyPromise)
        m_readyPromise.clear();

    // 7. Schedule a task to be executed at the first possible moment after the user agent has performed any processing necessary
    //    to suspend the playback of animation's target effect, if any.
    setTimeToRunPendingPauseTask(TimeToRunPendingTask::ASAP);

    // 8. Run the procedure to update an animation's finished state for animation with the did seek flag set to false, and the synchronously notify flag set to false.
    updateFinishedState(DidSeek::No, SynchronouslyNotify::No);

    return { };
}

void WebAnimation::setTimeToRunPendingPauseTask(TimeToRunPendingTask timeToRunPendingTask)
{
    m_timeToRunPendingPauseTask = timeToRunPendingTask;
    updatePendingTasks();
}

void WebAnimation::runPendingPauseTask()
{
    // 3.4.11. Pausing an animation, step 7.
    // https://drafts.csswg.org/web-animations-1/#pause-an-animation

    m_timeToRunPendingPauseTask = TimeToRunPendingTask::NotScheduled;

    // 1. Let ready time be the time value of the timeline associated with animation at the moment when the user agent
    //    completed processing necessary to suspend playback of animation's target effect.
    auto readyTime = m_timeline->currentTime();
    auto animationStartTime = startTime();

    // 2. If animation's start time is resolved and its hold time is not resolved, let animation's hold time be the result of
    //    evaluating (ready time - start time) × playback rate.
    //    Note: The hold time might be already set if the animation is finished, or if the animation is pending, waiting to begin
    //    playback. In either case we want to preserve the hold time as we enter the paused state.
    if (animationStartTime && !m_holdTime)
        m_holdTime = (readyTime.value() - animationStartTime.value()) * m_playbackRate;

    // 3. Make animation's start time unresolved.
    setStartTime(std::nullopt);

    // 4. Resolve animation's current ready promise with animation.
    m_readyPromise.resolve(*this);

    // 5. Run the procedure to update an animation's finished state for animation with the did seek flag set to false, and the
    //    synchronously notify flag set to false.
    updateFinishedState(DidSeek::No, SynchronouslyNotify::No);
}

void WebAnimation::updatePendingTasks()
{
    if (m_timeToRunPendingPauseTask == TimeToRunPendingTask::ASAP && m_timeline)
        runPendingPauseTask();

    // FIXME: This should only happen if we're ready, at the moment we think we're ready if we have a timeline.
    if (m_timeToRunPendingPlayTask == TimeToRunPendingTask::WhenReady && m_timeline)
        runPendingPlayTask();
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
    if (is<KeyframeEffect>(m_effect))
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
