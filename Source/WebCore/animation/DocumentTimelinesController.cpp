/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DocumentTimelinesController.h"

#include "AnimationEventBase.h"
#include "CSSTransition.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentTimeline.h"
#include "EventLoop.h"
#include "Logging.h"
#include "Page.h"
#include "Settings.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"
#include <JavaScriptCore/VM.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

DocumentTimelinesController::DocumentTimelinesController(Document& document)
    : m_document(document)
{
    if (auto* page = document.page()) {
        if (page->settings().hiddenPageCSSAnimationSuspensionEnabled() && !page->isVisible())
            suspendAnimations();
    }
}

DocumentTimelinesController::~DocumentTimelinesController() = default;

void DocumentTimelinesController::addTimeline(DocumentTimeline& timeline)
{
    m_timelines.add(timeline);

    if (m_isSuspended)
        timeline.suspendAnimations();
    else
        timeline.resumeAnimations();
}

void DocumentTimelinesController::removeTimeline(DocumentTimeline& timeline)
{
    m_timelines.remove(timeline);
}

void DocumentTimelinesController::detachFromDocument()
{
    m_currentTimeClearingTaskCancellationGroup.cancel();

    while (!m_timelines.computesEmpty())
        m_timelines.begin()->detachFromDocument();
}

void DocumentTimelinesController::updateAnimationsAndSendEvents(ReducedResolutionSeconds timestamp)
{
    LOG_WITH_STREAM(Animations, stream << "DocumentTimelinesController::updateAnimationsAndSendEvents for time " << timestamp);

    ASSERT(!m_timelines.hasNullReferences());

    // We need to copy m_timelines before iterating over its members since the steps in this procedure may mutate m_timelines.
    Vector<Ref<DocumentTimeline>> protectedTimelines;
    for (auto& timeline : m_timelines)
        protectedTimelines.append(timeline);

    // We need to freeze the current time even if no animation is running.
    // document.timeline.currentTime may be called from a rAF callback and
    // it has to match the rAF timestamp.
    if (!m_isSuspended)
        cacheCurrentTime(timestamp);

    // 1. Update the current time of all timelines associated with document passing now as the timestamp.
    Vector<Ref<DocumentTimeline>> timelinesToUpdate;
    Vector<Ref<WebAnimation>> animationsToRemove;
    Vector<Ref<CSSTransition>> completedTransitions;
    for (auto& timeline : protectedTimelines) {
        auto shouldUpdateAnimationsAndSendEvents = timeline->documentWillUpdateAnimationsAndSendEvents();
        if (shouldUpdateAnimationsAndSendEvents == DocumentTimeline::ShouldUpdateAnimationsAndSendEvents::No)
            continue;

        timelinesToUpdate.append(timeline.copyRef());

        for (auto& animation : copyToVector(timeline->relevantAnimations())) {
            if (animation->timeline() != timeline.ptr()) {
                ASSERT(!animation->timeline());
                continue;
            }

            // This will notify the animation that timing has changed and will call automatically
            // schedule invalidation if required for this animation.
            animation->tick();

            if (!animation->isRelevant() && !animation->needsTick())
                animationsToRemove.append(*animation);

            if (is<CSSTransition>(*animation)) {
                auto& transition = downcast<CSSTransition>(*animation);
                if (!transition.needsTick() && transition.playState() == WebAnimation::PlayState::Finished && transition.owningElement())
                    completedTransitions.append(transition);
            }
        }
    }

    if (timelinesToUpdate.isEmpty())
        return;

    // 2. Remove replaced animations for document.
    for (auto& timeline : protectedTimelines)
        timeline->removeReplacedAnimations();

    // 3. Perform a microtask checkpoint.
    makeRef(m_document)->eventLoop().performMicrotaskCheckpoint();

    // 4. Let events to dispatch be a copy of doc's pending animation event queue.
    // 5. Clear doc's pending animation event queue.
    AnimationEvents events;
    for (auto& timeline : timelinesToUpdate)
        events.appendVector(timeline->prepareForPendingAnimationEventsDispatch());

    // 6. Perform a stable sort of the animation events in events to dispatch as follows.
    std::stable_sort(events.begin(), events.end(), [] (const Ref<AnimationEventBase>& lhs, const Ref<AnimationEventBase>& rhs) {
        // 1. Sort the events by their scheduled event time such that events that were scheduled to occur earlier, sort before events scheduled to occur later
        // and events whose scheduled event time is unresolved sort before events with a resolved scheduled event time.
        // 2. Within events with equal scheduled event times, sort by their composite order. FIXME: Need to do this.
        return lhs->timelineTime() < rhs->timelineTime();
    });

    // 7. Dispatch each of the events in events to dispatch at their corresponding target using the order established in the previous step.
    for (auto& event : events)
        event->target()->dispatchEvent(event);

    // This will cancel any scheduled invalidation if we end up removing all animations.
    for (auto& animation : animationsToRemove) {
        // An animation that was initially marked as irrelevant may have changed while
        // we were sending events, so redo the the check for whether it should be removed.
        if (auto timeline = animation->timeline()) {
            if (!animation->isRelevant() && !animation->needsTick())
                timeline->removeAnimation(animation);
        }
    }

    // Now that animations that needed removal have been removed, update the list of completed transitions.
    // This needs to happen after dealing with the list of animations to remove as the animation may have been
    // removed from the list of completed transitions otherwise.
    for (auto& completedTransition : completedTransitions) {
        if (auto timeline = completedTransition->timeline())
            downcast<DocumentTimeline>(*timeline).transitionDidComplete(WTFMove(completedTransition));
    }

    for (auto& timeline : timelinesToUpdate)
        timeline->documentDidUpdateAnimationsAndSendEvents();
}

void DocumentTimelinesController::suspendAnimations()
{
    if (m_isSuspended)
        return;

    if (!m_cachedCurrentTime)
        m_cachedCurrentTime = liveCurrentTime();

    for (auto& timeline : m_timelines)
        timeline.suspendAnimations();

    m_isSuspended = true;
}

void DocumentTimelinesController::resumeAnimations()
{
    if (!m_isSuspended)
        return;

    m_cachedCurrentTime = std::nullopt;

    m_isSuspended = false;

    for (auto& timeline : m_timelines)
        timeline.resumeAnimations();
}

bool DocumentTimelinesController::animationsAreSuspended() const
{
    return m_isSuspended;
}

ReducedResolutionSeconds DocumentTimelinesController::liveCurrentTime() const
{
    return m_document.domWindow()->nowTimestamp();
}

std::optional<Seconds> DocumentTimelinesController::currentTime()
{
    if (!m_document.domWindow())
        return std::nullopt;

    if (!m_cachedCurrentTime)
        cacheCurrentTime(liveCurrentTime());

    return *m_cachedCurrentTime;
}

void DocumentTimelinesController::cacheCurrentTime(ReducedResolutionSeconds newCurrentTime)
{
    m_cachedCurrentTime = newCurrentTime;
    // We want to be sure to keep this time cached until we've both finished running JS and finished updating
    // animations, so we schedule the invalidation task and register a whenIdle callback on the VM, which will
    // fire syncronously if no JS is running.
    m_waitingOnVMIdle = true;
    if (!m_currentTimeClearingTaskCancellationGroup.hasPendingTask()) {
        CancellableTask task(m_currentTimeClearingTaskCancellationGroup, std::bind(&DocumentTimelinesController::maybeClearCachedCurrentTime, this));
        m_document.eventLoop().queueTask(TaskSource::InternalAsyncTask, WTFMove(task));
    }
    // We extent the associated Document's lifecycle until the VM became idle since the DocumentTimelinesController
    // is owned by the Document.
    m_document.vm().whenIdle([this, protectedDocument = makeRefPtr(m_document)]() {
        m_waitingOnVMIdle = false;
        maybeClearCachedCurrentTime();
    });
}

void DocumentTimelinesController::maybeClearCachedCurrentTime()
{
    // We want to make sure we only clear the cached current time if we're not currently running
    // JS or waiting on all current animation updating code to have completed. This is so that
    // we're guaranteed to have a consistent current time reported for all work happening in a given
    // JS frame or throughout updating animations in WebCore.
    if (!m_isSuspended && !m_waitingOnVMIdle && !m_currentTimeClearingTaskCancellationGroup.hasPendingTask())
        m_cachedCurrentTime = std::nullopt;
}

} // namespace WebCore

