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
#include "CSSAnimation.h"
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

    while (RefPtr timeline = m_timelines.takeAny())
        timeline->detachFromDocument();
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
    protectedDocument()->eventLoop().performMicrotaskCheckpoint();

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
    return m_document->domWindow()->nowTimestamp();
}

std::optional<Seconds> AnimationTimelinesController::currentTime()
{
    if (!m_document->domWindow())
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
        m_document->eventLoop().queueTask(TaskSource::InternalAsyncTask, WTFMove(task));
    }

    // AnimationTimelinesController is owned by Document.
    m_document->vm().whenIdle([this, weakDocument = WeakPtr<Document, WeakPtrImplWithEventTargetData> { m_document }]() {
        RefPtr document = weakDocument.get();
        if (!document)
            return;

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

static const WeakStyleable originatingElement(const Ref<ScrollTimeline>& timeline)
{
    if (RefPtr viewTimeline = dynamicDowncast<ViewTimeline>(timeline))
        return viewTimeline->subjectStyleable();
    return timeline->sourceStyleable();
}

static const WeakStyleable originatingStyleableIncludingTimelineScope(const Ref<ScrollTimeline>& timeline)
{
    if (auto element = timeline->timelineScopeDeclaredElement())
        return Styleable::fromElement(*element);
    return originatingElement(timeline);
}

static const WeakStyleable originatingElementExcludingTimelineScope(const Ref<ScrollTimeline>& timeline)
{
    return timeline->timelineScopeDeclaredElement() ? WeakStyleable() : originatingElement(timeline);
}

Vector<WeakStyleable> AnimationTimelinesController::relatedTimelineScopeElements(const AtomString& name)
{
    Vector<WeakStyleable> timelineScopeElements;
    for (auto& scope : m_timelineScopeEntries) {
        if (scope.second && (scope.first.type == TimelineScope::Type::All || (scope.first.type == TimelineScope::Type::Ident && scope.first.scopeNames.contains(name))))
            timelineScopeElements.append(scope.second);
    }
    return timelineScopeElements;
}

ScrollTimeline& AnimationTimelinesController::inactiveNamedTimeline(const AtomString& name)
{
    auto inactiveTimeline = ScrollTimeline::createInactiveStyleOriginatedTimeline(name);
    timelinesForName(name).append(inactiveTimeline);
    return inactiveTimeline.get();
}

static bool containsElement(const Vector<WeakStyleable>& timelineScopeElements, Element* matchElement)
{
    return timelineScopeElements.containsIf([matchElement] (const WeakStyleable& entry ) {
        return entry.element() == matchElement;
    });
}

ScrollTimeline* AnimationTimelinesController::determineTreeOrder(const Vector<Ref<ScrollTimeline>>& ancestorTimelines, const Styleable& styleable, const Vector<WeakStyleable>& timelineScopeElements)
{
    RefPtr element = &styleable.element;
    while (element) {
        Vector<Ref<ScrollTimeline>> matchedTimelines;
        for (auto& timeline : ancestorTimelines) {
            if (element == originatingStyleableIncludingTimelineScope(timeline).element().get())
                matchedTimelines.append(timeline);
        }
        if (!matchedTimelines.isEmpty()) {
            if (containsElement(timelineScopeElements, element.get())) {
                if (matchedTimelines.size() == 1)
                    return matchedTimelines.first().ptr();
                // Naming conflict due to timeline-scope
                return &inactiveNamedTimeline(matchedTimelines.first()->name());
            }
            ASSERT(matchedTimelines.size() <= 2);
            // Favor scroll timelines in case of conflict
            if (!is<ViewTimeline>(matchedTimelines.first()))
                return matchedTimelines.first().ptr();
            return matchedTimelines.last().ptr();
        }
        // Has blocking timeline scope element
        if (containsElement(timelineScopeElements, element.get()))
            return nullptr;
        element = element->parentElement();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

ScrollTimeline* AnimationTimelinesController::determineTimelineForElement(const Vector<Ref<ScrollTimeline>>& timelines, const Styleable& styleable, const Vector<WeakStyleable>& timelineScopeElements)
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
        auto styleableForTimeline = originatingStyleableIncludingTimelineScope(timeline).styleable();
        if (!styleableForTimeline)
            continue;
        if (&styleableForTimeline->element == &styleable.element || styleable.element.isDescendantOf(styleableForTimeline->element))
            matchedTimelines.append(timeline);
    }
    if (matchedTimelines.isEmpty())
        return nullptr;
    return determineTreeOrder(matchedTimelines, styleable, timelineScopeElements);
}

Vector<Ref<ScrollTimeline>>& AnimationTimelinesController::timelinesForName(const AtomString& name)
{
    return m_nameToTimelineMap.ensure(name, [] {
        return Vector<Ref<ScrollTimeline>> { };
    }).iterator->value;
}

void AnimationTimelinesController::updateTimelineForTimelineScope(const Ref<ScrollTimeline>& timeline, const AtomString& name)
{
    Vector<Styleable> matchedTimelineScopeElements;
    auto timelineElement = originatingElementExcludingTimelineScope(timeline).styleable();
    if (!timelineElement)
        return;

    for (auto& entry : m_timelineScopeEntries) {
        if (auto entryElement = entry.second.styleable()) {
            if (timelineElement->element.isDescendantOf(entryElement->element) && (entry.first.type == TimelineScope::Type::All ||  entry.first.scopeNames.contains(name)))
                matchedTimelineScopeElements.appendIfNotContains(*entryElement);
        }
    }
    auto* element = &timelineElement->element;
    while (element) {
        auto it = matchedTimelineScopeElements.findIf([element] (const Styleable& entry) {
            return &entry.element == element;
        });
        if (it != notFound) {
            timeline->setTimelineScopeElement(matchedTimelineScopeElements.at(it).element);
            return;
        }
        element = element->parentElement();
    }
}

void AnimationTimelinesController::registerNamedScrollTimeline(const AtomString& name, const Styleable& source, ScrollAxis axis)
{
    LOG_WITH_STREAM(Animations, stream << "AnimationTimelinesController::registerNamedScrollTimeline: " << name << " source: " << source);

    auto& timelines = timelinesForName(name);

    auto existingTimelineIndex = timelines.findIf([&](auto& timeline) {
        return !is<ViewTimeline>(timeline) && timeline->sourceStyleable() == source;
    });

    if (existingTimelineIndex != notFound) {
        Ref existingScrollTimeline = timelines[existingTimelineIndex].get();
        existingScrollTimeline->setAxis(axis);
    } else {
        auto newScrollTimeline = ScrollTimeline::create(name, axis);
        newScrollTimeline->setSource(source);
        updateTimelineForTimelineScope(newScrollTimeline, name);
        timelines.append(WTFMove(newScrollTimeline));
        updateCSSAnimationsAssociatedWithNamedTimeline(name);
    }
}

void AnimationTimelinesController::updateCSSAnimationsAssociatedWithNamedTimeline(const AtomString& name)
{
    // First, we need to gather all CSS Animations attached to existing timelines
    // with the specified name. We do this prior to updating animation-to-timeline
    // relationship because this could mutate the timeline's animations list.
    HashSet<Ref<CSSAnimation>> cssAnimationsWithMatchingTimelineName;
    for (auto& timeline : timelinesForName(name)) {
        for (auto& animation : timeline->relevantAnimations()) {
            if (RefPtr cssAnimation = dynamicDowncast<CSSAnimation>(animation.get())) {
                if (!cssAnimation->owningElement())
                    continue;
                if (auto* timelineName = std::get_if<AtomString>(&cssAnimation->backingAnimation().timeline())) {
                    if (*timelineName == name)
                        cssAnimationsWithMatchingTimelineName.add(*cssAnimation);
                }
            }
        }
    }

    for (auto& cssAnimation : cssAnimationsWithMatchingTimelineName)
        cssAnimation->syncStyleOriginatedTimeline();
}

void AnimationTimelinesController::removePendingOperationsForCSSAnimation(const CSSAnimation& animation)
{
    m_pendingAttachOperations.removeAllMatching([&] (const TimelineMapAttachOperation& operation) {
        return operation.animation.ptr() == &animation;
    });
}

void AnimationTimelinesController::documentDidResolveStyle()
{
    auto operations = std::exchange(m_pendingAttachOperations, { });
    for (auto& operation : operations) {
        if (WeakPtr animation = operation.animation) {
            if (auto styleable = operation.element.styleable())
                setTimelineForName(operation.name, *styleable, *animation, AllowsDeferral::No);
        }
    }

    // Purge any inactive named timeline no longer attached to an animation.
    Vector<AtomString> namesToRemove;
    for (auto& [name, timelines] : m_nameToTimelineMap) {
        timelines.removeAllMatching([](auto& timeline) {
            return timeline->isInactiveStyleOriginatedTimeline() && timeline->relevantAnimations().isEmpty();
        });
        if (timelines.isEmpty())
            m_nameToTimelineMap.remove(name);
    }

    m_removedTimelines.clear();
}

void AnimationTimelinesController::registerNamedViewTimeline(const AtomString& name, const Styleable& subject, ScrollAxis axis, ViewTimelineInsets&& insets)
{
    LOG_WITH_STREAM(Animations, stream << "AnimationTimelinesController::registerNamedViewTimeline: " << name << " subject: " << subject);

    auto& timelines = timelinesForName(name);

    auto existingTimelineIndex = timelines.findIf([&](auto& timeline) {
        if (RefPtr viewTimeline = dynamicDowncast<ViewTimeline>(timeline))
            return viewTimeline->subjectStyleable() == subject;
        return false;
    });

    auto hasExistingTimeline = existingTimelineIndex != notFound;

    if (hasExistingTimeline) {
        Ref existingViewTimeline = downcast<ViewTimeline>(timelines[existingTimelineIndex].get());
        existingViewTimeline->setAxis(axis);
        existingViewTimeline->setInsets(WTFMove(insets));
    } else {
        auto newViewTimeline = ViewTimeline::create(name, axis, WTFMove(insets));
        newViewTimeline->setSubject(subject);
        updateTimelineForTimelineScope(newViewTimeline, name);
        timelines.append(WTFMove(newViewTimeline));
    }

    if (!hasExistingTimeline)
        updateCSSAnimationsAssociatedWithNamedTimeline(name);
}

void AnimationTimelinesController::unregisterNamedTimeline(const AtomString& name, const Styleable& styleable)
{
    LOG_WITH_STREAM(Animations, stream << "AnimationTimelinesController::unregisterNamedTimeline: " << name << " styleable: " << styleable);

    auto it = m_nameToTimelineMap.find(name);
    if (it == m_nameToTimelineMap.end())
        return;

    auto& timelines = it->value;

    auto i = timelines.findIf([&] (auto& entry) {
        return originatingElement(entry) == styleable;
    });

    if (i == notFound)
        return;

    auto timeline = timelines.at(i);

    // Make sure to remove the named timeline from our name-to-timelines map first,
    // such that re-syncing any CSS Animation previously registered with it resolves
    // their `animation-timeline` properly.
    timelines.remove(i);

    for (Ref animation : timeline->relevantAnimations()) {
        if (RefPtr cssAnimation = dynamicDowncast<CSSAnimation>(animation)) {
            if (cssAnimation->owningElement())
                cssAnimation->syncStyleOriginatedTimeline();
        }
    }

    if (timelines.isEmpty())
        m_nameToTimelineMap.remove(it);
    else
        updateCSSAnimationsAssociatedWithNamedTimeline(name);
}

void AnimationTimelinesController::setTimelineForName(const AtomString& name, const Styleable& styleable, CSSAnimation& animation)
{
    setTimelineForName(name, styleable, animation, AllowsDeferral::Yes);
}

void AnimationTimelinesController::setTimelineForName(const AtomString& name, const Styleable& styleable, CSSAnimation& animation, AllowsDeferral allowsDeferral)
{
    LOG_WITH_STREAM(Animations, stream << "AnimationTimelinesController::setTimelineForName: " << name << " styleable: " << styleable);

    auto it = m_nameToTimelineMap.find(name);
    auto hasNamedTimeline = it != m_nameToTimelineMap.end() && it->value.containsIf([](auto& timeline) {
        return !timeline->isInactiveStyleOriginatedTimeline();
    });

    // If we don't have an active named timeline yet and deferral is allowed,
    // just register a pending timeline attachment operation so we can try again
    // when style has resolved.
    if (!hasNamedTimeline && allowsDeferral == AllowsDeferral::Yes) {
        m_pendingAttachOperations.append({ styleable, name, animation });
        return;
    }

    auto timelineScopeElements = relatedTimelineScopeElements(name);

    if (!hasNamedTimeline) {
        // First, determine whether the name is within scope, ie. whether a parent element
        // has a `timeline-scope` property that contains this timeline name.
        auto nameIsWithinScope = [&] {
            for (auto timelineScopeElement : timelineScopeElements) {
                ASSERT(timelineScopeElement.element());
                if (styleable == timelineScopeElement.styleable() || styleable.element.isDescendantOf(*timelineScopeElement.element()))
                    return true;
            }
            return false;
        }();

        ASSERT(allowsDeferral == AllowsDeferral::No);
        // We don't have an active named timeline and yet we must set a timeline since
        // we've already dealt with the defferal case before. There are two cases:
        //     1. the name is within scope and we should create a placeholder inactive
        //        scroll timeline, or,
        //     2. the name is not within scope and the timeline is null.
        if (nameIsWithinScope)
            animation.setTimeline(&inactiveNamedTimeline(name));
        else {
            animation.setTimeline(nullptr);
            // Since we have no timelines defined for this name yet, we need
            // to keep a pending operation such that we may attach the named
            // timeline should it appear.
            m_pendingAttachOperations.append({ styleable, name, animation });
        }
    } else {
        auto& timelines = it->value;
        if (RefPtr timeline = determineTimelineForElement(timelines, styleable, timelineScopeElements)) {
            LOG_WITH_STREAM(Animations, stream << "AnimationTimelinesController::setTimelineForName: " << name << " styleable: " << styleable << " attaching to timeline of element: " << originatingElement(*timeline));
            animation.setTimeline(WTFMove(timeline));
        }
    }
}

static void updateTimelinesForTimelineScope(Vector<Ref<ScrollTimeline>> entries, const Styleable& styleable)
{
    for (auto& entry : entries) {
        if (auto entryElement = originatingElementExcludingTimelineScope(entry).styleable()) {
            if (entryElement->element.isDescendantOf(styleable.element))
                entry->setTimelineScopeElement(styleable.element);
        }
    }
}

void AnimationTimelinesController::updateNamedTimelineMapForTimelineScope(const TimelineScope& scope, const Styleable& styleable)
{
    LOG_WITH_STREAM(Animations, stream << "AnimationTimelinesController::updateNamedTimelineMapForTimelineScope: " << scope << " styleable: " << styleable);

    // https://drafts.csswg.org/scroll-animations-1/#timeline-scope
    // This property declares the scope of the specified timeline names to extend across this element’s subtree. This allows a named timeline
    // (such as a named scroll progress timeline or named view progress timeline) to be referenced by elements outside the timeline-defining element’s
    // subtree—​for example, by siblings, cousins, or ancestors.
    switch (scope.type) {
    case TimelineScope::Type::None:
        for (auto& entry : m_nameToTimelineMap) {
            for (auto& timeline : entry.value) {
                if (timeline->timelineScopeDeclaredElement() == &styleable.element)
                    timeline->clearTimelineScopeDeclaredElement();
            }
        }
        m_timelineScopeEntries.removeAllMatching([&] (const std::pair<TimelineScope, WeakStyleable> entry) {
            return entry.second == styleable;
        });
        break;
    case TimelineScope::Type::All:
        for (auto& entry : m_nameToTimelineMap)
            updateTimelinesForTimelineScope(entry.value, styleable);
        m_timelineScopeEntries.append(std::make_pair(scope, styleable));
        break;
    case TimelineScope::Type::Ident:
        for (auto& name : scope.scopeNames) {
            auto it = m_nameToTimelineMap.find(name);
            if (it != m_nameToTimelineMap.end())
                updateTimelinesForTimelineScope(it->value, styleable);
        }
        m_timelineScopeEntries.append(std::make_pair(scope, styleable));
        break;
    }
}

bool AnimationTimelinesController::isPendingTimelineAttachment(const WebAnimation& animation) const
{
    if (RefPtr cssAnimation = dynamicDowncast<CSSAnimation>(animation)) {
        return m_pendingAttachOperations.containsIf([&](auto& operation) {
            return operation.animation.ptr() == cssAnimation.get();
        });
    }
    return false;
}

void AnimationTimelinesController::unregisterNamedTimelinesAssociatedWithElement(const Styleable& styleable)
{
    LOG_WITH_STREAM(Animations, stream << "AnimationTimelinesController::unregisterNamedTimelinesAssociatedWithElement element: " << styleable);

    UncheckedKeyHashSet<AtomString> namesToClear;

    for (auto& entry : m_nameToTimelineMap) {
        auto& timelines = entry.value;
        for (size_t i = 0; i < timelines.size(); ++i) {
            auto& timeline = timelines[i];
            if (originatingElement(timeline) == styleable) {
                m_removedTimelines.add(timeline.get());
                timelines.remove(i);
                i--;
            }
        }
        if (timelines.isEmpty())
            namesToClear.add(entry.key);
    }

    for (auto& name : namesToClear)
        m_nameToTimelineMap.remove(name);
}

void AnimationTimelinesController::styleableWasRemoved(const Styleable& styleable)
{
    for (auto& timeline : m_removedTimelines) {
        if (originatingElement(timeline) != styleable)
            continue;
        auto& timelineName = timeline->name();
        for (auto& animation : timeline->relevantAnimations()) {
            if (RefPtr cssAnimation = dynamicDowncast<CSSAnimation>(animation.get())) {
                if (auto owningElement = cssAnimation->owningElement()) {
                    setTimelineForName(timelineName, *owningElement, *cssAnimation, AllowsDeferral::Yes);
                    owningElement->element.invalidateStyleForAnimation();
                }
            }
        }
    }
}

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
AcceleratedEffectStackUpdater& AnimationTimelinesController::acceleratedEffectStackUpdater()
{
    if (!m_acceleratedEffectStackUpdater)
        m_acceleratedEffectStackUpdater = makeUnique<AcceleratedEffectStackUpdater>(m_document.get());
    return *m_acceleratedEffectStackUpdater;
}
#endif

} // namespace WebCore

