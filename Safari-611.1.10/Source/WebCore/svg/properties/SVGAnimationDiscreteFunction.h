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

namespace WebCore {

template<typename ValueType>
class SVGAnimationDiscreteFunction : public SVGAnimationFunction {
public:
    SVGAnimationDiscreteFunction(AnimationMode animationMode, CalcMode, bool, bool)
        : SVGAnimationFunction(animationMode)
    {
    }

    bool isDiscrete() const override { return true; }

    void setToAtEndOfDurationValue(const String&) override
    {
        ASSERT_NOT_REACHED();
    }

    void setFromAndByValues(SVGElement*, const String&, const String&) override
    {
        ASSERT_NOT_REACHED();
    }

    void animate(SVGElement*, float progress, unsigned, ValueType& animated)
    {
        if ((m_animationMode == AnimationMode::FromTo && progress > 0.5) || m_animationMode == AnimationMode::To || progress == 1)
            animated = m_to;
        else
            animated = m_from;
    }

protected:
    ValueType m_from;
    ValueType m_to;
};

}
