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
#include <wtf/Hasher.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static inline bool isValidCSSUnitTypeForDoubleConversion(CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Degree:
    case CSSUnitType::Dimension:
    case CSSUnitType::DynamicViewportBlockSize:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportInlineSize:
    case CSSUnitType::DynamicViewportMax:
    case CSSUnitType::DynamicViewportMin:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Fr:
    case CSSUnitType::Gradian:
    case CSSUnitType::Hertz:
    case CSSUnitType::Inch:
    case CSSUnitType::Kilohertz:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Millisecond:
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Pica:
    case CSSUnitType::Percentage:
    case CSSUnitType::Point:
    case CSSUnitType::Pixel:
    case CSSUnitType::QuarterMillimeter:
    case CSSUnitType::Lh:
    case CSSUnitType::LargeViewportBlockSize:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportInlineSize:
    case CSSUnitType::LargeViewportMax:
    case CSSUnitType::LargeViewportMin:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::Rlh:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Radian:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Second:
    case CSSUnitType::SmallViewportBlockSize:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportInlineSize:
    case CSSUnitType::SmallViewportMax:
    case CSSUnitType::SmallViewportMin:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::Turn:
    case CSSUnitType::ViewportPercentageBlockSize:
    case CSSUnitType::ViewportPercentageHeight:
    case CSSUnitType::ViewportPercentageInlineSize:
    case CSSUnitType::ViewportPercentageMax:
    case CSSUnitType::ViewportPercentageMin:
    case CSSUnitType::ViewportPercentageWidth:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::X:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInlineSize:
    case CSSUnitType::ContainerQueryBlockSize:
    case CSSUnitType::ContainerQueryMin:
    case CSSUnitType::ContainerQueryMax:
        return true;
    case CSSUnitType::Attr:
    case CSSUnitType::FontFamily:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::PropertyId:
    case CSSUnitType::String:
    case CSSUnitType::Unknown:
    case CSSUnitType::ValueId:
        return false;
    case CSSUnitType::Ident:
        break;
    }

    ASSERT_NOT_REACHED();
    return false;
}

#if ASSERT_ENABLED

static inline bool isStringType(CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::String:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::Attr:
    case CSSUnitType::FontFamily:
        return true;
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Degree:
    case CSSUnitType::Dimension:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::DynamicViewportBlockSize:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportInlineSize:
    case CSSUnitType::DynamicViewportMax:
    case CSSUnitType::DynamicViewportMin:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::X:
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Fr:
    case CSSUnitType::Gradian:
    case CSSUnitType::Hertz:
    case CSSUnitType::Ident:
    case CSSUnitType::Inch:
    case CSSUnitType::Kilohertz:
    case CSSUnitType::LargeViewportBlockSize:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportInlineSize:
    case CSSUnitType::LargeViewportMax:
    case CSSUnitType::LargeViewportMin:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Millisecond:
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Pica:
    case CSSUnitType::Percentage:
    case CSSUnitType::PropertyId:
    case CSSUnitType::Point:
    case CSSUnitType::Pixel:
    case CSSUnitType::QuarterMillimeter:
    case CSSUnitType::Lh:
    case CSSUnitType::Rlh:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Radian:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Second:
    case CSSUnitType::SmallViewportBlockSize:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportInlineSize:
    case CSSUnitType::SmallViewportMax:
    case CSSUnitType::SmallViewportMin:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::Turn:
    case CSSUnitType::Unknown:
    case CSSUnitType::ValueId:
    case CSSUnitType::ViewportPercentageBlockSize:
    case CSSUnitType::ViewportPercentageHeight:
    case CSSUnitType::ViewportPercentageInlineSize:
    case CSSUnitType::ViewportPercentageMax:
    case CSSUnitType::ViewportPercentageMin:
    case CSSUnitType::ViewportPercentageWidth:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInlineSize:
    case CSSUnitType::ContainerQueryBlockSize:
    case CSSUnitType::ContainerQueryMin:
    case CSSUnitType::ContainerQueryMax:
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
    case CSSUnitType::PropertyId:
    case CSSUnitType::ValueId:
    case CSSUnitType::CustomIdent:
        return CSSUnitType::Ident;
    case CSSUnitType::FontFamily:
        // Web-exposed content expects font family values to have CSSUnitType::String primitive type
        // so we need to map our internal CSSUnitType::FontFamily type here.
        return CSSUnitType::String;
    default:
        if (RefPtr calcValue = cssCalcValue())
            return calcValue->primitiveType();

        return type;
    }
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSPropertyID propertyID)
    : CSSValue(ClassType::Primitive)
{
    setPrimitiveUnitType(CSSUnitType::PropertyId);
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
    setPrimitiveUnitType(CSSUnitType::ValueId);
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
    setPrimitiveUnitType(CSSUnitType::Calc);
    m_value.calc = &value.leakRef();
}

CSSPrimitiveValue::CSSPrimitiveValue(Ref<CSSAttrValue> value)
    : CSSValue(ClassType::Primitive)
{
    setPrimitiveUnitType(CSSUnitType::Attr);
    m_value.attr = &value.leakRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::String:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::FontFamily:
        if (m_value.string)
            m_value.string->deref();
        break;
    case CSSUnitType::Attr:
        m_value.attr->deref();
        break;
    case CSSUnitType::Calc:
        m_value.calc->deref();
        break;
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
        ASSERT_NOT_REACHED();
        break;
    case CSSUnitType::Dimension:
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Em:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Ex:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::Degree:
    case CSSUnitType::Radian:
    case CSSUnitType::Gradian:
    case CSSUnitType::Millisecond:
    case CSSUnitType::Second:
    case CSSUnitType::Hertz:
    case CSSUnitType::Kilohertz:
    case CSSUnitType::Turn:
    case CSSUnitType::ViewportPercentageWidth:
    case CSSUnitType::ViewportPercentageHeight:
    case CSSUnitType::ViewportPercentageMin:
    case CSSUnitType::ViewportPercentageMax:
    case CSSUnitType::ViewportPercentageBlockSize:
    case CSSUnitType::ViewportPercentageInlineSize:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMin:
    case CSSUnitType::SmallViewportMax:
    case CSSUnitType::SmallViewportBlockSize:
    case CSSUnitType::SmallViewportInlineSize:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMin:
    case CSSUnitType::LargeViewportMax:
    case CSSUnitType::LargeViewportBlockSize:
    case CSSUnitType::LargeViewportInlineSize:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMin:
    case CSSUnitType::DynamicViewportMax:
    case CSSUnitType::DynamicViewportBlockSize:
    case CSSUnitType::DynamicViewportInlineSize:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::X:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::Fr:
    case CSSUnitType::QuarterMillimeter:
    case CSSUnitType::Lh:
    case CSSUnitType::Rlh:
    case CSSUnitType::Ident:
    case CSSUnitType::Unknown:
    case CSSUnitType::PropertyId:
    case CSSUnitType::ValueId:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInlineSize:
    case CSSUnitType::ContainerQueryBlockSize:
    case CSSUnitType::ContainerQueryMin:
    case CSSUnitType::ContainerQueryMax:
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
    return adoptRef(*new CSSPrimitiveValue(value, CSSUnitType::Number));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(double value, CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::Number:
        if (RefPtr result = valueFromPool(staticCSSValuePool->m_numberValues, value))
            return result.releaseNonNull();
        break;
    case CSSUnitType::Percentage:
        if (RefPtr result = valueFromPool(staticCSSValuePool->m_percentageValues, value))
            return result.releaseNonNull();
        break;
    case CSSUnitType::Pixel:
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
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value), CSSUnitType::String));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<CSSCalc::Value> value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<CSSAttrValue> value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createCustomIdent(String value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value), CSSUnitType::CustomIdent));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createFontFamily(String value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value), CSSUnitType::FontFamily));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createInteger(double value)
{
    return adoptRef(*new CSSPrimitiveValue(value, CSSUnitType::Integer));
}

bool CSSPrimitiveValue::conversionToCanonicalUnitRequiresConversionData() const
{
    if (isCalculated())
        return m_value.calc->requiresConversionData();
    return WebCore::conversionToCanonicalUnitRequiresConversionData(primitiveType());
}

template<> int CSSPrimitiveValue::resolveAsLength(const CSSToLengthConversionData& conversionData) const
{
    return roundForImpreciseConversion<int>(resolveAsLengthDouble(conversionData));
}

template<> unsigned CSSPrimitiveValue::resolveAsLength(const CSSToLengthConversionData& conversionData) const
{
    return roundForImpreciseConversion<unsigned>(resolveAsLengthDouble(conversionData));
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
    return roundForImpreciseConversion<short>(resolveAsLengthDouble(conversionData));
}

template<> unsigned short CSSPrimitiveValue::resolveAsLength(const CSSToLengthConversionData& conversionData) const
{
    return roundForImpreciseConversion<unsigned short>(resolveAsLengthDouble(conversionData));
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
    case CSSUnitType::Pixel:
    case CSSUnitType::Degree:
    case CSSUnitType::Second:
    case CSSUnitType::Hertz:
    case CSSUnitType::DotsPerPixel:
        return 1.0;

    case CSSUnitType::X:
        return CSS::dppxPerX;
    case CSSUnitType::Centimeter:
        return CSS::pixelsPerCm;
    case CSSUnitType::DotsPerCentimeter:
        return CSS::dppxPerDpcm;
    case CSSUnitType::Millimeter:
        return CSS::pixelsPerMm;
    case CSSUnitType::QuarterMillimeter:
        return CSS::pixelsPerQ;
    case CSSUnitType::Inch:
        return CSS::pixelsPerInch;
    case CSSUnitType::DotsPerInch:
        return CSS::dppxPerDpi;
    case CSSUnitType::Point:
        return CSS::pixelsPerPt;
    case CSSUnitType::Pica:
        return CSS::pixelsPerPc;
    case CSSUnitType::Radian:
        return degreesPerRadianDouble;
    case CSSUnitType::Gradian:
        return degreesPerGradientDouble;
    case CSSUnitType::Turn:
        return degreesPerTurnDouble;
    case CSSUnitType::Millisecond:
        return CSS::secondsPerMillisecond;
    case CSSUnitType::Kilohertz:
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
        return calcValue->primitiveType() == CSSUnitType::Percentage ? calcValue->doubleValue(conversionData, { }) / 100.0 : calcValue->doubleValue(conversionData, { });
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
        return calcValue->primitiveType() == CSSUnitType::Percentage ? calcValue->doubleValueDeprecated() / 100.0 : calcValue->doubleValueDeprecated();
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
    if (requestedUnitType == sourceUnitType || requestedUnitType == CSSUnitType::Dimension)
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
        // We interpret conversion to CSSUnitType::Number as conversion to a canonical unit in this value's category.
        targetUnitType = canonicalUnitTypeForCategory(sourceCategory);
        if (targetUnitType == CSSUnitType::Unknown)
            return std::nullopt;
    }

    if (sourceUnitType == CSSUnitType::Number || sourceUnitType == CSSUnitType::Integer) {
        // Cannot convert between numbers and percent.
        if (targetCategory == CSSUnitCategory::Percent)
            return std::nullopt;
        // We interpret conversion from CSSUnitType::Number in the same way as CSSParser::validUnit() while using non-strict mode.
        sourceUnitType = canonicalUnitTypeForCategory(targetCategory);
        if (sourceUnitType == CSSUnitType::Unknown)
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
    if (requestedUnitType == sourceUnitType || requestedUnitType == CSSUnitType::Dimension)
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
        // We interpret conversion to CSSUnitType::Number as conversion to a canonical unit in this value's category.
        targetUnitType = canonicalUnitTypeForCategory(sourceCategory);
        if (targetUnitType == CSSUnitType::Unknown)
            return std::nullopt;
    }

    if (sourceUnitType == CSSUnitType::Number || sourceUnitType == CSSUnitType::Integer) {
        // Cannot convert between numbers and percent.
        if (targetCategory == CSSUnitCategory::Percent)
            return std::nullopt;
        // We interpret conversion from CSSUnitType::Number in the same way as CSSParser::validUnit() while using non-strict mode.
        sourceUnitType = canonicalUnitTypeForCategory(targetCategory);
        if (sourceUnitType == CSSUnitType::Unknown)
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
    case CSSUnitType::String:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::FontFamily:
        return m_value.string;
    case CSSUnitType::ValueId:
        return nameString(m_value.valueID);
    case CSSUnitType::PropertyId:
        return nameString(m_value.propertyID);
    case CSSUnitType::Attr:
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
    case CSSUnitType::Cap: return "cap"_s;
    case CSSUnitType::Ch: return "ch"_s;
    case CSSUnitType::Centimeter: return "cm"_s;
    case CSSUnitType::ContainerQueryBlockSize: return "cqb"_s;
    case CSSUnitType::ContainerQueryHeight: return "cqh"_s;
    case CSSUnitType::ContainerQueryInlineSize: return "cqi"_s;
    case CSSUnitType::ContainerQueryMax: return "cqmax"_s;
    case CSSUnitType::ContainerQueryMin: return "cqmin"_s;
    case CSSUnitType::ContainerQueryWidth: return "cqw"_s;
    case CSSUnitType::Degree: return "deg"_s;
    case CSSUnitType::DotsPerCentimeter: return "dpcm"_s;
    case CSSUnitType::DotsPerInch: return "dpi"_s;
    case CSSUnitType::DotsPerPixel: return "dppx"_s;
    case CSSUnitType::DynamicViewportBlockSize: return "dvb"_s;
    case CSSUnitType::DynamicViewportHeight: return "dvh"_s;
    case CSSUnitType::DynamicViewportInlineSize: return "dvi"_s;
    case CSSUnitType::DynamicViewportMax: return "dvmax"_s;
    case CSSUnitType::DynamicViewportMin: return "dvmin"_s;
    case CSSUnitType::DynamicViewportWidth: return "dvw"_s;
    case CSSUnitType::Em: return "em"_s;
    case CSSUnitType::Ex: return "ex"_s;
    case CSSUnitType::Fr: return "fr"_s;
    case CSSUnitType::Gradian: return "grad"_s;
    case CSSUnitType::Hertz: return "hz"_s;
    case CSSUnitType::Ic: return "ic"_s;
    case CSSUnitType::Inch: return "in"_s;
    case CSSUnitType::Kilohertz: return "khz"_s;
    case CSSUnitType::Lh: return "lh"_s;
    case CSSUnitType::LargeViewportBlockSize: return "lvb"_s;
    case CSSUnitType::LargeViewportHeight: return "lvh"_s;
    case CSSUnitType::LargeViewportInlineSize: return "lvi"_s;
    case CSSUnitType::LargeViewportMax: return "lvmax"_s;
    case CSSUnitType::LargeViewportMin: return "lvmin"_s;
    case CSSUnitType::LargeViewportWidth: return "lvw"_s;
    case CSSUnitType::Millimeter: return "mm"_s;
    case CSSUnitType::Millisecond: return "ms"_s;
    case CSSUnitType::Pica: return "pc"_s;
    case CSSUnitType::Percentage: return "%"_s;
    case CSSUnitType::Point: return "pt"_s;
    case CSSUnitType::Pixel: return "px"_s;
    case CSSUnitType::QuarterMillimeter: return "q"_s;
    case CSSUnitType::Radian: return "rad"_s;
    case CSSUnitType::Rcap: return "rcap"_s;
    case CSSUnitType::Rch: return "rch"_s;
    case CSSUnitType::Rem: return "rem"_s;
    case CSSUnitType::Rex: return "rex"_s;
    case CSSUnitType::Ric: return "ric"_s;
    case CSSUnitType::Rlh: return "rlh"_s;
    case CSSUnitType::Second: return "s"_s;
    case CSSUnitType::SmallViewportBlockSize: return "svb"_s;
    case CSSUnitType::SmallViewportHeight: return "svh"_s;
    case CSSUnitType::SmallViewportInlineSize: return "svi"_s;
    case CSSUnitType::SmallViewportMax: return "svmax"_s;
    case CSSUnitType::SmallViewportMin: return "svmin"_s;
    case CSSUnitType::SmallViewportWidth: return "svw"_s;
    case CSSUnitType::Turn: return "turn"_s;
    case CSSUnitType::ViewportPercentageBlockSize: return "vb"_s;
    case CSSUnitType::ViewportPercentageHeight: return "vh"_s;
    case CSSUnitType::ViewportPercentageInlineSize: return "vi"_s;
    case CSSUnitType::ViewportPercentageMax: return "vmax"_s;
    case CSSUnitType::ViewportPercentageMin: return "vmin"_s;
    case CSSUnitType::ViewportPercentageWidth: return "vw"_s;
    case CSSUnitType::X: return "x"_s;

    case CSSUnitType::Attr:
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::Dimension:
    case CSSUnitType::FontFamily:
    case CSSUnitType::Ident:
    case CSSUnitType::Integer:
    case CSSUnitType::Number:
    case CSSUnitType::PropertyId:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::String:
    case CSSUnitType::Unknown:
    case CSSUnitType::ValueId:
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
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Centimeter:
    case CSSUnitType::ContainerQueryBlockSize:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInlineSize:
    case CSSUnitType::ContainerQueryMax:
    case CSSUnitType::ContainerQueryMin:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::Degree:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::DynamicViewportBlockSize:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportInlineSize:
    case CSSUnitType::DynamicViewportMax:
    case CSSUnitType::DynamicViewportMin:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Fr:
    case CSSUnitType::Gradian:
    case CSSUnitType::Hertz:
    case CSSUnitType::Ic:
    case CSSUnitType::Inch:
    case CSSUnitType::Kilohertz:
    case CSSUnitType::Lh:
    case CSSUnitType::LargeViewportBlockSize:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportInlineSize:
    case CSSUnitType::LargeViewportMax:
    case CSSUnitType::LargeViewportMin:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Millisecond:
    case CSSUnitType::Number:
    case CSSUnitType::Pica:
    case CSSUnitType::Percentage:
    case CSSUnitType::Point:
    case CSSUnitType::Pixel:
    case CSSUnitType::QuarterMillimeter:
    case CSSUnitType::Radian:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Rlh:
    case CSSUnitType::Second:
    case CSSUnitType::SmallViewportBlockSize:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportInlineSize:
    case CSSUnitType::SmallViewportMax:
    case CSSUnitType::SmallViewportMin:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::Turn:
    case CSSUnitType::ViewportPercentageBlockSize:
    case CSSUnitType::ViewportPercentageHeight:
    case CSSUnitType::ViewportPercentageInlineSize:
    case CSSUnitType::ViewportPercentageMax:
    case CSSUnitType::ViewportPercentageMin:
    case CSSUnitType::ViewportPercentageWidth:
    case CSSUnitType::X:
        return formatNumberValue(unitTypeString(type));
    case CSSUnitType::Attr:
        return protectedCssAttrValue()->cssText(context);
    case CSSUnitType::Calc:
        return protectedCssCalcValue()->cssText(context);
    case CSSUnitType::Dimension:
        // FIXME: This isn't correct.
        return formatNumberValue(""_s);
    case CSSUnitType::FontFamily:
        return serializeFontFamily(m_value.string);
    case CSSUnitType::Integer:
        return formatIntegerValue(""_s);
    case CSSUnitType::QuirkyEm:
        return formatNumberValue("em"_s);
    case CSSUnitType::String:
        return serializeString(m_value.string);
    case CSSUnitType::CustomIdent: {
        StringBuilder builder;
        serializeIdentifier(m_value.string, builder);
        return builder.toString();
    }

    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::Ident:
    case CSSUnitType::PropertyId:
    case CSSUnitType::Unknown:
    case CSSUnitType::ValueId:
        break;
    }
    ASSERT_NOT_REACHED();
    return String();
}

String CSSPrimitiveValue::customCSSText(const CSS::SerializationContext& context) const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::Unknown:
        return String();
    case CSSUnitType::ValueId:
        return nameStringForSerialization(m_value.valueID);
    case CSSUnitType::PropertyId:
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
    case CSSUnitType::Unknown:
        return false;
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Em:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Ex:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::X:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::Degree:
    case CSSUnitType::Radian:
    case CSSUnitType::Gradian:
    case CSSUnitType::Millisecond:
    case CSSUnitType::Second:
    case CSSUnitType::Hertz:
    case CSSUnitType::Kilohertz:
    case CSSUnitType::Turn:
    case CSSUnitType::ViewportPercentageWidth:
    case CSSUnitType::ViewportPercentageHeight:
    case CSSUnitType::ViewportPercentageMin:
    case CSSUnitType::ViewportPercentageMax:
    case CSSUnitType::ViewportPercentageBlockSize:
    case CSSUnitType::ViewportPercentageInlineSize:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMin:
    case CSSUnitType::SmallViewportMax:
    case CSSUnitType::SmallViewportBlockSize:
    case CSSUnitType::SmallViewportInlineSize:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMin:
    case CSSUnitType::LargeViewportMax:
    case CSSUnitType::LargeViewportBlockSize:
    case CSSUnitType::LargeViewportInlineSize:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMin:
    case CSSUnitType::DynamicViewportMax:
    case CSSUnitType::DynamicViewportBlockSize:
    case CSSUnitType::DynamicViewportInlineSize:
    case CSSUnitType::Fr:
    case CSSUnitType::QuarterMillimeter:
    case CSSUnitType::Lh:
    case CSSUnitType::Rlh:
    case CSSUnitType::Dimension:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInlineSize:
    case CSSUnitType::ContainerQueryBlockSize:
    case CSSUnitType::ContainerQueryMin:
    case CSSUnitType::ContainerQueryMax:
        return m_value.number == other.m_value.number;
    case CSSUnitType::PropertyId:
        return m_value.propertyID == other.m_value.propertyID;
    case CSSUnitType::ValueId:
        return m_value.valueID == other.m_value.valueID;
    case CSSUnitType::String:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::FontFamily:
        return equal(m_value.string, other.m_value.string);
    case CSSUnitType::Attr:
        return protectedCssAttrValue()->equals(*other.protectedCssAttrValue());
    case CSSUnitType::Calc:
        return protectedCssCalcValue()->equals(*other.protectedCssCalcValue());
    case CSSUnitType::Ident:
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
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
    case CSSUnitType::Unknown:
        break;
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Em:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Ex:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::X:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::Degree:
    case CSSUnitType::Radian:
    case CSSUnitType::Gradian:
    case CSSUnitType::Millisecond:
    case CSSUnitType::Second:
    case CSSUnitType::Hertz:
    case CSSUnitType::Kilohertz:
    case CSSUnitType::Turn:
    case CSSUnitType::ViewportPercentageWidth:
    case CSSUnitType::ViewportPercentageHeight:
    case CSSUnitType::ViewportPercentageMin:
    case CSSUnitType::ViewportPercentageMax:
    case CSSUnitType::ViewportPercentageBlockSize:
    case CSSUnitType::ViewportPercentageInlineSize:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMin:
    case CSSUnitType::SmallViewportMax:
    case CSSUnitType::SmallViewportBlockSize:
    case CSSUnitType::SmallViewportInlineSize:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMin:
    case CSSUnitType::LargeViewportMax:
    case CSSUnitType::LargeViewportBlockSize:
    case CSSUnitType::LargeViewportInlineSize:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMin:
    case CSSUnitType::DynamicViewportMax:
    case CSSUnitType::DynamicViewportBlockSize:
    case CSSUnitType::DynamicViewportInlineSize:
    case CSSUnitType::Fr:
    case CSSUnitType::QuarterMillimeter:
    case CSSUnitType::Lh:
    case CSSUnitType::Rlh:
    case CSSUnitType::Dimension:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInlineSize:
    case CSSUnitType::ContainerQueryBlockSize:
    case CSSUnitType::ContainerQueryMin:
    case CSSUnitType::ContainerQueryMax:
        add(hasher, m_value.number);
        break;
    case CSSUnitType::PropertyId:
        add(hasher, m_value.propertyID);
        break;
    case CSSUnitType::ValueId:
        add(hasher, m_value.valueID);
        break;
    case CSSUnitType::String:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::FontFamily:
        add(hasher, String { m_value.string });
        break;
    case CSSUnitType::Attr:
        add(hasher, m_value.attr);
        break;
    case CSSUnitType::Calc:
        add(hasher, m_value.calc);
        break;
        break;
    case CSSUnitType::Ident:
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
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
