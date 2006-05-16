/*
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

#ifndef CSSPrimitiveValue_H
#define CSSPrimitiveValue_H

#include "CSSValue.h"

namespace WebCore {

class Counter;
class DashboardRegion;
class Pair;
class RectImpl;
class RenderStyle;

typedef int ExceptionCode;

class CSSPrimitiveValue : public CSSValue
{
public:
    enum UnitTypes {
        CSS_UNKNOWN = 0,
        CSS_NUMBER = 1,
        CSS_PERCENTAGE = 2,
        CSS_EMS = 3,
        CSS_EXS = 4,
        CSS_PX = 5,
        CSS_CM = 6,
        CSS_MM = 7,
        CSS_IN = 8,
        CSS_PT = 9,
        CSS_PC = 10,
        CSS_DEG = 11,
        CSS_RAD = 12,
        CSS_GRAD = 13,
        CSS_MS = 14,
        CSS_S = 15,
        CSS_HZ = 16,
        CSS_KHZ = 17,
        CSS_DIMENSION = 18,
        CSS_STRING = 19,
        CSS_URI = 20,
        CSS_IDENT = 21,
        CSS_ATTR = 22,
        CSS_COUNTER = 23,
        CSS_RECT = 24,
        CSS_RGBCOLOR = 25,
        CSS_PAIR = 100, // We envision this being exposed as a means of getting computed style values for pairs (border-spacing/radius, background-position, etc.)
        CSS_DASHBOARD_REGION = 101 // FIXME: What on earth is this doing as a primitive value? It should not be!
    };

    // FIXME: int vs. unsigned overloading is too tricky for color vs. ident
    CSSPrimitiveValue();
    CSSPrimitiveValue(int ident);
    CSSPrimitiveValue(double, UnitTypes);
    CSSPrimitiveValue(const String&, UnitTypes);
    CSSPrimitiveValue(PassRefPtr<Counter>);
    CSSPrimitiveValue(PassRefPtr<RectImpl>);
    CSSPrimitiveValue(unsigned color); // RGB value
    CSSPrimitiveValue(PassRefPtr<Pair>);

#if __APPLE__
    CSSPrimitiveValue(PassRefPtr<DashboardRegion>); // FIXME: Why is dashboard region a primitive value? This makes no sense.
#endif

    virtual ~CSSPrimitiveValue();

    void cleanup();

    unsigned short primitiveType() const { return m_type; }

    /*
     * computes a length in pixels out of the given CSSValue. Need the RenderStyle to get
     * the fontinfo in case val is defined in em or ex.
     *
     * The metrics have to be a bit different for screen and printer output.
     * For screen output we assume 1 inch == 72 px, for printer we assume 300 dpi
     *
     * this is screen/printer dependent, so we probably need a config option for this,
     * and some tool to calibrate.
     */
    int computeLengthInt(RenderStyle*);
    int computeLengthInt(RenderStyle*, double multiplier);
    int computeLengthIntForLength(RenderStyle*);
    int computeLengthIntForLength(RenderStyle*, double multiplier);
    short computeLengthShort(RenderStyle*);
    short computeLengthShort(RenderStyle*, double multiplier);
    double computeLengthFloat(RenderStyle*, bool applyZoomFactor = true);

    // use with care!!!
    void setPrimitiveType(unsigned short type) { m_type = type; }
    void setFloatValue(unsigned short unitType, double floatValue, ExceptionCode&);
    double getFloatValue(unsigned short unitType);
    double getFloatValue() { return m_value.num; }

    void setStringValue(unsigned short stringType, const String& stringValue, ExceptionCode&);
    String getStringValue() const;
    
    Counter* getCounterValue () const {
        return m_type != CSS_COUNTER ? 0 : m_value.counter;
    }

    RectImpl* getRectValue () const {
        return m_type != CSS_RECT ? 0 : m_value.rect;
    }

    unsigned getRGBColorValue() const {
        return m_type != CSS_RGBCOLOR ? 0 : m_value.rgbcolor;
    }

    Pair* getPairValue() const {
        return m_type != CSS_PAIR ? 0 : m_value.pair;
    }

#if __APPLE__
    DashboardRegion *getDashboardRegionValue () const {
        return m_type != CSS_DASHBOARD_REGION ? 0 : m_value.region;
    }
#endif

    virtual bool isPrimitiveValue() const { return true; }
    virtual unsigned short cssValueType() const;

    int getIdent();

    virtual bool parseString(const String&, bool = false);
    virtual String cssText() const;

    virtual bool isQuirkValue() { return false; }

protected:
    int m_type;
    union {
        int ident;
        double num;
        StringImpl* string;
        Counter* counter;
        RectImpl* rect;
        unsigned rgbcolor;
        Pair* pair;
#if __APPLE__
        DashboardRegion* region;
#endif
    } m_value;
};

} // namespace

#endif
