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

#include "SVGAnimatedPropertyImpl.h"
#include "SVGAnimatedPropertyPairAnimator.h"
#include "SVGMarkerTypes.h"

namespace WebCore {

class SVGElement;

class SVGAnimatedAngleOrientAnimator final : public SVGAnimatedPropertyPairAnimator<SVGAnimatedAngleAnimator, SVGAnimatedOrientTypeAnimator> {
    using Base = SVGAnimatedPropertyPairAnimator<SVGAnimatedAngleAnimator, SVGAnimatedOrientTypeAnimator>;
    using Base::Base;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedAngle>& animated1, Ref<SVGAnimatedOrientType>& animated2, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedAngleOrientAnimator(attributeName, animated1, animated2, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void setFromAndToValues(SVGElement*, const String& from, const String& to) final
    {
        auto pairFrom = SVGPropertyTraits<std::pair<SVGAngleValue, SVGMarkerOrientType>>::fromString(from);
        auto pairTo = SVGPropertyTraits<std::pair<SVGAngleValue, SVGMarkerOrientType>>::fromString(to);

        m_animatedPropertyAnimator1->m_function.m_from = pairFrom.first;
        m_animatedPropertyAnimator1->m_function.m_to = pairTo.first;

        m_animatedPropertyAnimator2->m_function.m_from = pairFrom.second;
        m_animatedPropertyAnimator2->m_function.m_to = pairTo.second;
    }

    void setFromAndByValues(SVGElement* targetElement, const String& from, const String& by) final
    {
        setFromAndToValues(targetElement, from, by);
        if (m_animatedPropertyAnimator2->m_function.m_from != SVGMarkerOrientAngle || m_animatedPropertyAnimator2->m_function.m_to != SVGMarkerOrientAngle)
            return;
        m_animatedPropertyAnimator1->m_function.addFromAndToValues(targetElement);
    }

    void animate(SVGElement* targetElement, float progress, unsigned repeatCount) final
    {
        if (m_animatedPropertyAnimator2->m_function.m_from != m_animatedPropertyAnimator2->m_function.m_to) {
            // Discrete animation - no linear interpolation possible between values (e.g. auto to angle).
            m_animatedPropertyAnimator2->animate(targetElement, progress, repeatCount);

            SVGAngleValue animatedAngle;
            if (progress < 0.5f && m_animatedPropertyAnimator2->m_function.m_from == SVGMarkerOrientAngle)
                animatedAngle = m_animatedPropertyAnimator1->m_function.m_from;
            else if (progress >= 0.5f && m_animatedPropertyAnimator2->m_function.m_to == SVGMarkerOrientAngle)
                animatedAngle = m_animatedPropertyAnimator1->m_function.m_to;

            m_animatedPropertyAnimator1->m_animated->setAnimVal(animatedAngle);
            return;
        }

        if (m_animatedPropertyAnimator2->m_function.m_from == SVGMarkerOrientAngle) {
            // Regular from- toangle animation, with support for smooth interpolation, and additive and accumulated animation.
            m_animatedPropertyAnimator2->m_animated->setAnimVal(SVGMarkerOrientAngle);

            m_animatedPropertyAnimator1->animate(targetElement, progress, repeatCount);
            return;
        }

        // auto, auto-start-reverse, or unknown.
        m_animatedPropertyAnimator1->m_animated->animVal()->value().setValue(0);

        if (m_animatedPropertyAnimator2->m_function.m_from == SVGMarkerOrientAuto || m_animatedPropertyAnimator2->m_function.m_from == SVGMarkerOrientAutoStartReverse)
            m_animatedPropertyAnimator2->m_animated->setAnimVal(m_animatedPropertyAnimator2->m_function.m_from);
        else
            m_animatedPropertyAnimator2->m_animated->setAnimVal(SVGMarkerOrientUnknown);
    }

    void stop(SVGElement* targetElement) final
    {
        if (!m_animatedPropertyAnimator1->m_animated->isAnimating())
            return;
        apply(targetElement);
        Base::stop(targetElement);
    }
};

class SVGAnimatedIntegerPairAnimator final : public SVGAnimatedPropertyPairAnimator<SVGAnimatedIntegerAnimator, SVGAnimatedIntegerAnimator> {
    using Base = SVGAnimatedPropertyPairAnimator<SVGAnimatedIntegerAnimator, SVGAnimatedIntegerAnimator>;
    using Base::Base;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedInteger>& animated1, Ref<SVGAnimatedInteger>& animated2, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedIntegerPairAnimator(attributeName, animated1, animated2, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void setFromAndToValues(SVGElement*, const String& from, const String& to) final
    {
        auto pairFrom = SVGPropertyTraits<std::pair<int, int>>::fromString(from);
        auto pairTo = SVGPropertyTraits<std::pair<int, int>>::fromString(to);

        m_animatedPropertyAnimator1->m_function.m_from = pairFrom.first;
        m_animatedPropertyAnimator1->m_function.m_to = pairTo.first;

        m_animatedPropertyAnimator2->m_function.m_from = pairFrom.second;
        m_animatedPropertyAnimator2->m_function.m_to = pairTo.second;
    }

    void setFromAndByValues(SVGElement*, const String& from, const String& by) final
    {
        auto pairFrom = SVGPropertyTraits<std::pair<int, int>>::fromString(from);
        auto pairBy = SVGPropertyTraits<std::pair<int, int>>::fromString(by);

        m_animatedPropertyAnimator1->m_function.m_from = pairFrom.first;
        m_animatedPropertyAnimator1->m_function.m_to = pairFrom.first + pairBy.first;

        m_animatedPropertyAnimator2->m_function.m_from = pairFrom.second;
        m_animatedPropertyAnimator2->m_function.m_to = pairFrom.second + pairBy.second;
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) final
    {
        auto pairToAtEndOfDuration = SVGPropertyTraits<std::pair<int, int>>::fromString(toAtEndOfDuration);
        m_animatedPropertyAnimator1->m_function.m_toAtEndOfDuration = pairToAtEndOfDuration.first;
        m_animatedPropertyAnimator2->m_function.m_toAtEndOfDuration = pairToAtEndOfDuration.second;
    }
};

class SVGAnimatedNumberPairAnimator final : public SVGAnimatedPropertyPairAnimator<SVGAnimatedNumberAnimator, SVGAnimatedNumberAnimator> {
    using Base = SVGAnimatedPropertyPairAnimator<SVGAnimatedNumberAnimator, SVGAnimatedNumberAnimator>;
    using Base::Base;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedNumber>& animated1, Ref<SVGAnimatedNumber>& animated2, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return adoptRef(*new SVGAnimatedNumberPairAnimator(attributeName, animated1, animated2, animationMode, calcMode, isAccumulated, isAdditive));
    }

private:
    void setFromAndToValues(SVGElement*, const String& from, const String& to) final
    {
        auto pairFrom = SVGPropertyTraits<std::pair<float, float>>::fromString(from);
        auto pairTo = SVGPropertyTraits<std::pair<float, float>>::fromString(to);
        
        m_animatedPropertyAnimator1->m_function.m_from = pairFrom.first;
        m_animatedPropertyAnimator1->m_function.m_to = pairTo.first;
        
        m_animatedPropertyAnimator2->m_function.m_from = pairFrom.second;
        m_animatedPropertyAnimator2->m_function.m_to = pairTo.second;
    }

    void setFromAndByValues(SVGElement*, const String& from, const String& by) final
    {
        auto pairFrom = SVGPropertyTraits<std::pair<float, float>>::fromString(from);
        auto pairBy = SVGPropertyTraits<std::pair<float, float>>::fromString(by);
        
        m_animatedPropertyAnimator1->m_function.m_from = pairFrom.first;
        m_animatedPropertyAnimator1->m_function.m_to = pairFrom.first + pairBy.first;
        
        m_animatedPropertyAnimator2->m_function.m_from = pairFrom.second;
        m_animatedPropertyAnimator2->m_function.m_to = pairFrom.second + pairBy.second;
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) final
    {
        auto pairToAtEndOfDuration = SVGPropertyTraits<std::pair<float, float>>::fromString(toAtEndOfDuration);
        m_animatedPropertyAnimator1->m_function.m_toAtEndOfDuration = pairToAtEndOfDuration.first;
        m_animatedPropertyAnimator2->m_function.m_toAtEndOfDuration = pairToAtEndOfDuration.second;
    }
};

}
