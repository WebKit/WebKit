/*
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006, 2008, 2014 Apple Inc. All rights reserved.
    Copyright (C) 2011 Rik Cabanier (cabanier@adobe.com)
    Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef Length_h
#define Length_h

#include "AnimationUtilities.h"
#include <memory>
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>

namespace WebCore {

enum LengthType {
    Auto, Relative, Percent, Fixed,
    Intrinsic, MinIntrinsic,
    MinContent, MaxContent, FillAvailable, FitContent,
    Calculated,
    ViewportPercentageWidth, ViewportPercentageHeight, ViewportPercentageMin, ViewportPercentageMax,
    Undefined
};

class CalculationValue;    
    
struct Length {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Length(LengthType = Auto);

    Length(int value, LengthType, bool hasQuirk = false);
    Length(LayoutUnit value, LengthType, bool hasQuirk = false);
    Length(float value, LengthType, bool hasQuirk = false);
    Length(double value, LengthType, bool hasQuirk = false);

    explicit Length(PassRef<CalculationValue>);

    Length(const Length&);
    Length(Length&&);
    Length& operator=(const Length&);
    Length& operator=(Length&&);

    ~Length();

    void setValue(LengthType, int value);
    void setValue(LengthType, float value);
    void setValue(LengthType, LayoutUnit value);
    Length& operator*=(float);

    void setHasQuirk(bool);

    bool operator==(const Length&) const;
    bool operator!=(const Length&) const;

    float value() const;
    int intValue() const;
    float percent() const;
    float viewportPercentageLength() const;
    CalculationValue& calculationValue() const;

    LengthType type() const;

    bool isAuto() const;
    bool isCalculated() const;
    bool isFixed() const;
    bool isMaxContent() const;
    bool isMinContent() const;
    bool isPercentNotCalculated() const; // FIXME: Rename to isPercent.
    bool isRelative() const;
    bool isUndefined() const;

    bool hasQuirk() const;

    // FIXME calc: https://bugs.webkit.org/show_bug.cgi?id=80357. A calculated Length
    // always contains a percentage, and without a maxValue passed to these functions
    // it's impossible to determine the sign or zero-ness. The following three functions
    // act as if all calculated values are positive.
    bool isZero() const;
    bool isPositive() const;
    bool isNegative() const;

    bool isPercent() const; // Returns true for both Percent and Calculated. FIXME: Find a better name for this.

    bool isIntrinsic() const;
    bool isIntrinsicOrAuto() const;
    bool isSpecified() const;
    bool isSpecifiedOrIntrinsic() const;
    bool isViewportPercentage() const;

    // Blend two lengths to produce a new length that is in between them. Used for animation.
    // FIXME: Why is this a member function?
    Length blend(const Length& from, double progress) const;

    float nonNanCalculatedValue(int maxValue) const;

private:
    bool isLegacyIntrinsic() const;

    bool isCalculatedEqual(const Length&) const;
    Length blendMixedTypes(const Length& from, double progress) const;

    void ref() const;
    void deref() const;
    
    union {
        int m_intValue;
        float m_floatValue;
        unsigned m_calculationValueHandle;
    };
    bool m_hasQuirk;
    unsigned char m_type;
    bool m_isFloat;
};

std::unique_ptr<Length[]> newCoordsArray(const String&, int& length);
std::unique_ptr<Length[]> newLengthArray(const String&, int& length);

inline Length::Length(LengthType type)
    : m_intValue(0), m_hasQuirk(false), m_type(type), m_isFloat(false)
{
    ASSERT(type != Calculated);
}

inline Length::Length(int value, LengthType type, bool hasQuirk)
    : m_intValue(value), m_hasQuirk(hasQuirk), m_type(type), m_isFloat(false)
{
    ASSERT(type != Calculated);
}

inline Length::Length(LayoutUnit value, LengthType type, bool hasQuirk)
    : m_floatValue(value.toFloat()), m_hasQuirk(hasQuirk), m_type(type), m_isFloat(true)
{
    ASSERT(type != Calculated);
}

inline Length::Length(float value, LengthType type, bool hasQuirk)
    : m_floatValue(value), m_hasQuirk(hasQuirk), m_type(type), m_isFloat(true)
{
    ASSERT(type != Calculated);
}

inline Length::Length(double value, LengthType type, bool hasQuirk)
    : m_floatValue(static_cast<float>(value)), m_hasQuirk(hasQuirk), m_type(type), m_isFloat(true)
{
    ASSERT(type != Calculated);
}

inline Length::Length(const Length& other)
{
    if (other.isCalculated())
        other.ref();

    memcpy(this, &other, sizeof(Length));
}

inline Length::Length(Length&& other)
{
    memcpy(this, &other, sizeof(Length));
    other.m_type = Auto;
}

inline Length& Length::operator=(const Length& other)
{
    if (other.isCalculated())
        other.ref();
    if (isCalculated())
        deref();

    memcpy(this, &other, sizeof(Length));
    return *this;
}

inline Length& Length::operator=(Length&& other)
{
    if (this == &other)
        return *this;

    if (isCalculated())
        deref();

    memcpy(this, &other, sizeof(Length));
    other.m_type = Auto;
    return *this;
}

inline Length::~Length()
{
    if (isCalculated())
        deref();
}

inline bool Length::operator==(const Length& other) const
{
    // FIXME: This might be too long to be inline.
    if (type() != other.type() || hasQuirk() != other.hasQuirk())
        return false;
    if (isUndefined())
        return true;
    if (isCalculated())
        return isCalculatedEqual(other);
    return value() == other.value();
}

inline bool Length::operator!=(const Length& other) const
{
    return !(*this == other);
}

inline Length& Length::operator*=(float value)
{
    ASSERT(!isCalculated());
    if (isCalculated())
        return *this;

    if (m_isFloat)
        m_floatValue *= value;
    else
        m_intValue *= value;

    return *this;
}

inline float Length::value() const
{
    ASSERT(!isUndefined());
    ASSERT(!isCalculated());
    return m_isFloat ? m_floatValue : m_intValue;
}

inline int Length::intValue() const
{
    ASSERT(!isUndefined());
    ASSERT(!isCalculated());
    // FIXME: Makes no sense to return 0 here but not in the value() function above.
    if (isCalculated())
        return 0;
    return m_isFloat ? static_cast<int>(m_floatValue) : m_intValue;
}

inline float Length::percent() const
{
    ASSERT(isPercentNotCalculated());
    return value();
}

inline float Length::viewportPercentageLength() const
{
    ASSERT(isViewportPercentage());
    return value();
}

inline LengthType Length::type() const
{
    return static_cast<LengthType>(m_type);
}

inline bool Length::hasQuirk() const
{
    return m_hasQuirk;
}

inline void Length::setHasQuirk(bool hasQuirk)
{
    m_hasQuirk = hasQuirk;
}

inline void Length::setValue(LengthType type, int value)
{
    ASSERT(m_type != Calculated);
    ASSERT(type != Calculated);
    m_type = type;
    m_intValue = value;
    m_isFloat = false;
}

inline void Length::setValue(LengthType type, float value)
{
    ASSERT(m_type != Calculated);
    ASSERT(type != Calculated);
    m_type = type;
    m_floatValue = value;
    m_isFloat = true;
}

inline void Length::setValue(LengthType type, LayoutUnit value)
{
    ASSERT(m_type != Calculated);
    ASSERT(type != Calculated);
    m_type = type;
    m_floatValue = value;
    m_isFloat = true;
}

inline bool Length::isAuto() const
{
    return type() == Auto;
}

inline bool Length::isFixed() const
{
    return type() == Fixed;
}

inline bool Length::isMaxContent() const
{
    return type() == MaxContent;
}

inline bool Length::isMinContent() const
{
    return type() == MinContent;
}

inline bool Length::isNegative() const
{
    if (isUndefined() || isCalculated())
        return false;
    return m_isFloat ? (m_floatValue < 0) : (m_intValue < 0);
}

inline bool Length::isPercentNotCalculated() const
{
    return type() == Percent;
}

inline bool Length::isRelative() const
{
    return type() == Relative;
}

inline bool Length::isUndefined() const
{
    return type() == Undefined;
}

inline bool Length::isPercent() const
{
    return isPercentNotCalculated() || isCalculated();
}

inline bool Length::isPositive() const
{
    if (isUndefined())
        return false;
    if (isCalculated())
        return true;
    return m_isFloat ? (m_floatValue > 0) : (m_intValue > 0);
}

inline bool Length::isZero() const
{
    ASSERT(!isUndefined());
    if (isCalculated())
        return false;
    return m_isFloat ? !m_floatValue : !m_intValue;
}

inline bool Length::isCalculated() const
{
    return type() == Calculated;
}

inline bool Length::isLegacyIntrinsic() const
{
    return type() == Intrinsic || type() == MinIntrinsic;
}

inline bool Length::isIntrinsic() const
{
    return type() == MinContent || type() == MaxContent || type() == FillAvailable || type() == FitContent;
}

inline bool Length::isIntrinsicOrAuto() const
{
    return isAuto() || isIntrinsic() || isLegacyIntrinsic();
}

inline bool Length::isSpecified() const
{
    return isFixed() || isPercent() || isViewportPercentage();
}

inline bool Length::isSpecifiedOrIntrinsic() const
{
    return isSpecified() || isIntrinsic();
}

inline bool Length::isViewportPercentage() const
{
    return type() >= ViewportPercentageWidth && type() <= ViewportPercentageMax;
}

// FIXME: Does this need to be in the header? Is it valuable to inline? Does it get inlined?
inline Length Length::blend(const Length& from, double progress) const
{
    if (from.type() == Calculated || type() == Calculated)
        return blendMixedTypes(from, progress);

    if (!from.isZero() && !isZero() && from.type() != type())
        return blendMixedTypes(from, progress);

    if (from.isZero() && isZero())
        return *this;

    LengthType resultType = type();
    if (isZero())
        resultType = from.type();

    if (resultType == Percent) {
        float fromPercent = from.isZero() ? 0 : from.percent();
        float toPercent = isZero() ? 0 : percent();
        return Length(WebCore::blend(fromPercent, toPercent, progress), Percent);
    }

    float fromValue = from.isZero() ? 0 : from.value();
    float toValue = isZero() ? 0 : value();
    return Length(WebCore::blend(fromValue, toValue, progress), resultType);
}

} // namespace WebCore

#endif // Length_h
