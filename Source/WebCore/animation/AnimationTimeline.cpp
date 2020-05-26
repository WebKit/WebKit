/*
 * Copyright (C) Canon Inc. 2016
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
#include "AnimationTimeline.h"

#include "Animation.h"
#include "AnimationEffect.h"
#include "AnimationList.h"
#include "CSSAnimation.h"
#include "CSSPropertyAnimation.h"
#include "CSSTransition.h"
#include "DocumentTimeline.h"
#include "Element.h"
#include "KeyframeEffect.h"
#include "KeyframeEffectStack.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "WebAnimationUtilities.h"
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

AnimationTimeline::AnimationTimeline() = default;
AnimationTimeline::~AnimationTimeline() = default;

void AnimationTimeline::forgetAnimation(WebAnimation* animation)
{
    m_allAnimations.removeFirst(animation);
}

void AnimationTimeline::animationTimingDidChange(WebAnimation& animation)
{
    updateGlobalPosition(animation);

    if (m_animations.add(&animation)) {
        m_allAnimations.append(makeWeakPtr(&animation));
        auto* timeline = animation.timeline();
        if (timeline && timeline != this)
            timeline->removeAnimation(animation);
    }
}

void AnimationTimeline::updateGlobalPosition(WebAnimation& animation)
{
    static uint64_t s_globalPosition = 0;
    if (!animation.globalPosition() && animation.canHaveGlobalPosition())
        animation.setGlobalPosition(++s_globalPosition);
}

void AnimationTimeline::removeAnimation(WebAnimation& animation)
{
    ASSERT(!animation.timeline() || animation.timeline() == this);
    m_animations.remove(&animation);
    if (is<KeyframeEffect>(animation.effect())) {
        if (auto* target = downcast<KeyframeEffect>(animation.effect())->targetElementOrPseudoElement()) {
            animationWasRemovedFromElement(animation, *target);
            target->ensureKeyframeEffectStack().removeEffect(*downcast<KeyframeEffect>(animation.effect()));
        }
    }
}

Optional<double> AnimationTimeline::bindingsCurrentTime()
{
    auto time = currentTime();
    if (!time)
        return WTF::nullopt;
    return secondsToWebAnimationsAPITime(*time);
}

void AnimationTimeline::animationWasAddedToElement(WebAnimation& animation, Element& element)
{
    if (is<CSSTransition>(animation) && downcast<CSSTransition>(animation).owningElement() == &element)
        element.ensureTransitions().add(&animation);
    else if (is<CSSAnimation>(animation) && downcast<CSSAnimation>(animation).owningElement() == &element)
        element.ensureCSSAnimations().add(&animation);
    else
        element.ensureWebAnimations().add(&animation);
}

static inline bool removeCSSTransitionFromMap(CSSTransition& transition, PropertyToTransitionMap& cssTransitionsByProperty)
{
    auto transitionIterator = cssTransitionsByProperty.find(transition.property());
    if (transitionIterator == cssTransitionsByProperty.end() || transitionIterator->value != &transition)
        return false;

    cssTransitionsByProperty.remove(transitionIterator);
    return true;
}

void AnimationTimeline::animationWasRemovedFromElement(WebAnimation& animation, Element& element)
{
    element.ensureTransitions().remove(&animation);
    element.ensureCSSAnimations().remove(&animation);
    element.ensureWebAnimations().remove(&animation);

    // Now, if we're dealing with a CSS Transition, we remove it from the m_elementToRunningCSSTransitionByCSSPropertyID map.
    // We don't need to do this for CSS Animations because their timing can be set via CSS to end, which would cause this
    // function to be called, but they should remain associated with their owning element until this is changed via a call
    // to the JS API or changing the target element's animation-name property.
    if (is<CSSTransition>(animation))
        removeDeclarativeAnimationFromListsForOwningElement(animation, element);
}

void AnimationTimeline::removeDeclarativeAnimationFromListsForOwningElement(WebAnimation& animation, Element& element)
{
    ASSERT(is<DeclarativeAnimation>(animation));

    if (is<CSSTransition>(animation)) {
        auto& transition = downcast<CSSTransition>(animation);
        if (!removeCSSTransitionFromMap(transition, element.ensureRunningTransitionsByProperty()))
            removeCSSTransitionFromMap(transition, element.ensureCompletedTransitionsByProperty());
    }
}

Vector<RefPtr<WebAnimation>> AnimationTimeline::animationsForElement(Element& element, Ordering ordering) const
{
    Vector<RefPtr<WebAnimation>> animations;

    if (ordering == Ordering::Sorted) {
        if (element.hasKeyframeEffects()) {
            for (auto& effect : element.ensureKeyframeEffectStack().sortedEffects())
                animations.append(effect->animation());
        }
    } else {
        if (auto* cssTransitions = element.transitions())
            animations.appendRange(cssTransitions->begin(), cssTransitions->end());
        if (auto* cssAnimations = element.cssAnimations())
            animations.appendRange(cssAnimations->begin(), cssAnimations->end());
        if (auto* webAnimations = element.webAnimations())
            animations.appendRange(webAnimations->begin(), webAnimations->end());
    }

    return animations;
}

void AnimationTimeline::removeCSSAnimationCreatedByMarkup(Element& element, CSSAnimation& cssAnimation)
{
    element.animationsCreatedByMarkup().remove(&cssAnimation);

    if (!element.hasKeyframeEffects())
        return;

    auto& keyframeEffectStack = element.ensureKeyframeEffectStack();
    auto* cssAnimationList = keyframeEffectStack.cssAnimationList();
    if (!cssAnimationList || cssAnimationList->isEmpty())
        return;

    auto& backingAnimation = cssAnimation.backingAnimation();
    for (size_t i = 0; i < cssAnimationList->size(); ++i) {
        if (cssAnimationList->animation(i) == backingAnimation) {
            auto newAnimationList = cssAnimationList->copy();
            newAnimationList->remove(i);
            keyframeEffectStack.setCSSAnimationList(WTFMove(newAnimationList));
            return;
        }
    }
}

void AnimationTimeline::elementWasRemoved(Element& element)
{
    cancelDeclarativeAnimationsForElement(element, WebAnimation::Silently::Yes);
}

void AnimationTimeline::willChangeRendererForElement(Element& element)
{
    for (auto& animation : animationsForElement(element))
        animation->willChangeRenderer();
}

void AnimationTimeline::cancelDeclarativeAnimationsForElement(Element& element, WebAnimation::Silently silently)
{
    if (auto* transitions = element.transitions()) {
        for (auto& cssTransition : *transitions)
            cssTransition->cancel(silently);
    }

    if (auto* cssAnimations = element.cssAnimations()) {
        for (auto& cssAnimation : *cssAnimations) {
            if (is<CSSAnimation>(cssAnimation))
                removeCSSAnimationCreatedByMarkup(element, downcast<CSSAnimation>(*cssAnimation));
            cssAnimation->cancel(silently);
        }
    }
}

static bool shouldConsiderAnimation(Element& element, const Animation& animation)
{
    if (!animation.isValidAnimation())
        return false;

    static NeverDestroyed<const String> animationNameNone(MAKE_STATIC_STRING_IMPL("none"));

    auto& name = animation.name();
    if (name == animationNameNone || name.isEmpty())
        return false;

    if (auto* styleScope = Style::Scope::forOrdinal(element, animation.nameStyleScopeOrdinal()))
        return styleScope->resolver().isAnimationNameValid(name);

    return false;
}

void AnimationTimeline::updateCSSAnimationsForElement(Element& element, const RenderStyle* currentStyle, const RenderStyle& newStyle)
{
    auto& keyframeEffectStack = element.ensureKeyframeEffectStack();

    // In case this element is newly getting a "display: none" we need to cancel all of its animations and disregard new ones.
    if (currentStyle && currentStyle->display() != DisplayType::None && newStyle.display() == DisplayType::None) {
        for (auto& cssAnimation : element.animationsCreatedByMarkup())
            cssAnimation->cancelFromStyle();
        keyframeEffectStack.setCSSAnimationList(nullptr);
        return;
    }

    auto* currentAnimationList = newStyle.animations();
    auto* previousAnimationList = keyframeEffectStack.cssAnimationList();
    if (previousAnimationList && !previousAnimationList->isEmpty() && newStyle.hasAnimations() && *(previousAnimationList) == *(newStyle.animations()))
        return;

    CSSAnimationCollection newAnimations;
    auto& previousAnimations = element.animationsCreatedByMarkup();

    // https://www.w3.org/TR/css-animations-1/#animations
    // The same @keyframes rule name may be repeated within an animation-name. Changes to the animation-name update existing
    // animations by iterating over the new list of animations from last to first, and, for each animation, finding the last
    // matching animation in the list of existing animations. If a match is found, the existing animation is updated using the
    // animation properties corresponding to its position in the new list of animations, whilst maintaining its current playback
    // time as described above. The matching animation is removed from the existing list of animations such that it will not match
    // twice. If a match is not found, a new animation is created. As a result, updating animation-name from ‘a’ to ‘a, a’ will
    // cause the existing animation for ‘a’ to become the second animation in the list and a new animation will be created for the
    // first item in the list.
    if (currentAnimationList) {
        for (size_t i = currentAnimationList->size(); i > 0; --i) {
            auto& currentAnimation = currentAnimationList->animation(i - 1);
            if (!shouldConsiderAnimation(element, currentAnimation))
                continue;

            bool foundMatchingAnimation = false;
            for (auto& previousAnimation : previousAnimations) {
                if (previousAnimation->animationName() == currentAnimation.name()) {
                    // Timing properties or play state may have changed so we need to update the backing animation with
                    // the Animation found in the current style.
                    previousAnimation->setBackingAnimation(currentAnimation);
                    newAnimations.add(previousAnimation);
                    // Remove the matched animation from the list of previous animations so we may not match it again.
                    previousAnimations.remove(previousAnimation);
                    foundMatchingAnimation = true;
                    break;
                }
            }

            if (!foundMatchingAnimation)
                newAnimations.add(CSSAnimation::create(element, currentAnimation, currentStyle, newStyle));
        }
    }

    // Any animation found in previousAnimations but not found in newAnimations is not longer current and should be canceled.
    for (auto& previousAnimation : previousAnimations) {
        if (!newAnimations.contains(previousAnimation)) {
            if (previousAnimation->owningElement())
                previousAnimation->cancelFromStyle();
        }
    }

    element.setAnimationsCreatedByMarkup(WTFMove(newAnimations));

    keyframeEffectStack.setCSSAnimationList(currentAnimationList);
}

static KeyframeEffect* keyframeEffectForElementAndProperty(Element& element, CSSPropertyID property)
{
    if (auto* keyframeEffectStack = element.keyframeEffectStack()) {
        auto effects = keyframeEffectStack->sortedEffects();
        for (const auto& effect : makeReversedRange(effects)) {
            if (effect->animatesProperty(property))
                return effect.get();
        }
    }

    return nullptr;
}

static bool propertyInStyleMatchesValueForTransitionInMap(CSSPropertyID property, const RenderStyle& style, PropertyToTransitionMap& transitions)
{
    if (auto* transition = transitions.get(property)) {
        if (CSSPropertyAnimation::propertiesEqual(property, &style, &transition->targetStyle()))
            return true;
    }
    return false;
}

static double transitionCombinedDuration(const Animation* transition)
{
    return std::max(0.0, transition->duration()) + transition->delay();
}

static bool transitionMatchesProperty(const Animation& transition, CSSPropertyID property)
{
    auto mode = transition.property().mode;
    if (mode == Animation::TransitionMode::None || mode == Animation::TransitionMode::UnknownProperty)
        return false;
    if (mode == Animation::TransitionMode::SingleProperty) {
        auto transitionProperty = transition.property().id;
        if (transitionProperty != property) {
            for (auto longhand : shorthandForProperty(transitionProperty)) {
                if (longhand == property)
                    return true;
            }
            return false;
        }
    }
    return true;
}

static void compileTransitionPropertiesInStyle(const RenderStyle& style, HashSet<CSSPropertyID>& transitionProperties, bool& transitionPropertiesContainAll)
{
    if (transitionPropertiesContainAll)
        return;

    auto* transitions = style.transitions();
    if (!transitions)
        return;

    for (size_t i = 0; i < transitions->size(); ++i) {
        const auto& animation = transitions->animation(i);
        auto mode = animation.property().mode;
        if (mode == Animation::TransitionMode::SingleProperty) {
            auto property = animation.property().id;
            if (isShorthandCSSProperty(property)) {
                for (auto longhand : shorthandForProperty(property))
                    transitionProperties.add(longhand);
            } else if (property != CSSPropertyInvalid)
                transitionProperties.add(property);
        } else if (mode == Animation::TransitionMode::All) {
            transitionPropertiesContainAll = true;
            return;
        }
    }
}

void AnimationTimeline::updateCSSTransitionsForElementAndProperty(Element& element, CSSPropertyID property, const RenderStyle& currentStyle, const RenderStyle& newStyle, const MonotonicTime generationTime)
{
    auto* keyframeEffect = keyframeEffectForElementAndProperty(element, property);
    auto* animation = keyframeEffect ? keyframeEffect->animation() : nullptr;
    bool isDeclarative = animation && is<DeclarativeAnimation>(animation) && downcast<DeclarativeAnimation>(*animation).owningElement() == &element;
    if (animation && !isDeclarative)
        return;

    const Animation* matchingBackingAnimation = nullptr;
    if (auto* transitions = newStyle.transitions()) {
        for (size_t i = 0; i < transitions->size(); ++i) {
            auto& backingAnimation = transitions->animation(i);
            if (transitionMatchesProperty(backingAnimation, property))
                matchingBackingAnimation = &backingAnimation;
        }
    }

    // https://drafts.csswg.org/css-transitions-1/#before-change-style
    // Define the before-change style as the computed values of all properties on the element as of the previous style change event, except with
    // any styles derived from declarative animations such as CSS Transitions, CSS Animations, and SMIL Animations updated to the current time.
    auto beforeChangeStyle = [&]() -> const RenderStyle {
        if (animation && animation->isRelevant()) {
            auto animatedStyle = RenderStyle::clone(currentStyle);
            animation->resolve(animatedStyle);
            return animatedStyle;
        }

        // If it exists, use the recorded RenderStyle for this element during a previous call to Style::TreeResolver::createAnimatedElementUpdate().
        if (auto* lastStyleChangeEventStyle = element.lastStyleChangeEventStyle())
            return RenderStyle::clone(*lastStyleChangeEventStyle);

        // If we haven't computed styles from animations for this element, the before-change style is the previously resolved style for this element.
        return RenderStyle::clone(currentStyle);
    }();

    // https://drafts.csswg.org/css-transitions-1/#after-change-style
    // Likewise, define the after-change style as the computed values of all properties on the element based on the information known at the start
    // of that style change event, but using the computed values of the animation-* properties from the before-change style, excluding any styles
    // from CSS Transitions in the computation, and inheriting from the after-change style of the parent. Note that this means the after-change
    // style does not differ from the before-change style due to newly created or canceled CSS Animations.
    auto afterChangeStyle = [&]() -> const RenderStyle {
        if (is<CSSAnimation>(animation) && animation->isRelevant()) {
            auto animatedStyle = RenderStyle::clone(newStyle);
            animation->resolve(animatedStyle);
            return animatedStyle;
        }

        return RenderStyle::clone(newStyle);
    }();

    if (!element.hasRunningTransitionsForProperty(property)
        && !CSSPropertyAnimation::propertiesEqual(property, &beforeChangeStyle, &afterChangeStyle)
        && CSSPropertyAnimation::canPropertyBeInterpolated(property, &beforeChangeStyle, &afterChangeStyle)
        && !propertyInStyleMatchesValueForTransitionInMap(property, afterChangeStyle, element.ensureCompletedTransitionsByProperty())
        && matchingBackingAnimation && transitionCombinedDuration(matchingBackingAnimation) > 0) {
        // 1. If all of the following are true:
        //   - the element does not have a running transition for the property,
        //   - the before-change style is different from and can be interpolated with the after-change style for that property,
        //   - the element does not have a completed transition for the property or the end value of the completed transition is different from the after-change style for the property,
        //   - there is a matching transition-property value, and
        //   - the combined duration is greater than 0s,

        // then implementations must remove the completed transition (if present) from the set of completed transitions
        element.ensureCompletedTransitionsByProperty().remove(property);

        // and start a transition whose:
        //   - start time is the time of the style change event plus the matching transition delay,
        //   - end time is the start time plus the matching transition duration,
        //   - start value is the value of the transitioning property in the before-change style,
        //   - end value is the value of the transitioning property in the after-change style,
        //   - reversing-adjusted start value is the same as the start value, and
        //   - reversing shortening factor is 1.
        auto delay = Seconds(matchingBackingAnimation->delay());
        auto duration = Seconds(matchingBackingAnimation->duration());
        auto& reversingAdjustedStartStyle = beforeChangeStyle;
        auto reversingShorteningFactor = 1;
        element.ensureRunningTransitionsByProperty().set(property, CSSTransition::create(element, property, generationTime, *matchingBackingAnimation, &beforeChangeStyle, afterChangeStyle, delay, duration, reversingAdjustedStartStyle, reversingShorteningFactor));
    } else if (element.hasCompletedTransitionsForProperty(property) && !propertyInStyleMatchesValueForTransitionInMap(property, afterChangeStyle, element.ensureCompletedTransitionsByProperty())) {
        // 2. Otherwise, if the element has a completed transition for the property and the end value of the completed transition is different from
        //    the after-change style for the property, then implementations must remove the completed transition from the set of completed transitions.
        element.ensureCompletedTransitionsByProperty().remove(property);
    }

    bool hasRunningTransition = element.hasRunningTransitionsForProperty(property);
    if ((hasRunningTransition || element.hasCompletedTransitionsForProperty(property)) && !matchingBackingAnimation) {
        // 3. If the element has a running transition or completed transition for the property, and there is not a matching transition-property
        //    value, then implementations must cancel the running transition or remove the completed transition from the set of completed transitions.
        if (hasRunningTransition)
            element.ensureRunningTransitionsByProperty().take(property)->cancel();
        else
            element.ensureCompletedTransitionsByProperty().remove(property);
    }

    if (matchingBackingAnimation && element.hasRunningTransitionsForProperty(property) && !propertyInStyleMatchesValueForTransitionInMap(property, afterChangeStyle, element.ensureRunningTransitionsByProperty())) {
        auto previouslyRunningTransition = element.ensureRunningTransitionsByProperty().take(property);
        auto& previouslyRunningTransitionCurrentStyle = previouslyRunningTransition->currentStyle();
        // 4. If the element has a running transition for the property, there is a matching transition-property value, and the end value of the running
        //    transition is not equal to the value of the property in the after-change style, then:
        if (CSSPropertyAnimation::propertiesEqual(property, &previouslyRunningTransitionCurrentStyle, &afterChangeStyle) || !CSSPropertyAnimation::canPropertyBeInterpolated(property, &currentStyle, &afterChangeStyle)) {
            // 1. If the current value of the property in the running transition is equal to the value of the property in the after-change style,
            //    or if these two values cannot be interpolated, then implementations must cancel the running transition.
            previouslyRunningTransition->cancelFromStyle();
        } else if (transitionCombinedDuration(matchingBackingAnimation) <= 0.0 || !CSSPropertyAnimation::canPropertyBeInterpolated(property, &previouslyRunningTransitionCurrentStyle, &afterChangeStyle)) {
            // 2. Otherwise, if the combined duration is less than or equal to 0s, or if the current value of the property in the running transition
            //    cannot be interpolated with the value of the property in the after-change style, then implementations must cancel the running transition.
            previouslyRunningTransition->cancelFromStyle();
        } else if (CSSPropertyAnimation::propertiesEqual(property, &previouslyRunningTransition->reversingAdjustedStartStyle(), &afterChangeStyle)) {
            // 3. Otherwise, if the reversing-adjusted start value of the running transition is the same as the value of the property in the after-change
            //    style (see the section on reversing of transitions for why these case exists), implementations must cancel the running transition
            previouslyRunningTransition->cancelFromStyle();

            // and start a new transition whose:
            //   - reversing-adjusted start value is the end value of the running transition,
            //   - reversing shortening factor is the absolute value, clamped to the range [0, 1], of the sum of:
            //       1. the output of the timing function of the old transition at the time of the style change event, times the reversing shortening factor of the old transition
            //       2. 1 minus the reversing shortening factor of the old transition.
            //   - start time is the time of the style change event plus:
            //       1. if the matching transition delay is nonnegative, the matching transition delay, or
            //       2. if the matching transition delay is negative, the product of the new transition’s reversing shortening factor and the matching transition delay,
            //   - end time is the start time plus the product of the matching transition duration and the new transition’s reversing shortening factor,
            //   - start value is the current value of the property in the running transition,
            //   - end value is the value of the property in the after-change style
            auto& reversingAdjustedStartStyle = previouslyRunningTransition->targetStyle();
            double transformedProgress = 1;
            if (auto* effect = previouslyRunningTransition->effect()) {
                if (auto computedTimingProgress = effect->getComputedTiming().progress)
                    transformedProgress = *computedTimingProgress;
            }
            auto reversingShorteningFactor = std::max(std::min(((transformedProgress * previouslyRunningTransition->reversingShorteningFactor()) + (1 - previouslyRunningTransition->reversingShorteningFactor())), 1.0), 0.0);
            auto delay = matchingBackingAnimation->delay() < 0 ? Seconds(matchingBackingAnimation->delay()) * reversingShorteningFactor : Seconds(matchingBackingAnimation->delay());
            auto duration = Seconds(matchingBackingAnimation->duration()) * reversingShorteningFactor;

            element.ensureRunningTransitionsByProperty().set(property, CSSTransition::create(element, property, generationTime, *matchingBackingAnimation, &previouslyRunningTransitionCurrentStyle, afterChangeStyle, delay, duration, reversingAdjustedStartStyle, reversingShorteningFactor));
        } else {
            // 4. Otherwise, implementations must cancel the running transition
            previouslyRunningTransition->cancelFromStyle();

            // and start a new transition whose:
            //   - start time is the time of the style change event plus the matching transition delay,
            //   - end time is the start time plus the matching transition duration,
            //   - start value is the current value of the property in the running transition,
            //   - end value is the value of the property in the after-change style,
            //   - reversing-adjusted start value is the same as the start value, and
            //   - reversing shortening factor is 1.
            auto delay = Seconds(matchingBackingAnimation->delay());
            auto duration = Seconds(matchingBackingAnimation->duration());
            auto& reversingAdjustedStartStyle = currentStyle;
            auto reversingShorteningFactor = 1;
            element.ensureRunningTransitionsByProperty().set(property, CSSTransition::create(element, property, generationTime, *matchingBackingAnimation, &previouslyRunningTransitionCurrentStyle, afterChangeStyle, delay, duration, reversingAdjustedStartStyle, reversingShorteningFactor));
        }
    }
}

void AnimationTimeline::updateCSSTransitionsForElement(Element& element, const RenderStyle& currentStyle, const RenderStyle& newStyle)
{
    // In case this element is newly getting a "display: none" we need to cancel all of its transitions and disregard new ones.
    if (currentStyle.hasTransitions() && currentStyle.display() != DisplayType::None && newStyle.display() == DisplayType::None) {
        if (element.hasRunningTransitions()) {
            auto runningTransitions = element.ensureRunningTransitionsByProperty();
            for (const auto& cssTransitionsByCSSPropertyIDMapItem : runningTransitions)
                cssTransitionsByCSSPropertyIDMapItem.value->cancelFromStyle();
        }
        return;
    }

    // Section 3 "Starting of transitions" from the CSS Transitions Level 1 specification.
    // https://drafts.csswg.org/css-transitions-1/#starting

    auto generationTime = MonotonicTime::now();

    // First, let's compile the list of all CSS properties found in the current style and the after-change style.
    bool transitionPropertiesContainAll = false;
    HashSet<CSSPropertyID> transitionProperties;
    compileTransitionPropertiesInStyle(currentStyle, transitionProperties, transitionPropertiesContainAll);
    compileTransitionPropertiesInStyle(newStyle, transitionProperties, transitionPropertiesContainAll);

    if (transitionPropertiesContainAll) {
        auto numberOfProperties = CSSPropertyAnimation::getNumProperties();
        for (int propertyIndex = 0; propertyIndex < numberOfProperties; ++propertyIndex) {
            Optional<bool> isShorthand;
            auto property = CSSPropertyAnimation::getPropertyAtIndex(propertyIndex, isShorthand);
            if (isShorthand && *isShorthand)
                continue;
            updateCSSTransitionsForElementAndProperty(element, property, currentStyle, newStyle, generationTime);
        }
        return;
    }

    for (auto property : transitionProperties)
        updateCSSTransitionsForElementAndProperty(element, property, currentStyle, newStyle, generationTime);
}

} // namespace WebCore
