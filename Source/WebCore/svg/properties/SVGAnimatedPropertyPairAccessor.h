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

#include "SVGMemberAccessor.h"

namespace WebCore {

template<typename OwnerType, typename AccessorType1, typename AccessorType2>
class SVGAnimatedPropertyPairAccessor : public SVGMemberAccessor<OwnerType> {
    using AnimatedPropertyType1 = typename AccessorType1::AnimatedProperty;
    using AnimatedPropertyType2 = typename AccessorType2::AnimatedProperty;
    using Base = SVGMemberAccessor<OwnerType>;

public:
    SVGAnimatedPropertyPairAccessor(Ref<AnimatedPropertyType1> OwnerType::*property1, Ref<AnimatedPropertyType2> OwnerType::*property2)
        : m_accessor1(property1)
        , m_accessor2(property2)
    {
    }

protected:
    template<typename AccessorType, Ref<AnimatedPropertyType1> OwnerType::*property1, Ref<AnimatedPropertyType2> OwnerType::*property2>
    static SVGMemberAccessor<OwnerType>& singleton()
    {
        static NeverDestroyed<AccessorType> propertyAccessor { property1, property2 };
        return propertyAccessor;
    }

    bool isAnimatedProperty() const override { return true; }

    Ref<AnimatedPropertyType1>& property1(OwnerType& owner) const { return m_accessor1.property(owner); }
    const Ref<AnimatedPropertyType1>& property1(const OwnerType& owner) const { return m_accessor1.property(owner); }

    Ref<AnimatedPropertyType2>& property2(OwnerType& owner) const { return m_accessor2.property(owner); }
    const Ref<AnimatedPropertyType2>& property2(const OwnerType& owner) const { return m_accessor2.property(owner); }

    void detach(const OwnerType& owner) const override
    {
        property1(owner)->detach();
        property2(owner)->detach();
    }

    bool matches(const OwnerType& owner, const SVGAnimatedProperty& animatedProperty) const override
    {
        return m_accessor1.matches(owner, animatedProperty) || m_accessor2.matches(owner, animatedProperty);
    }

    AccessorType1 m_accessor1;
    AccessorType2 m_accessor2;
};

}
