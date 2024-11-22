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
#include "AnimationTimelinesController.h"

#include "AnimationEventBase.h"
#include "CSSTransition.h"
#include "Document.h"
#include "DocumentTimeline.h"
#include "ElementInlines.h"
#include "EventLoop.h"
#include "KeyframeEffect.h"
#include "LocalDOMWindow.h"
#include "Logging.h"
#include "Page.h"
#include "ScrollTimeline.h"
#include "Settings.h"
#include "ViewTimeline.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"
#include <JavaScriptCore/VM.h>
#include <wtf/HashSet.h>
#include <wtf/text/TextStream.h>

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
#include "AcceleratedEffectStackUpdater.h"
#endif

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AnimationTimelinesController);

AnimationTimelinesController::AnimationTimelinesController(Document& document)
    : m_document(document)
{
    if (auto* page = document.page()) {
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
    m_currentTimeClearingTaskCancellationGroup.cancel();

    while (!m_timelines.isEmptyIgnoringNullReferences()) {
        Ref timeline = *m_timelines.begin();
        timeline->detachFromDocument();
    }
}

void AnimationTimelinesController::updateAnimationsAndSendEvents(ReducedResolutionSeconds timestamp)
{
    auto previousMaximumAnimationFrameRate = maximumAnimationFrameRate();
    // This will hold the frame rate at which we would schedule updates not
    // accounting for the frame rate of animations.
    std::optional<FramesPerSecond> defaultTimelineFrameRate;
    // This will hold the frame rate used for this timeline until now.
    std::optional<FramesPerSecond> previousTimelineFrameRate;
    if (RefPtr page = m_document.page()) {
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
    Vector<Ref<AnimationTimeline>> timelinesToUpdate;
    Vector<Ref<WebAnimation>> animationsToRemove;
    Vector<Ref<CSSTransition>> completedTransitions;
    for (auto& timeline : protectedTimelines) {
        auto shouldUpdateAnimationsAndSendEvents = timeline->documentWillUpdateAnimationsAndSendEvents();
        if (shouldUpdateAnimationsAndSendEvents == AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::No)
            continue;

        timelinesToUpdate.append(timeline.copyRef());

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

            if (auto* transition = dynamicDowncast<CSSTransition>(animation.get())) {
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
        if (RefPtr page = m_document.page()) {
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
    Ref { m_document }->eventLoop().performMicrotaskCheckpoint();

    // 4. Let events to dispatch be a copy of doc's pending animation event queue.
    // 5. Clear doc's pending animation event queue.
    AnimationEvents events;
    for (auto& timeline : timelinesToUpdate) {
        if (RefPtr documentTimeline = dynamicDowncast<DocumentTimeline>(timeline))
            events.appendVector(documentTimeline->prepareForPendingAnimationEventsDispatch());
    }

    // 6. Perform a stable sort of the animation events in events to dispatch as follows.
    std::stable_sort(events.begin(), events.end(), [] (const Ref<AnimationEventBase>& lhs, const Ref<AnimationEventBase>& rhs) {
        return compareAnimationEventsByCompositeOrder(lhs.get(), rhs.get());
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
        if (auto documentTimeline = dynamicDowncast<DocumentTimeline>(completedTransition->timeline()))
            documentTimeline->transitionDidComplete(WTFMove(completedTransition));
    }

    for (auto& timeline : timelinesToUpdate) {
        if (RefPtr documentTimeline = dynamicDowncast<DocumentTimeline>(timeline))
            documentTimeline->documentDidUpdateAnimationsAndSendEvents();
    }
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

    for (auto& timeline : m_timelines)
        timeline.suspendAnimations();

    m_isSuspended = true;
}

void AnimationTimelinesController::resumeAnimations()
{
    if (!m_isSuspended)
        return;

    m_cachedCurrentTime = std::nullopt;

    m_isSuspended = false;

    for (auto& timeline : m_timelines)
        timeline.resumeAnimations();
}

ReducedResolutionSeconds AnimationTimelinesController::liveCurrentTime() const
{
    return m_document.domWindow()->nowTimestamp();
}

std::optional<Seconds> AnimationTimelinesController::currentTime()
{
    if (!m_document.domWindow())
        return std::nullopt;

    if (!m_cachedCurrentTime)
        cacheCurrentTime(liveCurrentTime());

    return *m_cachedCurrentTime;
}

void AnimationTimelinesController::cacheCurrentTime(ReducedResolutionSeconds newCurrentTime)
{
    m_cachedCurrentTime = newCurrentTime;
    // We want to be sure to keep this time cached until we've both finished running JS and finished updating
    // animations, so we schedule the invalidation task and register a whenIdle callback on the VM, which will
    // fire syncronously if no JS is running.
    m_waitingOnVMIdle = true;
    if (!m_currentTimeClearingTaskCancellationGroup.hasPendingTask()) {
        CancellableTask task(m_currentTimeClearingTaskCancellationGroup, std::bind(&AnimationTimelinesController::maybeClearCachedCurrentTime, this));
        m_document.eventLoop().queueTask(TaskSource::InternalAsyncTask, WTFMove(task));
    }
    // We extent the associated Document's lifecycle until the VM became idle since the AnimationTimelinesController
    // is owned by the Document.
    m_document.vm().whenIdle([this, protectedDocument = Ref { m_document }]() {
        m_waitingOnVMIdle = false;
        maybeClearCachedCurrentTime();
    });
}

void AnimationTimelinesController::maybeClearCachedCurrentTime()
{
    // We want to make sure we only clear the cached current time if we're not currently running
    // JS or waiting on all current animation updating code to have completed. This is so that
    // we're guaranteed to have a consistent current time reported for all work happening in a given
    // JS frame or throughout updating animations in WebCore.
    if (!m_isSuspended && !m_waitingOnVMIdle && !m_currentTimeClearingTaskCancellationGroup.hasPendingTask())
        m_cachedCurrentTime = std::nullopt;
}

static Element* originatingElement(const Ref<ScrollTimeline>& timeline)
{
    if (RefPtr viewTimeline = dynamicDowncast<ViewTimeline>(timeline))
        return viewTimeline->subject();
    return timeline->source();
}

static Element* originatingElementIncludingTimelineScope(const Ref<ScrollTimeline>& timeline)
{
    if (WeakPtr element = timeline->timelineScopeDeclaredElement())
        return element.get();
    return originatingElement(timeline);
}

static Element* originatingElementExcludingTimelineScope(const Ref<ScrollTimeline>& timeline)
{
    return timeline->timelineScopeDeclaredElement() ? nullptr : originatingElement(timeline);
}

Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>> AnimationTimelinesController::relatedTimelineScopeElements(const AtomString& name)
{
    Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>> timelineScopeElements;
    for (auto& scope : m_timelineScopeEntries) {
        if (scope.second && (scope.first.type == TimelineScope::Type::All || (scope.first.type == TimelineScope::Type::Ident && scope.first.scopeNames.contains(name))))
            timelineScopeElements.append(scope.second);
    }
    return timelineScopeElements;
}

static ScrollTimeline* determineTreeOrder(const Vector<Ref<ScrollTimeline>>& ancestorTimelines, const Element& matchElement, const Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>>& timelineScopeElements)
{
    RefPtr element = &matchElement;
    while (element) {
        Vector<Ref<ScrollTimeline>> matchedTimelines;
        for (auto& timeline : ancestorTimelines) {
            if (element == originatingElementIncludingTimelineScope(timeline))
                matchedTimelines.append(timeline);
        }
        if (!matchedTimelines.isEmpty()) {
            if (timelineScopeElements.contains(element.get())) {
                if (matchedTimelines.size() == 1)
                    return matchedTimelines.first().ptr();
                // Naming conflict due to timeline-scope
                return nullptr;
            }
            ASSERT(matchedTimelines.size() <= 2);
            // Favor scroll timelines in case of conflict
            if (!is<ViewTimeline>(matchedTimelines.first()))
                return matchedTimelines.first().ptr();
            return matchedTimelines.last().ptr();
        }
        // Has blocking timeline scope element
        if (timelineScopeElements.contains(element.get()))
            return nullptr;
        element = element->parentElement();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

static ScrollTimeline* determineTimelineForElement(const Vector<Ref<ScrollTimeline>>& timelines, const Element& element, const Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>>& timelineScopeElements)
{
    // https://drafts.csswg.org/scroll-animations-1/#timeline-scoping
    // A named scroll progress timeline or view progress timeline is referenceable by:
    // 1. the name-declaring element itself
    // 2. that element’s descendants
    // If multiple elements have declared the same timeline name, the matching timeline is the one declared on the nearest element in tree order.
    // In case of a name conflict on the same element, names declared later in the naming property (scroll-timeline-name, view-timeline-name) take
    // precedence, and scroll progress timelines take precedence over view progress timelines.
    Vector<Ref<ScrollTimeline>> matchedTimelines;
    for (auto& timeline : timelines) {
        if (originatingElementIncludingTimelineScope(timeline) == &element || element.isDescendantOf(originatingElementIncludingTimelineScope(timeline)))
            matchedTimelines.append(timeline);
    }
    if (matchedTimelines.isEmpty())
        return nullptr;
    return determineTreeOrder(matchedTimelines, element, timelineScopeElements);
}

Vector<Ref<ScrollTimeline>>& AnimationTimelinesController::timelinesForName(const AtomString& name)
{
    return m_nameToTimelineMap.ensure(name, [] {
        return Vector<Ref<ScrollTimeline>> { };
    }).iterator->value;
}

void AnimationTimelinesController::updateTimelineForTimelineScope(const Ref<ScrollTimeline>& timeline, const AtomString& name)
{
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> matchedTimelineScopeElements;
    RefPtr timelineElement = originatingElementExcludingTimelineScope(timeline);
    for (auto& entry : m_timelineScopeEntries) {
        if (WeakPtr entryElement = entry.second) {
            if (timelineElement->isDescendantOf(*entryElement) && (entry.first.type == TimelineScope::Type::All ||  entry.first.scopeNames.contains(name)))
                matchedTimelineScopeElements.add(*entryElement);
        }
    }
    while (timelineElement) {
        if (matchedTimelineScopeElements.contains(*timelineElement)) {
            timeline->setTimelineScopeElement(*timelineElement);
            return;
        }
        timelineElement = timelineElement->parentElement();
    }
}

void AnimationTimelinesController::registerNamedScrollTimeline(const AtomString& name, const Element& source, ScrollAxis axis)
{
    auto& timelines = timelinesForName(name);

    auto existingTimelineIndex = timelines.findIf([&](auto& timeline) {
        return !is<ViewTimeline>(timeline) && timeline->source() == &source;
    });

    if (existingTimelineIndex != notFound) {
        Ref existingScrollTimeline = timelines[existingTimelineIndex].get();
        existingScrollTimeline->setAxis(axis);
    } else {
        auto newScrollTimeline = ScrollTimeline::create(name, axis);
        newScrollTimeline->setSource(&source);
        updateTimelineForTimelineScope(newScrollTimeline, name);
        timelines.append(WTFMove(newScrollTimeline));
    }
    attachPendingOperations();
}

void AnimationTimelinesController::attachPendingOperations()
{
    auto queries = std::exchange(m_pendingAttachOperations, { });
    for (auto& operation : queries) {
        if (WeakPtr animation = operation.animation) {
            if (WeakPtr element = operation.element)
                setTimelineForName(operation.name, *element, *animation);
        }
    }
}

void AnimationTimelinesController::registerNamedViewTimeline(const AtomString& name, const Element& subject, ScrollAxis axis, ViewTimelineInsets&& insets)
{
    auto& timelines = timelinesForName(name);

    auto existingTimelineIndex = timelines.findIf([&](auto& timeline) {
        if (RefPtr viewTimeline = dynamicDowncast<ViewTimeline>(timeline))
            return viewTimeline->subject() == &subject;
        return false;
    });

    if (existingTimelineIndex != notFound) {
        Ref existingViewTimeline = downcast<ViewTimeline>(timelines[existingTimelineIndex].get());
        existingViewTimeline->setAxis(axis);
        existingViewTimeline->setInsets(WTFMove(insets));
    } else {
        auto newViewTimeline = ViewTimeline::create(name, axis, WTFMove(insets));
        newViewTimeline->setSubject(&subject);
        updateTimelineForTimelineScope(newViewTimeline, name);
        timelines.append(WTFMove(newViewTimeline));
    }
    attachPendingOperations();
}

void AnimationTimelinesController::unregisterNamedTimeline(const AtomString& name, const Element& element)
{
    auto it = m_nameToTimelineMap.find(name);
    if (it == m_nameToTimelineMap.end())
        return;

    auto& timelines = it->value;

    auto i = timelines.findIf([&] (auto& entry) {
        return originatingElement(entry) == &element;
    });

    if (i == notFound)
        return;

    auto& timeline = timelines.at(i);

    for (Ref animation : timeline->relevantAnimations()) {
        if (RefPtr effect = dynamicDowncast<KeyframeEffect>(animation->effect()))
            m_pendingAttachOperations.append(TimelineMapAttachOperation { effect->target(), name, animation });
    }
    timelines.remove(i);

    if (timelines.isEmpty())
        m_nameToTimelineMap.remove(it);
    attachPendingOperations();
}

void AnimationTimelinesController::setTimelineForName(const AtomString& name, const Element& element, WebAnimation& animation)
{
    auto it = m_nameToTimelineMap.find(name);
    if (it == m_nameToTimelineMap.end()) {
        m_pendingAttachOperations.append({ element, name, animation });
        return;
    }

    auto& timelines = it->value;
    if (RefPtr timeline = determineTimelineForElement(timelines, element, relatedTimelineScopeElements(name))) {
        animation.setTimeline(WTFMove(timeline));
        return;
    }
    animation.setTimeline(nullptr);

    m_pendingAttachOperations.append({ element, name, animation });
}

static void updateTimelinesForTimelineScope(Vector<Ref<ScrollTimeline>> entries, const Element& element)
{
    for (auto& entry : entries) {
        if (RefPtr entryElement = originatingElementExcludingTimelineScope(entry)) {
            if (entryElement->isDescendantOf(element))
                entry->setTimelineScopeElement(element);
        }
    }
}

void AnimationTimelinesController::updateNamedTimelineMapForTimelineScope(const TimelineScope& scope, const Element& element)
{
    // https://drafts.csswg.org/scroll-animations-1/#timeline-scope
    // This property declares the scope of the specified timeline names to extend across this element’s subtree. This allows a named timeline
    // (such as a named scroll progress timeline or named view progress timeline) to be referenced by elements outside the timeline-defining element’s
    // subtree—​for example, by siblings, cousins, or ancestors.
    switch (scope.type) {
    case TimelineScope::Type::None:
        for (auto& entry : m_nameToTimelineMap) {
            for (auto& timeline : entry.value) {
                if (timeline->timelineScopeDeclaredElement() == &element)
                    timeline->clearTimelineScopeDeclaredElement();
            }
        }
        m_timelineScopeEntries.removeAllMatching([&] (const std::pair<TimelineScope, WeakPtr<Element, WeakPtrImplWithEventTargetData>>& entry) {
            return entry.second == &element;
        });
        break;
    case TimelineScope::Type::All:
        for (auto& entry : m_nameToTimelineMap)
            updateTimelinesForTimelineScope(entry.value, element);
        m_timelineScopeEntries.append(std::make_pair(scope, WeakPtr { &element }));
        break;
    case TimelineScope::Type::Ident:
        for (auto& name : scope.scopeNames) {
            auto it = m_nameToTimelineMap.find(name);
            if (it != m_nameToTimelineMap.end())
                updateTimelinesForTimelineScope(it->value, element);
        }
        m_timelineScopeEntries.append(std::make_pair(scope, WeakPtr { &element }));
        break;
    }
    attachPendingOperations();
}

bool AnimationTimelinesController::isPendingTimelineAttachment(const WebAnimation& animation) const
{
    return m_pendingAttachOperations.containsIf([&] (auto& entry) {
        return entry.animation.ptr() == &animation;
    });
}

void AnimationTimelinesController::unregisterNamedTimelinesAssociatedWithElement(const Element& element)
{
    HashSet<AtomString> namesToClear;

    for (auto& entry : m_nameToTimelineMap) {
        auto& timelines = entry.value;
        timelines.removeAllMatching([&] (const auto& timeline) {
            return originatingElement(timeline) == &element;
        });
        if (timelines.isEmpty())
            namesToClear.add(entry.key);
    }

    for (auto& name : namesToClear)
        m_nameToTimelineMap.remove(name);
}

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
AcceleratedEffectStackUpdater& AnimationTimelinesController::acceleratedEffectStackUpdater()
{
    if (!m_acceleratedEffectStackUpdater)
        m_acceleratedEffectStackUpdater = makeUnique<AcceleratedEffectStackUpdater>(m_document);
    return *m_acceleratedEffectStackUpdater;
}
#endif

} // namespace WebCore

