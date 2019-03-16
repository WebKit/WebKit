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

#include "SVGAnimatedProperty.h"
#include "SVGProperty.h"

namespace WebCore {

template<typename PropertyType>
class SVGAnimatedPrimitiveProperty : public SVGAnimatedProperty {
public:
    using ValueType = PropertyType;

    static Ref<SVGAnimatedPrimitiveProperty> create(SVGElement* contextElement)
    {
        return adoptRef(*new SVGAnimatedPrimitiveProperty(contextElement));
    }

    static Ref<SVGAnimatedPrimitiveProperty> create(SVGElement* contextElement, const PropertyType& value)
    {
        return adoptRef(*new SVGAnimatedPrimitiveProperty(contextElement, value));
    }

    // Used by the DOM.
    ExceptionOr<void> setBaseVal(const PropertyType& baseVal)
    {
        m_baseVal = baseVal;
        commitPropertyChange();
        return { };
    }

    // Used by SVGElement::parseAttribute().
    void setBaseValInternal(const PropertyType& baseVal) { m_baseVal = baseVal; }
    const PropertyType& baseVal() const { return m_baseVal; }

    // Used by SVGAttributeAnimator::progress.
    void setAnimVal(const PropertyType& animVal)
    {
        ASSERT(isAnimating());
        m_animVal = animVal;
    }

    const PropertyType& animVal() const
    {
        ASSERT_IMPLIES(isAnimating(), m_animVal);
        return isAnimating() ? *m_animVal : m_baseVal;
    }

    PropertyType& animVal()
    {
        ASSERT_IMPLIES(isAnimating(), m_animVal);
        return isAnimating() ? *m_animVal : m_baseVal;
    }

    // Used when committing a change from the SVGAnimatedProperty to the attribute.
    String baseValAsString() const override { return SVGPropertyTraits<PropertyType>::toString(m_baseVal); }

    // Used to apply the SVGAttributeAnimator change to the target element.
    String animValAsString() const override
    {
        ASSERT(isAnimating() && !!m_animVal);
        return SVGPropertyTraits<PropertyType>::toString(*m_animVal);
    }

    // Managing the relationship with the owner.
    void setDirty() override { m_state = SVGPropertyState::Dirty; }
    bool isDirty() const override { return m_state == SVGPropertyState::Dirty; }
    Optional<String> synchronize() override
    {
        if (m_state == SVGPropertyState::Clean)
            return WTF::nullopt;
        m_state = SVGPropertyState::Clean;
        return baseValAsString();
    }

    // Used by RenderSVGElements and DumpRenderTree.
    const PropertyType& currentValue() const
    {
        ASSERT_IMPLIES(isAnimating(), m_animVal);
        return isAnimating() ? *m_animVal : m_baseVal;
    }

    // Controlling the animation.
    void startAnimation() override
    {
        if (isAnimating())
            return;
        m_animVal = m_baseVal;
        SVGAnimatedProperty::startAnimation();
    }

    void stopAnimation() override
    {
        if (!isAnimating())
            return;
        m_animVal = WTF::nullopt;
        SVGAnimatedProperty::stopAnimation();
    }

protected:
    SVGAnimatedPrimitiveProperty(SVGElement* contextElement)
        : SVGAnimatedProperty(contextElement)
        , m_baseVal(SVGPropertyTraits<PropertyType>::initialValue())
    {
    }

    SVGAnimatedPrimitiveProperty(SVGElement* contextElement, const PropertyType& value)
        : SVGAnimatedProperty(contextElement)
        , m_baseVal(value)
    {
    }

    PropertyType m_baseVal;
    mutable Optional<PropertyType> m_animVal;
    SVGPropertyState m_state { SVGPropertyState::Clean };
};

}
