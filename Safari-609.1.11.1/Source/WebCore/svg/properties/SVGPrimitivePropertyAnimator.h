/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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

#include "SVGPropertyAnimator.h"
#include "SVGPropertyTraits.h"
#include "SVGValueProperty.h"

namespace WebCore {

template<typename PropertyType, typename AnimationFunction>
class SVGPrimitivePropertyAnimator : public SVGPropertyAnimator<AnimationFunction> {
    using Base = SVGPropertyAnimator<AnimationFunction>;
    using ValuePropertyType = SVGValueProperty<PropertyType>;
    using Base::Base;
    using Base::applyAnimatedStylePropertyChange;
    using Base::computeCSSPropertyValue;
    using Base::m_attributeName;
    using Base::m_function;
    
public:
    static auto create(const QualifiedName& attributeName, Ref<SVGProperty>&& property, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGPrimitivePropertyAnimator(attributeName, WTFMove(property), animationMode, calcMode, isAccumulated, isAdditive));
    }
    
    template<typename... Arguments>
    SVGPrimitivePropertyAnimator(const QualifiedName& attributeName, Ref<SVGProperty>&& property, Arguments&&... arguments)
        : Base(attributeName, std::forward<Arguments>(arguments)...)
        , m_property(static_reference_cast<ValuePropertyType>(WTFMove(property)))
    {
    }

    void start(SVGElement* targetElement) override
    {
        String baseValue = computeCSSPropertyValue(targetElement, cssPropertyID(m_attributeName.localName()));
        m_property->setValue(SVGPropertyTraits<PropertyType>::fromString(baseValue));
    }

    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) override
    {
        PropertyType& animated = m_property->value();
        m_function.animate(targetElement, progress, repeatCount, animated);
    }

    void apply(SVGElement* targetElement) override
    {
        applyAnimatedStylePropertyChange(targetElement, SVGPropertyTraits<PropertyType>::toString(m_property->value()));
    }

protected:
    Ref<ValuePropertyType> m_property;
};
    
}
