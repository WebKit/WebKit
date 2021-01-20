/*
 * Copyright (C) 2018-2019 Apple Inc.  All rights reserved.
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

#include "SVGAttributeAnimator.h"

namespace WebCore {

class SVGElement;

template<typename AnimatedPropertyAnimator1, typename AnimatedPropertyAnimator2>
class SVGAnimatedPropertyPairAnimator : public SVGAttributeAnimator {
public:
    using AnimatedProperty1 = typename AnimatedPropertyAnimator1::AnimatorAnimatedProperty;
    using AnimatedProperty2 = typename AnimatedPropertyAnimator2::AnimatorAnimatedProperty;

    SVGAnimatedPropertyPairAnimator(const QualifiedName& attributeName, Ref<AnimatedProperty1>& animated1, Ref<AnimatedProperty2>& animated2, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
        : SVGAttributeAnimator(attributeName)
        , m_animatedPropertyAnimator1(AnimatedPropertyAnimator1::create(attributeName, animated1, animationMode, calcMode, isAccumulated, isAdditive))
        , m_animatedPropertyAnimator2(AnimatedPropertyAnimator2::create(attributeName, animated2, animationMode, calcMode, isAccumulated, isAdditive))
    {
    }

    void appendAnimatedInstance(Ref<AnimatedProperty1>& animated1, Ref<AnimatedProperty2>& animated2)
    {
        m_animatedPropertyAnimator1->appendAnimatedInstance(animated1);
        m_animatedPropertyAnimator2->appendAnimatedInstance(animated2);
    }

protected:
    void start(SVGElement* targetElement) override
    {
        m_animatedPropertyAnimator1->start(targetElement);
        m_animatedPropertyAnimator2->start(targetElement);
    }

    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) override
    {
        m_animatedPropertyAnimator1->animate(targetElement, progress, repeatCount);
        m_animatedPropertyAnimator2->animate(targetElement, progress, repeatCount);
    }

    void apply(SVGElement* targetElement) override
    {
        applyAnimatedPropertyChange(targetElement);
    }

    void stop(SVGElement* targetElement) override
    {
        m_animatedPropertyAnimator1->stop(targetElement);
        m_animatedPropertyAnimator2->stop(targetElement);
    }

    Ref<AnimatedPropertyAnimator1> m_animatedPropertyAnimator1;
    Ref<AnimatedPropertyAnimator2> m_animatedPropertyAnimator2;
};

}
