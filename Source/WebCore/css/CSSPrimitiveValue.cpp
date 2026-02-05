/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSPrimitiveValue.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSMarkup.h"
#include "CSSParserIdioms.h"
#include "CSSPrimitiveNumericCategory.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSSerializationContext.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "ComputedStyleDependencies.h"
#include "ContainerQueryEvaluator.h"
#include "FontCascade.h"
#include "NodeRenderStyle.h"
#include "RenderBoxInlines.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "StyleCalculationValue.h"
#include "StyleLengthResolution.h"
#include "StylePrimitiveNumericTypes+Rounding.h"
#include <wtf/Hasher.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static inline bool isValidCSSUnitTypeForDoubleConversion(CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        return true;
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_VALUE_ID:
        return false;
    case CSSUnitType::CSS_IDENT:
        break;
    }

    ASSERT_NOT_REACHED();
    return false;
}

#if ASSERT_ENABLED

static inline bool isStringType(CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_FONT_FAMILY:
        return true;
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_VALUE_ID:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

#endif // ASSERT_ENABLED

static HashMap<const CSSPrimitiveValue*, String>& serializedPrimitiveValues()
{
    static NeverDestroyed<HashMap<const CSSPrimitiveValue*, String>> map;
    return map;
}

CSSUnitType CSSPrimitiveValue::primitiveType() const
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_VALUE_ID:
    case CSSUnitType::CustomIdent:
        return CSSUnitType::CSS_IDENT;
    case CSSUnitType::CSS_FONT_FAMILY:
        // Web-exposed content expects font family values to have CSSUnitType::CSS_STRING primitive type
        // so we need to map our internal CSSUnitType::CSS_FONT_FAMILY type here.
        return CSSUnitType::CSS_STRING;
    default:
        if (RefPtr calcValue = cssCalcValue())
            return calcValue->primitiveType();

        return type;
    }
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSPropertyID propertyID)
    : CSSValue(ClassType::Primitive)
{
    setPrimitiveUnitType(CSSUnitType::CSS_PROPERTY_ID);
    m_value.propertyID = propertyID;
}

CSSPrimitiveValue::CSSPrimitiveValue(double number, CSSUnitType type)
    : CSSValue(ClassType::Primitive)
{
    setPrimitiveUnitType(type);
    m_value.number = number;
}

CSSPrimitiveValue::CSSPrimitiveValue(const String& string, CSSUnitType type)
    : CSSValue(ClassType::Primitive)
{
    ASSERT(isStringType(type));
    setPrimitiveUnitType(type);
    if ((m_value.string = string.impl()))
        m_value.string->ref();
}

CSSPrimitiveValue::CSSPrimitiveValue(StaticCSSValueTag, CSSValueID valueID)
    : CSSValue(ClassType::Primitive)
{
    setPrimitiveUnitType(CSSUnitType::CSS_VALUE_ID);
    m_value.valueID = valueID;
    makeStatic();
}

CSSPrimitiveValue::CSSPrimitiveValue(StaticCSSValueTag, double number, CSSUnitType type)
    : CSSPrimitiveValue(number, type)
{
    makeStatic();
}

CSSPrimitiveValue::CSSPrimitiveValue(StaticCSSValueTag, ImplicitInitialValueTag)
    : CSSPrimitiveValue(StaticCSSValue, CSSValueInitial)
{
    m_isImplicitInitialValue = true;
}

CSSPrimitiveValue::CSSPrimitiveValue(Ref<CSSCalc::Value> value)
    : CSSValue(ClassType::Primitive)
{
    setPrimitiveUnitType(CSSUnitType::CSS_CALC);
    m_value.calc = &value.leakRef();
}

CSSPrimitiveValue::CSSPrimitiveValue(Ref<CSSAttrValue> value)
    : CSSValue(ClassType::Primitive)
{
    setPrimitiveUnitType(CSSUnitType::CSS_ATTR);
    m_value.attr = &value.leakRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_FONT_FAMILY:
        if (m_value.string)
            m_value.string->deref();
        break;
    case CSSUnitType::CSS_ATTR:
        m_value.attr->deref();
        break;
    case CSSUnitType::CSS_CALC:
        m_value.calc->deref();
        break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        ASSERT_NOT_REACHED();
        break;
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_VALUE_ID:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        ASSERT(!isStringType(type));
        break;
    }
    if (m_hasCachedCSSText) {
        ASSERT(serializedPrimitiveValues().contains(this));
        serializedPrimitiveValues().remove(this);
    }
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(CSSPropertyID propertyID)
{
    return adoptRef(*new CSSPrimitiveValue(propertyID));
}

static CSSPrimitiveValue* valueFromPool(std::span<AlignedStorage<CSSPrimitiveValue>> pool, double value)
{
    // Casting to a signed integer first since casting a negative floating point value to an unsigned
    // integer is undefined behavior.
    unsigned poolIndex = static_cast<unsigned>(static_cast<int>(value));
    double roundTripValue = poolIndex;
    if (equalSpans(asByteSpan(value), asByteSpan(roundTripValue)) && poolIndex < pool.size())
        return pool[poolIndex].get();
    return nullptr;
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(double value)
{
    if (RefPtr result = valueFromPool(staticCSSValuePool->m_numberValues, value))
        return result.releaseNonNull();
    return adoptRef(*new CSSPrimitiveValue(value, CSSUnitType::CSS_NUMBER));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(double value, CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::CSS_NUMBER:
        if (RefPtr result = valueFromPool(staticCSSValuePool->m_numberValues, value))
            return result.releaseNonNull();
        break;
    case CSSUnitType::CSS_PERCENTAGE:
        if (RefPtr result = valueFromPool(staticCSSValuePool->m_percentageValues, value))
            return result.releaseNonNull();
        break;
    case CSSUnitType::CSS_PX:
        if (RefPtr result = valueFromPool(staticCSSValuePool->m_pixelValues, value))
            return result.releaseNonNull();
        break;
    default:
        break;
    }
    return adoptRef(*new CSSPrimitiveValue(value, type));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(String value)
{
    return adoptRef(*new CSSPrimitiveValue(WTF::move(value), CSSUnitType::CSS_STRING));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<CSSCalc::Value> value)
{
    return adoptRef(*new CSSPrimitiveValue(WTF::move(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<CSSAttrValue> value)
{
    return adoptRef(*new CSSPrimitiveValue(WTF::move(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createCustomIdent(String value)
{
    return adoptRef(*new CSSPrimitiveValue(WTF::move(value), CSSUnitType::CustomIdent));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createFontFamily(String value)
{
    return adoptRef(*new CSSPrimitiveValue(WTF::move(value), CSSUnitType::CSS_FONT_FAMILY));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createInteger(double value)
{
    return adoptRef(*new CSSPrimitiveValue(value, CSSUnitType::CSS_INTEGER));
}

bool CSSPrimitiveValue::conversionToCanonicalUnitRequiresConversionData() const
{
    if (isCalculated())
        return m_value.calc->requiresConversionData();
    return WebCore::conversionToCanonicalUnitRequiresConversionData(primitiveType());
}

template<> int CSSPrimitiveValue::resolveAsLength(const CSSToLengthConversionData& conversionData) const
{
    return Style::roundForImpreciseConversion<int>(resolveAsLengthDouble(conversionData));
}

template<> unsigned CSSPrimitiveValue::resolveAsLength(const CSSToLengthConversionData& conversionData) const
{
    return Style::roundForImpreciseConversion<unsigned>(resolveAsLengthDouble(conversionData));
}

template<> float CSSPrimitiveValue::resolveAsLength(const CSSToLengthConversionData& conversionData) const
{
    return narrowPrecisionToFloat(resolveAsLengthDouble(conversionData));
}

template<> double CSSPrimitiveValue::resolveAsLength(const CSSToLengthConversionData& conversionData) const
{
    return resolveAsLengthDouble(conversionData);
}

template<> short CSSPrimitiveValue::resolveAsLength(const CSSToLengthConversionData& conversionData) const
{
    return Style::roundForImpreciseConversion<short>(resolveAsLengthDouble(conversionData));
}

template<> unsigned short CSSPrimitiveValue::resolveAsLength(const CSSToLengthConversionData& conversionData) const
{
    return Style::roundForImpreciseConversion<unsigned short>(resolveAsLengthDouble(conversionData));
}

template<> LayoutUnit CSSPrimitiveValue::resolveAsLength(const CSSToLengthConversionData& conversionData) const
{
    return LayoutUnit(resolveAsLengthDouble(conversionData));
}

double CSSPrimitiveValue::resolveAsLengthDouble(const CSSToLengthConversionData& conversionData) const
{
    if (RefPtr calcValue = cssCalcValue()) {
        // The multiplier and factor is applied to each value in the calc expression individually
        return calcValue->computeLengthPx(conversionData, CSSCalcSymbolTable { });
    }

    auto lengthUnit = CSS::toLengthUnit(primitiveType());
    if (!lengthUnit) {
        ASSERT_NOT_REACHED();
        return -1.0;
    }
    return Style::computeNonCalcLengthDouble(m_value.number, *lengthUnit, conversionData);
}

std::optional<double> CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(CSSUnitType unitType)
{
    // FIXME: the switch can be replaced by an array of scale factors.
    switch (unitType) {
    // These are "canonical" units in their respective categories.
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_DPPX:
        return 1.0;

    case CSSUnitType::CSS_X:
        return CSS::dppxPerX;
    case CSSUnitType::CSS_CM:
        return CSS::pixelsPerCm;
    case CSSUnitType::CSS_DPCM:
        return CSS::dppxPerDpcm;
    case CSSUnitType::CSS_MM:
        return CSS::pixelsPerMm;
    case CSSUnitType::CSS_Q:
        return CSS::pixelsPerQ;
    case CSSUnitType::CSS_IN:
        return CSS::pixelsPerInch;
    case CSSUnitType::CSS_DPI:
        return CSS::dppxPerDpi;
    case CSSUnitType::CSS_PT:
        return CSS::pixelsPerPt;
    case CSSUnitType::CSS_PC:
        return CSS::pixelsPerPc;
    case CSSUnitType::CSS_RAD:
        return degreesPerRadianDouble;
    case CSSUnitType::CSS_GRAD:
        return degreesPerGradientDouble;
    case CSSUnitType::CSS_TURN:
        return degreesPerTurnDouble;
    case CSSUnitType::CSS_MS:
        return CSS::secondsPerMillisecond;
    case CSSUnitType::CSS_KHZ:
        return CSS::hertzPerKilohertz;

    default:
        return std::nullopt;
    }
}

ExceptionOr<float> CSSPrimitiveValue::getFloatValueDeprecated(CSSUnitType targetUnit) const
{
    auto result = doubleValueInternalDeprecated(targetUnit);
    if (!result)
        return Exception { ExceptionCode::InvalidAccessError };
    return clampTo<float>(result.value());
}

// MARK: Arbitrarily converting

double CSSPrimitiveValue::doubleValue(CSSUnitType targetUnit, const CSSToLengthConversionData& conversionData) const
{
    return doubleValueInternal(targetUnit, conversionData).value_or(0);
}

double CSSPrimitiveValue::doubleValueNoConversionDataRequired(CSSUnitType targetUnit) const
{
    ASSERT(!isCalculated());
    return doubleValueInternalDeprecated(targetUnit).value_or(0);
}

double CSSPrimitiveValue::doubleValueDeprecated(CSSUnitType targetUnit) const
{
    return doubleValueInternalDeprecated(targetUnit).value_or(0);
}

// MARK: Non-converting

double CSSPrimitiveValue::doubleValue(const CSSToLengthConversionData& conversionData) const
{
    if (RefPtr calcValue = cssCalcValue())
        return calcValue->doubleValue(conversionData, { });
    return m_value.number;
}

double CSSPrimitiveValue::doubleValueDeprecated() const
{
    if (RefPtr calcValue = cssCalcValue())
        return calcValue->doubleValueDeprecated();
    return m_value.number;
}

// MARK: `doubleValueDividingBy100IfPercentage`.

double CSSPrimitiveValue::doubleValueDividingBy100IfPercentage(const CSSToLengthConversionData& conversionData) const
{
    ASSERT(isNumberOrInteger() || isPercentage());

    if (RefPtr calcValue = cssCalcValue())
        return calcValue->primitiveType() == CSSUnitType::CSS_PERCENTAGE ? calcValue->doubleValue(conversionData, { }) / 100.0 : calcValue->doubleValue(conversionData, { });
    if (isPercentage())
        return m_value.number / 100.0;
    return m_value.number;
}

double CSSPrimitiveValue::doubleValueDividingBy100IfPercentageNoConversionDataRequired() const
{
    ASSERT(isNumberOrInteger() || isPercentage());
    ASSERT(!isCalculated());

    if (isPercentage())
        return m_value.number / 100.0;
    return m_value.number;
}

double CSSPrimitiveValue::doubleValueDividingBy100IfPercentageDeprecated() const
{
    ASSERT(isNumberOrInteger() || isPercentage());

    if (RefPtr calcValue = cssCalcValue())
        return calcValue->primitiveType() == CSSUnitType::CSS_PERCENTAGE ? calcValue->doubleValueDeprecated() / 100.0 : calcValue->doubleValueDeprecated();
    if (isPercentage())
        return m_value.number / 100.0;
    return m_value.number;
}

std::optional<bool> CSSPrimitiveValue::isZero() const
{
    if (isCalculated())
        return std::nullopt;
    return !m_value.number;
}

std::optional<bool> CSSPrimitiveValue::isOne() const
{
    if (isCalculated())
        return std::nullopt;
    return m_value.number == 1;
}

std::optional<bool> CSSPrimitiveValue::isPositive() const
{
    if (isCalculated())
        return std::nullopt;
    return m_value.number > 0;
}

std::optional<bool> CSSPrimitiveValue::isNegative() const
{
    if (isCalculated())
        return std::nullopt;
    return m_value.number < 0;
}

std::optional<double> CSSPrimitiveValue::doubleValueInternal(CSSUnitType requestedUnitType, const CSSToLengthConversionData& conversionData) const
{
    if (!isValidCSSUnitTypeForDoubleConversion(primitiveUnitType()) || !isValidCSSUnitTypeForDoubleConversion(requestedUnitType))
        return std::nullopt;

    CSSUnitType sourceUnitType = primitiveType();
    if (requestedUnitType == sourceUnitType || requestedUnitType == CSSUnitType::CSS_DIMENSION)
        return doubleValue(conversionData);

    CSSUnitCategory sourceCategory = unitCategory(sourceUnitType);
    ASSERT(sourceCategory != CSSUnitCategory::Other);

    CSSUnitType targetUnitType = requestedUnitType;
    CSSUnitCategory targetCategory = unitCategory(targetUnitType);
    ASSERT(targetCategory != CSSUnitCategory::Other);

    // Cannot convert between unrelated unit categories if one of them is not CSSUnitCategory::Number.
    if (sourceCategory != targetCategory && sourceCategory != CSSUnitCategory::Number && targetCategory != CSSUnitCategory::Number)
        return std::nullopt;

    if (targetCategory == CSSUnitCategory::Number) {
        // Cannot convert between numbers and percent.
        if (sourceCategory == CSSUnitCategory::Percent)
            return std::nullopt;
        // We interpret conversion to CSSUnitType::CSS_NUMBER as conversion to a canonical unit in this value's category.
        targetUnitType = canonicalUnitTypeForCategory(sourceCategory);
        if (targetUnitType == CSSUnitType::CSS_UNKNOWN)
            return std::nullopt;
    }

    if (sourceUnitType == CSSUnitType::CSS_NUMBER || sourceUnitType == CSSUnitType::CSS_INTEGER) {
        // Cannot convert between numbers and percent.
        if (targetCategory == CSSUnitCategory::Percent)
            return std::nullopt;
        // We interpret conversion from CSSUnitType::CSS_NUMBER in the same way as CSSParser::validUnit() while using non-strict mode.
        sourceUnitType = canonicalUnitTypeForCategory(targetCategory);
        if (sourceUnitType == CSSUnitType::CSS_UNKNOWN)
            return std::nullopt;
    }

    double convertedValue = doubleValue(conversionData);

    // If we don't need to scale it, don't worry about if we can scale it.
    if (sourceUnitType == targetUnitType)
        return convertedValue;

    // First convert the value from primitiveUnitType() to canonical type.
    auto sourceFactor = conversionToCanonicalUnitsScaleFactor(sourceUnitType);
    if (!sourceFactor.has_value())
        return std::nullopt;
    convertedValue *= sourceFactor.value();

    // Now convert from canonical type to the target unitType.
    auto targetFactor = conversionToCanonicalUnitsScaleFactor(targetUnitType);
    if (!targetFactor.has_value())
        return std::nullopt;
    convertedValue /= targetFactor.value();

    return convertedValue;
}

std::optional<double> CSSPrimitiveValue::doubleValueInternalDeprecated(CSSUnitType requestedUnitType) const
{
    if (!isValidCSSUnitTypeForDoubleConversion(primitiveUnitType()) || !isValidCSSUnitTypeForDoubleConversion(requestedUnitType))
        return std::nullopt;

    CSSUnitType sourceUnitType = primitiveType();
    if (requestedUnitType == sourceUnitType || requestedUnitType == CSSUnitType::CSS_DIMENSION)
        return doubleValueDeprecated();

    CSSUnitCategory sourceCategory = unitCategory(sourceUnitType);
    ASSERT(sourceCategory != CSSUnitCategory::Other);

    CSSUnitType targetUnitType = requestedUnitType;
    CSSUnitCategory targetCategory = unitCategory(targetUnitType);
    ASSERT(targetCategory != CSSUnitCategory::Other);

    // Cannot convert between unrelated unit categories if one of them is not CSSUnitCategory::Number.
    if (sourceCategory != targetCategory && sourceCategory != CSSUnitCategory::Number && targetCategory != CSSUnitCategory::Number)
        return std::nullopt;

    if (targetCategory == CSSUnitCategory::Number) {
        // Cannot convert between numbers and percent.
        if (sourceCategory == CSSUnitCategory::Percent)
            return std::nullopt;
        // We interpret conversion to CSSUnitType::CSS_NUMBER as conversion to a canonical unit in this value's category.
        targetUnitType = canonicalUnitTypeForCategory(sourceCategory);
        if (targetUnitType == CSSUnitType::CSS_UNKNOWN)
            return std::nullopt;
    }

    if (sourceUnitType == CSSUnitType::CSS_NUMBER || sourceUnitType == CSSUnitType::CSS_INTEGER) {
        // Cannot convert between numbers and percent.
        if (targetCategory == CSSUnitCategory::Percent)
            return std::nullopt;
        // We interpret conversion from CSSUnitType::CSS_NUMBER in the same way as CSSParser::validUnit() while using non-strict mode.
        sourceUnitType = canonicalUnitTypeForCategory(targetCategory);
        if (sourceUnitType == CSSUnitType::CSS_UNKNOWN)
            return std::nullopt;
    }

    double convertedValue = doubleValueDeprecated();

    // If we don't need to scale it, don't worry about if we can scale it.
    if (sourceUnitType == targetUnitType)
        return convertedValue;

    // First convert the value from primitiveUnitType() to canonical type.
    auto sourceFactor = conversionToCanonicalUnitsScaleFactor(sourceUnitType);
    if (!sourceFactor.has_value())
        return std::nullopt;
    convertedValue *= sourceFactor.value();

    // Now convert from canonical type to the target unitType.
    auto targetFactor = conversionToCanonicalUnitsScaleFactor(targetUnitType);
    if (!targetFactor.has_value())
        return std::nullopt;
    convertedValue /= targetFactor.value();

    return convertedValue;
}

String CSSPrimitiveValue::stringValue() const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_FONT_FAMILY:
        return m_value.string;
    case CSSUnitType::CSS_VALUE_ID:
        return nameString(m_value.valueID);
    case CSSUnitType::CSS_PROPERTY_ID:
        return nameString(m_value.propertyID);
    case CSSUnitType::CSS_ATTR:
        return protectedCssAttrValue()->cssText(CSS::defaultSerializationContext());
    default:
        return String();
    }
}

NEVER_INLINE String CSSPrimitiveValue::formatNumberValue(ASCIILiteral suffix) const
{
    return CSS::formatCSSNumberValue(CSS::SerializableNumber { m_value.number, suffix });
}

NEVER_INLINE String CSSPrimitiveValue::formatIntegerValue(ASCIILiteral suffix) const
{
    if (!std::isfinite(m_value.number))
        return CSS::formatNonfiniteCSSNumberValue(CSS::SerializableNumber { m_value.number, suffix });
    return makeString(m_value.number, suffix);
}

ASCIILiteral CSSPrimitiveValue::unitTypeString(CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::CSS_CAP: return "cap"_s;
    case CSSUnitType::CSS_CH: return "ch"_s;
    case CSSUnitType::CSS_CM: return "cm"_s;
    case CSSUnitType::CSS_CQB: return "cqb"_s;
    case CSSUnitType::CSS_CQH: return "cqh"_s;
    case CSSUnitType::CSS_CQI: return "cqi"_s;
    case CSSUnitType::CSS_CQMAX: return "cqmax"_s;
    case CSSUnitType::CSS_CQMIN: return "cqmin"_s;
    case CSSUnitType::CSS_CQW: return "cqw"_s;
    case CSSUnitType::CSS_DEG: return "deg"_s;
    case CSSUnitType::CSS_DPCM: return "dpcm"_s;
    case CSSUnitType::CSS_DPI: return "dpi"_s;
    case CSSUnitType::CSS_DPPX: return "dppx"_s;
    case CSSUnitType::CSS_DVB: return "dvb"_s;
    case CSSUnitType::CSS_DVH: return "dvh"_s;
    case CSSUnitType::CSS_DVI: return "dvi"_s;
    case CSSUnitType::CSS_DVMAX: return "dvmax"_s;
    case CSSUnitType::CSS_DVMIN: return "dvmin"_s;
    case CSSUnitType::CSS_DVW: return "dvw"_s;
    case CSSUnitType::CSS_EM: return "em"_s;
    case CSSUnitType::CSS_EX: return "ex"_s;
    case CSSUnitType::CSS_FR: return "fr"_s;
    case CSSUnitType::CSS_GRAD: return "grad"_s;
    case CSSUnitType::CSS_HZ: return "hz"_s;
    case CSSUnitType::CSS_IC: return "ic"_s;
    case CSSUnitType::CSS_IN: return "in"_s;
    case CSSUnitType::CSS_KHZ: return "khz"_s;
    case CSSUnitType::CSS_LH: return "lh"_s;
    case CSSUnitType::CSS_LVB: return "lvb"_s;
    case CSSUnitType::CSS_LVH: return "lvh"_s;
    case CSSUnitType::CSS_LVI: return "lvi"_s;
    case CSSUnitType::CSS_LVMAX: return "lvmax"_s;
    case CSSUnitType::CSS_LVMIN: return "lvmin"_s;
    case CSSUnitType::CSS_LVW: return "lvw"_s;
    case CSSUnitType::CSS_MM: return "mm"_s;
    case CSSUnitType::CSS_MS: return "ms"_s;
    case CSSUnitType::CSS_PC: return "pc"_s;
    case CSSUnitType::CSS_PERCENTAGE: return "%"_s;
    case CSSUnitType::CSS_PT: return "pt"_s;
    case CSSUnitType::CSS_PX: return "px"_s;
    case CSSUnitType::CSS_Q: return "q"_s;
    case CSSUnitType::CSS_RAD: return "rad"_s;
    case CSSUnitType::CSS_RCAP: return "rcap"_s;
    case CSSUnitType::CSS_RCH: return "rch"_s;
    case CSSUnitType::CSS_REM: return "rem"_s;
    case CSSUnitType::CSS_REX: return "rex"_s;
    case CSSUnitType::CSS_RIC: return "ric"_s;
    case CSSUnitType::CSS_RLH: return "rlh"_s;
    case CSSUnitType::CSS_S: return "s"_s;
    case CSSUnitType::CSS_SVB: return "svb"_s;
    case CSSUnitType::CSS_SVH: return "svh"_s;
    case CSSUnitType::CSS_SVI: return "svi"_s;
    case CSSUnitType::CSS_SVMAX: return "svmax"_s;
    case CSSUnitType::CSS_SVMIN: return "svmin"_s;
    case CSSUnitType::CSS_SVW: return "svw"_s;
    case CSSUnitType::CSS_TURN: return "turn"_s;
    case CSSUnitType::CSS_VB: return "vb"_s;
    case CSSUnitType::CSS_VH: return "vh"_s;
    case CSSUnitType::CSS_VI: return "vi"_s;
    case CSSUnitType::CSS_VMAX: return "vmax"_s;
    case CSSUnitType::CSS_VMIN: return "vmin"_s;
    case CSSUnitType::CSS_VW: return "vw"_s;
    case CSSUnitType::CSS_X: return "x"_s;

    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_VALUE_ID:
    case CSSUnitType::CustomIdent:
        return ""_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

ALWAYS_INLINE String CSSPrimitiveValue::serializeInternal(const CSS::SerializationContext& context) const
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQMAX:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_X:
        return formatNumberValue(unitTypeString(type));
    case CSSUnitType::CSS_ATTR:
        return protectedCssAttrValue()->cssText(context);
    case CSSUnitType::CSS_CALC:
        return protectedCssCalcValue()->cssText(context);
    case CSSUnitType::CSS_DIMENSION:
        // FIXME: This isn't correct.
        return formatNumberValue(""_s);
    case CSSUnitType::CSS_FONT_FAMILY:
        return serializeFontFamily(m_value.string);
    case CSSUnitType::CSS_INTEGER:
        return formatIntegerValue(""_s);
    case CSSUnitType::CSS_QUIRKY_EM:
        return formatNumberValue("em"_s);
    case CSSUnitType::CSS_STRING:
        return serializeString(m_value.string);
    case CSSUnitType::CustomIdent: {
        StringBuilder builder;
        serializeIdentifier(m_value.string, builder);
        return builder.toString();
    }

    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_VALUE_ID:
        break;
    }
    ASSERT_NOT_REACHED();
    return String();
}

String CSSPrimitiveValue::customCSSText(const CSS::SerializationContext& context) const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_UNKNOWN:
        return String();
    case CSSUnitType::CSS_VALUE_ID:
        return nameStringForSerialization(m_value.valueID);
    case CSSUnitType::CSS_PROPERTY_ID:
        return nameString(m_value.propertyID);
    default:
        auto& map = serializedPrimitiveValues();
        ASSERT(map.contains(this) == m_hasCachedCSSText);
        if (m_hasCachedCSSText)
            return map.get(this);
        String serializedValue = serializeInternal(context);
        m_hasCachedCSSText = true;
        map.add(this, serializedValue);
        return serializedValue;
    }
}

bool CSSPrimitiveValue::equals(const CSSPrimitiveValue& other) const
{
    if (primitiveUnitType() != other.primitiveUnitType())
        return false;

    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_UNKNOWN:
        return false;
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        return m_value.number == other.m_value.number;
    case CSSUnitType::CSS_PROPERTY_ID:
        return m_value.propertyID == other.m_value.propertyID;
    case CSSUnitType::CSS_VALUE_ID:
        return m_value.valueID == other.m_value.valueID;
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_FONT_FAMILY:
        return equal(m_value.string, other.m_value.string);
    case CSSUnitType::CSS_ATTR:
        return protectedCssAttrValue()->equals(*other.protectedCssAttrValue());
    case CSSUnitType::CSS_CALC:
        return protectedCssCalcValue()->equals(*other.protectedCssCalcValue());
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        // FIXME: seems like these should be handled.
        ASSERT_NOT_REACHED();
        break;
    }
    return false;
}

bool CSSPrimitiveValue::addDerivedHash(Hasher& hasher) const
{
    add(hasher, primitiveUnitType());

    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_UNKNOWN:
        break;
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        add(hasher, m_value.number);
        break;
    case CSSUnitType::CSS_PROPERTY_ID:
        add(hasher, m_value.propertyID);
        break;
    case CSSUnitType::CSS_VALUE_ID:
        add(hasher, m_value.valueID);
        break;
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_FONT_FAMILY:
        add(hasher, String { m_value.string });
        break;
    case CSSUnitType::CSS_ATTR:
        add(hasher, m_value.attr);
        break;
    case CSSUnitType::CSS_CALC:
        add(hasher, m_value.calc);
        break;
        break;
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

// https://drafts.css-houdini.org/css-properties-values-api/#dependency-cycles
void CSSPrimitiveValue::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    if (RefPtr calcValue = cssCalcValue()) {
        calcValue->collectComputedStyleDependencies(dependencies);
        return;
    }

    if (auto lengthUnit = CSS::toLengthUnit(primitiveUnitType()))
        CSS::collectComputedStyleDependencies(dependencies, *lengthUnit);
}

} // namespace WebCore
