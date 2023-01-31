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

#include "CSSBasicShapes.h"
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
#include "Counter.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "FontCascade.h"
#include "Length.h"
#include "NodeRenderStyle.h"
#include "Pair.h"
#include "Rect.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

static inline bool isValidCSSUnitTypeForDoubleConversion(CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CHS:
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
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_EXS:
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
    case CSSUnitType::CSS_LHS:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_RLHS:
    case CSSUnitType::CSS_QUIRKY_EMS:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_REMS:
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
    case CSSUnitType::CSS_COUNTER:
    case CSSUnitType::CSS_COUNTER_NAME:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_PAIR:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_QUAD:
    case CSSUnitType::CSS_RECT:
    case CSSUnitType::CSS_RGBCOLOR:
    case CSSUnitType::CSS_SHAPE:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNICODE_RANGE:
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
    case CSSUnitType::CSS_COUNTER_NAME:
    case CSSUnitType::CSS_FONT_FAMILY:
        return true;
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CHS:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_COUNTER:
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
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_EXS:
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
    case CSSUnitType::CSS_PAIR:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LHS:
    case CSSUnitType::CSS_RLHS:
    case CSSUnitType::CSS_QUAD:
    case CSSUnitType::CSS_QUIRKY_EMS:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_RECT:
    case CSSUnitType::CSS_REMS:
    case CSSUnitType::CSS_RGBCOLOR:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SHAPE:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_UNICODE_RANGE:
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

    if (primitiveUnitType() != CSSUnitType::CSS_CALC)
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

CSSPrimitiveValue::CSSPrimitiveValue(const Color& color)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_RGBCOLOR);
    static_assert(sizeof(m_value.colorAsInteger) == sizeof(color));
    new (reinterpret_cast<Color*>(&m_value.colorAsInteger)) Color(color);
}

CSSPrimitiveValue::CSSPrimitiveValue(StaticCSSValueTag, CSSValueID valueID)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_VALUE_ID);
    m_value.valueID = valueID;
    makeStatic();
}

CSSPrimitiveValue::CSSPrimitiveValue(StaticCSSValueTag, const Color& color)
    : CSSPrimitiveValue(color)
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

CSSPrimitiveValue::CSSPrimitiveValue(Ref<Counter>&& counter)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_COUNTER);
    m_value.counter = &counter.leakRef();
}

CSSPrimitiveValue::CSSPrimitiveValue(Ref<Rect>&& rect)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_RECT);
    m_value.rect = &rect.leakRef();
}

CSSPrimitiveValue::CSSPrimitiveValue(Ref<Quad>&& quad)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_QUAD);
    m_value.quad = &quad.leakRef();
}

CSSPrimitiveValue::CSSPrimitiveValue(Ref<Pair>&& pair)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_PAIR);
    m_value.pair = &pair.leakRef();
}

CSSPrimitiveValue::CSSPrimitiveValue(Ref<CSSBasicShape>&& shape)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_SHAPE);
    m_value.shape = &shape.leakRef();
}

CSSPrimitiveValue::CSSPrimitiveValue(Ref<CSSCalcValue>&& value)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_CALC);
    m_value.calc = &value.leakRef();
}

CSSPrimitiveValue::CSSPrimitiveValue(Ref<CSSUnresolvedColor>&& unresolvedColor)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_UNRESOLVED_COLOR);
    m_value.unresolvedColor = &unresolvedColor.leakRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_COUNTER_NAME:
    case CSSUnitType::CSS_FONT_FAMILY:
        if (m_value.string)
            m_value.string->deref();
        break;
    case CSSUnitType::CSS_COUNTER:
        m_value.counter->deref();
        break;
    case CSSUnitType::CSS_RECT:
        m_value.rect->deref();
        break;
    case CSSUnitType::CSS_QUAD:
        m_value.quad->deref();
        break;
    case CSSUnitType::CSS_PAIR:
        m_value.pair->deref();
        break;
    case CSSUnitType::CSS_CALC:
        m_value.calc->deref();
        break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        ASSERT_NOT_REACHED();
        break;
    case CSSUnitType::CSS_SHAPE:
        m_value.shape->deref();
        break;
    case CSSUnitType::CSS_RGBCOLOR:
        std::destroy_at(reinterpret_cast<Color*>(&m_value.colorAsInteger));
        break;
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
        m_value.unresolvedColor->deref();
        break;
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_QUIRKY_EMS:
    case CSSUnitType::CSS_EXS:
    case CSSUnitType::CSS_REMS:
    case CSSUnitType::CSS_CHS:
    case CSSUnitType::CSS_IC:
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
    case CSSUnitType::CSS_LHS:
    case CSSUnitType::CSS_RLHS:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_UNICODE_RANGE:
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

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(double value, CSSUnitType type)
{
    if (auto values = [type]() -> LazyNeverDestroyed<CSSPrimitiveValue>* {
        switch (type) {
        case CSSUnitType::CSS_NUMBER:
            return staticCSSValuePool->m_numberValues;
        case CSSUnitType::CSS_PERCENTAGE:
            return staticCSSValuePool->m_percentValues;
        case CSSUnitType::CSS_PX:
            return staticCSSValuePool->m_pixelValues;
        default:
            return nullptr;
        }
    }()) {
        int intValue = value;
        double roundTripValue = intValue;
        if (!memcmp(&value, &roundTripValue, sizeof(double)) && intValue >= 0 && intValue <= StaticCSSValuePool::maximumCacheableIntegerValue)
            return values[intValue].get();
    }
    return adoptRef(*new CSSPrimitiveValue(value, type));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(const String& value, CSSUnitType type)
{
    return adoptRef(*new CSSPrimitiveValue(value, type));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(const Length& length)
{
    switch (length.type()) {
    case LengthType::Auto:
        return create(CSSValueAuto);
    case LengthType::Content:
        return create(CSSValueContent);
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
    case LengthType::FillAvailable:
        return create(CSSValueWebkitFillAvailable);
    case LengthType::FitContent:
        return create(CSSValueFitContent);
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
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
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

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(const LengthSize& lengthSize, const RenderStyle& style)
{
    return create(Pair::create(create(lengthSize.width, style), create(lengthSize.height, style)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<CSSBasicShape>&& value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<CSSCalcValue>&& value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<Counter>&& value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<Pair>&& value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<Quad>&& value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<Rect>&& value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(Ref<CSSUnresolvedColor>&& value)
{
    return adoptRef(*new CSSPrimitiveValue(WTFMove(value)));
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
    if (primitiveUnitType() == CSSUnitType::CSS_CALC) {
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

double CSSPrimitiveValue::computeUnzoomedNonCalcLengthDouble(CSSUnitType primitiveType, double value, CSSPropertyID propertyToCompute, const FontMetrics* fontMetrics, const FontCascadeDescription* fontDescription, const FontCascadeDescription* rootFontDescription, const RenderView* renderView)
{
    switch (primitiveType) {
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_QUIRKY_EMS:
        ASSERT(fontDescription);
        return ((propertyToCompute == CSSPropertyFontSize) ? fontDescription->specifiedSize() : fontDescription->computedSize()) * value;
    case CSSUnitType::CSS_EXS:
        ASSERT(fontMetrics);
        if (fontMetrics->hasXHeight())
            return fontMetrics->xHeight() * value;
        ASSERT(fontDescription);
        return ((propertyToCompute == CSSPropertyFontSize) ? fontDescription->specifiedSize() : fontDescription->computedSize()) / 2.0 * value;
    case CSSUnitType::CSS_REMS:
        if (!rootFontDescription)
            return value;
        return ((propertyToCompute == CSSPropertyFontSize) ? rootFontDescription->specifiedSize() : rootFontDescription->computedSize()) * value;
    case CSSUnitType::CSS_CHS:
        ASSERT(fontMetrics);
        ASSERT(fontDescription);
        return fontMetrics->zeroWidth().value_or(fontDescription->computedSize() / 2) * value;
    case CSSUnitType::CSS_IC:
        ASSERT(fontMetrics);
        return fontMetrics->ideogramWidth() * value;
    case CSSUnitType::CSS_PX:
        return value;
    case CSSUnitType::CSS_CM:
        return cssPixelsPerInch / cmPerInch * value;
    case CSSUnitType::CSS_MM:
        return cssPixelsPerInch / mmPerInch * value;
    case CSSUnitType::CSS_Q:
        return cssPixelsPerInch / QPerInch * value;
    case CSSUnitType::CSS_LHS:
    case CSSUnitType::CSS_RLHS:
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
    auto selectContainerRenderer = [&](CQ::Axis axis) -> const RenderBox* {
        conversionData.setUsesContainerUnits();
        auto* element = conversionData.elementForContainerUnitResolution();
        if (!element)
            return nullptr;
        // FIXME: Use cached query containers when available.
        auto* container = Style::ContainerQueryEvaluator::selectContainer(axis, nullString(), *element);
        if (!container)
            return nullptr;
        return dynamicDowncast<RenderBox>(container->renderer());
    };

    switch (primitiveType) {
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_QUIRKY_EMS:
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), nullptr, &conversionData.fontCascadeForFontUnits().fontDescription());
        break;

    case CSSUnitType::CSS_EXS:
        // FIXME: We have a bug right now where the zoom will be applied twice to EX units.
        // We really need to compute EX using fontMetrics for the original specifiedSize and not use
        // our actual constructed rendering font.
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), &conversionData.fontCascadeForFontUnits().metricsOfPrimaryFont(), &conversionData.fontCascadeForFontUnits().fontDescription());
        break;

    case CSSUnitType::CSS_REMS:
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), nullptr, nullptr, conversionData.rootStyle() ? &conversionData.rootStyle()->fontDescription() : nullptr);
        break;

    case CSSUnitType::CSS_CHS:
    case CSSUnitType::CSS_IC:
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), &conversionData.fontCascadeForFontUnits().metricsOfPrimaryFont(), &conversionData.fontCascadeForFontUnits().fontDescription());
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

    case CSSUnitType::CSS_LHS:
        if (conversionData.computingLineHeight() || conversionData.computingFontSize()) {
            // Try to get the parent's computed line-height, or fall back to the initial line-height of this element's font spacing.
            value *= conversionData.parentStyle() ? conversionData.parentStyle()->computedLineHeight() : conversionData.fontCascadeForFontUnits().metricsOfPrimaryFont().lineSpacing();
        } else
            value *= conversionData.computedLineHeightForFontUnits();
        break;

    case CSSUnitType::CSS_CQW: {
        if (auto* containerRenderer = selectContainerRenderer(CQ::Axis::Width))
            return containerRenderer->width() * value / 100;
        return value * conversionData.smallViewportFactor().width();
    }

    case CSSUnitType::CSS_CQH: {
        if (auto* containerRenderer = selectContainerRenderer(CQ::Axis::Height))
            return containerRenderer->height() * value / 100;
        return value * conversionData.smallViewportFactor().height();
    }

    case CSSUnitType::CSS_CQI: {
        if (auto* containerRenderer = selectContainerRenderer(CQ::Axis::Inline))
            return containerRenderer->logicalWidth() * value / 100;
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Inline, conversionData.smallViewportFactor(), conversionData.rootStyle());
    }

    case CSSUnitType::CSS_CQB: {
        if (auto* containerRenderer = selectContainerRenderer(CQ::Axis::Block))
            return containerRenderer->logicalHeight() * value / 100;
        return value * lengthOfViewportPhysicalAxisForLogicalAxis(LogicalBoxAxis::Block, conversionData.smallViewportFactor(), conversionData.rootStyle());
    }

    case CSSUnitType::CSS_CQMAX:
        return std::max(computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_CQB, value), computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_CQI, value));

    case CSSUnitType::CSS_CQMIN:
        return std::min(computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_CQB, value), computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_CQI, value));

    case CSSUnitType::CSS_RLHS:
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
        return Exception { InvalidAccessError };
    return clampTo<float>(result.value());
}

double CSSPrimitiveValue::doubleValue(CSSUnitType unitType) const
{
    return doubleValueInternal(unitType).value_or(0);
}

double CSSPrimitiveValue::doubleValue() const
{
    return primitiveUnitType() != CSSUnitType::CSS_CALC ? m_value.number : m_value.calc->doubleValue();
}

double CSSPrimitiveValue::doubleValueDividingBy100IfPercentage() const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_CALC:
        return m_value.calc->primitiveType() == CSSUnitType::CSS_PERCENTAGE ? m_value.calc->doubleValue() / 100.0 : m_value.calc->doubleValue();
    
    case CSSUnitType::CSS_PERCENTAGE:
        return m_value.number / 100.0;
        
    default:
        return m_value.number;
    }
}

std::optional<bool> CSSPrimitiveValue::isZero() const
{
    if (primitiveUnitType() == CSSUnitType::CSS_CALC)
        return std::nullopt;
    return !m_value.number;
}

std::optional<bool> CSSPrimitiveValue::isPositive() const
{
    if (primitiveUnitType() == CSSUnitType::CSS_CALC)
        return std::nullopt;
    return m_value.number > 0;
}

std::optional<bool> CSSPrimitiveValue::isNegative() const
{
    if (primitiveUnitType() == CSSUnitType::CSS_CALC)
        return std::nullopt;
    return m_value.number < 0;
}

bool CSSPrimitiveValue::isCenterPosition() const
{
    return valueID() == CSSValueCenter || doubleValue(CSSUnitType::CSS_PERCENTAGE) == 50;
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

static NEVER_INLINE String formatNonfiniteValue(double number, ASCIILiteral suffix)
{
    auto prefix = [&] {
        if (number == std::numeric_limits<double>::infinity())
            return "infinity"_s;
        if (number == -std::numeric_limits<double>::infinity())
            return "-infinity"_s;
        ASSERT(std::isnan(number));
        return "NaN"_s;
    }();
    return makeString(prefix, suffix.isEmpty() ? ""_s : " * 1"_s, suffix);
}

NEVER_INLINE String CSSPrimitiveValue::formatNumberValue(ASCIILiteral suffix) const
{
    if (!std::isfinite(m_value.number))
        return formatNonfiniteValue(m_value.number, suffix);
    return makeString(FormattedCSSNumber::create(m_value.number), suffix);
}

NEVER_INLINE String CSSPrimitiveValue::formatIntegerValue(ASCIILiteral suffix) const
{
    if (!std::isfinite(m_value.number))
        return formatNonfiniteValue(m_value.number, suffix);
    return makeString(m_value.number, suffix);
}

ASCIILiteral CSSPrimitiveValue::unitTypeString(CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::CSS_CHS: return "ch"_s;
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
    case CSSUnitType::CSS_EMS: return "em"_s;
    case CSSUnitType::CSS_EXS: return "ex"_s;
    case CSSUnitType::CSS_FR: return "fr"_s;
    case CSSUnitType::CSS_GRAD: return "grad"_s;
    case CSSUnitType::CSS_HZ: return "hz"_s;
    case CSSUnitType::CSS_IC: return "ic"_s;
    case CSSUnitType::CSS_IN: return "in"_s;
    case CSSUnitType::CSS_KHZ: return "khz"_s;
    case CSSUnitType::CSS_LHS: return "lh"_s;
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
    case CSSUnitType::CSS_REMS: return "rem"_s;
    case CSSUnitType::CSS_RLHS: return "rlh"_s;
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
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_COUNTER:
    case CSSUnitType::CSS_COUNTER_NAME:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_PAIR:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_QUAD:
    case CSSUnitType::CSS_QUIRKY_EMS:
    case CSSUnitType::CSS_RECT:
    case CSSUnitType::CSS_RGBCOLOR:
    case CSSUnitType::CSS_SHAPE:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNICODE_RANGE:
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

static String serialize(const Counter& counter)
{
    auto listStyleSeparator = counter.listStyle == CSSValueDecimal ? ""_s : ", "_s;
    auto listStyleLiteral = counter.listStyle == CSSValueDecimal ? ""_s : nameLiteral(counter.listStyle);
    if (counter.separator.isEmpty())
        return makeString("counter("_s, counter.identifier, listStyleSeparator, listStyleLiteral, ')');
    StringBuilder result;
    result.append("counters("_s, counter.identifier, ", "_s);
    serializeString(counter.separator, result);
    result.append(listStyleSeparator, listStyleLiteral, ')');
    return result.toString();
}

ALWAYS_INLINE String CSSPrimitiveValue::serializeInternal() const
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::CSS_CHS:
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
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_EXS:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_LHS:
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
    case CSSUnitType::CSS_REMS:
    case CSSUnitType::CSS_RLHS:
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
        return makeString("attr("_s, m_value.string, ')');
    case CSSUnitType::CSS_CALC:
        if (!m_value.calc)
            break;
        return m_value.calc->cssText();
    case CSSUnitType::CSS_COUNTER:
        return serialize(*m_value.counter);
    case CSSUnitType::CSS_COUNTER_NAME:
        return makeString("counter(", m_value.string, ')');
    case CSSUnitType::CSS_DIMENSION:
        // FIXME: This isn't correct.
        return formatNumberValue(""_s);
    case CSSUnitType::CSS_FONT_FAMILY:
        return serializeFontFamily(m_value.string);
    case CSSUnitType::CSS_INTEGER:
        return formatIntegerValue(""_s);
    case CSSUnitType::CSS_PAIR:
        return m_value.pair->cssText();
    case CSSUnitType::CSS_QUAD:
        return m_value.quad->cssText();
    case CSSUnitType::CSS_QUIRKY_EMS:
        return formatNumberValue("em"_s);
    case CSSUnitType::CSS_RECT:
        return m_value.rect->cssText();
    case CSSUnitType::CSS_RGBCOLOR:
        return serializationForCSS(color());
    case CSSUnitType::CSS_SHAPE:
        return m_value.shape->cssText();
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
    case CSSUnitType::CSS_UNICODE_RANGE:
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
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_QUIRKY_EMS:
    case CSSUnitType::CSS_EXS:
    case CSSUnitType::CSS_REMS:
    case CSSUnitType::CSS_CHS:
    case CSSUnitType::CSS_IC:
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
    case CSSUnitType::CSS_LHS:
    case CSSUnitType::CSS_RLHS:
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
    case CSSUnitType::CSS_COUNTER_NAME:
    case CSSUnitType::CSS_FONT_FAMILY:
        return equal(m_value.string, other.m_value.string);
    case CSSUnitType::CSS_COUNTER:
        return m_value.counter->equals(*other.m_value.counter);
    case CSSUnitType::CSS_RECT:
        return m_value.rect->equals(*other.m_value.rect);
    case CSSUnitType::CSS_QUAD:
        return m_value.quad->equals(*other.m_value.quad);
    case CSSUnitType::CSS_RGBCOLOR:
        return color() == other.color();
    case CSSUnitType::CSS_PAIR:
        return m_value.pair->equals(*other.m_value.pair);
    case CSSUnitType::CSS_CALC:
        return m_value.calc && other.m_value.calc && m_value.calc->equals(*other.m_value.calc);
    case CSSUnitType::CSS_SHAPE:
        return m_value.shape->equals(*other.m_value.shape);
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
        return m_value.unresolvedColor->equals(*other.m_value.unresolvedColor);
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_UNICODE_RANGE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        // FIXME: seems like these should be handled.
        ASSERT_NOT_REACHED();
        break;
    }
    return false;
}

Ref<DeprecatedCSSOMPrimitiveValue> CSSPrimitiveValue::createDeprecatedCSSOMPrimitiveWrapper(CSSStyleDeclaration& styleDeclaration) const
{
    return DeprecatedCSSOMPrimitiveValue::create(*this, styleDeclaration);
}

// https://drafts.css-houdini.org/css-properties-values-api/#dependency-cycles
void CSSPrimitiveValue::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_REMS:
        dependencies.rootProperties.appendIfNotContains(CSSPropertyFontSize);
        break;
    case CSSUnitType::CSS_RLHS:
        dependencies.rootProperties.appendIfNotContains(CSSPropertyFontSize);
        dependencies.rootProperties.appendIfNotContains(CSSPropertyLineHeight);
        break;
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_QUIRKY_EMS:
    case CSSUnitType::CSS_EXS:
    case CSSUnitType::CSS_CHS:
    case CSSUnitType::CSS_IC:
        dependencies.properties.appendIfNotContains(CSSPropertyFontSize);
        break;
    case CSSUnitType::CSS_LHS:
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
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_COUNTER:
    case CSSUnitType::CSS_RECT:
    case CSSUnitType::CSS_RGBCOLOR:
    case CSSUnitType::CSS_PAIR:
    case CSSUnitType::CSS_UNICODE_RANGE:
    case CSSUnitType::CSS_COUNTER_NAME:
    case CSSUnitType::CSS_SHAPE:
    case CSSUnitType::CSS_QUAD:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_VALUE_ID:
        break;
    }
}

} // namespace WebCore
