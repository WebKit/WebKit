/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "CSSPropertyAnimation.h"
#include "CSSTransition.h"
#include "Document.h"
#include "KeyframeEffect.h"
#include "RenderStyleInlines.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "TransformOperations.h"
#include "TranslateTransformOperation.h"
#include "WebAnimation.h"
#include "WebAnimationUtilities.h"
#include <wtf/PointerComparison.h>

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(KeyframeEffectStack);

KeyframeEffectStack::KeyframeEffectStack()
{
}

KeyframeEffectStack::~KeyframeEffectStack()
{
}

bool KeyframeEffectStack::addEffect(KeyframeEffect& effect)
{
    // To qualify for membership in an effect stack, an effect must have a target, an animation, a timeline and be relevant.
    // This method will be called in WebAnimation and KeyframeEffect as those properties change.
    if (!effect.targetStyleable() || !effect.animation() || !effect.animation()->timeline() || !effect.animation()->isRelevant())
        return false;

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

bool KeyframeEffectStack::hasMatchingEffect(const Function<bool(const KeyframeEffect&)>& function) const
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

bool KeyframeEffectStack::hasEffectWithImplicitKeyframes() const
{
    return hasMatchingEffect([] (const KeyframeEffect& effect) {
        return effect.hasImplicitKeyframes();
    });
}

bool KeyframeEffectStack::isCurrentlyAffectingProperty(CSSPropertyID property) const
{
    return hasMatchingEffect([property] (const KeyframeEffect& effect) {
        return effect.isCurrentlyAffectingProperty(property) || effect.isRunningAcceleratedAnimationForProperty(property);
    });
}

Vector<WeakPtr<KeyframeEffect>> KeyframeEffectStack::sortedEffects()
{
    ensureEffectsAreSorted();
    return m_effects;
}

void KeyframeEffectStack::ensureEffectsAreSorted()
{
    if (m_isSorted || m_effects.size() < 2)
        return;

    std::stable_sort(m_effects.begin(), m_effects.end(), [](auto& lhs, auto& rhs) {
        RELEASE_ASSERT(lhs.get());
        RELEASE_ASSERT(rhs.get());
        
        auto* lhsAnimation = lhs->animation();
        auto* rhsAnimation = rhs->animation();

        RELEASE_ASSERT(lhsAnimation);
        RELEASE_ASSERT(rhsAnimation);

        return compareAnimationsByCompositeOrder(*lhsAnimation, *rhsAnimation);
    });

    m_isSorted = true;
}

void KeyframeEffectStack::setCSSAnimationList(RefPtr<const AnimationList>&& cssAnimationList)
{
    m_cssAnimationList = WTFMove(cssAnimationList);
    // Since the list of animation names has changed, the sorting order of the animation effects may have changed as well.
    m_isSorted = false;
}

OptionSet<AnimationImpact> KeyframeEffectStack::applyKeyframeEffects(RenderStyle& targetStyle, HashSet<AnimatableCSSProperty>& affectedProperties, const RenderStyle* previousLastStyleChangeEventStyle, const Style::ResolutionContext& resolutionContext)
{
    OptionSet<AnimationImpact> impact;

    auto& previousStyle = previousLastStyleChangeEventStyle ? *previousLastStyleChangeEventStyle : RenderStyle::defaultStyle();

    auto transformRelatedPropertyChanged = [&]() -> bool {
        return !arePointingToEqualData(targetStyle.translate(), previousStyle.translate())
            || !arePointingToEqualData(targetStyle.scale(), previousStyle.scale())
            || !arePointingToEqualData(targetStyle.rotate(), previousStyle.rotate())
            || targetStyle.transform() != previousStyle.transform();
    }();

    auto unanimatedStyle = RenderStyle::clone(targetStyle);

    for (const auto& effect : sortedEffects()) {
        auto keyframeRecomputationReason = effect->recomputeKeyframesIfNecessary(previousLastStyleChangeEventStyle, unanimatedStyle, resolutionContext);

        ASSERT(effect->animation());
        auto* animation = effect->animation();
        animation->resolve(targetStyle, resolutionContext);

        if (effect->isRunningAccelerated() || effect->isAboutToRunAccelerated())
            impact.add(AnimationImpact::RequiresRecomposite);

        if (effect->triggersStackingContext())
            impact.add(AnimationImpact::ForcesStackingContext);

        if (transformRelatedPropertyChanged && effect->isRunningAcceleratedTransformRelatedAnimation())
            effect->transformRelatedPropertyDidChange();

        // If one of the effect's resolved property changed it could affect whether that effect's animation is removed.
        if (keyframeRecomputationReason && *keyframeRecomputationReason == KeyframeEffect::RecomputationReason::LogicalPropertyChange) {
            ASSERT(animation->timeline());
            animation->timeline()->animationTimingDidChange(*animation);
        }

        affectedProperties.formUnion(effect->animatedProperties());
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
    // stack is unable to be accelerated.
    return !hasMatchingEffect([] (const KeyframeEffect& effect) {
        return effect.preventsAcceleration();
    });
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
        if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(animatedProperty, document.settings()))
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

    for (auto& effect : m_effects) {
        if (hasActiveAcceleratedEffect)
            effect->applyPendingAcceleratedActionsOrUpdateTimingProperties();
        else
            effect->applyPendingAcceleratedActions();
    }
}

} // namespace WebCore
