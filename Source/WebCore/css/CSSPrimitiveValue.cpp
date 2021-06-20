/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2012, 2013 Apple Inc. All rights reserved.
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
#include "CSSFontFamily.h"
#include "CSSHelper.h"
#include "CSSMarkup.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueKeywords.h"
#include "CalculationCategory.h"
#include "CalculationValue.h"
#include "Color.h"
#include "ColorSerialization.h"
#include "Counter.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "FontCascade.h"
#include "Node.h"
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
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_DIMENSION:
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
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LHS:
    case CSSUnitType::CSS_RLHS:
    case CSSUnitType::CSS_QUIRKY_EMS:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_REMS:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPPX:
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
        return true;
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CHS:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_COUNTER:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_EXS:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_NUMBER:
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
    case CSSUnitType::CSS_SHAPE:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_UNICODE_RANGE:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_VALUE_ID:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VW:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

#endif // ASSERT_ENABLED

typedef HashMap<const CSSPrimitiveValue*, String> CSSTextCache;
static CSSTextCache& cssTextCache()
{
    static NeverDestroyed<CSSTextCache> cache;
    return cache;
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
        return m_value.calc->primitiveType();
    case CalculationCategory::Other:
        return CSSUnitType::CSS_UNKNOWN;
    }
    return CSSUnitType::CSS_UNKNOWN;
}

static const AtomString& propertyName(CSSPropertyID propertyID)
{
    ASSERT_ARG(propertyID, (propertyID >= firstCSSProperty && propertyID < firstCSSProperty + numCSSProperties));

    return getPropertyNameAtomString(propertyID);
}

static const AtomString& valueName(CSSValueID valueID)
{
    ASSERT_ARG(valueID, (valueID >= firstCSSValueKeyword && valueID <= lastCSSValueKeyword));

    return getValueNameAtomString(valueID);
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSValueID valueID)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_VALUE_ID);
    m_value.valueID = valueID;
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSPropertyID propertyID)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(CSSUnitType::CSS_PROPERTY_ID);
    m_value.propertyID = propertyID;
}

CSSPrimitiveValue::CSSPrimitiveValue(double num, CSSUnitType type)
    : CSSValue(PrimitiveClass)
{
    setPrimitiveUnitType(type);
    ASSERT(std::isfinite(num));
    m_value.num = num;
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
    m_value.color = new Color(color);
}

CSSPrimitiveValue::CSSPrimitiveValue(const Length& length)
    : CSSValue(PrimitiveClass)
{
    init(length);
}

CSSPrimitiveValue::CSSPrimitiveValue(const Length& length, const RenderStyle& style)
    : CSSValue(PrimitiveClass)
{
    switch (length.type()) {
    case LengthType::Auto:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
    case LengthType::Percent:
        init(length);
        return;
    case LengthType::Fixed:
        setPrimitiveUnitType(CSSUnitType::CSS_PX);
        m_value.num = adjustFloatForAbsoluteZoom(length.value(), style);
        return;
    case LengthType::Calculated:
        init(CSSCalcValue::create(length.calculationValue(), style));
        return;
    case LengthType::Relative:
    case LengthType::Undefined:
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT_NOT_REACHED();
}

CSSPrimitiveValue::CSSPrimitiveValue(const LengthSize& lengthSize, const RenderStyle& style)
    : CSSValue(PrimitiveClass)
{
    init(lengthSize, style);
}

CSSPrimitiveValue::CSSPrimitiveValue(StaticCSSValueTag, CSSValueID valueID)
    : CSSPrimitiveValue(valueID)
{
    makeStatic();
}

CSSPrimitiveValue::CSSPrimitiveValue(StaticCSSValueTag, const Color& color)
    : CSSPrimitiveValue(color)
{
    makeStatic();
}

CSSPrimitiveValue::CSSPrimitiveValue(StaticCSSValueTag, double num, CSSUnitType type)
    : CSSPrimitiveValue(num, type)
{
    makeStatic();
}

void CSSPrimitiveValue::init(const Length& length)
{
    switch (length.type()) {
    case LengthType::Auto:
        setPrimitiveUnitType(CSSUnitType::CSS_VALUE_ID);
        m_value.valueID = CSSValueAuto;
        return;
    case LengthType::Fixed:
        setPrimitiveUnitType(CSSUnitType::CSS_PX);
        m_value.num = length.value();
        return;
    case LengthType::Intrinsic:
        setPrimitiveUnitType(CSSUnitType::CSS_VALUE_ID);
        m_value.valueID = CSSValueIntrinsic;
        return;
    case LengthType::MinIntrinsic:
        setPrimitiveUnitType(CSSUnitType::CSS_VALUE_ID);
        m_value.valueID = CSSValueMinIntrinsic;
        return;
    case LengthType::MinContent:
        setPrimitiveUnitType(CSSUnitType::CSS_VALUE_ID);
        m_value.valueID = CSSValueMinContent;
        return;
    case LengthType::MaxContent:
        setPrimitiveUnitType(CSSUnitType::CSS_VALUE_ID);
        m_value.valueID = CSSValueMaxContent;
        return;
    case LengthType::FillAvailable:
        setPrimitiveUnitType(CSSUnitType::CSS_VALUE_ID);
        m_value.valueID = CSSValueWebkitFillAvailable;
        return;
    case LengthType::FitContent:
        setPrimitiveUnitType(CSSUnitType::CSS_VALUE_ID);
        m_value.valueID = CSSValueFitContent;
        return;
    case LengthType::Percent:
        setPrimitiveUnitType(CSSUnitType::CSS_PERCENTAGE);
        ASSERT(std::isfinite(length.percent()));
        m_value.num = length.percent();
        return;
    case LengthType::Calculated:
    case LengthType::Relative:
    case LengthType::Undefined:
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT_NOT_REACHED();
}

void CSSPrimitiveValue::init(const LengthSize& lengthSize, const RenderStyle& style)
{
    setPrimitiveUnitType(CSSUnitType::CSS_PAIR);
    m_hasCachedCSSText = false;
    m_value.pair = &Pair::create(create(lengthSize.width, style), create(lengthSize.height, style)).leakRef();
}

void CSSPrimitiveValue::init(Ref<Counter>&& counter)
{
    setPrimitiveUnitType(CSSUnitType::CSS_COUNTER);
    m_hasCachedCSSText = false;
    m_value.counter = &counter.leakRef();
}

void CSSPrimitiveValue::init(Ref<Rect>&& r)
{
    setPrimitiveUnitType(CSSUnitType::CSS_RECT);
    m_hasCachedCSSText = false;
    m_value.rect = &r.leakRef();
}

void CSSPrimitiveValue::init(Ref<Quad>&& quad)
{
    setPrimitiveUnitType(CSSUnitType::CSS_QUAD);
    m_hasCachedCSSText = false;
    m_value.quad = &quad.leakRef();
}

void CSSPrimitiveValue::init(Ref<Pair>&& p)
{
    setPrimitiveUnitType(CSSUnitType::CSS_PAIR);
    m_hasCachedCSSText = false;
    m_value.pair = &p.leakRef();
}

void CSSPrimitiveValue::init(Ref<CSSBasicShape>&& shape)
{
    setPrimitiveUnitType(CSSUnitType::CSS_SHAPE);
    m_hasCachedCSSText = false;
    m_value.shape = &shape.leakRef();
}

void CSSPrimitiveValue::init(RefPtr<CSSCalcValue>&& c)
{
    setPrimitiveUnitType(CSSUnitType::CSS_CALC);
    m_hasCachedCSSText = false;
    m_value.calc = c.leakRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    cleanup();
}

void CSSPrimitiveValue::cleanup()
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_COUNTER_NAME:
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
        if (m_value.calc)
            m_value.calc->deref();
        break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        ASSERT_NOT_REACHED();
        break;
    case CSSUnitType::CSS_SHAPE:
        m_value.shape->deref();
        break;
    case CSSUnitType::CSS_FONT_FAMILY:
        ASSERT(m_value.fontFamily);
        delete m_value.fontFamily;
        m_value.fontFamily = nullptr;
        break;
    case CSSUnitType::CSS_RGBCOLOR:
        ASSERT(m_value.color);
        delete m_value.color;
        m_value.color = nullptr;
        break;
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_QUIRKY_EMS:
    case CSSUnitType::CSS_EXS:
    case CSSUnitType::CSS_REMS:
    case CSSUnitType::CSS_CHS:
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
    case CSSUnitType::CSS_DPPX:
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
        ASSERT(!isStringType(type));
        break;
    }
    setPrimitiveUnitType(CSSUnitType::CSS_UNKNOWN);
    if (m_hasCachedCSSText) {
        cssTextCache().remove(this);
        m_hasCachedCSSText = false;
    }
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

    return computeNonCalcLengthDouble(conversionData, primitiveType(), m_value.num);
}

static constexpr double mmPerInch = 25.4;
static constexpr double cmPerInch = 2.54;
static constexpr double QPerInch = 25.4 * 4.0;

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
        return fontMetrics->zeroWidth() * value;
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
        return renderView ? renderView->viewportSizeForCSSViewportUnits().height() / 100.0 * value : 0;
    case CSSUnitType::CSS_VW:
        return renderView ? renderView->viewportSizeForCSSViewportUnits().width() / 100.0 * value : 0;
    case CSSUnitType::CSS_VMAX:
        if (renderView) {
            IntSize viewportSizeForCSSViewportUnits = renderView->viewportSizeForCSSViewportUnits();
            return std::max(viewportSizeForCSSViewportUnits.width(), viewportSizeForCSSViewportUnits.height()) / 100.0 * value;
        }
        return value;
    case CSSUnitType::CSS_VMIN:
        if (renderView) {
            IntSize viewportSizeForCSSViewportUnits = renderView->viewportSizeForCSSViewportUnits();
            return std::min(viewportSizeForCSSViewportUnits.width(), viewportSizeForCSSViewportUnits.height()) / 100.0 * value;
        }
        return value;
    default:
        ASSERT_NOT_REACHED();
        return -1.0;
    }
}

double CSSPrimitiveValue::computeNonCalcLengthDouble(const CSSToLengthConversionData& conversionData, CSSUnitType primitiveType, double value)
{
    switch (primitiveType) {
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_QUIRKY_EMS:
        ASSERT(conversionData.style());
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), nullptr, &conversionData.style()->fontDescription());
        break;

    case CSSUnitType::CSS_EXS:
        // FIXME: We have a bug right now where the zoom will be applied twice to EX units.
        // We really need to compute EX using fontMetrics for the original specifiedSize and not use
        // our actual constructed rendering font.
        ASSERT(conversionData.style());
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), &conversionData.style()->fontMetrics(), &conversionData.style()->fontDescription());
        break;

    case CSSUnitType::CSS_REMS:
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), nullptr, nullptr, conversionData.rootStyle() ? &conversionData.rootStyle()->fontDescription() : nullptr);
        break;

    case CSSUnitType::CSS_CHS:
        ASSERT(conversionData.style());
        value = computeUnzoomedNonCalcLengthDouble(primitiveType, value, conversionData.propertyToCompute(), &conversionData.style()->fontMetrics());
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
        return value * conversionData.viewportHeightFactor();

    case CSSUnitType::CSS_VW:
        return value * conversionData.viewportWidthFactor();

    case CSSUnitType::CSS_VMAX:
        return value * conversionData.viewportMaxFactor();

    case CSSUnitType::CSS_VMIN:
        return value * conversionData.viewportMinFactor();

    case CSSUnitType::CSS_LHS:
        ASSERT(conversionData.style());
        if (conversionData.computingLineHeight() || conversionData.computingFontSize()) {
            // Try to get the parent's computed line-height, or fall back to the initial line-height of this element's font spacing.
            value *= conversionData.parentStyle() ? conversionData.parentStyle()->computedLineHeight() : conversionData.style()->fontMetrics().lineSpacing();
        } else
            value *= conversionData.style()->computedLineHeight();
        break;

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

    if (styleA.fontMetrics().xHeight() != styleB.fontMetrics().xHeight())
        return false;
    if (styleA.fontMetrics().zeroWidth() != styleB.fontMetrics().zeroWidth())
        return false;

    if (styleA.zoom() != styleB.zoom())
        return false;

    return true;
}

double CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(CSSUnitType unitType)
{
    double factor = 1.0;
    // FIXME: the switch can be replaced by an array of scale factors.
    switch (unitType) {
    // These are "canonical" units in their respective categories.
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_DPPX:
        break;
    case CSSUnitType::CSS_CM:
        factor = cssPixelsPerInch / cmPerInch;
        break;
    case CSSUnitType::CSS_DPCM:
        factor = cmPerInch / cssPixelsPerInch; // (2.54 cm/in)
        break;
    case CSSUnitType::CSS_MM:
        factor = cssPixelsPerInch / mmPerInch;
        break;
    case CSSUnitType::CSS_Q:
        factor = cssPixelsPerInch / QPerInch;
        break;
    case CSSUnitType::CSS_IN:
        factor = cssPixelsPerInch;
        break;
    case CSSUnitType::CSS_DPI:
        factor = 1 / cssPixelsPerInch;
        break;
    case CSSUnitType::CSS_PT:
        factor = cssPixelsPerInch / 72.0;
        break;
    case CSSUnitType::CSS_PC:
        factor = cssPixelsPerInch * 12.0 / 72.0; // 1 pc == 12 pt
        break;
    case CSSUnitType::CSS_RAD:
        factor = degreesPerRadianDouble;
        break;
    case CSSUnitType::CSS_GRAD:
        factor = degreesPerGradientDouble;
        break;
    case CSSUnitType::CSS_TURN:
        factor = degreesPerTurnDouble;
        break;
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_KHZ:
        factor = 1000;
        break;
    default:
        break;
    }

    return factor;
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
    return primitiveUnitType() != CSSUnitType::CSS_CALC ? m_value.num : m_value.calc->doubleValue();
}

std::optional<bool> CSSPrimitiveValue::isZero() const
{
    if (primitiveUnitType() == CSSUnitType::CSS_CALC)
        return std::nullopt;
    return !m_value.num;
}

std::optional<bool> CSSPrimitiveValue::isPositive() const
{
    if (primitiveUnitType() == CSSUnitType::CSS_CALC)
        return std::nullopt;
    return m_value.num > 0;
}

std::optional<bool> CSSPrimitiveValue::isNegative() const
{
    if (primitiveUnitType() == CSSUnitType::CSS_CALC)
        return std::nullopt;
    return m_value.num < 0;
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
        // We interpret conversion to CSSUnitType::CSS_NUMBER as conversion to a canonical unit in this value's category.
        targetUnitType = canonicalUnitTypeForCategory(sourceCategory);
        if (targetUnitType == CSSUnitType::CSS_UNKNOWN)
            return std::nullopt;
    }

    if (sourceUnitType == CSSUnitType::CSS_NUMBER) {
        // We interpret conversion from CSSUnitType::CSS_NUMBER in the same way as CSSParser::validUnit() while using non-strict mode.
        sourceUnitType = canonicalUnitTypeForCategory(targetCategory);
        if (sourceUnitType == CSSUnitType::CSS_UNKNOWN)
            return std::nullopt;
    }

    double convertedValue = doubleValue();

    // First convert the value from primitiveUnitType() to canonical type.
    double factor = conversionToCanonicalUnitsScaleFactor(sourceUnitType);
    convertedValue *= factor;

    // Now convert from canonical type to the target unitType.
    factor = conversionToCanonicalUnitsScaleFactor(targetUnitType);
    convertedValue /= factor;

    return convertedValue;
}

String CSSPrimitiveValue::stringValue() const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_URI:
        return m_value.string;
    case CSSUnitType::CSS_FONT_FAMILY:
        return m_value.fontFamily->familyName;
    case CSSUnitType::CSS_VALUE_ID:
        return valueName(m_value.valueID);
    case CSSUnitType::CSS_PROPERTY_ID:
        return propertyName(m_value.propertyID);
    default:
        return String();
    }
}

NEVER_INLINE String CSSPrimitiveValue::formatNumberValue(StringView suffix) const
{
    return makeString(m_value.num, suffix);
}

// FIXME: Should return const char*.
String CSSPrimitiveValue::unitTypeString(CSSUnitType unitType)
{
    switch (unitType) {
        case CSSUnitType::CSS_PERCENTAGE: return "%";
        case CSSUnitType::CSS_EMS: return "em";
        case CSSUnitType::CSS_EXS: return "ex";
        case CSSUnitType::CSS_PX: return "px";
        case CSSUnitType::CSS_CM: return "cm";
        case CSSUnitType::CSS_MM: return "mm";
        case CSSUnitType::CSS_IN: return "in";
        case CSSUnitType::CSS_PT: return "pt";
        case CSSUnitType::CSS_PC: return "pc";
        case CSSUnitType::CSS_DEG: return "deg";
        case CSSUnitType::CSS_RAD: return "rad";
        case CSSUnitType::CSS_GRAD: return "grad";
        case CSSUnitType::CSS_MS: return "ms";
        case CSSUnitType::CSS_S: return "s";
        case CSSUnitType::CSS_HZ: return "hz";
        case CSSUnitType::CSS_KHZ: return "khz";
        case CSSUnitType::CSS_VW: return "vw";
        case CSSUnitType::CSS_VH: return "vh";
        case CSSUnitType::CSS_VMIN: return "vmin";
        case CSSUnitType::CSS_VMAX: return "vmax";
        case CSSUnitType::CSS_DPPX: return "dppx";
        case CSSUnitType::CSS_DPI: return "dpi";
        case CSSUnitType::CSS_DPCM: return "dpcm";
        case CSSUnitType::CSS_FR: return "fr";
        case CSSUnitType::CSS_Q: return "q";
        case CSSUnitType::CSS_LHS: return "lh";
        case CSSUnitType::CSS_RLHS: return "rlh";
        case CSSUnitType::CSS_TURN: return "turn";
        case CSSUnitType::CSS_REMS: return "rem";
        case CSSUnitType::CSS_CHS: return "ch";

        case CSSUnitType::CSS_UNKNOWN:
        case CSSUnitType::CSS_NUMBER:
        case CSSUnitType::CSS_DIMENSION:
        case CSSUnitType::CSS_STRING:
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
        case CSSUnitType::CSS_CALC:
        case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
        case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        case CSSUnitType::CSS_FONT_FAMILY:
        case CSSUnitType::CSS_PROPERTY_ID:
        case CSSUnitType::CSS_VALUE_ID:
        case CSSUnitType::CSS_QUIRKY_EMS:
            return emptyString();
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

ALWAYS_INLINE String CSSPrimitiveValue::formatNumberForCustomCSSText() const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_UNKNOWN:
        return String();
    case CSSUnitType::CSS_NUMBER:
        return formatNumberValue("");
    case CSSUnitType::CSS_PERCENTAGE:
        return formatNumberValue("%");
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_QUIRKY_EMS:
        return formatNumberValue("em");
    case CSSUnitType::CSS_EXS:
        return formatNumberValue("ex");
    case CSSUnitType::CSS_REMS:
        return formatNumberValue("rem");
    case CSSUnitType::CSS_CHS:
        return formatNumberValue("ch");
    case CSSUnitType::CSS_PX:
        return formatNumberValue("px");
    case CSSUnitType::CSS_CM:
        return formatNumberValue("cm");
    case CSSUnitType::CSS_DPPX:
        return formatNumberValue("dppx");
    case CSSUnitType::CSS_DPI:
        return formatNumberValue("dpi");
    case CSSUnitType::CSS_DPCM:
        return formatNumberValue("dpcm");
    case CSSUnitType::CSS_MM:
        return formatNumberValue("mm");
    case CSSUnitType::CSS_IN:
        return formatNumberValue("in");
    case CSSUnitType::CSS_PT:
        return formatNumberValue("pt");
    case CSSUnitType::CSS_PC:
        return formatNumberValue("pc");
    case CSSUnitType::CSS_DEG:
        return formatNumberValue("deg");
    case CSSUnitType::CSS_RAD:
        return formatNumberValue("rad");
    case CSSUnitType::CSS_GRAD:
        return formatNumberValue("grad");
    case CSSUnitType::CSS_MS:
        return formatNumberValue("ms");
    case CSSUnitType::CSS_S:
        return formatNumberValue("s");
    case CSSUnitType::CSS_HZ:
        return formatNumberValue("hz");
    case CSSUnitType::CSS_KHZ:
        return formatNumberValue("khz");
    case CSSUnitType::CSS_TURN:
        return formatNumberValue("turn");
    case CSSUnitType::CSS_FR:
        return formatNumberValue("fr");
    case CSSUnitType::CSS_Q:
        return formatNumberValue("q");
    case CSSUnitType::CSS_LHS:
        return formatNumberValue("lh");
    case CSSUnitType::CSS_RLHS:
        return formatNumberValue("rlh");
    case CSSUnitType::CSS_DIMENSION:
        // FIXME: This isn't correct.
        return formatNumberValue("");
    case CSSUnitType::CSS_STRING:
        return serializeString(m_value.string);
    case CSSUnitType::CustomIdent: {
        StringBuilder builder;
        serializeIdentifier(m_value.string, builder);
        return builder.toString();
    }
    case CSSUnitType::CSS_FONT_FAMILY:
        return serializeFontFamily(m_value.fontFamily->familyName);
    case CSSUnitType::CSS_URI:
        return serializeURL(m_value.string);
    case CSSUnitType::CSS_VALUE_ID:
        return valueName(m_value.valueID);
    case CSSUnitType::CSS_PROPERTY_ID:
        return propertyName(m_value.propertyID);
    case CSSUnitType::CSS_ATTR:
        return "attr(" + String(m_value.string) + ')';
    case CSSUnitType::CSS_COUNTER_NAME:
        return "counter(" + String(m_value.string) + ')';
    case CSSUnitType::CSS_COUNTER: {
        StringBuilder result;
        auto separator = m_value.counter->separator();
        auto listStyle = m_value.counter->listStyle();
        result.append(separator.isEmpty() ? "counter(" : "counters(", m_value.counter->identifier(), separator.isEmpty() ? "" : ", ");
        if (!separator.isEmpty())
            serializeString(separator, result);
        if (!(listStyle.isEmpty() || listStyle == "decimal"))
            result.append(", ", listStyle);
        result.append(')');
        return result.toString();
    }
    case CSSUnitType::CSS_RECT:
        return rectValue()->cssText();
    case CSSUnitType::CSS_QUAD:
        return quadValue()->cssText();
    case CSSUnitType::CSS_RGBCOLOR:
        return serializationForCSS(color());
    case CSSUnitType::CSS_PAIR:
        return pairValue()->cssText();
    case CSSUnitType::CSS_CALC:
        if (!m_value.calc)
            break;
        return m_value.calc->cssText();
    case CSSUnitType::CSS_SHAPE:
        return m_value.shape->cssText();
    case CSSUnitType::CSS_VW:
        return formatNumberValue("vw");
    case CSSUnitType::CSS_VH:
        return formatNumberValue("vh");
    case CSSUnitType::CSS_VMIN:
        return formatNumberValue("vmin");
    case CSSUnitType::CSS_VMAX:
        return formatNumberValue("vmax");
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_UNICODE_RANGE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        ASSERT_NOT_REACHED();
    }
    return String();
}

String CSSPrimitiveValue::customCSSText() const
{
    // FIXME: return the original value instead of a generated one (e.g. color
    // name if it was specified) - check what spec says about this

    CSSTextCache& cssTextCache = WebCore::cssTextCache();

    if (m_hasCachedCSSText) {
        ASSERT(cssTextCache.contains(this));
        return cssTextCache.get(this);
    }

    String text = formatNumberForCustomCSSText();

    ASSERT(!cssTextCache.contains(this));
    m_hasCachedCSSText = true;
    cssTextCache.set(this, text);
    return text;
}

bool CSSPrimitiveValue::equals(const CSSPrimitiveValue& other) const
{
    if (primitiveUnitType() != other.primitiveUnitType())
        return false;

    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_UNKNOWN:
        return false;
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_QUIRKY_EMS:
    case CSSUnitType::CSS_EXS:
    case CSSUnitType::CSS_REMS:
    case CSSUnitType::CSS_CHS:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_DPPX:
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
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LHS:
    case CSSUnitType::CSS_RLHS:
    case CSSUnitType::CSS_DIMENSION:
        return m_value.num == other.m_value.num;
    case CSSUnitType::CSS_PROPERTY_ID:
        return propertyName(m_value.propertyID) == propertyName(other.m_value.propertyID);
    case CSSUnitType::CSS_VALUE_ID:
        return valueName(m_value.valueID) == valueName(other.m_value.valueID);
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_COUNTER_NAME:
        return equal(m_value.string, other.m_value.string);
    case CSSUnitType::CSS_COUNTER:
        return m_value.counter && other.m_value.counter && m_value.counter->equals(*other.m_value.counter);
    case CSSUnitType::CSS_RECT:
        return m_value.rect && other.m_value.rect && m_value.rect->equals(*other.m_value.rect);
    case CSSUnitType::CSS_QUAD:
        return m_value.quad && other.m_value.quad && m_value.quad->equals(*other.m_value.quad);
    case CSSUnitType::CSS_RGBCOLOR:
        return color() == other.color();
    case CSSUnitType::CSS_PAIR:
        return m_value.pair && other.m_value.pair && m_value.pair->equals(*other.m_value.pair);
    case CSSUnitType::CSS_CALC:
        return m_value.calc && other.m_value.calc && m_value.calc->equals(*other.m_value.calc);
    case CSSUnitType::CSS_SHAPE:
        return m_value.shape && other.m_value.shape && m_value.shape->equals(*other.m_value.shape);
    case CSSUnitType::CSS_FONT_FAMILY:
        return fontFamily() == other.fontFamily();
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
void CSSPrimitiveValue::collectDirectComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_QUIRKY_EMS:
    case CSSUnitType::CSS_EXS:
    case CSSUnitType::CSS_CHS:
        values.add(CSSPropertyFontSize);
        break;
    case CSSUnitType::CSS_LHS:
        values.add(CSSPropertyFontSize);
        values.add(CSSPropertyLineHeight);
        break;
    case CSSUnitType::CSS_CALC:
        m_value.calc->collectDirectComputationalDependencies(values);
        break;
    default:
        break;
    }
}

void CSSPrimitiveValue::collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_REMS:
        values.add(CSSPropertyFontSize);
        break;
    case CSSUnitType::CSS_RLHS:
        values.add(CSSPropertyFontSize);
        values.add(CSSPropertyLineHeight);
        break;
    case CSSUnitType::CSS_CALC:
        m_value.calc->collectDirectRootComputationalDependencies(values);
        break;
    default:
        break;
    }
}

} // namespace WebCore
