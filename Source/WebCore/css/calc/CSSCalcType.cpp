/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "CSSCalcType.h"

#include "CSSUnits.h"
#include "CalculationCategory.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace CSSCalc {

std::optional<Type> Type::add(Type type1, Type type2)
{
    // https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-add-two-types

    // To add two types type1 and type2, perform the following steps:

    // 1.
    //
    // Replace type1 with a fresh copy of type1, and type2 with a fresh copy of type2. Let finalType be a new type with an initially empty ordered map and an initially null percent hint.

    auto finalType = Type { };

    // 2.
    //
    if (type1.percentHint && type2.percentHint) {
        // If both type1 and type2 have non-null percent hints with different values. The types can’t be added. Return failure.
        if (*type1.percentHint != *type2.percentHint)
            return std::nullopt;
    } else if (type1.percentHint && !type2.percentHint) {
        // If type1 has a non-null percent hint hint and type2 doesn’t, apply the percent hint hint to type2.
        type2.percentHint = *type1.percentHint;
    } else if (!type1.percentHint && type2.percentHint) {
        // Vice versa if type2 has a non-null percent hint and type1 doesn’t.
        type1.percentHint = *type2.percentHint;
    }

    // 3.
    //
    // - If all the entries of type1 with non-zero values are contained in type2 with the same value, and vice-versa
    if (type1.allNonZeroValuesEqual(type2) && type2.allNonZeroValuesEqual(type1)) {
        // Copy all of type1’s entries to finalType, and then copy all of type2’s entries to finalType that finalType doesn’t already contain. Set finalType’s percent hint to type1’s percent hint. Return finalType.
        finalType = type1;
        for (auto baseType : Type::allBaseTypes()) {
            if (type2[baseType] && !finalType[baseType])
                finalType[baseType] = type2[baseType];
        }
        return finalType;
    }

    // - If type1 and/or type2 contain "percent" with a non-zero value, and type1 and/or type2 contain a key other than "percent" with a non-zero value
    if ((type1[BaseType::Percent] || type2[BaseType::Percent]) && (type1.hasNonPercentEntry() || type2.hasNonPercentEntry())) {
        // For each base type other than "percent" hint:
        for (auto hint : Type::allPotentialPercentHintTypes()) {
            auto type1Copy = type1;
            auto type2Copy = type2;

            // 1. Provisionally apply the percent hint `hint` to both type1 and type2.
            type1.applyPercentHint(hint);
            type2.applyPercentHint(hint);

            // 2. If, afterwards, all the entries of type1 with non-zero values are contained in type2 with the same value, and vice versa, then copy all of type1’s entries to finalType, and then copy all of type2’s entries to finalType that finalType doesn’t already contain. Set finalType’s percent hint to hint. Return finalType.
            if (type1.allNonZeroValuesEqual(type2) && type2.allNonZeroValuesEqual(type1)) {
                finalType = type1;
                for (auto baseType : Type::allBaseTypes()) {
                    if (type2[baseType] && !finalType[baseType])
                        finalType[baseType] = type2[baseType];
                }
                finalType.percentHint = hint;
                return finalType;
            }

            // 3. Otherwise, revert type1 and type2 to their state at the start of this loop.
            type1 = type1Copy;
            type2 = type2Copy;
        }

        // If the loop finishes without returning finalType, then the types can’t be added. Return failure.
        return std::nullopt;
    }

    // - The types can’t be added. Return failure.
    return std::nullopt;
}

// https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-multiply-two-types
std::optional<Type> Type::multiply(Type type1, Type type2)
{
    // To multiply two types type1 and type2, perform the following steps:

    // 1.
    //
    // Replace type1 with a fresh copy of type1, and type2 with a fresh copy of type2. Let finalType be a new type with an initially empty ordered map and an initially null percent hint.

    auto finalType = Type { };

    // 2/3.
    //
    // If both type1 and type2 have non-null percent hints with different values, the types can’t be multiplied. Return failure.
    if (type1.percentHint && type2.percentHint) {
        if (*type1.percentHint != *type2.percentHint) {
            // The types can’t be multiplied. Return failure.
            return std::nullopt;
        }
    } else if (type1.percentHint && !type2.percentHint) {
        // If type1 has a non-null percent hint hint and type2 doesn’t, apply the percent hint hint to type2.
        type2.percentHint = *type1.percentHint;
    } else if (!type1.percentHint && type2.percentHint) {
        // Vice versa if type2 has a non-null percent hint and type1 doesn’t.
        type1.percentHint = *type2.percentHint;
    }

    // 4. Copy all of type1’s entries to finalType, then for each `baseType` → `power` of type2:
    for (auto baseType : Type::allBaseTypes()) {
        // If finalType[baseType] exists, increment its value by power. Otherwise, set finalType[baseType] to power.

        // NOTE: This is amended in our implementation to return `failure` if exponent would overflow.
        auto checkedExponent = checkedSum<int8_t>(type1[baseType], type2[baseType]);
        if (checkedExponent.hasOverflowed())
            return std::nullopt;

        finalType[baseType] = checkedExponent.value();
    }
    // Set finalType’s percent hint to type1’s percent hint.
    finalType.percentHint = type1.percentHint;

    // 5. Return finalType.
    return finalType;
}

// https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-invert-a-type
Type Type::invert(Type type)
{
    // To invert a type `type`, perform the following steps:

    // 1. Let result be a new type with an initially empty ordered map and a percent hint matching that of type.
    auto result = Type { };
    result.percentHint = type.percentHint;

    // 2. For each unit → exponent of type, set result[unit] to (-1 * exponent).
    for (auto unit : Type::allBaseTypes())
        result[unit] = -1 * type[unit];

    // 3. Return result.
    return result;
}

std::optional<Type> Type::sameType(Type a, Type b)
{
    if (a != b)
        return std::nullopt;
    return a;
}

// https://drafts.csswg.org/css-values-4/#css-consistent-type
std::optional<Type> Type::consistentType(Type a, Type b)
{
    // Two or more calculations have a consistent type if adding the types doesn’t result in failure. The consistent type is the result of the type addition.
    return add(a, b);
}

// https://drafts.csswg.org/css-values-4/#css-make-a-type-consistent
std::optional<Type> Type::madeConsistent(Type base, Type input)
{
    // 1. If both base and input have different non-null percent hints, they can’t be made consistent. Return failure.
    if (base.percentHint && input.percentHint && *base.percentHint != *input.percentHint)
        return std::nullopt;

    // 2. If base has a null percent hint set base’s percent hint to input’s percent hint.
    if (!base.percentHint)
        base.percentHint = input.percentHint;

    // 3. Return base.
    return base;
}

// Part of https://drafts.csswg.org/css-values-4/#determine-the-type-of-a-calculation
Type Type::determineType(CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
        // the type is «[ ]» (empty map)
        return Type { };

    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        // the type is «[ "length" → 1 ]»
        return Type { .length = 1 };

    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
        // the type is «[ "angle" → 1 ]»
        return Type { .angle = 1 };

    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
        // the type is «[ "time" → 1 ]»
        return Type { .time = 1 };

    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
        // the type is «[ "frequency" → 1 ]»
        return Type { .frequency = 1 };

    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
        // the type is «[ "resolution" → 1 ]»
        return Type { .resolution = 1 };

    case CSSUnitType::CSS_FR:
        // the type is «[ "flex" → 1 ]»
        return Type { .flex = 1 };

    case CSSUnitType::CSS_PERCENTAGE:
        // NOTE: This is the context-independent type. When context is available, this may be transformed into a percent-hint.

        // the type is «[ "percent" → 1 ]».
        return Type { .percent = 1 };

    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_VALUE_ID:
    case CSSUnitType::CustomIdent:
        break;
    }

    ASSERT_NOT_REACHED();
    return { };
}

Type::PercentHintValue Type::determinePercentHint(Calculation::Category category)
{
    switch (category) {
    case Calculation::Category::Integer:
    case Calculation::Category::Number:
    case Calculation::Category::Percentage:
    case Calculation::Category::Length:
    case Calculation::Category::Angle:
    case Calculation::Category::Time:
    case Calculation::Category::Frequency:
    case Calculation::Category::Resolution:
    case Calculation::Category::Flex:
        return { };

    case Calculation::Category::LengthPercentage:
        return PercentHint::Length;
    case Calculation::Category::AnglePercentage:
        return PercentHint::Angle;
    }

    ASSERT_NOT_REACHED();
    return { };
}

bool Type::matches(Calculation::Category category) const
{
    switch (category) {
    case Calculation::Category::Integer:
    case Calculation::Category::Number:
        return matchesAny<Match::Number>();
    case Calculation::Category::Percentage:
        return matchesAny<Match::Percent>();
    case Calculation::Category::Length:
        return matchesAny<Match::Length>();
    case Calculation::Category::Angle:
        return matchesAny<Match::Angle>();
    case Calculation::Category::Time:
        return matchesAny<Match::Time>();
    case Calculation::Category::Frequency:
        return matchesAny<Match::Frequency>();
    case Calculation::Category::Resolution:
        return matchesAny<Match::Resolution>();
    case Calculation::Category::Flex:
        return matchesAny<Match::Flex>();
    case Calculation::Category::LengthPercentage:
        return matchesAny<Match::Length, Match::Percent>({ .allowsPercentHint = true });
    case Calculation::Category::AnglePercentage:
        return matchesAny<Match::Angle, Match::Percent>({ .allowsPercentHint = true });
    }

    ASSERT_NOT_REACHED();
    return false;
}

std::optional<Calculation::Category> Type::calculationCategory() const
{
    std::optional<BaseType> matchingUnit;
    for (auto unit : Type::allBaseTypes()) {
        if ((*this)[unit] == 0)
            continue;
        if ((*this)[unit] == 1) {
            if (matchingUnit) {
                // If `matchingUnit` has already been set, that means this type has a mixed base.
                return std::nullopt;
            }
            matchingUnit = unit;
            continue;
        }

        // If `unit` is not 0 or 1, that means this type has a base type of a higher or negative power, which no calculation categories match.
        return std::nullopt;
    }

    if (!matchingUnit)
        return Calculation::Category::Number;

    switch (*matchingUnit) {
    case BaseType::Length:
        if (percentHint)
            return Calculation::Category::LengthPercentage;
        return Calculation::Category::Length;
    case BaseType::Angle:
        if (percentHint)
            return Calculation::Category::AnglePercentage;
        return Calculation::Category::Angle;
    case BaseType::Time:
        ASSERT(!percentHint);
        return Calculation::Category::Time;
    case BaseType::Frequency:
        ASSERT(!percentHint);
        return Calculation::Category::Frequency;
    case BaseType::Resolution:
        ASSERT(!percentHint);
        return Calculation::Category::Resolution;
    case BaseType::Flex:
        ASSERT(!percentHint);
        return Calculation::Category::Flex;
    case BaseType::Percent:
        ASSERT(!percentHint);
        return Calculation::Category::Percentage;
    }

    ASSERT_NOT_REACHED();
    return Calculation::Category::Number;
}

static ASCIILiteral literal(BaseType baseType)
{
    switch (baseType) {
    case BaseType::Length: return "length"_s;
    case BaseType::Angle: return "angle"_s;
    case BaseType::Time: return "time"_s;
    case BaseType::Frequency: return "trequency"_s;
    case BaseType::Resolution: return "resolution"_s;
    case BaseType::Flex: return "flex"_s;
    case BaseType::Percent: return "percent"_s;
    }

    ASSERT_NOT_REACHED();
    return ""_s;
}

static ASCIILiteral literal(PercentHint percentHint)
{
    switch (percentHint) {
    case PercentHint::Length: return "length"_s;
    case PercentHint::Angle: return "angle"_s;
    case PercentHint::Time: return "time"_s;
    case PercentHint::Frequency: return "frequency"_s;
    case PercentHint::Resolution: return "resolution"_s;
    case PercentHint::Flex: return "flex"_s;
    }

    ASSERT_NOT_REACHED();
    return ""_s;
}

static ASCIILiteral literal(Type::PercentHintValue value)
{
    if (!value)
        return "(unset)"_s;
    return literal(*value);
}

TextStream& operator<<(TextStream& ts, BaseType baseType)
{
    return ts << literal(baseType);
}

TextStream& operator<<(TextStream& ts, PercentHint percentHint)
{
    return ts << literal(percentHint);
}

TextStream& operator<<(TextStream& ts, Type::PercentHintValue percentHint)
{
    return ts << literal(percentHint);
}

TextStream& operator<<(TextStream& ts, Type type)
{
    return ts << "CSSCalc::Type [ px=" << type.length << " deg=" << type.angle << " s=" << type.time << " hz=" << type.frequency << " dppx=" << type.resolution << " fr=" << type.flex << " %=" << type.percent << " hint=" << type.percentHint << " ]";
}

} // namespace CSSCalc
} // namespace WebCore
