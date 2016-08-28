/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)

#include "CSSValue.h"

namespace WebCore {

class CSSAnimationTriggerScrollValue final : public CSSValue {
public:
    static Ref<CSSAnimationTriggerScrollValue> create(Ref<CSSValue>&& startValue, RefPtr<CSSValue>&& endValue = nullptr)
    {
        return adoptRef(*new CSSAnimationTriggerScrollValue(WTFMove(startValue), WTFMove(endValue)));
    }

    const CSSValue& startValue() const { return m_startValue.get(); }
    const CSSValue* endValue() const { return m_endValue.get(); }
    bool hasEndValue() const { return m_endValue; }

    String customCSSText() const;

    bool equals(const CSSAnimationTriggerScrollValue&) const;
    bool operator==(const CSSAnimationTriggerScrollValue& other) const { return equals(other); }

private:
    CSSAnimationTriggerScrollValue(Ref<CSSValue>&& startValue, RefPtr<CSSValue>&& endValue)
        : CSSValue(AnimationTriggerScrollClass)
        , m_startValue(WTFMove(startValue))
        , m_endValue(WTFMove(endValue))
    {
    }

    Ref<CSSValue> m_startValue;
    RefPtr<CSSValue> m_endValue;
};

}

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSAnimationTriggerScrollValue, isAnimationTriggerScrollValue())

#endif
