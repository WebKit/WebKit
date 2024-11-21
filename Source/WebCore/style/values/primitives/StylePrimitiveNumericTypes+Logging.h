/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSPrimitiveValue.h"
#include "StylePrimitiveNumericTypes.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

template<StyleNumericPrimitive T> WTF::TextStream& operator<<(WTF::TextStream& ts, const T& value)
{
    return ts << formatCSSNumberValue(value.value, CSSPrimitiveValue::unitTypeString(value.unit));
}

template<auto R> WTF::TextStream& operator<<(WTF::TextStream& ts, const AnglePercentage<R>& value)
{
    return value.value.switchOn(
        [&](Angle<R> value) -> WTF::TextStream& {
            return ts << formatCSSNumberValue(value.value, CSSPrimitiveValue::unitTypeString(value.unit));
        },
        [&](Percentage<R> value) -> WTF::TextStream& {
            return ts << formatCSSNumberValue(value.value, CSSPrimitiveValue::unitTypeString(value.unit));
        },
        [&](Ref<CalculationValue> value) -> WTF::TextStream& {
            return ts << value.get();
        }
    );
}

template<auto R> WTF::TextStream& operator<<(WTF::TextStream& ts, const LengthPercentage<R>& value)
{
    return value.value.switchOn(
        [&](Length<R> value) -> WTF::TextStream& {
            return ts << formatCSSNumberValue(value.value, CSSPrimitiveValue::unitTypeString(value.unit));
        },
        [&](Percentage<R> value) -> WTF::TextStream& {
            return ts << formatCSSNumberValue(value.value, CSSPrimitiveValue::unitTypeString(value.unit));
        },
        [&](Ref<CalculationValue> value) -> WTF::TextStream& {
            return ts << value.get();
        }
    );
}

template<auto R> WTF::TextStream& operator<<(WTF::TextStream& ts, const NumberOrPercentageResolvedToNumber<R>& value)
{
    return ts << formatCSSNumberValue(value.value, ""_s);
}

} // namespace Style
} // namespace WebCore
