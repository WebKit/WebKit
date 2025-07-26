/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveNumericConcepts.h"
#include "CSSPrimitiveNumericRange.h"
#include "CSSUnits.h"
#include "CalculationCategory.h"
#include <wtf/Brigand.h>
#include <wtf/EnumTraits.h>
#include <wtf/MathExtras.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {
namespace CSS {

// MARK: - Unit Literal Type

template<auto unitValue>
    requires UnitEnum<decltype(unitValue)>
struct ValueLiteral {
    using UnitType = decltype(unitValue);

    static constexpr UnitType unit = unitValue;
    double value;

    constexpr explicit ValueLiteral(double initialValue)
        : value { initialValue }
    {
    }

    // Synthesize all comparison and equality operators.

    constexpr auto operator<=>(const ValueLiteral&) const = default;

    // Support unary operators.

    constexpr ValueLiteral operator+()
    {
        return ValueLiteral { value };
    }

    constexpr ValueLiteral operator-()
    {
        return ValueLiteral { -value };
    }

    // Support addition between `ValueLiteral` and machine numeric types.

    constexpr ValueLiteral& operator+=(const ValueLiteral& rhs)
    {
        value += rhs.value;
        return *this;
    }
    constexpr ValueLiteral& operator+=(std::convertible_to<double> auto const& rhs)
    {
        value += static_cast<double>(rhs);
        return *this;
    }
    friend constexpr ValueLiteral operator+(const ValueLiteral& lhs, const ValueLiteral& rhs)
    {
        return ValueLiteral { lhs.value + rhs.value };
    }
    friend constexpr ValueLiteral operator+(const ValueLiteral& lhs, std::convertible_to<double> auto const& rhs)
    {
        return ValueLiteral { lhs.value + static_cast<double>(rhs) };
    }
    friend constexpr ValueLiteral operator+(std::convertible_to<double> auto const& lhs, const ValueLiteral& rhs)
    {
        return ValueLiteral { static_cast<double>(lhs) + rhs.value };
    }

    // Support subtraction between `ValueLiteral` and machine numeric types.

    constexpr ValueLiteral& operator-=(const ValueLiteral& rhs)
    {
        value -= rhs.value;
        return *this;
    }
    constexpr ValueLiteral& operator-=(std::convertible_to<double> auto const& rhs)
    {
        value -= static_cast<double>(rhs);
        return *this;
    }
    friend constexpr ValueLiteral operator-(const ValueLiteral& lhs, const ValueLiteral& rhs)
    {
        return ValueLiteral { lhs.value - rhs.value };
    }
    friend constexpr ValueLiteral operator-(const ValueLiteral& lhs, std::convertible_to<double> auto const& rhs)
    {
        return ValueLiteral { lhs.value - static_cast<double>(rhs) };
    }
    friend constexpr ValueLiteral operator-(std::convertible_to<double> auto const& lhs, const ValueLiteral& rhs)
    {
        return ValueLiteral { static_cast<double>(lhs) - rhs.value };
    }

    // Support multiplication between `ValueLiteral` and machine numeric types.

    constexpr ValueLiteral& operator*=(std::convertible_to<double> auto const& rhs)
    {
        value *= static_cast<double>(rhs);
        return *this;
    }
    friend constexpr ValueLiteral operator*(const ValueLiteral& lhs, std::convertible_to<double> auto const& rhs)
    {
        return ValueLiteral { lhs.value * static_cast<double>(rhs) };
    }
    friend constexpr ValueLiteral operator*(std::convertible_to<double> auto const& lhs, const ValueLiteral& rhs)
    {
        return ValueLiteral { static_cast<double>(lhs) * rhs.value };
    }

    // Support division between `ValueLiteral` and machine numeric types.

    constexpr ValueLiteral& operator/=(std::convertible_to<double> auto const& rhs)
    {
        value /= static_cast<double>(rhs);
        return *this;
    }
    friend constexpr ValueLiteral operator/(const ValueLiteral& lhs, std::convertible_to<double> auto const& rhs)
    {
        return ValueLiteral { lhs.value / static_cast<double>(rhs) };
    }
};

#define CSS_DEFINE_UNIT_LITERAL(type, name) \
    inline namespace Literals { \
        consteval ValueLiteral<type> operator""_css_##name(long double value) { return ValueLiteral<type> { static_cast<double>(value) }; } \
        consteval ValueLiteral<type> operator""_css_##name(unsigned long long value) { return ValueLiteral<type> { static_cast<double>(value) }; } \
    }

// MARK: - Unit Cast

// Checks if casting `other` to unit type `T` is a valid cast.
template<UnitEnum T, UnitEnum U>
constexpr bool isUnit(U other)
{
    if constexpr (std::same_as<T, U>)
        return true;
    else if constexpr (NestedUnitEnumOf<U, T>)
        return true;
    else if constexpr (NestedUnitEnumOf<T, U>)
        return UnitTraits<U>::template is<T>(other);
}

// Allows identity casts, upcasts or downcasts of a unit type.
template<UnitEnum T, UnitEnum U>
constexpr T unitCast(U other)
{
    if constexpr (std::same_as<T, U>)
        return other;
    else if constexpr (NestedUnitEnumOf<U, T>)
        return UnitTraits<T>::upcast(other);
    else if constexpr (NestedUnitEnumOf<T, U>) {
        RELEASE_ASSERT(isUnit<T>(other));
        return UnitTraits<U>::template downcast<T>(other);
    }
}

// Allows casting UP from a unit type to a composite unit type that includes it.
//    `AnglePercentageUnit::Deg` == unitUpcast<AnglePercentageUnit>(AngleUnit::Deg)
template<UnitEnum T, UnitEnum U>
constexpr std::optional<T> dynamicUnitCast(U other)
{
    if constexpr (std::same_as<T, U>)
        return other;
    else if constexpr (NestedUnitEnumOf<U, T>)
        return UnitTraits<T>::upcast(other);
    else if constexpr (NestedUnitEnumOf<T, U>) {
        if (!isUnit<T>(other))
            return { };
        return UnitTraits<U>::template downcast<T>(other);
    }
}

// Allows casting UP from a unit type to a composite unit type that includes it.
//    `AnglePercentageUnit::Deg` == unitUpcast<AnglePercentageUnit>(AngleUnit::Deg)
template<UnitEnum T, UnitEnum U>
    requires NestedUnitEnumOf<U, T>
constexpr T unitUpcast(U other)
{
    return UnitTraits<T>::upcast(other);
}

// Allows casting DOWN from a composite unit type to one of the unit types it includes.
//    `AngleUnit::Deg` == unitDowncast<AngleUnit>(AnglePercentageUnit::Deg)
//    RELEASE_ASSERT   == unitDowncast<AngleUnit>(AnglePercentageUnit::Percentage)
// NOTE: This will RELEASE_ASSERT that the cast is valid.
template<UnitEnum T, UnitEnum U>
    requires NestedUnitEnumOf<T, U>
constexpr T unitDowncast(U other)
{
    RELEASE_ASSERT(isUnit<T>(other));
    return UnitTraits<U>::template downcast<T>(other);
}

// Allows conditional casting DOWN from a composite unit type to one of the unit types it includes.
//    `AngleUnit::Deg` == dynamicUnitDowncast<AngleUnit>(AnglePercentageUnit::Deg)
//    `std::nullopt`   == dynamicUnitDowncast<AngleUnit>(AnglePercentageUnit::Percentage)
// NOTE: Returns `std::nullopt` if the cast cannot succeed.
template<UnitEnum T, UnitEnum U>
    requires NestedUnitEnumOf<T, U>
constexpr std::optional<T> dynamicUnitDowncast(U other)
{
    if (!isUnit<T>(other))
        return { };
    return UnitTraits<U>::template downcast<T>(other);
}

// MARK: - Generic Unit Switching for Composite Units.

template<CompositeUnitEnum U, typename... F> constexpr decltype(auto) switchOnUnitType(U unit, F&&... f)
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    return UnitTraits<U>::switchOnUnitType(unit, visitor);
}

// NOTE: All unit enums use an underlying type of `uint8_t` for consistency.

// MARK: - <integer>

enum class IntegerUnit : uint8_t {
    Integer
};

constexpr CSSUnitType toCSSUnitType(IntegerUnit)
{
    return CSSUnitType::CSS_INTEGER;
}

constexpr std::optional<IntegerUnit> toIntegerUnit(CSSUnitType cssUnit)
{
    if (cssUnit == CSSUnitType::CSS_INTEGER)
        return IntegerUnit::Integer;
    return std::nullopt;
}

constexpr bool conversionToCanonicalUnitRequiresConversionData(IntegerUnit)
{
    return false;
}

constexpr ASCIILiteral unitString(IntegerUnit)
{
    return ""_s;
}

template<> struct UnitTraits<IntegerUnit> {
    static constexpr auto count = 1;
    static constexpr auto canonical = IntegerUnit::Integer;
    static constexpr auto category = Calculation::Category::Integer;
    static consteval bool isValidRangeForCategory(Range) { return true; }

    static constexpr std::optional<IntegerUnit> validate(CSSUnitType cssUnit) { return toIntegerUnit(cssUnit); }
};
static_assert(UnitTraits<IntegerUnit>::count == enumToUnderlyingType(IntegerUnit::Integer) + 1);

CSS_DEFINE_UNIT_LITERAL(IntegerUnit::Integer, integer)

// MARK: - <number>

enum class NumberUnit : uint8_t {
    Number
};

constexpr CSSUnitType toCSSUnitType(NumberUnit)
{
    return CSSUnitType::CSS_NUMBER;
}

constexpr std::optional<NumberUnit> toNumberUnit(CSSUnitType cssUnit)
{
    if (cssUnit == CSSUnitType::CSS_NUMBER)
        return NumberUnit::Number;
    return std::nullopt;
}

constexpr bool conversionToCanonicalUnitRequiresConversionData(NumberUnit)
{
    return false;
}

constexpr ASCIILiteral unitString(NumberUnit)
{
    return ""_s;
}

template<> struct UnitTraits<NumberUnit> {
    static constexpr auto count = 1;
    static constexpr auto canonical = NumberUnit::Number;
    static constexpr auto category = Calculation::Category::Number;
    static consteval bool isValidRangeForCategory(Range) { return true; }

    static constexpr std::optional<NumberUnit> validate(CSSUnitType cssUnit) { return toNumberUnit(cssUnit); }
};
static_assert(UnitTraits<NumberUnit>::count == enumToUnderlyingType(NumberUnit::Number) + 1);

CSS_DEFINE_UNIT_LITERAL(NumberUnit::Number, number)

// MARK: - <percentage>

enum class PercentageUnit : uint8_t {
    Percentage
};

constexpr CSSUnitType toCSSUnitType(PercentageUnit)
{
    return CSSUnitType::CSS_PERCENTAGE;
}

constexpr std::optional<PercentageUnit> toPercentageUnit(CSSUnitType cssUnit)
{
    if (cssUnit == CSSUnitType::CSS_PERCENTAGE)
        return PercentageUnit::Percentage;
    return std::nullopt;
}

constexpr bool conversionToCanonicalUnitRequiresConversionData(PercentageUnit)
{
    return false;
}

constexpr ASCIILiteral unitString(PercentageUnit)
{
    return "%"_s;
}

template<> struct UnitTraits<PercentageUnit> {
    static constexpr auto count = 1;
    static constexpr auto canonical = PercentageUnit::Percentage;
    static constexpr auto category = Calculation::Category::Percentage;
    static consteval bool isValidRangeForCategory(Range) { return true; }

    static constexpr std::optional<PercentageUnit> validate(CSSUnitType cssUnit) { return toPercentageUnit(cssUnit); }
    static consteval bool isValidRangeForUnitType(Range) { return true; }
};
static_assert(UnitTraits<PercentageUnit>::count == enumToUnderlyingType(PercentageUnit::Percentage) + 1);

CSS_DEFINE_UNIT_LITERAL(PercentageUnit::Percentage, percentage)

// MARK: - <angle>

enum class AngleUnit : uint8_t {
    Deg,
    Rad,
    Grad,
    Turn
};

constexpr CSSUnitType toCSSUnitType(AngleUnit angleUnit)
{
    using enum AngleUnit;

    switch (angleUnit) {
    case Deg:    return CSSUnitType::CSS_DEG;
    case Rad:    return CSSUnitType::CSS_RAD;
    case Grad:   return CSSUnitType::CSS_GRAD;
    case Turn:   return CSSUnitType::CSS_TURN;
    }

    WTF_UNREACHABLE();
}

constexpr std::optional<AngleUnit> toAngleUnit(CSSUnitType cssUnit)
{
    using enum AngleUnit;

    switch (cssUnit) {
    case CSSUnitType::CSS_DEG:  return Deg;
    case CSSUnitType::CSS_RAD:  return Rad;
    case CSSUnitType::CSS_GRAD: return Grad;
    case CSSUnitType::CSS_TURN: return Turn;
    default: break;
    }

    return std::nullopt;
}

constexpr bool conversionToCanonicalUnitRequiresConversionData(AngleUnit)
{
    return false;
}

template<AngleUnit To, typename T> constexpr T convertAngle(T value, AngleUnit unit)
{
    using enum AngleUnit;

    if constexpr (To == Deg) {
        switch (unit) {
        case Deg:
            return value;
        case Rad:
            return rad2deg(value);
        case Grad:
            return grad2deg(value);
        case Turn:
            return turn2deg(value);
        }

        WTF_UNREACHABLE();
    } else if constexpr (To == Rad) {
        switch (unit) {
        case Deg:
            return deg2rad(value);
        case Rad:
            return value;
        case Grad:
            return grad2rad(value);
        case Turn:
            return turn2rad(value);
        }

        WTF_UNREACHABLE();
    } else if constexpr (To == Grad) {
        switch (unit) {
        case Deg:
            return deg2grad(value);
        case Rad:
            return rad2grad(value);
        case Grad:
            return value;
        case Turn:
            return turn2grad(value);
        }

        WTF_UNREACHABLE();
    } else if constexpr (To == Turn) {
        switch (unit) {
        case Deg:
            return deg2turn(value);
        case Rad:
            return rad2Turn(value);
        case Grad:
            return grad2turn(value);
        case Turn:
            return value;
        }

        WTF_UNREACHABLE();
    }
}

ASCIILiteral unitString(AngleUnit);

template<> struct UnitTraits<AngleUnit> {
    static constexpr auto count = 4;
    static constexpr auto canonical = AngleUnit::Deg;
    static constexpr auto category = Calculation::Category::Angle;
    static consteval bool isValidRangeForCategory(Range) { return true; }

    static constexpr std::optional<AngleUnit> validate(CSSUnitType cssUnit) { return toAngleUnit(cssUnit); }
    template<AngleUnit To, typename T> static constexpr T convert(T value, AngleUnit unit) { return convertAngle<To, T>(value, unit); }
};
static_assert(UnitTraits<AngleUnit>::count == enumToUnderlyingType(AngleUnit::Turn) + 1);

CSS_DEFINE_UNIT_LITERAL(AngleUnit::Deg, deg)
CSS_DEFINE_UNIT_LITERAL(AngleUnit::Rad, rad)
CSS_DEFINE_UNIT_LITERAL(AngleUnit::Grad, grad)
CSS_DEFINE_UNIT_LITERAL(AngleUnit::Turn, turn)

// MARK: - <length>

enum class LengthUnit : uint8_t {
    Px,
    Cm,
    Mm,
    Q,
    In,
    Pt,
    Pc,

    // "font dependent" length units
    Em,
    QuirkyEm,
    Ex,
    Lh,
    Cap,
    Ch,
    Ic,

    // "root font dependent" length units
    Rcap,
    Rch,
    Rem,
    Rex,
    Ric,
    Rlh,

    // "viewport-percentage" length units
    Vw,
    Vh,
    Vmin,
    Vmax,
    Vb,
    Vi,
    Svw,
    Svh,
    Svmin,
    Svmax,
    Svb,
    Svi,
    Lvw,
    Lvh,
    Lvmin,
    Lvmax,
    Lvb,
    Lvi,
    Dvw,
    Dvh,
    Dvmin,
    Dvmax,
    Dvb,
    Dvi,

    // "container-percentage" length units
    Cqw,
    Cqh,
    Cqi,
    Cqb,
    Cqmin,
    Cqmax
};

constexpr CSSUnitType toCSSUnitType(LengthUnit lengthUnit)
{
    using enum LengthUnit;

    switch (lengthUnit) {
    case Px:        return CSSUnitType::CSS_PX;
    case Cm:        return CSSUnitType::CSS_CM;
    case Mm:        return CSSUnitType::CSS_MM;
    case Q:         return CSSUnitType::CSS_Q;
    case In:        return CSSUnitType::CSS_IN;
    case Pt:        return CSSUnitType::CSS_PT;
    case Pc:        return CSSUnitType::CSS_PC;
    case Em:        return CSSUnitType::CSS_EM;
    case QuirkyEm:  return CSSUnitType::CSS_QUIRKY_EM;
    case Ex:        return CSSUnitType::CSS_EX;
    case Lh:        return CSSUnitType::CSS_LH;
    case Cap:       return CSSUnitType::CSS_CAP;
    case Ch:        return CSSUnitType::CSS_CH;
    case Ic:        return CSSUnitType::CSS_IC;
    case Rcap:      return CSSUnitType::CSS_RCAP;
    case Rch:       return CSSUnitType::CSS_RCH;
    case Rem:       return CSSUnitType::CSS_REM;
    case Rex:       return CSSUnitType::CSS_REX;
    case Ric:       return CSSUnitType::CSS_RIC;
    case Rlh:       return CSSUnitType::CSS_RLH;
    case Vw:        return CSSUnitType::CSS_VW;
    case Vh:        return CSSUnitType::CSS_VH;
    case Vmin:      return CSSUnitType::CSS_VMIN;
    case Vmax:      return CSSUnitType::CSS_VMAX;
    case Vb:        return CSSUnitType::CSS_VB;
    case Vi:        return CSSUnitType::CSS_VI;
    case Svw:       return CSSUnitType::CSS_SVW;
    case Svh:       return CSSUnitType::CSS_SVH;
    case Svmin:     return CSSUnitType::CSS_SVMIN;
    case Svmax:     return CSSUnitType::CSS_SVMAX;
    case Svb:       return CSSUnitType::CSS_SVB;
    case Svi:       return CSSUnitType::CSS_SVI;
    case Lvw:       return CSSUnitType::CSS_LVW;
    case Lvh:       return CSSUnitType::CSS_LVH;
    case Lvmin:     return CSSUnitType::CSS_LVMIN;
    case Lvmax:     return CSSUnitType::CSS_LVMAX;
    case Lvb:       return CSSUnitType::CSS_LVB;
    case Lvi:       return CSSUnitType::CSS_LVI;
    case Dvw:       return CSSUnitType::CSS_DVW;
    case Dvh:       return CSSUnitType::CSS_DVH;
    case Dvmin:     return CSSUnitType::CSS_DVMIN;
    case Dvmax:     return CSSUnitType::CSS_DVMAX;
    case Dvb:       return CSSUnitType::CSS_DVB;
    case Dvi:       return CSSUnitType::CSS_DVI;
    case Cqw:       return CSSUnitType::CSS_CQW;
    case Cqh:       return CSSUnitType::CSS_CQH;
    case Cqi:       return CSSUnitType::CSS_CQI;
    case Cqb:       return CSSUnitType::CSS_CQB;
    case Cqmin:     return CSSUnitType::CSS_CQMIN;
    case Cqmax:     return CSSUnitType::CSS_CQMAX;
    }

    WTF_UNREACHABLE();
}

constexpr std::optional<LengthUnit> toLengthUnit(CSSUnitType cssUnit)
{
    using enum LengthUnit;

    switch (cssUnit) {
    case CSSUnitType::CSS_PX:        return Px;
    case CSSUnitType::CSS_CM:        return Cm;
    case CSSUnitType::CSS_MM:        return Mm;
    case CSSUnitType::CSS_Q:         return Q;
    case CSSUnitType::CSS_IN:        return In;
    case CSSUnitType::CSS_PT:        return Pt;
    case CSSUnitType::CSS_PC:        return Pc;
    case CSSUnitType::CSS_EM:        return Em;
    case CSSUnitType::CSS_QUIRKY_EM: return QuirkyEm;
    case CSSUnitType::CSS_EX:        return Ex;
    case CSSUnitType::CSS_LH:        return Lh;
    case CSSUnitType::CSS_CAP:       return Cap;
    case CSSUnitType::CSS_CH:        return Ch;
    case CSSUnitType::CSS_IC:        return Ic;
    case CSSUnitType::CSS_RCAP:      return Rcap;
    case CSSUnitType::CSS_RCH:       return Rch;
    case CSSUnitType::CSS_REM:       return Rem;
    case CSSUnitType::CSS_REX:       return Rex;
    case CSSUnitType::CSS_RIC:       return Ric;
    case CSSUnitType::CSS_RLH:       return Rlh;
    case CSSUnitType::CSS_VW:        return Vw;
    case CSSUnitType::CSS_VH:        return Vh;
    case CSSUnitType::CSS_VMIN:      return Vmin;
    case CSSUnitType::CSS_VMAX:      return Vmax;
    case CSSUnitType::CSS_VB:        return Vb;
    case CSSUnitType::CSS_VI:        return Vi;
    case CSSUnitType::CSS_SVW:       return Svw;
    case CSSUnitType::CSS_SVH:       return Svh;
    case CSSUnitType::CSS_SVMIN:     return Svmin;
    case CSSUnitType::CSS_SVMAX:     return Svmax;
    case CSSUnitType::CSS_SVB:       return Svb;
    case CSSUnitType::CSS_SVI:       return Svi;
    case CSSUnitType::CSS_LVW:       return Lvw;
    case CSSUnitType::CSS_LVH:       return Lvh;
    case CSSUnitType::CSS_LVMIN:     return Lvmin;
    case CSSUnitType::CSS_LVMAX:     return Lvmax;
    case CSSUnitType::CSS_LVB:       return Lvb;
    case CSSUnitType::CSS_LVI:       return Lvi;
    case CSSUnitType::CSS_DVW:       return Dvw;
    case CSSUnitType::CSS_DVH:       return Dvh;
    case CSSUnitType::CSS_DVMIN:     return Dvmin;
    case CSSUnitType::CSS_DVMAX:     return Dvmax;
    case CSSUnitType::CSS_DVB:       return Dvb;
    case CSSUnitType::CSS_DVI:       return Dvi;
    case CSSUnitType::CSS_CQW:       return Cqw;
    case CSSUnitType::CSS_CQH:       return Cqh;
    case CSSUnitType::CSS_CQI:       return Cqi;
    case CSSUnitType::CSS_CQB:       return Cqb;
    case CSSUnitType::CSS_CQMIN:     return Cqmin;
    case CSSUnitType::CSS_CQMAX:     return Cqmax;
    default: break;
    }

    return std::nullopt;
}

constexpr bool conversionToCanonicalUnitRequiresConversionData(LengthUnit unit)
{
    using enum LengthUnit;

    switch (unit) {
    case Px:
    case Cm:
    case Mm:
    case Q:
    case In:
    case Pt:
    case Pc:
        return false;
    case Em:
    case QuirkyEm:
    case Ex:
    case Lh:
    case Cap:
    case Ch:
    case Ic:
    case Rcap:
    case Rch:
    case Rem:
    case Rex:
    case Ric:
    case Rlh:
    case Vw:
    case Vh:
    case Vmin:
    case Vmax:
    case Vb:
    case Vi:
    case Svw:
    case Svh:
    case Svmin:
    case Svmax:
    case Svb:
    case Svi:
    case Lvw:
    case Lvh:
    case Lvmin:
    case Lvmax:
    case Lvb:
    case Lvi:
    case Dvw:
    case Dvh:
    case Dvmin:
    case Dvmax:
    case Dvb:
    case Dvi:
    case Cqw:
    case Cqh:
    case Cqi:
    case Cqb:
    case Cqmin:
    case Cqmax:
        return true;
    }

    WTF_UNREACHABLE();
}

constexpr bool isFontRelativeLength(LengthUnit lengthUnit)
{
    using enum LengthUnit;

    switch (lengthUnit) {
    case Em:
    case QuirkyEm:
    case Ex:
    case Lh:
    case Cap:
    case Ch:
    case Ic:
        return true;
    default:
        return false;
    }
}

constexpr bool isRootFontRelativeLength(LengthUnit lengthUnit)
{
    using enum LengthUnit;

    switch (lengthUnit) {
    case Rcap:
    case Rch:
    case Rem:
    case Rex:
    case Ric:
    case Rlh:
        return true;
    default:
        return false;
    }
}

constexpr bool isFontOrRootFontRelativeLength(LengthUnit lengthUnit)
{
    return isFontRelativeLength(lengthUnit)
        || isRootFontRelativeLength(lengthUnit);
}

constexpr bool isViewportPercentageLength(LengthUnit lengthUnit)
{
    using enum LengthUnit;

    switch (lengthUnit) {
    case Vw:
    case Vh:
    case Vmin:
    case Vmax:
    case Vb:
    case Vi:
    case Svw:
    case Svh:
    case Svmin:
    case Svmax:
    case Svb:
    case Svi:
    case Lvw:
    case Lvh:
    case Lvmin:
    case Lvmax:
    case Lvb:
    case Lvi:
    case Dvw:
    case Dvh:
    case Dvmin:
    case Dvmax:
    case Dvb:
    case Dvi:
        return true;
    default:
        return false;
    }
}

constexpr bool isContainerPercentageLength(LengthUnit lengthUnit)
{
    using enum LengthUnit;

    switch (lengthUnit) {
    case Cqw:
    case Cqh:
    case Cqi:
    case Cqb:
    case Cqmin:
    case Cqmax:
        return true;
    default:
        return false;
    }
}

ASCIILiteral unitString(LengthUnit);

template<> struct UnitTraits<LengthUnit> {
    static constexpr auto count = 50;
    static constexpr auto canonical = LengthUnit::Px;
    static constexpr auto category = Calculation::Category::Length;
    static consteval bool isValidRangeForCategory(Range) { return true; }

    static constexpr std::optional<LengthUnit> validate(CSSUnitType cssUnit) { return toLengthUnit(cssUnit); }
};
static_assert(UnitTraits<LengthUnit>::count == enumToUnderlyingType(LengthUnit::Cqmax) + 1);

CSS_DEFINE_UNIT_LITERAL(LengthUnit::Px, px)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Cm, cm)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Mm, mm)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Q, q)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::In, in)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Pt, pt)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Pc, pc)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Em, em)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::QuirkyEm, quirky_em)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Ex, ex)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Lh, lh)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Cap, cap)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Ch, ch)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Ic, ic)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Rcap, rcap)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Rch, rch)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Rem, rem)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Rex, rex)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Ric, ric)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Rlh, rlh)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Vw, vw)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Vh, vh)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Vmin, vmin)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Vmax, vmax)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Vb, vb)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Vi, vi)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Svw, svw)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Svh, svh)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Svmin, svmin)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Svmax, svmax)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Svb, svb)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Svi, svi)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Lvw, lvw)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Lvh, lvh)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Lvmin, lvmin)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Lvmax, lvmax)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Lvb, lvb)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Lvi, lvi)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Dvw, dvw)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Dvh, dvh)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Dvmin, dvmin)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Dvmax, dvmax)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Dvb, dvb)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Dvi, dvi)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Cqw, cqw)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Cqh, cqh)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Cqi, cqi)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Cqb, cqb)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Cqmin, cqmin)
CSS_DEFINE_UNIT_LITERAL(LengthUnit::Cqmax, cqmax)


// MARK: - <time>

enum class TimeUnit : uint8_t {
    S,
    Ms
};

constexpr CSSUnitType toCSSUnitType(TimeUnit timeUnit)
{
    using enum TimeUnit;

    switch (timeUnit) {
    case S:    return CSSUnitType::CSS_S;
    case Ms:   return CSSUnitType::CSS_MS;
    }

    WTF_UNREACHABLE();
}

constexpr std::optional<TimeUnit> toTimeUnit(CSSUnitType cssUnit)
{
    using enum TimeUnit;

    switch (cssUnit) {
    case CSSUnitType::CSS_S:    return S;
    case CSSUnitType::CSS_MS:   return Ms;
    default: break;
    }

    return std::nullopt;
}

constexpr bool conversionToCanonicalUnitRequiresConversionData(TimeUnit)
{
    return false;
}

template<TimeUnit To, typename T> constexpr T convertTime(T value, TimeUnit unit)
{
    using enum TimeUnit;

    if constexpr (To == S) {
        switch (unit) {
        case S:
            return value;
        case Ms:
            return value * secondsPerMillisecond;
        }

        WTF_UNREACHABLE();
    } else if constexpr (To == Ms) {
        switch (unit) {
        case S:
            return value / secondsPerMillisecond;
        case Ms:
            return value;
        }

        WTF_UNREACHABLE();
    }
}

ASCIILiteral unitString(TimeUnit);

template<> struct UnitTraits<TimeUnit> {
    static constexpr auto count = 2;
    static constexpr auto canonical = TimeUnit::S;
    static constexpr auto category = Calculation::Category::Time;
    static consteval bool isValidRangeForCategory(Range) { return true; }

    static constexpr std::optional<TimeUnit> validate(CSSUnitType cssUnit) { return toTimeUnit(cssUnit); }
    template<TimeUnit To, typename T> static constexpr T convert(T value, TimeUnit unit) { return convertTime<To, T>(value, unit); }
};
static_assert(UnitTraits<TimeUnit>::count == enumToUnderlyingType(TimeUnit::Ms) + 1);

CSS_DEFINE_UNIT_LITERAL(TimeUnit::S, s)
CSS_DEFINE_UNIT_LITERAL(TimeUnit::Ms, ms)

// MARK: - <frequency>

enum class FrequencyUnit : uint8_t {
    Hz,
    Khz
};

constexpr CSSUnitType toCSSUnitType(FrequencyUnit frequencyUnit)
{
    using enum FrequencyUnit;

    switch (frequencyUnit) {
    case Hz:    return CSSUnitType::CSS_HZ;
    case Khz:   return CSSUnitType::CSS_KHZ;
    }

    WTF_UNREACHABLE();
}

constexpr std::optional<FrequencyUnit> toFrequencyUnit(CSSUnitType cssUnit)
{
    using enum FrequencyUnit;

    switch (cssUnit) {
    case CSSUnitType::CSS_HZ:    return Hz;
    case CSSUnitType::CSS_KHZ:   return Khz;
    default: break;
    }

    return std::nullopt;
}

constexpr bool conversionToCanonicalUnitRequiresConversionData(FrequencyUnit)
{
    return false;
}

template<FrequencyUnit To, typename T> constexpr T convertFrequency(T value, FrequencyUnit unit)
{
    using enum FrequencyUnit;

    if constexpr (To == Hz) {
        switch (unit) {
        case Hz:
            return value;
        case Khz:
            return value * hertzPerKilohertz;
        }

        WTF_UNREACHABLE();
    } else if constexpr (To == Khz) {
        switch (unit) {
        case Hz:
            return value / hertzPerKilohertz;
        case Khz:
            return value;
        }

        WTF_UNREACHABLE();
    }
}

ASCIILiteral unitString(FrequencyUnit);

template<> struct UnitTraits<FrequencyUnit> {
    static constexpr auto count = 2;
    static constexpr auto canonical = FrequencyUnit::Hz;
    static constexpr auto category = Calculation::Category::Frequency;
    static consteval bool isValidRangeForCategory(Range) { return true; }

    static constexpr std::optional<FrequencyUnit> validate(CSSUnitType cssUnit) { return toFrequencyUnit(cssUnit); }
    template<FrequencyUnit To, typename T> static constexpr T convert(T value, FrequencyUnit unit) { return convertFrequency<To, T>(value, unit); }
};
static_assert(UnitTraits<FrequencyUnit>::count == enumToUnderlyingType(FrequencyUnit::Khz) + 1);

CSS_DEFINE_UNIT_LITERAL(FrequencyUnit::Hz, hz)
CSS_DEFINE_UNIT_LITERAL(FrequencyUnit::Khz, khz)

// MARK: - <resolution>

enum class ResolutionUnit : uint8_t {
    Dppx,
    X,
    Dpi,
    Dpcm
};

constexpr CSSUnitType toCSSUnitType(ResolutionUnit resolutionUnit)
{
    using enum ResolutionUnit;

    switch (resolutionUnit) {
    case Dppx:   return CSSUnitType::CSS_DPPX;
    case X:      return CSSUnitType::CSS_X;
    case Dpi:    return CSSUnitType::CSS_DPI;
    case Dpcm:   return CSSUnitType::CSS_DPCM;
    }

    WTF_UNREACHABLE();
}

constexpr std::optional<ResolutionUnit> toResolutionUnit(CSSUnitType cssUnit)
{
    using enum ResolutionUnit;

    switch (cssUnit) {
    case CSSUnitType::CSS_DPPX: return Dppx;
    case CSSUnitType::CSS_X:    return X;
    case CSSUnitType::CSS_DPI:  return Dpi;
    case CSSUnitType::CSS_DPCM: return Dpcm;
    default: break;
    }

    return std::nullopt;
}

constexpr bool conversionToCanonicalUnitRequiresConversionData(ResolutionUnit)
{
    return false;
}

template<ResolutionUnit To, typename T> constexpr T convertResolution(T value, ResolutionUnit unit)
{
    using enum ResolutionUnit;

    if constexpr (To == Dppx) {
        switch (unit) {
        case Dppx:
            return value;
        case X:
            return value * dppxPerX;
        case Dpi:
            return value * dppxPerDpi;
        case Dpcm:
            return value * dppxPerDpcm;
        }

        WTF_UNREACHABLE();
    } else if constexpr (To == X) {
        switch (unit) {
        case Dppx:
            return value / dppxPerX;
        case X:
            return value;
        case Dpi:
            return value * dppxPerDpi / dppxPerX;
        case Dpcm:
            return value * dppxPerDpcm / dppxPerX;
        }

        WTF_UNREACHABLE();
    } else if constexpr (To == Dpi) {
        switch (unit) {
        case Dppx:
            return value / dppxPerDpi;
        case X:
            return value * dppxPerX / dppxPerDpi;
        case Dpi:
            return value;
        case Dpcm:
            return value * dppxPerDpcm / dppxPerDpi;
        }

        WTF_UNREACHABLE();
    } else if constexpr (To == Dpcm) {
        switch (unit) {
        case Dppx:
            return value / dppxPerDpcm;
        case X:
            return value * dppxPerX / dppxPerDpcm;
        case Dpi:
            return value * dppxPerDpi / dppxPerDpcm;
        case Dpcm:
            return value;
        }

        WTF_UNREACHABLE();
    }
}

ASCIILiteral unitString(ResolutionUnit);

template<> struct UnitTraits<ResolutionUnit> {
    static constexpr auto count = 4;
    static constexpr auto canonical = ResolutionUnit::Dppx;
    static constexpr auto category = Calculation::Category::Resolution;
    static consteval bool isValidRangeForCategory(Range range) { return range.min >= 0; }

    static constexpr std::optional<ResolutionUnit> validate(CSSUnitType cssUnit) { return toResolutionUnit(cssUnit); }
    template<ResolutionUnit To, typename T> static constexpr T convert(T value, ResolutionUnit unit) { return convertResolution<To, T>(value, unit); }
};
static_assert(UnitTraits<ResolutionUnit>::count == enumToUnderlyingType(ResolutionUnit::Dpcm) + 1);

CSS_DEFINE_UNIT_LITERAL(ResolutionUnit::Dppx, dppx)
CSS_DEFINE_UNIT_LITERAL(ResolutionUnit::X, x)
CSS_DEFINE_UNIT_LITERAL(ResolutionUnit::Dpi, dpi)
CSS_DEFINE_UNIT_LITERAL(ResolutionUnit::Dpcm, dpcm)

// MARK: - <flex>

enum class FlexUnit : uint8_t {
    Fr
};

constexpr CSSUnitType toCSSUnitType(FlexUnit)
{
    return CSSUnitType::CSS_FR;
}

constexpr std::optional<FlexUnit> toFlexUnit(CSSUnitType cssUnit)
{
    if (cssUnit == CSSUnitType::CSS_FR)
        return FlexUnit::Fr;
    return std::nullopt;
}

constexpr bool conversionToCanonicalUnitRequiresConversionData(FlexUnit)
{
    return false;
}

constexpr ASCIILiteral unitString(FlexUnit)
{
    return "fr"_s;
}

template<> struct UnitTraits<FlexUnit> {
    static constexpr auto count = 1;
    static constexpr auto canonical = FlexUnit::Fr;
    static constexpr auto category = Calculation::Category::Flex;
    static consteval bool isValidRangeForCategory(Range) { return true; }

    static constexpr std::optional<FlexUnit> validate(CSSUnitType cssUnit) { return toFlexUnit(cssUnit); }
};
static_assert(UnitTraits<FlexUnit>::count == enumToUnderlyingType(FlexUnit::Fr) + 1);

CSS_DEFINE_UNIT_LITERAL(FlexUnit::Fr, fr)

// MARK: - <angle-percentage>

// NOTE: The value of the <angle> units in `AnglePercentageUnit` must match their counterpart in `AngleUnit`. This is statically asserted in CSSPrimitiveNumericsUnits.cpp, so if new <angle> units are added, please ensure the counter parts and assertions are updated as well.

enum class AnglePercentageUnit : uint8_t {
    Deg,
    Rad,
    Grad,
    Turn,
    Percentage
};

// Overload of `operator==` to allow comparing `AnglePercentageUnit` and `AngleUnit`.
constexpr bool operator==(AnglePercentageUnit a, AngleUnit b)
{
    return enumToUnderlyingType(a) == enumToUnderlyingType(b);
}

// Overload of `operator==` to allow comparing `AnglePercentageUnit` and `PercentageUnit`.
constexpr bool operator==(AnglePercentageUnit a, PercentageUnit)
{
    return a == AnglePercentageUnit::Percentage;
}

constexpr CSSUnitType toCSSUnitType(AnglePercentageUnit anglePercentageUnit)
{
    using enum AnglePercentageUnit;

    switch (anglePercentageUnit) {
    case Deg:           return CSSUnitType::CSS_DEG;
    case Rad:           return CSSUnitType::CSS_RAD;
    case Grad:          return CSSUnitType::CSS_GRAD;
    case Turn:          return CSSUnitType::CSS_TURN;
    case Percentage:    return CSSUnitType::CSS_PERCENTAGE;
    }

    WTF_UNREACHABLE();
}

constexpr std::optional<AnglePercentageUnit> toAnglePercentageUnit(CSSUnitType cssUnit)
{
    using enum AnglePercentageUnit;

    switch (cssUnit) {
    case CSSUnitType::CSS_DEG:          return Deg;
    case CSSUnitType::CSS_RAD:          return Rad;
    case CSSUnitType::CSS_GRAD:         return Grad;
    case CSSUnitType::CSS_TURN:         return Turn;
    case CSSUnitType::CSS_PERCENTAGE:   return Percentage;
    default: break;
    }

    return std::nullopt;
}

constexpr bool conversionToCanonicalUnitRequiresConversionData(AnglePercentageUnit)
{
    return false;
}

template<> struct UnitTraits<AnglePercentageUnit> {
    static constexpr auto count = UnitTraits<AngleUnit>::count + UnitTraits<PercentageUnit>::count;
    static constexpr auto canonical = AnglePercentageUnit::Deg;
    static constexpr auto category = Calculation::Category::AnglePercentage;
    static consteval bool isValidRangeForCategory(Range) { return true; }
    using Composite = brigand::set<AngleUnit, PercentageUnit>;

    static constexpr std::optional<AnglePercentageUnit> validate(CSSUnitType cssUnit)
    {
        return toAnglePercentageUnit(cssUnit);
    }

    template<UnitEnum E> static constexpr AnglePercentageUnit upcast(E unit)
    {
        if constexpr (std::same_as<E, AngleUnit>)
            return static_cast<AnglePercentageUnit>(unit);
        else if constexpr (std::same_as<E, PercentageUnit>)
            return AnglePercentageUnit::Percentage;
    }

    template<UnitEnum E> static constexpr bool is(AnglePercentageUnit unit)
    {
        if constexpr (std::same_as<E, AngleUnit>)
            return unit != AnglePercentageUnit::Percentage;
        else if constexpr (std::same_as<E, PercentageUnit>)
            return unit == AnglePercentageUnit::Percentage;
    }

    template<UnitEnum E> static constexpr E downcast(AnglePercentageUnit unit)
    {
        if constexpr (std::same_as<E, AngleUnit>)
            return static_cast<AngleUnit>(unit);
        else if constexpr (std::same_as<E, PercentageUnit>)
            return PercentageUnit::Percentage;
    }

    static constexpr decltype(auto) switchOnUnitType(AnglePercentageUnit unit, NOESCAPE auto&& f)
    {
        if (unit == AnglePercentageUnit::Percentage)
            return f(downcast<PercentageUnit>(unit));
        return f(downcast<AngleUnit>(unit));
    }
};
static_assert(UnitTraits<AnglePercentageUnit>::count == enumToUnderlyingType(AnglePercentageUnit::Percentage) + 1);

// MARK: - <length-percentage>

// NOTE: The value of the <length> units in `LengthPercentageUnit` must match their counterpart in `LengthUnit`. This is statically asserted in CSSPrimitiveNumericsUnits.cpp, so if new <length> units are added, please ensure the counter parts and assertions are updated as well.

enum class LengthPercentageUnit : uint8_t {
    Px,
    Cm,
    Mm,
    Q,
    In,
    Pt,
    Pc,
    Em,
    QuirkyEm,
    Ex,
    Lh,
    Cap,
    Ch,
    Ic,
    Rcap,
    Rch,
    Rem,
    Rex,
    Ric,
    Rlh,
    Vw,
    Vh,
    Vmin,
    Vmax,
    Vb,
    Vi,
    Svw,
    Svh,
    Svmin,
    Svmax,
    Svb,
    Svi,
    Lvw,
    Lvh,
    Lvmin,
    Lvmax,
    Lvb,
    Lvi,
    Dvw,
    Dvh,
    Dvmin,
    Dvmax,
    Dvb,
    Dvi,
    Cqw,
    Cqh,
    Cqi,
    Cqb,
    Cqmin,
    Cqmax,
    Percentage
};

// Overload of `operator==` to allow comparing `LengthPercentageUnit` and `LengthUnit`.
constexpr bool operator==(LengthPercentageUnit a, LengthUnit b)
{
    return enumToUnderlyingType(a) == enumToUnderlyingType(b);
}

// Overload of `operator==` to allow comparing `LengthPercentageUnit` and `PercentageUnit`.
constexpr bool operator==(LengthPercentageUnit a, PercentageUnit)
{
    return a == LengthPercentageUnit::Percentage;
}

constexpr CSSUnitType toCSSUnitType(LengthPercentageUnit lengthPercentageUnit)
{
    using enum LengthPercentageUnit;

    switch (lengthPercentageUnit) {
    case Px:            return CSSUnitType::CSS_PX;
    case Cm:            return CSSUnitType::CSS_CM;
    case Mm:            return CSSUnitType::CSS_MM;
    case Q:             return CSSUnitType::CSS_Q;
    case In:            return CSSUnitType::CSS_IN;
    case Pt:            return CSSUnitType::CSS_PT;
    case Pc:            return CSSUnitType::CSS_PC;
    case Em:            return CSSUnitType::CSS_EM;
    case QuirkyEm:      return CSSUnitType::CSS_QUIRKY_EM;
    case Ex:            return CSSUnitType::CSS_EX;
    case Lh:            return CSSUnitType::CSS_LH;
    case Cap:           return CSSUnitType::CSS_CAP;
    case Ch:            return CSSUnitType::CSS_CH;
    case Ic:            return CSSUnitType::CSS_IC;
    case Rcap:          return CSSUnitType::CSS_RCAP;
    case Rch:           return CSSUnitType::CSS_RCH;
    case Rem:           return CSSUnitType::CSS_REM;
    case Rex:           return CSSUnitType::CSS_REX;
    case Ric:           return CSSUnitType::CSS_RIC;
    case Rlh:           return CSSUnitType::CSS_RLH;
    case Vw:            return CSSUnitType::CSS_VW;
    case Vh:            return CSSUnitType::CSS_VH;
    case Vmin:          return CSSUnitType::CSS_VMIN;
    case Vmax:          return CSSUnitType::CSS_VMAX;
    case Vb:            return CSSUnitType::CSS_VB;
    case Vi:            return CSSUnitType::CSS_VI;
    case Svw:           return CSSUnitType::CSS_SVW;
    case Svh:           return CSSUnitType::CSS_SVH;
    case Svmin:         return CSSUnitType::CSS_SVMIN;
    case Svmax:         return CSSUnitType::CSS_SVMAX;
    case Svb:           return CSSUnitType::CSS_SVB;
    case Svi:           return CSSUnitType::CSS_SVI;
    case Lvw:           return CSSUnitType::CSS_LVW;
    case Lvh:           return CSSUnitType::CSS_LVH;
    case Lvmin:         return CSSUnitType::CSS_LVMIN;
    case Lvmax:         return CSSUnitType::CSS_LVMAX;
    case Lvb:           return CSSUnitType::CSS_LVB;
    case Lvi:           return CSSUnitType::CSS_LVI;
    case Dvw:           return CSSUnitType::CSS_DVW;
    case Dvh:           return CSSUnitType::CSS_DVH;
    case Dvmin:         return CSSUnitType::CSS_DVMIN;
    case Dvmax:         return CSSUnitType::CSS_DVMAX;
    case Dvb:           return CSSUnitType::CSS_DVB;
    case Dvi:           return CSSUnitType::CSS_DVI;
    case Cqw:           return CSSUnitType::CSS_CQW;
    case Cqh:           return CSSUnitType::CSS_CQH;
    case Cqi:           return CSSUnitType::CSS_CQI;
    case Cqb:           return CSSUnitType::CSS_CQB;
    case Cqmin:         return CSSUnitType::CSS_CQMIN;
    case Cqmax:         return CSSUnitType::CSS_CQMAX;
    case Percentage:    return CSSUnitType::CSS_PERCENTAGE;
    }

    WTF_UNREACHABLE();
}

constexpr std::optional<LengthPercentageUnit> toLengthPercentageUnit(CSSUnitType cssUnit)
{
    using enum LengthPercentageUnit;

    switch (cssUnit) {
    case CSSUnitType::CSS_PX:           return Px;
    case CSSUnitType::CSS_CM:           return Cm;
    case CSSUnitType::CSS_MM:           return Mm;
    case CSSUnitType::CSS_Q:            return Q;
    case CSSUnitType::CSS_IN:           return In;
    case CSSUnitType::CSS_PT:           return Pt;
    case CSSUnitType::CSS_PC:           return Pc;
    case CSSUnitType::CSS_EM:           return Em;
    case CSSUnitType::CSS_QUIRKY_EM:    return QuirkyEm;
    case CSSUnitType::CSS_EX:           return Ex;
    case CSSUnitType::CSS_LH:           return Lh;
    case CSSUnitType::CSS_CAP:          return Cap;
    case CSSUnitType::CSS_CH:           return Ch;
    case CSSUnitType::CSS_IC:           return Ic;
    case CSSUnitType::CSS_RCAP:         return Rcap;
    case CSSUnitType::CSS_RCH:          return Rch;
    case CSSUnitType::CSS_REM:          return Rem;
    case CSSUnitType::CSS_REX:          return Rex;
    case CSSUnitType::CSS_RIC:          return Ric;
    case CSSUnitType::CSS_RLH:          return Rlh;
    case CSSUnitType::CSS_VW:           return Vw;
    case CSSUnitType::CSS_VH:           return Vh;
    case CSSUnitType::CSS_VMIN:         return Vmin;
    case CSSUnitType::CSS_VMAX:         return Vmax;
    case CSSUnitType::CSS_VB:           return Vb;
    case CSSUnitType::CSS_VI:           return Vi;
    case CSSUnitType::CSS_SVW:          return Svw;
    case CSSUnitType::CSS_SVH:          return Svh;
    case CSSUnitType::CSS_SVMIN:        return Svmin;
    case CSSUnitType::CSS_SVMAX:        return Svmax;
    case CSSUnitType::CSS_SVB:          return Svb;
    case CSSUnitType::CSS_SVI:          return Svi;
    case CSSUnitType::CSS_LVW:          return Lvw;
    case CSSUnitType::CSS_LVH:          return Lvh;
    case CSSUnitType::CSS_LVMIN:        return Lvmin;
    case CSSUnitType::CSS_LVMAX:        return Lvmax;
    case CSSUnitType::CSS_LVB:          return Lvb;
    case CSSUnitType::CSS_LVI:          return Lvi;
    case CSSUnitType::CSS_DVW:          return Dvw;
    case CSSUnitType::CSS_DVH:          return Dvh;
    case CSSUnitType::CSS_DVMIN:        return Dvmin;
    case CSSUnitType::CSS_DVMAX:        return Dvmax;
    case CSSUnitType::CSS_DVB:          return Dvb;
    case CSSUnitType::CSS_DVI:          return Dvi;
    case CSSUnitType::CSS_CQW:          return Cqw;
    case CSSUnitType::CSS_CQH:          return Cqh;
    case CSSUnitType::CSS_CQI:          return Cqi;
    case CSSUnitType::CSS_CQB:          return Cqb;
    case CSSUnitType::CSS_CQMIN:        return Cqmin;
    case CSSUnitType::CSS_CQMAX:        return Cqmax;
    case CSSUnitType::CSS_PERCENTAGE:   return Percentage;
    default: break;
    }

    return std::nullopt;
}

constexpr bool conversionToCanonicalUnitRequiresConversionData(LengthPercentageUnit unit)
{
    using enum LengthPercentageUnit;

    switch (unit) {
    case Px:
    case Cm:
    case Mm:
    case Q:
    case In:
    case Pt:
    case Pc:
    case Percentage:
        return false;
    case Em:
    case QuirkyEm:
    case Ex:
    case Lh:
    case Cap:
    case Ch:
    case Ic:
    case Rcap:
    case Rch:
    case Rem:
    case Rex:
    case Ric:
    case Rlh:
    case Vw:
    case Vh:
    case Vmin:
    case Vmax:
    case Vb:
    case Vi:
    case Svw:
    case Svh:
    case Svmin:
    case Svmax:
    case Svb:
    case Svi:
    case Lvw:
    case Lvh:
    case Lvmin:
    case Lvmax:
    case Lvb:
    case Lvi:
    case Dvw:
    case Dvh:
    case Dvmin:
    case Dvmax:
    case Dvb:
    case Dvi:
    case Cqw:
    case Cqh:
    case Cqi:
    case Cqb:
    case Cqmin:
    case Cqmax:
        return true;
    }

    WTF_UNREACHABLE();
}

template<> struct UnitTraits<LengthPercentageUnit> {
    static constexpr auto count = UnitTraits<LengthUnit>::count + UnitTraits<PercentageUnit>::count;
    static constexpr auto canonical = LengthPercentageUnit::Px;
    static constexpr auto category = Calculation::Category::LengthPercentage;
    static consteval bool isValidRangeForCategory(Range) { return true; }
    using Composite = brigand::set<LengthUnit, PercentageUnit>;

    static constexpr std::optional<LengthPercentageUnit> validate(CSSUnitType cssUnit)
    {
        return toLengthPercentageUnit(cssUnit);
    }

    template<UnitEnum E> static constexpr LengthPercentageUnit upcast(E unit)
    {
        if constexpr (std::same_as<E, LengthUnit>)
            return static_cast<LengthPercentageUnit>(unit);
        else if constexpr (std::same_as<E, PercentageUnit>)
            return LengthPercentageUnit::Percentage;
    }

    template<UnitEnum E> static constexpr bool is(LengthPercentageUnit unit)
    {
        if constexpr (std::same_as<E, LengthUnit>)
            return unit != LengthPercentageUnit::Percentage;
        else if constexpr (std::same_as<E, PercentageUnit>)
            return unit == LengthPercentageUnit::Percentage;
    }

    template<UnitEnum E> static constexpr E downcast(LengthPercentageUnit unit)
    {
        if constexpr (std::same_as<E, LengthUnit>)
            return static_cast<LengthUnit>(unit);
        else if constexpr (std::same_as<E, PercentageUnit>)
            return PercentageUnit::Percentage;
    }

    static constexpr decltype(auto) switchOnUnitType(LengthPercentageUnit unit, NOESCAPE auto&& f)
    {
        if (unit == LengthPercentageUnit::Percentage)
            return f(downcast<PercentageUnit>(unit));
        return f(downcast<LengthUnit>(unit));
    }
};
static_assert(UnitTraits<LengthPercentageUnit>::count == enumToUnderlyingType(LengthPercentageUnit::Percentage) + 1);

constexpr ASCIILiteral unitString(CompositeUnitEnum auto unit)
{
    return switchOnUnitType(unit, [](auto alternative) { return unitString(alternative); });
}

// MARK: - Generic Unit Conversion within a type

template<auto unit, typename T> constexpr decltype(auto) convertToValueInUnitsOf(T value)
    requires std::same_as<typename T::UnitType, decltype(unit)>
{
    return UnitTraits<typename T::UnitType>::template convert<unit>(value.value, value.unit);
}

} // namespace CSS
} // namespace WebCore
