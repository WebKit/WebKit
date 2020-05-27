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
#include "SVGDecoratedProperty.h"

namespace WebCore {

template<template <typename, typename> class DecoratedProperty, typename DecorationType>
class SVGAnimatedDecoratedProperty : public SVGAnimatedProperty {
public:
    template<typename PropertyType, typename AnimatedProperty = SVGAnimatedDecoratedProperty>
    static Ref<AnimatedProperty> create(SVGElement* contextElement)
    {
        return adoptRef(*new AnimatedProperty(contextElement, adoptRef(*new DecoratedProperty<DecorationType, PropertyType>())));
    }

    template<typename PropertyType, typename AnimatedProperty = SVGAnimatedDecoratedProperty>
    static Ref<AnimatedProperty> create(SVGElement* contextElement, const PropertyType& value)
    {
        return adoptRef(*new AnimatedProperty(contextElement, DecoratedProperty<DecorationType, PropertyType>::create(value)));
    }

    SVGAnimatedDecoratedProperty(SVGElement* contextElement, Ref<SVGDecoratedProperty<DecorationType>>&& baseVal)
        : SVGAnimatedProperty(contextElement)
        , m_baseVal(WTFMove(baseVal))
    {
    }

    // Used by the DOM.
    ExceptionOr<void> setBaseVal(const DecorationType& baseVal)
    {
        if (!m_baseVal->setValue(baseVal))
            return Exception { TypeError };
        commitPropertyChange(nullptr);
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

    DecorationType baseVal() const { return m_baseVal->value(); }

    // Used by SVGAnimator::progress.
    template<typename PropertyType>
    void setAnimVal(const PropertyType& animVal)
    {
        ASSERT(isAnimating() && m_animVal);
        m_animVal->setValueInternal(static_cast<DecorationType>(animVal));
    }

    template<typename PropertyType = DecorationType>
    PropertyType animVal() const
    {
        ASSERT_IMPLIES(isAnimating(), m_animVal);
        return static_cast<PropertyType>((isAnimating() ? *m_animVal : m_baseVal.get()).value());
    }

    // Used when committing a change from the SVGAnimatedProperty to the attribute.
    String baseValAsString() const override { return m_baseVal->valueAsString(); }

    // Used to apply the SVGAnimator change to the target element.
    String animValAsString() const override
    {
        ASSERT(isAnimating() && !!m_animVal);
        return m_animVal->valueAsString();
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
    template<typename PropertyType>
    PropertyType currentValue() const
    {
        ASSERT_IMPLIES(isAnimating(), m_animVal);
        return static_cast<PropertyType>((isAnimating() ? *m_animVal : m_baseVal.get()).valueInternal());
    }

    // Controlling the animation.
    void startAnimation(SVGAttributeAnimator& animator) override
    {
        if (m_animVal)
            m_animVal->setValue(m_baseVal->value());
        else
            m_animVal = m_baseVal->clone();
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
        m_animVal = static_cast<decltype(*this)>(animated).m_animVal;
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
    Ref<SVGDecoratedProperty<DecorationType>> m_baseVal;
    RefPtr<SVGDecoratedProperty<DecorationType>> m_animVal;
    SVGPropertyState m_state { SVGPropertyState::Clean };
};

}
