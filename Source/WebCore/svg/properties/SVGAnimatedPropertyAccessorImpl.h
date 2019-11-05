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

#include "SVGAnimatedPropertyAccessor.h"
#include "SVGAnimatedPropertyAnimatorImpl.h"
#include "SVGAnimatedPropertyImpl.h"
#include "SVGNames.h"

namespace WebCore {

template<typename OwnerType>
class SVGAnimatedAngleAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedAngle> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedAngle>;

public:
    using Base::Base;
    template<Ref<SVGAnimatedAngle> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedAngleAccessor, property>(); }
};

template<typename OwnerType>
class SVGAnimatedBooleanAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedBoolean> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedBoolean>;
    using Base::property;

public:
    using Base::Base;
    template<Ref<SVGAnimatedBoolean> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedBooleanAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedBooleanAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedBooleanAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};

template<typename OwnerType, typename EnumType>
class SVGAnimatedEnumerationAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedEnumeration> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedEnumeration>;
    using Base::property;

public:
    using Base::Base;
    template<Ref<SVGAnimatedEnumeration> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedEnumerationAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedEnumerationAnimator<EnumType>::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedEnumerationAnimator<EnumType>&>(animator).appendAnimatedInstance(property(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedIntegerAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedInteger> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedInteger>;

public:
    using Base::Base;
    using Base::property;
    template<Ref<SVGAnimatedInteger> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedIntegerAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedIntegerAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedIntegerAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedLengthAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedLength> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedLength>;
    using Base::property;

public:
    using Base::Base;
    template<Ref<SVGAnimatedLength> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedLengthAccessor, property>(); }

private:
    bool isAnimatedLength() const override { return true; }

    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        SVGLengthMode lengthMode = property(owner)->baseVal()->value().unitMode();
        return SVGAnimatedLengthAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive, lengthMode);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedLengthAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedLengthListAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedLengthList> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedLengthList>;
    using Base::property;

public:
    using Base::Base;
    template<Ref<SVGAnimatedLengthList> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedLengthListAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedLengthListAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive, LengthModeWidth);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedLengthListAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedNumberAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedNumber> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedNumber>;

public:
    using Base::Base;
    using Base::property;
    template<Ref<SVGAnimatedNumber> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedNumberAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedNumberAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedNumberAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedNumberListAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedNumberList> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedNumberList>;
    using Base::property;

public:
    using Base::Base;
    template<Ref<SVGAnimatedNumberList> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedNumberListAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedNumberListAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedNumberListAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedPathSegListAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedPathSegList> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedPathSegList>;
    using Base::property;

public:
    using Base::Base;
    template<Ref<SVGAnimatedPathSegList> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedPathSegListAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedPathSegListAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedPathSegListAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedPointListAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedPointList> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedPointList>;
    using Base::property;

public:
    using Base::Base;
    template<Ref<SVGAnimatedPointList> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedPointListAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedPointListAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedPointListAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};
    
template<typename OwnerType>
class SVGAnimatedOrientTypeAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedOrientType> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedOrientType>;

public:
    using Base::Base;
    template<Ref<SVGAnimatedOrientType> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() {return Base::template singleton<SVGAnimatedOrientTypeAccessor, property>(); }
};

template<typename OwnerType>
class SVGAnimatedPreserveAspectRatioAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedPreserveAspectRatio> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedPreserveAspectRatio>;
    using Base::property;

public:
    using Base::Base;
    template<Ref<SVGAnimatedPreserveAspectRatio> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedPreserveAspectRatioAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedPreserveAspectRatioAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedPreserveAspectRatioAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedRectAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedRect> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedRect>;
    using Base::property;

public:
    using Base::Base;
    template<Ref<SVGAnimatedRect> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedRectAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedRectAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedRectAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedStringAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedString> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedString>;
    using Base::property;

public:
    using Base::Base;
    template<Ref<SVGAnimatedString> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedStringAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedStringAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedStringAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};

template<typename OwnerType>
class SVGAnimatedTransformListAccessor final : public SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedTransformList> {
    using Base = SVGAnimatedPropertyAccessor<OwnerType, SVGAnimatedTransformList>;
    using Base::property;

public:
    using Base::Base;
    template<Ref<SVGAnimatedTransformList> OwnerType::*property>
    constexpr static const SVGMemberAccessor<OwnerType>& singleton() { return Base::template singleton<SVGAnimatedTransformListAccessor, property>(); }

private:
    RefPtr<SVGAttributeAnimator> createAnimator(OwnerType& owner, const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive) const final
    {
        return SVGAnimatedTransformListAnimator::create(attributeName, property(owner), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void appendAnimatedInstance(OwnerType& owner, SVGAttributeAnimator& animator) const final
    {
        static_cast<SVGAnimatedTransformListAnimator&>(animator).appendAnimatedInstance(property(owner));
    }
};

}
