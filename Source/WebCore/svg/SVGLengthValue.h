/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "ExceptionOr.h"
#include "SVGParsingError.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

class CSSPrimitiveValue;
class SVGLengthContext;

enum class SVGLengthType : uint8_t {
    Unknown = 0,
    Number,
    Percentage,
    Ems,
    Exs,
    Pixels,
    Centimeters,
    Millimeters,
    Inches,
    Points,
    Picas
};

enum class SVGLengthMode : uint8_t {
    Width,
    Height,
    Other
};

enum class SVGLengthNegativeValuesMode : uint8_t {
    Allow,
    Forbid
};

class SVGLengthValue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SVGLengthValue(SVGLengthMode = SVGLengthMode::Other, const String& valueAsString = { });
    SVGLengthValue(float valueInSpecifiedUnits, SVGLengthType, SVGLengthMode = SVGLengthMode::Other);
    SVGLengthValue(const SVGLengthContext&, float, SVGLengthType = SVGLengthType::Number, SVGLengthMode = SVGLengthMode::Other);

    static std::optional<SVGLengthValue> construct(SVGLengthMode, StringView);
    static SVGLengthValue construct(SVGLengthMode, StringView, SVGParsingError&, SVGLengthNegativeValuesMode = SVGLengthNegativeValuesMode::Allow);
    static SVGLengthValue blend(const SVGLengthValue& from, const SVGLengthValue& to, float progress);

    static SVGLengthValue fromCSSPrimitiveValue(const CSSPrimitiveValue&);
    static Ref<CSSPrimitiveValue> toCSSPrimitiveValue(const SVGLengthValue&);

    SVGLengthType lengthType() const { return m_lengthType; }
    SVGLengthMode lengthMode() const { return m_lengthMode; }

    bool isZero() const { return !m_valueInSpecifiedUnits;  }
    bool isRelative() const { return m_lengthType == SVGLengthType::Percentage || m_lengthType == SVGLengthType::Ems || m_lengthType == SVGLengthType::Exs; }

    float value(const SVGLengthContext&) const;
    float valueAsPercentage() const { return m_lengthType == SVGLengthType::Percentage ? m_valueInSpecifiedUnits / 100 : m_valueInSpecifiedUnits; }
    float valueInSpecifiedUnits() const { return m_valueInSpecifiedUnits; }
    
    String valueAsString() const;
    ExceptionOr<float> valueForBindings(const SVGLengthContext&) const;

    void setValueInSpecifiedUnits(float value) { m_valueInSpecifiedUnits = value; }
    ExceptionOr<void> setValue(const SVGLengthContext&, float);
    ExceptionOr<void> setValue(const SVGLengthContext&, float, SVGLengthType, SVGLengthMode);

    ExceptionOr<void> setValueAsString(StringView);
    ExceptionOr<void> setValueAsString(StringView, SVGLengthMode);

    ExceptionOr<void> convertToSpecifiedUnits(const SVGLengthContext&, SVGLengthType);

private:
    float m_valueInSpecifiedUnits { 0 };
    SVGLengthType m_lengthType { SVGLengthType::Number };
    SVGLengthMode m_lengthMode { SVGLengthMode::Other };
};

inline bool operator==(const SVGLengthValue& a, const SVGLengthValue& b)
{
    return a.valueInSpecifiedUnits() == b.valueInSpecifiedUnits() && a.lengthType() == b.lengthType() && a.lengthMode() == b.lengthMode();
}

inline bool operator!=(const SVGLengthValue& a, const SVGLengthValue& b)
{
    return a.valueInSpecifiedUnits() != b.valueInSpecifiedUnits() || a.lengthType() != b.lengthType() || a.lengthMode() != b.lengthMode();
}

WTF::TextStream& operator<<(WTF::TextStream&, const SVGLengthValue&);

} // namespace WebCore
