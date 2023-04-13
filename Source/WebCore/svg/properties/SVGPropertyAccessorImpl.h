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

#include "SVGPropertyAccessor.h"
#include "SVGStringList.h"
#include "SVGTests.h"

namespace WebCore {

template<typename OwnerType>
class SVGConditionalProcessingAttributeAccessor final : public SVGMemberAccessor<OwnerType> {
    using Base = SVGMemberAccessor<OwnerType>;

public:
    SVGConditionalProcessingAttributeAccessor(Ref<SVGStringList> SVGConditionalProcessingAttributes::*property)
        : m_property(property)
    {
    }

    Ref<SVGStringList>& property(OwnerType& owner) const { return owner.conditionalProcessingAttributes().*m_property; }
    const Ref<SVGStringList>& property(const OwnerType& owner) const { return const_cast<OwnerType&>(owner).conditionalProcessingAttributes().*m_property; }

    void detach(const OwnerType& owner) const override
    {
        property(owner)->detach();
    }

    std::optional<String> synchronize(const OwnerType& owner) const override
    {
        return property(owner)->synchronize();
    }

    bool matches(const OwnerType& owner, const SVGProperty& property) const override
    {
        return this->property(owner).ptr() == &property;
    }

    template<Ref<SVGStringList> SVGConditionalProcessingAttributes::*>
    static const SVGMemberAccessor<OwnerType>& singleton();

private:
    Ref<SVGStringList> SVGConditionalProcessingAttributes::*m_property;
};

template<typename OwnerType>
template<Ref<SVGStringList> SVGConditionalProcessingAttributes::*member>
const SVGMemberAccessor<OwnerType>& SVGConditionalProcessingAttributeAccessor<OwnerType>::singleton()
{
    static NeverDestroyed<SVGConditionalProcessingAttributeAccessor<OwnerType>> propertyAccessor { member };
    return propertyAccessor;
}

template<typename OwnerType>
class SVGTransformListAccessor final : public SVGPropertyAccessor<OwnerType, SVGTransformList> {
    using Base = SVGPropertyAccessor<OwnerType, SVGTransformList>;

public:
    using Base::Base;
    template<Ref<SVGTransformList> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGTransformListAccessor, property>(); }
};

}
