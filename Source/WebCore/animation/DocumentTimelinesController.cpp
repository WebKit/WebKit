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
#include "Document.h"
#include "DocumentTimeline.h"
#include "EventLoop.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"

namespace WebCore {

DocumentTimelinesController::DocumentTimelinesController(Document& document)
    : m_document(document)
{
}

DocumentTimelinesController::~DocumentTimelinesController()
{
}

void DocumentTimelinesController::addTimeline(DocumentTimeline& timeline)
{
    m_timelines.add(timeline);
}

void DocumentTimelinesController::removeTimeline(DocumentTimeline& timeline)
{
    m_timelines.remove(timeline);
}

void DocumentTimelinesController::detachFromDocument()
{
    while (!m_timelines.computesEmpty())
        m_timelines.begin()->detachFromDocument();
}

void DocumentTimelinesController::updateAnimationsAndSendEvents(DOMHighResTimeStamp timestamp)
{
    ASSERT(!m_timelines.hasNullReferences());

    // We need to copy m_timelines before iterating over its members since the steps in this procedure may mutate m_timelines.
    Vector<RefPtr<DocumentTimeline>> protectedTimelines;
    for (auto& timeline : m_timelines)
        protectedTimelines.append(&timeline);

    // 1. Update the current time of all timelines associated with doc passing now as the timestamp.
    Vector<DocumentTimeline*> timelinesToUpdate;
    Vector<RefPtr<WebAnimation>> animationsToRemove;
    Vector<RefPtr<CSSTransition>> completedTransitions;
    for (auto& timeline : protectedTimelines) {
        auto shouldUpdateAnimationsAndSendEvents = timeline->documentWillUpdateAnimationsAndSendEvents(timestamp);
        if (shouldUpdateAnimationsAndSendEvents == DocumentTimeline::ShouldUpdateAnimationsAndSendEvents::No)
            continue;

        timelinesToUpdate.append(timeline.get());

        for (auto& animation : timeline->relevantAnimations()) {
            if (animation->timeline() != timeline.get()) {
                ASSERT(!animation->timeline());
                animationsToRemove.append(animation);
                continue;
            }

            // This will notify the animation that timing has changed and will call automatically
            // schedule invalidation if required for this animation.
            animation->tick();

            if (!animation->isRelevant() && !animation->needsTick())
                animationsToRemove.append(animation);

            if (!animation->needsTick() && is<CSSTransition>(animation) && animation->playState() == WebAnimation::PlayState::Finished) {
                auto* transition = downcast<CSSTransition>(animation.get());
                if (transition->owningElement())
                    completedTransitions.append(transition);
            }
        }
    }

    if (timelinesToUpdate.isEmpty())
        return;

    // 2. Remove replaced animations for doc.
    for (auto& timeline : m_timelines)
        timeline.removeReplacedAnimations();

    // 3. Perform a microtask checkpoint.
    auto document = makeRefPtr(m_document);
    document->eventLoop().performMicrotaskCheckpoint();

    // 4. Let events to dispatch be a copy of doc's pending animation event queue.
    // 5. Clear doc's pending animation event queue.
    AnimationEvents events;
    for (auto& timeline : timelinesToUpdate)
        events.appendVector(timeline->prepareForPendingAnimationEventsDispatch());

    // 6. Perform a stable sort of the animation events in events to dispatch as follows.
    std::stable_sort(events.begin(), events.end(), [] (const Ref<AnimationEventBase>& lhs, const Ref<AnimationEventBase>& rhs) {
        // 1. Sort the events by their scheduled event time such that events that were scheduled to occur earlier, sort before events scheduled to occur later
        // and events whose scheduled event time is unresolved sort before events with a resolved scheduled event time.
        // 2. Within events with equal scheduled event times, sort by their composite order. FIXME: We don't do this.
        if (lhs->timelineTime() && !rhs->timelineTime())
            return false;
        if (!lhs->timelineTime() && rhs->timelineTime())
            return true;
        if (!lhs->timelineTime() && !rhs->timelineTime())
            return true;
        return lhs->timelineTime().value() < rhs->timelineTime().value();
    });

    // 7. Dispatch each of the events in events to dispatch at their corresponding target using the order established in the previous step.
    for (auto& event : events)
        event->target()->dispatchEvent(event);

    // This will cancel any scheduled invalidation if we end up removing all animations.
    for (auto& animation : animationsToRemove) {
        // An animation that was initially marked as irrelevant may have changed while we were sending events, so we run the same
        // check that we ran to add it to animationsToRemove in the first place.
        if (!animation->isRelevant() && !animation->needsTick())
            animation->timeline()->removeAnimation(*animation);
    }

    // Now that animations that needed removal have been removed, let's update the list of completed transitions.
    // This needs to happen after dealing with the list of animations to remove as the animation may have been
    // removed from the list of completed transitions otherwise.
    for (auto& completedTransition : completedTransitions)
        downcast<DocumentTimeline>(*completedTransition->timeline()).transitionDidComplete(completedTransition);

    for (auto& timeline : timelinesToUpdate)
        timeline->documentDidUpdateAnimationsAndSendEvents();
}

} // namespace WebCore

