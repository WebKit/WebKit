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

template<typename AnimatedProperty, typename AnimationFunction>
class SVGAnimatedPropertyAnimator : public SVGAttributeAnimator {
public:
    using AnimatorAnimatedProperty = AnimatedProperty;

    template<typename... Arguments>
    SVGAnimatedPropertyAnimator(const QualifiedName& attributeName, Ref<AnimatedProperty>& animated, Arguments&&... arguments)
        : SVGAttributeAnimator(attributeName)
        , m_animated(animated.copyRef())
        , m_function(std::forward<Arguments>(arguments)...)
    {
    }

    void appendAnimatedInstance(Ref<AnimatedProperty>& animated)
    {
        m_animatedInstances.append(animated.copyRef());
    }

    bool isDiscrete() const override { return m_function.isDiscrete(); }

    void setFromAndToValues(SVGElement* targetElement, const String& from, const String& to) override
    {
        m_function.setFromAndToValues(targetElement, from, to);
    }

    void setFromAndByValues(SVGElement* targetElement, const String& from, const String& by) override
    {
        m_function.setFromAndByValues(targetElement, from, by);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_function.setToAtEndOfDurationValue(toAtEndOfDuration);
    }

    void start(SVGElement*) override
    {
        m_animated->startAnimation(*this);
        for (auto& instance : m_animatedInstances)
            instance->instanceStartAnimation(*this, m_animated);
    }

    void apply(SVGElement* targetElement) override
    {
        if (isAnimatedStylePropertyAniamtor(targetElement))
            applyAnimatedStylePropertyChange(targetElement, m_animated->animValAsString());
        applyAnimatedPropertyChange(targetElement);
    }

    void stop(SVGElement* targetElement) override
    {
        if (!m_animated->isAnimating())
            return;

        applyAnimatedPropertyChange(targetElement);
        if (isAnimatedStylePropertyAniamtor(targetElement))
            removeAnimatedStyleProperty(targetElement);

        m_animated->stopAnimation(*this);
        for (auto& instance : m_animatedInstances)
            instance->instanceStopAnimation(*this);
    }

    Optional<float> calculateDistance(SVGElement* targetElement, const String& from, const String& to) const override
    {
        return m_function.calculateDistance(targetElement, from, to);
    }

protected:
    Ref<AnimatedProperty> m_animated;
    Vector<Ref<AnimatedProperty>> m_animatedInstances;
    AnimationFunction m_function;
};

}
