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

#include "SVGAnimationFunction.h"

namespace WebCore {

class SVGAnimationAdditiveFunction : public SVGAnimationFunction {
public:
    SVGAnimationAdditiveFunction(AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
        : SVGAnimationFunction(animationMode)
        , m_calcMode(calcMode)
        , m_isAccumulated(isAccumulated)
        , m_isAdditive(isAdditive)
    {
    }

    void setFromAndByValues(SVGElement* targetElement, const String& from, const String& by) override
    {
        setFromAndToValues(targetElement, from, by);
        addFromAndToValues(targetElement);
    }

    void setToAtEndOfDurationValue(const String&) override
    {
        ASSERT_NOT_REACHED();
    }

protected:
    float animate(float progress, unsigned repeatCount, float from, float to, float toAtEndOfDuration, float animated)
    {
        float number;
        if (m_calcMode == CalcMode::Discrete)
            number = progress < 0.5 ? from : to;
        else
            number = (to - from) * progress + from;

        if (m_isAccumulated && repeatCount)
            number += toAtEndOfDuration * repeatCount;

        if (m_isAdditive && m_animationMode != AnimationMode::To)
            number += animated;

        return number;
    }

    CalcMode m_calcMode;
    bool m_isAccumulated;
    bool m_isAdditive;
};

}
