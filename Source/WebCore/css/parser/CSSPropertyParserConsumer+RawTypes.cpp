/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+RawTypes.h"

#include "CSSCalcSymbolTable.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

void serializationForCSS(StringBuilder& builder, const AngleRaw& value)
{
    formatCSSNumberValue(builder, value.value, CSSPrimitiveValue::unitTypeString(value.type));
}

void serializationForCSS(StringBuilder& builder, const NumberRaw& value)
{
    formatCSSNumberValue(builder, value.value, CSSPrimitiveValue::unitTypeString(CSSUnitType::CSS_NUMBER));
}

void serializationForCSS(StringBuilder& builder, const PercentRaw& value)
{
    formatCSSNumberValue(builder, value.value, CSSPrimitiveValue::unitTypeString(CSSUnitType::CSS_PERCENTAGE));
}

void serializationForCSS(StringBuilder& builder, const LengthRaw& value)
{
    formatCSSNumberValue(builder, value.value, CSSPrimitiveValue::unitTypeString(value.type));
}

void serializationForCSS(StringBuilder& builder, const TimeRaw& value)
{
    formatCSSNumberValue(builder, value.value, CSSPrimitiveValue::unitTypeString(value.type));
}

void serializationForCSS(StringBuilder& builder, const ResolutionRaw& value)
{
    formatCSSNumberValue(builder, value.value, CSSPrimitiveValue::unitTypeString(value.type));
}

void serializationForCSS(StringBuilder& builder, const NoneRaw&)
{
    builder.append("none"_s);
}

void serializationForCSS(StringBuilder& builder, const SymbolRaw& value)
{
    builder.append(nameLiteralForSerialization(value.value));
}

NumberRaw replaceSymbol(SymbolRaw raw, const CSSCalcSymbolTable& symbolTable)
{
    auto result = symbolTable.get(raw.value);

    // We should only get here if the symbol was previously looked up in the symbol table.
    ASSERT(result);
    return { result->value };
}

} // namespace WebCore
