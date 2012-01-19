/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "CSSHelper.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "CSSWrapShapes.h"
#include "Color.h"
#include "Counter.h"
#include "ExceptionCode.h"
#include "Node.h"
#include "Pair.h"
#include "RGBColor.h"
#include "Rect.h"
#include "RenderStyle.h"
#include <wtf/ASCIICType.h>
#include <wtf/DecimalNumber.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

using namespace WTF;

namespace WebCore {

static inline bool isValidCSSUnitTypeForDoubleConversion(CSSPrimitiveValue::UnitTypes unitType)
{
    switch (unitType) {
    case CSSPrimitiveValue:: CSS_CM:
    case CSSPrimitiveValue:: CSS_DEG:
    case CSSPrimitiveValue:: CSS_DIMENSION:
    case CSSPrimitiveValue:: CSS_EMS:
    case CSSPrimitiveValue:: CSS_EXS:
    case CSSPrimitiveValue:: CSS_GRAD:
    case CSSPrimitiveValue:: CSS_HZ:
    case CSSPrimitiveValue:: CSS_IN:
    case CSSPrimitiveValue:: CSS_KHZ:
    case CSSPrimitiveValue:: CSS_MM:
    case CSSPrimitiveValue:: CSS_MS:
    case CSSPrimitiveValue:: CSS_NUMBER:
    case CSSPrimitiveValue:: CSS_PERCENTAGE:
    case CSSPrimitiveValue:: CSS_PC:
    case CSSPrimitiveValue:: CSS_PT:
    case CSSPrimitiveValue:: CSS_PX:
    case CSSPrimitiveValue:: CSS_RAD:
    case CSSPrimitiveValue:: CSS_REMS:
    case CSSPrimitiveValue:: CSS_S:
    case CSSPrimitiveValue:: CSS_TURN:
        return true;
    case CSSPrimitiveValue:: CSS_ATTR:
    case CSSPrimitiveValue:: CSS_COUNTER:
    case CSSPrimitiveValue:: CSS_COUNTER_NAME:
    case CSSPrimitiveValue:: CSS_DASHBOARD_REGION:
    case CSSPrimitiveValue:: CSS_IDENT:
    case CSSPrimitiveValue:: CSS_PAIR:
    case CSSPrimitiveValue:: CSS_PARSER_HEXCOLOR:
    case CSSPrimitiveValue:: CSS_PARSER_IDENTIFIER:
    case CSSPrimitiveValue:: CSS_PARSER_INTEGER:
    case CSSPrimitiveValue:: CSS_PARSER_OPERATOR:
    case CSSPrimitiveValue:: CSS_RECT:
    case CSSPrimitiveValue:: CSS_QUAD:
    case CSSPrimitiveValue:: CSS_RGBCOLOR:
    case CSSPrimitiveValue:: CSS_SHAPE:
    case CSSPrimitiveValue:: CSS_STRING:
    case CSSPrimitiveValue:: CSS_UNICODE_RANGE:
    case CSSPrimitiveValue:: CSS_UNKNOWN:
    case CSSPrimitiveValue:: CSS_URI:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static CSSPrimitiveValue::UnitCategory unitCategory(CSSPrimitiveValue::UnitTypes type)
{
    // Here we violate the spec (http://www.w3.org/TR/DOM-Level-2-Style/css.html#CSS-CSSPrimitiveValue) and allow conversions
    // between CSS_PX and relative lengths (see cssPixelsPerInch comment in CSSHelper.h for the topic treatment).
    switch (type) {
    case CSSPrimitiveValue::CSS_NUMBER:
        return CSSPrimitiveValue::UNumber;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        return CSSPrimitiveValue::UPercent;
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
        return CSSPrimitiveValue::ULength;
    case CSSPrimitiveValue::CSS_MS:
    case CSSPrimitiveValue::CSS_S:
        return CSSPrimitiveValue::UTime;
    case CSSPrimitiveValue::CSS_DEG:
    case CSSPrimitiveValue::CSS_RAD:
    case CSSPrimitiveValue::CSS_GRAD:
    case CSSPrimitiveValue::CSS_TURN:
        return CSSPrimitiveValue::UAngle;
    case CSSPrimitiveValue::CSS_HZ:
    case CSSPrimitiveValue::CSS_KHZ:
        return CSSPrimitiveValue::UFrequency;
    default:
        return CSSPrimitiveValue::UOther;
    }
}

typedef HashMap<const CSSPrimitiveValue*, String> CSSTextCache;
static CSSTextCache& cssTextCache()
{
    DEFINE_STATIC_LOCAL(CSSTextCache, cache, ());
    return cache;
}

static const AtomicString& valueOrPropertyName(int valueOrPropertyID)
{
    ASSERT_ARG(valueOrPropertyID, valueOrPropertyID >= 0);
    ASSERT_ARG(valueOrPropertyID, valueOrPropertyID < numCSSValueKeywords || (valueOrPropertyID >= firstCSSProperty && valueOrPropertyID < firstCSSProperty + numCSSProperties));

    if (valueOrPropertyID < 0)
        return nullAtom;

    if (valueOrPropertyID < numCSSValueKeywords) {
        static AtomicString* keywordStrings = new AtomicString[numCSSValueKeywords]; // Leaked intentionally.
        AtomicString& keywordString = keywordStrings[valueOrPropertyID];
        if (keywordString.isNull())
            keywordString = getValueName(valueOrPropertyID);
        return keywordString;
    }

    if (valueOrPropertyID >= firstCSSProperty && valueOrPropertyID < firstCSSProperty + numCSSProperties) {
        static AtomicString* propertyStrings = new AtomicString[numCSSProperties]; // Leaked intentionally.
        AtomicString& propertyString = propertyStrings[valueOrPropertyID - firstCSSProperty];
        if (propertyString.isNull())
            propertyString = getPropertyName(static_cast<CSSPropertyID>(valueOrPropertyID));
        return propertyString;
    }

    return nullAtom;
}

CSSPrimitiveValue::CSSPrimitiveValue()
    : CSSValue(PrimitiveClass)
{
}

CSSPrimitiveValue::CSSPrimitiveValue(int ident)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_IDENT;
    m_value.ident = ident;
}

CSSPrimitiveValue::CSSPrimitiveValue(ClassType classType, int ident)
    : CSSValue(classType)
{
    m_primitiveUnitType = CSS_IDENT;
    m_value.ident = ident;
}

CSSPrimitiveValue::CSSPrimitiveValue(double num, UnitTypes type)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = type;
    ASSERT(isfinite(num));
    m_value.num = num;
}

CSSPrimitiveValue::CSSPrimitiveValue(const String& str, UnitTypes type)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = type;
    if ((m_value.string = str.impl()))
        m_value.string->ref();
}


CSSPrimitiveValue::CSSPrimitiveValue(ClassType classType, const String& str, UnitTypes type)
    : CSSValue(classType)
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
    switch (length.type()) {
        case Auto:
            m_primitiveUnitType = CSS_IDENT;
            m_value.ident = CSSValueAuto;
            break;
        case WebCore::Fixed:
            m_primitiveUnitType = CSS_PX;
            m_value.num = length.value();
            break;
        case Intrinsic:
            m_primitiveUnitType = CSS_IDENT;
            m_value.ident = CSSValueIntrinsic;
            break;
        case MinIntrinsic:
            m_primitiveUnitType = CSS_IDENT;
            m_value.ident = CSSValueMinIntrinsic;
            break;
        case Percent:
            m_primitiveUnitType = CSS_PERCENTAGE;
            ASSERT(isfinite(length.percent()));
            m_value.num = length.percent();
            break;
        case Relative:
        case Undefined:
            ASSERT_NOT_REACHED();
            break;
    }
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

void CSSPrimitiveValue::init(PassRefPtr<CSSWrapShape> shape)
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
    switch (m_primitiveUnitType) {
        case CSS_STRING:
        case CSS_URI:
        case CSS_ATTR:
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
        case CSS_PAIR:
            m_value.pair->deref();
            break;
#if ENABLE(DASHBOARD_SUPPORT)
        case CSS_DASHBOARD_REGION:
            if (m_value.region)
                m_value.region->deref();
            break;
#endif
        case CSS_SHAPE:
            m_value.shape->deref();
            break;
        default:
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

template<> int CSSPrimitiveValue::computeLength(RenderStyle* style, RenderStyle* rootStyle, double multiplier, bool computingFontSize)
{
    return roundForImpreciseConversion<int, INT_MAX, INT_MIN>(computeLengthDouble(style, rootStyle, multiplier, computingFontSize));
}

template<> unsigned CSSPrimitiveValue::computeLength(RenderStyle* style, RenderStyle* rootStyle, double multiplier, bool computingFontSize)
{
    return roundForImpreciseConversion<unsigned, UINT_MAX, 0>(computeLengthDouble(style, rootStyle, multiplier, computingFontSize));
}

template<> Length CSSPrimitiveValue::computeLength(RenderStyle* style, RenderStyle* rootStyle, double multiplier, bool computingFontSize)
{
    // FIXME: Length.h no longer expects 28 bit integers, so these bounds should be INT_MAX and INT_MIN
    return Length(roundForImpreciseConversion<int, intMaxForLength, intMinForLength>(computeLengthDouble(style, rootStyle, multiplier, computingFontSize)), Fixed);
}

template<> short CSSPrimitiveValue::computeLength(RenderStyle* style, RenderStyle* rootStyle, double multiplier, bool computingFontSize)
{
    return roundForImpreciseConversion<short, SHRT_MAX, SHRT_MIN>(computeLengthDouble(style, rootStyle, multiplier, computingFontSize));
}

template<> unsigned short CSSPrimitiveValue::computeLength(RenderStyle* style, RenderStyle* rootStyle, double multiplier, bool computingFontSize)
{
    return roundForImpreciseConversion<unsigned short, USHRT_MAX, 0>(computeLengthDouble(style, rootStyle, multiplier, computingFontSize));
}

template<> float CSSPrimitiveValue::computeLength(RenderStyle* style, RenderStyle* rootStyle, double multiplier, bool computingFontSize)
{
    return static_cast<float>(computeLengthDouble(style, rootStyle, multiplier, computingFontSize));
}

template<> double CSSPrimitiveValue::computeLength(RenderStyle* style, RenderStyle* rootStyle, double multiplier, bool computingFontSize)
{
    return computeLengthDouble(style, rootStyle, multiplier, computingFontSize);
}

double CSSPrimitiveValue::computeLengthDouble(RenderStyle* style, RenderStyle* rootStyle, double multiplier, bool computingFontSize)
{
    unsigned short type = primitiveType();

    // We do not apply the zoom factor when we are computing the value of the font-size property.  The zooming
    // for font sizes is much more complicated, since we have to worry about enforcing the minimum font size preference
    // as well as enforcing the implicit "smart minimum."  In addition the CSS property text-size-adjust is used to
    // prevent text from zooming at all.  Therefore we will not apply the zoom here if we are computing font-size.
    bool applyZoomMultiplier = !computingFontSize;

    double factor = 1.0;
    switch (type) {
        case CSS_EMS:
            applyZoomMultiplier = false;
            factor = computingFontSize ? style->fontDescription().specifiedSize() : style->fontDescription().computedSize();
            break;
        case CSS_EXS:
            // FIXME: We have a bug right now where the zoom will be applied twice to EX units.
            // We really need to compute EX using fontMetrics for the original specifiedSize and not use
            // our actual constructed rendering font.
            applyZoomMultiplier = false;
            factor = style->fontMetrics().xHeight();
            break;
        case CSS_REMS:
            applyZoomMultiplier = false;
            factor = computingFontSize ? rootStyle->fontDescription().specifiedSize() : rootStyle->fontDescription().computedSize();
            break;
        case CSS_PX:
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
        default:
            ASSERT_NOT_REACHED();
            return -1.0;
    }

    double result = getDoubleValue() * factor;
    if (!applyZoomMultiplier || multiplier == 1.0)
        return result;

    // Any original result that was >= 1 should not be allowed to fall below 1.  This keeps border lines from
    // vanishing.
    double zoomedResult = result * multiplier;
    if (result >= 1.0)
        zoomedResult = max(1.0, zoomedResult);
    return zoomedResult;
}

void CSSPrimitiveValue::setFloatValue(unsigned short, double, ExceptionCode& ec)
{
    // Keeping values immutable makes optimizations easier and allows sharing of the primitive value objects.
    // No other engine supports mutating style through this API. Computed style is always read-only anyway.
    // Supporting setter would require making primitive value copy-on-write and taking care of style invalidation.
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

static double conversionToCanonicalUnitsScaleFactor(unsigned short unitType)
{
    double factor = 1.0;
    // FIXME: the switch can be replaced by an array of scale factors.
    switch (unitType) {
        // These are "canonical" units in their respective categories.
        case CSSPrimitiveValue::CSS_PX:
        case CSSPrimitiveValue::CSS_DEG:
        case CSSPrimitiveValue::CSS_MS:
        case CSSPrimitiveValue::CSS_HZ:
            break;
        case CSSPrimitiveValue::CSS_CM:
            factor = cssPixelsPerInch / 2.54; // (2.54 cm/in)
            break;
        case CSSPrimitiveValue::CSS_MM:
            factor = cssPixelsPerInch / 25.4;
            break;
        case CSSPrimitiveValue::CSS_IN:
            factor = cssPixelsPerInch;
            break;
        case CSSPrimitiveValue::CSS_PT:
            factor = cssPixelsPerInch / 72.0;
            break;
        case CSSPrimitiveValue::CSS_PC:
            factor = cssPixelsPerInch * 12.0 / 72.0; // 1 pc == 12 pt
            break;
        case CSSPrimitiveValue::CSS_RAD:
            factor = 180 / piDouble;
            break;
        case CSSPrimitiveValue::CSS_GRAD:
            factor = 0.9;
            break;
        case CSSPrimitiveValue::CSS_TURN:
            factor = 360;
            break;
        case CSSPrimitiveValue::CSS_S:
        case CSSPrimitiveValue::CSS_KHZ:
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
    default:
        return CSS_UNKNOWN;
    }
}

bool CSSPrimitiveValue::getDoubleValueInternal(UnitTypes requestedUnitType, double* result) const
{
    if (!isValidCSSUnitTypeForDoubleConversion(static_cast<UnitTypes>(m_primitiveUnitType)) || !isValidCSSUnitTypeForDoubleConversion(requestedUnitType))
        return false;
    if (requestedUnitType == static_cast<UnitTypes>(m_primitiveUnitType) || requestedUnitType == CSS_DIMENSION) {
        *result = m_value.num;
        return true;
    }

    UnitTypes sourceUnitType = static_cast<UnitTypes>(m_primitiveUnitType);
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

    double convertedValue = m_value.num;

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
        case CSS_IDENT:
            return valueOrPropertyName(m_value.ident);
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
        case CSS_IDENT:
            return valueOrPropertyName(m_value.ident);
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

static String formatNumber(double number)
{
    DecimalNumber decimal(number);

    StringBuffer<UChar> buffer(decimal.bufferLengthForStringDecimal());
    unsigned length = decimal.toStringDecimal(buffer.characters(), buffer.length());
    ASSERT_UNUSED(length, length == buffer.length());

    return String::adopt(buffer);
}

String CSSPrimitiveValue::customCssText() const
{
    // FIXME: return the original value instead of a generated one (e.g. color
    // name if it was specified) - check what spec says about this

    if (m_hasCachedCSSText) {
        ASSERT(cssTextCache().contains(this));
        return cssTextCache().get(this);
    }

    String text;
    switch (m_primitiveUnitType) {
        case CSS_UNKNOWN:
            // FIXME
            break;
        case CSS_NUMBER:
        case CSS_PARSER_INTEGER:
            text = formatNumber(m_value.num);
            break;
        case CSS_PERCENTAGE:
            text = formatNumber(m_value.num) + "%";
            break;
        case CSS_EMS:
            text = formatNumber(m_value.num) + "em";
            break;
        case CSS_EXS:
            text = formatNumber(m_value.num) + "ex";
            break;
        case CSS_REMS:
            text = formatNumber(m_value.num) + "rem";
            break;
        case CSS_PX:
            text = formatNumber(m_value.num) + "px";
            break;
        case CSS_CM:
            text = formatNumber(m_value.num) + "cm";
            break;
        case CSS_MM:
            text = formatNumber(m_value.num) + "mm";
            break;
        case CSS_IN:
            text = formatNumber(m_value.num) + "in";
            break;
        case CSS_PT:
            text = formatNumber(m_value.num) + "pt";
            break;
        case CSS_PC:
            text = formatNumber(m_value.num) + "pc";
            break;
        case CSS_DEG:
            text = formatNumber(m_value.num) + "deg";
            break;
        case CSS_RAD:
            text = formatNumber(m_value.num) + "rad";
            break;
        case CSS_GRAD:
            text = formatNumber(m_value.num) + "grad";
            break;
        case CSS_MS:
            text = formatNumber(m_value.num) + "ms";
            break;
        case CSS_S:
            text = formatNumber(m_value.num) + "s";
            break;
        case CSS_HZ:
            text = formatNumber(m_value.num) + "hz";
            break;
        case CSS_KHZ:
            text = formatNumber(m_value.num) + "khz";
            break;
        case CSS_TURN:
            text = formatNumber(m_value.num) + "turn";
            break;
        case CSS_DIMENSION:
            // FIXME
            break;
        case CSS_STRING:
            text = quoteCSSStringIfNeeded(m_value.string);
            break;
        case CSS_URI:
            text = "url(" + quoteCSSURLIfNeeded(m_value.string) + ")";
            break;
        case CSS_IDENT:
            text = valueOrPropertyName(m_value.ident);
            break;
        case CSS_ATTR: {
            DEFINE_STATIC_LOCAL(const String, attrParen, ("attr("));

            StringBuilder result;
            result.reserveCapacity(6 + m_value.string->length());

            result.append(attrParen);
            result.append(m_value.string);
            result.append(')');

            text = result.toString();
            break;
        }
        case CSS_COUNTER_NAME:
            text = "counter(";
            text += m_value.string;
            text += ")";
            break;
        case CSS_COUNTER: {
            DEFINE_STATIC_LOCAL(const String, counterParen, ("counter("));
            DEFINE_STATIC_LOCAL(const String, countersParen, ("counters("));
            DEFINE_STATIC_LOCAL(const String, commaSpace, (", "));

            StringBuilder result;
            String separator = m_value.counter->separator();
            result.append(separator.isEmpty() ? counterParen : countersParen);

            result.append(m_value.counter->identifier());
            if (!separator.isEmpty()) {
                result.append(commaSpace);
                result.append(quoteCSSStringIfNeeded(separator));
            }
            String listStyle = m_value.counter->listStyle();
            if (!listStyle.isEmpty()) {
                result.append(commaSpace);
                result.append(listStyle);
            }
            result.append(')');

            text = result.toString();
            break;
        }
        case CSS_RECT: {
            DEFINE_STATIC_LOCAL(const String, rectParen, ("rect("));

            Rect* rectVal = getRectValue();
            StringBuilder result;
            result.reserveCapacity(32);
            result.append(rectParen);

            result.append(rectVal->top()->cssText());
            result.append(' ');

            result.append(rectVal->right()->cssText());
            result.append(' ');

            result.append(rectVal->bottom()->cssText());
            result.append(' ');

            result.append(rectVal->left()->cssText());
            result.append(')');

            text = result.toString();
            break;
        }
        case CSS_QUAD: {
            Quad* quadVal = getQuadValue();
            Vector<UChar> result;
            result.reserveInitialCapacity(32);
            append(result, quadVal->top()->cssText());
            if (quadVal->right() != quadVal->top() || quadVal->bottom() != quadVal->top() || quadVal->left() != quadVal->top()) {
                result.append(' ');
                append(result, quadVal->right()->cssText());
                if (quadVal->bottom() != quadVal->top() || quadVal->right() != quadVal->left()) {
                    result.append(' ');
                    append(result, quadVal->bottom()->cssText());
                    if (quadVal->left() != quadVal->right()) {
                        result.append(' ');
                        append(result, quadVal->left()->cssText());
                    }
                }
            }
            text = String::adopt(result);
            break;
        }
        case CSS_RGBCOLOR:
        case CSS_PARSER_HEXCOLOR: {
            DEFINE_STATIC_LOCAL(const String, commaSpace, (", "));
            DEFINE_STATIC_LOCAL(const String, rgbParen, ("rgb("));
            DEFINE_STATIC_LOCAL(const String, rgbaParen, ("rgba("));

            RGBA32 rgbColor = m_value.rgbcolor;
            if (m_primitiveUnitType == CSS_PARSER_HEXCOLOR)
                Color::parseHexColor(m_value.string, rgbColor);
            Color color(rgbColor);

            Vector<UChar> result;
            result.reserveInitialCapacity(32);
            if (color.hasAlpha())
                append(result, rgbaParen);
            else
                append(result, rgbParen);

            appendNumber(result, static_cast<unsigned char>(color.red()));
            append(result, commaSpace);

            appendNumber(result, static_cast<unsigned char>(color.green()));
            append(result, commaSpace);

            appendNumber(result, static_cast<unsigned char>(color.blue()));
            if (color.hasAlpha()) {
                append(result, commaSpace);
                append(result, String::number(color.alpha() / 256.0f));
            }

            result.append(')');
            text = String::adopt(result);
            break;
        }
        case CSS_PAIR:
            text = m_value.pair->first()->cssText();
            if (m_value.pair->second() != m_value.pair->first()) {
                text += " ";
                text += m_value.pair->second()->cssText();
            }
            break;
#if ENABLE(DASHBOARD_SUPPORT)
        case CSS_DASHBOARD_REGION:
            for (DashboardRegion* region = getDashboardRegionValue(); region; region = region->m_next.get()) {
                if (!text.isEmpty())
                    text.append(' ');
                text += "dashboard-region(";
                text += region->m_label;
                if (region->m_isCircle)
                    text += " circle";
                else if (region->m_isRectangle)
                    text += " rectangle";
                else
                    break;
                if (region->top()->m_primitiveUnitType == CSS_IDENT && region->top()->getIdent() == CSSValueInvalid) {
                    ASSERT(region->right()->m_primitiveUnitType == CSS_IDENT);
                    ASSERT(region->bottom()->m_primitiveUnitType == CSS_IDENT);
                    ASSERT(region->left()->m_primitiveUnitType == CSS_IDENT);
                    ASSERT(region->right()->getIdent() == CSSValueInvalid);
                    ASSERT(region->bottom()->getIdent() == CSSValueInvalid);
                    ASSERT(region->left()->getIdent() == CSSValueInvalid);
                } else {
                    text.append(' ');
                    text += region->top()->cssText() + " ";
                    text += region->right()->cssText() + " ";
                    text += region->bottom()->cssText() + " ";
                    text += region->left()->cssText();
                }
                text += ")";
            }
            break;
#endif
        case CSS_PARSER_OPERATOR: {
            char c = static_cast<char>(m_value.ident);
            text = String(&c, 1U);
            break;
        }
        case CSS_PARSER_IDENTIFIER:
            text = quoteCSSStringIfNeeded(m_value.string);
            break;
        case CSS_SHAPE:
            text = m_value.shape->cssText();
            break;
    }

    ASSERT(!cssTextCache().contains(this));
    cssTextCache().set(this, text);
    m_hasCachedCSSText = true;
    return text;
}

void CSSPrimitiveValue::addSubresourceStyleURLs(ListHashSet<KURL>& urls, const CSSStyleSheet* styleSheet)
{
    if (m_primitiveUnitType == CSS_URI)
        addSubresourceURL(urls, styleSheet->completeURL(m_value.string));
}

} // namespace WebCore
