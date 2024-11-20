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

#include "config.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"

#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

void rawNumericSerialization(StringBuilder& builder, double value, CSSUnitType type)
{
    formatCSSNumberValue(builder, value, CSSPrimitiveValue::unitTypeString(type));
}

void Serialize<NumberOrPercentageResolvedToNumber>::operator()(StringBuilder& builder, const NumberOrPercentageResolvedToNumber& value)
{
    WTF::switchOn(value.value,
        [&](const Number<>& number) {
            serializationForCSS(builder, number);
        },
        [&](const Percentage<>& percentage) {
            if (auto raw = percentage.raw())
                serializationForCSS(builder, NumberRaw<> { raw->value / 100.0 });
            else
                serializationForCSS(builder, percentage);
        }
    );
}

void Serialize<SymbolRaw>::operator()(StringBuilder& builder, const SymbolRaw& value)
{
    builder.append(nameLiteralForSerialization(value.value));
}

void Serialize<Symbol>::operator()(StringBuilder& builder, const Symbol& value)
{
    builder.append(nameLiteralForSerialization(value.value));
}

} // namespace CSS
} // namespace WebCore
