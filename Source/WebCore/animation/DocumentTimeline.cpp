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
#include "DocumentTimeline.h"

#include "AnimationPlaybackEvent.h"
#include "CSSPropertyAnimation.h"
#include "DOMWindow.h"
#include "DeclarativeAnimation.h"
#include "Document.h"
#include "GraphicsLayer.h"
#include "KeyframeEffect.h"
#include "Microtasks.h"
#include "Page.h"
#include "RenderElement.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"

static const Seconds defaultAnimationInterval { 15_ms };
static const Seconds throttledAnimationInterval { 30_ms };

namespace WebCore {

Ref<DocumentTimeline> DocumentTimeline::create(Document& document)
{
    return adoptRef(*new DocumentTimeline(document, 0_s));
}

Ref<DocumentTimeline> DocumentTimeline::create(Document& document, DocumentTimelineOptions&& options)
{
    return adoptRef(*new DocumentTimeline(document, Seconds::fromMilliseconds(options.originTime)));
}

DocumentTimeline::DocumentTimeline(Document& document, Seconds originTime)
    : AnimationTimeline(DocumentTimelineClass)
    , m_document(&document)
    , m_originTime(originTime)
    , m_animationScheduleTimer(*this, &DocumentTimeline::animationScheduleTimerFired)
#if !USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    , m_animationResolutionTimer(*this, &DocumentTimeline::animationResolutionTimerFired)
#endif
{
    if (m_document && m_document->page() && !m_document->page()->isVisible())
        suspendAnimations();
}

DocumentTimeline::~DocumentTimeline() = default;

void DocumentTimeline::detachFromDocument()
{
    m_invalidationTaskQueue.close();
    m_eventDispatchTaskQueue.close();
    m_animationScheduleTimer.stop();
    m_elementsWithRunningAcceleratedAnimations.clear();

    auto& animationsToRemove = animations();
    while (!animationsToRemove.isEmpty())
        animationsToRemove.first()->remove();

    m_document = nullptr;
}

void DocumentTimeline::updateThrottlingState()
{
    m_needsUpdateAnimationSchedule = false;
    timingModelDidChange();
}

Seconds DocumentTimeline::animationInterval() const
{
    if (!m_document || !m_document->page())
        return Seconds::infinity();
    return m_document->page()->isLowPowerModeEnabled() ? throttledAnimationInterval : defaultAnimationInterval;
}

void DocumentTimeline::suspendAnimations()
{
    if (animationsAreSuspended())
        return;

    m_invalidationTaskQueue.cancelAllTasks();
    if (m_animationScheduleTimer.isActive())
        m_animationScheduleTimer.stop();

    for (const auto& animation : animations())
        animation->setSuspended(true);

    m_isSuspended = true;

    applyPendingAcceleratedAnimations();
}

void DocumentTimeline::resumeAnimations()
{
    if (!animationsAreSuspended())
        return;

    m_isSuspended = false;

    for (const auto& animation : animations())
        animation->setSuspended(false);

    m_needsUpdateAnimationSchedule = false;
    timingModelDidChange();
}

bool DocumentTimeline::animationsAreSuspended()
{
    return m_isSuspended;
}

unsigned DocumentTimeline::numberOfActiveAnimationsForTesting() const
{
    unsigned count = 0;
    for (const auto& animation : animations()) {
        if (!animation->isSuspended())
            ++count;
    }
    return count;
}

std::optional<Seconds> DocumentTimeline::currentTime()
{
    if (!m_document || !m_document->domWindow())
        return AnimationTimeline::currentTime();

    if (auto* mainDocumentTimeline = m_document->existingTimeline()) {
        if (mainDocumentTimeline != this) {
            if (auto mainDocumentTimelineCurrentTime = mainDocumentTimeline->currentTime())
                return mainDocumentTimelineCurrentTime.value() - m_originTime;
            return std::nullopt;
        }
    }

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    // If we're in the middle of firing a frame, either due to a requestAnimationFrame callback
    // or scheduling an animation update, we want to ensure we use the same time we're using as
    // the timestamp for requestAnimationFrame() callbacks.
    if (m_document->animationScheduler().isFiring())
        m_cachedCurrentTime = m_document->animationScheduler().lastTimestamp();
#endif

    if (!m_cachedCurrentTime) {
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
        // If we're not in the middle of firing a frame, let's make our best guess at what the currentTime should
        // be since the last time a frame fired by increment of our update interval. This way code using something
        // like setTimeout() or handling events will get a time that's only updating at around 60fps, or less if
        // we're throttled.
        auto lastAnimationSchedulerTimestamp = m_document->animationScheduler().lastTimestamp();
        auto delta = Seconds(m_document->domWindow()->nowTimestamp()) - lastAnimationSchedulerTimestamp;
        int frames = std::floor(delta.seconds() / animationInterval().seconds());
        m_cachedCurrentTime = lastAnimationSchedulerTimestamp + Seconds(frames * animationInterval().seconds());
#else
        m_cachedCurrentTime = Seconds(m_document->domWindow()->nowTimestamp());
#endif
        // We want to be sure to keep this time cached until we've both finished running JS and finished updating
        // animations, so we schedule the invalidation task and register a whenIdle callback on the VM, which will
        // fire syncronously if no JS is running.
        scheduleInvalidationTaskIfNeeded();
        m_waitingOnVMIdle = true;
        m_document->vm().whenIdle([this, protectedThis = makeRefPtr(this)]() {
            m_waitingOnVMIdle = false;
            maybeClearCachedCurrentTime();
        });
    }
    return m_cachedCurrentTime.value() - m_originTime;
}

void DocumentTimeline::timingModelDidChange()
{
    if (m_needsUpdateAnimationSchedule || m_isSuspended)
        return;

    m_needsUpdateAnimationSchedule = true;

    // We know that we will resolve animations again, so we can cancel the timer right away.
    if (m_animationScheduleTimer.isActive())
        m_animationScheduleTimer.stop();

    scheduleInvalidationTaskIfNeeded();
}

void DocumentTimeline::scheduleInvalidationTaskIfNeeded()
{
    if (m_invalidationTaskQueue.hasPendingTasks())
        return;

    m_invalidationTaskQueue.enqueueTask(std::bind(&DocumentTimeline::performInvalidationTask, this));
}

void DocumentTimeline::performInvalidationTask()
{
    // Now that the timing model has changed we can see if there are DOM events to dispatch for declarative animations.
    if (!m_isSuspended) {
        for (auto& animation : animations()) {
            if (is<DeclarativeAnimation>(animation))
                downcast<DeclarativeAnimation>(*animation).invalidateDOMEvents();
        }
    }

    applyPendingAcceleratedAnimations();

    updateAnimationSchedule();
    maybeClearCachedCurrentTime();
}

void DocumentTimeline::maybeClearCachedCurrentTime()
{
    // We want to make sure we only clear the cached current time if we're not currently running
    // JS or waiting on all current animation updating code to have completed. This is so that
    // we're guaranteed to have a consistent current time reported for all work happening in a given
    // JS frame or throughout updating animations in WebCore.
    if (!m_waitingOnVMIdle && !m_invalidationTaskQueue.hasPendingTasks())
        m_cachedCurrentTime = std::nullopt;
}

void DocumentTimeline::updateAnimationSchedule()
{
    if (!m_needsUpdateAnimationSchedule)
        return;

    m_needsUpdateAnimationSchedule = false;

    if (!m_acceleratedAnimationsPendingRunningStateChange.isEmpty()) {
        scheduleAnimationResolution();
        return;
    }

    Seconds scheduleDelay = Seconds::infinity();

    for (const auto& animation : animations()) {
        auto animationTimeToNextRequiredTick = animation->timeToNextRequiredTick();
        if (animationTimeToNextRequiredTick < animationInterval()) {
            scheduleAnimationResolution();
            return;
        }
        scheduleDelay = std::min(scheduleDelay, animationTimeToNextRequiredTick);
    }

    if (scheduleDelay < Seconds::infinity())
        m_animationScheduleTimer.startOneShot(scheduleDelay);
}

void DocumentTimeline::animationScheduleTimerFired()
{
    scheduleAnimationResolution();
}

void DocumentTimeline::scheduleAnimationResolution()
{
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    m_document->animationScheduler().scheduleWebAnimationsResolution();
#else
    // FIXME: We need to use the same logic as ScriptedAnimationController here,
    // which will be addressed by the refactor tracked by webkit.org/b/179293.
    m_animationResolutionTimer.startOneShot(animationInterval());
#endif
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
void DocumentTimeline::documentAnimationSchedulerDidFire()
#else
void DocumentTimeline::animationResolutionTimerFired()
#endif
{
    updateAnimations();
}

void DocumentTimeline::updateAnimations()
{
    m_numberOfAnimationTimelineInvalidationsForTesting++;

    for (const auto& animation : animations())
        animation->runPendingTasks();

    // Perform a microtask checkpoint such that all promises that may have resolved while
    // running pending tasks can fire right away.
    MicrotaskQueue::mainThreadQueue().performMicrotaskCheckpoint();

    // Let's first resolve any animation that does not have a target.
    for (auto* animation : animationsWithoutTarget())
        animation->resolve();

    // For the rest of the animations, we will resolve them via TreeResolver::createAnimatedElementUpdate()
    // by invalidating their target element's style.
    if (m_document && hasElementAnimations()) {
        for (const auto& elementToAnimationsMapItem : elementToAnimationsMap())
            elementToAnimationsMapItem.key->invalidateStyleAndLayerComposition();
        for (const auto& elementToCSSAnimationsMapItem : elementToCSSAnimationsMap())
            elementToCSSAnimationsMapItem.key->invalidateStyleAndLayerComposition();
        for (const auto& elementToCSSTransitionsMapItem : elementToCSSTransitionsMap())
            elementToCSSTransitionsMapItem.key->invalidateStyleAndLayerComposition();
        m_document->updateStyleIfNeeded();
    }

    // Time has advanced, the timing model requires invalidation now.
    timingModelDidChange();
}

bool DocumentTimeline::computeExtentOfAnimation(RenderElement& renderer, LayoutRect& bounds) const
{
    if (!renderer.element())
        return true;

    KeyframeEffectReadOnly* matchingEffect = nullptr;
    for (const auto& animation : animationsForElement(*renderer.element())) {
        auto* effect = animation->effect();
        if (is<KeyframeEffectReadOnly>(effect)) {
            auto* keyframeEffect = downcast<KeyframeEffectReadOnly>(effect);
            if (keyframeEffect->animatedProperties().contains(CSSPropertyTransform))
                matchingEffect = downcast<KeyframeEffectReadOnly>(effect);
        }
    }

    if (matchingEffect)
        return matchingEffect->computeExtentOfTransformAnimation(bounds);

    return true;
}

bool DocumentTimeline::isRunningAnimationOnRenderer(RenderElement& renderer, CSSPropertyID property) const
{
    if (!renderer.element())
        return false;

    for (const auto& animation : animationsForElement(*renderer.element())) {
        auto playState = animation->playState();
        if (playState != WebAnimation::PlayState::Running && playState != WebAnimation::PlayState::Paused)
            continue;
        auto* effect = animation->effect();
        if (is<KeyframeEffectReadOnly>(effect) && downcast<KeyframeEffectReadOnly>(effect)->animatedProperties().contains(property))
            return true;
    }

    return false;
}

bool DocumentTimeline::isRunningAcceleratedAnimationOnRenderer(RenderElement& renderer, CSSPropertyID property) const
{
    if (!renderer.element())
        return false;

    for (const auto& animation : animationsForElement(*renderer.element())) {
        auto playState = animation->playState();
        if (playState != WebAnimation::PlayState::Running && playState != WebAnimation::PlayState::Paused)
            continue;
        auto* effect = animation->effect();
        if (is<KeyframeEffectReadOnly>(effect)) {
            auto* keyframeEffect = downcast<KeyframeEffectReadOnly>(effect);
            if (keyframeEffect->isRunningAccelerated() && keyframeEffect->animatedProperties().contains(property))
                return true;
        }
    }

    return false;
}

std::unique_ptr<RenderStyle> DocumentTimeline::animatedStyleForRenderer(RenderElement& renderer)
{
    std::unique_ptr<RenderStyle> result;

    if (auto* element = renderer.element()) {
        for (const auto& animation : animationsForElement(*element)) {
            if (is<KeyframeEffectReadOnly>(animation->effect()))
                downcast<KeyframeEffectReadOnly>(animation->effect())->getAnimatedStyle(result);
        }
    }

    if (!result)
        result = RenderStyle::clonePtr(renderer.style());

    return result;
}

void DocumentTimeline::animationWasAddedToElement(WebAnimation& animation, Element& element)
{
    AnimationTimeline::animationWasAddedToElement(animation, element);
    updateListOfElementsWithRunningAcceleratedAnimationsForElement(element);
}

void DocumentTimeline::animationWasRemovedFromElement(WebAnimation& animation, Element& element)
{
    AnimationTimeline::animationWasRemovedFromElement(animation, element);
    updateListOfElementsWithRunningAcceleratedAnimationsForElement(element);
}

void DocumentTimeline::animationAcceleratedRunningStateDidChange(WebAnimation& animation)
{
    m_acceleratedAnimationsPendingRunningStateChange.add(&animation);

    if (is<KeyframeEffectReadOnly>(animation.effect())) {
        if (auto* target = downcast<KeyframeEffectReadOnly>(animation.effect())->target())
            updateListOfElementsWithRunningAcceleratedAnimationsForElement(*target);
    }
}

void DocumentTimeline::updateListOfElementsWithRunningAcceleratedAnimationsForElement(Element& element)
{
    auto animations = animationsForElement(element);
    bool runningAnimationsForElementAreAllAccelerated = !animations.isEmpty();
    for (const auto& animation : animations) {
        if (is<KeyframeEffectReadOnly>(animation->effect()) && !downcast<KeyframeEffectReadOnly>(animation->effect())->isRunningAccelerated()) {
            runningAnimationsForElementAreAllAccelerated = false;
            break;
        }
    }

    if (runningAnimationsForElementAreAllAccelerated)
        m_elementsWithRunningAcceleratedAnimations.add(&element);
    else
        m_elementsWithRunningAcceleratedAnimations.remove(&element);
}

void DocumentTimeline::applyPendingAcceleratedAnimations()
{
    auto acceleratedAnimationsPendingRunningStateChange = m_acceleratedAnimationsPendingRunningStateChange;
    m_acceleratedAnimationsPendingRunningStateChange.clear();

    bool hasForcedLayout = false;
    for (auto& animation : acceleratedAnimationsPendingRunningStateChange) {
        if (!hasForcedLayout) {
            auto* effect = animation->effect();
            if (is<KeyframeEffectReadOnly>(effect))
                hasForcedLayout |= downcast<KeyframeEffectReadOnly>(effect)->forceLayoutIfNeeded();
        }
        animation->applyPendingAcceleratedActions();
    }
}

bool DocumentTimeline::resolveAnimationsForElement(Element& element, RenderStyle& targetStyle)
{
    bool hasNonAcceleratedAnimations = false;
    bool hasPendingAcceleratedAnimations = true;
    for (const auto& animation : animationsForElement(element)) {
        animation->resolve(targetStyle);
        if (!hasNonAcceleratedAnimations) {
            if (auto* effect = animation->effect()) {
                if (is<KeyframeEffectReadOnly>(effect)) {
                    auto* keyframeEffect = downcast<KeyframeEffectReadOnly>(effect);
                    for (auto cssPropertyId : keyframeEffect->animatedProperties()) {
                        if (!CSSPropertyAnimation::animationOfPropertyIsAccelerated(cssPropertyId)) {
                            hasNonAcceleratedAnimations = true;
                            continue;
                        }
                        if (!hasPendingAcceleratedAnimations)
                            hasPendingAcceleratedAnimations = keyframeEffect->hasPendingAcceleratedAction();
                    }
                }
            }
        }
    }

    // If there are no non-accelerated animations and we've encountered at least one pending
    // accelerated animation, we should recomposite this element's layer for animation purposes.
    return !hasNonAcceleratedAnimations && hasPendingAcceleratedAnimations;
}

bool DocumentTimeline::runningAnimationsForElementAreAllAccelerated(Element& element) const
{
    return m_elementsWithRunningAcceleratedAnimations.contains(&element);
}

void DocumentTimeline::enqueueAnimationPlaybackEvent(AnimationPlaybackEvent& event)
{
    m_pendingAnimationEvents.append(event);

    if (!m_eventDispatchTaskQueue.hasPendingTasks())
        m_eventDispatchTaskQueue.enqueueTask(std::bind(&DocumentTimeline::performEventDispatchTask, this));
}

static inline bool compareAnimationPlaybackEvents(const Ref<WebCore::AnimationPlaybackEvent>& lhs, const Ref<WebCore::AnimationPlaybackEvent>& rhs)
{
    // Sort the events by their scheduled event time such that events that were scheduled to occur earlier, sort before events scheduled to occur later
    // and events whose scheduled event time is unresolved sort before events with a resolved scheduled event time.
    if (lhs->timelineTime() && !rhs->timelineTime())
        return false;
    if (!lhs->timelineTime() && rhs->timelineTime())
        return true;
    if (!lhs->timelineTime() && !rhs->timelineTime())
        return true;
    return lhs->timelineTime().value() < rhs->timelineTime().value();
}

void DocumentTimeline::performEventDispatchTask()
{
    if (m_pendingAnimationEvents.isEmpty())
        return;

    auto pendingAnimationEvents = WTFMove(m_pendingAnimationEvents);

    std::stable_sort(pendingAnimationEvents.begin(), pendingAnimationEvents.end(), compareAnimationPlaybackEvents);
    for (auto& pendingEvent : pendingAnimationEvents)
        pendingEvent->target()->dispatchEvent(pendingEvent);
}

Vector<std::pair<String, double>> DocumentTimeline::acceleratedAnimationsForElement(Element& element) const
{
    auto* renderer = element.renderer();
    if (renderer && renderer->isComposited()) {
        auto* compositedRenderer = downcast<RenderBoxModelObject>(renderer);
        if (auto* graphicsLayer = compositedRenderer->layer()->backing()->graphicsLayer())
            return graphicsLayer->acceleratedAnimationsForTesting();
    }
    return { };
}

unsigned DocumentTimeline::numberOfAnimationTimelineInvalidationsForTesting() const
{
    return m_numberOfAnimationTimelineInvalidationsForTesting;
}

} // namespace WebCore
