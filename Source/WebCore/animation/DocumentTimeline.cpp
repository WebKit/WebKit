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
#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMWindow.h"
#include "DeclarativeAnimation.h"
#include "DisplayRefreshMonitor.h"
#include "DisplayRefreshMonitorManager.h"
#include "Document.h"
#include "KeyframeEffect.h"
#include "Page.h"
#include "RenderElement.h"

static const Seconds defaultAnimationInterval { 15_ms };
static const Seconds throttledAnimationInterval { 30_ms };

namespace WebCore {

Ref<DocumentTimeline> DocumentTimeline::create(Document& document, PlatformDisplayID displayID)
{
    return adoptRef(*new DocumentTimeline(document, displayID));
}

DocumentTimeline::DocumentTimeline(Document& document, PlatformDisplayID displayID)
    : AnimationTimeline(DocumentTimelineClass)
    , m_document(&document)
    , m_animationScheduleTimer(*this, &DocumentTimeline::animationScheduleTimerFired)
#if !USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    , m_animationResolutionTimer(*this, &DocumentTimeline::animationResolutionTimerFired)
#endif
{
    windowScreenDidChange(displayID);
}

DocumentTimeline::~DocumentTimeline()
{
}

void DocumentTimeline::detachFromDocument()
{
    m_invalidationTaskQueue.close();
    m_eventDispatchTaskQueue.close();
    m_animationScheduleTimer.stop();
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

    m_isSuspended = true;

    m_invalidationTaskQueue.cancelAllTasks();
    if (m_animationScheduleTimer.isActive())
        m_animationScheduleTimer.stop();

    for (const auto& animation : animations())
        animation->setSuspended(true);

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
    if (m_paused || m_isSuspended || !m_document || !m_document->domWindow())
        return AnimationTimeline::currentTime();

    if (!m_cachedCurrentTime) {
        m_cachedCurrentTime = Seconds(m_document->domWindow()->nowTimestamp());
        scheduleInvalidationTaskIfNeeded();
    }
    return m_cachedCurrentTime;
}

void DocumentTimeline::pause()
{
    m_paused = true;
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
    for (auto& animation : animations()) {
        if (is<DeclarativeAnimation>(animation))
            downcast<DeclarativeAnimation>(*animation).invalidateDOMEvents();
    }

    applyPendingAcceleratedAnimations();

    updateAnimationSchedule();
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
    DisplayRefreshMonitorManager::sharedManager().scheduleAnimation(*this);
#else
    // FIXME: We need to use the same logic as ScriptedAnimationController here,
    // which will be addressed by the refactor tracked by webkit.org/b/179293.
    m_animationResolutionTimer.startOneShot(animationInterval());
#endif
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
void DocumentTimeline::displayRefreshFired()
#else
void DocumentTimeline::animationResolutionTimerFired()
#endif
{
    updateAnimations();
}

void DocumentTimeline::updateAnimations()
{
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

void DocumentTimeline::animationAcceleratedRunningStateDidChange(WebAnimation& animation)
{
    m_acceleratedAnimationsPendingRunningStateChange.add(&animation);
}

void DocumentTimeline::applyPendingAcceleratedAnimations()
{
    bool hasForcedLayout = false;
    for (auto& animation : m_acceleratedAnimationsPendingRunningStateChange) {
        if (!hasForcedLayout) {
            auto* effect = animation->effect();
            if (is<KeyframeEffectReadOnly>(effect))
                hasForcedLayout |= downcast<KeyframeEffectReadOnly>(effect)->forceLayoutIfNeeded();
        }
        animation->applyPendingAcceleratedActions();
    }

    m_acceleratedAnimationsPendingRunningStateChange.clear();
}

bool DocumentTimeline::runningAnimationsForElementAreAllAccelerated(Element& element)
{
    // FIXME: This will let animations run using hardware compositing even if later in the active
    // span of the current animations a new animation should require hardware compositing to be
    // disabled (webkit.org/b/179974).
    auto animations = animationsForElement(element);
    for (const auto& animation : animations) {
        if (is<KeyframeEffectReadOnly>(animation->effect()) && !downcast<KeyframeEffectReadOnly>(animation->effect())->isRunningAccelerated())
            return false;
    }
    return !animations.isEmpty();
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

void DocumentTimeline::windowScreenDidChange(PlatformDisplayID displayID)
{
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    DisplayRefreshMonitorManager::sharedManager().windowScreenDidChange(displayID, *this);
#else
    UNUSED_PARAM(displayID);
#endif
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
RefPtr<DisplayRefreshMonitor> DocumentTimeline::createDisplayRefreshMonitor(PlatformDisplayID displayID) const
{
    if (!m_document || !m_document->page())
        return nullptr;

    if (auto monitor = m_document->page()->chrome().client().createDisplayRefreshMonitor(displayID))
        return monitor;

    return DisplayRefreshMonitor::createDefaultDisplayRefreshMonitor(displayID);
}
#endif

} // namespace WebCore
