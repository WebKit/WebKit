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

#include "AnimationEventBase.h"
#include "AnimationTimelinesController.h"
#include "CSSProperty.h"
#include "CSSTransition.h"
#include "CustomAnimationOptions.h"
#include "CustomEffect.h"
#include "CustomEffectCallback.h"
#include "Document.h"
#include "EventNames.h"
#include "GraphicsLayer.h"
#include "KeyframeEffect.h"
#include "KeyframeEffectStack.h"
#include "Logging.h"
#include "Node.h"
#include "Page.h"
#include "RenderBoxModelObject.h"
#include "RenderElement.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "StyleOriginatedAnimation.h"
#include "WebAnimationTypes.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
#include "AcceleratedEffectStackUpdater.h"
#endif

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
    : m_tickScheduleTimer(*this, &DocumentTimeline::scheduleAnimationResolution)
    , m_document(document)
    , m_originTime(originTime)
{
    document.ensureTimelinesController().addTimeline(*this);
}

DocumentTimeline::~DocumentTimeline() = default;

AnimationTimelinesController* DocumentTimeline::controller() const
{
    if (m_document)
        return &m_document->ensureTimelinesController();
    return nullptr;
}

void DocumentTimeline::detachFromDocument()
{
    AnimationTimeline::detachFromDocument();

    m_pendingAnimationEvents.clear();

    clearTickScheduleTimer();
    m_document = nullptr;
}

Seconds DocumentTimeline::animationInterval() const
{
    if (!m_document || !m_document->page())
        return Seconds::infinity();

    return m_document->page()->preferredRenderingUpdateInterval();
}

void DocumentTimeline::suspendAnimations()
{
    AnimationTimeline::suspendAnimations();

    applyPendingAcceleratedAnimations();

    clearTickScheduleTimer();
}

void DocumentTimeline::resumeAnimations()
{
    AnimationTimeline::resumeAnimations();

    scheduleAnimationResolution();
}

unsigned DocumentTimeline::numberOfActiveAnimationsForTesting() const
{
    unsigned count = 0;
    for (const auto& animation : m_animations) {
        if (!animation->isSuspended())
            ++count;
    }
    return count;
}

std::optional<WebAnimationTime> DocumentTimeline::currentTime(const TimelineRange&)
{
    if (auto* controller = this->controller()) {
        if (auto currentTime = controller->currentTime())
            return *currentTime - m_originTime;
        return std::nullopt;
    }
    return AnimationTimeline::currentTime();
}

void DocumentTimeline::animationTimingDidChange(WebAnimation& animation)
{
    AnimationTimeline::animationTimingDidChange(animation);
    if (!animation.isEffectInvalidationSuspended())
        scheduleAnimationResolution();
}

void DocumentTimeline::removeAnimation(WebAnimation& animation)
{
    AnimationTimeline::removeAnimation(animation);

    if (m_animations.isEmpty())
        clearTickScheduleTimer();
}

void DocumentTimeline::scheduleAnimationResolution()
{
    if (animationsAreSuspended() || m_animationResolutionScheduled || !m_document || !m_document->page())
        return;

    bool havePendingActivity = shouldRunUpdateAnimationsAndSendEventsIgnoringSuspensionState();
    LOG_WITH_STREAM(Animations, stream << "DocumentTimeline " << this << " scheduleAnimationResolution - havePendingActivity " << havePendingActivity);

    // We need some relevant animations or pending events to proceed.
    if (!havePendingActivity)
        return;

    m_document->page()->scheduleRenderingUpdate(RenderingUpdateStep::Animations);
    m_animationResolutionScheduled = true;
}

void DocumentTimeline::clearTickScheduleTimer()
{
    m_tickScheduleTimer.stop();
}

bool DocumentTimeline::shouldRunUpdateAnimationsAndSendEventsIgnoringSuspensionState() const
{
    return !m_animations.isEmpty() || !m_pendingAnimationEvents.isEmpty() || !m_acceleratedAnimationsPendingRunningStateChange.isEmpty();
}

AnimationTimeline::ShouldUpdateAnimationsAndSendEvents DocumentTimeline::documentWillUpdateAnimationsAndSendEvents()
{
    // Updating animations and sending events may invalidate the timing of some animations, so we must set the m_animationResolutionScheduled
    // flag to false prior to running that procedure to allow animation with timing model updates to schedule updates.
    bool wasAnimationResolutionScheduled = std::exchange(m_animationResolutionScheduled, false);

    if (!wasAnimationResolutionScheduled || animationsAreSuspended() || !shouldRunUpdateAnimationsAndSendEventsIgnoringSuspensionState())
        return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::No;

    m_numberOfAnimationTimelineInvalidationsForTesting++;

    // enqueueAnimationEvent() calls scheduleAnimationResolution() to ensure that the "update animations and send events"
    // procedure is run and enqueued events are dispatched in the next frame. However, events that are enqueued while
    // this procedure is running should not schedule animation resolution until the event queue has been cleared.
    m_shouldScheduleAnimationResolutionForNewPendingEvents = false;

    return AnimationTimeline::ShouldUpdateAnimationsAndSendEvents::Yes;
}

void DocumentTimeline::documentDidUpdateAnimationsAndSendEvents()
{
    applyPendingAcceleratedAnimations();

    if (!m_animationResolutionScheduled)
        scheduleNextTick();
}

void DocumentTimeline::styleOriginatedAnimationsWereCreated()
{
    scheduleAnimationResolution();
}

bool DocumentTimeline::animationCanBeRemoved(WebAnimation& animation)
{
    // https://drafts.csswg.org/web-animations/#removing-replaced-animations

    ASSERT(m_document);

    // - is replaceable, and
    if (!animation.isReplaceable())
        return false;

    // - has a replace state of active, and
    if (animation.replaceState() != WebAnimation::ReplaceState::Active)
        return false;

    // - has an associated animation effect whose target element is a descendant of doc, and
    auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(animation.effect());
    if (!keyframeEffect)
        return false;

    auto target = keyframeEffect->targetStyleable();
    if (!target || !target->element.isDescendantOf(*m_document))
        return false;

IGNORE_GCC_WARNINGS_BEGIN("dangling-reference")
    auto& style = [&]() -> const RenderStyle& {
        if (auto* renderer = target->renderer())
            return renderer->style();
        return RenderStyle::defaultStyle();
    }();
IGNORE_GCC_WARNINGS_END

    auto resolvedProperty = [&] (AnimatableCSSProperty property) -> AnimatableCSSProperty {
        if (std::holds_alternative<CSSPropertyID>(property))
            return CSSProperty::resolveDirectionAwareProperty(std::get<CSSPropertyID>(property), style.writingMode());
        return property;
    };

    HashSet<AnimatableCSSProperty> propertiesToMatch;
    for (auto property : keyframeEffect->animatedProperties())
        propertiesToMatch.add(resolvedProperty(property));

    auto protectedAnimations = [&]() -> Vector<RefPtr<WebAnimation>> {
        if (auto* effectStack = target->keyframeEffectStack()) {
            return effectStack->sortedEffects().map([](auto& effect) -> RefPtr<WebAnimation> {
                return effect->animation();
            });
        }
        return { };
    }();

    for (auto& animationWithHigherCompositeOrder : makeReversedRange(protectedAnimations)) {
        if (&animation == animationWithHigherCompositeOrder)
            break;

        if (animationWithHigherCompositeOrder && animationWithHigherCompositeOrder->isReplaceable()) {
            if (auto* keyframeEffectWithHigherCompositeOrder = dynamicDowncast<KeyframeEffect>(animationWithHigherCompositeOrder->effect())) {
                for (auto property : keyframeEffectWithHigherCompositeOrder->animatedProperties()) {
                    if (propertiesToMatch.remove(resolvedProperty(property)) && propertiesToMatch.isEmpty())
                        break;
                }
            }
        }
    }

    return propertiesToMatch.isEmpty();
}

void DocumentTimeline::removeReplacedAnimations()
{
    // https://drafts.csswg.org/web-animations/#removing-replaced-animations
    auto& eventNames = WebCore::eventNames();
    Vector<Ref<WebAnimation>> animationsToRemove;

    // When asked to remove replaced animations for a Document, doc, then for every animation, animation
    for (auto& animation : m_animations) {
        if (!animationCanBeRemoved(animation))
            continue;

        // perform the following steps:
        // 1. Set animation's replace state to removed.
        animation->setReplaceState(WebAnimation::ReplaceState::Removed);
        // 2. Create an AnimationPlaybackEvent, removeEvent.
        // 3. Set removeEvent's type attribute to remove.
        // 4. Set removeEvent's currentTime attribute to the current time of animation.
        // 5. Set removeEvent's timelineTime attribute to the current time of the timeline with which animation is associated.
        // 6. If animation has a document for timing, then append removeEvent to its document for timing's pending animation
        //    event queue along with its target, animation. For the scheduled event time, use the result of applying the procedure
        //    to convert timeline time to origin-relative time to the current time of the timeline with which animation is associated.
        //    Otherwise, queue a task to dispatch removeEvent at animation. The task source for this task is the DOM manipulation task source.
        if (animation->hasEventListeners(eventNames.removeEvent)) {
            auto scheduledTime = [&]() -> std::optional<Seconds> {
                if (auto* documentTimeline = dynamicDowncast<DocumentTimeline>(animation->timeline())) {
                    if (auto currentTime = documentTimeline->currentTime()) {
                        ASSERT(currentTime->time());
                        return documentTimeline->convertTimelineTimeToOriginRelativeTime(*currentTime->time());
                    }
                }
                return std::nullopt;
            }();
            auto animationCurrentTime = [&]() -> std::optional<Seconds> {
                if (auto animationTime = animation->currentTime())
                    return animationTime->time();
                return std::nullopt;
            }();
            animation->enqueueAnimationPlaybackEvent(eventNames.removeEvent, animationCurrentTime, scheduledTime);
        }

        animationsToRemove.append(animation.get());
    }

    for (auto& animation : animationsToRemove) {
        if (auto* timeline = animation->timeline())
            timeline->removeAnimation(animation);
    }
}

void DocumentTimeline::transitionDidComplete(Ref<CSSTransition>&& transition)
{
    removeAnimation(transition.get());
    if (auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(transition->effect())) {
        if (auto styleable = keyframeEffect->targetStyleable()) {
            auto property = transition->property();
            if (styleable->hasRunningTransitionForProperty(property))
                styleable->ensureCompletedTransitionsByProperty().set(property, WTFMove(transition));
        }
    }
}

void DocumentTimeline::scheduleNextTick()
{
    // If we have pending animation events, we need to schedule an update right away.
    if (!m_pendingAnimationEvents.isEmpty())
        scheduleAnimationResolution();

    // There is no tick to schedule if we don't have any relevant animations.
    if (m_animations.isEmpty())
        return;

    Seconds scheduleDelay = Seconds::infinity();

    const auto nextTickTimeEpsilon = 1_ms;

    auto timeUntilNextTickForAnimationsWithFrameRate = [&](std::optional<FramesPerSecond> frameRate) -> std::optional<Seconds> {
        if (frameRate) {
            if (auto* controller = this->controller())
                return controller->timeUntilNextTickForAnimationsWithFrameRate(*frameRate);
        }
        return std::nullopt;
    };

    auto animationInterval = this->animationInterval();
    
    for (const auto& animation : m_animations) {
        if (!animation->isRelevant())
            continue;

        // Get the time until the next tick for this animation. This does not
        // account for the animation frame rate, only accounting for the timing
        // model and the playback rate.
        auto animationTimeToNextRequiredTick = animation->timeToNextTick();

        if (auto animationFrameRate = animation->frameRate()) {
            // Now let's get the time until any animation with this animation's frame rate would tick.
            // If that time is longer than what we previously computed without accounting for the frame
            // rate, we use this time instead since our animation wouldn't tick anyway since the
            // AnimationTimelinesController would ignore it. Doing this ensures that we don't schedule
            // updates that wouldn't actually yield any work and guarantees that in a page with only
            // animations as the source for scheduling updates, updates are only scheduled at the minimal
            // frame rate.
            auto timeToNextPossibleTickAccountingForFrameRate = timeUntilNextTickForAnimationsWithFrameRate(animationFrameRate);
            if (timeToNextPossibleTickAccountingForFrameRate && animationTimeToNextRequiredTick < *timeToNextPossibleTickAccountingForFrameRate)
                animationTimeToNextRequiredTick = *timeToNextPossibleTickAccountingForFrameRate - nextTickTimeEpsilon;
        }

        if (animationTimeToNextRequiredTick < animationInterval) {
            LOG_WITH_STREAM(Animations, stream << "DocumentTimeline " << this << " scheduleNextTick - scheduleAnimationResolution now");
            scheduleAnimationResolution();
            return;
        }
        scheduleDelay = std::min(scheduleDelay, animationTimeToNextRequiredTick);
    }

    if (scheduleDelay < Seconds::infinity()) {
        scheduleDelay = std::max(scheduleDelay - animationInterval, 0_s);
        LOG_WITH_STREAM(Animations, stream << "DocumentTimeline " << this << " scheduleNextTick - scheduleDelay " << scheduleDelay);
        m_tickScheduleTimer.startOneShot(scheduleDelay);
    }
}

void DocumentTimeline::animationAcceleratedRunningStateDidChange(WebAnimation& animation)
{
    m_acceleratedAnimationsPendingRunningStateChange.add(&animation);

    if (shouldRunUpdateAnimationsAndSendEventsIgnoringSuspensionState())
        scheduleAnimationResolution();
    else
        clearTickScheduleTimer();
}

void DocumentTimeline::applyPendingAcceleratedAnimations()
{
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    if (m_document && m_document->settings().threadedAnimationResolutionEnabled()) {
        m_acceleratedAnimationsPendingRunningStateChange.clear();
        if (CheckedPtr timelinesController = m_document->timelinesController()) {
            if (auto* acceleratedEffectStackUpdater = timelinesController->existingAcceleratedEffectStackUpdater())
                acceleratedEffectStackUpdater->updateEffectStacks();
        }
        return;
    }
#endif

    auto acceleratedAnimationsPendingRunningStateChange = std::exchange(m_acceleratedAnimationsPendingRunningStateChange, { });

    HashSet<KeyframeEffectStack*> keyframeEffectStacksToUpdate;

    bool hasForcedLayout = false;
    for (auto& animation : acceleratedAnimationsPendingRunningStateChange) {
        if (auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(animation->effect())) {
            if (!hasForcedLayout)
                hasForcedLayout |= keyframeEffect->forceLayoutIfNeeded();

            // If the animation is no longer relevant, apply the pending accelerations in isolation.
            if (!animation->isRelevant()) {
                keyframeEffect->applyPendingAcceleratedActions();
                continue;
            }

            // Otherwise, this accelerated animation exists in the context of an effect stack and we must
            // update all effects in that stack to ensure their sort order is respected.
            if (auto target = keyframeEffect->targetStyleable()) {
                auto* keyframeEffectStack = target ? target->keyframeEffectStack() : nullptr;
                if (keyframeEffectStack)
                    keyframeEffectStacksToUpdate.add(keyframeEffectStack);
            }
        }
    }

    for (auto* keyframeEffectStack : keyframeEffectStacksToUpdate)
        keyframeEffectStack->applyPendingAcceleratedActions();
}

void DocumentTimeline::enqueueAnimationEvent(AnimationEventBase& event)
{
    m_pendingAnimationEvents.append(event);
    if (m_shouldScheduleAnimationResolutionForNewPendingEvents)
        scheduleAnimationResolution();
}

bool DocumentTimeline::hasPendingAnimationEventForAnimation(const WebAnimation& animation) const
{
    return m_pendingAnimationEvents.containsIf([&](auto& event) {
        return event->animation() == &animation;
    });
}

AnimationEvents DocumentTimeline::prepareForPendingAnimationEventsDispatch()
{
    m_shouldScheduleAnimationResolutionForNewPendingEvents = true;
    return std::exchange(m_pendingAnimationEvents, { });
}

Vector<std::pair<String, double>> DocumentTimeline::acceleratedAnimationsForElement(Element& element) const
{
    ASSERT(m_document);
    auto* renderer = element.renderer();
    if (renderer && renderer->isComposited()) {
        auto* compositedRenderer = downcast<RenderBoxModelObject>(renderer);
        if (auto* graphicsLayer = compositedRenderer->layer()->backing()->graphicsLayer())
            return graphicsLayer->acceleratedAnimationsForTesting(m_document->settings());
    }
    return { };
}

unsigned DocumentTimeline::numberOfAnimationTimelineInvalidationsForTesting() const
{
    return m_numberOfAnimationTimelineInvalidationsForTesting;
}

ExceptionOr<Ref<WebAnimation>> DocumentTimeline::animate(Ref<CustomEffectCallback>&& callback, std::optional<std::variant<double, CustomAnimationOptions>>&& options)
{
    if (!m_document)
        return Exception { ExceptionCode::InvalidStateError };

    String id = emptyString();
    std::variant<FramesPerSecond, AnimationFrameRatePreset> frameRate = AnimationFrameRatePreset::Auto;
    std::optional<std::variant<double, EffectTiming>> customEffectOptions;

    if (options) {
        std::variant<double, EffectTiming> customEffectOptionsVariant;
        if (std::holds_alternative<double>(*options))
            customEffectOptionsVariant = std::get<double>(*options);
        else {
            auto customEffectOptions = std::get<CustomAnimationOptions>(*options);
            id = customEffectOptions.id;
            frameRate = customEffectOptions.frameRate;
            customEffectOptionsVariant = WTFMove(customEffectOptions);
        }
        customEffectOptions = customEffectOptionsVariant;
    }

    auto customEffectResult = CustomEffect::create(*m_document, WTFMove(callback), WTFMove(customEffectOptions));
    if (customEffectResult.hasException())
        return customEffectResult.releaseException();

    auto animation = WebAnimation::create(*document(), &customEffectResult.returnValue().get());
    animation->setId(WTFMove(id));
    animation->setBindingsFrameRate(WTFMove(frameRate));

    auto animationPlayResult = animation->play();
    if (animationPlayResult.hasException())
        return animationPlayResult.releaseException();

    return animation;
}

std::optional<FramesPerSecond> DocumentTimeline::maximumFrameRate() const
{
    if (!m_document || !m_document->page())
        return std::nullopt;
    return m_document->page()->displayNominalFramesPerSecond();
}

Seconds DocumentTimeline::convertTimelineTimeToOriginRelativeTime(Seconds timelineTime) const
{
    // https://drafts.csswg.org/web-animations-1/#ref-for-timeline-time-to-origin-relative-time
    return timelineTime + m_originTime;
}

} // namespace WebCore
