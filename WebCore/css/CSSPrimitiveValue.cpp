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
#include "CSSValueKeywords.h"
#include "Color.h"
#include "Counter.h"
#include "DashboardRegion.h"
#include "ExceptionCode.h"
#include "Pair.h"
#include "RenderStyle.h"
#include <wtf/ASCIICType.h>

using namespace WTF;

namespace WebCore {

// "ident" from the CSS tokenizer, minus backslash-escape sequences
static bool isCSSTokenizerIdentifier(const String& string)
{
    const UChar* p = string.characters();
    const UChar* end = p + string.length();

    // -?
    if (p != end && p[0] == '-')
        ++p;

    // {nmstart}
    if (p == end || !(p[0] == '_' || p[0] >= 128 || isASCIIAlpha(p[0])))
        return false;
    ++p;

    // {nmchar}*
    for (; p != end; ++p) {
        if (!(p[0] == '_' || p[0] == '-' || p[0] >= 128 || isASCIIAlphanumeric(p[0])))
            return false;
    }

    return true;
}

// "url" from the CSS tokenizer, minus backslash-escape sequences
static bool isCSSTokenizerURL(const String& string)
{
    const UChar* p = string.characters();
    const UChar* end = p + string.length();

    for (; p != end; ++p) {
        UChar c = p[0];
        switch (c) {
            case '!':
            case '#':
            case '$':
            case '%':
            case '&':
                break;
            default:
                if (c < '*')
                    return false;
                if (c <= '~')
                    break;
                if (c < 128)
                    return false;
        }
    }

    return true;
}

// We use single quotes for now because markup.cpp uses double quotes.
static String quoteString(const String& string)
{
    // FIXME: Also need to escape characters like '\n'.
    String s = string;
    s.replace('\\', "\\\\");
    s.replace('\'', "\\'");
    return "'" + s + "'";
}

static String quoteStringIfNeeded(const String& string)
{
    return isCSSTokenizerIdentifier(string) ? string : quoteString(string);
}

static String quoteURLIfNeeded(const String& string)
{
    return isCSSTokenizerURL(string) ? string : quoteString(string);
}

CSSPrimitiveValue::CSSPrimitiveValue()
    : m_type(0)
{
}

CSSPrimitiveValue::CSSPrimitiveValue(int ident)
    : m_type(CSS_IDENT)
{
    m_value.ident = ident;
}

CSSPrimitiveValue::CSSPrimitiveValue(double num, UnitTypes type)
    : m_type(type)
{
    m_value.num = num;
}

CSSPrimitiveValue::CSSPrimitiveValue(const String& str, UnitTypes type)
    : m_type(type)
{
    if ((m_value.string = str.impl()))
        m_value.string->ref();
}

CSSPrimitiveValue::CSSPrimitiveValue(RGBA32 color)
    : m_type(CSS_RGBCOLOR)
{
    m_value.rgbcolor = color;
}

CSSPrimitiveValue::CSSPrimitiveValue(const Length& length)
{
    switch (length.type()) {
        case Auto:
            m_type = CSS_IDENT;
            m_value.ident = CSS_VAL_AUTO;
            break;
        case WebCore::Fixed:
            m_type = CSS_PX;
            m_value.num = length.value();
            break;
        case Intrinsic:
            m_type = CSS_IDENT;
            m_value.ident = CSS_VAL_INTRINSIC;
            break;
        case MinIntrinsic:
            m_type = CSS_IDENT;
            m_value.ident = CSS_VAL_MIN_INTRINSIC;
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
    m_value.counter = c.releaseRef();
}

void CSSPrimitiveValue::init(PassRefPtr<Rect> r)
{
    m_type = CSS_RECT;
    m_value.rect = r.releaseRef();
}

void CSSPrimitiveValue::init(PassRefPtr<DashboardRegion> r)
{
    m_type = CSS_DASHBOARD_REGION;
    m_value.region = r.releaseRef();
}

void CSSPrimitiveValue::init(PassRefPtr<Pair> p)
{
    m_type = CSS_PAIR;
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
        case CSS_DASHBOARD_REGION:
            if (m_value.region)
                m_value.region->deref();
            break;
        default:
            break;
    }

    m_type = 0;
}

int CSSPrimitiveValue::computeLengthInt(RenderStyle* style)
{
    double result = computeLengthDouble(style);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > INT_MAX || result < INT_MIN)
        return 0;
    return static_cast<int>(result);
}

int CSSPrimitiveValue::computeLengthInt(RenderStyle* style, double multiplier)
{
    double result = multiplier * computeLengthDouble(style);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > INT_MAX || result < INT_MIN)
        return 0;
    return static_cast<int>(result);
}

const int intMaxForLength = 0x7ffffff; // max value for a 28-bit int
const int intMinForLength = (-0x7ffffff - 1); // min value for a 28-bit int

// Lengths expect an int that is only 28-bits, so we have to check for a different overflow.
int CSSPrimitiveValue::computeLengthIntForLength(RenderStyle* style)
{
    double result = computeLengthDouble(style);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > intMaxForLength || result < intMinForLength)
        return 0;
    return static_cast<int>(result);
}

// Lengths expect an int that is only 28-bits, so we have to check for a different overflow.
int CSSPrimitiveValue::computeLengthIntForLength(RenderStyle* style, double multiplier)
{
    double result = multiplier * computeLengthDouble(style);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > intMaxForLength || result < intMinForLength)
        return 0;
    return static_cast<int>(result);
}

short CSSPrimitiveValue::computeLengthShort(RenderStyle* style)
{
    double result = computeLengthDouble(style);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > SHRT_MAX || result < SHRT_MIN)
        return 0;
    return static_cast<short>(result);
}

short CSSPrimitiveValue::computeLengthShort(RenderStyle* style, double multiplier)
{
    double result = multiplier * computeLengthDouble(style);

    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;

    if (result > SHRT_MAX || result < SHRT_MIN)
        return 0;
    return static_cast<short>(result);
}

float CSSPrimitiveValue::computeLengthFloat(RenderStyle* style, bool applyZoomFactor)
{
    return static_cast<float>(computeLengthDouble(style, applyZoomFactor));
}

double CSSPrimitiveValue::computeLengthDouble(RenderStyle* style, bool applyZoomFactor)
{
    unsigned short type = primitiveType();

    double factor = 1.0;
    switch (type) {
        case CSS_EMS:
            factor = applyZoomFactor ? style->fontDescription().computedSize() : style->fontDescription().specifiedSize();
            break;
        case CSS_EXS:
            // FIXME: We have a bug right now where the zoom will be applied multiple times to EX units.
            // We really need to compute EX using fontMetrics for the original specifiedSize and not use
            // our actual constructed rendering font.
            factor = style->font().xHeight();
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

    return getDoubleValue() * factor;
}

void CSSPrimitiveValue::setFloatValue(unsigned short unitType, double floatValue, ExceptionCode& ec)
{
    ec = 0;

    // FIXME: check if property supports this type
    if (m_type > CSS_DIMENSION) {
        ec = SYNTAX_ERR;
        return;
    }

    cleanup();

    //if(m_type > CSS_DIMENSION) throw DOMException(INVALID_ACCESS_ERR);
    m_value.num = floatValue;
    m_type = unitType;
}

double scaleFactorForConversion(unsigned short unitType)
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

    //if(m_type < CSS_STRING) throw DOMException(INVALID_ACCESS_ERR);
    //if(m_type > CSS_ATTR) throw DOMException(INVALID_ACCESS_ERR);
    if (m_type < CSS_STRING || m_type > CSS_ATTR) {
        ec = SYNTAX_ERR;
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
            return m_value.string;
        case CSS_IDENT:
            return getValueName(m_value.ident);
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
            return m_value.string;
        case CSS_IDENT:
            return getValueName(m_value.ident);
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

unsigned CSSPrimitiveValue::getRGBColorValue(ExceptionCode& ec) const
{
    ec = 0;
    if (m_type != CSS_RGBCOLOR) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    return m_value.rgbcolor;
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
    String text;
    switch (m_type) {
        case CSS_UNKNOWN:
            // FIXME
            break;
        case CSS_NUMBER:
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
        case CSS_DIMENSION:
            // FIXME
            break;
        case CSS_STRING:
            text = quoteStringIfNeeded(m_value.string);
            break;
        case CSS_URI:
            text = "url(" + quoteURLIfNeeded(m_value.string) + ")";
            break;
        case CSS_IDENT:
            text = getValueName(m_value.ident);
            break;
        case CSS_ATTR:
            // FIXME
            break;
        case CSS_COUNTER:
            text = "counter(";
            text += String::number(m_value.num);
            text += ")";
            // FIXME: Add list-style and separator
            break;
        case CSS_RECT: {
            Rect* rectVal = getRectValue();
            text = "rect(";
            text += rectVal->top()->cssText() + " ";
            text += rectVal->right()->cssText() + " ";
            text += rectVal->bottom()->cssText() + " ";
            text += rectVal->left()->cssText() + ")";
            break;
        }
        case CSS_RGBCOLOR: {
            Color color(m_value.rgbcolor);
            text = (color.alpha() < 0xFF) ? "rgba(" : "rgb(";
            text += String::number(color.red()) + ", ";
            text += String::number(color.green()) + ", ";
            text += String::number(color.blue());
            if (color.alpha() < 0xFF)
                text += ", " + String::number(static_cast<float>(color.alpha()) / 0xFF);
            text += ")";
            break;
        }
        case CSS_PAIR:
            text = m_value.pair->first()->cssText();
            text += " ";
            text += m_value.pair->second()->cssText();
            break;
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
                if (region->top()->m_type == CSS_IDENT && region->top()->getIdent() == CSS_VAL_INVALID) {
                    ASSERT(region->right()->m_type == CSS_IDENT);
                    ASSERT(region->bottom()->m_type == CSS_IDENT);
                    ASSERT(region->left()->m_type == CSS_IDENT);
                    ASSERT(region->right()->getIdent() == CSS_VAL_INVALID);
                    ASSERT(region->bottom()->getIdent() == CSS_VAL_INVALID);
                    ASSERT(region->left()->getIdent() == CSS_VAL_INVALID);
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
    }
    return text;
}

} // namespace WebCore
