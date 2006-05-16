/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include "CSSPrimitiveValue.h"

#include "Color.h"
#include "Counter.h"
#include "CSSValueKeywords.h"
#include "DashboardRegion.h"
#include "ExceptionCode.h"
#include "Pair.h"
#include "render_style.h"

namespace WebCore {

// Quotes the string if it needs quoting.
// We use single quotes for now beause markup.cpp uses double quotes.
static String quoteStringIfNeeded(const String &string)
{
    // For now, just do this for strings that start with "#" to fix Korean font names that start with "#".
    // Post-Tiger, we should isLegalIdentifier instead after working out all the ancillary issues.
    if (string[0] != '#')
        return string;

    // FIXME: Also need to transform control characters into \ sequences.
    String s = string;
    s.replace('\\', "\\\\");
    s.replace('\'', "\\'");
    return "'" + s + "'";
}

CSSPrimitiveValue::CSSPrimitiveValue()
{
    m_type = 0;
}

CSSPrimitiveValue::CSSPrimitiveValue(int ident)
{
    m_value.ident = ident;
    m_type = CSS_IDENT;
}

CSSPrimitiveValue::CSSPrimitiveValue(double num, UnitTypes type)
{
    m_value.num = num;
    m_type = type;
}

CSSPrimitiveValue::CSSPrimitiveValue(const String& str, UnitTypes type)
{
    if ((m_value.string = str.impl()))
        m_value.string->ref();
    m_type = type;
}

CSSPrimitiveValue::CSSPrimitiveValue(PassRefPtr<Counter> c)
{
    m_value.counter = c.release();
    m_type = CSS_COUNTER;
}

CSSPrimitiveValue::CSSPrimitiveValue(PassRefPtr<RectImpl> r)
{
    m_value.rect = r.release();
    m_type = CSS_RECT;
}

#if __APPLE__
CSSPrimitiveValue::CSSPrimitiveValue(PassRefPtr<DashboardRegion> r)
{
    m_value.region = r.release();
    m_type = CSS_DASHBOARD_REGION;
}
#endif

CSSPrimitiveValue::CSSPrimitiveValue(RGBA32 color)
{
    m_value.rgbcolor = color;
    m_type = CSS_RGBCOLOR;
}

CSSPrimitiveValue::CSSPrimitiveValue(PassRefPtr<Pair> p)
{
    m_value.pair = p.release();
    m_type = CSS_PAIR;
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    cleanup();
}

void CSSPrimitiveValue::cleanup()
{
    switch(m_type) {
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
#if __APPLE__
    case CSS_DASHBOARD_REGION:
        if (m_value.region)
            m_value.region->deref();
        break;
#endif
    default:
        break;
    }

    m_type = 0;
}

int CSSPrimitiveValue::computeLengthInt(RenderStyle *style)
{
    double result = computeLengthFloat(style);
    
    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;
    
    if (result > INT_MAX || result < INT_MIN)
        return 0;
    return (int)result;    
}

int CSSPrimitiveValue::computeLengthInt(RenderStyle *style, double multiplier)
{
    double result = multiplier * computeLengthFloat(style);
    
    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;
    
    if (result > INT_MAX || result < INT_MIN)
        return 0;
    return (int)result;  
}

const int intMaxForLength = 0x7ffffff; // max value for a 28-bit int
const int intMinForLength = (-0x7ffffff-1); // min value for a 28-bit int

// Lengths expect an int that is only 28-bits, so we have to check for a different overflow.
int CSSPrimitiveValue::computeLengthIntForLength(RenderStyle *style)
{
    double result = computeLengthFloat(style);
    
    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;
    
    if (result > intMaxForLength || result < intMinForLength)
        return 0;
    return (int)result;    
}

// Lengths expect an int that is only 28-bits, so we have to check for a different overflow.
int CSSPrimitiveValue::computeLengthIntForLength(RenderStyle *style, double multiplier)
{
    double result = multiplier * computeLengthFloat(style);
    
    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;
    
    if (result > intMaxForLength || result < intMinForLength)
        return 0;
    return (int)result;  
}

short CSSPrimitiveValue::computeLengthShort(RenderStyle *style)
{
    double result = computeLengthFloat(style);
    
    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;
    
    if (result > SHRT_MAX || result < SHRT_MIN)
        return 0;
    return (short)result;    
}

short CSSPrimitiveValue::computeLengthShort(RenderStyle *style, double multiplier)
{
    double result = multiplier * computeLengthFloat(style);
    
    // This conversion is imprecise, often resulting in values of, e.g., 44.99998.  We
    // need to go ahead and round if we're really close to the next integer value.
    result += result < 0 ? -0.01 : +0.01;
    
    if (result > SHRT_MAX || result < SHRT_MIN)
        return 0;
    return (short)result;  
}

double CSSPrimitiveValue::computeLengthFloat(RenderStyle *style, bool applyZoomFactor)
{
    unsigned short type = primitiveType();

    // We always assume 96 CSS pixels in a CSS inch. This is the cold hard truth of the Web.
    // At high DPI, we may scale a CSS pixel, but the ratio of the CSS pixel to the so-called
    // "absolute" CSS length units like inch and pt is always fixed and never changes.
    double cssPixelsPerInch = 96.;

    double factor = 1.;
    switch(type) {
    case CSS_EMS:
        factor = applyZoomFactor ?
          style->fontDescription().computedSize() :
          style->fontDescription().specifiedSize();
        break;
    case CSS_EXS: {
        // FIXME: We have a bug right now where the zoom will be applied multiple times to EX units.
        // We really need to compute EX using fontMetrics for the original specifiedSize and not use
        // our actual constructed rendering font.
        factor = style->font().xHeight();
        break;
    }
    case CSS_PX:
        break;
    case CSS_CM:
        factor = cssPixelsPerInch/2.54; // (2.54 cm/in)
        break;
    case CSS_MM:
        factor = cssPixelsPerInch/25.4;
        break;
    case CSS_IN:
        factor = cssPixelsPerInch;
        break;
    case CSS_PT:
        factor = cssPixelsPerInch/72.;
        break;
    case CSS_PC:
        // 1 pc == 12 pt
        factor = cssPixelsPerInch*12./72.;
        break;
    default:
        return -1;
    }

    return getFloatValue() * factor;
}

void CSSPrimitiveValue::setFloatValue( unsigned short unitType, double floatValue, ExceptionCode& ec)
{
    ec = 0;
    cleanup();
    // ### check if property supports this type
    if (m_type > CSS_DIMENSION) {
        ec = SYNTAX_ERR;
        return;
    }
    //if(m_type > CSS_DIMENSION) throw DOMException(INVALID_ACCESS_ERR);
    m_value.num = floatValue;
    m_type = unitType;
}

double scaleFactorForConversion(unsigned short unitType)
{
    double cssPixelsPerInch = 96.0;
    double factor = 1.0;
    
    switch(unitType) {
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

double CSSPrimitiveValue::getFloatValue(unsigned short unitType)
{
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

void CSSPrimitiveValue::setStringValue( unsigned short stringType, const String &stringValue, ExceptionCode& ec)
{
    ec = 0;
    cleanup();
    //if(m_type < CSS_STRING) throw DOMException(INVALID_ACCESS_ERR);
    //if(m_type > CSS_ATTR) throw DOMException(INVALID_ACCESS_ERR);
    if (m_type < CSS_STRING || m_type > CSS_ATTR) {
        ec = SYNTAX_ERR;
        return;
    }
    if (stringType != CSS_IDENT) {
        m_value.string = stringValue.impl();
        m_value.string->ref();
        m_type = stringType;
    }
    // ### parse ident
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
            // FIXME: The CSS 2.1 spec says you should throw an exception here.
            break;
    }
    
    return String();
}

unsigned short CSSPrimitiveValue::cssValueType() const
{
    return CSS_PRIMITIVE_VALUE;
}

bool CSSPrimitiveValue::parseString( const String &/*string*/, bool )
{
    // ###
    return false;
}

int CSSPrimitiveValue::getIdent()
{
    if(m_type != CSS_IDENT) return 0;
    return m_value.ident;
}

String CSSPrimitiveValue::cssText() const
{
    // ### return the original value instead of a generated one (e.g. color
    // name if it was specified) - check what spec says about this
    String text;
    switch ( m_type ) {
        case CSS_UNKNOWN:
            // ###
            break;
        case CSS_NUMBER:
            text = String::number(m_value.num);
            break;
        case CSS_PERCENTAGE:
            text = String::number(m_value.num) + "%";
            break;
        case CSS_EMS:
            text = String::number(m_value.num) + "em";
            break;
        case CSS_EXS:
            text = String::number(m_value.num) + "ex";
            break;
        case CSS_PX:
            text = String::number(m_value.num) + "px";
            break;
        case CSS_CM:
            text = String::number(m_value.num) + "cm";
            break;
        case CSS_MM:
            text = String::number(m_value.num) + "mm";
            break;
        case CSS_IN:
            text = String::number(m_value.num) + "in";
            break;
        case CSS_PT:
            text = String::number(m_value.num) + "pt";
            break;
        case CSS_PC:
            text = String::number(m_value.num) + "pc";
            break;
        case CSS_DEG:
            text = String::number(m_value.num) + "deg";
            break;
        case CSS_RAD:
            text = String::number(m_value.num) + "rad";
            break;
        case CSS_GRAD:
            text = String::number(m_value.num) + "grad";
            break;
        case CSS_MS:
            text = String::number(m_value.num) + "ms";
            break;
        case CSS_S:
            text = String::number(m_value.num) + "s";
            break;
        case CSS_HZ:
            text = String::number(m_value.num) + "hz";
            break;
        case CSS_KHZ:
            text = String::number(m_value.num) + "khz";
            break;
        case CSS_DIMENSION:
            // ###
            break;
        case CSS_STRING:
            text = quoteStringIfNeeded(m_value.string);
            break;
        case CSS_URI:
            text = "url(" + String(m_value.string) + ")";
            break;
        case CSS_IDENT:
            text = getValueName(m_value.ident);
            break;
        case CSS_ATTR:
            // ###
            break;
        case CSS_COUNTER:
            // ###
            break;
        case CSS_RECT: {
            RectImpl* rectVal = getRectValue();
            text = "rect(";
            text += rectVal->top()->cssText() + " ";
            text += rectVal->right()->cssText() + " ";
            text += rectVal->bottom()->cssText() + " ";
            text += rectVal->left()->cssText() + ")";
            break;
        }
        case CSS_RGBCOLOR: {
            Color color(m_value.rgbcolor);
            if (color.alpha() < 0xFF)
                text = "rgba(";
            else
                text = "rgb(";
            text += String::number(color.red()) + ", ";
            text += String::number(color.green()) + ", ";
            text += String::number(color.blue());
            if (color.alpha() < 0xFF)
                text += ", " + String::number((float)color.alpha() / 0xFF);
            text += ")";
            break;
        }
        case CSS_PAIR:
            text = m_value.pair->first()->cssText();
            text += " ";
            text += m_value.pair->second()->cssText();
            break;
#if __APPLE__
        case CSS_DASHBOARD_REGION:
            for (DashboardRegion* region = getDashboardRegionValue(); region; region = region->m_next.get()) {
                text = "dashboard-region(";
                text += region->m_label;
                if (region->m_isCircle)
                    text += " circle ";
                else if (region->m_isRectangle)
                    text += " rectangle ";
                else
                    break;
                text += region->top()->cssText() + " ";
                text += region->right()->cssText() + " ";
                text += region->bottom()->cssText() + " ";
                text += region->left()->cssText();
                text += ")";
            }
            break;
#endif
    }
    return text;
}

}
