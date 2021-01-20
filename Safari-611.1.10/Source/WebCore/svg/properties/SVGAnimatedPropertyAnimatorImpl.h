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

#include "HTMLNames.h"
#include "SVGAnimatedPropertyAnimator.h"
#include "SVGAnimatedPropertyImpl.h"
#include "SVGAnimationAdditiveListFunctionImpl.h"
#include "SVGAnimationAdditiveValueFunctionImpl.h"
#include "SVGAnimationDiscreteFunctionImpl.h"

namespace WebCore {

class SVGAnimatedAngleAnimator;
class SVGAnimatedIntegerPairAnimator;
class SVGAnimatedOrientTypeAnimator;

template<typename AnimatedPropertyAnimator1, typename AnimatedPropertyAnimator2>
class SVGAnimatedPropertyPairAnimator;

class SVGAnimatedAngleAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedAngle, SVGAnimationAngleFunction> {
    friend class SVGAnimatedPropertyPairAnimator<SVGAnimatedAngleAnimator, SVGAnimatedOrientTypeAnimator>;
    friend class SVGAnimatedAngleOrientAnimator;
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedAngle, SVGAnimationAngleFunction>;
    using Base::Base;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedAngle>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedAngleAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        m_function.animate(targetElement, progress, repeatCount, m_animated->animVal()->value());
    }
};

class SVGAnimatedBooleanAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedBoolean, SVGAnimationBooleanFunction>  {
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedBoolean, SVGAnimationBooleanFunction>;

public:
    using Base::Base;

    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedBoolean>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedBooleanAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        bool& animated = m_animated->animVal();
        m_function.animate(targetElement, progress, repeatCount, animated);
    }
};

template<typename EnumType>
class SVGAnimatedEnumerationAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedEnumeration, SVGAnimationEnumerationFunction<EnumType>> {
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedEnumeration, SVGAnimationEnumerationFunction<EnumType>>;
    using Base::Base;
    using Base::m_animated;
    using Base::m_function;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedEnumeration>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedEnumerationAnimator<EnumType>(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        EnumType animated;
        m_function.animate(targetElement, progress, repeatCount, animated);
        m_animated->template setAnimVal<EnumType>(animated);
    }
};

class SVGAnimatedIntegerAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedInteger, SVGAnimationIntegerFunction> {
    friend class SVGAnimatedPropertyPairAnimator<SVGAnimatedIntegerAnimator, SVGAnimatedIntegerAnimator>;
    friend class SVGAnimatedIntegerPairAnimator;
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedInteger, SVGAnimationIntegerFunction>;

public:
    using Base::Base;

    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedInteger>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedIntegerAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        m_function.animate(targetElement, progress, repeatCount, m_animated->animVal());
    }
};

class SVGAnimatedLengthAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedLength, SVGAnimationLengthFunction> {
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedLength, SVGAnimationLengthFunction>;

public:
    SVGAnimatedLengthAnimator(const QualifiedName& attributeName, Ref<SVGAnimatedLength>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive, SVGLengthMode lengthMode)
        : Base(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive, lengthMode)
    {
    }

    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedLength>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive, SVGLengthMode lengthMode)
    {
        return adoptRef(*new SVGAnimatedLengthAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive, lengthMode));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        m_function.animate(targetElement, progress, repeatCount, m_animated->animVal()->value());
    }
};

class SVGAnimatedLengthListAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedLengthList, SVGAnimationLengthListFunction> {
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedLengthList, SVGAnimationLengthListFunction>;

public:
    SVGAnimatedLengthListAnimator(const QualifiedName& attributeName, Ref<SVGAnimatedLengthList>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive, SVGLengthMode lengthMode)
        : Base(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive, lengthMode)
    {
    }

    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedLengthList>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive, SVGLengthMode lengthMode)
    {
        return adoptRef(*new SVGAnimatedLengthListAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive, lengthMode));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        m_function.animate(targetElement, progress, repeatCount, m_animated->animVal());
    }
};

class SVGAnimatedNumberAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedNumber, SVGAnimationNumberFunction> {
    friend class SVGAnimatedPropertyPairAnimator<SVGAnimatedNumberAnimator, SVGAnimatedNumberAnimator>;
    friend class SVGAnimatedNumberPairAnimator;
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedNumber, SVGAnimationNumberFunction>;
    using Base::Base;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedNumber>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedNumberAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        m_function.animate(targetElement, progress, repeatCount, m_animated->animVal());
    }
};

class SVGAnimatedNumberListAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedNumberList, SVGAnimationNumberListFunction> {
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedNumberList, SVGAnimationNumberListFunction>;
    using Base::Base;
    
public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedNumberList>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedNumberListAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }
    
private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        m_function.animate(targetElement, progress, repeatCount, m_animated->animVal());
    }
};

class SVGAnimatedPathSegListAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedPathSegList, SVGAnimationPathSegListFunction> {
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedPathSegList, SVGAnimationPathSegListFunction>;
    using Base::Base;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedPathSegList>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedPathSegListAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        m_animated->animVal()->pathByteStreamWillChange();
        m_function.animate(targetElement, progress, repeatCount, m_animated->animVal()->pathByteStream());
    }
};

class SVGAnimatedPointListAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedPointList, SVGAnimationPointListFunction> {
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedPointList, SVGAnimationPointListFunction>;
    using Base::Base;
    
public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedPointList>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedPointListAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }
    
private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        m_function.animate(targetElement, progress, repeatCount, m_animated->animVal());
    }
};

class SVGAnimatedOrientTypeAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedOrientType, SVGAnimationOrientTypeFunction> {
    friend class SVGAnimatedPropertyPairAnimator<SVGAnimatedAngleAnimator, SVGAnimatedOrientTypeAnimator>;
    friend class SVGAnimatedAngleOrientAnimator;
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedOrientType, SVGAnimationOrientTypeFunction>;
    using Base::Base;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedOrientType>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedOrientTypeAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        SVGMarkerOrientType animated;
        m_function.animate(targetElement, progress, repeatCount, animated);
        m_animated->setAnimVal(animated);
    }
};

class SVGAnimatedPreserveAspectRatioAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedPreserveAspectRatio, SVGAnimationPreserveAspectRatioFunction> {
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedPreserveAspectRatio, SVGAnimationPreserveAspectRatioFunction>;
    using Base::Base;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedPreserveAspectRatio>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedPreserveAspectRatioAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        SVGPreserveAspectRatioValue& animated = m_animated->animVal()->value();
        m_function.animate(targetElement, progress, repeatCount, animated);
    }
};

class SVGAnimatedRectAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedRect, SVGAnimationRectFunction> {
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedRect, SVGAnimationRectFunction>;

public:
    using Base::Base;

    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedRect>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedRectAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        m_function.animate(targetElement, progress, repeatCount, m_animated->animVal()->value());
    }
};

class SVGAnimatedStringAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedString, SVGAnimationStringFunction> {
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedString, SVGAnimationStringFunction>;
    using Base::Base;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedString>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedStringAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    bool isAnimatedStyleClassAniamtor() const
    {
        return m_attributeName.matches(HTMLNames::classAttr);
    }

    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        String& animated = m_animated->animVal();
        m_function.animate(targetElement, progress, repeatCount, animated);
    }
    
    void apply(SVGElement* targetElement) final
    {
        Base::apply(targetElement);
        if (isAnimatedStyleClassAniamtor())
            invalidateStyle(targetElement);
    }
    
    void stop(SVGElement* targetElement) final
    {
        if (!m_animated->isAnimating())
            return;

        Base::stop(targetElement);
        if (isAnimatedStyleClassAniamtor())
            invalidateStyle(targetElement);
    }
};

class SVGAnimatedTransformListAnimator final : public SVGAnimatedPropertyAnimator<SVGAnimatedTransformList, SVGAnimationTransformListFunction> {
    using Base = SVGAnimatedPropertyAnimator<SVGAnimatedTransformList, SVGAnimationTransformListFunction>;
    using Base::Base;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedTransformList>& animated, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedTransformListAnimator(attributeName, animated, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        m_function.animate(targetElement, progress, repeatCount, m_animated->animVal());
    }
};

}
