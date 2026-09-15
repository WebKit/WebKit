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

namespace WebCore {
namespace Style {

// Resolves the basis, binds it to the `size` keyword, then resolves the calculation. A keyword basis
// stands for the element's intrinsic size, which only layout knows, so callers check
// behavesAsKeyword() and degrade to the keyword before reaching here.
// FIXME: Resolve keyword bases in layout, so calc-size(auto, size * 2) works.
WEBCORE_EXPORT double evaluateCalcSize(const CalcSizeValue&, double percentResolutionLength, ZoomFactor);

// Clamps the result to the property's range, as Calculation::Value::evaluate() does for a calc().
WEBCORE_EXPORT double evaluateCalcSize(const CalcSizeValue&, CSS::Range, double percentResolutionLength, ZoomFactor);

template<typename T> concept IsPercentageOrCalcOrCalcSize = IsPercentageOrCalc<T> || std::same_as<T, UnevaluatedCalcSize>;

template<typename Result> struct Evaluation<UnevaluatedCalcSize, Result> {
    auto operator()(const UnevaluatedCalcSize& calcSize, Result percentResolutionLength, ZoomFactor usedZoom) -> Result
    {
        return Result(calcSize.evaluate(static_cast<double>(percentResolutionLength), usedZoom));
    }
};

} // namespace Style
} // namespace WebCore
