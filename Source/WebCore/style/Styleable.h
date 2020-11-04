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

#pragma once

#include "Element.h"
#include "PseudoElement.h"
#include "RenderElement.h"
#include "RenderStyleConstants.h"
#include "WebAnimationTypes.h"

namespace WebCore {

class KeyframeEffectStack;
class RenderStyle;

struct Styleable {
    Element& element;
    PseudoId pseudoId;

    Styleable(Element& element, PseudoId pseudoId)
        : element(element)
        , pseudoId(pseudoId)
    {
        ASSERT(!is<PseudoElement>(element));
    }

    static const Styleable fromElement(Element& element)
    {
        if (is<PseudoElement>(element))
            return Styleable(*downcast<PseudoElement>(element).hostElement(), element.pseudoId());
        return Styleable(element, element.pseudoId());
    }

    static const Optional<const Styleable> fromRenderer(const RenderElement& renderer)
    {
        if (auto* element = renderer.element())
            return fromElement(*element);
        return WTF::nullopt;
    }

    bool operator==(const Styleable& other) const
    {
        return (&element == &other.element && pseudoId == other.pseudoId);
    }

    bool operator!=(const Styleable& other) const
    {
        return !(*this == other);
    }

    KeyframeEffectStack* keyframeEffectStack() const
    {
        return element.keyframeEffectStack(pseudoId);
    }

    KeyframeEffectStack& ensureKeyframeEffectStack() const
    {
        return element.ensureKeyframeEffectStack(pseudoId);
    }

    bool hasKeyframeEffects() const
    {
        return element.hasKeyframeEffects(pseudoId);
    }

    OptionSet<AnimationImpact> applyKeyframeEffects(RenderStyle& style) const
    {
        return element.applyKeyframeEffects(pseudoId, style);
    }

    const AnimationCollection* animations() const
    {
        return element.animations(pseudoId);
    }

    bool hasCompletedTransitionsForProperty(CSSPropertyID property) const
    {
        return element.hasCompletedTransitionsForProperty(pseudoId, property);
    }

    bool hasRunningTransitionsForProperty(CSSPropertyID property) const
    {
        return element.hasRunningTransitionsForProperty(pseudoId, property);
    }

    bool hasRunningTransitions() const
    {
        return element.hasRunningTransitions(pseudoId);
    }

    AnimationCollection& ensureAnimations() const
    {
        return element.ensureAnimations(pseudoId);
    }

    PropertyToTransitionMap& ensureCompletedTransitionsByProperty() const
    {
        return element.ensureCompletedTransitionsByProperty(pseudoId);
    }

    PropertyToTransitionMap& ensureRunningTransitionsByProperty() const
    {
        return element.ensureRunningTransitionsByProperty(pseudoId);
    }

    CSSAnimationCollection& animationsCreatedByMarkup() const
    {
        return element.animationsCreatedByMarkup(pseudoId);
    }

    void setAnimationsCreatedByMarkup(CSSAnimationCollection&& collection) const
    {
        element.setAnimationsCreatedByMarkup(pseudoId, WTFMove(collection));
    }

    const RenderStyle* lastStyleChangeEventStyle() const
    {
        return element.lastStyleChangeEventStyle(pseudoId);
    }

    void setLastStyleChangeEventStyle(std::unique_ptr<const RenderStyle>&& style) const
    {
        element.setLastStyleChangeEventStyle(pseudoId, WTFMove(style));
    }

};

} // namespace WebCore
