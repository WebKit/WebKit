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

#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

// MARK: - Serialization

struct SerializableNumber {
    double value;
    ASCIILiteral suffix;
};

void formatNonfiniteCSSNumberValue(StringBuilder&, const SerializableNumber&);
String formatNonfiniteCSSNumberValue(const SerializableNumber&);

void formatCSSNumberValue(StringBuilder&, const SerializableNumber&);
String formatCSSNumberValue(const SerializableNumber&);

template<> struct Serialize<SerializableNumber> {
    void operator()(StringBuilder&, const SerializationContext&, const SerializableNumber&);
};

template<NumericRaw RawType> struct Serialize<RawType> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const RawType& value)
    {
        serializationForCSS(builder, context, SerializableNumber { value.value, unitString(value.unit) });
    }
};

template<auto nR, auto pR, typename V> struct Serialize<NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const NumberOrPercentageResolvedToNumber<nR, pR, V>& value)
    {
        WTF::switchOn(value,
            [&](const typename NumberOrPercentageResolvedToNumber<nR, pR, V>::Number& number) {
                serializationForCSS(builder, context, number);
            },
            [&](const typename NumberOrPercentageResolvedToNumber<nR, pR, V>::Percentage& percentage) {
                if (auto raw = percentage.raw())
                    serializationForCSS(builder, context, NumberRaw<nR, V> { raw->value / 100.0 });
                else
                    serializationForCSS(builder, context, percentage);
            }
        );
    }
};

} // namespace CSS
} // namespace WebCore
