/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "SVGAnimatedPropertyAccessor.h"

namespace WebCore {

template<typename OwnerType, typename AnimatedPropertyType>
class SVGAnimatedPrimitivePropertyAccessor : public SVGAnimatedPropertyAccessor<OwnerType, AnimatedPropertyType> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, AnimatedPropertyType>;

public:
    using Base::property;
    using ValueType = AnimatedPropertyType::ValueType;

    SVGAnimatedPrimitivePropertyAccessor(const Ref<AnimatedPropertyType> OwnerType::*property, ValueType initialValue)
        : Base(property)
        , m_initialValue(initialValue)
    {
    }

    template<typename AccessorType, const Ref<AnimatedPropertyType> OwnerType::*property, ValueType initialValue>
    static const SVGMemberAccessor<OwnerType>& singleton()
    {
        static NeverDestroyed<AccessorType> propertyAccessor { property, initialValue };
        return propertyAccessor;
    }

    void resetBaseVal(const OwnerType& owner) const override
    {
        property(owner)->setBaseValInternal(m_initialValue);
    }

protected:
    ValueType m_initialValue;
};

} // namespace WebCore

