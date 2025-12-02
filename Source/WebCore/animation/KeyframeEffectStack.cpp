/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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
#include "KeyframeEffectStack.h"

#include "AnimationTimeline.h"
#include "CSSAnimation.h"
#include "CSSTransition.h"
#include "Document.h"
#include "KeyframeEffect.h"
#include "RenderStyleInlines.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "Settings.h"
#include "StyleInterpolation.h"
#include "StyleRotate.h"
#include "StyleScale.h"
#include "StyleTransform.h"
#include "StyleTranslate.h"
#include "TransformOperations.h"
#include "TranslateTransformOperation.h"
#include "WebAnimation.h"
#include <WebCore/WebAnimationTypes.h>
#include "WebAnimationUtilities.h"
#include <ranges>
#include <wtf/OptionSet.h>
#include <wtf/PointerComparison.h>

namespace WebCore {

KeyframeEffectStack::KeyframeEffectStack() = default;

KeyframeEffectStack::~KeyframeEffectStack() = default;

bool KeyframeEffectStack::addEffect(KeyframeEffect& effect)
{
    // To qualify for membership in an effect stack, an effect must have a target, an animation, a timeline and be relevant.
    // This method will be called in WebAnimation and KeyframeEffect as those properties change.
    if (!effect.targetStyleable() || !effect.animation() || !effect.animation()->isRelevant())
        return false;

    ASSERT(!m_effects.contains(&effect));

    m_effects.append(effect);
    m_isSorted = false;

    if (m_effects.size() > 1 && effect.preventsAcceleration())
        stopAcceleratedAnimations();

    effect.wasAddedToEffectStack();

    return true;
}

void KeyframeEffectStack::removeEffect(KeyframeEffect& effect)
{
    auto removedEffect = m_effects.removeFirst(&effect);

    if (removedEffect)
        effect.wasRemovedFromEffectStack();

    if (!removedEffect || m_effects.isEmpty())
        return;

    if (!effect.canBeAccelerated())
        startAcceleratedAnimationsIfPossible();
}

bool KeyframeEffectStack::hasMatchingEffect(NOESCAPE const Function<bool(const KeyframeEffect&)>& function) const
{
    for (auto& effect : m_effects) {
        if (function(*effect))
            return true;
    }
    return false;
}

bool KeyframeEffectStack::containsProperty(CSSPropertyID property) const
{
    return hasMatchingEffect([property] (const KeyframeEffect& effect) {
        return effect.animatesProperty(property);
    });
}

bool KeyframeEffectStack::requiresPseudoElement() const
{
    return hasMatchingEffect([] (const KeyframeEffect& effect) {
        return effect.requiresPseudoElement();
    });
}

bool KeyframeEffectStack::isCurrentlyAffectingProperty(CSSPropertyID property) const
{
    return hasMatchingEffect([property] (const KeyframeEffect& effect) {
        return effect.isCurrentlyAffectingProperty(property) || effect.isRunningAcceleratedAnimationForProperty(property);
    });
}

const Vector<WeakPtr<KeyframeEffect>>& KeyframeEffectStack::sortedEffects()
{
    if (!m_isSorted && m_effects.size() > 1) {
        std::ranges::stable_sort(m_effects, compareAnimationsByCompositeOrder, [](auto& weakEffect) -> WebAnimation& {
            RELEASE_ASSERT(weakEffect->animation());
            return *weakEffect->animation();
        });
        m_isSorted = true;
    }

    return m_effects;
}

void KeyframeEffectStack::setCSSAnimationList(std::optional<Style::Animations>&& cssAnimationList)
{
    m_cssAnimationList = WTFMove(cssAnimationList);
    // Since the list of animation names has changed, the sorting order of the animation effects may have changed as well.
    m_isSorted = false;
}

OptionSet<AnimationImpact> KeyframeEffectStack::applyKeyframeEffects(RenderStyle& targetStyle, HashSet<AnimatableCSSProperty>& affectedProperties, const RenderStyle* previousLastStyleChangeEventStyle, const Style::ResolutionContext& resolutionContext)
{
    OptionSet<AnimationImpact> impact;

    auto& previousStyle = previousLastStyleChangeEventStyle ? *previousLastStyleChangeEventStyle : RenderStyle::defaultStyleSingleton();

    auto transformRelatedPropertyChanged = [&]() -> bool {
        return targetStyle.translate() != previousStyle.translate()
            || targetStyle.scale() != previousStyle.scale()
            || targetStyle.rotate() != previousStyle.rotate()
            || targetStyle.transform() != previousStyle.transform();
    }();

    auto unanimatedStyle = RenderStyle::clone(targetStyle);

    // We iterate over a snapshot of the effect list as it may mutate during application.
    for (const auto& effect : copyToVector(sortedEffects())) {
        auto keyframeRecomputationReason = effect->recomputeKeyframesIfNecessary(previousLastStyleChangeEventStyle, unanimatedStyle, resolutionContext);

        Ref animation = *effect->animation();
        impact.add(animation->resolve(targetStyle, resolutionContext));

        if (effect->isRunningAccelerated() || effect->isAboutToRunAccelerated())
            impact.add(AnimationImpact::RequiresRecomposite);

        if (effect->triggersStackingContext())
            impact.add(AnimationImpact::ForcesStackingContext);

        if (transformRelatedPropertyChanged && effect->isRunningAcceleratedTransformRelatedAnimation())
            effect->transformRelatedPropertyDidChange();

        // If one of the effect's resolved property changed it could affect whether that effect's animation is removed.
        if (keyframeRecomputationReason && *keyframeRecomputationReason == KeyframeEffect::RecomputationReason::LogicalPropertyChange) {
            if (RefPtr timeline = animation->timeline())
                timeline->animationTimingDidChange(animation.get());
        }

        affectedProperties.addAll(effect->animatedProperties());
    }

    return impact;
}

void KeyframeEffectStack::clearInvalidCSSAnimationNames()
{
    m_invalidCSSAnimationNames.clear();
}

bool KeyframeEffectStack::hasInvalidCSSAnimationNames() const
{
    return !m_invalidCSSAnimationNames.isEmpty();
}

bool KeyframeEffectStack::containsInvalidCSSAnimationName(const String& name) const
{
    return m_invalidCSSAnimationNames.contains(name);
}

void KeyframeEffectStack::addInvalidCSSAnimationName(const String& name)
{
    m_invalidCSSAnimationNames.add(name);
}

void KeyframeEffectStack::effectAbilityToBeAcceleratedDidChange(const KeyframeEffect& effect)
{
    ASSERT(m_effects.contains(&effect));
    if (effect.preventsAcceleration())
        stopAcceleratedAnimations();
    else
        startAcceleratedAnimationsIfPossible();
}

bool KeyframeEffectStack::allowsAcceleration() const
{
    // We could try and be a lot smarter here and do this on a per-property basis and
    // account for fully replacing effects which could co-exist with effects that
    // don't support acceleration lower in the stack, etc. But, if we are not able to run
    // all effects that could support acceleration using acceleration, then we might
    // as well not run any at all since we'll be updating effects for this stack
    // for each animation frame. So for now, we simply return false if any effect in the
    // stack is unable to be accelerated, or if we have more than one effect animating
    // an accelerated property with an implicit keyframe.

    HashSet<AnimatableCSSProperty> allAcceleratedProperties;

#if ENABLE(THREADED_ANIMATIONS)
    OptionSet<AcceleratedEffectProperty> threadedAcceleratedProperties;
    OptionSet<AcceleratedEffectProperty> nonThreadedAcceleratedProperties;

    auto toAcceleratedProperties = [](const HashSet<AnimatableCSSProperty>& properties) {
        OptionSet<AcceleratedEffectProperty> acceleratedProperties;
        if (properties.contains(CSSPropertyFilter) || properties.contains(CSSPropertyBackdropFilter))
            acceleratedProperties.add(AcceleratedEffectProperty::Filter);
        if (properties.contains(CSSPropertyOpacity))
            acceleratedProperties.add(AcceleratedEffectProperty::Opacity);
        if (properties.contains(CSSPropertyRotate)
            || properties.contains(CSSPropertyScale)
            || properties.contains(CSSPropertyTransform)
            || properties.contains(CSSPropertyTranslate)
            || properties.contains(CSSPropertyOffsetAnchor)
            || properties.contains(CSSPropertyOffsetDistance)
            || properties.contains(CSSPropertyOffsetPath)
            || properties.contains(CSSPropertyOffsetPosition)
            || properties.contains(CSSPropertyOffsetRotate))
            acceleratedProperties.add(AcceleratedEffectProperty::Transform);
        return acceleratedProperties;
    };
#endif

    for (auto& effect : m_effects) {
        if (effect->preventsAcceleration())
            return false;
        auto& acceleratedProperties = effect->acceleratedProperties();
        if (!allAcceleratedProperties.isEmpty()) {
            auto previouslySeenAcceleratedPropertiesAffectingCurrentEffect = allAcceleratedProperties.intersectionWith(acceleratedProperties);
            if (!previouslySeenAcceleratedPropertiesAffectingCurrentEffect.isEmpty()
                && !effect->acceleratedPropertiesWithImplicitKeyframe().intersectionWith(previouslySeenAcceleratedPropertiesAffectingCurrentEffect).isEmpty()) {
                return false;
            }
        }

#if ENABLE(THREADED_ANIMATIONS)
        (effect->canHaveAcceleratedRepresentation() ? threadedAcceleratedProperties : nonThreadedAcceleratedProperties).add(toAcceleratedProperties(acceleratedProperties));
        if (threadedAcceleratedProperties.containsAny(nonThreadedAcceleratedProperties))
            return false;
#endif

        allAcceleratedProperties.addAll(acceleratedProperties);
    }

    return true;
}

void KeyframeEffectStack::startAcceleratedAnimationsIfPossible()
{
    if (!allowsAcceleration())
        return;

    for (auto& effect : m_effects)
        effect->effectStackNoLongerPreventsAcceleration();
}

void KeyframeEffectStack::stopAcceleratedAnimations()
{
    for (auto& effect : m_effects)
        effect->effectStackNoLongerAllowsAcceleration();
}

void KeyframeEffectStack::lastStyleChangeEventStyleDidChange(const RenderStyle* previousStyle, const RenderStyle* currentStyle)
{
    for (auto& effect : m_effects)
        effect->lastStyleChangeEventStyleDidChange(previousStyle, currentStyle);
}

void KeyframeEffectStack::cascadeDidOverrideProperties(const HashSet<AnimatableCSSProperty>& overriddenProperties, const Document& document)
{
    HashSet<AnimatableCSSProperty> acceleratedPropertiesOverriddenByCascade;
    for (auto animatedProperty : overriddenProperties) {
        if (Style::Interpolation::isAccelerated(animatedProperty, document.settings()))
            acceleratedPropertiesOverriddenByCascade.add(animatedProperty);
    }

    if (acceleratedPropertiesOverriddenByCascade == m_acceleratedPropertiesOverriddenByCascade)
        return;

    m_acceleratedPropertiesOverriddenByCascade = WTFMove(acceleratedPropertiesOverriddenByCascade);

    for (auto& effect : m_effects)
        effect->acceleratedPropertiesOverriddenByCascadeDidChange();
}

void KeyframeEffectStack::applyPendingAcceleratedActions() const
{
    bool hasActiveAcceleratedEffect = m_effects.containsIf([](const auto& effect) {
        return effect->canBeAccelerated() && effect->animation()->playState() == WebAnimation::PlayState::Running;
    });

    auto accelerationWasPrevented = false;

    for (auto& effect : m_effects) {
        if (hasActiveAcceleratedEffect)
            effect->applyPendingAcceleratedActionsOrUpdateTimingProperties();
        else
            effect->applyPendingAcceleratedActions();
        accelerationWasPrevented = accelerationWasPrevented || effect->accelerationWasPrevented() || effect->preventsAcceleration();
    }

    if (accelerationWasPrevented) {
        for (auto& effect : m_effects)
            effect->effectStackNoLongerAllowsAccelerationDuringAcceleratedActionApplication();
    }
}

bool KeyframeEffectStack::hasAcceleratedEffects(const Settings& settings) const
{
#if ENABLE(THREADED_ANIMATIONS)
    if (settings.threadedScrollDrivenAnimationsEnabled() || settings.threadedTimeBasedAnimationsEnabled())
        return !m_acceleratedEffects.isEmptyIgnoringNullReferences();
#else
    UNUSED_PARAM(settings);
#endif
    return hasMatchingEffect([](const auto& effect) {
        return effect.isRunningAccelerated();
    });
}

} // namespace WebCore
