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
#include "Color.h"
#include "Counter.h"
#include "ExceptionCode.h"
#include "Node.h"
#include "Pair.h"
#include "RGBColor.h"
#include "Rect.h"
#include "RenderStyle.h"
#include <wtf/ASCIICType.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

using namespace WTF;

namespace WebCore {

typedef HashMap<const CSSPrimitiveValue*, String> CSSTextCache;
static CSSTextCache& cssTextCache()
{
    DEFINE_STATIC_LOCAL(CSSTextCache, cache, ());
    return cache;
}

// A more stylish solution than sharing would be to turn CSSPrimitiveValue (or CSSValues in general) into non-virtual,
// non-refcounted simple type with value semantics. In practice these sharing tricks get similar memory benefits 
// with less need for refactoring.

inline PassRefPtr<CSSPrimitiveValue> CSSPrimitiveValue::createUncachedIdentifier(int identifier)
{
    return adoptRef(new CSSPrimitiveValue(identifier));
}

inline PassRefPtr<CSSPrimitiveValue> CSSPrimitiveValue::createUncachedColor(unsigned rgbValue)
{
    return adoptRef(new CSSPrimitiveValue(rgbValue));
}

inline PassRefPtr<CSSPrimitiveValue> CSSPrimitiveValue::createUncached(double value, UnitTypes type)
{
    return adoptRef(new CSSPrimitiveValue(value, type));
}

PassRefPtr<CSSPrimitiveValue> CSSPrimitiveValue::createIdentifier(int ident)
{
    static RefPtr<CSSPrimitiveValue>* identValueCache = new RefPtr<CSSPrimitiveValue>[numCSSValueKeywords];
    if (ident >= 0 && ident < numCSSValueKeywords) {
        RefPtr<CSSPrimitiveValue> primitiveValue = identValueCache[ident];
        if (!primitiveValue) {
            primitiveValue = createUncachedIdentifier(ident);
            identValueCache[ident] = primitiveValue;
        }
        return primitiveValue.release();
    } 
    return createUncachedIdentifier(ident);
}

PassRefPtr<CSSPrimitiveValue> CSSPrimitiveValue::createColor(unsigned rgbValue)
{
    typedef HashMap<unsigned, RefPtr<CSSPrimitiveValue> > ColorValueCache;
    static ColorValueCache* colorValueCache = new ColorValueCache;
    // These are the empty and deleted values of the hash table.
    if (rgbValue == Color::transparent) {
        static CSSPrimitiveValue* colorTransparent = createUncachedColor(Color::transparent).releaseRef();
        return colorTransparent;
    }
    if (rgbValue == Color::white) {
        static CSSPrimitiveValue* colorWhite = createUncachedColor(Color::white).releaseRef();
        return colorWhite;
    }
    RefPtr<CSSPrimitiveValue> primitiveValue = colorValueCache->get(rgbValue);
    if (primitiveValue)
        return primitiveValue.release();
    primitiveValue = createUncachedColor(rgbValue);
    // Just wipe out the cache and start rebuilding when it gets too big.
    const int maxColorCacheSize = 512;
    if (colorValueCache->size() >= maxColorCacheSize)
        colorValueCache->clear();
    colorValueCache->add(rgbValue, primitiveValue);
    
    return primitiveValue.release();
}

PassRefPtr<CSSPrimitiveValue> CSSPrimitiveValue::create(double value, UnitTypes type)
{
    // Small integers are very common. Try to share them.
    const int cachedIntegerCount = 128;
    // Other common primitive types have UnitTypes smaller than this.
    const int maxCachedUnitType = CSS_PX;
    typedef RefPtr<CSSPrimitiveValue>(* IntegerValueCache)[maxCachedUnitType + 1];
    static IntegerValueCache integerValueCache = new RefPtr<CSSPrimitiveValue>[cachedIntegerCount][maxCachedUnitType + 1];
    if (type <= maxCachedUnitType && value >= 0 && value < cachedIntegerCount) {
        int intValue = static_cast<int>(value);
        if (value == intValue) {
            RefPtr<CSSPrimitiveValue> primitiveValue = integerValueCache[intValue][type];
            if (!primitiveValue) {
                primitiveValue = createUncached(value, type);
                integerValueCache[intValue][type] = primitiveValue;
            }
            return primitiveValue.release();
        }
    }

    return createUncached(value, type);
}

PassRefPtr<CSSPrimitiveValue> CSSPrimitiveValue::create(const String& value, UnitTypes type)
{
    return adoptRef(new CSSPrimitiveValue(value, type));
}

static const AtomicString& valueOrPropertyName(int valueOrPropertyID)
{
    ASSERT_ARG(valueOrPropertyID, valueOrPropertyID >= 0);
    ASSERT_ARG(valueOrPropertyID, valueOrPropertyID < numCSSValueKeywords || (valueOrPropertyID >= firstCSSProperty && valueOrPropertyID < firstCSSProperty + numCSSProperties));

    if (valueOrPropertyID < 0)
        return nullAtom;

    if (valueOrPropertyID < numCSSValueKeywords) {
        static AtomicString* cssValueKeywordStrings[numCSSValueKeywords];
        if (!cssValueKeywordStrings[valueOrPropertyID])
            cssValueKeywordStrings[valueOrPropertyID] = new AtomicString(getValueName(valueOrPropertyID));
        return *cssValueKeywordStrings[valueOrPropertyID];
    }

    if (valueOrPropertyID >= firstCSSProperty && valueOrPropertyID < firstCSSProperty + numCSSProperties) {
        static AtomicString* cssPropertyStrings[numCSSProperties];
        int propertyIndex = valueOrPropertyID - firstCSSProperty;
        if (!cssPropertyStrings[propertyIndex])
            cssPropertyStrings[propertyIndex] = new AtomicString(getPropertyName(static_cast<CSSPropertyID>(valueOrPropertyID)));
        return *cssPropertyStrings[propertyIndex];
    }

    return nullAtom;
}

CSSPrimitiveValue::CSSPrimitiveValue()
    : m_type(0)
    , m_hasCachedCSSText(false)
{
}

CSSPrimitiveValue::CSSPrimitiveValue(int ident)
    : m_type(CSS_IDENT)
    , m_hasCachedCSSText(false)
{
    m_value.ident = ident;
}

CSSPrimitiveValue::CSSPrimitiveValue(double num, UnitTypes type)
    : m_type(type)
    , m_hasCachedCSSText(false)
{
    m_value.num = num;
}

CSSPrimitiveValue::CSSPrimitiveValue(const String& str, UnitTypes type)
    : m_type(type)
    , m_hasCachedCSSText(false)
{
    if ((m_value.string = str.impl()))
        m_value.string->ref();
}

CSSPrimitiveValue::CSSPrimitiveValue(RGBA32 color)
    : m_type(CSS_RGBCOLOR)
    , m_hasCachedCSSText(false)
{
    m_value.rgbcolor = color;
}

CSSPrimitiveValue::CSSPrimitiveValue(const Length& length)
    : m_hasCachedCSSText(false)
{
    switch (length.type()) {
        case Auto:
            m_type = CSS_IDENT;
            m_value.ident = CSSValueAuto;
            break;
        case WebCore::Fixed:
            m_type = CSS_PX;
            m_value.num = length.value();
            break;
        case Intrinsic:
            m_type = CSS_IDENT;
            m_value.ident = CSSValueIntrinsic;
            break;
        case MinIntrinsic:
            m_type = CSS_IDENT;
            m_value.ident = CSSValueMinIntrinsic;
            break;
        case Percent:
            m_type = CSS_PERCENTAGE;
            m_value.num = length.percent();
            break;
        case Relative:
        case Static:
            ASSERT_NOT_REACHED();
            break;
    }
}

void CSSPrimitiveValue::init(PassRefPtr<Counter> c)
{
    m_type = CSS_COUNTER;
    m_hasCachedCSSText = false;
    m_value.counter = c.releaseRef();
}

void CSSPrimitiveValue::init(PassRefPtr<Rect> r)
{
    m_type = CSS_RECT;
    m_hasCachedCSSText = false;
    m_value.rect = r.releaseRef();
}

#if ENABLE(DASHBOARD_SUPPORT)
void CSSPrimitiveValue::init(PassRefPtr<DashboardRegion> r)
{
    m_type = CSS_DASHBOARD_REGION;
    m_hasCachedCSSText = false;
    m_value.region = r.releaseRef();
}
#endif

void CSSPrimitiveValue::init(PassRefPtr<Pair> p)
{
    m_type = CSS_PAIR;
    m_hasCachedCSSText = false;
    m_value.pair = p.releaseRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    cleanup();
}

void CSSPrimitiveValue::cleanup()
{
    switch (m_type) {
        case CSS_STRING:
        case CSS_URI:
        case CSS_ATTR:
        case CSS_PARSER_VARIABLE_FUNCTION_SYNTAX:
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
        case CSS_PAIR:
            m_value.pair->deref();
            break;
#if ENABLE(DASHBOARD_SUPPORT)
        case CSS_DASHBOARD_REGION:
            if (m_value.region)
                m_value.region->deref();
            break;
#endif
        default:
            break;
    }

    m_type = 0;
    if (m_hasCachedCSSText) {
        cssTextCache().remove(this);
        m_hasCachedCSSText = false;
    }
}

int CSSPrimitiveValue::computeLengthInt(RenderStyle* style, RenderStyle* rootStyle)
{
    double result = computeLengthDouble(style, rootStyle);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > INT_MAX || result < INT_MIN)
        return 0;
    return static_cast<int>(result);
}

int CSSPrimitiveValue::computeLengthInt(RenderStyle* style, RenderStyle* rootStyle, double multiplier)
{
    double result = computeLengthDouble(style, rootStyle, multiplier);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > INT_MAX || result < INT_MIN)
        return 0;
    return static_cast<int>(result);
}

// Lengths expect an int that is only 28-bits, so we have to check for a different overflow.
int CSSPrimitiveValue::computeLengthIntForLength(RenderStyle* style, RenderStyle* rootStyle)
{
    double result = computeLengthDouble(style, rootStyle);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > intMaxForLength || result < intMinForLength)
        return 0;
    return static_cast<int>(result);
}

// Lengths expect an int that is only 28-bits, so we have to check for a different overflow.
int CSSPrimitiveValue::computeLengthIntForLength(RenderStyle* style, RenderStyle* rootStyle, double multiplier)
{
    double result = computeLengthDouble(style, rootStyle, multiplier);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > intMaxForLength || result < intMinForLength)
        return 0;
    return static_cast<int>(result);
}

short CSSPrimitiveValue::computeLengthShort(RenderStyle* style, RenderStyle* rootStyle)
{
    double result = computeLengthDouble(style, rootStyle);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > SHRT_MAX || result < SHRT_MIN)
        return 0;
    return static_cast<short>(result);
}

short CSSPrimitiveValue::computeLengthShort(RenderStyle* style, RenderStyle* rootStyle, double multiplier)
{
    double result = computeLengthDouble(style, rootStyle, multiplier);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > SHRT_MAX || result < SHRT_MIN)
        return 0;
    return static_cast<short>(result);
}

float CSSPrimitiveValue::computeLengthFloat(RenderStyle* style, RenderStyle* rootStyle, bool computingFontSize)
{
    return static_cast<float>(computeLengthDouble(style, rootStyle, 1.0, computingFontSize));
}

float CSSPrimitiveValue::computeLengthFloat(RenderStyle* style, RenderStyle* rootStyle, double multiplier, bool computingFontSize)
{
    return static_cast<float>(computeLengthDouble(style, rootStyle, multiplier, computingFontSize));
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
            factor = style->font().xHeight();
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

void CSSPrimitiveValue::setFloatValue(unsigned short unitType, double floatValue, ExceptionCode& ec)
{
    ec = 0;

    if (m_type < CSS_NUMBER || m_type > CSS_DIMENSION || unitType < CSS_NUMBER || unitType > CSS_DIMENSION) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    cleanup();

    m_value.num = floatValue;
    m_type = unitType;
}

static double scaleFactorForConversion(unsigned short unitType)
{
    double factor = 1.0;
    switch (unitType) {
        case CSSPrimitiveValue::CSS_PX:
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
        default:
            break;
    }

    return factor;
}

double CSSPrimitiveValue::getDoubleValue(unsigned short unitType, ExceptionCode& ec)
{
    ec = 0;
    if (m_type < CSS_NUMBER || m_type > CSS_DIMENSION || unitType < CSS_NUMBER || unitType > CSS_DIMENSION) {
        ec = INVALID_ACCESS_ERR;
        return 0.0;
    }

    if (unitType == m_type || unitType < CSS_PX || unitType > CSS_PC)
        return m_value.num;

    double convertedValue = m_value.num;

    // First convert the value from m_type into CSSPixels
    double factor = scaleFactorForConversion(m_type);
    convertedValue *= factor;

    // Now convert from CSSPixels to the specified unitType
    factor = scaleFactorForConversion(unitType);
    convertedValue /= factor;

    return convertedValue;
}

double CSSPrimitiveValue::getDoubleValue(unsigned short unitType)
{
    if (m_type < CSS_NUMBER || m_type > CSS_DIMENSION || unitType < CSS_NUMBER || unitType > CSS_DIMENSION)
        return 0;

    if (unitType == m_type || unitType < CSS_PX || unitType > CSS_PC)
        return m_value.num;

    double convertedValue = m_value.num;

    // First convert the value from m_type into CSSPixels
    double factor = scaleFactorForConversion(m_type);
    convertedValue *= factor;

    // Now convert from CSSPixels to the specified unitType
    factor = scaleFactorForConversion(unitType);
    convertedValue /= factor;

    return convertedValue;
}


void CSSPrimitiveValue::setStringValue(unsigned short stringType, const String& stringValue, ExceptionCode& ec)
{
    ec = 0;

    if (m_type < CSS_STRING || m_type > CSS_ATTR || stringType < CSS_STRING || stringType > CSS_ATTR) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    cleanup();

    if (stringType != CSS_IDENT) {
        m_value.string = stringValue.impl();
        m_value.string->ref();
        m_type = stringType;
    }
    // FIXME: parse ident
}

String CSSPrimitiveValue::getStringValue(ExceptionCode& ec) const
{
    ec = 0;
    switch (m_type) {
        case CSS_STRING:
        case CSS_ATTR:
        case CSS_URI:
        case CSS_PARSER_VARIABLE_FUNCTION_SYNTAX:
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
    switch (m_type) {
        case CSS_STRING:
        case CSS_ATTR:
        case CSS_URI:
        case CSS_PARSER_VARIABLE_FUNCTION_SYNTAX:
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
    if (m_type != CSS_COUNTER) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return m_value.counter;
}

Rect* CSSPrimitiveValue::getRectValue(ExceptionCode& ec) const
{
    ec = 0;
    if (m_type != CSS_RECT) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return m_value.rect;
}

PassRefPtr<RGBColor> CSSPrimitiveValue::getRGBColorValue(ExceptionCode& ec) const
{
    ec = 0;
    if (m_type != CSS_RGBCOLOR) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    // FIMXE: This should not return a new object for each invocation.
    return RGBColor::create(m_value.rgbcolor);
}

Pair* CSSPrimitiveValue::getPairValue(ExceptionCode& ec) const
{
    ec = 0;
    if (m_type != CSS_PAIR) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return m_value.pair;
}

unsigned short CSSPrimitiveValue::cssValueType() const
{
    return CSS_PRIMITIVE_VALUE;
}

bool CSSPrimitiveValue::parseString(const String& /*string*/, bool /*strict*/)
{
    // FIXME
    return false;
}

int CSSPrimitiveValue::getIdent()
{
    if (m_type != CSS_IDENT)
        return 0;
    return m_value.ident;
}

String CSSPrimitiveValue::cssText() const
{
    // FIXME: return the original value instead of a generated one (e.g. color
    // name if it was specified) - check what spec says about this

    if (m_hasCachedCSSText) {
        ASSERT(cssTextCache().contains(this));
        return cssTextCache().get(this);
    }

    String text;
    switch (m_type) {
        case CSS_UNKNOWN:
            // FIXME
            break;
        case CSS_NUMBER:
        case CSS_PARSER_INTEGER:
            text = String::number(m_value.num);
            break;
        case CSS_PERCENTAGE:
            text = String::format("%.6lg%%", m_value.num);
            break;
        case CSS_EMS:
            text = String::format("%.6lgem", m_value.num);
            break;
        case CSS_EXS:
            text = String::format("%.6lgex", m_value.num);
            break;
        case CSS_REMS:
            text = String::format("%.6lgrem", m_value.num);
            break;
        case CSS_PX:
            text = String::format("%.6lgpx", m_value.num);
            break;
        case CSS_CM:
            text = String::format("%.6lgcm", m_value.num);
            break;
        case CSS_MM:
            text = String::format("%.6lgmm", m_value.num);
            break;
        case CSS_IN:
            text = String::format("%.6lgin", m_value.num);
            break;
        case CSS_PT:
            text = String::format("%.6lgpt", m_value.num);
            break;
        case CSS_PC:
            text = String::format("%.6lgpc", m_value.num);
            break;
        case CSS_DEG:
            text = String::format("%.6lgdeg", m_value.num);
            break;
        case CSS_RAD:
            text = String::format("%.6lgrad", m_value.num);
            break;
        case CSS_GRAD:
            text = String::format("%.6lggrad", m_value.num);
            break;
        case CSS_MS:
            text = String::format("%.6lgms", m_value.num);
            break;
        case CSS_S:
            text = String::format("%.6lgs", m_value.num);
            break;
        case CSS_HZ:
            text = String::format("%.6lghz", m_value.num);
            break;
        case CSS_KHZ:
            text = String::format("%.6lgkhz", m_value.num);
            break;
        case CSS_TURN:
            text = String::format("%.6lgturn", m_value.num);
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

            Vector<UChar> result;
            result.reserveInitialCapacity(6 + m_value.string->length());

            append(result, attrParen);
            append(result, m_value.string);
            result.uncheckedAppend(')');

            text = String::adopt(result);
            break;
        }
        case CSS_COUNTER:
            text = "counter(";
            text += String::number(m_value.num);
            text += ")";
            // FIXME: Add list-style and separator
            break;
        case CSS_RECT: {
            DEFINE_STATIC_LOCAL(const String, rectParen, ("rect("));

            Rect* rectVal = getRectValue();
            Vector<UChar> result;
            result.reserveInitialCapacity(32);
            append(result, rectParen);

            append(result, rectVal->top()->cssText());
            result.append(' ');

            append(result, rectVal->right()->cssText());
            result.append(' ');

            append(result, rectVal->bottom()->cssText());
            result.append(' ');

            append(result, rectVal->left()->cssText());
            result.append(')');

            text = String::adopt(result);
            break;
        }
        case CSS_RGBCOLOR:
        case CSS_PARSER_HEXCOLOR: {
            DEFINE_STATIC_LOCAL(const String, commaSpace, (", "));
            DEFINE_STATIC_LOCAL(const String, rgbParen, ("rgb("));
            DEFINE_STATIC_LOCAL(const String, rgbaParen, ("rgba("));

            RGBA32 rgbColor = m_value.rgbcolor;
            if (m_type == CSS_PARSER_HEXCOLOR)
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
            text += " ";
            text += m_value.pair->second()->cssText();
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
                if (region->top()->m_type == CSS_IDENT && region->top()->getIdent() == CSSValueInvalid) {
                    ASSERT(region->right()->m_type == CSS_IDENT);
                    ASSERT(region->bottom()->m_type == CSS_IDENT);
                    ASSERT(region->left()->m_type == CSS_IDENT);
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
        case CSS_PARSER_VARIABLE_FUNCTION_SYNTAX:
            text = "-webkit-var(";
            text += m_value.string;
            text += ")";
            break;
        case CSS_PARSER_OPERATOR: {
            char c = static_cast<char>(m_value.ident);
            text = String(&c, 1U);
            break;
        }
        case CSS_PARSER_IDENTIFIER:
            text = quoteCSSStringIfNeeded(m_value.string);
            break;
    }

    ASSERT(!cssTextCache().contains(this));
    cssTextCache().set(this, text);
    m_hasCachedCSSText = true;
    return text;
}

CSSParserValue CSSPrimitiveValue::parserValue() const
{
    // We only have to handle a subset of types.
    CSSParserValue value;
    value.id = 0;
    value.isInt = false;
    value.unit = CSSPrimitiveValue::CSS_IDENT;
    switch (m_type) {
        case CSS_NUMBER:
        case CSS_PERCENTAGE:
        case CSS_EMS:
        case CSS_EXS:
        case CSS_REMS:
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
        case CSS_DIMENSION:
        case CSS_TURN:
            value.fValue = m_value.num;
            value.unit = m_type;
            break;
        case CSS_STRING:
        case CSS_URI:
        case CSS_PARSER_VARIABLE_FUNCTION_SYNTAX:
        case CSS_PARSER_HEXCOLOR:
            value.string.characters = const_cast<UChar*>(m_value.string->characters());
            value.string.length = m_value.string->length();
            value.unit = m_type;
            break;
        case CSS_IDENT: {
            value.id = m_value.ident;
            const AtomicString& name = valueOrPropertyName(m_value.ident);
            value.string.characters = const_cast<UChar*>(name.characters());
            value.string.length = name.length();
            break;
        }
        case CSS_PARSER_OPERATOR:
            value.iValue = m_value.ident;
            value.unit = CSSParserValue::Operator;
            break;
        case CSS_PARSER_INTEGER:
            value.fValue = m_value.num;
            value.unit = CSSPrimitiveValue::CSS_NUMBER;
            value.isInt = true;
            break;
        case CSS_PARSER_IDENTIFIER:
            value.string.characters = const_cast<UChar*>(m_value.string->characters());
            value.string.length = m_value.string->length();
            value.unit = CSSPrimitiveValue::CSS_IDENT;
            break;
        case CSS_UNKNOWN:
        case CSS_ATTR:
        case CSS_COUNTER:
        case CSS_RECT:
        case CSS_RGBCOLOR:
        case CSS_PAIR:
#if ENABLE(DASHBOARD_SUPPORT)
        case CSS_DASHBOARD_REGION:
#endif
            ASSERT_NOT_REACHED();
            break;
    }
    
    return value;
}

void CSSPrimitiveValue::addSubresourceStyleURLs(ListHashSet<KURL>& urls, const CSSStyleSheet* styleSheet)
{
    if (m_type == CSS_URI)
        addSubresourceURL(urls, styleSheet->completeURL(m_value.string));
}

} // namespace WebCore
