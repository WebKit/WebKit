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
    case CSSUnitType::CSS_ANCHOR:
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
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
    case CSSUnitType::CSS_RGBCOLOR:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
    case CSSUnitType::CSS_URI:
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
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_FONT_FAMILY:
        return true;
    case CSSUnitType::CSS_ANCHOR:
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
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
    case CSSUnitType::CSS_RGBCOLOR:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
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
    // FIXME: Use a switch statement here.

    if (primitiveUnitType() == CSSUnitType::CSS_PROPERTY_ID || primitiveUnitType() == CSSUnitType::CSS_VALUE_ID || primitiveUnitType() == CSSUnitType::CustomIdent)
        return CSSUnitType::CSS_IDENT;

    // Web-exposed content expects font family values to have CSSUnitType::CSS_STRING primitive type
    // so we need to map our internal CSSUnitType::CSS_FONT_FAMILY type here.
    if (primitiveUnitType() == CSSUnitType::CSS_FONT_FAMILY)
        return CSSUnitType::CSS_STRING;

    if (!isCalculated())
        return primitiveUnitType();

    switch (m_value.calc->category()) {
    case CalculationCategory::Number:
        return CSSUnitType::CSS_NUMBER;
    case CalculationCategory::Percent:
        return CSSUnitType::CSS_PERCENTAGE;
    case CalculationCategory::PercentNumber:
        return CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER;
    case CalculationCategory::PercentLength:
        return CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH;
    case CalculationCategory::Length:
    case CalculationCategory::Angle:
    case CalculationCategory::Time:
    case CalculationCategory::Frequency:
    case CalculationCategory::Resolution:
        return m_value.calc->primitiveType();
    case CalculationCategory::Other:
        return CSSUnitType::CSS_UNKNOWN;
    }
    return CSSUnitType::CSS_UNKNOWN;
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSPropertyID propertyID)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_PROPERTY_ID);
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
    setPrimitiveUnitType(CSSUnitType::CSS_RGBCOLOR);
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
    setPrimitiveUnitType(CSSUnitType::CSS_VALUE_ID);
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
    setPrimitiveUnitType(CSSUnitType::CSS_CALC);
    m_value.calc = &value.leakRef();
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSUnresolvedColor unresolvedColor)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_UNRESOLVED_COLOR);
    m_value.unresolvedColor = new CSSUnresolvedColor(WTFMove(unresolvedColor));
}

CSSPrimitiveValue::CSSPrimitiveValue(Ref<CSSAnchorValue> value)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_ANCHOR);
    m_value.anchor = &value.leakRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_FONT_FAMILY:
        if (m_value.string)
            m_value.string->deref();
        break;
    case CSSUnitType::CSS_ANCHOR:
        m_value.anchor->deref();
        break;
    case CSSUnitType::CSS_CALC:
        m_value.calc->deref();
        break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        ASSERT_NOT_REACHED();
        break;
    case CSSUnitType::CSS_RGBCOLOR:
        std::destroy_at(reinterpret_cast<Color*>(&m_value.colorAsInteger));
        break;
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
        delete m_value.unresolvedColor;
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
    return adoptRef(*new CSSPrimitiveValue(value, CSSUnitType::CSS_NUMBER));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(double value, CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::CSS_NUMBER:
        if (auto* result = valueFromPool(staticCSSValuePool->m_numberValues, value))
            return *result;
        break;
    case CSSUnitType::CSS_PERCENTAGE:
        if (auto* result = valueFromPool(staticCSSValuePool->m_percentValues, value))
            return *result;
        break;
    case CSSUnitType::CSS_PX:
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
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value), CSSUnitType::CSS_STRING));
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
        return create(length.value(), CSSUnitType::CSS_PX);
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
        return create(length.percent(), CSSUnitType::CSS_PERCENTAGE);
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
        return create(adjustFloatForAbsoluteZoom(length.value(), style), CSSUnitType::CSS_PX);
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
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value), CSSUnitType::CSS_ATTR));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createCustomIdent(String value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value), CSSUnitType::CustomIdent));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createFontFamily(String value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value), CSSUnitType::CSS_FONT_FAMILY));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createInteger(double value)
{
    return adoptRef(*new CSSPrimitiveValue(value, CSSUnitType::CSS_INTEGER));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createURI(String value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value), CSSUnitType::CSS_URI));
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
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_REM: {
        ASSERT(fontCascadeForUnit);
        auto& fontDescription = fontCascadeForUnit->fontDescription();
        return ((propertyToCompute == CSSPropertyFontSize) ? fontDescription.specifiedSize() : fontDescription.computedSize()) * value;
    }
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_REX: {
        ASSERT(fontCascadeForUnit);
        auto& fontMetrics = fontCascadeForUnit->metricsOfPrimaryFont();
        if (fontMetrics.xHeight())
            return fontMetrics.xHeight().value() * value;
        auto& fontDescription = fontCascadeForUnit->fontDescription();
        return ((propertyToCompute == CSSPropertyFontSize) ? fontDescription.specifiedSize() : fontDescription.computedSize()) / 2.0 * value;
    }
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_RCAP: {
        ASSERT(fontCascadeForUnit);
        auto& fontMetrics = fontCascadeForUnit->metricsOfPrimaryFont();
        if (fontMetrics.capHeight())
            return fontMetrics.capHeight().value() * value;
        return fontMetrics.intAscent() * value;
    }
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_RCH:
        ASSERT(fontCascadeForUnit);
        return fontCascadeForUnit->zeroWidth() * value;
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RIC:
        ASSERT(fontCascadeForUnit);
        return fontCascadeForUnit->metricsOfPrimaryFont().ideogramWidth().value_or(0) * value;
    case CSSUnitType::CSS_PX:
        return value;
    case CSSUnitType::CSS_CM:
        return cssPixelsPerInch / cmPerInch * value;
    case CSSUnitType::CSS_MM:
        return cssPixelsPerInch / mmPerInch * value;
    case CSSUnitType::CSS_Q:
        return cssPixelsPerInch / QPerInch * value;
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_RLH:
        ASSERT_NOT_REACHED();
        return -1.0;
    case CSSUnitType::CSS_IN:
        return cssPixelsPerInch * value;
    case CSSUnitType::CSS_PT:
        return cssPixelsPerInch / 72.0 * value;
    case CSSUnitType::CSS_PC:
        // 1 pc == 12 pt
        return cssPixelsPerInch * 12.0 / 72.0 * value;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
        ASSERT_NOT_REACHED();
        return -1.0;
    case CSSUnitType::CSS_VH:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().height() / 100.0 * value : 0;
    case CSSUnitType::CSS_VW:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().width() / 100.0 * value : 0;
    case CSSUnitType::CSS_VMAX:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().maxDimension() / 100.0 * value : value;
    case CSSUnitType::CSS_VMIN:
        return renderView ? renderView->sizeForCSSDefaultViewportUnits().minDimension() / 100.0 * value : value;
    case CSSUnitType::CSS_VB:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSDefaultViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::CSS_VI:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, renderView->sizeForCSSDefaultViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::CSS_SVH:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().height() / 100.0 * value : 0;
    case CSSUnitType::CSS_SVW:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().width() / 100.0 * value : 0;
    case CSSUnitType::CSS_SVMAX:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().maxDimension() / 100.0 * value : value;
    case CSSUnitType::CSS_SVMIN:
        return renderView ? renderView->sizeForCSSSmallViewportUnits().minDimension() / 100.0 * value : value;
    case CSSUnitType::CSS_SVB:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSSmallViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::CSS_SVI:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, renderView->sizeForCSSSmallViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::CSS_LVH:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().height() / 100.0 * value : 0;
    case CSSUnitType::CSS_LVW:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().width() / 100.0 * value : 0;
    case CSSUnitType::CSS_LVMAX:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().maxDimension() / 100.0 * value : value;
    case CSSUnitType::CSS_LVMIN:
        return renderView ? renderView->sizeForCSSLargeViewportUnits().minDimension() / 100.0 * value : value;
    case CSSUnitType::CSS_LVB:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSLargeViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::CSS_LVI:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, renderView->sizeForCSSLargeViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::CSS_DVH:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().height() / 100.0 * value : 0;
    case CSSUnitType::CSS_DVW:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().width() / 100.0 * value : 0;
    case CSSUnitType::CSS_DVMAX:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().maxDimension() / 100.0 * value : value;
    case CSSUnitType::CSS_DVMIN:
        return renderView ? renderView->sizeForCSSDynamicViewportUnits().minDimension() / 100.0 * value : value;
    case CSSUnitType::CSS_DVB:
        return renderView ? lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, renderView->sizeForCSSDynamicViewportUnits(), *renderView) / 100.0 * value : 0;
    case CSSUnitType::CSS_DVI:
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
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
        // FIXME: We have a bug right now where the zoom will be applied twice to EX units.
        // We really need to compute EX using fontMetrics for the original specifiedSize and not use
        // our actual constructed rendering font.
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), &conversionData.fontCascadeForFontUnits());
        break;

    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), conversionData.rootStyle() ? &conversionData.rootStyle()->fontCascade() : &conversionData.fontCascadeForFontUnits());
        break;

    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute());
        break;

    case CSSUnitType::CSS_VH:
        return value * conversionData.defaultViewportFactor().height();

    case CSSUnitType::CSS_VW:
        return value * conversionData.defaultViewportFactor().width();

    case CSSUnitType::CSS_VMAX:
        return value * conversionData.defaultViewportFactor().maxDimension();

    case CSSUnitType::CSS_VMIN:
        return value * conversionData.defaultViewportFactor().minDimension();

    case CSSUnitType::CSS_VB:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.defaultViewportFactor(), conversionData.style());

    case CSSUnitType::CSS_VI:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.defaultViewportFactor(), conversionData.style());

    case CSSUnitType::CSS_SVH:
        return value * conversionData.smallViewportFactor().height();

    case CSSUnitType::CSS_SVW:
        return value * conversionData.smallViewportFactor().width();

    case CSSUnitType::CSS_SVMAX:
        return value * conversionData.smallViewportFactor().maxDimension();

    case CSSUnitType::CSS_SVMIN:
        return value * conversionData.smallViewportFactor().minDimension();

    case CSSUnitType::CSS_SVB:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.smallViewportFactor(), conversionData.style());

    case CSSUnitType::CSS_SVI:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.smallViewportFactor(), conversionData.style());

    case CSSUnitType::CSS_LVH:
        return value * conversionData.largeViewportFactor().height();

    case CSSUnitType::CSS_LVW:
        return value * conversionData.largeViewportFactor().width();

    case CSSUnitType::CSS_LVMAX:
        return value * conversionData.largeViewportFactor().maxDimension();

    case CSSUnitType::CSS_LVMIN:
        return value * conversionData.largeViewportFactor().minDimension();

    case CSSUnitType::CSS_LVB:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.largeViewportFactor(), conversionData.style());

    case CSSUnitType::CSS_LVI:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.largeViewportFactor(), conversionData.style());

    case CSSUnitType::CSS_DVH:
        return value * conversionData.dynamicViewportFactor().height();

    case CSSUnitType::CSS_DVW:
        return value * conversionData.dynamicViewportFactor().width();

    case CSSUnitType::CSS_DVMAX:
        return value * conversionData.dynamicViewportFactor().maxDimension();

    case CSSUnitType::CSS_DVMIN:
        return value * conversionData.dynamicViewportFactor().minDimension();

    case CSSUnitType::CSS_DVB:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.dynamicViewportFactor(), conversionData.style());

    case CSSUnitType::CSS_DVI:
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.dynamicViewportFactor(), conversionData.style());

    case CSSUnitType::CSS_LH:
        if (conversionData.computingLineHeight() || conversionData.computingFontSize()) {
            // Try to get the parent's computed line-height, or fall back to the initial line-height of this element's font spacing.
            value *= conversionData.parentStyle() ? conversionData.parentStyle()->computedLineHeight() : conversionData.fontCascadeForFontUnits().metricsOfPrimaryFont().intLineSpacing();
        } else
            value *= conversionData.computedLineHeightForFontUnits();
        break;

    case CSSUnitType::CSS_CQW: {
        if (auto resolvedValue = resolveContainerUnit(CQ::Axis::Width))
            return *resolvedValue;
        return computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_SVW, value);
    }

    case CSSUnitType::CSS_CQH: {
        if (auto resolvedValue = resolveContainerUnit(CQ::Axis::Height))
            return *resolvedValue;
        return computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_SVH, value);
    }

    case CSSUnitType::CSS_CQI: {
        if (auto resolvedValue = resolveContainerUnit(conversionData.style()->isHorizontalWritingMode() ? CQ::Axis::Width : CQ::Axis::Height))
            return *resolvedValue;
        return computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_SVI, value);
    }

    case CSSUnitType::CSS_CQB: {
        if (auto resolvedValue = resolveContainerUnit(conversionData.style()->isHorizontalWritingMode() ? CQ::Axis::Height : CQ::Axis::Width))
            return *resolvedValue;
        return computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_SVB, value);
    }

    case CSSUnitType::CSS_CQMAX:
        return std::max(computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_CQB, value), computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_CQI, value));

    case CSSUnitType::CSS_CQMIN:
        return std::min(computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_CQB, value), computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_CQI, value));

    case CSSUnitType::CSS_RLH:
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
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_DPPX:
        return 1.0;

    case CSSUnitType::CSS_X:
        // This is semantically identical to (canonical) dppx
        return 1.0;

    case CSSUnitType::CSS_CM:
        return cssPixelsPerInch / cmPerInch;

    case CSSUnitType::CSS_DPCM:
        return cmPerInch / cssPixelsPerInch; // (2.54 cm/in)

    case CSSUnitType::CSS_MM:
        return cssPixelsPerInch / mmPerInch;

    case CSSUnitType::CSS_Q:
        return cssPixelsPerInch / QPerInch;

    case CSSUnitType::CSS_IN:
        return cssPixelsPerInch;

    case CSSUnitType::CSS_DPI:
        return 1 / cssPixelsPerInch;

    case CSSUnitType::CSS_PT:
        return cssPixelsPerInch / 72.0;

    case CSSUnitType::CSS_PC:
        return cssPixelsPerInch * 12.0 / 72.0; // 1 pc == 12 pt

    case CSSUnitType::CSS_RAD:
        return degreesPerRadianDouble;

    case CSSUnitType::CSS_GRAD:
        return degreesPerGradientDouble;

    case CSSUnitType::CSS_TURN:
        return degreesPerTurnDouble;

    case CSSUnitType::CSS_MS:
        return 0.001;
    case CSSUnitType::CSS_KHZ:
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
        return m_value.calc->primitiveType() == CSSUnitType::CSS_PERCENTAGE ? m_value.calc->doubleValue({ }) / 100.0 : m_value.calc->doubleValue({ });
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
    if (requestedUnitType == sourceUnitType || requestedUnitType == CSSUnitType::CSS_DIMENSION)
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
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_URI:
        return m_value.string;
    case CSSUnitType::CSS_VALUE_ID:
        return nameString(m_value.valueID);
    case CSSUnitType::CSS_PROPERTY_ID:
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

    case CSSUnitType::CSS_ANCHOR:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_RGBCOLOR:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_VALUE_ID:
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

    case CSSUnitType::CSS_ANCHOR:
        return m_value.anchor->customCSSText();
    case CSSUnitType::CSS_ATTR:
        return makeString("attr("_s, m_value.string, ')');
    case CSSUnitType::CSS_CALC:
        return m_value.calc->cssText();
    case CSSUnitType::CSS_DIMENSION:
        // FIXME: This isn't correct.
        return formatNumberValue(""_s);
    case CSSUnitType::CSS_FONT_FAMILY:
        return serializeFontFamily(m_value.string);
    case CSSUnitType::CSS_INTEGER:
        return formatIntegerValue(""_s);
    case CSSUnitType::CSS_QUIRKY_EM:
        return formatNumberValue("em"_s);
    case CSSUnitType::CSS_RGBCOLOR:
        return serializationForCSS(color());
    case CSSUnitType::CSS_STRING:
        return serializeString(m_value.string);
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
        return m_value.unresolvedColor->serializationForCSS();
    case CSSUnitType::CSS_URI:
        return serializeURL(m_value.string);
    case CSSUnitType::CustomIdent: {
        StringBuilder builder;
        serializeIdentifier(m_value.string, builder);
        return builder.toString();
    }

    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_VALUE_ID:
        break;
    }
    ASSERT_NOT_REACHED();
    return String();
}

String CSSPrimitiveValue::customCSSText() const
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
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_FONT_FAMILY:
        return equal(m_value.string, other.m_value.string);
    case CSSUnitType::CSS_RGBCOLOR:
        return color() == other.color();
    case CSSUnitType::CSS_CALC:
        return m_value.calc->equals(*other.m_value.calc);
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
        return m_value.unresolvedColor->equals(*other.m_value.unresolvedColor);
    case CSSUnitType::CSS_ANCHOR:
        return m_value.anchor->equals(*other.m_value.anchor);
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
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
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_FONT_FAMILY:
        add(hasher, String { m_value.string });
        break;
    case CSSUnitType::CSS_RGBCOLOR:
        add(hasher, color());
        break;
    case CSSUnitType::CSS_CALC:
        add(hasher, m_value.calc);
        break;
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
        add(hasher, m_value.unresolvedColor);
        break;
    case CSSUnitType::CSS_ANCHOR:
        add(hasher, m_value.anchor);
        break;
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

// https://drafts.css-houdini.org/css-properties-values-api/#dependency-cycles
void CSSPrimitiveValue::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_REM:
        dependencies.rootProperties.appendIfNotContains(CSSPropertyFontSize);
        break;
    case CSSUnitType::CSS_RLH:
        dependencies.rootProperties.appendIfNotContains(CSSPropertyFontSize);
        dependencies.rootProperties.appendIfNotContains(CSSPropertyLineHeight);
        break;
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
        dependencies.properties.appendIfNotContains(CSSPropertyFontSize);
        break;
    case CSSUnitType::CSS_LH:
        dependencies.properties.appendIfNotContains(CSSPropertyFontSize);
        dependencies.properties.appendIfNotContains(CSSPropertyLineHeight);
        break;
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        dependencies.containerDimensions = true;
        break;
    case CSSUnitType::CSS_CALC:
        m_value.calc->collectComputedStyleDependencies(dependencies);
        break;
    case CSSUnitType::CSS_ANCHOR:
        m_value.anchor->collectComputedStyleDependencies(dependencies);
        break;
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
        dependencies.viewportDimensions = true;
        break;
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_RGBCOLOR:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_VALUE_ID:
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
