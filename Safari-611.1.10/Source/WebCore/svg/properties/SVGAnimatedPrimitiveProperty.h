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
#include "SVGSharedPrimitiveProperty.h"

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
        m_baseVal->setValue(baseVal);
        commitPropertyChange(nullptr);
        return { };
    }

    // Used by SVGElement::parseAttribute().
    void setBaseValInternal(const PropertyType& baseVal) { m_baseVal->setValue(baseVal); }
    const PropertyType& baseVal() const { return m_baseVal->value(); }

    // Used by SVGAttributeAnimator::progress.
    void setAnimVal(const PropertyType& animVal)
    {
        ASSERT(isAnimating() && m_animVal);
        m_animVal->setValue(animVal);
    }

    const PropertyType& animVal() const
    {
        ASSERT_IMPLIES(isAnimating(), m_animVal);
        return isAnimating() ? m_animVal->value() : m_baseVal->value();
    }

    PropertyType& animVal()
    {
        ASSERT_IMPLIES(isAnimating(), m_animVal);
        return isAnimating() ? m_animVal->value() : m_baseVal->value();
    }

    // Used when committing a change from the SVGAnimatedProperty to the attribute.
    String baseValAsString() const override { return m_baseVal->valueAsString(); }

    // Used to apply the SVGAttributeAnimator change to the target element.
    String animValAsString() const override
    {
        ASSERT(isAnimating() && m_animVal);
        return m_animVal->valueAsString();
    }

    // Managing the relationship with the owner.
    void setDirty() override { m_baseVal->setDirty(); }
    bool isDirty() const override { return m_baseVal->isDirty(); }
    Optional<String> synchronize() override { return m_baseVal->synchronize(); }

    // Used by RenderSVGElements and DumpRenderTree.
    const PropertyType& currentValue() const
    {
        ASSERT_IMPLIES(isAnimating(), m_animVal);
        return isAnimating() ? m_animVal->value() : m_baseVal->value();
    }

    // Controlling the animation.
    void startAnimation(SVGAttributeAnimator& animator) override
    {
        if (m_animVal)
            m_animVal->setValue(m_baseVal->value());
        else
            ensureAnimVal();
        SVGAnimatedProperty::startAnimation(animator);
    }

    void stopAnimation(SVGAttributeAnimator& animator) override
    {
        SVGAnimatedProperty::stopAnimation(animator);
        if (!isAnimating())
            m_animVal = nullptr;
        else if (m_animVal)
            m_animVal->setValue(m_baseVal->value());
    }

    // Controlling the instance animation.
    void instanceStartAnimation(SVGAttributeAnimator& animator, SVGAnimatedProperty& animated) override
    {
        if (isAnimating())
            return;
        m_animVal = static_cast<SVGAnimatedPrimitiveProperty&>(animated).m_animVal;
        SVGAnimatedProperty::instanceStartAnimation(animator, animated);
    }

    void instanceStopAnimation(SVGAttributeAnimator& animator) override
    {
        if (!isAnimating())
            return;
        m_animVal = nullptr;
        SVGAnimatedProperty::instanceStopAnimation(animator);
    }

protected:
    SVGAnimatedPrimitiveProperty(SVGElement* contextElement)
        : SVGAnimatedProperty(contextElement)
        , m_baseVal(SVGSharedPrimitiveProperty<PropertyType>::create())
    {
    }

    SVGAnimatedPrimitiveProperty(SVGElement* contextElement, const PropertyType& value)
        : SVGAnimatedProperty(contextElement)
        , m_baseVal(SVGSharedPrimitiveProperty<PropertyType>::create(value))
    {
    }

    RefPtr<SVGSharedPrimitiveProperty<PropertyType>>& ensureAnimVal()
    {
        if (!m_animVal)
            m_animVal = SVGSharedPrimitiveProperty<PropertyType>::create(m_baseVal->value());
        return m_animVal;
    }

    Ref<SVGSharedPrimitiveProperty<PropertyType>> m_baseVal;
    mutable RefPtr<SVGSharedPrimitiveProperty<PropertyType>> m_animVal;
};

}
