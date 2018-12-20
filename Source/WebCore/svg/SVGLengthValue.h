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

class SVGLengthValue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // FIXME: Once all SVGLengthValue users use Length internally, we make this a wrapper for Length.
    SVGLengthValue(SVGLengthMode = LengthModeOther, const String& valueAsString = String());
    SVGLengthValue(const SVGLengthContext&, float, SVGLengthMode = LengthModeOther, SVGLengthType = LengthTypeNumber);

    SVGLengthType unitType() const;
    SVGLengthMode unitMode() const;

    bool operator==(const SVGLengthValue&) const;
    bool operator!=(const SVGLengthValue&) const;

    static SVGLengthValue construct(SVGLengthMode, const String&, SVGParsingError&, SVGLengthNegativeValuesMode = AllowNegativeLengths);

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
        auto type = unitType();
        return type == LengthTypePercentage || type == LengthTypeEMS || type == LengthTypeEXS;
    }

    bool isZero() const 
    {
        return !m_valueInSpecifiedUnits;
    }

    static SVGLengthValue fromCSSPrimitiveValue(const CSSPrimitiveValue&);
    static Ref<CSSPrimitiveValue> toCSSPrimitiveValue(const SVGLengthValue&);
    static SVGLengthMode lengthModeForAnimatedLengthAttribute(const QualifiedName&);

    SVGLengthValue blend(const SVGLengthValue& from, float progress) const
    {
        auto toType = unitType();
        auto fromType = from.unitType();
        if ((from.isZero() && isZero())
            || fromType == LengthTypeUnknown
            || toType == LengthTypeUnknown
            || (!from.isZero() && fromType != LengthTypePercentage && toType == LengthTypePercentage)
            || (!isZero() && fromType == LengthTypePercentage && toType != LengthTypePercentage)
            || (!from.isZero() && !isZero() && (fromType == LengthTypeEMS || fromType == LengthTypeEXS) && fromType != toType))
            return *this;

        SVGLengthValue length;

        if (fromType == LengthTypePercentage || toType == LengthTypePercentage) {
            auto fromPercent = from.valueAsPercentage() * 100;
            auto toPercent = valueAsPercentage() * 100;
            auto result = length.newValueSpecifiedUnits(LengthTypePercentage, WebCore::blend(fromPercent, toPercent, progress));
            if (result.hasException())
                return { };
            return length;
        }

        if (fromType == toType || from.isZero() || isZero() || fromType == LengthTypeEMS || fromType == LengthTypeEXS) {
            auto fromValue = from.valueInSpecifiedUnits();
            auto toValue = valueInSpecifiedUnits();
            if (isZero()) {
                auto result = length.newValueSpecifiedUnits(fromType, WebCore::blend(fromValue, toValue, progress));
                if (result.hasException())
                    return { };
            } else {
                auto result = length.newValueSpecifiedUnits(toType, WebCore::blend(fromValue, toValue, progress));
                if (result.hasException())
                    return { };
            }
            return length;
        }

        ASSERT(!isRelative());
        ASSERT(!from.isRelative());

        SVGLengthContext nonRelativeLengthContext(nullptr);
        auto fromValueInUserUnits = nonRelativeLengthContext.convertValueToUserUnits(from.valueInSpecifiedUnits(), from.unitMode(), fromType);
        if (fromValueInUserUnits.hasException())
            return { };

        auto fromValue = nonRelativeLengthContext.convertValueFromUserUnits(fromValueInUserUnits.releaseReturnValue(), unitMode(), toType);
        if (fromValue.hasException())
            return { };

        float toValue = valueInSpecifiedUnits();
        auto result = length.newValueSpecifiedUnits(toType, WebCore::blend(fromValue.releaseReturnValue(), toValue, progress));
        if (result.hasException())
            return { };
        return length;
    }

private:
    float m_valueInSpecifiedUnits { 0 };
    unsigned m_unit;
};

template<> struct SVGPropertyTraits<SVGLengthValue> {
    static SVGLengthValue initialValue() { return { }; }
    static Optional<SVGLengthValue> parse(const QualifiedName& attrName, const String& string)
    {
        SVGLengthValue length;
        length.setValueAsString(string, SVGLengthValue::lengthModeForAnimatedLengthAttribute(attrName)).hasException();
        return length;
    }
    static String toString(const SVGLengthValue& length) { return length.valueAsString(); }
};

WTF::TextStream& operator<<(WTF::TextStream&, const SVGLengthValue&);

} // namespace WebCore
