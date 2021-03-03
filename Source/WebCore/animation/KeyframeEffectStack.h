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

#pragma once

#include "AnimationList.h"
#include "CSSPropertyNames.h"
#include "WebAnimationTypes.h"
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class KeyframeEffect;
class RenderStyle;

class KeyframeEffectStack {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit KeyframeEffectStack();
    ~KeyframeEffectStack();

    bool addEffect(KeyframeEffect&);
    void removeEffect(KeyframeEffect&);
    bool hasEffects() const { return !m_effects.isEmpty(); }
    Vector<WeakPtr<KeyframeEffect>> sortedEffects();
    const AnimationList* cssAnimationList() const { return m_cssAnimationList.get(); }
    void setCSSAnimationList(RefPtr<const AnimationList>&&);
    bool isCurrentlyAffectingProperty(CSSPropertyID) const;
    bool requiresPseudoElement() const;
    OptionSet<AnimationImpact> applyKeyframeEffects(RenderStyle& targetStyle, const RenderStyle& previousLastStyleChangeEventStyle, const RenderStyle* parentElementStyle);
    bool hasEffectWithImplicitKeyframes() const;

    void stopAcceleratingTransformRelatedProperties(UseAcceleratedAction);

private:
    void ensureEffectsAreSorted();

    Vector<WeakPtr<KeyframeEffect>> m_effects;
    RefPtr<const AnimationList> m_cssAnimationList;
    bool m_isSorted { true };

};

} // namespace WebCore
