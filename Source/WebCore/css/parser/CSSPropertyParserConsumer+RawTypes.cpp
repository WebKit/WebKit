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
#include "CSSPrimitiveValueMappings.h"
#include "CSSValueKeywords.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// MARK: Canonicalization

CanonicalAngleRaw canonicalize(const AngleRaw& value)
{
    return { CSSPrimitiveValue::computeDegrees(value.type, value.value) };
}

CanonicalLengthRaw canonicalize(const LengthRaw& value, const CSSToLengthConversionData& conversionData)
{
    // FIXME: Extract computeLength logic into a free function to allow use without CSSPrimitiveValue construction.
    Ref temp = CSSPrimitiveValue::create(value.value, value.type);
    return { temp->computeLength<double>(conversionData) };
}

CanonicalResolutionRaw canonicalize(const ResolutionRaw& value)
{
    return { CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(value.type).value() * value.value };
}

CanonicalTimeRaw canonicalize(const TimeRaw& value)
{
    return { CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(value.type).value() * value.value };
}

// MARK: Serialization

template<typename RawValueType> static void serializationRawValueForCSS(StringBuilder& builder, const RawValueType& value)
{
    formatCSSNumberValue(builder, value.value, CSSPrimitiveValue::unitTypeString(value.type));
}

void serializationIntegerForCSS(StringBuilder& builder, int value)
{
    formatCSSNumberValue(builder, static_cast<double>(value), CSSPrimitiveValue::unitTypeString(CSSUnitType::CSS_INTEGER));
}

void serializationIntegerForCSS(StringBuilder& builder, unsigned value)
{
    formatCSSNumberValue(builder, static_cast<double>(value), CSSPrimitiveValue::unitTypeString(CSSUnitType::CSS_INTEGER));
}

void serializationForCSS(StringBuilder& builder, const AngleRaw& value)
{
    serializationRawValueForCSS(builder, value);
}

void serializationForCSS(StringBuilder& builder, const CanonicalAngleRaw& value)
{
    serializationRawValueForCSS(builder, value);
}

void serializationForCSS(StringBuilder& builder, const NumberRaw& value)
{
    serializationRawValueForCSS(builder, value);
}

void serializationForCSS(StringBuilder& builder, const PercentRaw& value)
{
    serializationRawValueForCSS(builder, value);
}

void serializationForCSS(StringBuilder& builder, const LengthRaw& value)
{
    serializationRawValueForCSS(builder, value);
}

void serializationForCSS(StringBuilder& builder, const CanonicalLengthRaw& value)
{
    serializationRawValueForCSS(builder, value);
}

void serializationForCSS(StringBuilder& builder, const TimeRaw& value)
{
    serializationRawValueForCSS(builder, value);
}

void serializationForCSS(StringBuilder& builder, const CanonicalTimeRaw& value)
{
    serializationRawValueForCSS(builder, value);
}

void serializationForCSS(StringBuilder& builder, const ResolutionRaw& value)
{
    serializationRawValueForCSS(builder, value);
}

void serializationForCSS(StringBuilder& builder, const CanonicalResolutionRaw& value)
{
    serializationRawValueForCSS(builder, value);
}

void serializationForCSS(StringBuilder& builder, const SymbolRaw& value)
{
    builder.append(nameLiteralForSerialization(value.value));
}

void serializationForCSS(StringBuilder& builder, const NoneRaw&)
{
    builder.append("none"_s);
}

// MARK: CSSPrimitiveValue construction

template<typename RawValueType> static Ref<CSSPrimitiveValue> createCSSPrimitiveValueFromRawValue(const RawValueType& value)
{
    return CSSPrimitiveValue::create(value.value, value.type);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValueForInteger(int value)
{
    return CSSPrimitiveValue::createInteger(static_cast<double>(value));
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValueForInteger(unsigned value)
{
    return CSSPrimitiveValue::createInteger(static_cast<double>(value));
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const AngleRaw& value)
{
    return createCSSPrimitiveValueFromRawValue(value);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const CanonicalAngleRaw& value)
{
    return createCSSPrimitiveValueFromRawValue(value);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const NumberRaw& value)
{
    return createCSSPrimitiveValueFromRawValue(value);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const PercentRaw& value)
{
    return createCSSPrimitiveValueFromRawValue(value);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const LengthRaw& value)
{
    return createCSSPrimitiveValueFromRawValue(value);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const CanonicalLengthRaw& value)
{
    return createCSSPrimitiveValueFromRawValue(value);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const TimeRaw& value)
{
    return createCSSPrimitiveValueFromRawValue(value);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const CanonicalTimeRaw& value)
{
    return createCSSPrimitiveValueFromRawValue(value);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const ResolutionRaw& value)
{
    return createCSSPrimitiveValueFromRawValue(value);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const CanonicalResolutionRaw& value)
{
    return createCSSPrimitiveValueFromRawValue(value);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const SymbolRaw& value)
{
    return CSSPrimitiveValue::create(value.value);
}

Ref<CSSPrimitiveValue> createCSSPrimitiveValue(const NoneRaw&)
{
    return CSSPrimitiveValue::create(CSSValueNone);
}

// MARK: SymbolRaw lookup

NumberRaw replaceSymbol(SymbolRaw raw, const CSSCalcSymbolTable& symbolTable)
{
    auto result = symbolTable.get(raw.value);

    // We should only get here if the symbol was previously looked up in the symbol table.
    ASSERT(result);
    return { result->value };
}

} // namespace WebCore
