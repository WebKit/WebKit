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

#include "CSSParserToken.h"
#include "CSSPrimitiveValue.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSUnitValue);

CSSUnitType CSSUnitValue::parseUnit(const String& unit)
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
    auto type = CSSNumericType::create(parsedUnit);
    if (!type)
        return Exception { TypeError };
    auto unitValue = adoptRef(*new CSSUnitValue(value, parsedUnit));
    unitValue->m_type = WTFMove(*type);
    return unitValue;
}

CSSUnitValue::CSSUnitValue(double value, CSSUnitType unit)
    : CSSNumericValue(CSSNumericType::create(unit).value_or(CSSNumericType()))
    , m_value(value)
    , m_unit(unit)
{
}

static double conversionToCanonicalUnitsScaleFactor(CSSUnitType unit)
{
    constexpr double pixelsPerInch = 96;
    constexpr double pixelsPerCentimeter = pixelsPerInch / 2.54;
    constexpr double pointsPerInch = 72;
    constexpr double picasPerInch = 6;

    switch (unit) {
    case CSSUnitType::CSS_MS:
        return 0.001;
    case CSSUnitType::CSS_CM:
        return pixelsPerCentimeter;
    case CSSUnitType::CSS_DPCM:
        return 1.0 / pixelsPerCentimeter;
    case CSSUnitType::CSS_MM:
        return pixelsPerCentimeter / 10;
    case CSSUnitType::CSS_Q:
        return pixelsPerCentimeter / 40;
    case CSSUnitType::CSS_IN:
        return pixelsPerInch;
    case CSSUnitType::CSS_DPI:
        return 1.0 / pixelsPerInch;
    case CSSUnitType::CSS_PT:
        return pixelsPerInch / pointsPerInch;
    case CSSUnitType::CSS_PC:
        return pixelsPerInch / picasPerInch;
    case CSSUnitType::CSS_RAD:
        return 180 / piDouble;
    case CSSUnitType::CSS_GRAD:
        return 0.9;
    case CSSUnitType::CSS_TURN:
        return 360;
    case CSSUnitType::CSS_KHZ:
        return 1000;
    default:
        return 1.0;
    }
}

RefPtr<CSSUnitValue> CSSUnitValue::convertTo(CSSUnitType unit) const
{
    // https://drafts.css-houdini.org/css-typed-om/#convert-a-cssunitvalue
    if (unitCategory(unitEnum()) != unitCategory(unit))
        return nullptr;

    return create(m_value * conversionToCanonicalUnitsScaleFactor(unitEnum()) / conversionToCanonicalUnitsScaleFactor(unit), unit);
}

auto CSSUnitValue::toSumValue() const -> std::optional<SumValue>
{
    // https://drafts.css-houdini.org/css-typed-om/#create-a-sum-value
    auto canonicalUnit = [] (CSSUnitType unit) {
        // FIXME: We probably want to change the definition of canonicalUnitTypeForCategory so this lambda isn't necessary.
        auto category = unitCategory(unit);
        switch (category) {
        case CSSUnitCategory::Percent:
            return CSSUnitType::CSS_PERCENTAGE;
        case CSSUnitCategory::Flex:
            return CSSUnitType::CSS_FR;
        default:
            break;
        }
        auto result = canonicalUnitTypeForCategory(category);
        return result == CSSUnitType::CSS_UNKNOWN ? unit : result;
    } (m_unit);
    auto convertedValue = m_value * conversionToCanonicalUnitsScaleFactor(unitEnum()) / conversionToCanonicalUnitsScaleFactor(canonicalUnit);

    if (m_unit == CSSUnitType::CSS_NUMBER)
        return { { { convertedValue, { } } } };
    return { { { convertedValue, { { canonicalUnit, 1 } } } } };
}

bool CSSUnitValue::equals(const CSSNumericValue& other) const
{
    // https://drafts.css-houdini.org/css-typed-om/#equal-numeric-value
    auto* otherUnitValue = dynamicDowncast<CSSUnitValue>(other);
    if (!otherUnitValue)
        return false;
    return m_value == otherUnitValue->m_value && m_unit == otherUnitValue->m_unit;
}

} // namespace WebCore
