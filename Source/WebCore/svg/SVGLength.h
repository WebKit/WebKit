/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGLength_h
#define SVGLength_h

#if ENABLE(SVG)
#include "ExceptionCode.h"
#include "SVGParsingError.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

class CSSPrimitiveValue;

enum SVGLengthType {
    LengthTypeUnknown = 0,
    LengthTypeNumber = 1,
    LengthTypePercentage = 2,
    LengthTypeEMS = 3,
    LengthTypeEXS = 4,
    LengthTypePX = 5,
    LengthTypeCM = 6,
    LengthTypeMM = 7,
    LengthTypeIN = 8,
    LengthTypePT = 9,
    LengthTypePC = 10
};

enum SVGLengthMode {
    LengthModeWidth = 0,
    LengthModeHeight,
    LengthModeOther
};

enum SVGLengthNegativeValuesMode {
    AllowNegativeLengths,
    ForbidNegativeLengths
};

class QualifiedName;
class SVGElement;

class SVGLength {
public:
    // Forward declare these enums in the w3c naming scheme, for IDL generation
    enum {
        SVG_LENGTHTYPE_UNKNOWN = LengthTypeUnknown,
        SVG_LENGTHTYPE_NUMBER = LengthTypeNumber,
        SVG_LENGTHTYPE_PERCENTAGE = LengthTypePercentage,
        SVG_LENGTHTYPE_EMS = LengthTypeEMS,
        SVG_LENGTHTYPE_EXS = LengthTypeEXS,
        SVG_LENGTHTYPE_PX = LengthTypePX,
        SVG_LENGTHTYPE_CM = LengthTypeCM,
        SVG_LENGTHTYPE_MM = LengthTypeMM,
        SVG_LENGTHTYPE_IN = LengthTypeIN,
        SVG_LENGTHTYPE_PT = LengthTypePT,
        SVG_LENGTHTYPE_PC = LengthTypePC
    };

    SVGLength(SVGLengthMode mode = LengthModeOther, const String& valueAsString = String());
    SVGLength(const SVGElement*, float, SVGLengthMode mode = LengthModeOther, SVGLengthType type = LengthTypeNumber);
    SVGLength(const SVGLength&);

    SVGLengthType unitType() const;

    bool operator==(const SVGLength&) const;
    bool operator!=(const SVGLength&) const;

    static SVGLength construct(SVGLengthMode, const String&, SVGParsingError&, SVGLengthNegativeValuesMode = AllowNegativeLengths);

    float value(const SVGElement* context) const;
    float value(const SVGElement* context, ExceptionCode&) const;
    void setValue(float, const SVGElement* context, ExceptionCode&);
    void setValue(const SVGElement* context, float, SVGLengthMode, SVGLengthType, ExceptionCode&);

    float valueInSpecifiedUnits() const { return m_valueInSpecifiedUnits; }
    void setValueInSpecifiedUnits(float value) { m_valueInSpecifiedUnits = value; }

    float valueAsPercentage() const;

    String valueAsString() const;
    void setValueAsString(const String&, ExceptionCode&);
    void setValueAsString(const String&, SVGLengthMode, ExceptionCode&);
    
    void newValueSpecifiedUnits(unsigned short, float valueInSpecifiedUnits, ExceptionCode&);
    void convertToSpecifiedUnits(unsigned short, const SVGElement* context, ExceptionCode&);

    // Helper functions
    inline bool isRelative() const
    {
        SVGLengthType type = unitType();
        return type == LengthTypePercentage || type == LengthTypeEMS || type == LengthTypeEXS;
    }

    bool isZero() const 
    { 
        return !m_valueInSpecifiedUnits;
    }

    static SVGLength fromCSSPrimitiveValue(CSSPrimitiveValue*);
    static PassRefPtr<CSSPrimitiveValue> toCSSPrimitiveValue(const SVGLength&);
    static SVGLengthMode lengthModeForAnimatedLengthAttribute(const QualifiedName&);

    SVGLength blend(const SVGLength& from, float progress) const
    {
        SVGLengthType toType = unitType();
        SVGLengthType fromType = from.unitType();

        if ((from.isZero() && isZero())
            || fromType == LengthTypeUnknown
            || toType == LengthTypeUnknown
            || (!from.isZero() && fromType != LengthTypePercentage && toType == LengthTypePercentage)
            || (!isZero() && fromType == LengthTypePercentage && toType != LengthTypePercentage)
            || (!from.isZero() && !isZero() && (fromType == LengthTypeEMS || fromType == LengthTypeEXS) && fromType != toType))
            return *this;

        SVGLength length;
        ExceptionCode ec = 0;

        if (fromType == LengthTypePercentage || toType == LengthTypePercentage) {
            float fromPercent = from.valueAsPercentage() * 100;
            float toPercent = valueAsPercentage() * 100;
            length.newValueSpecifiedUnits(LengthTypePercentage, fromPercent + (toPercent - fromPercent) * progress, ec);
            if (ec)
                return SVGLength();
            return length;
        }

        if (fromType == toType || from.isZero() || isZero() || fromType == LengthTypeEMS || fromType == LengthTypeEXS) {
            float fromValue = from.valueInSpecifiedUnits();
            float toValue = valueInSpecifiedUnits();
            if (isZero())
                length.newValueSpecifiedUnits(fromType, fromValue + (toValue - fromValue) * progress, ec);
            else
                length.newValueSpecifiedUnits(toType, fromValue + (toValue - fromValue) * progress, ec);
            if (ec)
                return SVGLength();
            return length;
        }

        ASSERT(!isRelative());
        ASSERT(!from.isRelative());
        float fromValueInUserUnits = convertValueToUserUnits(from.valueInSpecifiedUnits(), fromType, 0, ec);
        if (ec)
            return SVGLength();

        float fromValue = convertValueFromUserUnits(fromValueInUserUnits, toType, 0, ec);
        if (ec)
            return SVGLength();

        float toValue = valueInSpecifiedUnits();
        length.newValueSpecifiedUnits(toType, fromValue + (toValue - fromValue) * progress, ec);

        if (ec)
            return SVGLength();
        return length;
    }

private:
    bool determineViewport(const SVGElement* context, float& width, float& height) const;

    float convertValueToUserUnits(float value, SVGLengthType fromUnit, const SVGElement* context, ExceptionCode&) const;
    float convertValueFromUserUnits(float value, SVGLengthType toUnit, const SVGElement* context, ExceptionCode&) const;

    float convertValueFromPercentageToUserUnits(float value, const SVGElement* context, ExceptionCode&) const;
    float convertValueFromUserUnitsToPercentage(float value, const SVGElement* context, ExceptionCode&) const;

    float convertValueFromUserUnitsToEMS(float value, const SVGElement* context, ExceptionCode&) const;
    float convertValueFromEMSToUserUnits(float value, const SVGElement* context, ExceptionCode&) const;

    float convertValueFromUserUnitsToEXS(float value, const SVGElement* context, ExceptionCode&) const;
    float convertValueFromEXSToUserUnits(float value, const SVGElement* context, ExceptionCode&) const;

private:
    float m_valueInSpecifiedUnits;
    unsigned int m_unit;
};

template<>
struct SVGPropertyTraits<SVGLength> {
    static SVGLength initialValue() { return SVGLength(); }
    static String toString(const SVGLength& type) { return type.valueAsString(); }
};


} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGLength_h
