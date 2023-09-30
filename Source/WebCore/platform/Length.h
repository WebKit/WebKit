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

#pragma once

#include "AnimationUtilities.h"
#include <string.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/UniqueArray.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

enum class LengthType : uint8_t {
    Auto,
    Normal,
    Relative,
    Percent,
    Fixed,
    Intrinsic,
    MinIntrinsic,
    MinContent,
    MaxContent,
    FillAvailable,
    FitContent,
    Calculated,
    Content,
    Undefined
};

enum class ValueRange : uint8_t {
    All,
    NonNegative
};

struct BlendingContext;
class CalculationValue;

struct Length {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Length(LengthType = LengthType::Auto);

    Length(int value, LengthType, bool hasQuirk = false);
    Length(LayoutUnit value, LengthType, bool hasQuirk = false);
    Length(float value, LengthType, bool hasQuirk = false);
    Length(double value, LengthType, bool hasQuirk = false);

    WEBCORE_EXPORT explicit Length(Ref<CalculationValue>&&);

    Length(const Length&);
    Length(Length&&);
    Length& operator=(const Length&);
    Length& operator=(Length&&);

    ~Length();

    void setValue(LengthType, int value);
    void setValue(LengthType, float value);
    void setValue(LengthType, LayoutUnit value);
    Length& operator*=(float);

    bool operator==(const Length&) const;

    float value() const;
    int intValue() const;
    float percent() const;
    CalculationValue& calculationValue() const;

    LengthType type() const;

    bool isAuto() const;
    bool isCalculated() const;
    bool isFixed() const;
    bool isMaxContent() const;
    bool isMinContent() const;
    bool isNormal() const;
    bool isPercent() const;
    bool isRelative() const;
    bool isUndefined() const;
    bool isFillAvailable() const;
    bool isFitContent() const;
    bool isMinIntrinsic() const;
    bool isContent() const;

    bool hasQuirk() const;
    void setHasQuirk(bool);

    // FIXME calc: https://bugs.webkit.org/show_bug.cgi?id=80357. A calculated Length
    // always contains a percentage, and without a maxValue passed to these functions
    // it's impossible to determine the sign or zero-ness. The following three functions
    // act as if all calculated values are positive.
    bool isZero() const;
    bool isPositive() const;
    bool isNegative() const;

    bool isFloat() const;

    bool isPercentOrCalculated() const; // Returns true for both Percent and Calculated.

    bool isIntrinsic() const;
    bool isIntrinsicOrAuto() const;
    bool isSpecified() const;
    bool isSpecifiedOrIntrinsic() const;

    WEBCORE_EXPORT float nonNanCalculatedValue(float maxValue) const;

    bool isLegacyIntrinsic() const;

private:
    friend struct IPC::ArgumentCoder<Length, void>;
    Length(std::variant<int, float, unsigned>&& value, LengthType type, bool hasQuirk)
        : m_value(WTFMove(value))
        , m_type(type)
        , m_hasQuirk(hasQuirk)
    {
        // This constructor is solely used for IPC decoding, and all such Length
        // objects should be Fixed
        RELEASE_ASSERT(isFixed());
    }

    bool isCalculatedEqual(const Length&) const;

    void initialize(const Length&);
    void initialize(Length&&);

    WEBCORE_EXPORT void ref() const;
    WEBCORE_EXPORT void deref() const;

    std::variant<int, float, unsigned> m_value;
    LengthType m_type;
    bool m_hasQuirk { false };
};

// Blend two lengths to produce a new length that is in between them. Used for animation.
Length blend(const Length& from, const Length& to, const BlendingContext&);
Length blend(const Length& from, const Length& to, const BlendingContext&, ValueRange);

UniqueArray<Length> newLengthArray(const String&, int& length);

inline Length::Length(LengthType type)
    : m_type(type)
{
    ASSERT(type != LengthType::Calculated);
}

inline Length::Length(int value, LengthType type, bool hasQuirk)
    : m_value(value)
    , m_type(type)
    , m_hasQuirk(hasQuirk)
{
    ASSERT(type != LengthType::Calculated);
}

inline Length::Length(LayoutUnit value, LengthType type, bool hasQuirk)
    : m_value(value.toFloat())
    , m_type(type)
    , m_hasQuirk(hasQuirk)
{
    ASSERT(type != LengthType::Calculated);
}

inline Length::Length(float value, LengthType type, bool hasQuirk)
    : m_value(value)
    , m_type(type)
    , m_hasQuirk(hasQuirk)
{
    ASSERT(type != LengthType::Calculated);
}

inline Length::Length(double value, LengthType type, bool hasQuirk)
    : m_value(static_cast<float>(value))
    , m_type(type)
    , m_hasQuirk(hasQuirk)
{
    ASSERT(type != LengthType::Calculated);
}

inline Length::Length(const Length& other)
{
    initialize(other);
}

inline Length::Length(Length&& other)
{
    initialize(WTFMove(other));
}

inline Length& Length::operator=(const Length& other)
{
    if (this == &other)
        return *this;

    if (isCalculated())
        deref();

    initialize(other);
    return *this;
}

inline Length& Length::operator=(Length&& other)
{
    if (this == &other)
        return *this;

    if (isCalculated())
        deref();

    initialize(WTFMove(other));
    return *this;
}

inline void Length::initialize(const Length& other)
{
    m_type = other.m_type;
    m_hasQuirk = other.m_hasQuirk;
    m_value = other.m_value;

    if (m_type == LengthType::Calculated)
        ref();
}

inline void Length::initialize(Length&& other)
{
    m_type = other.m_type;
    m_hasQuirk = other.m_hasQuirk;
    m_value = other.m_value;

    other.m_type = LengthType::Auto;
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

inline Length& Length::operator*=(float value)
{
    ASSERT(!isCalculated());
    if (isCalculated())
        return *this;

    std::visit([&](auto arg) { m_value = arg * value; }, m_value);

    return *this;
}

inline float Length::value() const
{
    ASSERT(!isUndefined());
    ASSERT(!isCalculated());

    return std::visit([](auto arg) { return static_cast<float>(arg); }, m_value);
}

inline int Length::intValue() const
{
    ASSERT(!isUndefined());
    ASSERT(!isCalculated());
    // FIXME: Makes no sense to return 0 here but not in the value() function above.
    if (isCalculated())
        return 0;

    return std::visit([](auto arg) { return static_cast<int>(arg); }, m_value);
}

inline float Length::percent() const
{
    ASSERT(isPercent());
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

inline bool Length::isFloat() const
{
    return std::holds_alternative<float>(m_value);
}

inline void Length::setHasQuirk(bool hasQuirk)
{
    m_hasQuirk = hasQuirk;
}

inline void Length::setValue(LengthType type, int value)
{
    ASSERT(m_type != LengthType::Calculated);
    ASSERT(type != LengthType::Calculated);
    m_type = type;
    m_value = value;
}

inline void Length::setValue(LengthType type, float value)
{
    ASSERT(m_type != LengthType::Calculated);
    ASSERT(type != LengthType::Calculated);
    m_type = type;
    m_value = value;
}

inline void Length::setValue(LengthType type, LayoutUnit value)
{
    ASSERT(m_type != LengthType::Calculated);
    ASSERT(type != LengthType::Calculated);
    m_type = type;
    m_value = value.toFloat();
}

inline bool Length::isNormal() const
{
    return type() == LengthType::Normal;
}

inline bool Length::isAuto() const
{
    return type() == LengthType::Auto;
}

inline bool Length::isFixed() const
{
    return type() == LengthType::Fixed;
}

inline bool Length::isMaxContent() const
{
    return type() == LengthType::MaxContent;
}

inline bool Length::isMinContent() const
{
    return type() == LengthType::MinContent;
}

inline bool Length::isNegative() const
{
    if (isUndefined() || isCalculated())
        return false;
    return std::visit([](auto arg) { return arg < 0; }, m_value);
}

inline bool Length::isPercent() const
{
    return type() == LengthType::Percent;
}

inline bool Length::isRelative() const
{
    return type() == LengthType::Relative;
}

inline bool Length::isUndefined() const
{
    return type() == LengthType::Undefined;
}

inline bool Length::isPercentOrCalculated() const
{
    return isPercent() || isCalculated();
}

inline bool Length::isPositive() const
{
    if (isUndefined())
        return false;
    if (isCalculated())
        return true;
    return std::visit([](auto arg) { return arg > 0; }, m_value);
}

inline bool Length::isZero() const
{
    ASSERT(!isUndefined());
    if (isCalculated() || isAuto())
        return false;
    return std::visit([](auto arg) { return !arg; }, m_value);
}

inline bool Length::isCalculated() const
{
    return type() == LengthType::Calculated;
}

inline bool Length::isLegacyIntrinsic() const
{
    return type() == LengthType::Intrinsic || type() == LengthType::MinIntrinsic;
}

inline bool Length::isIntrinsic() const
{
    // FIXME: This is misleadingly named. One would expect this function does "return type() == LengthType::Intrinsic;".
    return type() == LengthType::MinContent || type() == LengthType::MaxContent || type() == LengthType::FillAvailable || type() == LengthType::FitContent;
}

inline bool Length::isIntrinsicOrAuto() const
{
    return isAuto() || isIntrinsic() || isLegacyIntrinsic();
}

inline bool Length::isSpecified() const
{
    return isFixed() || isPercentOrCalculated();
}

inline bool Length::isSpecifiedOrIntrinsic() const
{
    return isSpecified() || isIntrinsic();
}

inline bool Length::isFillAvailable() const
{
    return type() == LengthType::FillAvailable;
}

inline bool Length::isFitContent() const
{
    return type() == LengthType::FitContent;
}

inline bool Length::isMinIntrinsic() const
{
    return type() == LengthType::MinIntrinsic;
}

inline bool Length::isContent() const
{
    return type() == LengthType::Content;
}

Length convertTo100PercentMinusLength(const Length&);

WTF::TextStream& operator<<(WTF::TextStream&, Length);

} // namespace WebCore
