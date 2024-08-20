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

#include "CSSAnchorValue.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSHelper.h"
#include "CSSMarkup.h"
#include "CSSParserIdioms.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSToLengthConversionData.h"
#include "CSSUnresolvedColor.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "CalculationCategory.h"
#include "CalculationValue.h"
#include "Color.h"
#include "ColorSerialization.h"
#include "ContainerQueryEvaluator.h"
#include "FontCascade.h"
#include "Length.h"
#include "NodeRenderStyle.h"
#include "RenderBoxInlines.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include <wtf/Hasher.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static inline bool isValidCSSUnitTypeForDoubleConversion(CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::Anchor:
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::CalcPercentageWithNumber:
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Degree:
    case CSSUnitType::Dimension:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::DynamicViewportMaximum:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Fraction:
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
    case CSSUnitType::Quarter:
    case CSSUnitType::LineHeight:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Radian:
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
    case CSSUnitType::Second:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::Turn:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::MultiplicationFactor:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryMaximum:
        return true;
    case CSSUnitType::Attribute:
    case CSSUnitType::FontFamily:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::PropertyID:
    case CSSUnitType::RgbColor:
    case CSSUnitType::String:
    case CSSUnitType::Unknown:
    case CSSUnitType::UnresolvedColor:
    case CSSUnitType::Uri:
    case CSSUnitType::ValueID:
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
    case CSSUnitType::Uri:
    case CSSUnitType::Attribute:
    case CSSUnitType::FontFamily:
        return true;
    case CSSUnitType::Anchor:
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::CalcPercentageWithNumber:
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Degree:
    case CSSUnitType::Dimension:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::DynamicViewportMaximum:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::MultiplicationFactor:
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Fraction:
    case CSSUnitType::Gradian:
    case CSSUnitType::Hertz:
    case CSSUnitType::Ident:
    case CSSUnitType::Inch:
    case CSSUnitType::Kilohertz:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Millisecond:
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Pica:
    case CSSUnitType::Percentage:
    case CSSUnitType::PropertyID:
    case CSSUnitType::Point:
    case CSSUnitType::Pixel:
    case CSSUnitType::Quarter:
    case CSSUnitType::LineHeight:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Radian:
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
    case CSSUnitType::RgbColor:
    case CSSUnitType::Second:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::Turn:
    case CSSUnitType::Unknown:
    case CSSUnitType::UnresolvedColor:
    case CSSUnitType::ValueID:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryMaximum:
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
    // FIXME: Use a switch statement here.

    if (primitiveUnitType() == CSSUnitType::PropertyID || primitiveUnitType() == CSSUnitType::ValueID || primitiveUnitType() == CSSUnitType::CustomIdent)
        return CSSUnitType::Ident;

    // Web-exposed content expects font family values to have CSSUnitType::CSS_STRING primitive type
    // so we need to map our internal CSSUnitType::CSS_FONT_FAMILY type here.
    if (primitiveUnitType() == CSSUnitType::FontFamily)
        return CSSUnitType::String;

    if (!isCalculated())
        return primitiveUnitType();

    switch (m_value.calc->category()) {
    case CalculationCategory::Number:
        return CSSUnitType::Number;
    case CalculationCategory::Percent:
        return CSSUnitType::Percentage;
    case CalculationCategory::PercentNumber:
        return CSSUnitType::CalcPercentageWithNumber;
    case CalculationCategory::PercentLength:
        return CSSUnitType::CalcPercentageWithLength;
    case CalculationCategory::Length:
    case CalculationCategory::Angle:
    case CalculationCategory::Time:
    case CalculationCategory::Frequency:
    case CalculationCategory::Resolution:
        return m_value.calc->primitiveType();
    case CalculationCategory::Other:
        return CSSUnitType::Unknown;
    }
    return CSSUnitType::Unknown;
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSPropertyID propertyID)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::PropertyID);
    m_value.propertyID = propertyID;
}

CSSPrimitiveValue::CSSPrimitiveValue(double number, CSSUnitType type)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(type);
    m_value.number = number;
}

CSSPrimitiveValue::CSSPrimitiveValue(const String& string, CSSUnitType type)
    : CSSValue(PrimitiveClass)
{
    ASSERT(isStringType(type));
    setPrimitiveUnitType(type);
    if ((m_value.string = string.impl()))
        m_value.string->ref();
}

CSSPrimitiveValue::CSSPrimitiveValue(Color color)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::RgbColor);
    static_assert(sizeof(m_value.colorAsInteger) == sizeof(color));
    new (reinterpret_cast<Color*>(&m_value.colorAsInteger)) Color(WTFMove(color));
}

Color CSSPrimitiveValue::absoluteColor() const
{
    if (isColor())
        return color();

    // FIXME: there are some cases where we can resolve a dynamic color at parse time, we should support them.
    if (isUnresolvedColor())
        return { };

    if (StyleColor::isAbsoluteColorKeyword(valueID()))
        return StyleColor::colorFromAbsoluteKeyword(valueID());

    return { };
}

CSSPrimitiveValue::CSSPrimitiveValue(StaticCSSValueTag, CSSValueID valueID)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::ValueID);
    m_value.valueID = valueID;
    makeStatic();
}

CSSPrimitiveValue::CSSPrimitiveValue(StaticCSSValueTag, Color color)
    : CSSPrimitiveValue(WTFMove(color))
{
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

CSSPrimitiveValue::CSSPrimitiveValue(Ref<CSSCalcValue> value)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::Calc);
    m_value.calc = &value.leakRef();
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSUnresolvedColor unresolvedColor)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::UnresolvedColor);
    m_value.unresolvedColor = new CSSUnresolvedColor(WTFMove(unresolvedColor));
}

CSSPrimitiveValue::CSSPrimitiveValue(Ref<CSSAnchorValue> value)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::Anchor);
    m_value.anchor = &value.leakRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::String:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::Uri:
    case CSSUnitType::Attribute:
    case CSSUnitType::FontFamily:
        if (m_value.string)
            m_value.string->deref();
        break;
    case CSSUnitType::Anchor:
        m_value.anchor->deref();
        break;
    case CSSUnitType::Calc:
        m_value.calc->deref();
        break;
    case CSSUnitType::CalcPercentageWithNumber:
    case CSSUnitType::CalcPercentageWithLength:
        ASSERT_NOT_REACHED();
        break;
    case CSSUnitType::RgbColor:
        std::destroy_at(reinterpret_cast<Color*>(&m_value.colorAsInteger));
        break;
    case CSSUnitType::UnresolvedColor:
        delete m_value.unresolvedColor;
        break;
    case CSSUnitType::Dimension:
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Em:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Ex:
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
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
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::DynamicViewportMaximum:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::MultiplicationFactor:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::Fraction:
    case CSSUnitType::Quarter:
    case CSSUnitType::LineHeight:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::Ident:
    case CSSUnitType::Unknown:
    case CSSUnitType::PropertyID:
    case CSSUnitType::ValueID:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryMaximum:
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

static CSSPrimitiveValue* valueFromPool(std::span<LazyNeverDestroyed<CSSPrimitiveValue>> pool, double value)
{
    // Casting to a signed integer first since casting a negative floating point value to an unsigned
    // integer is undefined behavior.
    unsigned poolIndex = static_cast<unsigned>(static_cast<int>(value));
    double roundTripValue = poolIndex;
    if (!memcmp(&value, &roundTripValue, sizeof(double)) && poolIndex < pool.size())
        return &pool[poolIndex].get();
    return nullptr;
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(double value)
{
    if (auto* result = valueFromPool(staticCSSValuePool->m_numberValues, value))
        return *result;
    return adoptRef(*new CSSPrimitiveValue(value, CSSUnitType::Number));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(double value, CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::Number:
        if (auto* result = valueFromPool(staticCSSValuePool->m_numberValues, value))
            return *result;
        break;
    case CSSUnitType::Percentage:
        if (auto* result = valueFromPool(staticCSSValuePool->m_percentValues, value))
            return *result;
        break;
    case CSSUnitType::Pixel:
        if (auto* result = valueFromPool(staticCSSValuePool->m_pixelValues, value))
            return *result;
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

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(const Length& length)
{
    switch (length.type()) {
    case LengthType::Auto:
        return create(CSSValueAuto);
    case LengthType::Content:
        return create(CSSValueContent);
    case LengthType::FillAvailable:
        return create(CSSValueWebkitFillAvailable);
    case LengthType::FitContent:
        return create(CSSValueFitContent);
    case LengthType::Fixed:
        return create(length.value(), CSSUnitType::Pixel);
    case LengthType::Intrinsic:
        return create(CSSValueIntrinsic);
    case LengthType::MinIntrinsic:
        return create(CSSValueMinIntrinsic);
    case LengthType::MinContent:
        return create(CSSValueMinContent);
    case LengthType::MaxContent:
        return create(CSSValueMaxContent);
    case LengthType::Normal:
        return create(CSSValueNormal);
    case LengthType::Percent:
        ASSERT(std::isfinite(length.percent()));
        return create(length.percent(), CSSUnitType::Percentage);
    case LengthType::Calculated:
    case LengthType::Relative:
    case LengthType::Undefined:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(const Length& length, const RenderStyle& style)
{
    switch (length.type()) {
    case LengthType::Auto:
    case LengthType::Content:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::Normal:
    case LengthType::Percent:
        return create(length);
    case LengthType::Fixed:
        return create(adjustFloatForAbsoluteZoom(length.value(), style), CSSUnitType::Pixel);
    case LengthType::Calculated:
        // FIXME: Do we have a guarantee that CSSCalcValue::create will not return null here?
        return create(CSSCalcValue::create(length.calculationValue(), style).releaseNonNull());
    case LengthType::Relative:
    case LengthType::Undefined:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<CSSCalcValue> value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(CSSUnresolvedColor value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<CSSAnchorValue> value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createAttr(String value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value), CSSUnitType::Attribute));
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

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createURI(String value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value), CSSUnitType::Uri));
}

double CSSPrimitiveValue::computeDegrees() const
{
    return computeDegrees(primitiveType(), doubleValue());
}

template<> int CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return roundForImpreciseConversion<int>(computeLengthDouble(conversionData));
}

template<> unsigned CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return roundForImpreciseConversion<unsigned>(computeLengthDouble(conversionData));
}

template<> Length CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return Length(clampTo<double>(computeLengthDouble(conversionData), minValueForCssLength, maxValueForCssLength), LengthType::Fixed);
}

template<> short CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return roundForImpreciseConversion<short>(computeLengthDouble(conversionData));
}

template<> unsigned short CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return roundForImpreciseConversion<unsigned short>(computeLengthDouble(conversionData));
}

template<> float CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return static_cast<float>(computeLengthDouble(conversionData));
}

template<> double CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return computeLengthDouble(conversionData);
}

template<> LayoutUnit CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData) const
{
    return LayoutUnit(computeLengthDouble(conversionData));
}

double CSSPrimitiveValue::computeLengthDouble(const CSSToLengthConversionData& conversionData) const
{
    if (isCalculated()) {
        // The multiplier and factor is applied to each value in the calc expression individually
        return m_value.calc->computeLengthPx(conversionData);
    }

    return computeNonCalcLengthDouble(conversionData, primitiveType(), m_value.number);
}

static constexpr double mmPerInch = 25.4;
static constexpr double cmPerInch = 2.54;
static constexpr double QPerInch = 25.4 * 4.0;

static double lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis logicalAxis, const FloatSize& size, const RenderStyle* style)
{
    if (!style)
        return 0;

    switch (mapLogicalAxisToPhysicalAxis(makeTextFlow(style->writingMode(), style->direction()), logicalAxis)) {
    case BoxAxis::Horizontal:
        return size.width();

    case BoxAxis::Vertical:
        return size.height();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static double lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis logicalAxis, const FloatSize& size, const RenderView& renderView)
{
    const auto* rootElement = renderView.document().documentElement();
    if (!rootElement)
        return 0;

    return lengthOfViewportPhysicalAxisForLogicalAxis(logicalAxis, size, rootElement->renderStyle());
}

double CSSPrimitiveValue::computeUnzoomedNonCalcLengthDouble(CSSUnitType primitiveType, double value, CSSPropertyID propertyToCompute, const FontCascade* fontCascadeForUnit, const RenderView* renderView)
{
    switch (primitiveType) {
    case CSSUnitType::Em:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::RootEm: {
        ASSERT(fontCascadeForUnit);
        auto& fontDescription = fontCascadeForUnit->fontDescription();
        return ((propertyToCompute == CSSPropertyFontSize) ? fontDescription.specifiedSize() : fontDescription.computedSize()) * value;
    }
    case CSSUnitType::Ex:
    case CSSUnitType::RootEx: {
        ASSERT(fontCascadeForUnit);
        auto& fontMetrics = fontCascadeForUnit->metricsOfPrimaryFont();
        if (fontMetrics.xHeight())
            return fontMetrics.xHeight().value() * value;
        auto& fontDescription = fontCascadeForUnit->fontDescription();
        return ((propertyToCompute == CSSPropertyFontSize) ? fontDescription.specifiedSize() : fontDescription.computedSize()) / 2.0 * value;
    }
    case CSSUnitType::CapHeight:
    case CSSUnitType::RootCapHeight: {
        ASSERT(fontCascadeForUnit);
        auto& fontMetrics = fontCascadeForUnit->metricsOfPrimaryFont();
        if (fontMetrics.capHeight())
            return fontMetrics.capHeight().value() * value;
        return fontMetrics.intAscent() * value;
    }
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::RootCharacterWidth:
        ASSERT(fontCascadeForUnit);
        return fontCascadeForUnit->zeroWidth() * value;
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::RootIdeographicCharacter:
        ASSERT(fontCascadeForUnit);
        return fontCascadeForUnit->metricsOfPrimaryFont().ideogramWidth().value_or(0) * value;
    case CSSUnitType::Pixel:
        return value;
    case CSSUnitType::Centimeter:
        return cssPixelsPerInch / cmPerInch * value;
    case CSSUnitType::Millimeter:
        return cssPixelsPerInch / mmPerInch * value;
    case CSSUnitType::Quarter:
        return cssPixelsPerInch / QPerInch * value;
    case CSSUnitType::LineHeight:
    case CSSUnitType::RootLineHeight:
        ASSERT_NOT_REACHED();
        return -1.0;
    case CSSUnitType::Inch:
        return cssPixelsPerInch * value;
    case CSSUnitType::Point:
        return cssPixelsPerInch / 72.0 * value;
    case CSSUnitType::Pica:
        // 1 pc == 12 pt
        return cssPixelsPerInch * 12.0 / 72.0 * value;
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::CalcPercentageWithNumber:
        ASSERT_NOT_REACHED();
        return -1.0;
    case CSSUnitType::ViewportHeight:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().height() / 100.0 * value : 0;
    case CSSUnitType::ViewportWidth:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().width() / 100.0 * value : 0;
    case CSSUnitType::ViewportMaximum:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().maxDimension() / 100.0 * value : value;
    case CSSUnitType::ViewportMinimum:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().minDimension() / 100.0 * value : value;
    case CSSUnitType::ViewportBlock:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSDefaultViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::ViewportInline:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, renderView->sizeForCSSDefaultViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::SmallViewportHeight:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().height() / 100.0 * value : 0;
    case CSSUnitType::SmallViewportWidth:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().width() / 100.0 * value : 0;
    case CSSUnitType::SmallViewportMaximum:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().maxDimension() / 100.0 * value : value;
    case CSSUnitType::SmallViewportMinimum:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().minDimension() / 100.0 * value : value;
    case CSSUnitType::SmallViewportBlock:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSSmallViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::SmallViewportInline:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, renderView->sizeForCSSSmallViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::LargeViewportHeight:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().height() / 100.0 * value : 0;
    case CSSUnitType::LargeViewportWidth:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().width() / 100.0 * value : 0;
    case CSSUnitType::LargeViewportMaximum:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().maxDimension() / 100.0 * value : value;
    case CSSUnitType::LargeViewportMinimum:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().minDimension() / 100.0 * value : value;
    case CSSUnitType::LargeViewportBlock:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSLargeViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::LargeViewportInline:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, renderView->sizeForCSSLargeViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::DynamicViewportHeight:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().height() / 100.0 * value : 0;
    case CSSUnitType::DynamicViewportWidth:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().width() / 100.0 * value : 0;
    case CSSUnitType::DynamicViewportMaximum:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().maxDimension() / 100.0 * value : value;
    case CSSUnitType::DynamicViewportMinimum:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().minDimension() / 100.0 * value : value;
    case CSSUnitType::DynamicViewportBlock:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSDynamicViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::DynamicViewportInline:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, renderView->sizeForCSSDynamicViewportUnits(), *renderView) / 100.0 * value : 0;
    default:
        ASSERT_NOT_REACHED();
        return -1.0;
    }
}

double CSSPrimitiveValue::computeNonCalcLengthDouble(const CSSToLengthConversionData& conversionData, CSSUnitType primitiveType, double value)
{
    auto resolveContainerUnit = [&](CQ::Axis physicalAxis) -> std::optional<double> {
        ASSERT(physicalAxis == CQ::Axis::Width || physicalAxis == CQ::Axis::Height);

        conversionData.setUsesContainerUnits();

        auto* element = conversionData.elementForContainerUnitResolution();
        if (!element)
            return { };

        auto mode = conversionData.style()->pseudoElementType() == PseudoId::None
            ? Style::ContainerQueryEvaluator::SelectionMode::Element
            : Style::ContainerQueryEvaluator::SelectionMode::PseudoElement;

        // "The query container for each axis is the nearest ancestor container that accepts container size queries on that axis."
        while ((element = Style::ContainerQueryEvaluator::selectContainer(physicalAxis, nullString(), *element, mode))) {
            auto* containerRenderer = dynamicDowncast<RenderBox>(element->renderer());
            if (containerRenderer && containerRenderer->hasEligibleContainmentForSizeQuery()) {
                auto widthOrHeight = physicalAxis == CQ::Axis::Width ? containerRenderer->contentWidth() : containerRenderer->contentHeight();
                return widthOrHeight * value / 100;
            }
            // For pseudo-elements the element itself can be the container. Avoid looping forever.
            mode = Style::ContainerQueryEvaluator::SelectionMode::Element;
        }
        return { };
    };

    switch (primitiveType) {
    case CSSUnitType::Em:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Ex:
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::IdeographicCharacter:
        // FIXME: We have a bug right now where the zoom will be applied twice to EX units.
        // We really need to compute EX using fontMetrics for the original specifiedSize and not use
        // our actual constructed rendering font.
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), &conversionData.fontCascadeForFontUnits());
        break;

    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), conversionData.rootStyle() ? &conversionData.rootStyle()->fontCascade() : &conversionData.fontCascadeForFontUnits());
        break;

    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Quarter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::CalcPercentageWithNumber:
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute());
        break;

    case CSSUnitType::ViewportHeight:
        return value * conversionData.defaultViewportFactor().height();

    case CSSUnitType::ViewportWidth:
        return value * conversionData.defaultViewportFactor().width();

    case CSSUnitType::ViewportMaximum:
        return value * conversionData.defaultViewportFactor().maxDimension();

    case CSSUnitType::ViewportMinimum:
        return value * conversionData.defaultViewportFactor().minDimension();

    case CSSUnitType::ViewportBlock:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.defaultViewportFactor(), conversionData.style());

    case CSSUnitType::ViewportInline:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.defaultViewportFactor(), conversionData.style());

    case CSSUnitType::SmallViewportHeight:
        return value * conversionData.smallViewportFactor().height();

    case CSSUnitType::SmallViewportWidth:
        return value * conversionData.smallViewportFactor().width();

    case CSSUnitType::SmallViewportMaximum:
        return value * conversionData.smallViewportFactor().maxDimension();

    case CSSUnitType::SmallViewportMinimum:
        return value * conversionData.smallViewportFactor().minDimension();

    case CSSUnitType::SmallViewportBlock:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.smallViewportFactor(), conversionData.style());

    case CSSUnitType::SmallViewportInline:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.smallViewportFactor(), conversionData.style());

    case CSSUnitType::LargeViewportHeight:
        return value * conversionData.largeViewportFactor().height();

    case CSSUnitType::LargeViewportWidth:
        return value * conversionData.largeViewportFactor().width();

    case CSSUnitType::LargeViewportMaximum:
        return value * conversionData.largeViewportFactor().maxDimension();

    case CSSUnitType::LargeViewportMinimum:
        return value * conversionData.largeViewportFactor().minDimension();

    case CSSUnitType::LargeViewportBlock:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.largeViewportFactor(), conversionData.style());

    case CSSUnitType::LargeViewportInline:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.largeViewportFactor(), conversionData.style());

    case CSSUnitType::DynamicViewportHeight:
        return value * conversionData.dynamicViewportFactor().height();

    case CSSUnitType::DynamicViewportWidth:
        return value * conversionData.dynamicViewportFactor().width();

    case CSSUnitType::DynamicViewportMaximum:
        return value * conversionData.dynamicViewportFactor().maxDimension();

    case CSSUnitType::DynamicViewportMinimum:
        return value * conversionData.dynamicViewportFactor().minDimension();

    case CSSUnitType::DynamicViewportBlock:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.dynamicViewportFactor(), conversionData.style());

    case CSSUnitType::DynamicViewportInline:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.dynamicViewportFactor(), conversionData.style());

    case CSSUnitType::LineHeight:
        if (conversionData.computingLineHeight() || conversionData.computingFontSize()) {
            // Try to get the parent's computed line-height, or fall back to the initial line-height of this element's font spacing.
            value *= conversionData.parentStyle() ? conversionData.parentStyle()->computedLineHeight() : conversionData.fontCascadeForFontUnits().metricsOfPrimaryFont().intLineSpacing();
        } else
            value *= conversionData.computedLineHeightForFontUnits();
        break;

    case CSSUnitType::ContainerQueryWidth: {
        if (auto resolvedValue = resolveContainerUnit(CQ::Axis::Width))
            return *resolvedValue;
        return computeNonCalcLengthDouble(conversionData, CSSUnitType::SmallViewportWidth, value);
    }

    case CSSUnitType::ContainerQueryHeight: {
        if (auto resolvedValue = resolveContainerUnit(CQ::Axis::Height))
            return *resolvedValue;
        return computeNonCalcLengthDouble(conversionData, CSSUnitType::SmallViewportHeight, value);
    }

    case CSSUnitType::ContainerQueryInline: {
        if (auto resolvedValue = resolveContainerUnit(conversionData.style()->isHorizontalWritingMode() ? CQ::Axis::Width : CQ::Axis::Height))
            return *resolvedValue;
        return computeNonCalcLengthDouble(conversionData, CSSUnitType::SmallViewportInline, value);
    }

    case CSSUnitType::ContainerQueryBlock: {
        if (auto resolvedValue = resolveContainerUnit(conversionData.style()->isHorizontalWritingMode() ? CQ::Axis::Height : CQ::Axis::Width))
            return *resolvedValue;
        return computeNonCalcLengthDouble(conversionData, CSSUnitType::SmallViewportBlock, value);
    }

    case CSSUnitType::ContainerQueryMaximum:
        return std::max(computeNonCalcLengthDouble(conversionData, CSSUnitType::ContainerQueryBlock, value), computeNonCalcLengthDouble(conversionData, CSSUnitType::ContainerQueryInline, value));

    case CSSUnitType::ContainerQueryMinimum:
        return std::min(computeNonCalcLengthDouble(conversionData, CSSUnitType::ContainerQueryBlock, value), computeNonCalcLengthDouble(conversionData, CSSUnitType::ContainerQueryInline, value));

    case CSSUnitType::RootLineHeight:
        if (conversionData.rootStyle()) {
            if (conversionData.computingLineHeight() || conversionData.computingFontSize())
                value *= conversionData.rootStyle()->computeLineHeight(conversionData.rootStyle()->specifiedLineHeight());
            else
                value *= conversionData.rootStyle()->computedLineHeight();
        }
        break;

    default:
        ASSERT_NOT_REACHED();
        return -1.0;
    }

    // We do not apply the zoom factor when we are computing the value of the font-size property. The zooming
    // for font sizes is much more complicated, since we have to worry about enforcing the minimum font size preference
    // as well as enforcing the implicit "smart minimum."
    if (conversionData.computingFontSize() || isFontRelativeLength(primitiveType))
        return value;

    return value * conversionData.zoom();
}

bool CSSPrimitiveValue::equalForLengthResolution(const RenderStyle& styleA, const RenderStyle& styleB)
{
    // These properties affect results of computeNonCalcLengthDouble above.
    if (styleA.fontDescription().computedSize() != styleB.fontDescription().computedSize())
        return false;
    if (styleA.fontDescription().specifiedSize() != styleB.fontDescription().specifiedSize())
        return false;

    if (styleA.metricsOfPrimaryFont().xHeight() != styleB.metricsOfPrimaryFont().xHeight())
        return false;
    if (styleA.metricsOfPrimaryFont().zeroWidth() != styleB.metricsOfPrimaryFont().zeroWidth())
        return false;

    if (styleA.zoom() != styleB.zoom())
        return false;

    return true;
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

    case CSSUnitType::MultiplicationFactor:
        // This is semantically identical to (canonical) dppx
        return 1.0;

    case CSSUnitType::Centimeter:
        return cssPixelsPerInch / cmPerInch;

    case CSSUnitType::DotsPerCentimeter:
        return cmPerInch / cssPixelsPerInch; // (2.54 cm/in)

    case CSSUnitType::Millimeter:
        return cssPixelsPerInch / mmPerInch;

    case CSSUnitType::Quarter:
        return cssPixelsPerInch / QPerInch;

    case CSSUnitType::Inch:
        return cssPixelsPerInch;

    case CSSUnitType::DotsPerInch:
        return 1 / cssPixelsPerInch;

    case CSSUnitType::Point:
        return cssPixelsPerInch / 72.0;

    case CSSUnitType::Pica:
        return cssPixelsPerInch * 12.0 / 72.0; // 1 pc == 12 pt

    case CSSUnitType::Radian:
        return degreesPerRadianDouble;

    case CSSUnitType::Gradian:
        return degreesPerGradientDouble;

    case CSSUnitType::Turn:
        return degreesPerTurnDouble;

    case CSSUnitType::Millisecond:
        return 0.001;
    case CSSUnitType::Kilohertz:
        return 1000;

    default:
        return std::nullopt;
    }
}

ExceptionOr<float> CSSPrimitiveValue::getFloatValue(CSSUnitType unitType) const
{
    auto result = doubleValueInternal(unitType);
    if (!result)
        return Exception { ExceptionCode::InvalidAccessError };
    return clampTo<float>(result.value());
}

double CSSPrimitiveValue::doubleValue(CSSUnitType unitType) const
{
    return doubleValueInternal(unitType).value_or(0);
}

double CSSPrimitiveValue::doubleValue() const
{
    return isCalculated() ? m_value.calc->doubleValue({ }) : m_value.number;
}

double CSSPrimitiveValue::doubleValueDividingBy100IfPercentage() const
{
    if (isCalculated())
        return m_value.calc->primitiveType() == CSSUnitType::Percentage ? m_value.calc->doubleValue({ }) / 100.0 : m_value.calc->doubleValue({ });
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

std::optional<double> CSSPrimitiveValue::doubleValueInternal(CSSUnitType requestedUnitType) const
{
    if (!isValidCSSUnitTypeForDoubleConversion(primitiveUnitType()) || !isValidCSSUnitTypeForDoubleConversion(requestedUnitType))
        return std::nullopt;

    CSSUnitType sourceUnitType = primitiveType();
    if (requestedUnitType == sourceUnitType || requestedUnitType == CSSUnitType::Dimension)
        return doubleValue();

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
        if (targetUnitType == CSSUnitType::Unknown)
            return std::nullopt;
    }

    if (sourceUnitType == CSSUnitType::Number || sourceUnitType == CSSUnitType::Integer) {
        // Cannot convert between numbers and percent.
        if (targetCategory == CSSUnitCategory::Percent)
            return std::nullopt;
        // We interpret conversion from CSSUnitType::CSS_NUMBER in the same way as CSSParser::validUnit() while using non-strict mode.
        sourceUnitType = canonicalUnitTypeForCategory(targetCategory);
        if (sourceUnitType == CSSUnitType::Unknown)
            return std::nullopt;
    }

    double convertedValue = doubleValue();

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
    case CSSUnitType::Attribute:
    case CSSUnitType::FontFamily:
    case CSSUnitType::Uri:
        return m_value.string;
    case CSSUnitType::ValueID:
        return nameString(m_value.valueID);
    case CSSUnitType::PropertyID:
        return nameString(m_value.propertyID);
    default:
        return String();
    }
}

static NEVER_INLINE ASCIILiteral formatNonfiniteCSSNumberValuePrefix(double number)
{
    if (number == std::numeric_limits<double>::infinity())
        return "infinity"_s;
    if (number == -std::numeric_limits<double>::infinity())
        return "-infinity"_s;
    ASSERT(std::isnan(number));
    return "NaN"_s;
}

static NEVER_INLINE void formatNonfiniteCSSNumberValue(StringBuilder& builder, double number, ASCIILiteral suffix)
{
    return builder.append(formatNonfiniteCSSNumberValuePrefix(number), suffix.isEmpty() ? ""_s : " * 1"_s, suffix);
}

static NEVER_INLINE String formatNonfiniteCSSNumberValue(double number, ASCIILiteral suffix)
{
    return makeString(formatNonfiniteCSSNumberValuePrefix(number), suffix.isEmpty() ? ""_s : " * 1"_s, suffix);
}

NEVER_INLINE void formatCSSNumberValue(StringBuilder& builder, double value, ASCIILiteral suffix)
{
    if (!std::isfinite(value))
        return formatNonfiniteCSSNumberValue(builder, value, suffix);
    return builder.append(FormattedCSSNumber::create(value), suffix);
}

NEVER_INLINE String formatCSSNumberValue(double value, ASCIILiteral suffix)
{
    if (!std::isfinite(value))
        return formatNonfiniteCSSNumberValue(value, suffix);
    return makeString(FormattedCSSNumber::create(value), suffix);
}

NEVER_INLINE String CSSPrimitiveValue::formatNumberValue(ASCIILiteral suffix) const
{
    return formatCSSNumberValue(m_value.number, suffix);
}

NEVER_INLINE String CSSPrimitiveValue::formatIntegerValue(ASCIILiteral suffix) const
{
    if (!std::isfinite(m_value.number))
        return formatNonfiniteCSSNumberValue(m_value.number, suffix);
    return makeString(m_value.number, suffix);
}

ASCIILiteral CSSPrimitiveValue::unitTypeString(CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::CapHeight: return "cap"_s;
    case CSSUnitType::CharacterWidth: return "ch"_s;
    case CSSUnitType::Centimeter: return "cm"_s;
    case CSSUnitType::ContainerQueryBlock: return "cqb"_s;
    case CSSUnitType::ContainerQueryHeight: return "cqh"_s;
    case CSSUnitType::ContainerQueryInline: return "cqi"_s;
    case CSSUnitType::ContainerQueryMaximum: return "cqmax"_s;
    case CSSUnitType::ContainerQueryMinimum: return "cqmin"_s;
    case CSSUnitType::ContainerQueryWidth: return "cqw"_s;
    case CSSUnitType::Degree: return "deg"_s;
    case CSSUnitType::DotsPerCentimeter: return "dpcm"_s;
    case CSSUnitType::DotsPerInch: return "dpi"_s;
    case CSSUnitType::DotsPerPixel: return "dppx"_s;
    case CSSUnitType::DynamicViewportBlock: return "dvb"_s;
    case CSSUnitType::DynamicViewportHeight: return "dvh"_s;
    case CSSUnitType::DynamicViewportInline: return "dvi"_s;
    case CSSUnitType::DynamicViewportMaximum: return "dvmax"_s;
    case CSSUnitType::DynamicViewportMinimum: return "dvmin"_s;
    case CSSUnitType::DynamicViewportWidth: return "dvw"_s;
    case CSSUnitType::Em: return "em"_s;
    case CSSUnitType::Ex: return "ex"_s;
    case CSSUnitType::Fraction: return "fr"_s;
    case CSSUnitType::Gradian: return "grad"_s;
    case CSSUnitType::Hertz: return "hz"_s;
    case CSSUnitType::IdeographicCharacter: return "ic"_s;
    case CSSUnitType::Inch: return "in"_s;
    case CSSUnitType::Kilohertz: return "khz"_s;
    case CSSUnitType::LineHeight: return "lh"_s;
    case CSSUnitType::LargeViewportBlock: return "lvb"_s;
    case CSSUnitType::LargeViewportHeight: return "lvh"_s;
    case CSSUnitType::LargeViewportInline: return "lvi"_s;
    case CSSUnitType::LargeViewportMaximum: return "lvmax"_s;
    case CSSUnitType::LargeViewportMinimum: return "lvmin"_s;
    case CSSUnitType::LargeViewportWidth: return "lvw"_s;
    case CSSUnitType::Millimeter: return "mm"_s;
    case CSSUnitType::Millisecond: return "ms"_s;
    case CSSUnitType::Pica: return "pc"_s;
    case CSSUnitType::Percentage: return "%"_s;
    case CSSUnitType::Point: return "pt"_s;
    case CSSUnitType::Pixel: return "px"_s;
    case CSSUnitType::Quarter: return "q"_s;
    case CSSUnitType::Radian: return "rad"_s;
    case CSSUnitType::RootCapHeight: return "rcap"_s;
    case CSSUnitType::RootCharacterWidth: return "rch"_s;
    case CSSUnitType::RootEm: return "rem"_s;
    case CSSUnitType::RootEx: return "rex"_s;
    case CSSUnitType::RootIdeographicCharacter: return "ric"_s;
    case CSSUnitType::RootLineHeight: return "rlh"_s;
    case CSSUnitType::Second: return "s"_s;
    case CSSUnitType::SmallViewportBlock: return "svb"_s;
    case CSSUnitType::SmallViewportHeight: return "svh"_s;
    case CSSUnitType::SmallViewportInline: return "svi"_s;
    case CSSUnitType::SmallViewportMaximum: return "svmax"_s;
    case CSSUnitType::SmallViewportMinimum: return "svmin"_s;
    case CSSUnitType::SmallViewportWidth: return "svw"_s;
    case CSSUnitType::Turn: return "turn"_s;
    case CSSUnitType::ViewportBlock: return "vb"_s;
    case CSSUnitType::ViewportHeight: return "vh"_s;
    case CSSUnitType::ViewportInline: return "vi"_s;
    case CSSUnitType::ViewportMaximum: return "vmax"_s;
    case CSSUnitType::ViewportMinimum: return "vmin"_s;
    case CSSUnitType::ViewportWidth: return "vw"_s;
    case CSSUnitType::MultiplicationFactor: return "x"_s;

    case CSSUnitType::Anchor:
    case CSSUnitType::Attribute:
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::CalcPercentageWithNumber:
    case CSSUnitType::Dimension:
    case CSSUnitType::FontFamily:
    case CSSUnitType::Ident:
    case CSSUnitType::Integer:
    case CSSUnitType::Number:
    case CSSUnitType::PropertyID:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::RgbColor:
    case CSSUnitType::String:
    case CSSUnitType::Unknown:
    case CSSUnitType::UnresolvedColor:
    case CSSUnitType::Uri:
    case CSSUnitType::ValueID:
    case CSSUnitType::CustomIdent:
        return ""_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

ALWAYS_INLINE String CSSPrimitiveValue::serializeInternal() const
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::Centimeter:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryMaximum:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::Degree:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::DynamicViewportMaximum:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Fraction:
    case CSSUnitType::Gradian:
    case CSSUnitType::Hertz:
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::Inch:
    case CSSUnitType::Kilohertz:
    case CSSUnitType::LineHeight:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Millisecond:
    case CSSUnitType::Number:
    case CSSUnitType::Pica:
    case CSSUnitType::Percentage:
    case CSSUnitType::Point:
    case CSSUnitType::Pixel:
    case CSSUnitType::Quarter:
    case CSSUnitType::Radian:
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::Second:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::Turn:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::MultiplicationFactor:
        return formatNumberValue(unitTypeString(type));

    case CSSUnitType::Anchor:
        return m_value.anchor->customCSSText();
    case CSSUnitType::Attribute:
        return makeString("attr("_s, m_value.string, ')');
    case CSSUnitType::Calc:
        return m_value.calc->cssText();
    case CSSUnitType::Dimension:
        // FIXME: This isn't correct.
        return formatNumberValue(""_s);
    case CSSUnitType::FontFamily:
        return serializeFontFamily(m_value.string);
    case CSSUnitType::Integer:
        return formatIntegerValue(""_s);
    case CSSUnitType::QuirkyEm:
        return formatNumberValue("em"_s);
    case CSSUnitType::RgbColor:
        return serializationForCSS(color());
    case CSSUnitType::String:
        return serializeString(m_value.string);
    case CSSUnitType::UnresolvedColor:
        return m_value.unresolvedColor->serializationForCSS();
    case CSSUnitType::Uri:
        return serializeURL(m_value.string);
    case CSSUnitType::CustomIdent: {
        StringBuilder builder;
        serializeIdentifier(m_value.string, builder);
        return builder.toString();
    }

    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::CalcPercentageWithNumber:
    case CSSUnitType::Ident:
    case CSSUnitType::PropertyID:
    case CSSUnitType::Unknown:
    case CSSUnitType::ValueID:
        break;
    }
    ASSERT_NOT_REACHED();
    return String();
}

String CSSPrimitiveValue::customCSSText() const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::Unknown:
        return String();
    case CSSUnitType::ValueID:
        return nameStringForSerialization(m_value.valueID);
    case CSSUnitType::PropertyID:
        return nameString(m_value.propertyID);
    default:
        auto& map = serializedPrimitiveValues();
        ASSERT(map.contains(this) == m_hasCachedCSSText);
        if (m_hasCachedCSSText)
            return map.get(this);
        String serializedValue = serializeInternal();
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
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::MultiplicationFactor:
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
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::DynamicViewportMaximum:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::Fraction:
    case CSSUnitType::Quarter:
    case CSSUnitType::LineHeight:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::Dimension:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryMaximum:
        return m_value.number == other.m_value.number;
    case CSSUnitType::PropertyID:
        return m_value.propertyID == other.m_value.propertyID;
    case CSSUnitType::ValueID:
        return m_value.valueID == other.m_value.valueID;
    case CSSUnitType::String:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::Uri:
    case CSSUnitType::Attribute:
    case CSSUnitType::FontFamily:
        return equal(m_value.string, other.m_value.string);
    case CSSUnitType::RgbColor:
        return color() == other.color();
    case CSSUnitType::Calc:
        return m_value.calc->equals(*other.m_value.calc);
    case CSSUnitType::UnresolvedColor:
        return m_value.unresolvedColor->equals(*other.m_value.unresolvedColor);
    case CSSUnitType::Anchor:
        return m_value.anchor->equals(*other.m_value.anchor);
    case CSSUnitType::Ident:
    case CSSUnitType::CalcPercentageWithNumber:
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
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::MultiplicationFactor:
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
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::DynamicViewportMaximum:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::Fraction:
    case CSSUnitType::Quarter:
    case CSSUnitType::LineHeight:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::Dimension:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryMaximum:
        add(hasher, m_value.number);
        break;
    case CSSUnitType::PropertyID:
        add(hasher, m_value.propertyID);
        break;
    case CSSUnitType::ValueID:
        add(hasher, m_value.valueID);
        break;
    case CSSUnitType::String:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::Uri:
    case CSSUnitType::Attribute:
    case CSSUnitType::FontFamily:
        add(hasher, String { m_value.string });
        break;
    case CSSUnitType::RgbColor:
        add(hasher, color());
        break;
    case CSSUnitType::Calc:
        add(hasher, m_value.calc);
        break;
    case CSSUnitType::UnresolvedColor:
        add(hasher, m_value.unresolvedColor);
        break;
    case CSSUnitType::Anchor:
        add(hasher, m_value.anchor);
        break;
    case CSSUnitType::Ident:
    case CSSUnitType::CalcPercentageWithNumber:
    case CSSUnitType::CalcPercentageWithLength:
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

// https://drafts.css-houdini.org/css-properties-values-api/#dependency-cycles
void CSSPrimitiveValue::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
    case CSSUnitType::RootEm:
        dependencies.rootProperties.appendIfNotContains(CSSPropertyFontSize);
        break;
    case CSSUnitType::RootLineHeight:
        dependencies.rootProperties.appendIfNotContains(CSSPropertyFontSize);
        dependencies.rootProperties.appendIfNotContains(CSSPropertyLineHeight);
        break;
    case CSSUnitType::Em:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Ex:
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::IdeographicCharacter:
        dependencies.properties.appendIfNotContains(CSSPropertyFontSize);
        break;
    case CSSUnitType::LineHeight:
        dependencies.properties.appendIfNotContains(CSSPropertyFontSize);
        dependencies.properties.appendIfNotContains(CSSPropertyLineHeight);
        break;
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryMaximum:
        dependencies.containerDimensions = true;
        break;
    case CSSUnitType::Calc:
        m_value.calc->collectComputedStyleDependencies(dependencies);
        break;
    case CSSUnitType::Anchor:
        m_value.anchor->collectComputedStyleDependencies(dependencies);
        break;
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::DynamicViewportMaximum:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportInline:
        dependencies.viewportDimensions = true;
        break;
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::Degree:
    case CSSUnitType::Radian:
    case CSSUnitType::Gradian:
    case CSSUnitType::Turn:
    case CSSUnitType::Millisecond:
    case CSSUnitType::Second:
    case CSSUnitType::Hertz:
    case CSSUnitType::Kilohertz:
    case CSSUnitType::Dimension:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::MultiplicationFactor:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::Fraction:
    case CSSUnitType::Quarter:
    case CSSUnitType::Unknown:
    case CSSUnitType::String:
    case CSSUnitType::FontFamily:
    case CSSUnitType::Uri:
    case CSSUnitType::Ident:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::Attribute:
    case CSSUnitType::RgbColor:
    case CSSUnitType::CalcPercentageWithNumber:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::UnresolvedColor:
    case CSSUnitType::PropertyID:
    case CSSUnitType::ValueID:
        break;
    }
}

bool CSSPrimitiveValue::convertingToLengthHasRequiredConversionData(int lengthConversion, const CSSToLengthConversionData& conversionData) const
{
    // FIXME: We should probably make CSSPrimitiveValue::computeLengthDouble and
    // CSSPrimitiveValue::computeNonCalcLengthDouble (which has the style assertion)
    // return std::optional<double> instead of having this check here.

    bool isFixedNumberConversion = lengthConversion & (FixedIntegerConversion | FixedFloatConversion);
    if (!isFixedNumberConversion)
        return true;

    auto dependencies = computedStyleDependencies();
    if (!dependencies.rootProperties.isEmpty() && !conversionData.rootStyle())
        return false;

    if (!dependencies.properties.isEmpty() && !conversionData.style())
        return false;

    if (dependencies.containerDimensions && !conversionData.elementForContainerUnitResolution())
        return false;

    if (dependencies.viewportDimensions && !conversionData.renderView())
        return false;

    return true;
}

IterationStatus CSSPrimitiveValue::customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
{
    if (auto* calc = cssCalcValue()) {
        if (func(const_cast<CSSCalcValue&>(*calc)) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

} // namespace WebCore
