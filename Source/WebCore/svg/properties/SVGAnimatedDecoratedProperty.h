/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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
#include "SVGDecoratedProperty.h"
#include "SVGPropertyTraits.h"
#include <type_traits>

namespace WebCore {

template<template <typename, typename> class DecoratedProperty, typename DecorationType>
class SVGAnimatedDecoratedProperty : public SVGAnimatedProperty<SVGAnimatedDecoratedProperty<DecoratedProperty, DecorationType>> {
    using Base = SVGAnimatedProperty<SVGAnimatedDecoratedProperty<DecoratedProperty, DecorationType>>;
public:
    // The value passed here is the attribute's initial value, which the property keeps so that it
    // can be restored when the attribute is removed or fails to parse. There is deliberately no
    // overload without it: a property with no initial value would restore 0, which is not a legal
    // enumeration value.
    template<typename PropertyType, typename AnimatedProperty = SVGAnimatedDecoratedProperty>
    static Ref<AnimatedProperty> create(SVGElement* contextElement, const PropertyType& initialValue)
    {
        return adoptRef(*new AnimatedProperty(contextElement, DecoratedProperty<DecorationType, PropertyType>::create(initialValue), static_cast<DecorationType>(initialValue)));
    }

    SVGAnimatedDecoratedProperty(SVGElement* contextElement, Ref<SVGDecoratedProperty<DecorationType>>&& baseVal, DecorationType initialValue)
        : Base(contextElement)
        , m_baseVal(WTF::move(baseVal))
        , m_initialValue(initialValue)
    {
    }

    // Used by the DOM.
    ExceptionOr<void> setBaseVal(const DecorationType& baseVal)
    {
        if (!m_baseVal->setValue(baseVal))
            return Exception { ExceptionCode::TypeError };
        this->commitPropertyChange(nullptr);
        return { };
    }

    // Used by SVGElement::parseAttribute().
    template<typename PropertyType>
    void setBaseValInternal(const PropertyType& baseVal)
    {
        m_baseVal->setValueInternal(static_cast<DecorationType>(baseVal));
        if (m_animVal)
            m_animVal->setValueInternal(static_cast<DecorationType>(baseVal));
    }

    // Parses an enumerated attribute value, restoring the attribute's initial value when the value
    // is absent or is not one of the keywords in the attribute's grammar.
    // https://w3c.github.io/svgwg/svg2-draft/types.html#InvalidValues
    //
    // Returns whether the value was recognized. The test for "not recognized" matches the one
    // SVGDecoratedEnumeration::setValue() applies to values coming from the DOM: zero, which every
    // SVGPropertyTraits<EnumType>::fromString() returns for an unrecognized keyword, or a value
    // above the highest keyword of this attribute's grammar. The latter matters for attributes
    // whose enumeration is shared with a non-SVG consumer that has more values than SVG allows,
    // such as feBlend's mode, where BlendMode carries PlusDarker and PlusLighter.
    //
    // FIXME: SVGPropertyTraits<BlendMode>::fromString() cannot report failure at all -- it has no
    // zero to return, so an unrecognized feBlend mode arrives here as BlendMode::Normal and is
    // reported as recognized. Harmless today because Normal is also feBlend's initial value, so the
    // restored value is right either way, but it means a caller cannot tell a bad mode from a good
    // one. Fixing that needs BlendMode itself to express failure, which is out of scope here.
    template<typename EnumType>
    bool parseBaseVal(SVGElement& contextElement, const AtomString& value)
    {
        static_assert(std::is_integral<DecorationType>::value, "Restoring the initial value relies on zero meaning \"not a legal enumeration value\", which needs an integral DecorationType.");

        auto parsedValue = SVGPropertyTraits<EnumType>::fromString(contextElement, value);
        auto decoratedValue = static_cast<DecorationType>(parsedValue);
        if (!decoratedValue || decoratedValue > SVGPropertyTraits<EnumType>::highestEnumValue()) {
            setBaseValInternal<DecorationType>(m_initialValue);
            return false;
        }
        setBaseValInternal<EnumType>(parsedValue);
        return true;
    }

    DecorationType baseVal() const { return m_baseVal->value(); }

    // Used by SVGAnimator::progress.
    template<typename PropertyType>
    void setAnimVal(const PropertyType& animVal)
    {
        ASSERT(this->isAnimating() && m_animVal);
        m_animVal->setValueInternal(static_cast<DecorationType>(animVal));
    }

    template<typename PropertyType = DecorationType>
    PropertyType animVal() const
    {
        ASSERT_IMPLIES(this->isAnimating(), m_animVal);
        return static_cast<PropertyType>((this->isAnimating() ? *m_animVal : m_baseVal.get()).value());
    }

    // Used when committing a change from the SVGAnimatedProperty to the attribute.
    String baseValAsString() const override { return m_baseVal->valueAsString(); }

    // Used to apply the SVGAnimator change to the target element.
    String animValAsString() const override
    {
        ASSERT(this->isAnimating() && !!m_animVal);
        return m_animVal->valueAsString();
    }

    // Managing the relationship with the owner.
    void setDirty() override { m_state = SVGPropertyState::Dirty; }
    bool isDirty() const override { return m_state == SVGPropertyState::Dirty; }
    std::optional<String> synchronize() override
    {
        if (m_state == SVGPropertyState::Clean)
            return std::nullopt;
        m_state = SVGPropertyState::Clean;
        return baseValAsString();
    }

    // Used by RenderSVGElements and DumpRenderTree.
    template<typename PropertyType>
    PropertyType currentValue() const
    {
        ASSERT_IMPLIES(this->isAnimating(), m_animVal);
        return static_cast<PropertyType>((this->isAnimating() ? *m_animVal : m_baseVal.get()).valueInternal());
    }

    // Controlling the animation.
    void startAnimation(SVGAttributeAnimator& animator) override
    {
        if (m_animVal)
            m_animVal->setValue(m_baseVal->value());
        else
            m_animVal = m_baseVal->clone();
        Base::startAnimation(animator);
    }
    void stopAnimation(SVGAttributeAnimator& animator) override
    {
        Base::stopAnimation(animator);
        if (!this->isAnimating())
            m_animVal = nullptr;
        else if (m_animVal)
            m_animVal->setValue(m_baseVal->value());
    }

    // Controlling the instance animation.
    void instanceStartAnimationImpl(SVGAttributeAnimator& animator, SVGAnimatedDecoratedProperty& animated) override
    {
        if (!this->isAnimating())
            m_animVal = animated.m_animVal;
        Base::startAnimation(animator);
    }

    void instanceStopAnimationImpl(SVGAttributeAnimator& animator) override
    {
        Base::stopAnimation(animator);
        if (!this->isAnimating())
            m_animVal = nullptr;
    }

protected:
    const Ref<SVGDecoratedProperty<DecorationType>> m_baseVal;
    RefPtr<SVGDecoratedProperty<DecorationType>> m_animVal;
    const DecorationType m_initialValue { 0 };
    SVGPropertyState m_state { SVGPropertyState::Clean };
};

}
