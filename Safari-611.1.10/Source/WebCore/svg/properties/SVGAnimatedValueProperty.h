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

namespace WebCore {
    
template<typename PropertyType>
class SVGAnimatedValueProperty : public SVGAnimatedProperty {
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
        ASSERT(isAnimating() && m_animVal);
        m_animVal->setValue(animVal);
    }

    // Used by the DOM.
    const RefPtr<PropertyType>& animVal() const { return const_cast<SVGAnimatedValueProperty*>(this)->ensureAnimVal(); }

    // Called by SVGAnimatedPropertyAnimator to pass the animVal to the SVGAnimationFunction::progress.
    RefPtr<PropertyType>& animVal() { return ensureAnimVal(); }

    // Used when committing a change from the SVGAnimatedProperty to the attribute.
    String baseValAsString() const override { return m_baseVal->valueAsString(); }

    // Used to apply the SVGAnimator change to the target element.
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
    const ValueType& currentValue() const
    {
        ASSERT_IMPLIES(isAnimating(), m_animVal);
        return (isAnimating() ? *m_animVal : m_baseVal.get()).value();
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
        if (m_animVal)
            m_animVal->setValue(m_baseVal->value());
    }

    // Controlling the instance animation.
    void instanceStartAnimation(SVGAttributeAnimator& animator, SVGAnimatedProperty& animated) override
    {
        if (isAnimating())
            return;
        m_animVal = static_cast<SVGAnimatedValueProperty&>(animated).animVal();
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
    // The packed arguments are used in PropertyType creation, for example passing
    // SVGLengthMode to SVGLength.
    template<typename... Arguments>
    SVGAnimatedValueProperty(SVGElement* contextElement, Arguments&&... arguments)
        : SVGAnimatedProperty(contextElement)
        , m_baseVal(PropertyType::create(this, SVGPropertyAccess::ReadWrite, ValueType(std::forward<Arguments>(arguments)...)))
    {
    }

    template<typename... Arguments>
    SVGAnimatedValueProperty(SVGElement* contextElement, SVGPropertyAccess access, Arguments&&... arguments)
        : SVGAnimatedProperty(contextElement)
        , m_baseVal(PropertyType::create(this, access, ValueType(std::forward<Arguments>(arguments)...)))
    {
    }

    RefPtr<PropertyType>& ensureAnimVal()
    {
        if (!m_animVal)
            m_animVal = PropertyType::create(this, SVGPropertyAccess::ReadOnly, m_baseVal->value());
        return m_animVal;
    }

    // Called when m_baseVal changes.
    void commitPropertyChange(SVGProperty* property) override
    {
        if (m_animVal)
            m_animVal->setValue(m_baseVal->value());
        SVGAnimatedProperty::commitPropertyChange(property);
    }

    Ref<PropertyType> m_baseVal;
    mutable RefPtr<PropertyType> m_animVal;
};

}
