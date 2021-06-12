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
#include "CSSComputedStyleDeclaration.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMPromiseProxy.h"
#include "DeclarativeAnimation.h"
#include "Document.h"
#include "DocumentTimeline.h"
#include "Element.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "InspectorInstrumentation.h"
#include "JSWebAnimation.h"
#include "KeyframeEffect.h"
#include "KeyframeEffectStack.h"
#include "Logging.h"
#include "RenderElement.h"
#include "StyledElement.h"
#include "WebAnimationUtilities.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebAnimation);

HashSet<WebAnimation*>& WebAnimation::instances()
{
    static NeverDestroyed<HashSet<WebAnimation*>> instances;
    return instances;
}

Ref<WebAnimation> WebAnimation::create(Document& document, AnimationEffect* effect)
{
    auto result = adoptRef(*new WebAnimation(document));
    result->setEffect(effect);
    result->setTimeline(&document.timeline());

    InspectorInstrumentation::didCreateWebAnimation(result.get());

    return result;
}

Ref<WebAnimation> WebAnimation::create(Document& document, AnimationEffect* effect, AnimationTimeline* timeline)
{
    auto result = adoptRef(*new WebAnimation(document));
    result->setEffect(effect);
    if (timeline)
        result->setTimeline(timeline);

    InspectorInstrumentation::didCreateWebAnimation(result.get());

    return result;
}

WebAnimation::WebAnimation(Document& document)
    : ActiveDOMObject(document)
    , m_readyPromise(makeUniqueRef<ReadyPromise>(*this, &WebAnimation::readyPromiseResolve))
    , m_finishedPromise(makeUniqueRef<FinishedPromise>(*this, &WebAnimation::finishedPromiseResolve))
{
    m_readyPromise->resolve(*this);
    suspendIfNeeded();

    instances().add(this);
}

WebAnimation::~WebAnimation()
{
    InspectorInstrumentation::willDestroyWebAnimation(*this);

    if (m_timeline)
        m_timeline->forgetAnimation(this);

    ASSERT(instances().contains(this));
    instances().remove(this);
}

void WebAnimation::contextDestroyed()
{
    InspectorInstrumentation::willDestroyWebAnimation(*this);

    ActiveDOMObject::contextDestroyed();
}

void WebAnimation::remove()
{
    // This object could be deleted after either clearing the effect or timeline relationship.
    auto protectedThis = makeRef(*this);
    setEffectInternal(nullptr);
    setTimeline(nullptr);
}

void WebAnimation::suspendEffectInvalidation()
{
    ++m_suspendCount;
}

void WebAnimation::unsuspendEffectInvalidation()
{
    ASSERT(m_suspendCount > 0);
    --m_suspendCount;
}

void WebAnimation::effectTimingDidChange()
{
    timingDidChange(DidSeek::No, SynchronouslyNotify::Yes);

    if (m_effect)
        m_effect->animationDidChangeTimingProperties();

    InspectorInstrumentation::didChangeWebAnimationEffectTiming(*this);
}

void WebAnimation::setId(const String& id)
{
    m_id = id;

    InspectorInstrumentation::didChangeWebAnimationName(*this);
}

void WebAnimation::setBindingsEffect(RefPtr<AnimationEffect>&& newEffect)
{
    setEffect(WTFMove(newEffect));
}

void WebAnimation::setEffect(RefPtr<AnimationEffect>&& newEffect)
{
    // 3.4.3. Setting the target effect of an animation
    // https://drafts.csswg.org/web-animations-1/#setting-the-target-effect

    // 1. Let old effect be the current target effect of animation, if any.
    auto oldEffect = m_effect;

    // 2. If new effect is the same object as old effect, abort this procedure.
    if (newEffect == oldEffect)
        return;

    // 3. If animation has a pending pause task, reschedule that task to run as soon as animation is ready.
    if (hasPendingPauseTask())
        m_timeToRunPendingPauseTask = TimeToRunPendingTask::WhenReady;

    // 4. If animation has a pending play task, reschedule that task to run as soon as animation is ready to play new effect.
    if (hasPendingPlayTask())
        m_timeToRunPendingPlayTask = TimeToRunPendingTask::WhenReady;

    // 5. If new effect is not null and if new effect is the target effect of another animation, previous animation, run the
    // procedure to set the target effect of an animation (this procedure) on previous animation passing null as new effect.
    if (newEffect && newEffect->animation())
        newEffect->animation()->setEffect(nullptr);

    // 6. Let the target effect of animation be new effect.
    // In the case of a declarative animation, we don't want to remove the animation from the relevant maps because
    // while the effect was set via the API, the element still has a transition or animation set up and we must
    // not break the timeline-to-animation relationship.

    invalidateEffect();

    // This object could be deleted after clearing the effect relationship.
    auto protectedThis = makeRef(*this);
    setEffectInternal(WTFMove(newEffect), isDeclarativeAnimation());

    // 7. Run the procedure to update an animation's finished state for animation with the did seek flag set to false,
    // and the synchronously notify flag set to false.
    timingDidChange(DidSeek::No, SynchronouslyNotify::No);

    invalidateEffect();
}

void WebAnimation::setEffectInternal(RefPtr<AnimationEffect>&& newEffect, bool doNotRemoveFromTimeline)
{
    if (m_effect == newEffect)
        return;

    auto oldEffect = std::exchange(m_effect, WTFMove(newEffect));

    std::optional<const Styleable> previousTarget = is<KeyframeEffect>(oldEffect) ? downcast<KeyframeEffect>(oldEffect.get())->targetStyleable() : std::nullopt;
    std::optional<const Styleable> newTarget = is<KeyframeEffect>(m_effect) ? downcast<KeyframeEffect>(m_effect.get())->targetStyleable() : std::nullopt;

    // Update the effect-to-animation relationships and the timeline's animation map.
    if (oldEffect) {
        oldEffect->setAnimation(nullptr);
        if (!doNotRemoveFromTimeline && previousTarget && previousTarget != newTarget)
            previousTarget->animationWasRemoved(*this);
        updateRelevance();
    }

    if (m_effect) {
        m_effect->setAnimation(this);
        if (newTarget && previousTarget != newTarget)
            newTarget->animationWasAdded(*this);
    }

    InspectorInstrumentation::didSetWebAnimationEffect(*this);
}

void WebAnimation::setTimeline(RefPtr<AnimationTimeline>&& timeline)
{
    // 3.4.1. Setting the timeline of an animation
    // https://drafts.csswg.org/web-animations-1/#setting-the-timeline

    // 2. If new timeline is the same object as old timeline, abort this procedure.
    if (timeline == m_timeline)
        return;

    // 4. If the animation start time of animation is resolved, make animation's hold time unresolved.
    if (m_startTime)
        m_holdTime = std::nullopt;

    if (is<KeyframeEffect>(m_effect)) {
        if (auto target = downcast<KeyframeEffect>(m_effect.get())->targetStyleable()) {
            // In the case of a declarative animation, we don't want to remove the animation from the relevant maps because
            // while the timeline was set via the API, the element still has a transition or animation set up and we must
            // not break the relationship.
            if (!isDeclarativeAnimation())
                target->animationWasRemoved(*this);
            target->animationWasAdded(*this);
        }
    }

    // This object could be deleted after clearing the timeline relationship.
    auto protectedThis = makeRef(*this);
    setTimelineInternal(WTFMove(timeline));

    setSuspended(is<DocumentTimeline>(m_timeline) && downcast<DocumentTimeline>(*m_timeline).animationsAreSuspended());

    // 5. Run the procedure to update an animation's finished state for animation with the did seek flag set to false,
    // and the synchronously notify flag set to false.
    timingDidChange(DidSeek::No, SynchronouslyNotify::No);

    invalidateEffect();
}

void WebAnimation::setTimelineInternal(RefPtr<AnimationTimeline>&& timeline)
{
    if (m_timeline == timeline)
        return;

    if (m_timeline)
        m_timeline->removeAnimation(*this);

    m_timeline = WTFMove(timeline);

    if (m_effect)
        m_effect->animationTimelineDidChange(m_timeline.get());
}

void WebAnimation::effectTargetDidChange(const std::optional<const Styleable>& previousTarget, const std::optional<const Styleable>& newTarget)
{
    if (m_timeline) {
        if (previousTarget)
            previousTarget->animationWasRemoved(*this);

        if (newTarget)
            newTarget->animationWasAdded(*this);

        // This could have changed whether we have replaced animations, so we may need to schedule an update.
        m_timeline->animationTimingDidChange(*this);
    }

    InspectorInstrumentation::didChangeWebAnimationEffectTarget(*this);
}

std::optional<double> WebAnimation::bindingsStartTime() const
{
    if (!m_startTime)
        return std::nullopt;
    return secondsToWebAnimationsAPITime(*m_startTime);
}

void WebAnimation::setBindingsStartTime(std::optional<double> newStartTime)
{
    if (newStartTime)
        setStartTime(Seconds::fromMilliseconds(*newStartTime));
    else
        setStartTime(std::nullopt);
}

void WebAnimation::setStartTime(std::optional<Seconds> newStartTime)
{
    // 3.4.6 The procedure to set the start time of animation, animation, to new start time, is as follows:
    // https://drafts.csswg.org/web-animations/#setting-the-start-time-of-an-animation

    // 1. Let timeline time be the current time value of the timeline that animation is associated with. If
    //    there is no timeline associated with animation or the associated timeline is inactive, let the timeline
    //    time be unresolved.
    auto timelineTime = m_timeline ? m_timeline->currentTime() : std::nullopt;

    // 2. If timeline time is unresolved and new start time is resolved, make animation's hold time unresolved.
    if (!timelineTime && newStartTime)
        m_holdTime = std::nullopt;

    // 3. Let previous current time be animation's current time.
    auto previousCurrentTime = currentTime();

    // 4. Apply any pending playback rate on animation.
    applyPendingPlaybackRate();

    // 5. Set animation's start time to new start time.
    m_startTime = newStartTime;

    // 6. Update animation's hold time based on the first matching condition from the following,
    if (newStartTime) {
        // If new start time is resolved,
        // If animation's playback rate is not zero, make animation's hold time unresolved.
        if (m_playbackRate)
            m_holdTime = std::nullopt;
    } else {
        // Otherwise (new start time is unresolved),
        // Set animation's hold time to previous current time even if previous current time is unresolved.
        m_holdTime = previousCurrentTime;
    }

    // 7. If animation has a pending play task or a pending pause task, cancel that task and resolve animation's current ready promise with animation.
    if (pending()) {
        m_timeToRunPendingPauseTask = TimeToRunPendingTask::NotScheduled;
        m_timeToRunPendingPlayTask = TimeToRunPendingTask::NotScheduled;
        m_readyPromise->resolve(*this);
    }

    // 8. Run the procedure to update an animation's finished state for animation with the did seek flag set to true, and the synchronously notify flag set to false.
    timingDidChange(DidSeek::Yes, SynchronouslyNotify::No);

    invalidateEffect();
}

std::optional<double> WebAnimation::bindingsCurrentTime() const
{
    auto time = currentTime();
    if (!time)
        return std::nullopt;
    return secondsToWebAnimationsAPITime(time.value());
}

ExceptionOr<void> WebAnimation::setBindingsCurrentTime(std::optional<double> currentTime)
{
    if (!currentTime)
        return setCurrentTime(std::nullopt);
    return setCurrentTime(Seconds::fromMilliseconds(currentTime.value()));
}

std::optional<Seconds> WebAnimation::currentTime(std::optional<Seconds> startTime) const
{
    return currentTime(RespectHoldTime::Yes, startTime);
}

std::optional<Seconds> WebAnimation::currentTime(RespectHoldTime respectHoldTime, std::optional<Seconds> startTime) const
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
    return (*m_timeline->currentTime() - startTime.value_or(*m_startTime)) * m_playbackRate;
}

ExceptionOr<void> WebAnimation::silentlySetCurrentTime(std::optional<Seconds> seekTime)
{
    LOG_WITH_STREAM(Animations, stream << "WebAnimation " << this << " silentlySetCurrentTime " << seekTime);

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
    if (m_holdTime || !m_startTime || !m_timeline || !m_timeline->currentTime() || !m_playbackRate)
        m_holdTime = seekTime;
    else
        m_startTime = m_timeline->currentTime().value() - (seekTime.value() / m_playbackRate);

    // 3. If animation has no associated timeline or the associated timeline is inactive, make animation's start time unresolved.
    if (!m_timeline || !m_timeline->currentTime())
        m_startTime = std::nullopt;

    // 4. Make animation's previous current time unresolved.
    m_previousCurrentTime = std::nullopt;

    return { };
}

ExceptionOr<void> WebAnimation::setCurrentTime(std::optional<Seconds> seekTime)
{
    LOG_WITH_STREAM(Animations, stream << "WebAnimation " << this << " setCurrentTime " << seekTime);

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
        // 2. Apply any pending playback rate to animation.
        applyPendingPlaybackRate();
        // 3. Make animation's start time unresolved.
        m_startTime = std::nullopt;
        // 4. Cancel the pending pause task.
        m_timeToRunPendingPauseTask = TimeToRunPendingTask::NotScheduled;
        // 5. Resolve animation's current ready promise with animation.
        m_readyPromise->resolve(*this);
    }

    // 3. Run the procedure to update an animation's finished state for animation with the did seek flag set to true, and the synchronously notify flag set to false.
    timingDidChange(DidSeek::Yes, SynchronouslyNotify::No);

    if (m_effect)
        m_effect->animationDidChangeTimingProperties();

    invalidateEffect();

    return { };
}

double WebAnimation::effectivePlaybackRate() const
{
    // https://drafts.csswg.org/web-animations/#effective-playback-rate
    // The effective playback rate of an animation is its pending playback rate, if set, otherwise it is the animation's playback rate.
    return (m_pendingPlaybackRate ? m_pendingPlaybackRate.value() : m_playbackRate);
}

void WebAnimation::setPlaybackRate(double newPlaybackRate)
{
    // 3.4.17.1. Updating the playback rate of an animation
    // https://drafts.csswg.org/web-animations-1/#updating-the-playback-rate-of-an-animation

    // 1. Clear any pending playback rate on animation.
    m_pendingPlaybackRate = std::nullopt;
    
    // 2. Let previous time be the value of the current time of animation before changing the playback rate.
    auto previousTime = currentTime();

    // 3. Set the playback rate to new playback rate.
    m_playbackRate = newPlaybackRate;

    // 4. If previous time is resolved, set the current time of animation to previous time.
    if (previousTime)
        setCurrentTime(previousTime);

    if (m_effect)
        m_effect->animationDidChangeTimingProperties();
}

void WebAnimation::updatePlaybackRate(double newPlaybackRate)
{
    // https://drafts.csswg.org/web-animations/#seamlessly-update-the-playback-rate

    // The procedure to seamlessly update the playback rate an animation, animation, to new playback rate preserving its current time is as follows:

    // 1. Let previous play state be animation's play state.
    //    Note: It is necessary to record the play state before updating animation's effective playback rate since, in the following logic,
    //    we want to immediately apply the pending playback rate of animation if it is currently finished regardless of whether or not it will
    //    still be finished after we apply the pending playback rate.
    auto previousPlayState = playState();

    // 2. Let animation's pending playback rate be new playback rate.
    m_pendingPlaybackRate = newPlaybackRate;

    // 3. Perform the steps corresponding to the first matching condition from below:
    if (pending()) {
        // If animation has a pending play task or a pending pause task,
        // Abort these steps.
        // Note: The different types of pending tasks will apply the pending playback rate when they run so there is no further action required in this case.
        return;
    }

    if (previousPlayState == PlayState::Idle || previousPlayState == PlayState::Paused) {
        // If previous play state is idle or paused,
        // Apply any pending playback rate on animation.
        applyPendingPlaybackRate();
    } else if (previousPlayState == PlayState::Finished) {
        // If previous play state is finished,
        // 1. Let the unconstrained current time be the result of calculating the current time of animation substituting an unresolved time value for the hold time.
        auto unconstrainedCurrentTime = currentTime(RespectHoldTime::No);
        // 2. Let animation's start time be the result of evaluating the following expression:
        // timeline time - (unconstrained current time / pending playback rate)
        // Where timeline time is the current time value of the timeline associated with animation.
        // If pending playback rate is zero, let animation's start time be timeline time.
        
        // If timeline is inactive abort these steps.
        if (!m_timeline->currentTime())
            return;
        
        auto newStartTime = m_timeline->currentTime().value();
        if (m_pendingPlaybackRate)
            newStartTime -= (unconstrainedCurrentTime.value() / m_pendingPlaybackRate.value());
        m_startTime = newStartTime;
        // 3. Apply any pending playback rate on animation.
        applyPendingPlaybackRate();
        // 4. Run the procedure to update an animation's finished state for animation with the did seek flag set to false, and the synchronously notify flag set to false.
        timingDidChange(DidSeek::No, SynchronouslyNotify::No);

        invalidateEffect();
    } else {
        // Otherwise,
        // Run the procedure to play an animation for animation with the auto-rewind flag set to false.
        play(AutoRewind::No);
    }

    if (m_effect)
        m_effect->animationDidChangeTimingProperties();
}

void WebAnimation::applyPendingPlaybackRate()
{
    // https://drafts.csswg.org/web-animations/#apply-any-pending-playback-rate

    // 1. If animation does not have a pending playback rate, abort these steps.
    if (!m_pendingPlaybackRate)
        return;

    // 2. Set animation's playback rate to its pending playback rate.
    m_playbackRate = m_pendingPlaybackRate.value();

    // 3. Clear animation's pending playback rate.
    m_pendingPlaybackRate = std::nullopt;
}

auto WebAnimation::playState() const -> PlayState
{
    // 3.5.19 Play states
    // https://drafts.csswg.org/web-animations/#play-states

    // The play state of animation, animation, at a given moment is the state corresponding to the
    // first matching condition from the following:

    // The current time of animation is unresolved, and animation does not have either a pending
    // play task or a pending pause task,
    // → idle
    auto animationCurrentTime = currentTime();
    if (!animationCurrentTime && !pending())
        return PlayState::Idle;

    // Animation has a pending pause task, or both the start time of animation is unresolved and it does not
    // have a pending play task,
    // → paused
    if (hasPendingPauseTask() || (!m_startTime && !hasPendingPlayTask()))
        return PlayState::Paused;

    // For animation, current time is resolved and either of the following conditions are true:
    // animation's effective playback rate > 0 and current time ≥ target effect end; or
    // animation's effective playback rate < 0 and current time ≤ 0,
    // → finished
    if (animationCurrentTime && ((effectivePlaybackRate() > 0 && (*animationCurrentTime + timeEpsilon) >= effectEndTime()) || (effectivePlaybackRate() < 0 && (*animationCurrentTime - timeEpsilon) <= 0_s)))
        return PlayState::Finished;

    // Otherwise → running
    return PlayState::Running;
}

Seconds WebAnimation::effectEndTime() const
{
    // The target effect end of an animation is equal to the end time of the animation's target effect.
    // If the animation has no target effect, the target effect end is zero.
    return m_effect ? m_effect->endTime() : 0_s;
}

void WebAnimation::cancel()
{
    LOG_WITH_STREAM(Animations, stream << "WebAnimation " << this << " cancel() (current time is " << currentTime() << ")");

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
        // 3. Set the [[PromiseIsHandled]] internal slot of the current finished promise to true.
        if (!m_finishedPromise->isFulfilled())
            m_finishedPromise->reject(Exception { AbortError }, RejectAsHandled::Yes);

        // 4. Let current finished promise be a new (pending) Promise object.
        m_finishedPromise = makeUniqueRef<FinishedPromise>(*this, &WebAnimation::finishedPromiseResolve);

        // 5. Create an AnimationPlaybackEvent, cancelEvent.
        // 6. Set cancelEvent's type attribute to cancel.
        // 7. Set cancelEvent's currentTime to null.
        // 8. Let timeline time be the current time of the timeline with which animation is associated. If animation is not associated with an
        //    active timeline, let timeline time be n unresolved time value.
        // 9. Set cancelEvent's timelineTime to timeline time. If timeline time is unresolved, set it to null.
        // 10. If animation has a document for timing, then append cancelEvent to its document for timing's pending animation event queue along
        //    with its target, animation. If animation is associated with an active timeline that defines a procedure to convert timeline times
        //    to origin-relative time, let the scheduled event time be the result of applying that procedure to timeline time. Otherwise, the
        //    scheduled event time is an unresolved time value.
        // Otherwise, queue a task to dispatch cancelEvent at animation. The task source for this task is the DOM manipulation task source.
        enqueueAnimationPlaybackEvent(eventNames().cancelEvent, std::nullopt, m_timeline ? m_timeline->currentTime() : std::nullopt);
    }

    // 2. Make animation's hold time unresolved.
    m_holdTime = std::nullopt;

    // 3. Make animation's start time unresolved.
    m_startTime = std::nullopt;

    timingDidChange(DidSeek::No, SynchronouslyNotify::No);

    invalidateEffect();

    if (m_effect)
        m_effect->animationWasCanceled();
}

void WebAnimation::willChangeRenderer()
{
    if (is<KeyframeEffect>(m_effect))
        downcast<KeyframeEffect>(*m_effect).willChangeRenderer();
}

void WebAnimation::enqueueAnimationPlaybackEvent(const AtomString& type, std::optional<Seconds> currentTime, std::optional<Seconds> timelineTime)
{
    auto event = AnimationPlaybackEvent::create(type, currentTime, timelineTime, this);
    event->setTarget(this);
    enqueueAnimationEvent(WTFMove(event));
}

void WebAnimation::enqueueAnimationEvent(Ref<AnimationEventBase>&& event)
{
    if (is<DocumentTimeline>(m_timeline)) {
        // If animation has a document for timing, then append event to its document for timing's pending animation event queue along
        // with its target, animation. If animation is associated with an active timeline that defines a procedure to convert timeline times
        // to origin-relative time, let the scheduled event time be the result of applying that procedure to timeline time. Otherwise, the
        // scheduled event time is an unresolved time value.
        m_hasScheduledEventsDuringTick = true;
        downcast<DocumentTimeline>(*m_timeline).enqueueAnimationEvent(WTFMove(event));
    } else {
        // Otherwise, queue a task to dispatch event at animation. The task source for this task is the DOM manipulation task source.
        queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, WTFMove(event));
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
        m_timeToRunPendingPlayTask = TimeToRunPendingTask::NotScheduled;

    // 3. If animation has a pending pause task, cancel that task.
    if (hasPendingPauseTask())
        m_timeToRunPendingPauseTask = TimeToRunPendingTask::NotScheduled;

    // 4. Apply any pending playback rate on animation.
    applyPendingPlaybackRate();

    // 5. Reject animation's current ready promise with a DOMException named "AbortError".
    // 6. Set the [[PromiseIsHandled]] internal slot of animation’s current ready promise to true.
    m_readyPromise->reject(Exception { AbortError }, RejectAsHandled::Yes);

    // 7. Let animation's current ready promise be the result of creating a new resolved Promise object.
    m_readyPromise = makeUniqueRef<ReadyPromise>(*this, &WebAnimation::readyPromiseResolve);
    m_readyPromise->resolve(*this);
}

ExceptionOr<void> WebAnimation::finish()
{
    LOG_WITH_STREAM(Animations, stream << "WebAnimation " << this << " finish (current time is " << currentTime() << ")");

    // 3.4.15. Finishing an animation
    // https://drafts.csswg.org/web-animations-1/#finishing-an-animation-section

    // An animation can be advanced to the natural end of its current playback direction by using the procedure to finish an animation for animation defined below:
    //
    // 1. If animation's effective playback rate is zero, or if animation's effective playback rate > 0 and target effect end is infinity, throw an InvalidStateError and abort these steps.
    if (!effectivePlaybackRate() || (effectivePlaybackRate() > 0 && effectEndTime() == Seconds::infinity()))
        return Exception { InvalidStateError };

    // 2. Apply any pending playback rate to animation.
    applyPendingPlaybackRate();

    // 3. Set limit as follows:
    // If animation playback rate > 0, let limit be target effect end.
    // Otherwise, let limit be zero.
    auto limit = m_playbackRate > 0 ? effectEndTime() : 0_s;

    // 4. Silently set the current time to limit.
    silentlySetCurrentTime(limit);

    // 5. If animation's start time is unresolved and animation has an associated active timeline, let the start time be the result of
    //    evaluating timeline time - (limit / playback rate) where timeline time is the current time value of the associated timeline.
    if (!m_startTime && m_timeline && m_timeline->currentTime())
        m_startTime = m_timeline->currentTime().value() - (limit / m_playbackRate);

    // 6. If there is a pending pause task and start time is resolved,
    if (hasPendingPauseTask() && m_startTime) {
        // 1. Let the hold time be unresolved.
        m_holdTime = std::nullopt;
        // 2. Cancel the pending pause task.
        m_timeToRunPendingPauseTask = TimeToRunPendingTask::NotScheduled;
        // 3. Resolve the current ready promise of animation with animation.
        m_readyPromise->resolve(*this);
    }

    // 7. If there is a pending play task and start time is resolved, cancel that task and resolve the current ready promise of animation with animation.
    if (hasPendingPlayTask() && m_startTime) {
        m_timeToRunPendingPlayTask = TimeToRunPendingTask::NotScheduled;
        m_readyPromise->resolve(*this);
    }

    // 8. Run the procedure to update an animation's finished state animation with the did seek flag set to true, and the synchronously notify flag set to true.
    timingDidChange(DidSeek::Yes, SynchronouslyNotify::Yes);

    invalidateEffect();

    return { };
}

void WebAnimation::timingDidChange(DidSeek didSeek, SynchronouslyNotify synchronouslyNotify, Silently silently)
{
    m_shouldSkipUpdatingFinishedStateWhenResolving = false;
    updateFinishedState(didSeek, synchronouslyNotify);

    if (is<KeyframeEffect>(m_effect)) {
        updateRelevance();
        downcast<KeyframeEffect>(*m_effect).animationTimingDidChange();
    }

    if (silently == Silently::No && m_timeline)
        m_timeline->animationTimingDidChange(*this);
};

void WebAnimation::invalidateEffect()
{
    if (!isEffectInvalidationSuspended() && m_effect)
        m_effect->invalidate();
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
    if (unconstrainedCurrentTime && m_startTime && !pending()) {
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
                m_startTime = m_timeline->currentTime().value() - (m_holdTime.value() / m_playbackRate);
            // 2. Let the hold time be unresolved.
            m_holdTime = std::nullopt;
        }
    }

    // 3. Set the previous current time of animation be the result of calculating its current time.
    m_previousCurrentTime = currentTime();

    // 4. Let current finished state be true if the play state of animation is finished. Otherwise, let it be false.
    auto currentFinishedState = playState() == PlayState::Finished;

    // 5. If current finished state is true and the current finished promise is not yet resolved, perform the following steps:
    if (currentFinishedState && !m_finishedPromise->isFulfilled()) {
        if (synchronouslyNotify == SynchronouslyNotify::Yes) {
            // If synchronously notify is true, cancel any queued microtask to run the finish notification steps for this animation,
            // and run the finish notification steps immediately.
            m_finishNotificationStepsMicrotaskPending = false;
            finishNotificationSteps();
        } else if (!m_finishNotificationStepsMicrotaskPending) {
            // Otherwise, if synchronously notify is false, queue a microtask to run finish notification steps for animation unless there
            // is already a microtask queued to run those steps for animation.
            m_finishNotificationStepsMicrotaskPending = true;
            if (auto* context = scriptExecutionContext()) {
                context->eventLoop().queueMicrotask([this, protectedThis = makeRef(*this)] {
                    if (m_finishNotificationStepsMicrotaskPending) {
                        m_finishNotificationStepsMicrotaskPending = false;
                        finishNotificationSteps();
                    }
                });
            }
        }
    }

    // 6. If current finished state is false and animation's current finished promise is already resolved, set animation's current
    // finished promise to a new (pending) Promise object.
    if (!currentFinishedState && m_finishedPromise->isFulfilled())
        m_finishedPromise = makeUniqueRef<FinishedPromise>(*this, &WebAnimation::finishedPromiseResolve);

    updateRelevance();
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
    m_finishedPromise->resolve(*this);

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

    if (is<KeyframeEffect>(m_effect)) {
        if (auto target = makeRefPtr(downcast<KeyframeEffect>(*m_effect).target())) {
            if (auto* page = target->document().page())
                page->chrome().client().animationDidFinishForElement(*target);
        }
    }
}

ExceptionOr<void> WebAnimation::play()
{
    return play(AutoRewind::Yes);
}

ExceptionOr<void> WebAnimation::play(AutoRewind autoRewind)
{
    LOG_WITH_STREAM(Animations, stream << "WebAnimation " << this << " play(autoRewind " << (autoRewind == AutoRewind::Yes) << ") (current time is " << currentTime() << ")");

    // 3.4.10. Playing an animation
    // https://drafts.csswg.org/web-animations-1/#play-an-animation

    auto localTime = currentTime();
    auto endTime = effectEndTime();

    // 1. Let aborted pause be a boolean flag that is true if animation has a pending pause task, and false otherwise.
    bool abortedPause = hasPendingPauseTask();

    // 2. Let has pending ready promise be a boolean flag that is initially false.
    bool hasPendingReadyPromise = false;

    // 3. Perform the steps corresponding to the first matching condition from the following, if any:
    if (effectivePlaybackRate() > 0 && autoRewind == AutoRewind::Yes && (!localTime || localTime.value() < 0_s || (localTime.value() + timeEpsilon) >= endTime)) {
        // If animation's effective playback rate > 0, the auto-rewind flag is true and either animation's:
        //     - current time is unresolved, or
        //     - current time < zero, or
        //     - current time ≥ target effect end,
        // Set animation's hold time to zero.
        m_holdTime = 0_s;
    } else if (effectivePlaybackRate() < 0 && autoRewind == AutoRewind::Yes && (!localTime || localTime.value() <= 0_s || localTime.value() > endTime)) {
        // If animation's effective playback rate < 0, the auto-rewind flag is true and either animation's:
        //     - current time is unresolved, or
        //     - current time ≤ zero, or
        //     - current time > target effect end
        // If target effect end is positive infinity, throw an InvalidStateError and abort these steps.
        if (endTime == Seconds::infinity())
            return Exception { InvalidStateError };
        m_holdTime = endTime;
    } else if (!effectivePlaybackRate() && !localTime) {
        // If animation's effective playback rate = 0 and animation's current time is unresolved,
        // Set animation's hold time to zero.
        m_holdTime = 0_s;
    }

    // 4. If animation has a pending play task or a pending pause task,
    if (pending()) {
        // 1. Cancel that task.
        m_timeToRunPendingPauseTask = TimeToRunPendingTask::NotScheduled;
        m_timeToRunPendingPlayTask = TimeToRunPendingTask::NotScheduled;
        // 2. Set has pending ready promise to true.
        hasPendingReadyPromise = true;
    }

    // 5. If the following three conditions are all satisfied:
    //    - animation's hold time is unresolved, and
    //    - aborted pause is false, and
    //    - animation does not have a pending playback rate,
    // abort this procedure.
    if (!m_holdTime && !abortedPause && !m_pendingPlaybackRate)
        return { };

    // 6. If animation's hold time is resolved, let its start time be unresolved.
    if (m_holdTime)
        m_startTime = std::nullopt;

    // 7. If has pending ready promise is false, let animation's current ready promise be a new (pending) Promise object.
    if (!hasPendingReadyPromise)
        m_readyPromise = makeUniqueRef<ReadyPromise>(*this, &WebAnimation::readyPromiseResolve);

    // 8. Schedule a task to run as soon as animation is ready.
    m_timeToRunPendingPlayTask = TimeToRunPendingTask::WhenReady;

    // 9. Run the procedure to update an animation's finished state for animation with the did seek flag set to false, and the synchronously notify flag set to false.
    timingDidChange(DidSeek::No, SynchronouslyNotify::No);

    invalidateEffect();

    if (m_effect)
        m_effect->animationDidPlay();

    return { };
}

void WebAnimation::runPendingPlayTask()
{
    LOG_WITH_STREAM(Animations, stream << "WebAnimation " << this << " runPendingPlayTask (current time is " << currentTime() << ")");

    // 3.4.10. Playing an animation, step 8.
    // https://drafts.csswg.org/web-animations-1/#play-an-animation

    m_timeToRunPendingPlayTask = TimeToRunPendingTask::NotScheduled;

    // 1. Assert that at least one of animation's start time or hold time is resolved.
    ASSERT(m_startTime || m_holdTime);

    // 2. Let ready time be the time value of the timeline associated with animation at the moment when animation became ready.
    auto readyTime = m_timeline->currentTime();

    // 3. Perform the steps corresponding to the first matching condition below, if any:
    if (m_holdTime) {
        // If animation's hold time is resolved,
        // 1. Apply any pending playback rate on animation.
        applyPendingPlaybackRate();
        // 2. Let new start time be the result of evaluating ready time - hold time / animation playback rate for animation.
        // If the animation playback rate is zero, let new start time be simply ready time.
        // FIXME: Implementation cannot guarantee an active timeline at the point of this async dispatch.
        // Subsequently, the resulting readyTime value can be null. Unify behavior between C++17 and
        // C++14 builds (the latter using WTF's Optional) and avoid null Optional dereferencing
        // by defaulting to a Seconds(0) value. See https://bugs.webkit.org/show_bug.cgi?id=186189.
        auto newStartTime = readyTime.value_or(0_s);
        if (m_playbackRate)
            newStartTime -= m_holdTime.value() / m_playbackRate;
        // 3. Set the start time of animation to new start time.
        m_startTime = newStartTime;
        // 4. If animation's playback rate is not 0, make animation's hold time unresolved.
        if (m_playbackRate)
            m_holdTime = std::nullopt;
    } else if (m_startTime && m_pendingPlaybackRate) {
        // If animation's start time is resolved and animation has a pending playback rate,
        // 1. Let current time to match be the result of evaluating (ready time - start time) × playback rate for animation.
        auto currentTimeToMatch = (readyTime.value_or(0_s) - m_startTime.value()) * m_playbackRate;
        // 2. Apply any pending playback rate on animation.
        applyPendingPlaybackRate();
        // 3. If animation's playback rate is zero, let animation's hold time be current time to match.
        if (m_playbackRate)
            m_holdTime = currentTimeToMatch;
        // 4. Let new start time be the result of evaluating ready time - current time to match / playback rate for animation.
        // If the playback rate is zero, let new start time be simply ready time.
        auto newStartTime = readyTime.value_or(0_s);
        if (m_playbackRate)
            newStartTime -= currentTimeToMatch / m_playbackRate;
        // 5. Set the start time of animation to new start time.
        m_startTime = newStartTime;
    }

    // 4. Resolve animation's current ready promise with animation.
    if (!m_readyPromise->isFulfilled())
        m_readyPromise->resolve(*this);

    // 5. Run the procedure to update an animation's finished state for animation with the did seek flag set to false, and the synchronously notify flag set to false.
    timingDidChange(DidSeek::No, SynchronouslyNotify::No, Silently::Yes);

    invalidateEffect();
}

ExceptionOr<void> WebAnimation::pause()
{
    LOG_WITH_STREAM(Animations, stream << "WebAnimation " << this << " pause (current time is " << currentTime() << ")");

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
        m_timeToRunPendingPlayTask = TimeToRunPendingTask::NotScheduled;
        hasPendingReadyPromise = true;
    }

    // 6. If has pending ready promise is false, set animation's current ready promise to a new (pending) Promise object.
    if (!hasPendingReadyPromise)
        m_readyPromise = makeUniqueRef<ReadyPromise>(*this, &WebAnimation::readyPromiseResolve);

    // 7. Schedule a task to be executed at the first possible moment after the user agent has performed any processing necessary
    //    to suspend the playback of animation's target effect, if any.
    m_timeToRunPendingPauseTask = TimeToRunPendingTask::ASAP;

    // 8. Run the procedure to update an animation's finished state for animation with the did seek flag set to false, and the synchronously notify flag set to false.
    timingDidChange(DidSeek::No, SynchronouslyNotify::No);

    invalidateEffect();

    return { };
}

ExceptionOr<void> WebAnimation::bindingsReverse()
{
    return reverse();
}

ExceptionOr<void> WebAnimation::reverse()
{
    LOG_WITH_STREAM(Animations, stream << "WebAnimation " << this << " reverse (current time is " << currentTime() << ")");

    // 3.4.18. Reversing an animation
    // https://drafts.csswg.org/web-animations-1/#reverse-an-animation

    // The procedure to reverse an animation of animation animation is as follows:

    // 1. If there is no timeline associated with animation, or the associated timeline is inactive
    //    throw an InvalidStateError and abort these steps.
    if (!m_timeline || !m_timeline->currentTime())
        return Exception { InvalidStateError };

    // 2. Let original pending playback rate be animation's pending playback rate.
    auto originalPendingPlaybackRate = m_pendingPlaybackRate;

    // 3. Let animation's pending playback rate be the additive inverse of its effective playback rate (i.e. -effective playback rate).
    m_pendingPlaybackRate = -effectivePlaybackRate();

    // 4. Run the steps to play an animation for animation with the auto-rewind flag set to true.
    auto playResult = play(AutoRewind::Yes);

    // If the steps to play an animation throw an exception, set animation's pending playback rate to original
    // pending playback rate and propagate the exception.
    if (playResult.hasException()) {
        m_pendingPlaybackRate = originalPendingPlaybackRate;
        return playResult.releaseException();
    }

    if (m_effect)
        m_effect->animationDidChangeTimingProperties();

    return { };
}

void WebAnimation::runPendingPauseTask()
{
    LOG_WITH_STREAM(Animations, stream << "WebAnimation " << this << " runPendingPauseTask (current time is " << currentTime() << ")");

    // 3.4.11. Pausing an animation, step 7.
    // https://drafts.csswg.org/web-animations-1/#pause-an-animation

    m_timeToRunPendingPauseTask = TimeToRunPendingTask::NotScheduled;

    // 1. Let ready time be the time value of the timeline associated with animation at the moment when the user agent
    //    completed processing necessary to suspend playback of animation's target effect.
    auto readyTime = m_timeline->currentTime();
    auto animationStartTime = m_startTime;

    // 2. If animation's start time is resolved and its hold time is not resolved, let animation's hold time be the result of
    //    evaluating (ready time - start time) × playback rate.
    //    Note: The hold time might be already set if the animation is finished, or if the animation is pending, waiting to begin
    //    playback. In either case we want to preserve the hold time as we enter the paused state.
    if (animationStartTime && !m_holdTime) {
        // FIXME: Implementation cannot guarantee an active timeline at the point of this async dispatch.
        // Subsequently, the resulting readyTime value can be null. Unify behavior between C++17 and
        // C++14 builds (the latter using WTF's Optional) and avoid null Optional dereferencing
        // by defaulting to a Seconds(0) value. See https://bugs.webkit.org/show_bug.cgi?id=186189.
        m_holdTime = (readyTime.value_or(0_s) - animationStartTime.value()) * m_playbackRate;
    }

    // 3. Apply any pending playback rate on animation.
    applyPendingPlaybackRate();

    // 4. Make animation's start time unresolved.
    m_startTime = std::nullopt;

    // 5. Resolve animation's current ready promise with animation.
    if (!m_readyPromise->isFulfilled())
        m_readyPromise->resolve(*this);

    // 6. Run the procedure to update an animation's finished state for animation with the did seek flag set to false, and the
    //    synchronously notify flag set to false.
    timingDidChange(DidSeek::No, SynchronouslyNotify::No, Silently::Yes);

    invalidateEffect();
}

bool WebAnimation::isRunningAccelerated() const
{
    return is<KeyframeEffect>(m_effect) && downcast<KeyframeEffect>(*m_effect).isRunningAccelerated();
}

bool WebAnimation::needsTick() const
{
    return pending() || playState() == PlayState::Running || m_hasScheduledEventsDuringTick;
}

void WebAnimation::tick()
{
    m_hasScheduledEventsDuringTick = false;
    updateFinishedState(DidSeek::No, SynchronouslyNotify::Yes);
    m_shouldSkipUpdatingFinishedStateWhenResolving = true;

    // Run pending tasks, if any.
    if (hasPendingPauseTask())
        runPendingPauseTask();
    if (hasPendingPlayTask())
        runPendingPlayTask();

    if (!isEffectInvalidationSuspended() && m_effect)
        m_effect->animationDidTick();
}

void WebAnimation::resolve(RenderStyle& targetStyle, const RenderStyle* parentElementStyle, std::optional<Seconds> startTime)
{
    if (!m_shouldSkipUpdatingFinishedStateWhenResolving)
        updateFinishedState(DidSeek::No, SynchronouslyNotify::Yes);
    m_shouldSkipUpdatingFinishedStateWhenResolving = false;

    if (m_effect)
        m_effect->apply(targetStyle, parentElementStyle, startTime);
}

void WebAnimation::setSuspended(bool isSuspended)
{
    if (m_isSuspended == isSuspended)
        return;

    m_isSuspended = isSuspended;

    if (m_effect && playState() == PlayState::Running)
        m_effect->animationSuspensionStateDidChange(isSuspended);
}

void WebAnimation::acceleratedStateDidChange()
{
    if (is<DocumentTimeline>(m_timeline))
        downcast<DocumentTimeline>(*m_timeline).animationAcceleratedRunningStateDidChange(*this);
}

WebAnimation& WebAnimation::readyPromiseResolve()
{
    return *this;
}

WebAnimation& WebAnimation::finishedPromiseResolve()
{
    return *this;
}

const char* WebAnimation::activeDOMObjectName() const
{
    return "Animation";
}

void WebAnimation::suspend(ReasonForSuspension)
{
    setSuspended(true);
}

void WebAnimation::resume()
{
    setSuspended(false);
}

void WebAnimation::stop()
{
    ActiveDOMObject::stop();
    removeAllEventListeners();
}

bool WebAnimation::virtualHasPendingActivity() const
{
    // Keep the JS wrapper alive if the animation is considered relevant or could become relevant again by virtue of having a timeline.
    return m_timeline || m_isRelevant;
}

void WebAnimation::updateRelevance()
{
    m_isRelevant = computeRelevance();
}

bool WebAnimation::computeRelevance()
{
    // To be listed in getAnimations() an animation needs a target effect which is current or in effect.
    if (!m_effect)
        return false;

    if (m_replaceState == ReplaceState::Removed)
        return false;

    auto timing = m_effect->getBasicTiming();

    // An animation effect is in effect if its active time is not unresolved.
    if (timing.activeTime)
        return true;

    // An animation effect is current if either of the following conditions is true:
    // - the animation effect is in the before phase, or
    // - the animation effect is in play.

    // An animation effect is in play if all of the following conditions are met:
    // - the animation effect is in the active phase, and
    // - the animation effect is associated with an animation that is not finished.
    return timing.phase == AnimationEffectPhase::Before || (timing.phase == AnimationEffectPhase::Active && playState() != PlayState::Finished);
}

bool WebAnimation::isReplaceable() const
{
    // An animation is replaceable if all of the following conditions are true:
    // https://drafts.csswg.org/web-animations/#removing-replaced-animations

    // The existence of the animation is not prescribed by markup. That is, it is not a CSS animation with an owning element,
    // nor a CSS transition with an owning element.
    if (isDeclarativeAnimation() && downcast<DeclarativeAnimation>(this)->owningElement())
        return false;

    // The animation's play state is finished.
    if (playState() != PlayState::Finished)
        return false;

    // The animation's replace state is not removed.
    if (m_replaceState == ReplaceState::Removed)
        return false;

    // The animation is associated with a monotonically increasing timeline.
    if (!m_timeline)
        return false;

    // The animation has an associated target effect.
    if (!m_effect)
        return false;

    // The target effect associated with the animation is in effect.
    if (!m_effect->getBasicTiming().activeTime)
        return false;

    // The target effect has an associated target element.
    if (!is<KeyframeEffect>(m_effect) || !downcast<KeyframeEffect>(m_effect.get())->target())
        return false;

    return true;
}

void WebAnimation::persist()
{
    auto previousReplaceState = std::exchange(m_replaceState, ReplaceState::Persisted);

    if (previousReplaceState == ReplaceState::Removed && m_timeline) {
        if (is<KeyframeEffect>(m_effect)) {
            auto& keyframeEffect = downcast<KeyframeEffect>(*m_effect);
            auto styleable = keyframeEffect.targetStyleable();
            styleable->animationWasAdded(*this);
            styleable->ensureKeyframeEffectStack().addEffect(keyframeEffect);
        }
    }
}

ExceptionOr<void> WebAnimation::commitStyles()
{
    // https://drafts.csswg.org/web-animations-1/#commit-computed-styles

    // 1. Let targets be the set of all effect targets for animation effects associated with animation.
    auto* effect = is<KeyframeEffect>(m_effect) ? downcast<KeyframeEffect>(m_effect.get()) : nullptr;
    auto* target = effect ? effect->target() : nullptr;

    // 2. For each target in targets:
    //
    // 2.1 If target is not an element capable of having a style attribute (for example, it is a pseudo-element or is an element in a
    // document format for which style attributes are not defined) throw a "NoModificationAllowedError" DOMException and abort these steps.
    if (!is<StyledElement>(target) || effect->targetsPseudoElement())
        return Exception { NoModificationAllowedError };

    auto& styledElement = downcast<StyledElement>(*target);

    // 2.2 If, after applying any pending style changes, target is not being rendered, throw an "InvalidStateError" DOMException and abort these steps.
    styledElement.document().updateStyleIfNeeded();
    auto* renderer = styledElement.renderer();
    if (!renderer)
        return Exception { InvalidStateError };

    // 2.3 Let inline style be the result of getting the CSS declaration block corresponding to target’s style attribute. If target does not have a style
    // attribute, let inline style be a new empty CSS declaration block with the readonly flag unset and owner node set to target.

    // 2.4 Let targeted properties be the set of physical longhand properties that are a target property for at least one animation effect associated with
    // animation whose effect target is target.

    auto& style = renderer->style();
    auto computedStyleExtractor = ComputedStyleExtractor(&styledElement);
    auto inlineStyle = styledElement.document().createCSSStyleDeclaration();
    inlineStyle->setCssText(styledElement.getAttribute("style"));

    auto& keyframeStack = styledElement.ensureKeyframeEffectStack(PseudoId::None);

    // 2.5 For each property, property, in targeted properties:
    for (auto property : effect->animatedProperties()) {
        // 1. Let partialEffectStack be a copy of the effect stack for property on target.
        // 2. If animation's replace state is removed, add all animation effects associated with animation whose effect target is target and which include
        // property as a target property to partialEffectStack.
        // 3. Remove from partialEffectStack any animation effects whose associated animation has a higher composite order than animation.
        // 4. Let effect value be the result of calculating the result of partialEffectStack for property using target's computed style (see § 5.4.3 Calculating
        // the result of an effect stack).
        // 5. Set a CSS declaration property for effect value in inline style.
        // 6. Update style attribute for inline style.

        // We actually perform those steps in a different way: instead of building a copy of the effect stack and then removing stuff, we iterate through the
        // effect stack and stop when we've found this animation's effect or when we've found an effect associated with an animation with a higher composite order.
        auto animatedStyle = RenderStyle::clonePtr(style);
        for (const auto& effectInStack : keyframeStack.sortedEffects()) {
            if (effectInStack->animation() != this && !compareAnimationsByCompositeOrder(*effectInStack->animation(), *this))
                break;
            if (effectInStack->animatedProperties().contains(property))
                effectInStack->animation()->resolve(*animatedStyle, nullptr);
            if (effectInStack->animation() == this)
                break;
        }
        if (m_replaceState == ReplaceState::Removed)
            effect->animation()->resolve(*animatedStyle, nullptr);
        if (auto cssValue = computedStyleExtractor.valueForPropertyInStyle(*animatedStyle, property, renderer))
            inlineStyle->setPropertyInternal(property, cssValue->cssText(), false);
    }

    styledElement.setAttribute("style", inlineStyle->cssText());

    return { };
}

Seconds WebAnimation::timeToNextTick() const
{
    if (pending())
        return 0_s;

    // If we're not running, or time is not advancing for this animation, there's no telling when we'll end.
    auto playbackRate = effectivePlaybackRate();
    if (playState() != PlayState::Running || !playbackRate)
        return Seconds::infinity();

    ASSERT(effect());
    return effect()->timeToNextTick() / playbackRate;
}

} // namespace WebCore
