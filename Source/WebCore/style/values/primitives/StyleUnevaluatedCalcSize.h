/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSPrimitiveNumericRange.h>
#include <WebCore/CSSValueKeywords.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>

namespace WebCore {
namespace Style {

class CalcSizeValue;
Ref<CalcSizeValue> CLANG_POINTER_CONVERSION protect(CalcSizeValue&);

struct ZoomFactor;

// Wrapper for `Ref<CalcSizeValue>`, keeping the calc machinery out of the headers the sizing
// properties include. Every calc-size() has the same type and range, so this needs no type parameter.
class UnevaluatedCalcSize {
public:
    static constexpr auto range = CSS::NonnegativeLayoutUnitClamped;

    explicit UnevaluatedCalcSize(CalcSizeValue&);
    explicit UnevaluatedCalcSize(Ref<CalcSizeValue>&&);

    WEBCORE_EXPORT UnevaluatedCalcSize(const UnevaluatedCalcSize&);
    WEBCORE_EXPORT UnevaluatedCalcSize(UnevaluatedCalcSize&&);
    UnevaluatedCalcSize& operator=(const UnevaluatedCalcSize&);
    UnevaluatedCalcSize& operator=(UnevaluatedCalcSize&&);

    WEBCORE_EXPORT ~UnevaluatedCalcSize();

    CalcSizeValue& calcSize() const { return m_calcSize; }
    [[nodiscard]] CalcSizeValue& NODELETE leakRef();

    // CSSValueInvalid for a <calc-sum> or `any` basis, which behave as an ordinary length.
    WEBCORE_EXPORT CSSValueID basisKeyword() const;
    bool behavesAsKeyword() const { return basisKeyword() != CSSValueInvalid; }

    WEBCORE_EXPORT bool hasPercentage() const;
    WEBCORE_EXPORT bool basisHasPercentage() const;

    WEBCORE_EXPORT double evaluate(double percentResolutionLength, const ZoomFactor&) const;

    WEBCORE_EXPORT bool operator==(const UnevaluatedCalcSize&) const;

private:
    Ref<CalcSizeValue> m_calcSize;
};

WTF::TextStream& operator<<(WTF::TextStream&, const UnevaluatedCalcSize&);

// Queries on the value itself, for headers that only have it forward declared. Taking it by
// reference keeps the predicates free of the Ref that constructing an UnevaluatedCalcSize would
// bring, whose destructor is not allowed in the NODELETE callers that ask these questions.
WEBCORE_EXPORT CSSValueID NODELETE calcSizeBasisKeyword(const CalcSizeValue&);
WEBCORE_EXPORT bool NODELETE calcSizeHasPercentage(const CalcSizeValue&);

} // namespace Style
} // namespace WebCore
