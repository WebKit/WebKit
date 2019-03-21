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

class SVGAnimatedIntegerPairAnimator final : public SVGAnimatedPropertyPairAnimator<SVGAnimatedIntegerAnimator, SVGAnimatedIntegerAnimator> {
    using Base = SVGAnimatedPropertyPairAnimator<SVGAnimatedIntegerAnimator, SVGAnimatedIntegerAnimator>;
    using Base::Base;

public:
    static auto create(const QualifiedName& attributeName, Ref<SVGAnimatedInteger>& animated1, Ref<SVGAnimatedInteger>& animated2, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return std::unique_ptr<SVGAnimatedIntegerPairAnimator>(new SVGAnimatedIntegerPairAnimator(attributeName, animated1, animated2, animationMode, calcMode, isAccumulated, isAdditive));
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
        return std::unique_ptr<SVGAnimatedNumberPairAnimator>(new SVGAnimatedNumberPairAnimator(attributeName, animated1, animated2, animationMode, calcMode, isAccumulated, isAdditive));
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
