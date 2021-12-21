/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "CSSStyleValue.h"
#include <variant>
#include <wtf/FixedVector.h>

namespace WebCore {

class CSSNumericValue;
class CSSUnitValue;
class CSSMathSum;
struct CSSNumericType;

template<typename> class ExceptionOr;

class CSSNumericValue : public CSSStyleValue {
    WTF_MAKE_ISO_ALLOCATED(CSSNumericValue);
public:
    using CSSNumberish = std::variant<double, RefPtr<CSSNumericValue>>;

    Ref<CSSNumericValue> add(FixedVector<CSSNumberish>&&);
    Ref<CSSNumericValue> sub(FixedVector<CSSNumberish>&&);
    Ref<CSSNumericValue> mul(FixedVector<CSSNumberish>&&);
    Ref<CSSNumericValue> div(FixedVector<CSSNumberish>&&);
    Ref<CSSNumericValue> min(FixedVector<CSSNumberish>&&);
    Ref<CSSNumericValue> max(FixedVector<CSSNumberish>&&);
    
    bool equals(FixedVector<CSSNumberish>&&);
    
    Ref<CSSUnitValue> to(String&&);
    Ref<CSSMathSum> toSum(FixedVector<String>&&);
    CSSNumericType type();
    
    static ExceptionOr<Ref<CSSNumericValue>> parse(String&&);
    static Ref<CSSNumericValue> rectifyNumberish(CSSNumberish&&);
    
    CSSStyleValueType getType() const override { return CSSStyleValueType::CSSNumericValue; }

protected:
    CSSNumericValue() = default;
};

using CSSNumberish = CSSNumericValue::CSSNumberish;

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSNumericValue)
    static bool isType(const WebCore::CSSStyleValue& styleValue) { return styleValue.getType() == WebCore::CSSStyleValueType::CSSNumericValue; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
