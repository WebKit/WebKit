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

#include "SVGAnimatedPropertyAccessorImpl.h"
#include "SVGAnimatedPropertyAnimatorImpl.h"
#include "SVGAnimatedPropertyImpl.h"
#include "SVGAnimatedPropertyPairAccessor.h"
#include "SVGAnimatedPropertyPairAnimatorImpl.h"
#include "SVGNames.h"

namespace WebCore {

template<typename OwnerType>
class SVGAnimatedAngleOrientAccessor final : public SVGAnimatedPropertyPairAccessor<OwnerType, SVGAnimatedAngleAccessor<OwnerType>, SVGAnimatedOrientTypeAccessor<OwnerType>> {
    using Base = SVGAnimatedPropertyPairAccessor<OwnerType, SVGAnimatedAngleAccessor<OwnerType>, SVGAnimatedOrientTypeAccessor<OwnerType>>;
    using Base::property1;
    using Base::property2;

public:
    using Base::Base;
    template<Ref<SVGAnimatedAngle> OwnerType::*property1, Ref<SVGAnimatedOrientType> OwnerType::*property2>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedAngleOrientAccessor, property1, property2>(); }

private:
    Optional<String> synchronize(const OwnerType& owner) const final
    {
        bool dirty1 = property1(owner)->isDirty();
        bool dirty2 = property2(owner)->isDirty();
        if (!(dirty1 || dirty2))
            return WTF::nullopt;

        auto type = property2(owner)->baseVal();

        String string1 = dirty1 ? *property1(owner)->synchronize() : property1(owner)->baseValAsString();
        String string2 = dirty2 ? *property2(owner)->synchronize() : property2(owner)->baseValAsString();
        return type == SVGMarkerOrientAuto || type == SVGMarkerOrientAutoStartReverse ? string2 : string1;
    }

    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedAngleOrientAnimator::create(attributeName, property1(owner), property2(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedAngleOrientAnimator&>(animator).appendAnimatedInstance(property1(owner), property2(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedIntegerPairAccessor final : public SVGAnimatedPropertyPairAccessor<OwnerType, SVGAnimatedIntegerAccessor<OwnerType>, SVGAnimatedIntegerAccessor<OwnerType>> {
    using Base = SVGAnimatedPropertyPairAccessor<OwnerType, SVGAnimatedIntegerAccessor<OwnerType>, SVGAnimatedIntegerAccessor<OwnerType>>;
    using Base::property1;
    using Base::property2;

public:
    using Base::Base;
    template<Ref<SVGAnimatedInteger> OwnerType::*property1, Ref<SVGAnimatedInteger> OwnerType::*property2>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedIntegerPairAccessor, property1, property2>(); }

private:
    Optional<String> synchronize(const OwnerType& owner) const final
    {
        bool dirty1 = property1(owner)->isDirty();
        bool dirty2 = property2(owner)->isDirty();
        if (!(dirty1 || dirty2))
            return WTF::nullopt;

        String string1 = dirty1 ? *property1(owner)->synchronize() : property1(owner)->baseValAsString();
        String string2 = dirty2 ? *property2(owner)->synchronize() : property2(owner)->baseValAsString();
        return string1 == string2 ? string1 : string1 + ", " + string2;
    }

    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedIntegerPairAnimator::create(attributeName, property1(owner), property2(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedIntegerPairAnimator&>(animator).appendAnimatedInstance(property1(owner), property2(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedNumberPairAccessor final : public SVGAnimatedPropertyPairAccessor<OwnerType, SVGAnimatedNumberAccessor<OwnerType>, SVGAnimatedNumberAccessor<OwnerType>> {
    using Base = SVGAnimatedPropertyPairAccessor<OwnerType, SVGAnimatedNumberAccessor<OwnerType>, SVGAnimatedNumberAccessor<OwnerType>>;
    using Base::property1;
    using Base::property2;

public:
    using Base::Base;
    template<Ref<SVGAnimatedNumber> OwnerType::*property1, Ref<SVGAnimatedNumber> OwnerType::*property2 >
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedNumberPairAccessor, property1, property2>(); }

private:
    Optional<String> synchronize(const OwnerType& owner) const final
    {
        bool dirty1 = property1(owner)->isDirty();
        bool dirty2 = property2(owner)->isDirty();
        if (!(dirty1 || dirty2))
            return WTF::nullopt;

        String string1 = dirty1 ? *property1(owner)->synchronize() : property1(owner)->baseValAsString();
        String string2 = dirty2 ? *property2(owner)->synchronize() : property2(owner)->baseValAsString();
        return string1 == string2 ? string1 : string1 + ", " + string2;
    }

    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedNumberPairAnimator::create(attributeName, property1(owner), property2(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedNumberPairAnimator&>(animator).appendAnimatedInstance(property1(owner), property2(owner));
    }
};

}
