/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "CSSKeywordValue.h"
#include "CSSNumericValue.h"
#include "CSSStyleValue.h"

namespace WebCore {

class CSSKeywordValue;

using CSSKeywordish = std::variant<String, RefPtr<CSSKeywordValue>>;
using CSSColorPercent = std::variant<double, RefPtr<CSSNumericValue>, String, RefPtr<CSSKeywordValue>>;
using RectifiedCSSColorPercent = std::variant<RefPtr<CSSNumericValue>, RefPtr<CSSKeywordValue>>;
using CSSColorNumber = std::variant<double, RefPtr<CSSNumericValue>, String, RefPtr<CSSKeywordValue>>;
using RectifiedCSSColorNumber = std::variant<RefPtr<CSSNumericValue>, RefPtr<CSSKeywordValue>>;
using CSSColorAngle = std::variant<double, RefPtr<CSSNumericValue>, String, RefPtr<CSSKeywordValue>>;
using RectifiedCSSColorAngle = std::variant<RefPtr<CSSNumericValue>, RefPtr<CSSKeywordValue>>;

class CSSColorValue : public CSSStyleValue {
public:
    RefPtr<CSSKeywordValue> colorSpace();
    RefPtr<CSSColorValue> to(CSSKeywordish);
    static std::variant<RefPtr<CSSColorValue>, RefPtr<CSSStyleValue>> parse(const String&);

    static ExceptionOr<RectifiedCSSColorPercent> rectifyCSSColorPercent(CSSColorPercent&&);
    static ExceptionOr<RectifiedCSSColorAngle> rectifyCSSColorAngle(CSSColorAngle&&);
    static ExceptionOr<RectifiedCSSColorNumber> rectifyCSSColorNumber(CSSColorNumber&&);
    static CSSColorPercent toCSSColorPercent(const RectifiedCSSColorPercent&);
    static CSSColorPercent toCSSColorPercent(const CSSNumberish&);
    static CSSColorAngle toCSSColorAngle(const RectifiedCSSColorAngle&);
    static CSSColorNumber toCSSColorNumber(const RectifiedCSSColorNumber&);

    RefPtr<CSSValue> toCSSValue() const final;
};
    
} // namespace WebCore
