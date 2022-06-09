/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(CSS_TYPED_OM)

#include "CSSMathValue.h"

namespace WebCore {

class CSSMathInvert : public CSSMathValue {
    WTF_MAKE_ISO_ALLOCATED(CSSMathInvert);
public:
    static Ref<CSSMathInvert> create(CSSNumberish&&);
    const CSSNumericValue& value() const { return m_value.get(); }
    
private:
    CSSMathInvert(CSSNumberish&&);
    Ref<CSSNumericValue> m_value;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSMathInvert)
    static bool isType(const WebCore::CSSStyleValue& styleValue) { return is<WebCore::CSSNumericValue>(styleValue) && isType(downcast<WebCore::CSSNumericValue>(styleValue)); }
    static bool isType(const WebCore::CSSNumericValue& numericValue) { return is<WebCore::CSSMathValue>(numericValue) && isType(downcast<WebCore::CSSMathValue>(numericValue)); }
    static bool isType(const WebCore::CSSMathValue& mathValue) { return mathValue.getOperator() == WebCore::CSSMathOperator::Invert; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
