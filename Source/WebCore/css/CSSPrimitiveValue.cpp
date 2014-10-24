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
#include "CSSCalculationValue.h"
#include "CSSHelper.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueKeywords.h"
#include "CalculationValue.h"
#include "Color.h"
#include "Counter.h"
#include "ExceptionCode.h"
#include "Font.h"
#include "LayoutUnit.h"
#include "Node.h"
#include "Pair.h"
#include "RGBColor.h"
#include "Rect.h"
#include "RenderStyle.h"
#include "StyleSheetContents.h"
#include <wtf/ASCIICType.h>
#include <wtf/DecimalNumber.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

#if ENABLE(CSS_SCROLL_SNAP)
#include "LengthRepeat.h"
#endif

using namespace WTF;

namespace WebCore {

// Max/min values for CSS, needs to slightly smaller/larger than the true max/min values to allow for rounding without overflowing.
// Subtract two (rather than one) to allow for values to be converted to float and back without exceeding the LayoutUnit::max.
const int maxValueForCssLength = intMaxForLayoutUnit - 2;
const int minValueForCssLength = intMinForLayoutUnit + 2;

static inline bool isValidCSSUnitTypeForDoubleConversion(CSSPrimitiveValue::UnitTypes unitType)
{
    switch (unitType) {
    case CSSPrimitiveValue::CSS_CALC:
    case CSSPrimitiveValue::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSPrimitiveValue::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_DEG:
    case CSSPrimitiveValue::CSS_DIMENSION:
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
    case CSSPrimitiveValue::CSS_DPPX:
    case CSSPrimitiveValue::CSS_DPI:
    case CSSPrimitiveValue::CSS_DPCM:
#endif
    case CSSPrimitiveValue::CSS_EMS:
    case CSSPrimitiveValue::CSS_EXS:
    case CSSPrimitiveValue::CSS_GRAD:
    case CSSPrimitiveValue::CSS_HZ:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_KHZ:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_MS:
    case CSSPrimitiveValue::CSS_NUMBER:
    case CSSPrimitiveValue::CSS_PERCENTAGE:
    case CSSPrimitiveValue::CSS_PC:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_RAD:
    case CSSPrimitiveValue::CSS_REMS:
    case CSSPrimitiveValue::CSS_CHS:
    case CSSPrimitiveValue::CSS_S:
    case CSSPrimitiveValue::CSS_TURN:
    case CSSPrimitiveValue::CSS_VW:
    case CSSPrimitiveValue::CSS_VH:
    case CSSPrimitiveValue::CSS_VMIN:
    case CSSPrimitiveValue::CSS_VMAX:
    case CSSPrimitiveValue::CSS_FR:
        return true;
    case CSSPrimitiveValue::CSS_ATTR:
    case CSSPrimitiveValue::CSS_COUNTER:
    case CSSPrimitiveValue::CSS_COUNTER_NAME:
#if ENABLE(DASHBOARD_SUPPORT)
    case CSSPrimitiveValue::CSS_DASHBOARD_REGION:
#endif
#if !ENABLE(CSS_IMAGE_RESOLUTION) && !ENABLE(RESOLUTION_MEDIA_QUERY)
    case CSSPrimitiveValue::CSS_DPPX:
    case CSSPrimitiveValue::CSS_DPI:
    case CSSPrimitiveValue::CSS_DPCM:
#endif
    case CSSPrimitiveValue::CSS_IDENT:
    case CSSPrimitiveValue::CSS_PROPERTY_ID:
    case CSSPrimitiveValue::CSS_VALUE_ID:
    case CSSPrimitiveValue::CSS_PAIR:
    case CSSPrimitiveValue::CSS_PARSER_HEXCOLOR:
    case CSSPrimitiveValue::CSS_PARSER_IDENTIFIER:
    case CSSPrimitiveValue::CSS_PARSER_INTEGER:
    case CSSPrimitiveValue::CSS_PARSER_OPERATOR:
    case CSSPrimitiveValue::CSS_RECT:
    case CSSPrimitiveValue::CSS_QUAD:
#if ENABLE(CSS_SCROLL_SNAP)
    case CSSPrimitiveValue::CSS_LENGTH_REPEAT:
#endif
    case CSSPrimitiveValue::CSS_RGBCOLOR:
    case CSSPrimitiveValue::CSS_SHAPE:
    case CSSPrimitiveValue::CSS_STRING:
    case CSSPrimitiveValue::CSS_UNICODE_RANGE:
    case CSSPrimitiveValue::CSS_UNKNOWN:
    case CSSPrimitiveValue::CSS_URI:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

CSSPrimitiveValue::UnitCategory CSSPrimitiveValue::unitCategory(CSSPrimitiveValue::UnitTypes type)
{
    // Here we violate the spec (http://www.w3.org/TR/DOM-Level-2-Style/css.html#CSS-CSSPrimitiveValue) and allow conversions
    // between CSS_PX and relative lengths (see cssPixelsPerInch comment in CSSHelper.h for the topic treatment).
    switch (type) {
    case CSS_NUMBER:
        return UNumber;
    case CSS_PERCENTAGE:
        return UPercent;
    case CSS_PX:
    case CSS_CM:
    case CSS_MM:
    case CSS_IN:
    case CSS_PT:
    case CSS_PC:
        return ULength;
    case CSS_MS:
    case CSS_S:
        return UTime;
    case CSS_DEG:
    case CSS_RAD:
    case CSS_GRAD:
    case CSS_TURN:
        return UAngle;
    case CSS_HZ:
    case CSS_KHZ:
        return UFrequency;
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
    case CSS_DPPX:
    case CSS_DPI:
    case CSS_DPCM:
        return UResolution;
#endif
    default:
        return UOther;
    }
}

typedef HashMap<const CSSPrimitiveValue*, String> CSSTextCache;
static CSSTextCache& cssTextCache()
{
    static NeverDestroyed<CSSTextCache> cache;
    return cache;
}

unsigned short CSSPrimitiveValue::primitiveType() const
{
    if (m_primitiveUnitType == CSS_PROPERTY_ID || m_primitiveUnitType == CSS_VALUE_ID)
        return CSS_IDENT;

    if (m_primitiveUnitType != CSSPrimitiveValue::CSS_CALC)
        return m_primitiveUnitType;

    switch (m_value.calc->category()) {
    case CalcNumber:
        return CSSPrimitiveValue::CSS_NUMBER;
    case CalcLength:
        return CSSPrimitiveValue::CSS_PX;
    case CalcPercent:
        return CSSPrimitiveValue::CSS_PERCENTAGE;
    case CalcPercentNumber:
        return CSSPrimitiveValue::CSS_CALC_PERCENTAGE_WITH_NUMBER;
    case CalcPercentLength:
        return CSSPrimitiveValue::CSS_CALC_PERCENTAGE_WITH_LENGTH;
    case CalcAngle:
        return CSSPrimitiveValue::CSS_DEG;
    case CalcTime:
        return CSSPrimitiveValue::CSS_MS;
    case CalcFrequency:
        return CSSPrimitiveValue::CSS_HZ;
    case CalcOther:
        return CSSPrimitiveValue::CSS_UNKNOWN;
    }
    return CSSPrimitiveValue::CSS_UNKNOWN;
}

static const AtomicString& propertyName(CSSPropertyID propertyID)
{
    ASSERT_ARG(propertyID, propertyID >= 0);
    ASSERT_ARG(propertyID, (propertyID >= firstCSSProperty && propertyID < firstCSSProperty + numCSSProperties));

    if (propertyID < 0)
        return nullAtom;

    return getPropertyNameAtomicString(propertyID);
}

static const AtomicString& valueName(CSSValueID valueID)
{
    ASSERT_ARG(valueID, valueID >= 0);
    ASSERT_ARG(valueID, valueID < numCSSValueKeywords);

    if (valueID < 0)
        return nullAtom;

    static AtomicString* keywordStrings = new AtomicString[numCSSValueKeywords]; // Leaked intentionally.
    AtomicString& keywordString = keywordStrings[valueID];
    if (keywordString.isNull())
        keywordString = getValueName(valueID);
    return keywordString;
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSValueID valueID)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    m_value.valueID = valueID;
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSPropertyID propertyID)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_PROPERTY_ID;
    m_value.propertyID = propertyID;
}

CSSPrimitiveValue::CSSPrimitiveValue(int parserOperator)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_PARSER_OPERATOR;
    m_value.parserOperator = parserOperator;
}

CSSPrimitiveValue::CSSPrimitiveValue(double num, UnitTypes type)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = type;
    ASSERT(std::isfinite(num));
    m_value.num = num;
}

CSSPrimitiveValue::CSSPrimitiveValue(const String& str, UnitTypes type)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = type;
    if ((m_value.string = str.impl()))
        m_value.string->ref();
}

CSSPrimitiveValue::CSSPrimitiveValue(RGBA32 color)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_RGBCOLOR;
    m_value.rgbcolor = color;
}

CSSPrimitiveValue::CSSPrimitiveValue(const Length& length)
    : CSSValue(PrimitiveClass)
{
    init(length);
}

CSSPrimitiveValue::CSSPrimitiveValue(const Length& length, const RenderStyle* style)
    : CSSValue(PrimitiveClass)
{
    switch (length.type()) {
    case Auto:
    case Intrinsic:
    case MinIntrinsic:
    case MinContent:
    case MaxContent:
    case FillAvailable:
    case FitContent:
    case Percent:
        init(length);
        return;
    case Fixed:
        m_primitiveUnitType = CSS_PX;
        m_value.num = adjustFloatForAbsoluteZoom(length.value(), style);
        return;
    case Calculated: {
        RefPtr<CSSCalcValue> calcValue = CSSCalcValue::create(length.calculationValue(), *style);
        init(calcValue.release());
        return;
    }
    case Relative:
    case Undefined:
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT_NOT_REACHED();
}

CSSPrimitiveValue::CSSPrimitiveValue(const LengthSize& lengthSize, const RenderStyle* style)
    : CSSValue(PrimitiveClass)
{
    init(lengthSize, style);
}

void CSSPrimitiveValue::init(const Length& length)
{
    switch (length.type()) {
    case Auto:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueAuto;
        return;
    case WebCore::Fixed:
        m_primitiveUnitType = CSS_PX;
        m_value.num = length.value();
        return;
    case Intrinsic:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueIntrinsic;
        return;
    case MinIntrinsic:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueMinIntrinsic;
        return;
    case MinContent:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueWebkitMinContent;
        return;
    case MaxContent:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueWebkitMaxContent;
        return;
    case FillAvailable:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueWebkitFillAvailable;
        return;
    case FitContent:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueWebkitFitContent;
        return;
    case Percent:
        m_primitiveUnitType = CSS_PERCENTAGE;
        ASSERT(std::isfinite(length.percent()));
        m_value.num = length.percent();
        return;
    case Calculated:
    case Relative:
    case Undefined:
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT_NOT_REACHED();
}

void CSSPrimitiveValue::init(const LengthSize& lengthSize, const RenderStyle* style)
{
    m_primitiveUnitType = CSS_PAIR;
    m_hasCachedCSSText = false;
    m_value.pair = Pair::create(create(lengthSize.width(), style), create(lengthSize.height(), style)).leakRef();
}

void CSSPrimitiveValue::init(PassRefPtr<Counter> c)
{
    m_primitiveUnitType = CSS_COUNTER;
    m_hasCachedCSSText = false;
    m_value.counter = c.leakRef();
}

void CSSPrimitiveValue::init(PassRefPtr<Rect> r)
{
    m_primitiveUnitType = CSS_RECT;
    m_hasCachedCSSText = false;
    m_value.rect = r.leakRef();
}

void CSSPrimitiveValue::init(PassRefPtr<Quad> quad)
{
    m_primitiveUnitType = CSS_QUAD;
    m_hasCachedCSSText = false;
    m_value.quad = quad.leakRef();
}

#if ENABLE(CSS_SCROLL_SNAP)
void CSSPrimitiveValue::init(PassRefPtr<LengthRepeat> lengthRepeat)
{
    m_primitiveUnitType = CSS_LENGTH_REPEAT;
    m_hasCachedCSSText = false;
    m_value.lengthRepeat = lengthRepeat.leakRef();
}
#endif

#if ENABLE(DASHBOARD_SUPPORT)
void CSSPrimitiveValue::init(PassRefPtr<DashboardRegion> r)
{
    m_primitiveUnitType = CSS_DASHBOARD_REGION;
    m_hasCachedCSSText = false;
    m_value.region = r.leakRef();
}
#endif

void CSSPrimitiveValue::init(PassRefPtr<Pair> p)
{
    m_primitiveUnitType = CSS_PAIR;
    m_hasCachedCSSText = false;
    m_value.pair = p.leakRef();
}

void CSSPrimitiveValue::init(PassRefPtr<CSSCalcValue> c)
{
    m_primitiveUnitType = CSS_CALC;
    m_hasCachedCSSText = false;
    m_value.calc = c.leakRef();
}

void CSSPrimitiveValue::init(PassRefPtr<CSSBasicShape> shape)
{
    m_primitiveUnitType = CSS_SHAPE;
    m_hasCachedCSSText = false;
    m_value.shape = shape.leakRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    cleanup();
}

void CSSPrimitiveValue::cleanup()
{
    switch (static_cast<UnitTypes>(m_primitiveUnitType)) {
    case CSS_STRING:
    case CSS_URI:
    case CSS_ATTR:
    case CSS_COUNTER_NAME:
    case CSS_PARSER_HEXCOLOR:
        if (m_value.string)
            m_value.string->deref();
        break;
    case CSS_COUNTER:
        m_value.counter->deref();
        break;
    case CSS_RECT:
        m_value.rect->deref();
        break;
    case CSS_QUAD:
        m_value.quad->deref();
        break;
#if ENABLE(CSS_SCROLL_SNAP)
    case CSS_LENGTH_REPEAT:
        m_value.lengthRepeat->deref();
        break;
#endif
    case CSS_PAIR:
        m_value.pair->deref();
        break;
#if ENABLE(DASHBOARD_SUPPORT)
    case CSS_DASHBOARD_REGION:
        if (m_value.region)
            m_value.region->deref();
        break;
#endif
    case CSS_CALC:
        m_value.calc->deref();
        break;
    case CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSS_CALC_PERCENTAGE_WITH_LENGTH:
        ASSERT_NOT_REACHED();
        break;
    case CSS_SHAPE:
        m_value.shape->deref();
        break;
    case CSS_NUMBER:
    case CSS_PARSER_INTEGER:
    case CSS_PERCENTAGE:
    case CSS_EMS:
    case CSS_EXS:
    case CSS_REMS:
    case CSS_CHS:
    case CSS_PX:
    case CSS_CM:
    case CSS_MM:
    case CSS_IN:
    case CSS_PT:
    case CSS_PC:
    case CSS_DEG:
    case CSS_RAD:
    case CSS_GRAD:
    case CSS_MS:
    case CSS_S:
    case CSS_HZ:
    case CSS_KHZ:
    case CSS_TURN:
    case CSS_VW:
    case CSS_VH:
    case CSS_VMIN:
    case CSS_VMAX:
    case CSS_DPPX:
    case CSS_DPI:
    case CSS_DPCM:
    case CSS_FR:
    case CSS_IDENT:
    case CSS_RGBCOLOR:
    case CSS_DIMENSION:
    case CSS_UNKNOWN:
    case CSS_UNICODE_RANGE:
    case CSS_PARSER_OPERATOR:
    case CSS_PARSER_IDENTIFIER:
    case CSS_PROPERTY_ID:
    case CSS_VALUE_ID:
        break;
    }
    m_primitiveUnitType = 0;
    if (m_hasCachedCSSText) {
        cssTextCache().remove(this);
        m_hasCachedCSSText = false;
    }
}

double CSSPrimitiveValue::computeDegrees()
{
    switch (m_primitiveUnitType) {
    case CSS_DEG:
        return getDoubleValue();
    case CSS_RAD:
        return rad2deg(getDoubleValue());
    case CSS_GRAD:
        return grad2deg(getDoubleValue());
    case CSS_TURN:
        return turn2deg(getDoubleValue());
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
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
    return Length(clampTo<float>(computeLengthDouble(conversionData), minValueForCssLength, maxValueForCssLength), Fixed);
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

double CSSPrimitiveValue::computeLengthDouble(const CSSToLengthConversionData& conversionData) const
{
    if (m_primitiveUnitType == CSS_CALC)
        // The multiplier and factor is applied to each value in the calc expression individually
        return m_value.calc->computeLengthPx(conversionData);

    double factor;

    switch (primitiveType()) {
    case CSS_EMS:
        ASSERT(conversionData.style());
        factor = conversionData.computingFontSize() ? conversionData.style()->fontDescription().specifiedSize() : conversionData.style()->fontDescription().computedSize();
        break;
    case CSS_EXS:
        ASSERT(conversionData.style());
        // FIXME: We have a bug right now where the zoom will be applied twice to EX units.
        // We really need to compute EX using fontMetrics for the original specifiedSize and not use
        // our actual constructed rendering font.
        if (conversionData.style()->fontMetrics().hasXHeight())
            factor = conversionData.style()->fontMetrics().xHeight();
        else
            factor = (conversionData.computingFontSize() ? conversionData.style()->fontDescription().specifiedSize() : conversionData.style()->fontDescription().computedSize()) / 2.0;
        break;
    case CSS_REMS:
        if (conversionData.rootStyle())
            factor = conversionData.computingFontSize() ? conversionData.rootStyle()->fontDescription().specifiedSize() : conversionData.rootStyle()->fontDescription().computedSize();
        else
            factor = 1.0;
        break;
    case CSS_CHS:
        ASSERT(conversionData.style());
        factor = conversionData.style()->fontMetrics().zeroWidth();
        break;
    case CSS_PX:
        factor = 1.0;
        break;
    case CSS_CM:
        factor = cssPixelsPerInch / 2.54; // (2.54 cm/in)
        break;
    case CSS_MM:
        factor = cssPixelsPerInch / 25.4;
        break;
    case CSS_IN:
        factor = cssPixelsPerInch;
        break;
    case CSS_PT:
        factor = cssPixelsPerInch / 72.0;
        break;
    case CSS_PC:
        // 1 pc == 12 pt
        factor = cssPixelsPerInch * 12.0 / 72.0;
        break;
    case CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSS_CALC_PERCENTAGE_WITH_NUMBER:
        ASSERT_NOT_REACHED();
        return -1.0;
    case CSS_VH:
        factor = conversionData.viewportHeightFactor();
        break;
    case CSS_VW:
        factor = conversionData.viewportWidthFactor();
        break;
    case CSS_VMAX:
        factor = conversionData.viewportMaxFactor();
        break;
    case CSS_VMIN:
        factor = conversionData.viewportMinFactor();
        break;
    default:
        ASSERT_NOT_REACHED();
        return -1.0;
    }

    // We do not apply the zoom factor when we are computing the value of the font-size property. The zooming
    // for font sizes is much more complicated, since we have to worry about enforcing the minimum font size preference
    // as well as enforcing the implicit "smart minimum."
    double result = getDoubleValue() * factor;
    if (conversionData.computingFontSize() || isFontRelativeLength())
        return result;

    return result * conversionData.zoom();
}

void CSSPrimitiveValue::setFloatValue(unsigned short, double, ExceptionCode& ec)
{
    // Keeping values immutable makes optimizations easier and allows sharing of the primitive value objects.
    // No other engine supports mutating style through this API. Computed style is always read-only anyway.
    // Supporting setter would require making primitive value copy-on-write and taking care of style invalidation.
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

double CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(unsigned short unitType)
{
    double factor = 1.0;
    // FIXME: the switch can be replaced by an array of scale factors.
    switch (unitType) {
    // These are "canonical" units in their respective categories.
    case CSS_PX:
    case CSS_DEG:
    case CSS_MS:
    case CSS_HZ:
        break;
    case CSS_CM:
        factor = cssPixelsPerInch / 2.54; // (2.54 cm/in)
        break;
    case CSS_DPCM:
        factor = 2.54 / cssPixelsPerInch; // (2.54 cm/in)
        break;
    case CSS_MM:
        factor = cssPixelsPerInch / 25.4;
        break;
    case CSS_IN:
        factor = cssPixelsPerInch;
        break;
    case CSS_DPI:
        factor = 1 / cssPixelsPerInch;
        break;
    case CSS_PT:
        factor = cssPixelsPerInch / 72.0;
        break;
    case CSS_PC:
        factor = cssPixelsPerInch * 12.0 / 72.0; // 1 pc == 12 pt
        break;
    case CSS_RAD:
        factor = 180 / piDouble;
        break;
    case CSS_GRAD:
        factor = 0.9;
        break;
    case CSS_TURN:
        factor = 360;
        break;
    case CSS_S:
    case CSS_KHZ:
        factor = 1000;
        break;
    default:
        break;
    }

    return factor;
}

double CSSPrimitiveValue::getDoubleValue(unsigned short unitType, ExceptionCode& ec) const
{
    double result = 0;
    bool success = getDoubleValueInternal(static_cast<UnitTypes>(unitType), &result);
    if (!success) {
        ec = INVALID_ACCESS_ERR;
        return 0.0;
    }

    ec = 0;
    return result;
}

double CSSPrimitiveValue::getDoubleValue(unsigned short unitType) const
{
    double result = 0;
    getDoubleValueInternal(static_cast<UnitTypes>(unitType), &result);
    return result;
}

double CSSPrimitiveValue::getDoubleValue() const
{
    return m_primitiveUnitType != CSS_CALC ? m_value.num : m_value.calc->doubleValue();
}

CSSPrimitiveValue::UnitTypes CSSPrimitiveValue::canonicalUnitTypeForCategory(UnitCategory category)
{
    // The canonical unit type is chosen according to the way CSSParser::validUnit() chooses the default unit
    // in each category (based on unitflags).
    switch (category) {
    case UNumber:
        return CSS_NUMBER;
    case ULength:
        return CSS_PX;
    case UPercent:
        return CSS_UNKNOWN; // Cannot convert between numbers and percent.
    case UTime:
        return CSS_MS;
    case UAngle:
        return CSS_DEG;
    case UFrequency:
        return CSS_HZ;
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
    case UResolution:
        return CSS_DPPX;
#endif
    default:
        return CSS_UNKNOWN;
    }
}

bool CSSPrimitiveValue::getDoubleValueInternal(UnitTypes requestedUnitType, double* result) const
{
    if (!isValidCSSUnitTypeForDoubleConversion(static_cast<UnitTypes>(m_primitiveUnitType)) || !isValidCSSUnitTypeForDoubleConversion(requestedUnitType))
        return false;

    UnitTypes sourceUnitType = static_cast<UnitTypes>(primitiveType());
    if (requestedUnitType == sourceUnitType || requestedUnitType == CSS_DIMENSION) {
        *result = getDoubleValue();
        return true;
    }

    UnitCategory sourceCategory = unitCategory(sourceUnitType);
    ASSERT(sourceCategory != UOther);

    UnitTypes targetUnitType = requestedUnitType;
    UnitCategory targetCategory = unitCategory(targetUnitType);
    ASSERT(targetCategory != UOther);

    // Cannot convert between unrelated unit categories if one of them is not UNumber.
    if (sourceCategory != targetCategory && sourceCategory != UNumber && targetCategory != UNumber)
        return false;

    if (targetCategory == UNumber) {
        // We interpret conversion to CSS_NUMBER as conversion to a canonical unit in this value's category.
        targetUnitType = canonicalUnitTypeForCategory(sourceCategory);
        if (targetUnitType == CSS_UNKNOWN)
            return false;
    }

    if (sourceUnitType == CSS_NUMBER) {
        // We interpret conversion from CSS_NUMBER in the same way as CSSParser::validUnit() while using non-strict mode.
        sourceUnitType = canonicalUnitTypeForCategory(targetCategory);
        if (sourceUnitType == CSS_UNKNOWN)
            return false;
    }

    double convertedValue = getDoubleValue();

    // First convert the value from m_primitiveUnitType to canonical type.
    double factor = conversionToCanonicalUnitsScaleFactor(sourceUnitType);
    convertedValue *= factor;

    // Now convert from canonical type to the target unitType.
    factor = conversionToCanonicalUnitsScaleFactor(targetUnitType);
    convertedValue /= factor;

    *result = convertedValue;
    return true;
}

void CSSPrimitiveValue::setStringValue(unsigned short, const String&, ExceptionCode& ec)
{
    // Keeping values immutable makes optimizations easier and allows sharing of the primitive value objects.
    // No other engine supports mutating style through this API. Computed style is always read-only anyway.
    // Supporting setter would require making primitive value copy-on-write and taking care of style invalidation.
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

String CSSPrimitiveValue::getStringValue(ExceptionCode& ec) const
{
    ec = 0;
    switch (m_primitiveUnitType) {
    case CSS_STRING:
    case CSS_ATTR:
    case CSS_URI:
        return m_value.string;
    case CSS_VALUE_ID:
        return valueName(m_value.valueID);
    case CSS_PROPERTY_ID:
        return propertyName(m_value.propertyID);
    default:
        ec = INVALID_ACCESS_ERR;
        break;
    }

    return String();
}

String CSSPrimitiveValue::getStringValue() const
{
    switch (m_primitiveUnitType) {
    case CSS_STRING:
    case CSS_ATTR:
    case CSS_URI:
        return m_value.string;
    case CSS_VALUE_ID:
        return valueName(m_value.valueID);
    case CSS_PROPERTY_ID:
        return propertyName(m_value.propertyID);
    default:
        break;
    }

    return String();
}

Counter* CSSPrimitiveValue::getCounterValue(ExceptionCode& ec) const
{
    ec = 0;
    if (m_primitiveUnitType != CSS_COUNTER) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return m_value.counter;
}

Rect* CSSPrimitiveValue::getRectValue(ExceptionCode& ec) const
{
    ec = 0;
    if (m_primitiveUnitType != CSS_RECT) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return m_value.rect;
}

Quad* CSSPrimitiveValue::getQuadValue(ExceptionCode& ec) const
{
    ec = 0;
    if (m_primitiveUnitType != CSS_QUAD) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return m_value.quad;
}

#if ENABLE(CSS_SCROLL_SNAP)
LengthRepeat* CSSPrimitiveValue::getLengthRepeatValue(ExceptionCode& ec) const
{
    ec = 0;
    if (m_primitiveUnitType != CSS_LENGTH_REPEAT) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return m_value.lengthRepeat;
}
#endif

PassRefPtr<RGBColor> CSSPrimitiveValue::getRGBColorValue(ExceptionCode& ec) const
{
    ec = 0;
    if (m_primitiveUnitType != CSS_RGBCOLOR) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    // FIMXE: This should not return a new object for each invocation.
    return RGBColor::create(m_value.rgbcolor);
}

Pair* CSSPrimitiveValue::getPairValue(ExceptionCode& ec) const
{
    ec = 0;
    if (m_primitiveUnitType != CSS_PAIR) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return m_value.pair;
}

NEVER_INLINE PassRef<StringImpl> CSSPrimitiveValue::formatNumberValue(const char* suffix, unsigned suffixLength) const
{
    DecimalNumber decimal(m_value.num);

    unsigned bufferLength = decimal.bufferLengthForStringDecimal() + suffixLength;
    LChar* buffer;
    PassRef<StringImpl> string = StringImpl::createUninitialized(bufferLength, buffer);

    unsigned length = decimal.toStringDecimal(buffer, bufferLength);

    for (unsigned i = 0; i < suffixLength; ++i)
        buffer[length + i] = static_cast<LChar>(suffix[i]);

    return WTF::move(string);
}

template <unsigned characterCount>
ALWAYS_INLINE PassRef<StringImpl> CSSPrimitiveValue::formatNumberValue(const char (&characters)[characterCount]) const
{
    return formatNumberValue(characters, characterCount - 1);
}

ALWAYS_INLINE String CSSPrimitiveValue::formatNumberForCustomCSSText() const
{
    switch (m_primitiveUnitType) {
    case CSS_UNKNOWN:
        return String();
    case CSS_NUMBER:
    case CSS_PARSER_INTEGER:
        return formatNumberValue("");
    case CSS_PERCENTAGE:
        return formatNumberValue("%");
    case CSS_EMS:
        return formatNumberValue("em");
    case CSS_EXS:
        return formatNumberValue("ex");
    case CSS_REMS:
        return formatNumberValue("rem");
    case CSS_CHS:
        return formatNumberValue("ch");
    case CSS_PX:
        return formatNumberValue("px");
    case CSS_CM:
        return formatNumberValue("cm");
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
    case CSS_DPPX:
        return formatNumberValue("dppx");
    case CSS_DPI:
        return formatNumberValue("dpi");
    case CSS_DPCM:
        return formatNumberValue("dpcm");
#endif
    case CSS_MM:
        return formatNumberValue("mm");
    case CSS_IN:
        return formatNumberValue("in");
    case CSS_PT:
        return formatNumberValue("pt");
    case CSS_PC:
        return formatNumberValue("pc");
    case CSS_DEG:
        return formatNumberValue("deg");
    case CSS_RAD:
        return formatNumberValue("rad");
    case CSS_GRAD:
        return formatNumberValue("grad");
    case CSS_MS:
        return formatNumberValue("ms");
    case CSS_S:
        return formatNumberValue("s");
    case CSS_HZ:
        return formatNumberValue("hz");
    case CSS_KHZ:
        return formatNumberValue("khz");
    case CSS_TURN:
        return formatNumberValue("turn");
    case CSS_FR:
        return formatNumberValue("fr");
    case CSS_DIMENSION:
        // FIXME: We currently don't handle CSS_DIMENSION properly as we don't store
        // the actual dimension, just the numeric value as a string.
    case CSS_STRING:
        return quoteCSSStringIfNeeded(m_value.string);
    case CSS_URI:
        return "url(" + quoteCSSURLIfNeeded(m_value.string) + ')';
    case CSS_VALUE_ID:
        return valueName(m_value.valueID);
    case CSS_PROPERTY_ID:
        return propertyName(m_value.propertyID);
    case CSS_ATTR: {
        StringBuilder result;
        result.reserveCapacity(6 + m_value.string->length());
        result.appendLiteral("attr(");
        result.append(m_value.string);
        result.append(')');

        return result.toString();
    }
    case CSS_COUNTER_NAME:
        return "counter(" + String(m_value.string) + ')';
    case CSS_COUNTER: {
        StringBuilder result;
        String separator = m_value.counter->separator();
        if (separator.isEmpty())
            result.appendLiteral("counter(");
        else
            result.appendLiteral("counters(");

        result.append(m_value.counter->identifier());
        if (!separator.isEmpty()) {
            result.appendLiteral(", ");
            result.append(quoteCSSStringIfNeeded(separator));
        }
        String listStyle = m_value.counter->listStyle();
        if (!listStyle.isEmpty()) {
            result.appendLiteral(", ");
            result.append(listStyle);
        }
        result.append(')');

        return result.toString();
    }
    case CSS_RECT:
        return getRectValue()->cssText();
    case CSS_QUAD:
        return getQuadValue()->cssText();
#if ENABLE(CSS_SCROLL_SNAP)
    case CSS_LENGTH_REPEAT:
        return getLengthRepeatValue()->cssText();
#endif
    case CSS_RGBCOLOR:
    case CSS_PARSER_HEXCOLOR: {
        RGBA32 rgbColor = m_value.rgbcolor;
        if (m_primitiveUnitType == CSS_PARSER_HEXCOLOR)
            Color::parseHexColor(m_value.string, rgbColor);
        Color color(rgbColor);

        Vector<LChar> result;
        result.reserveInitialCapacity(32);
        bool colorHasAlpha = color.hasAlpha();
        if (colorHasAlpha)
            result.append("rgba(", 5);
        else
            result.append("rgb(", 4);

        appendNumber(result, static_cast<unsigned char>(color.red()));
        result.append(", ", 2);

        appendNumber(result, static_cast<unsigned char>(color.green()));
        result.append(", ", 2);

        appendNumber(result, static_cast<unsigned char>(color.blue()));
        if (colorHasAlpha) {
            result.append(", ", 2);

            NumberToStringBuffer buffer;
            const char* alphaString = numberToFixedPrecisionString(color.alpha() / 255.0f, 6, buffer, true);
            result.append(alphaString, strlen(alphaString));
        }

        result.append(')');
        return String::adopt(result);
    }
    case CSS_PAIR:
        return getPairValue()->cssText();
#if ENABLE(DASHBOARD_SUPPORT)
    case CSS_DASHBOARD_REGION: {
        StringBuilder result;
        for (DashboardRegion* region = getDashboardRegionValue(); region; region = region->m_next.get()) {
            if (!result.isEmpty())
                result.append(' ');
            result.appendLiteral("dashboard-region(");
            result.append(region->m_label);
            if (region->m_isCircle)
                result.appendLiteral(" circle");
            else if (region->m_isRectangle)
                result.appendLiteral(" rectangle");
            else
                break;
            if (region->top()->m_primitiveUnitType == CSS_VALUE_ID && region->top()->getValueID() == CSSValueInvalid) {
                ASSERT(region->right()->m_primitiveUnitType == CSS_VALUE_ID);
                ASSERT(region->bottom()->m_primitiveUnitType == CSS_VALUE_ID);
                ASSERT(region->left()->m_primitiveUnitType == CSS_VALUE_ID);
                ASSERT(region->right()->getValueID() == CSSValueInvalid);
                ASSERT(region->bottom()->getValueID() == CSSValueInvalid);
                ASSERT(region->left()->getValueID() == CSSValueInvalid);
            } else {
                result.append(' ');
                result.append(region->top()->cssText());
                result.append(' ');
                result.append(region->right()->cssText());
                result.append(' ');
                result.append(region->bottom()->cssText());
                result.append(' ');
                result.append(region->left()->cssText());
            }
            result.append(')');
        }
        return result.toString();
    }
#endif
    case CSS_PARSER_OPERATOR: {
        char c = static_cast<char>(m_value.parserOperator);
        return String(&c, 1U);
    }
    case CSS_PARSER_IDENTIFIER:
        return quoteCSSStringIfNeeded(m_value.string);
    case CSS_CALC:
        return m_value.calc->cssText();
    case CSS_SHAPE:
        return m_value.shape->cssText();
    case CSS_VW:
        return formatNumberValue("vw");
    case CSS_VH:
        return formatNumberValue("vh");
    case CSS_VMIN:
        return formatNumberValue("vmin");
    case CSS_VMAX:
        return formatNumberValue("vmax");
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

void CSSPrimitiveValue::addSubresourceStyleURLs(ListHashSet<URL>& urls, const StyleSheetContents* styleSheet) const
{
    if (m_primitiveUnitType == CSS_URI)
        addSubresourceURL(urls, styleSheet->completeURL(m_value.string));
}

PassRefPtr<CSSPrimitiveValue> CSSPrimitiveValue::cloneForCSSOM() const
{
    RefPtr<CSSPrimitiveValue> result;

    switch (m_primitiveUnitType) {
    case CSS_STRING:
    case CSS_URI:
    case CSS_ATTR:
    case CSS_COUNTER_NAME:
        result = CSSPrimitiveValue::create(m_value.string, static_cast<UnitTypes>(m_primitiveUnitType));
        break;
    case CSS_COUNTER:
        result = CSSPrimitiveValue::create(m_value.counter->cloneForCSSOM());
        break;
    case CSS_RECT:
        result = CSSPrimitiveValue::create(m_value.rect->cloneForCSSOM());
        break;
    case CSS_QUAD:
        result = CSSPrimitiveValue::create(m_value.quad->cloneForCSSOM());
        break;
#if ENABLE(CSS_SCROLL_SNAP)
    case CSS_LENGTH_REPEAT:
        result = CSSPrimitiveValue::create(m_value.lengthRepeat->cloneForCSSOM());
        break;
#endif
    case CSS_PAIR:
        // Pair is not exposed to the CSSOM, no need for a deep clone.
        result = CSSPrimitiveValue::create(m_value.pair);
        break;
#if ENABLE(DASHBOARD_SUPPORT)
    case CSS_DASHBOARD_REGION:
        // DashboardRegion is not exposed to the CSSOM, no need for a deep clone.
        result = CSSPrimitiveValue::create(m_value.region);
        break;
#endif
    case CSS_CALC:
        // CSSCalcValue is not exposed to the CSSOM, no need for a deep clone.
        result = CSSPrimitiveValue::create(m_value.calc);
        break;
    case CSS_SHAPE:
        // CSSShapeValue is not exposed to the CSSOM, no need for a deep clone.
        result = CSSPrimitiveValue::create(m_value.shape);
        break;
    case CSS_NUMBER:
    case CSS_PARSER_INTEGER:
    case CSS_PERCENTAGE:
    case CSS_EMS:
    case CSS_EXS:
    case CSS_REMS:
    case CSS_CHS:
    case CSS_PX:
    case CSS_CM:
    case CSS_MM:
    case CSS_IN:
    case CSS_PT:
    case CSS_PC:
    case CSS_DEG:
    case CSS_RAD:
    case CSS_GRAD:
    case CSS_MS:
    case CSS_S:
    case CSS_HZ:
    case CSS_KHZ:
    case CSS_TURN:
    case CSS_VW:
    case CSS_VH:
    case CSS_VMIN:
    case CSS_VMAX:
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
    case CSS_DPPX:
    case CSS_DPI:
    case CSS_DPCM:
#endif
    case CSS_FR:
        result = CSSPrimitiveValue::create(m_value.num, static_cast<UnitTypes>(m_primitiveUnitType));
        break;
    case CSS_PROPERTY_ID:
        result = CSSPrimitiveValue::createIdentifier(m_value.propertyID);
        break;
    case CSS_VALUE_ID:
        result = CSSPrimitiveValue::createIdentifier(m_value.valueID);
        break;
    case CSS_RGBCOLOR:
        result = CSSPrimitiveValue::createColor(m_value.rgbcolor);
        break;
    case CSS_DIMENSION:
    case CSS_UNKNOWN:
    case CSS_PARSER_OPERATOR:
    case CSS_PARSER_IDENTIFIER:
    case CSS_PARSER_HEXCOLOR:
        ASSERT_NOT_REACHED();
        break;
    }
    if (result)
        result->setCSSOMSafe();

    return result;
}

bool CSSPrimitiveValue::equals(const CSSPrimitiveValue& other) const
{
    if (m_primitiveUnitType != other.m_primitiveUnitType)
        return false;

    switch (m_primitiveUnitType) {
    case CSS_UNKNOWN:
        return false;
    case CSS_NUMBER:
    case CSS_PARSER_INTEGER:
    case CSS_PERCENTAGE:
    case CSS_EMS:
    case CSS_EXS:
    case CSS_REMS:
    case CSS_PX:
    case CSS_CM:
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
    case CSS_DPPX:
    case CSS_DPI:
    case CSS_DPCM:
#endif
    case CSS_MM:
    case CSS_IN:
    case CSS_PT:
    case CSS_PC:
    case CSS_DEG:
    case CSS_RAD:
    case CSS_GRAD:
    case CSS_MS:
    case CSS_S:
    case CSS_HZ:
    case CSS_KHZ:
    case CSS_TURN:
    case CSS_VW:
    case CSS_VH:
    case CSS_VMIN:
    case CSS_DIMENSION:
    case CSS_FR:
        return m_value.num == other.m_value.num;
    case CSS_PROPERTY_ID:
        return propertyName(m_value.propertyID) == propertyName(other.m_value.propertyID);
    case CSS_VALUE_ID:
        return valueName(m_value.valueID) == valueName(other.m_value.valueID);
    case CSS_STRING:
    case CSS_URI:
    case CSS_ATTR:
    case CSS_COUNTER_NAME:
    case CSS_PARSER_IDENTIFIER:
    case CSS_PARSER_HEXCOLOR:
        return equal(m_value.string, other.m_value.string);
    case CSS_COUNTER:
        return m_value.counter && other.m_value.counter && m_value.counter->equals(*other.m_value.counter);
    case CSS_RECT:
        return m_value.rect && other.m_value.rect && m_value.rect->equals(*other.m_value.rect);
    case CSS_QUAD:
        return m_value.quad && other.m_value.quad && m_value.quad->equals(*other.m_value.quad);
#if ENABLE(CSS_SCROLL_SNAP)
    case CSS_LENGTH_REPEAT:
        return m_value.lengthRepeat && other.m_value.lengthRepeat && m_value.lengthRepeat->equals(*other.m_value.lengthRepeat);
#endif
    case CSS_RGBCOLOR:
        return m_value.rgbcolor == other.m_value.rgbcolor;
    case CSS_PAIR:
        return m_value.pair && other.m_value.pair && m_value.pair->equals(*other.m_value.pair);
#if ENABLE(DASHBOARD_SUPPORT)
    case CSS_DASHBOARD_REGION:
        return m_value.region && other.m_value.region && m_value.region->equals(*other.m_value.region);
#endif
    case CSS_PARSER_OPERATOR:
        return m_value.parserOperator == other.m_value.parserOperator;
    case CSS_CALC:
        return m_value.calc && other.m_value.calc && m_value.calc->equals(*other.m_value.calc);
    case CSS_SHAPE:
        return m_value.shape && other.m_value.shape && m_value.shape->equals(*other.m_value.shape);
    }
    return false;
}

} // namespace WebCore
