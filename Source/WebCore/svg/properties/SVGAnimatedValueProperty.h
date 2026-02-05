/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

namespace WebCore {
    
template<typename PropertyType>
class SVGAnimatedValueProperty : public SVGAnimatedProperty<SVGAnimatedValueProperty<PropertyType>> {
    using Base = SVGAnimatedProperty<SVGAnimatedValueProperty<PropertyType>>;
public:
    using ValueType = typename PropertyType::ValueType;

    template<typename... Arguments>
    static Ref<SVGAnimatedValueProperty> create(SVGElement* contextElement, Arguments&&... arguments)
    {
        return adoptRef(*new SVGAnimatedValueProperty(contextElement, std::forward<Arguments>(arguments)...));
    }

    ~SVGAnimatedValueProperty()
    {
        m_baseVal->detach();
        if (m_animVal)
            m_animVal->detach();
    }

    // Used by SVGElement::parseAttribute().
    void setBaseValInternal(const ValueType& baseVal)
    {
        m_baseVal->setValue(baseVal);
        if (m_animVal)
            m_animVal->setValue(baseVal);
    }

    // Used by the DOM.
    const Ref<PropertyType>& baseVal() const { return m_baseVal; }

    Ref<PropertyType>& baseVal() { return m_baseVal; }

    // Used by SVGAnimator::progress.
    void setAnimVal(const ValueType& animVal)
    {
        ASSERT(this->isAnimating() && m_animVal);
        m_animVal->setValue(animVal);
    }

    // Used by the DOM.
    const PropertyType& animVal() const { return const_cast<SVGAnimatedValueProperty*>(this)->ensureAnimVal(); }

    // Called by SVGAnimatedPropertyAnimator to pass the animVal to the SVGAnimationFunction::progress.
    PropertyType& animVal() { return ensureAnimVal(); }

    // Used when committing a change from the SVGAnimatedProperty to the attribute.
    String baseValAsString() const override { return m_baseVal->valueAsString(); }

    // Used to apply the SVGAnimator change to the target element.
    String animValAsString() const override
    {
        ASSERT(this->isAnimating() && m_animVal);
        return m_animVal->valueAsString();
    }

    // Managing the relationship with the owner.
    void setDirty() override { m_baseVal->setDirty(); }
    bool isDirty() const override { return m_baseVal->isDirty(); }
    std::optional<String> synchronize() override { return m_baseVal->synchronize(); }

    // Used by RenderSVGElements and DumpRenderTree.
    const ValueType& currentValue() const
    {
        ASSERT_IMPLIES(this->isAnimating(), m_animVal);
        return (this->isAnimating() ? *m_animVal : m_baseVal.get()).value();
    }

    // Controlling the animation.
    void startAnimation(SVGAttributeAnimator& animator) override
    {
        if (m_animVal)
            m_animVal->setValue(m_baseVal->value());
        else
            ensureAnimVal();
        Base::startAnimation(animator);
    }

    void stopAnimation(SVGAttributeAnimator& animator) override
    {
        Base::stopAnimation(animator);
        if (m_animVal)
            m_animVal->setValue(m_baseVal->value());
    }

    // Controlling the instance animation.
    void instanceStartAnimationImpl(SVGAttributeAnimator& animator, SVGAnimatedValueProperty& animated) override
    {
        if (!this->isAnimating())
            m_animVal = animated.animVal();
        Base::startAnimation(animator);
    }

    void instanceStopAnimationImpl(SVGAttributeAnimator& animator) override
    {
        Base::stopAnimation(animator);
        if (!this->isAnimating())
            m_animVal = nullptr;
    }

protected:
    // The packed arguments are used in PropertyType creation, for example passing
    // SVGLengthMode to SVGLength.
    template<typename... Arguments>
    SVGAnimatedValueProperty(SVGElement* contextElement, Arguments&&... arguments)
        : Base(contextElement)
        , m_baseVal(PropertyType::create(this, SVGPropertyAccess::ReadWrite, ValueType(std::forward<Arguments>(arguments)...)))
    {
    }

    template<typename... Arguments>
    SVGAnimatedValueProperty(SVGElement* contextElement, SVGPropertyAccess access, Arguments&&... arguments)
        : Base(contextElement)
        , m_baseVal(PropertyType::create(this, access, ValueType(std::forward<Arguments>(arguments)...)))
    {
    }

    PropertyType& ensureAnimVal()
    {
        if (!m_animVal)
            m_animVal = PropertyType::create(this, SVGPropertyAccess::ReadOnly, m_baseVal->value());
        return *m_animVal;
    }

    // Called when m_baseVal changes.
    void commitPropertyChange(SVGProperty* property) override
    {
        if (m_animVal)
            m_animVal->setValue(m_baseVal->value());
        Base::commitPropertyChange(property);
    }

    Ref<PropertyType> m_baseVal;
    mutable RefPtr<PropertyType> m_animVal;
};

}
