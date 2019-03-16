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

#include "SVGAnimationAdditiveValueFunction.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

class SVGAnimationIntegerFunction : public SVGAnimationAdditiveValueFunction<int> {
    friend class SVGAnimatedIntegerPairAnimator;

public:
    using Base = SVGAnimationAdditiveValueFunction<int>;
    using Base::Base;

    void setFromAndToValues(SVGElement*, const String& from, const String& to) override
    {
        m_from = SVGPropertyTraits<int>::fromString(from);
        m_to = SVGPropertyTraits<int>::fromString(to);
    }

    void setToAtEndOfDurationValue(const String& toAtEndOfDuration) override
    {
        m_toAtEndOfDuration = SVGPropertyTraits<int>::fromString(toAtEndOfDuration);
    }

    void progress(SVGElement*, float percentage, unsigned repeatCount, int& animated)
    {
        animated = static_cast<int>(roundf(Base::progress(percentage, repeatCount, m_from, m_to, toAtEndOfDuration(), animated)));
    }

    float calculateDistance(SVGElement*, const String& from, const String& to) const override
    {
        return std::abs(to.toIntStrict() - from.toIntStrict());
    }

private:
    void addFromAndToValues(SVGElement*) override
    {
        m_to += m_from;
    }
};

}
