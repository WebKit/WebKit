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
#include "CSSAnimation.h"
#include "CSSTransition.h"
#include "DeclarativeAnimation.h"
#include "Document.h"
#include "DocumentTimelinesController.h"
#include "EventNames.h"
#include "GraphicsLayer.h"
#include "KeyframeEffect.h"
#include "KeyframeEffectStack.h"
#include "Node.h"
#include "Page.h"
#include "PseudoElement.h"
#include "RenderElement.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "Settings.h"

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
    : AnimationTimeline()
    , m_tickScheduleTimer(*this, &DocumentTimeline::scheduleAnimationResolution)
    , m_document(makeWeakPtr(document))
    , m_originTime(originTime)
{
    if (auto* controller = this->controller())
        controller->addTimeline(*this);
    if (auto* page = document.page()) {
        if (page->settings().hiddenPageCSSAnimationSuspensionEnabled() && !page->isVisible())
            suspendAnimations();
    }
}

DocumentTimeline::~DocumentTimeline()
{
    if (auto* controller = this->controller())
        controller->removeTimeline(*this);
}

DocumentTimelinesController* DocumentTimeline::controller() const
{
    if (m_document)
        return &m_document->ensureTimelinesController();
    return nullptr;
}

void DocumentTimeline::detachFromDocument()
{
    Ref<DocumentTimeline> protectedThis(*this);
    if (auto* controller = this->controller())
        controller->removeTimeline(*this);

    m_pendingAnimationEvents.clear();
    m_elementsWithRunningAcceleratedAnimations.clear();

    auto& animationsToRemove = m_animations;
    while (!animationsToRemove.isEmpty())
        animationsToRemove.first()->remove();

    clearTickScheduleTimer();
    m_document = nullptr;
}

static inline bool compareDeclarativeAnimationOwningElementPositionsInDocumentTreeOrder(Element* lhsOwningElement, Element* rhsOwningElement)
{
    // With regard to pseudo-elements, the sort order is as follows:
    //     - element
    //     - ::before
    //     - ::after
    //     - element children
    
    // We could be comparing two pseudo-elements that are hosted on the same element.
    if (is<PseudoElement>(lhsOwningElement) && is<PseudoElement>(rhsOwningElement)) {
        auto* lhsPseudoElement = downcast<PseudoElement>(lhsOwningElement);
        auto* rhsPseudoElement = downcast<PseudoElement>(rhsOwningElement);
        if (lhsPseudoElement->hostElement() == rhsPseudoElement->hostElement())
            return lhsPseudoElement->isBeforePseudoElement();
    }

    // Or comparing a pseudo-element that is compared to another non-pseudo element, in which case
    // we want to see if it's hosted on that other element, and if not use its host element to compare.
    if (is<PseudoElement>(lhsOwningElement)) {
        auto* lhsHostElement = downcast<PseudoElement>(lhsOwningElement)->hostElement();
        if (rhsOwningElement == lhsHostElement)
            return false;
        lhsOwningElement = lhsHostElement;
    }

    if (is<PseudoElement>(rhsOwningElement)) {
        auto* rhsHostElement = downcast<PseudoElement>(rhsOwningElement)->hostElement();
        if (lhsOwningElement == rhsHostElement)
            return true;
        rhsOwningElement = rhsHostElement;
    }

    return lhsOwningElement->compareDocumentPosition(*rhsOwningElement) & Node::DOCUMENT_POSITION_FOLLOWING;
}

Vector<RefPtr<WebAnimation>> DocumentTimeline::getAnimations() const
{
    ASSERT(m_document);

    Vector<RefPtr<WebAnimation>> cssTransitions;
    Vector<RefPtr<WebAnimation>> cssAnimations;
    Vector<RefPtr<WebAnimation>> webAnimations;

    // First, let's get all qualifying animations in their right group.
    for (const auto& animation : m_animations) {
        if (!animation || !animation->isRelevant() || animation->timeline() != this || !is<KeyframeEffect>(animation->effect()))
            continue;

        auto* target = downcast<KeyframeEffect>(animation->effect())->target();
        if (!target || !target->isDescendantOf(*m_document))
            continue;

        if (is<CSSTransition>(animation.get()) && downcast<CSSTransition>(animation.get())->owningElement())
            cssTransitions.append(animation);
        else if (is<CSSAnimation>(animation.get()) && downcast<CSSAnimation>(animation.get())->owningElement())
            cssAnimations.append(animation);
        else
            webAnimations.append(animation);
    }

    // Now sort CSS Transitions by their composite order.
    std::stable_sort(cssTransitions.begin(), cssTransitions.end(), [](auto& lhs, auto& rhs) {
        // https://drafts.csswg.org/css-transitions-2/#animation-composite-order
        auto* lhsTransition = downcast<CSSTransition>(lhs.get());
        auto* rhsTransition = downcast<CSSTransition>(rhs.get());

        auto* lhsOwningElement = lhsTransition->owningElement();
        auto* rhsOwningElement = rhsTransition->owningElement();

        // If the owning element of A and B differs, sort A and B by tree order of their corresponding owning elements.
        if (lhsOwningElement != rhsOwningElement)
            return compareDeclarativeAnimationOwningElementPositionsInDocumentTreeOrder(lhsOwningElement, rhsOwningElement);

        // Otherwise, if A and B have different transition generation values, sort by their corresponding transition generation in ascending order.
        if (lhsTransition->generationTime() != rhsTransition->generationTime())
            return lhsTransition->generationTime() < rhsTransition->generationTime();

        // Otherwise, sort A and B in ascending order by the Unicode codepoints that make up the expanded transition property name of each transition
        // (i.e. without attempting case conversion and such that ‘-moz-column-width’ sorts before ‘column-width’).
        return lhsTransition->transitionProperty().utf8() < rhsTransition->transitionProperty().utf8();
    });

    // Now sort CSS Animations by their composite order.
    std::stable_sort(cssAnimations.begin(), cssAnimations.end(), [](auto& lhs, auto& rhs) {
        // https://drafts.csswg.org/css-animations-2/#animation-composite-order
        auto* lhsOwningElement = downcast<CSSAnimation>(lhs.get())->owningElement();
        auto* rhsOwningElement = downcast<CSSAnimation>(rhs.get())->owningElement();

        // If the owning element of A and B differs, sort A and B by tree order of their corresponding owning elements.
        if (lhsOwningElement != rhsOwningElement)
            return compareDeclarativeAnimationOwningElementPositionsInDocumentTreeOrder(lhsOwningElement, rhsOwningElement);

        // Otherwise, sort A and B based on their position in the computed value of the animation-name property of the (common) owning element.
        return compareAnimationsByCompositeOrder(*lhs, *rhs, lhsOwningElement->ensureKeyframeEffectStack().cssAnimationList());
    });

    std::stable_sort(webAnimations.begin(), webAnimations.end(), [](auto& lhs, auto& rhs) {
        return lhs->globalPosition() < rhs->globalPosition();
    });

    // Finally, we can concatenate the sorted CSS Transitions, CSS Animations and Web Animations in their relative composite order.
    Vector<RefPtr<WebAnimation>> animations;
    animations.appendRange(cssTransitions.begin(), cssTransitions.end());
    animations.appendRange(cssAnimations.begin(), cssAnimations.end());
    animations.appendRange(webAnimations.begin(), webAnimations.end());
    return animations;
}

Seconds DocumentTimeline::animationInterval() const
{
    if (!m_document || !m_document->page())
        return Seconds::infinity();
    return m_document->page()->preferredRenderingUpdateInterval();
}

void DocumentTimeline::suspendAnimations()
{
    for (const auto& animation : m_animations)
        animation->setSuspended(true);

    applyPendingAcceleratedAnimations();

    clearTickScheduleTimer();
}

void DocumentTimeline::resumeAnimations()
{
    for (const auto& animation : m_animations)
        animation->setSuspended(false);

    scheduleAnimationResolution();
}

bool DocumentTimeline::animationsAreSuspended() const
{
    return controller() && controller()->animationsAreSuspended();
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

Optional<Seconds> DocumentTimeline::currentTime()
{
    if (auto* controller = this->controller()) {
        if (auto currentTime = controller->currentTime())
            return *currentTime - m_originTime;
        return WTF::nullopt;
    }
    return AnimationTimeline::currentTime();
}

void DocumentTimeline::animationTimingDidChange(WebAnimation& animation)
{
    AnimationTimeline::animationTimingDidChange(animation);
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

    // We need some relevant animations or pending events to proceed.
    if (!shouldRunUpdateAnimationsAndSendEventsIgnoringSuspensionState())
        return;

    m_document->page()->scheduleTimedRenderingUpdate();
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

DocumentTimeline::ShouldUpdateAnimationsAndSendEvents DocumentTimeline::documentWillUpdateAnimationsAndSendEvents()
{
    // Updating animations and sending events may invalidate the timing of some animations, so we must set the m_animationResolutionScheduled
    // flag to false prior to running that procedure to allow animation with timing model updates to schedule updates.
    bool wasAnimationResolutionScheduled = std::exchange(m_animationResolutionScheduled, false);

    if (!wasAnimationResolutionScheduled || animationsAreSuspended() || !shouldRunUpdateAnimationsAndSendEventsIgnoringSuspensionState())
        return DocumentTimeline::ShouldUpdateAnimationsAndSendEvents::No;

    m_numberOfAnimationTimelineInvalidationsForTesting++;

    // enqueueAnimationEvent() calls scheduleAnimationResolution() to ensure that the "update animations and send events"
    // procedure is run and enqueued events are dispatched in the next frame. However, events that are enqueued while
    // this procedure is running should not schedule animation resolution until the event queue has been cleared.
    m_shouldScheduleAnimationResolutionForNewPendingEvents = false;

    return DocumentTimeline::ShouldUpdateAnimationsAndSendEvents::Yes;
}

void DocumentTimeline::documentDidUpdateAnimationsAndSendEvents()
{
    applyPendingAcceleratedAnimations();

    if (!m_animationResolutionScheduled)
        scheduleNextTick();
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
    auto* effect = animation.effect();
    if (!is<KeyframeEffect>(effect))
        return false;

    auto* keyframeEffect = downcast<KeyframeEffect>(effect);
    auto* target = keyframeEffect->target();
    if (!target || !target->isDescendantOf(*m_document))
        return false;

    HashSet<CSSPropertyID> propertiesToMatch = keyframeEffect->animatedProperties();
    auto animations = animationsForElement(*target, AnimationTimeline::Ordering::Sorted);
    for (auto& animationWithHigherCompositeOrder : WTF::makeReversedRange(animations)) {
        if (&animation == animationWithHigherCompositeOrder)
            break;

        if (animationWithHigherCompositeOrder && animationWithHigherCompositeOrder->isReplaceable()) {
            auto* effectWithHigherCompositeOrder = animationWithHigherCompositeOrder->effect();
            if (is<KeyframeEffect>(effectWithHigherCompositeOrder)) {
                auto* keyframeEffectWithHigherCompositeOrder = downcast<KeyframeEffect>(effectWithHigherCompositeOrder);
                for (auto cssPropertyId : keyframeEffectWithHigherCompositeOrder->animatedProperties()) {
                    if (propertiesToMatch.remove(cssPropertyId) && propertiesToMatch.isEmpty())
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

    Vector<RefPtr<WebAnimation>> animationsToRemove;

    // When asked to remove replaced animations for a Document, doc, then for every animation, animation
    for (auto& animation : m_allAnimations) {
        if (animation && animationCanBeRemoved(*animation)) {
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
            animation->enqueueAnimationPlaybackEvent(eventNames().removeEvent, animation->currentTime(), currentTime());

            animationsToRemove.append(animation.get());
        }
    }

    for (auto& animation : animationsToRemove) {
        if (auto* timeline = animation->timeline())
            timeline->removeAnimation(*animation);
    }
}

void DocumentTimeline::transitionDidComplete(RefPtr<CSSTransition> transition)
{
    ASSERT(transition);
    removeAnimation(*transition);
    if (is<KeyframeEffect>(transition->effect())) {
        if (auto* target = downcast<KeyframeEffect>(transition->effect())->targetElementOrPseudoElement())
            target->ensureCompletedTransitionsByProperty().set(transition->property(), transition);
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

    for (const auto& animation : m_animations) {
        if (!animation->isRelevant())
            continue;
        auto animationTimeToNextRequiredTick = animation->timeToNextTick();
        if (animationTimeToNextRequiredTick < animationInterval()) {
            scheduleAnimationResolution();
            return;
        }
        scheduleDelay = std::min(scheduleDelay, animationTimeToNextRequiredTick);
    }

    if (scheduleDelay < Seconds::infinity())
        m_tickScheduleTimer.startOneShot(scheduleDelay);
}

bool DocumentTimeline::computeExtentOfAnimation(RenderElement& renderer, LayoutRect& bounds) const
{
    if (!renderer.element())
        return true;

    KeyframeEffect* matchingEffect = nullptr;
    for (const auto& animation : animationsForElement(*renderer.element())) {
        auto* effect = animation->effect();
        if (is<KeyframeEffect>(effect)) {
            auto* keyframeEffect = downcast<KeyframeEffect>(effect);
            if (keyframeEffect->animatedProperties().contains(CSSPropertyTransform))
                matchingEffect = downcast<KeyframeEffect>(effect);
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
        if (is<KeyframeEffect>(effect) && downcast<KeyframeEffect>(effect)->animatedProperties().contains(property))
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
        if (is<KeyframeEffect>(effect)) {
            auto* keyframeEffect = downcast<KeyframeEffect>(effect);
            if (keyframeEffect->isCurrentlyAffectingProperty(property, KeyframeEffect::Accelerated::Yes))
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
            if (is<KeyframeEffect>(animation->effect()))
                downcast<KeyframeEffect>(animation->effect())->getAnimatedStyle(result);
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

    if (is<KeyframeEffect>(animation.effect())) {
        if (auto* target = downcast<KeyframeEffect>(animation.effect())->targetElementOrPseudoElement())
            updateListOfElementsWithRunningAcceleratedAnimationsForElement(*target);
    }

    if (shouldRunUpdateAnimationsAndSendEventsIgnoringSuspensionState())
        scheduleAnimationResolution();
    else
        clearTickScheduleTimer();
}

void DocumentTimeline::updateListOfElementsWithRunningAcceleratedAnimationsForElement(Element& element)
{
    auto animations = animationsForElement(element);

    if (animations.isEmpty()) {
        m_elementsWithRunningAcceleratedAnimations.remove(&element);
        return;
    }

    for (const auto& animation : animations) {
        if (!animation->isRunningAccelerated()) {
            m_elementsWithRunningAcceleratedAnimations.remove(&element);
            return;
        }
    }

    m_elementsWithRunningAcceleratedAnimations.add(&element);
}

void DocumentTimeline::applyPendingAcceleratedAnimations()
{
    auto acceleratedAnimationsPendingRunningStateChange = m_acceleratedAnimationsPendingRunningStateChange;
    m_acceleratedAnimationsPendingRunningStateChange.clear();

    bool hasForcedLayout = false;
    for (auto& animation : acceleratedAnimationsPendingRunningStateChange) {
        if (!hasForcedLayout) {
            auto* effect = animation->effect();
            if (is<KeyframeEffect>(effect))
                hasForcedLayout |= downcast<KeyframeEffect>(effect)->forceLayoutIfNeeded();
        }
        animation->applyPendingAcceleratedActions();
    }
}

bool DocumentTimeline::runningAnimationsForElementAreAllAccelerated(Element& element) const
{
    return m_elementsWithRunningAcceleratedAnimations.contains(&element);
}

void DocumentTimeline::enqueueAnimationEvent(AnimationEventBase& event)
{
    m_pendingAnimationEvents.append(event);
    if (m_shouldScheduleAnimationResolutionForNewPendingEvents)
        scheduleAnimationResolution();
}

AnimationEvents DocumentTimeline::prepareForPendingAnimationEventsDispatch()
{
    m_shouldScheduleAnimationResolutionForNewPendingEvents = true;
    return std::exchange(m_pendingAnimationEvents, { });
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
