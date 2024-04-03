/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "Styleable.h"

#include "Animation.h"
#include "AnimationEffect.h"
#include "AnimationList.h"
#include "AnimationTimeline.h"
#include "CSSAnimation.h"
#include "CSSCustomPropertyValue.h"
#include "CSSPropertyAnimation.h"
#include "CSSTransition.h"
#include "CommonAtomStrings.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentTimeline.h"
#include "Element.h"
#include "KeyframeEffect.h"
#include "KeyframeEffectStack.h"
#include "Quirks.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderElement.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderStyleInlines.h"
#include "RenderView.h"
#include "StyleCustomPropertyData.h"
#include "StyleOriginatedAnimation.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "WebAnimation.h"
#include "WebAnimationUtilities.h"
#include "WillChangeData.h"

namespace WebCore {

const std::optional<const Styleable> Styleable::fromRenderer(const RenderElement& renderer)
{
    switch (renderer.style().pseudoElementType()) {
    case PseudoId::Backdrop:
        for (auto& topLayerElement : renderer.document().topLayerElements()) {
            if (topLayerElement->renderer() && topLayerElement->renderer()->backdropRenderer() == &renderer)
                return Styleable(topLayerElement.get(), Style::PseudoElementIdentifier { PseudoId::Backdrop });
        }
        break;
    case PseudoId::Marker: {
        auto* ancestor = renderer.parent();
        while (ancestor) {
            auto* renderListItem = dynamicDowncast<RenderListItem>(ancestor);
            if (renderListItem && ancestor->element() && renderListItem->markerRenderer() == &renderer)
                return Styleable(*ancestor->element(), Style::PseudoElementIdentifier { PseudoId::Marker });
            ancestor = ancestor->parent();
        }
        break;
    }
    case PseudoId::ViewTransitionGroup:
    case PseudoId::ViewTransitionImagePair:
    case PseudoId::ViewTransitionNew:
    case PseudoId::ViewTransitionOld:
        if (auto* documentElement = renderer.document().documentElement())
            return Styleable(*documentElement, Style::PseudoElementIdentifier { renderer.style().pseudoElementType(), renderer.style().pseudoElementNameArgument() });
        break;
    case PseudoId::ViewTransition:
        if (auto* documentElement = renderer.document().documentElement())
            return Styleable(*documentElement, Style::PseudoElementIdentifier { PseudoId::ViewTransition });
        break;
    case PseudoId::After:
    case PseudoId::Before:
    case PseudoId::None:
        if (auto* element = renderer.element())
            return fromElement(*element);
        break;
    default:
        return std::nullopt;
    }

    return std::nullopt;
}

RenderElement* Styleable::renderer() const
{
    if (!pseudoElementIdentifier)
        return element.renderer();

    switch (pseudoElementIdentifier->pseudoId) {
    case PseudoId::After:
        if (auto* afterPseudoElement = element.afterPseudoElement())
            return afterPseudoElement->renderer();
        break;
    case PseudoId::Backdrop:
        if (auto* hostRenderer = element.renderer())
            return hostRenderer->backdropRenderer().get();
        break;
    case PseudoId::Before:
        if (auto* beforePseudoElement = element.beforePseudoElement())
            return beforePseudoElement->renderer();
        break;
    case PseudoId::Marker:
        if (auto* renderListItem = dynamicDowncast<RenderListItem>(element.renderer())) {
            auto* markerRenderer = renderListItem->markerRenderer();
            if (markerRenderer && !markerRenderer->style().hasUsedContentNone())
                return markerRenderer;
        }
        break;
    case PseudoId::ViewTransition:
        if (element.renderer())
            return element.renderer()->view().viewTransitionRoot().get();
        break;
    case PseudoId::ViewTransitionGroup:
    case PseudoId::ViewTransitionImagePair:
    case PseudoId::ViewTransitionNew:
    case PseudoId::ViewTransitionOld: {
        if (!element.renderer())
            return nullptr;

        WeakPtr viewTransitionRoot = element.renderer()->view().viewTransitionRoot();
        if (!viewTransitionRoot)
            return nullptr;

        // Find the right ::view-transition-group().
        RenderBlockFlow* correctGroup = nullptr;
        for (auto& group : childrenOfType<RenderBlockFlow>(*viewTransitionRoot.get())) {
            if (group.style().pseudoElementNameArgument() == pseudoElementIdentifier->nameArgument) {
                correctGroup = &group;
                break;
            }
        }

        // If we can't find the correct group, return nullptr.
        if (!correctGroup)
            return nullptr;

        // Return early if we're looking for ::view-transition-group().
        if (pseudoElementIdentifier->pseudoId == PseudoId::ViewTransitionGroup)
            return correctGroup;

        // Go through all descendants until we find the relevant pseudo element otherwise.
        for (auto& descendant : descendantsOfType<RenderBox>(*correctGroup)) {
            if (descendant.style().pseudoElementType() == pseudoElementIdentifier->pseudoId)
                return &descendant;
        }
        break;
    }
    default:
        return nullptr;
    }

    return nullptr;
}

std::unique_ptr<RenderStyle> Styleable::computeAnimatedStyle() const
{
    std::unique_ptr<RenderStyle> animatedStyle;

    auto* effectStack = keyframeEffectStack();
    if (!effectStack)
        return animatedStyle;

    for (const auto& effect : effectStack->sortedEffects())
        effect->getAnimatedStyle(animatedStyle);

    return animatedStyle;
}

bool Styleable::computeAnimationExtent(LayoutRect& bounds) const
{
    auto* animations = this->animations();
    if (!animations)
        return false;

    KeyframeEffect* matchingEffect = nullptr;
    for (const auto& animation : *animations) {
        if (auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(animation->effect())) {
            if (keyframeEffect->animatedProperties().contains(CSSPropertyTransform))
                matchingEffect = keyframeEffect;
        }
    }

    if (matchingEffect)
        return matchingEffect->computeExtentOfTransformAnimation(bounds);

    return true;
}

bool Styleable::mayHaveNonZeroOpacity() const
{
    auto* renderer = this->renderer();
    if (!renderer)
        return false;

    if (renderer->style().opacity() != 0.0f)
        return true;

    if (renderer->style().willChange() && renderer->style().willChange()->containsProperty(CSSPropertyOpacity))
        return true;

    auto* effectStack = keyframeEffectStack();
    if (!effectStack || !effectStack->hasEffects())
        return false;

    for (const auto& effect : effectStack->sortedEffects()) {
        if (effect->animatesProperty(CSSPropertyOpacity))
            return true;
    }

    return false;
}

bool Styleable::isRunningAcceleratedTransformAnimation() const
{
    auto* effectStack = keyframeEffectStack();
    if (!effectStack)
        return false;

    for (const auto& effect : effectStack->sortedEffects()) {
        if (effect->isCurrentlyAffectingProperty(CSSPropertyTransform, KeyframeEffect::Accelerated::Yes))
            return true;
    }

    return false;
}

bool Styleable::hasRunningAcceleratedAnimations() const
{
    if (auto* effectStack = keyframeEffectStack())
        return effectStack->hasAcceleratedEffects(element.document().settings());
    return false;
}

void Styleable::animationWasAdded(WebAnimation& animation) const
{
    ensureAnimations().add(animation);
}

static inline bool removeCSSTransitionFromMap(CSSTransition& transition, AnimatableCSSPropertyToTransitionMap& cssTransitionsByProperty)
{
    auto transitionIterator = cssTransitionsByProperty.find(transition.property());
    if (transitionIterator == cssTransitionsByProperty.end() || transitionIterator->value.ptr() != &transition)
        return false;

    cssTransitionsByProperty.remove(transitionIterator);
    return true;
}

void Styleable::removeStyleOriginatedAnimationFromListsForOwningElement(WebAnimation& animation) const
{
    ASSERT(is<StyleOriginatedAnimation>(animation));

    if (auto* transition = dynamicDowncast<CSSTransition>(animation)) {
        if (!removeCSSTransitionFromMap(*transition, ensureRunningTransitionsByProperty()))
            removeCSSTransitionFromMap(*transition, ensureCompletedTransitionsByProperty());
    }
}

void Styleable::animationWasRemoved(WebAnimation& animation) const
{
    ensureAnimations().remove(animation);

    // Now, if we're dealing with a CSS Transition, we remove it from the m_elementToRunningCSSTransitionByAnimatableCSSProperty map.
    // We don't need to do this for CSS Animations because their timing can be set via CSS to end, which would cause this
    // function to be called, but they should remain associated with their owning element until this is changed via a call
    // to the JS API or changing the target element's animation-name property.
    if (is<CSSTransition>(animation))
        removeStyleOriginatedAnimationFromListsForOwningElement(animation);
}

void Styleable::elementWasRemoved() const
{
    cancelStyleOriginatedAnimations();
}

void Styleable::willChangeRenderer() const
{
    if (auto* animations = this->animations()) {
        for (const auto& animation : *animations)
            animation->willChangeRenderer();
    }
}

void Styleable::cancelStyleOriginatedAnimations(const WeakStyleOriginatedAnimations& animationsToCancelSilently) const
{
    if (auto* animations = this->animations()) {
        for (auto& animation : *animations) {
            if (auto* styleOriginatedAnimation = dynamicDowncast<StyleOriginatedAnimation>(animation.get())) {
                styleOriginatedAnimation->cancelFromStyle(animationsToCancelSilently.contains(styleOriginatedAnimation) ? WebAnimation::Silently::Yes : WebAnimation::Silently::No);
                setLastStyleChangeEventStyle(nullptr);
            }
        }
    }

    if (auto* effectStack = keyframeEffectStack())
        effectStack->setCSSAnimationList(nullptr);

    setAnimationsCreatedByMarkup({ });
}

static bool keyframesRuleExistsForAnimation(Element& element, const Animation& animation, const String& animationName)
{
    auto* styleScope = Style::Scope::forOrdinal(element, animation.name().scopeOrdinal);
    return styleScope && styleScope->resolver().isAnimationNameValid(animationName);
}

bool Styleable::animationListContainsNewlyValidAnimation(const AnimationList& animations) const
{
    auto& keyframeEffectStack = ensureKeyframeEffectStack();
    if (!keyframeEffectStack.hasInvalidCSSAnimationNames())
        return false;

    for (auto& animation : animations) {
        auto& name = animation->name().name;
        if (name != noneAtom() && !name.isEmpty() && keyframeEffectStack.containsInvalidCSSAnimationName(name) && keyframesRuleExistsForAnimation(element, animation.get(), name))
            return true;
    }

    return false;
}

void Styleable::updateCSSAnimations(const RenderStyle* currentStyle, const RenderStyle& newStyle, const Style::ResolutionContext& resolutionContext, WeakStyleOriginatedAnimations& newStyleOriginatedAnimations) const
{
    auto& keyframeEffectStack = ensureKeyframeEffectStack();

    // In case this element is newly getting a "display: none" we need to cancel all of its animations and disregard new ones.
    if ((!currentStyle || currentStyle->display() != DisplayType::None) && newStyle.display() == DisplayType::None) {
        for (auto& cssAnimation : animationsCreatedByMarkup())
            cssAnimation->cancelFromStyle();
        keyframeEffectStack.setCSSAnimationList(nullptr);
        setAnimationsCreatedByMarkup({ });
        return;
    }

    auto* currentAnimationList = newStyle.animations();
    auto* previousAnimationList = keyframeEffectStack.cssAnimationList();
    if (!element.hasPendingKeyframesUpdate(pseudoElementIdentifier) && previousAnimationList && !previousAnimationList->isEmpty() && newStyle.hasAnimations() && *(previousAnimationList) == *(newStyle.animations()) && !animationListContainsNewlyValidAnimation(*newStyle.animations()))
        return;

    CSSAnimationCollection newAnimations;
    auto& previousAnimations = animationsCreatedByMarkup();

    keyframeEffectStack.clearInvalidCSSAnimationNames();

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
        for (auto& currentAnimation : makeReversedRange(*currentAnimationList)) {
            if (!currentAnimation->isValidAnimation())
                continue;

            auto& animationName = currentAnimation->name().name;
            if (animationName == noneAtom() || animationName.isEmpty())
                continue;

            if (!keyframesRuleExistsForAnimation(element, currentAnimation.get(), animationName)) {
                keyframeEffectStack.addInvalidCSSAnimationName(animationName);
                continue;
            }

            bool foundMatchingAnimation = false;
            for (auto& previousAnimation : previousAnimations) {
                if (previousAnimation->animationName() == animationName) {
                    // Timing properties or play state may have changed so we need to update the backing animation with
                    // the Animation found in the current style.
                    previousAnimation->setBackingAnimation(currentAnimation.get());
                    // Keyframes may have been cleared if the @keyframes rules was changed since
                    // the last style update, so we must ensure keyframes are picked up.
                    previousAnimation->updateKeyframesIfNeeded(currentStyle, newStyle, resolutionContext);
                    newStyleOriginatedAnimations.append(previousAnimation.ptr());
                    newAnimations.add(previousAnimation);
                    // Remove the matched animation from the list of previous animations so we may not match it again.
                    previousAnimations.remove(previousAnimation);
                    foundMatchingAnimation = true;
                    break;
                }
            }

            if (!foundMatchingAnimation) {
                auto cssAnimation = CSSAnimation::create(*this, currentAnimation.get(), currentStyle, newStyle, resolutionContext);
                newStyleOriginatedAnimations.append(cssAnimation.ptr());
                newAnimations.add(WTFMove(cssAnimation));
            }
        }
    }

    // Any animation found in previousAnimations but not found in newAnimations is not longer current and should be canceled.
    for (auto& previousAnimation : previousAnimations) {
        if (!newAnimations.contains(previousAnimation)) {
            if (previousAnimation->owningElement())
                previousAnimation->cancelFromStyle();
        }
    }

    setAnimationsCreatedByMarkup(WTFMove(newAnimations));

    keyframeEffectStack.setCSSAnimationList(currentAnimationList);

    element.cssAnimationsDidUpdate(pseudoElementIdentifier);
}

static KeyframeEffect* keyframeEffectForElementAndProperty(const Styleable& styleable, const AnimatableCSSProperty& property)
{
    if (auto* keyframeEffectStack = styleable.keyframeEffectStack()) {
        auto effects = keyframeEffectStack->sortedEffects();
        for (const auto& effect : makeReversedRange(effects)) {
            if (effect->animatesProperty(property))
                return effect.get();
        }
    }

    return nullptr;
}

static bool propertyInStyleMatchesValueForTransitionInMap(const AnimatableCSSProperty& property, const RenderStyle& style, AnimatableCSSPropertyToTransitionMap& transitions, const Document& document)
{
    if (auto* transition = transitions.get(property)) {
        if (CSSPropertyAnimation::propertiesEqual(property, style, transition->targetStyle(), document))
            return true;
    }
    return false;
}

static bool transitionMatchesProperty(const Animation& transition, const AnimatableCSSProperty& property, const RenderStyle& style)
{
    if (transition.isPropertyFilled())
        return false;

    switch (transition.property().mode) {
    case Animation::TransitionMode::All:
        return true;
    case Animation::TransitionMode::None:
    case Animation::TransitionMode::UnknownProperty:
        return false;
    case Animation::TransitionMode::SingleProperty: {
        return WTF::switchOn(transition.property().animatableProperty,
            [&] (CSSPropertyID propertyId) {
                if (!std::holds_alternative<CSSPropertyID>(property))
                    return false;
                auto propertyIdToMatch = std::get<CSSPropertyID>(property);
                auto resolvedPropertyId = CSSProperty::resolveDirectionAwareProperty(propertyId, style.direction(), style.writingMode());
                if (resolvedPropertyId == propertyIdToMatch)
                    return true;
                for (auto longhand : shorthandForProperty(resolvedPropertyId)) {
                    auto resolvedLonghand = CSSProperty::resolveDirectionAwareProperty(longhand, style.direction(), style.writingMode());
                    if (resolvedLonghand == propertyIdToMatch)
                        return true;
                }
                return false;
            },
            [&] (const AtomString& customProperty) {
                if (!std::holds_alternative<AtomString>(property))
                    return false;
                return std::get<AtomString>(property) == customProperty;
            }
        );
    }
    }

    ASSERT_NOT_REACHED();
    return false;
}

static void compileTransitionPropertiesInStyle(const RenderStyle& style, CSSPropertiesBitSet& transitionProperties, HashSet<AtomString>& transitionCustomProperties, bool& transitionPropertiesContainAll)
{
    auto* transitions = style.transitions();
    if (!transitions) {
        // If we don't have any transitions in the map, this means that the initial value "all 0s" was set.
        transitionPropertiesContainAll = true;
        return;
    }

    for (const auto& transition : *transitions) {
        auto transitionProperty = transition->property();
        switch (transitionProperty.mode) {
        case Animation::TransitionMode::All:
            transitionPropertiesContainAll = true;
            break;
        case Animation::TransitionMode::None:
        case Animation::TransitionMode::UnknownProperty:
            break;
        case Animation::TransitionMode::SingleProperty: {
            WTF::switchOn(transitionProperty.animatableProperty,
                [&] (CSSPropertyID propertyId) {
                    auto resolvedPropertyId = CSSProperty::resolveDirectionAwareProperty(propertyId, style.direction(), style.writingMode());
                    if (isShorthand(resolvedPropertyId)) {
                        for (auto longhand : shorthandForProperty(resolvedPropertyId))
                            transitionProperties.m_properties.set(longhand);
                    } else if (resolvedPropertyId != CSSPropertyInvalid)
                        transitionProperties.m_properties.set(resolvedPropertyId);
                },
                [&] (const AtomString& customProperty) {
                    transitionCustomProperties.add(customProperty);
                }
            );
            break;
        }
        }
    }
}

static void updateCSSTransitionsForStyleableAndProperty(const Styleable& styleable, const AnimatableCSSProperty& property, const RenderStyle& currentStyle, const RenderStyle& newStyle, const MonotonicTime generationTime, WeakStyleOriginatedAnimations& newStyleOriginatedAnimations)
{
    auto* keyframeEffect = keyframeEffectForElementAndProperty(styleable, property);
    auto* animation = keyframeEffect ? keyframeEffect->animation() : nullptr;

    bool isDeclarative = false;
    if (auto* styleOriginatedAnimation = dynamicDowncast<StyleOriginatedAnimation>(animation)) {
        if (auto owningElement = styleOriginatedAnimation->owningElement())
            isDeclarative = *owningElement == styleable;
    }

    if (animation && !isDeclarative)
        return;

    auto& document = styleable.element.document();

    auto hasMatchingTransitionProperty = false;
    auto matchingTransitionDuration = 0.0;
    const Animation* matchingBackingAnimation = nullptr;
    if (auto* transitions = newStyle.transitions()) {
        for (auto& backingAnimation : *transitions) {
            if (transitionMatchesProperty(backingAnimation.get(), property, newStyle)) {
                hasMatchingTransitionProperty = true;
                matchingBackingAnimation = backingAnimation.ptr();
                matchingTransitionDuration = std::max(0.0, matchingBackingAnimation->duration()) + matchingBackingAnimation->delay();
            }
        }
    } else if (!document.quirks().needsResettingTransitionCancelsRunningTransitionQuirk()) {
        // If we don't have any transitions in the map, this means that the initial value "all 0s" was set
        // and thus all properties match.
        hasMatchingTransitionProperty = true;
    }

    auto effectTargetsProperty = [&property](KeyframeEffect& effect) {
        if (effect.animatesProperty(property))
            return true;
        if (auto* transition = dynamicDowncast<CSSTransition>(effect.animation()))
            return transition->property() == property;
        return false;
    };

    // https://drafts.csswg.org/css-transitions-1/#before-change-style
    // Define the before-change style as the computed values of all properties on the element as of the previous style change event, except with
    // any styles derived from declarative animations such as CSS Transitions, CSS Animations, and SMIL Animations updated to the current time.
    auto beforeChangeStyle = [&]() -> const RenderStyle {
        if (auto* lastStyleChangeEventStyle = styleable.lastStyleChangeEventStyle()) {
            auto style = RenderStyle::clone(*lastStyleChangeEventStyle);
            if (auto* keyframeEffectStack = styleable.keyframeEffectStack()) {
                for (const auto& effect : keyframeEffectStack->sortedEffects()) {
                    if (!effectTargetsProperty(*effect))
                        continue;
                    auto* effectAnimation = effect->animation();
                    auto* cssTransition = dynamicDowncast<CSSTransition>(effectAnimation);
                    bool shouldUseTimelineTimeAtCreation = cssTransition && (!effectAnimation->startTime() || *effectAnimation->startTime() == document.timeline().currentTime());
                    effectAnimation->resolve(style, { nullptr }, shouldUseTimelineTimeAtCreation ? cssTransition->timelineTimeAtCreation() : std::nullopt);
                }
            }
            return style;
        }
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
            animation->resolve(animatedStyle, { nullptr });
            return animatedStyle;
        }

        return RenderStyle::clone(newStyle);
    }();

    auto allowsDiscreteTransitions = matchingBackingAnimation && matchingBackingAnimation->allowsDiscreteTransitions();
    auto propertyCanBeInterpolated = [&](const AnimatableCSSProperty& property, const RenderStyle& a, const RenderStyle& b) {
        return allowsDiscreteTransitions || CSSPropertyAnimation::canPropertyBeInterpolated(property, a, b, document);
    };

    auto createCSSTransition = [&](const RenderStyle& oldStyle, Seconds delay, Seconds duration, const RenderStyle& reversingAdjustedStartStyle, double reversingShorteningFactor) {
        auto cssTransition = CSSTransition::create(styleable, property, generationTime, *matchingBackingAnimation, oldStyle, afterChangeStyle, delay, duration, reversingAdjustedStartStyle, reversingShorteningFactor);
        newStyleOriginatedAnimations.append(cssTransition.ptr());
        styleable.ensureRunningTransitionsByProperty().set(property, WTFMove(cssTransition));
    };

    bool hasRunningTransition = styleable.hasRunningTransitionForProperty(property);
    if (!hasRunningTransition
        && hasMatchingTransitionProperty && matchingTransitionDuration > 0
        && !CSSPropertyAnimation::propertiesEqual(property, beforeChangeStyle, afterChangeStyle, document)
        && propertyCanBeInterpolated(property, beforeChangeStyle, afterChangeStyle)
        && !propertyInStyleMatchesValueForTransitionInMap(property, afterChangeStyle, styleable.ensureCompletedTransitionsByProperty(), document)) {
        // 1. If all of the following are true:
        //   - the element does not have a running transition for the property,
        //   - the before-change style is different from and can be interpolated with the after-change style for that property,
        //   - the element does not have a completed transition for the property or the end value of the completed transition is different from the after-change style for the property,
        //   - there is a matching transition-property value, and
        //   - the combined duration is greater than 0s,

        // then implementations must remove the completed transition (if present) from the set of completed transitions
        styleable.ensureCompletedTransitionsByProperty().remove(property);

        // and start a transition whose:
        //   - start time is the time of the style change event plus the matching transition delay,
        //   - end time is the start time plus the matching transition duration,
        //   - start value is the value of the transitioning property in the before-change style,
        //   - end value is the value of the transitioning property in the after-change style,
        //   - reversing-adjusted start value is the same as the start value, and
        //   - reversing shortening factor is 1.
        ASSERT(matchingBackingAnimation);
        auto delay = Seconds(matchingBackingAnimation->delay());
        auto duration = Seconds(matchingBackingAnimation->duration());
        auto& reversingAdjustedStartStyle = beforeChangeStyle;
        auto reversingShorteningFactor = 1;
        createCSSTransition(beforeChangeStyle, delay, duration, reversingAdjustedStartStyle, reversingShorteningFactor);
        ASSERT(styleable.hasRunningTransitionForProperty(property));
        hasRunningTransition = true;
    } else if (styleable.hasCompletedTransitionForProperty(property) && !propertyInStyleMatchesValueForTransitionInMap(property, afterChangeStyle, styleable.ensureCompletedTransitionsByProperty(), document)) {
        // 2. Otherwise, if the element has a completed transition for the property and the end value of the completed transition is different from
        //    the after-change style for the property, then implementations must remove the completed transition from the set of completed transitions.
        styleable.ensureCompletedTransitionsByProperty().remove(property);
    }

    if (!hasMatchingTransitionProperty) {
        // 3. If the element has a running transition or completed transition for the property, and there is not a matching transition-property
        //    value, then implementations must cancel the running transition or remove the completed transition from the set of completed transitions.
        if (hasRunningTransition)
            styleable.ensureRunningTransitionsByProperty().take(property)->cancelFromStyle();
        else if (styleable.hasCompletedTransitionForProperty(property))
            styleable.ensureCompletedTransitionsByProperty().remove(property);
        return;
    }

    ASSERT(hasMatchingTransitionProperty);
    if (hasRunningTransition && !propertyInStyleMatchesValueForTransitionInMap(property, afterChangeStyle, styleable.ensureRunningTransitionsByProperty(), document)) {
        auto previouslyRunningTransition = styleable.ensureRunningTransitionsByProperty().take(property);
        auto& previouslyRunningTransitionCurrentStyle = previouslyRunningTransition->currentStyle();
        // 4. If the element has a running transition for the property, there is a matching transition-property value, and the end value of the running
        //    transition is not equal to the value of the property in the after-change style, then:
        if (CSSPropertyAnimation::propertiesEqual(property, previouslyRunningTransitionCurrentStyle, afterChangeStyle, document) || !propertyCanBeInterpolated(property, currentStyle, afterChangeStyle)) {
            // 1. If the current value of the property in the running transition is equal to the value of the property in the after-change style,
            //    or if these two values cannot be interpolated, then implementations must cancel the running transition.
            previouslyRunningTransition->cancelFromStyle();
        } else if (matchingTransitionDuration <= 0.0 || !propertyCanBeInterpolated(property, previouslyRunningTransitionCurrentStyle, afterChangeStyle)) {
            // 2. Otherwise, if the combined duration is less than or equal to 0s, or if the current value of the property in the running transition
            //    cannot be interpolated with the value of the property in the after-change style, then implementations must cancel the running transition.
            previouslyRunningTransition->cancelFromStyle();
        } else if (CSSPropertyAnimation::propertiesEqual(property, previouslyRunningTransition->reversingAdjustedStartStyle(), afterChangeStyle, document)) {
            // 3. Otherwise, if the reversing-adjusted start value of the running transition is the same as the value of the property in the after-change
            //    style (see the section on reversing of transitions for why these case exists), implementations must cancel the running transition
            //    and start a new transition whose:
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
            ASSERT(matchingBackingAnimation);
            auto& reversingAdjustedStartStyle = previouslyRunningTransition->targetStyle();
            double transformedProgress = 1;
            if (auto* effect = previouslyRunningTransition->effect()) {
                if (auto computedTimingProgress = effect->getComputedTiming().progress)
                    transformedProgress = *computedTimingProgress;
            }
            auto reversingShorteningFactor = std::max(std::min(((transformedProgress * previouslyRunningTransition->reversingShorteningFactor()) + (1 - previouslyRunningTransition->reversingShorteningFactor())), 1.0), 0.0);
            auto delay = matchingBackingAnimation->delay() < 0 ? Seconds(matchingBackingAnimation->delay()) * reversingShorteningFactor : Seconds(matchingBackingAnimation->delay());
            auto duration = Seconds(matchingBackingAnimation->duration()) * reversingShorteningFactor;

            previouslyRunningTransition->cancelFromStyle();
            createCSSTransition(beforeChangeStyle, delay, duration, reversingAdjustedStartStyle, reversingShorteningFactor);
        } else {
            // 4. Otherwise, implementations must cancel the running transition and start a new transition whose:
            //   - start time is the time of the style change event plus the matching transition delay,
            //   - end time is the start time plus the matching transition duration,
            //   - start value is the current value of the property in the running transition,
            //   - end value is the value of the property in the after-change style,
            //   - reversing-adjusted start value is the same as the start value, and
            //   - reversing shortening factor is 1.
            ASSERT(matchingBackingAnimation);
            auto delay = Seconds(matchingBackingAnimation->delay());
            auto duration = Seconds(matchingBackingAnimation->duration());
            auto& reversingAdjustedStartStyle = currentStyle;
            auto reversingShorteningFactor = 1;
            previouslyRunningTransition->cancelFromStyle();
            createCSSTransition(previouslyRunningTransitionCurrentStyle, delay, duration, reversingAdjustedStartStyle, reversingShorteningFactor);
        }
    }
}

void Styleable::updateCSSTransitions(const RenderStyle& currentStyle, const RenderStyle& newStyle, WeakStyleOriginatedAnimations& newStyleOriginatedAnimations) const
{
    // In case this element previous had "display: none" we can stop considering transitions altogether.
    if (currentStyle.display() == DisplayType::None)
        return;

    // In case this element is newly getting a "display: none" we need to cancel all of its transitions and disregard new ones.
    if (currentStyle.hasTransitions() && currentStyle.display() != DisplayType::None && newStyle.display() == DisplayType::None) {
        if (hasRunningTransitions()) {
            auto runningTransitions = ensureRunningTransitionsByProperty();
            for (const auto& cssTransitionsByAnimatableCSSPropertyMapItem : runningTransitions)
                cssTransitionsByAnimatableCSSPropertyMapItem.value->cancelFromStyle();
        }
        return;
    }

    // Section 3 "Starting of transitions" from the CSS Transitions Level 1 specification.
    // https://drafts.csswg.org/css-transitions-1/#starting

    auto generationTime = MonotonicTime::now();

    // First, let's compile the list of all CSS properties found in the current style and the after-change style.
    bool transitionPropertiesContainAll = false;
    CSSPropertiesBitSet transitionProperties;
    HashSet<AtomString> transitionCustomProperties;
    compileTransitionPropertiesInStyle(currentStyle, transitionProperties, transitionCustomProperties, transitionPropertiesContainAll);
    compileTransitionPropertiesInStyle(newStyle, transitionProperties, transitionCustomProperties, transitionPropertiesContainAll);

    if (transitionPropertiesContainAll) {
        auto addProperty = [&](const AnimatableCSSProperty& property) {
            WTF::switchOn(property,
                [&](CSSPropertyID propertyId) {
                    if (isShorthand(propertyId)) {
                        for (auto longhand : shorthandForProperty(propertyId))
                            transitionProperties.m_properties.set(longhand);
                    } else if (propertyId != CSSPropertyInvalid)
                        transitionProperties.m_properties.set(propertyId);
                },
                [&] (const AtomString&) { }
            );
        };

        const RenderStyle* targetStyle = &currentStyle;
        if (auto* lastStyleChangeEventStyle = this->lastStyleChangeEventStyle())
            targetStyle = lastStyleChangeEventStyle;

        targetStyle->conservativelyCollectChangedAnimatableProperties(newStyle, transitionProperties);

        // When we have keyframeEffectStack, it can affect on properties. So we just add them.
        if (keyframeEffectStack()) {
            for (const auto& effect : keyframeEffectStack()->sortedEffects()) {
                for (const auto& property : effect->animatedProperties())
                    addProperty(property);
                if (auto* transition = dynamicDowncast<CSSTransition>(effect->animation()))
                    addProperty(transition->property());
            }
        }

        if (auto* properties = element.runningTransitionsByProperty(pseudoElementIdentifier)) {
            for (const auto& [property, transition] : *properties)
                addProperty(property);
        }
        if (auto* properties = element.completedTransitionsByProperty(pseudoElementIdentifier)) {
            for (const auto& [property, transition] : *properties)
                addProperty(property);
        }

        transitionCustomProperties.clear();
        auto gatherAnimatableCustomProperties = [&](const StyleCustomPropertyData& customPropertyData) {
            if (!customPropertyData.mayHaveAnimatableProperties())
                return;

            customPropertyData.forEach([&](auto& customPropertyAndValuePair) {
                auto [customProperty, customPropertyValue] = customPropertyAndValuePair;
                if (customPropertyValue->isAnimatable())
                    transitionCustomProperties.add(customProperty);
                return IterationStatus::Continue;
            });
        };
        gatherAnimatableCustomProperties(currentStyle.inheritedCustomProperties());
        gatherAnimatableCustomProperties(currentStyle.nonInheritedCustomProperties());
        gatherAnimatableCustomProperties(newStyle.inheritedCustomProperties());
        gatherAnimatableCustomProperties(newStyle.nonInheritedCustomProperties());
    }

    transitionProperties.m_properties.forEachSetBit([&](unsigned index) {
        CSSPropertyID propertyId = static_cast<CSSPropertyID>(index);
        if (isShorthand(propertyId))
            return;
        updateCSSTransitionsForStyleableAndProperty(*this, propertyId, currentStyle, newStyle, generationTime, newStyleOriginatedAnimations);
    });
    for (auto& customProperty : transitionCustomProperties)
        updateCSSTransitionsForStyleableAndProperty(*this, customProperty, currentStyle, newStyle, generationTime, newStyleOriginatedAnimations);
}

void Styleable::queryContainerDidChange() const
{
    auto* animations = this->animations();
    if (!animations)
        return;
    for (auto animation : *animations) {
        auto* cssAnimation = dynamicDowncast<CSSAnimation>(animation.get());
        if (!cssAnimation)
            continue;
        auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(cssAnimation->effect());
        if (keyframeEffect && keyframeEffect->blendingKeyframes().usesContainerUnits())
            cssAnimation->keyframesRuleDidChange();
    }
}

} // namespace WebCore
