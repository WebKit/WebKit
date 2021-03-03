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

#include "CSSAnimation.h"
#include "CSSTransition.h"
#include "KeyframeEffect.h"
#include "WebAnimation.h"
#include "WebAnimationUtilities.h"

namespace WebCore {

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

    m_effects.append(makeWeakPtr(&effect));
    m_isSorted = false;
    return true;
}

void KeyframeEffectStack::removeEffect(KeyframeEffect& effect)
{
    m_effects.removeFirst(&effect);
}

bool KeyframeEffectStack::requiresPseudoElement() const
{
    for (auto& effect : m_effects) {
        if (effect->requiresPseudoElement())
            return true;
    }
    return false;
}

bool KeyframeEffectStack::hasEffectWithImplicitKeyframes() const
{
    for (auto& effect : m_effects) {
        if (effect->hasImplicitKeyframes())
            return true;
    }
    return false;
}

bool KeyframeEffectStack::isCurrentlyAffectingProperty(CSSPropertyID property) const
{
    for (auto& effect : m_effects) {
        if (effect->isCurrentlyAffectingProperty(property) || effect->isRunningAcceleratedAnimationForProperty(property))
            return true;
    }
    return false;
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

    std::stable_sort(m_effects.begin(), m_effects.end(), [&](auto& lhs, auto& rhs) {
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

OptionSet<AnimationImpact> KeyframeEffectStack::applyKeyframeEffects(RenderStyle& targetStyle, const RenderStyle& previousLastStyleChangeEventStyle, const RenderStyle* parentElementStyle)
{
    OptionSet<AnimationImpact> impact;

    auto transformRelatedPropertyChanged = [&]() -> bool {
        return !arePointingToEqualData(targetStyle.translate(), previousLastStyleChangeEventStyle.translate())
            || !arePointingToEqualData(targetStyle.scale(), previousLastStyleChangeEventStyle.scale())
            || !arePointingToEqualData(targetStyle.rotate(), previousLastStyleChangeEventStyle.rotate())
            || targetStyle.transform() != previousLastStyleChangeEventStyle.transform();
    }();

    for (const auto& effect : sortedEffects()) {
        ASSERT(effect->animation());
        effect->animation()->resolve(targetStyle, parentElementStyle);

        if (effect->isRunningAccelerated() || effect->isAboutToRunAccelerated())
            impact.add(AnimationImpact::RequiresRecomposite);

        if (effect->triggersStackingContext())
            impact.add(AnimationImpact::ForcesStackingContext);

        if (transformRelatedPropertyChanged && effect->isRunningAcceleratedTransformRelatedAnimation())
            effect->transformRelatedPropertyDidChange();
    }

    return impact;
}

void KeyframeEffectStack::stopAcceleratingTransformRelatedProperties(UseAcceleratedAction useAcceleratedAction)
{
    for (auto& effect : m_effects)
        effect->stopAcceleratingTransformRelatedProperties(useAcceleratedAction);
}

} // namespace WebCore
