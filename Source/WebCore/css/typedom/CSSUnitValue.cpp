/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSUnitValue.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSParserToken.h"
#include "CSSPrimitiveValue.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSUnitValue);

static std::optional<CSSNumericType> numericType(CSSUnitType unit)
{
    // https://drafts.css-houdini.org/css-typed-om/#cssnumericvalue-create-a-type
    CSSNumericType type;
    switch (unitCategory(unit)) {
    case CSSUnitCategory::Number:
        return { WTFMove(type) };
    case CSSUnitCategory::Percent:
        type.percent = 1;
        return { WTFMove(type) };
    case CSSUnitCategory::Length:
        type.length = 1;
        return { WTFMove(type) };
    case CSSUnitCategory::Angle:
        type.angle = 1;
        return { WTFMove(type) };
    case CSSUnitCategory::Time:
        type.time = 1;
        return { WTFMove(type) };
    case CSSUnitCategory::Frequency:
        type.frequency = 1;
        return { WTFMove(type) };
    case CSSUnitCategory::Resolution:
        type.resolution = 1;
        return { WTFMove(type) };
    case CSSUnitCategory::Other:
        if (unit == CSSUnitType::CSS_FR) {
            type.flex = 1;
            return { WTFMove(type) };
        }
        break;
    }
    
    return std::nullopt;
}

static CSSUnitType parseUnit(const String& unit)
{
    if (unit == "number"_s)
        return CSSUnitType::CSS_NUMBER;
    if (unit == "percent"_s)
        return CSSUnitType::CSS_PERCENTAGE;

    // FIXME: Remove these when LineHeightUnitsEnabled is changed back to true or removed
    // https://bugs.webkit.org/show_bug.cgi?id=211351
    if (unit == "lh"_s)
        return CSSUnitType::CSS_LHS;
    if (unit == "rlh"_s)
        return CSSUnitType::CSS_RLHS;

    return CSSParserToken::stringToUnitType(unit);
}

ASCIILiteral CSSUnitValue::unit() const
{
    switch (m_unit) {
    case CSSUnitType::CSS_NUMBER:
        return "number"_s;
    case CSSUnitType::CSS_PERCENTAGE:
        return "percent"_s;
    default:
        break;
    }
    return unitSerialization();
}

ASCIILiteral CSSUnitValue::unitSerialization() const
{
    return CSSPrimitiveValue::unitTypeString(m_unit);
}

void CSSUnitValue::serialize(StringBuilder& builder, OptionSet<SerializationArguments>) const
{
    builder.append(FormattedCSSNumber::create(m_value));
    builder.append(unitSerialization());
}

ExceptionOr<Ref<CSSUnitValue>> CSSUnitValue::create(double value, const String& unit)
{
    auto parsedUnit = parseUnit(unit);
    if (parsedUnit == CSSUnitType::CSS_UNKNOWN)
        return Exception { TypeError };
    auto type = numericType(parsedUnit);
    if (!type)
        return Exception { TypeError };
    auto unitValue = adoptRef(*new CSSUnitValue(value, parsedUnit));
    unitValue->m_type = WTFMove(*type);
    return unitValue;
}

CSSUnitValue::CSSUnitValue(double value, CSSUnitType unit)
    : CSSNumericValue(numericType(unit).value_or(CSSNumericType()))
    , m_value(value)
    , m_unit(unit)
{
}

} // namespace WebCore

#endif
