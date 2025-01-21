/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSCalcValue.h"
#include "CSSPrimitiveValue.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

class BuilderState;

template<Numeric StyleType> struct CSSValueConversions<StyleType> {
    using CSSType = typename ToCSSMapping<StyleType>::type;

    StyleType operator()(const CSSValue& value, const BuilderState& state)
    {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        if (RefPtr calc = const_cast<CSSCalcValue*>(primitiveValue.cssCalcValue()))
            return toStyle(CSSType { typename CSSType::Calc { calc.releaseNonNull() } }, state);

        auto unit = CSSType::UnitTraits::validate(primitiveValue.primitiveType());
        RELEASE_ASSERT(unit);
        return toStyle(CSSType { typename CSSType::Raw { *unit, primitiveValue.valueNoConversionDataRequired() } }, state);
    }
};

} // namespace Style
} // namespace WebCore
