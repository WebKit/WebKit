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

#pragma once

#include "AnimationUtilities.h"
#include "SVGLengthContext.h"
#include "SVGParsingError.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

class CSSPrimitiveValue;
class QualifiedName;

enum SVGLengthNegativeValuesMode {
    AllowNegativeLengths,
    ForbidNegativeLengths
};

class SVGLength {
    WTF_MAKE_FAST_ALLOCATED;
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

    // FIXME: Once all SVGLength users use Length internally, we make this a wrapper for Length.
    SVGLength(SVGLengthMode = LengthModeOther, const String& valueAsString = String());
    SVGLength(const SVGLengthContext&, float, SVGLengthMode = LengthModeOther, SVGLengthType = LengthTypeNumber);

    SVGLengthType unitType() const;
    SVGLengthMode unitMode() const;

    bool operator==(const SVGLength&) const;
    bool operator!=(const SVGLength&) const;

    static SVGLength construct(SVGLengthMode, const String&, SVGParsingError&, SVGLengthNegativeValuesMode = AllowNegativeLengths);

    float value(const SVGLengthContext&) const;
    ExceptionOr<float> valueForBindings(const SVGLengthContext&) const;
    ExceptionOr<void> setValue(float, const SVGLengthContext&);
    ExceptionOr<void> setValue(const SVGLengthContext&, float, SVGLengthMode, SVGLengthType);

    float valueInSpecifiedUnits() const { return m_valueInSpecifiedUnits; }
    void setValueInSpecifiedUnits(float value) { m_valueInSpecifiedUnits = value; }

    float valueAsPercentage() const;

    String valueAsString() const;
    ExceptionOr<void> setValueAsString(const String&);
    ExceptionOr<void> setValueAsString(const String&, SVGLengthMode);
    
    ExceptionOr<void> newValueSpecifiedUnits(unsigned short, float valueInSpecifiedUnits);
    ExceptionOr<void> convertToSpecifiedUnits(unsigned short, const SVGLengthContext&);

    // Helper functions
    bool isRelative() const
    {
        SVGLengthType type = unitType();
        return type == LengthTypePercentage || type == LengthTypeEMS || type == LengthTypeEXS;
    }

    bool isZero() const 
    {
        return !m_valueInSpecifiedUnits;
    }

    static SVGLength fromCSSPrimitiveValue(const CSSPrimitiveValue&);
    static Ref<CSSPrimitiveValue> toCSSPrimitiveValue(const SVGLength&);
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

        if (fromType == LengthTypePercentage || toType == LengthTypePercentage) {
            float fromPercent = from.valueAsPercentage() * 100;
            float toPercent = valueAsPercentage() * 100;
            auto result = length.newValueSpecifiedUnits(LengthTypePercentage, WebCore::blend(fromPercent, toPercent, progress));
            if (result.hasException())
                return SVGLength();
            return length;
        }

        if (fromType == toType || from.isZero() || isZero() || fromType == LengthTypeEMS || fromType == LengthTypeEXS) {
            float fromValue = from.valueInSpecifiedUnits();
            float toValue = valueInSpecifiedUnits();
            if (isZero()) {
                auto result = length.newValueSpecifiedUnits(fromType, WebCore::blend(fromValue, toValue, progress));
                if (result.hasException())
                    return SVGLength();
            } else {
                auto result = length.newValueSpecifiedUnits(toType, WebCore::blend(fromValue, toValue, progress));
                if (result.hasException())
                    return SVGLength();
            }
            return length;
        }

        ASSERT(!isRelative());
        ASSERT(!from.isRelative());

        SVGLengthContext nonRelativeLengthContext(nullptr);
        auto fromValueInUserUnits = nonRelativeLengthContext.convertValueToUserUnits(from.valueInSpecifiedUnits(), from.unitMode(), fromType);
        if (fromValueInUserUnits.hasException())
            return SVGLength();

        auto fromValue = nonRelativeLengthContext.convertValueFromUserUnits(fromValueInUserUnits.releaseReturnValue(), unitMode(), toType);
        if (fromValue.hasException())
            return SVGLength();

        float toValue = valueInSpecifiedUnits();
        auto result = length.newValueSpecifiedUnits(toType, WebCore::blend(fromValue.releaseReturnValue(), toValue, progress));
        if (result.hasException())
            return SVGLength();
        return length;
    }

private:
    float m_valueInSpecifiedUnits;
    unsigned m_unit;
};

template<> struct SVGPropertyTraits<SVGLength> {
    static SVGLength initialValue() { return SVGLength(); }
    static String toString(const SVGLength& type) { return type.valueAsString(); }
};

TextStream& operator<<(TextStream&, const SVGLength&);

} // namespace WebCore
