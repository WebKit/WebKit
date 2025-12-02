/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "AnimationTimelinesController.h"

#include "AnimationEventBase.h"
#include "CSSAnimation.h"
#include "CSSTransition.h"
#include "ContainerNodeInlines.h"
#include "DocumentEventLoop.h"
#include "DocumentPage.h"
#include "DocumentTimeline.h"
#include "DocumentWindow.h"
#include "ElementInlines.h"
#include "EventLoop.h"
#include "EventTargetInlines.h"
#include "GraphicsLayer.h"
#include "KeyframeEffect.h"
#include "LocalDOMWindow.h"
#include "Logging.h"
#include "RenderBoxModelObject.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderObject.h"
#include "ScrollTimeline.h"
#include "Settings.h"
#include "StyleOriginatedTimelinesController.h"
#include "ViewTimeline.h"
#include "WebAnimation.h"
#include <ranges>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/text/TextStream.h>

#if ENABLE(THREADED_ANIMATIONS)
#include "AcceleratedEffectStackUpdater.h"
#endif

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AnimationTimelinesController);

AnimationTimelinesController::AnimationTimelinesController(Document& document)
    : m_cachedCurrentTimeClearanceTimer(*this, &AnimationTimelinesController::clearCachedCurrentTime)
    , m_document(document)
{
    if (RefPtr page = document.page()) {
        if (page->settings().hiddenPageCSSAnimationSuspensionEnabled() && !page->isVisible())
            suspendAnimations();
    }
}

AnimationTimelinesController::~AnimationTimelinesController() = default;

void AnimationTimelinesController::addTimeline(AnimationTimeline& timeline)
{
    m_timelines.add(timeline);

    if (m_isSuspended)
        timeline.suspendAnimations();
    else
        timeline.resumeAnimations();
}

void AnimationTimelinesController::removeTimeline(AnimationTimeline& timeline)
{
    m_timelines.remove(timeline);
}

void AnimationTimelinesController::detachFromDocument()
{
    m_pendingAnimationsProcessingTaskCancellationGroup.cancel();

    while (RefPtr timeline = m_timelines.takeAny())
        timeline->detachFromDocument();
}

void AnimationTimelinesController::addPendingAnimation(WebAnimation& animation)
{
    m_pendingAnimations.add(animation);
}

void AnimationTimelinesController::updateAnimationsAndSendEvents(ReducedResolutionSeconds timestamp)
{
    auto previousMaximumAnimationFrameRate = maximumAnimationFrameRate();
    // This will hold the frame rate at which we would schedule updates not
    // accounting for the frame rate of animations.
    std::optional<FramesPerSecond> defaultTimelineFrameRate;
    // This will hold the frame rate used for this timeline until now.
    std::optional<FramesPerSecond> previousTimelineFrameRate;
    if (RefPtr page = m_document->page()) {
        defaultTimelineFrameRate = page->preferredRenderingUpdateFramesPerSecond({ Page::PreferredRenderingUpdateOption::IncludeThrottlingReasons });
        previousTimelineFrameRate = page->preferredRenderingUpdateFramesPerSecond({
            Page::PreferredRenderingUpdateOption::IncludeThrottlingReasons,
            Page::PreferredRenderingUpdateOption::IncludeAnimationsFrameRate
        });
    }

    LOG_WITH_STREAM(Animations, stream << "AnimationTimelinesController::updateAnimationsAndSendEvents for time " << timestamp);

    // We need to copy m_timelines before iterating over its members since the steps in this procedure may mutate m_timelines.
    auto protectedTimelines = copyToVectorOf<Ref<AnimationTimeline>>(m_timelines);

    // We need to freeze the current time even if no animation is running.
    // document.timeline.currentTime may be called from a rAF callback and
    // it has to match the rAF timestamp.
    if (!m_isSuspended)
        cacheCurrentTime(timestamp);

    m_frameRateAligner.beginUpdate(timestamp, previousTimelineFrameRate);

    // 1. Update the current time of all timelines associated with document passing now as the timestamp.
    ASSERT(m_updatedScrollTimelines.isEmpty());
    Vector<Ref<AnimationTimeline>> timelinesToUpdate;
    Vector<Ref<WebAnimation>> animationsToRemove;
    Vector<Ref<CSSTransition>> completedTransitions;
    for (auto& timeline : protectedTimelines) {
        auto shouldUpdateAnimationsAndSendEvents = timeline->documentWillUpdateAnimationsAndSendEvents();
        if (shouldUpdateAnimationsAndSendEvents == AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::No)
            continue;

        timelinesToUpdate.append(timeline.copyRef());

        // https://drafts.csswg.org/scroll-animations-1/#event-loop
        if (RefPtr scrollTimeline = dynamicDowncast<ScrollTimeline>(timeline))
            m_updatedScrollTimelines.append(*scrollTimeline);

        for (auto& animation : copyToVector(timeline->relevantAnimations())) {
            if (animation->isSkippedContentAnimation())
                continue;

            if (animation->timeline() != timeline.ptr()) {
                ASSERT(!animation->timeline());
                continue;
            }

            // Even though this animation is relevant, its frame rate may be such that it should
            // be disregarded during this update. If it does not specify an explicit frame rate,
            // this means this animation uses the default frame rate at which we typically schedule
            // updates not accounting for the frame rate of animations.
            auto animationFrameRate = animation->frameRate();
            if (!animationFrameRate)
                animationFrameRate = defaultTimelineFrameRate;

            if (animationFrameRate) {
                ASSERT(*animationFrameRate > 0);
                auto shouldUpdate = m_frameRateAligner.updateFrameRate(*animationFrameRate);
                // Even if we're told not to update, any newly-added animation should fire right away,
                // it will align with other animations of that frame rate at the next opportunity.
                if (shouldUpdate == FrameRateAligner::ShouldUpdate::No && !animation->pending())
                    continue;
            }

            // This will notify the animation that timing has changed and will call automatically
            // schedule invalidation if required for this animation.
            animation->tick();

            if (!animation->isRelevant() && !animation->needsTick() && !isPendingTimelineAttachment(animation))
                animationsToRemove.append(animation);

            if (RefPtr transition = dynamicDowncast<CSSTransition>(animation)) {
                if (!transition->needsTick() && transition->playState() == WebAnimation::PlayState::Finished && transition->owningElement())
                    completedTransitions.append(*transition);
            }
        }
    }

    m_frameRateAligner.finishUpdate();

    // If the maximum frame rate we've encountered is the same as the default frame rate,
    // let's reset it to not have an explicit value which will indicate that there is no
    // need to override the default animation frame rate to service animations.
    auto maximumAnimationFrameRate = m_frameRateAligner.maximumFrameRate();
    if (maximumAnimationFrameRate == defaultTimelineFrameRate)
        maximumAnimationFrameRate = std::nullopt;

    // Ensure the timeline updates at the maximum frame rate we've encountered for our animations.
    if (previousMaximumAnimationFrameRate != maximumAnimationFrameRate) {
        if (RefPtr page = m_document->page()) {
            if (previousTimelineFrameRate != maximumAnimationFrameRate)
                page->timelineControllerMaximumAnimationFrameRateDidChange(*this);
        }
    }

    if (timelinesToUpdate.isEmpty())
        return;

    // 2. Remove replaced animations for document.
    for (auto& timeline : protectedTimelines) {
        if (RefPtr documentTimeline = dynamicDowncast<DocumentTimeline>(timeline))
            documentTimeline->removeReplacedAnimations();
    }

    // 3. Perform a microtask checkpoint.
    protectedDocument()->checkedEventLoop()->performMicrotaskCheckpoint();

    if (RefPtr documentTimeline = m_document->existingTimeline()) {
        // FIXME: pending animation events should be owned by this controller rather
        // than the document timeline.

        // 4. Let events to dispatch be a copy of doc's pending animation event queue.
        // 5. Clear doc's pending animation event queue.
        auto events = documentTimeline->prepareForPendingAnimationEventsDispatch();

        // 6. Perform a stable sort of the animation events in events to dispatch as follows.
        std::ranges::stable_sort(events, compareAnimationEventsByCompositeOrder);

        // 7. Dispatch each of the events in events to dispatch at their corresponding target using the order established in the previous step.
        for (auto& event : events)
            event->protectedTarget()->dispatchEvent(event);
    }

    // This will cancel any scheduled invalidation if we end up removing all animations.
    for (auto& animation : animationsToRemove) {
        // An animation that was initially marked as irrelevant may have changed while
        // we were sending events, so redo the the check for whether it should be removed.
        if (RefPtr timeline = animation->timeline()) {
            if (!animation->isRelevant() && !animation->needsTick())
                timeline->removeAnimation(animation);
        }
    }

    // Now that animations that needed removal have been removed, update the list of completed transitions.
    // This needs to happen after dealing with the list of animations to remove as the animation may have been
    // removed from the list of completed transitions otherwise.
    for (auto& completedTransition : completedTransitions) {
        if (RefPtr documentTimeline = dynamicDowncast<DocumentTimeline>(completedTransition->timeline()))
            documentTimeline->transitionDidComplete(WTFMove(completedTransition));
    }

    for (auto& timeline : timelinesToUpdate) {
        if (RefPtr documentTimeline = dynamicDowncast<DocumentTimeline>(timeline))
            documentTimeline->documentDidUpdateAnimationsAndSendEvents();
    }
}

void AnimationTimelinesController::runPostRenderingUpdateTasks()
{
#if ENABLE(THREADED_ANIMATIONS)
    if (m_document->settings().threadedScrollDrivenAnimationsEnabled()) {
        for (Ref timeline : m_timelines)
            timeline->runPostRenderingUpdateTasks();
        if (m_acceleratedEffectStackUpdater)
            m_acceleratedEffectStackUpdater->update();
    }
#endif
    // https://drafts.csswg.org/scroll-animations-1/#event-loop
    auto scrollTimelines = std::exchange(m_updatedScrollTimelines, { });
    for (auto scrollTimeline : scrollTimelines)
        scrollTimeline->updateCurrentTimeIfStale();
}

std::optional<Seconds> AnimationTimelinesController::timeUntilNextTickForAnimationsWithFrameRate(FramesPerSecond frameRate) const
{
    if (!m_cachedCurrentTime)
        return std::nullopt;
    return m_frameRateAligner.timeUntilNextUpdateForFrameRate(frameRate, *m_cachedCurrentTime);
};

void AnimationTimelinesController::suspendAnimations()
{
    if (m_isSuspended)
        return;

    if (!m_cachedCurrentTime)
        m_cachedCurrentTime = liveCurrentTime();

    m_cachedCurrentTimeClearanceTimer.stop();

    for (Ref timeline : m_timelines)
        timeline->suspendAnimations();

    m_isSuspended = true;
}

void AnimationTimelinesController::resumeAnimations()
{
    if (!m_isSuspended)
        return;

    m_isSuspended = false;

    clearCachedCurrentTime();

    for (Ref timeline : m_timelines)
        timeline->resumeAnimations();
}

ReducedResolutionSeconds AnimationTimelinesController::liveCurrentTime() const
{
    return protectedDocument()->protectedWindow()->nowTimestamp();
}

std::optional<Seconds> AnimationTimelinesController::currentTime(UseCachedCurrentTime useCachedCurrentTime)
{
    if (!m_document->window())
        return std::nullopt;

    if (useCachedCurrentTime == UseCachedCurrentTime::No && !m_isSuspended)
        return liveCurrentTime();

    if (!m_cachedCurrentTime)
        cacheCurrentTime(liveCurrentTime());

    return *m_cachedCurrentTime;
}

void AnimationTimelinesController::cacheCurrentTime(ReducedResolutionSeconds newCurrentTime)
{
    if (m_cachedCurrentTime == newCurrentTime)
        return;

    m_cachedCurrentTimeClearanceTimer.stop();

    // We can get in a situation where the event loop will not run a task that had been enqueued.
    // If that is the case, we must clear the task group and run the callback prior to adding a
    // new task.
    if (m_pendingAnimationsProcessingTaskCancellationGroup.hasPendingTask() && m_cachedCurrentTime) {
        m_pendingAnimationsProcessingTaskCancellationGroup.cancel();
        processPendingAnimations();
    }

    m_cachedCurrentTime = newCurrentTime;

    // As we've advanced to a new current time, we want all animations created during this run
    // loop to have this newly-cached current time as their start time. To that end, we schedule
    // a task to set that current time on all animations created until then as their pending
    // start time.
    if (!m_pendingAnimationsProcessingTaskCancellationGroup.hasPendingTask()) {
        CancellableTask task(m_pendingAnimationsProcessingTaskCancellationGroup, std::bind(&AnimationTimelinesController::processPendingAnimations, this));
        protectedDocument()->checkedEventLoop()->queueTask(TaskSource::InternalAsyncTask, WTFMove(task));
    }

    if (!m_isSuspended) {
        // In order to not have a stale cached current time, we schedule a timer to reset it
        // in the time it would take an animation frame to run under normal circumstances.
        RefPtr page = m_document->page();
        auto renderingUpdateInterval = page ? page->preferredRenderingUpdateInterval() : FullSpeedAnimationInterval;
        m_cachedCurrentTimeClearanceTimer.startOneShot(renderingUpdateInterval);
    }
}

void AnimationTimelinesController::clearCachedCurrentTime()
{
    ASSERT(!m_isSuspended);
    m_cachedCurrentTime = std::nullopt;
}

void AnimationTimelinesController::processPendingAnimations()
{
    if (m_isSuspended || !m_cachedCurrentTime)
        return;

    ASSERT(!m_pendingAnimationsProcessingTaskCancellationGroup.hasPendingTask());

    auto pendingAnimations = std::exchange(m_pendingAnimations, { });
    for (Ref pendingAnimation : pendingAnimations) {
        if (pendingAnimation->timeline() && pendingAnimation->timeline()->isMonotonic())
            pendingAnimation->setPendingStartTime(*m_cachedCurrentTime);
    }
}

bool AnimationTimelinesController::isPendingTimelineAttachment(const WebAnimation& animation) const
{
    CheckedPtr styleOriginatedTimelinesController = protectedDocument()->styleOriginatedTimelinesController();
    return styleOriginatedTimelinesController && styleOriginatedTimelinesController->isPendingTimelineAttachment(animation);
}

#if ENABLE(THREADED_ANIMATIONS)
void AnimationTimelinesController::scheduleAcceleratedEffectStackUpdateForTarget(const Styleable& target)
{
    if (!m_acceleratedEffectStackUpdater)
        m_acceleratedEffectStackUpdater = makeUnique<AcceleratedEffectStackUpdater>();
    m_acceleratedEffectStackUpdater->scheduleUpdateForTarget(target);

    if (RefPtr documentTimeline = m_document->existingTimeline())
        documentTimeline->scheduleAcceleratedEffectStackUpdate();
}
#endif

Vector<GraphicsLayer::AcceleratedAnimationForTesting> AnimationTimelinesController::acceleratedAnimationsForElement(const Element& element) const
{
    CheckedPtr renderer = element.renderer();
    if (renderer && renderer->isComposited()) {
        CheckedPtr compositedRenderer = downcast<RenderBoxModelObject>(renderer.get());
        if (RefPtr graphicsLayer = compositedRenderer->layer()->backing()->graphicsLayer())
            return graphicsLayer->acceleratedAnimationsForTesting();
    }
    return { };
}

} // namespace WebCore

