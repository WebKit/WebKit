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

#include <WebCore/StylePrimitiveNumeric.h>
#include <WebCore/StyleUnevaluatedCalcSize.h>
#include <WebCore/StyleValueTypes.h>
#include <optional>

namespace WebCore {
namespace Style {

// Resolves the basis, substitutes it for the `size` keyword, then resolves the calculation, clamping
// to the property's range as Calculation::Value::evaluate() does for a calc().
//
// A keyword basis is the element's intrinsic size, which only layout knows, so layout passes it in as
// `keywordBasis`. Callers without one check behavesAsKeyword() and degrade to the keyword instead.
WEBCORE_EXPORT double evaluateCalcSize(const CalcSizeValue&, CSS::Range, double percentResolutionLength, ZoomFactor, std::optional<double> keywordBasis = std::nullopt);

template<typename T> concept IsPercentageOrCalcOrCalcSize = IsPercentageOrCalc<T> || std::same_as<T, UnevaluatedCalcSize>;

template<typename Result> struct Evaluation<UnevaluatedCalcSize, Result> {
    auto operator()(const UnevaluatedCalcSize& calcSize, Result percentResolutionLength, ZoomFactor usedZoom) -> Result
    {
        return Result(calcSize.evaluate(static_cast<double>(percentResolutionLength), usedZoom));
    }
};

} // namespace Style
} // namespace WebCore
