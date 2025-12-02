/*
 *  Copyright (C) 2021 Igalia, S.L. All rights reserved.
 *  Copyright (C) 2021 Sony Interactive Entertainment Inc.
 *  Copyright (C) 2021-2022 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include "JSObject.h"
#include <wtf/Int128.h>

namespace JSC {

#define JSC_TEMPORAL_PLAIN_DATE_UNITS(macro) \
    macro(year, Year) \
    macro(month, Month) \
    macro(day, Day) \

#define JSC_TEMPORAL_PLAIN_MONTH_DAY_UNITS(macro) \
    macro(month, Month) \
    macro(day, Day)

#define JSC_TEMPORAL_PLAIN_YEAR_MONTH_UNITS(macro) \
    macro(year, Year) \
    macro(month, Month)

#define JSC_TEMPORAL_PLAIN_TIME_UNITS(macro) \
    macro(hour, Hour) \
    macro(minute, Minute) \
    macro(second, Second) \
    macro(millisecond, Millisecond) \
    macro(microsecond, Microsecond) \
    macro(nanosecond, Nanosecond) \


#define JSC_TEMPORAL_UNITS(macro) \
    macro(year, Year) \
    macro(month, Month) \
    macro(week, Week) \
    macro(day, Day) \
    JSC_TEMPORAL_PLAIN_TIME_UNITS(macro) \


enum class TemporalUnit : uint8_t {
#define JSC_DEFINE_TEMPORAL_UNIT_ENUM(name, capitalizedName) capitalizedName,
    JSC_TEMPORAL_UNITS(JSC_DEFINE_TEMPORAL_UNIT_ENUM)
#undef JSC_DEFINE_TEMPORAL_UNIT_ENUM
};
#define JSC_COUNT_TEMPORAL_UNITS(name, capitalizedName) + 1
static constexpr unsigned numberOfTemporalUnits = 0 JSC_TEMPORAL_UNITS(JSC_COUNT_TEMPORAL_UNITS);
static constexpr unsigned numberOfTemporalPlainDateUnits = 0 JSC_TEMPORAL_PLAIN_DATE_UNITS(JSC_COUNT_TEMPORAL_UNITS);
static constexpr unsigned numberOfTemporalPlainTimeUnits = 0 JSC_TEMPORAL_PLAIN_TIME_UNITS(JSC_COUNT_TEMPORAL_UNITS);
static constexpr unsigned numberOfTemporalPlainYearMonthUnits = 0 JSC_TEMPORAL_PLAIN_YEAR_MONTH_UNITS(JSC_COUNT_TEMPORAL_UNITS);
static constexpr unsigned numberOfTemporalPlainMonthDayUnits = 0 JSC_TEMPORAL_PLAIN_MONTH_DAY_UNITS(JSC_COUNT_TEMPORAL_UNITS);
#undef JSC_COUNT_TEMPORAL_UNITS

extern const TemporalUnit temporalUnitsInTableOrder[numberOfTemporalUnits];

class TemporalObject final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    template<typename CellType, SubspaceAccess>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(TemporalObject, Base);
        return &vm.plainObjectSpace();
    }

    static TemporalObject* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*);

    DECLARE_INFO;

private:
    TemporalObject(VM&, Structure*);
    void finishCreation(VM&);
};

enum class RoundingMode : uint8_t {
    Ceil,
    Floor,
    Expand,
    Trunc,
    HalfCeil,
    HalfFloor,
    HalfExpand,
    HalfTrunc,
    HalfEven
};

enum class UnsignedRoundingMode : uint8_t {
    Infinity,
    Zero,
    HalfInfinity,
    HalfZero,
    HalfEven
};

enum class Precision : uint8_t {
    Minute,
    Fixed,
    Auto,
};

struct PrecisionData {
    std::tuple<Precision, unsigned> precision;
    TemporalUnit unit;
    unsigned increment;
};

enum class UnitGroup : uint8_t {
    DateTime,
    Date,
    Time,
};

enum class Inclusivity : bool {
    Inclusive,
    Exclusive
};

enum class DifferenceOperation : bool {
    Since,
    Until
};

enum class AddOrSubtract : bool {
    Add,
    Subtract
};

struct ParsedMonthCode {
    uint8_t monthNumber;
    bool isLeapMonth;
};

WTF::String ellipsizeAt(unsigned maxLength, const WTF::String&);
PropertyName temporalUnitPluralPropertyName(VM&, TemporalUnit);
PropertyName temporalUnitSingularPropertyName(VM&, TemporalUnit);
std::optional<TemporalUnit> temporalUnitType(StringView);
std::optional<TemporalUnit> temporalLargestUnit(JSGlobalObject*, JSObject* options, std::initializer_list<TemporalUnit> disallowedUnits, TemporalUnit autoValue);
std::optional<TemporalUnit> temporalSmallestUnit(JSGlobalObject*, JSObject* options, std::initializer_list<TemporalUnit> disallowedUnits);
std::tuple<TemporalUnit, TemporalUnit, RoundingMode, double> extractDifferenceOptions(JSGlobalObject*, JSValue, UnitGroup, TemporalUnit defaultSmallestUnit, TemporalUnit defaultLargestUnit);
std::optional<unsigned> temporalFractionalSecondDigits(JSGlobalObject*, JSObject* options);
PrecisionData secondsStringPrecision(JSGlobalObject*, JSObject* options);
RoundingMode temporalRoundingMode(JSGlobalObject*, JSObject*, RoundingMode);
RoundingMode negateTemporalRoundingMode(RoundingMode);
void formatSecondsStringFraction(StringBuilder&, unsigned fraction, std::tuple<Precision, unsigned>);
void formatSecondsStringPart(StringBuilder&, unsigned second, unsigned fraction, PrecisionData);
std::optional<double> maximumRoundingIncrement(TemporalUnit);
double temporalRoundingIncrement(JSGlobalObject*, JSObject* options, std::optional<double> dividend, bool inclusive);
double roundNumberToIncrement(double, double increment, RoundingMode);
double roundNumberToIncrementDouble(double, double increment, RoundingMode);
Int128 roundNumberToIncrementInt128(Int128, Int128, RoundingMode);
Int128 roundNumberToIncrementAsIfPositive(Int128, Int128, RoundingMode);
double applyUnsignedRoundingMode(double, double, double, UnsignedRoundingMode);
void rejectObjectWithCalendarOrTimeZone(JSGlobalObject*, JSObject*);

constexpr Int128 lengthInNanoseconds(TemporalUnit unit)
{
    switch (unit) {
    case TemporalUnit::Nanosecond:
        return 1;
    case TemporalUnit::Microsecond:
        return 1000;
    case TemporalUnit::Millisecond:
        return 1000 * lengthInNanoseconds(TemporalUnit::Microsecond);
    case TemporalUnit::Second:
        return 1000 * lengthInNanoseconds(TemporalUnit::Millisecond);
    case TemporalUnit::Minute:
        return 60 * lengthInNanoseconds(TemporalUnit::Second);
    case TemporalUnit::Hour:
        return 60 * lengthInNanoseconds(TemporalUnit::Minute);
    case TemporalUnit::Day:
        return 24 * lengthInNanoseconds(TemporalUnit::Hour);
    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// https://tc39.es/proposal-temporal/#sec-getunsignedroundingmode
constexpr UnsignedRoundingMode getUnsignedRoundingMode(RoundingMode roundingMode, bool isNegative)
{
    switch (roundingMode) {
    case RoundingMode::Ceil:
        return isNegative ? UnsignedRoundingMode::Zero : UnsignedRoundingMode::Infinity;
    case RoundingMode::Floor:
        return isNegative ? UnsignedRoundingMode::Infinity : UnsignedRoundingMode::Zero;
    case RoundingMode::Expand:
        return UnsignedRoundingMode::Infinity;
    case RoundingMode::Trunc:
        return UnsignedRoundingMode::Zero;
    case RoundingMode::HalfCeil:
        return isNegative ? UnsignedRoundingMode::HalfZero : UnsignedRoundingMode::HalfInfinity;
    case RoundingMode::HalfFloor:
        return isNegative ? UnsignedRoundingMode::HalfInfinity : UnsignedRoundingMode::HalfZero;
    case RoundingMode::HalfExpand:
        return UnsignedRoundingMode::HalfInfinity;
    case RoundingMode::HalfTrunc:
        return UnsignedRoundingMode::HalfZero;
    default:
        return UnsignedRoundingMode::HalfEven;
    }
}

enum class TemporalOverflow : bool {
    Constrain,
    Reject,
};

TemporalOverflow toTemporalOverflow(JSGlobalObject*, JSObject*);
TemporalOverflow toTemporalOverflow(JSGlobalObject*, JSValue);
String toTemporalCalendarName(JSGlobalObject*, JSObject*);

enum class TemporalDisambiguation : uint8_t {
    Compatible,
    Earlier,
    Later,
    Reject,
};

enum class TemporalDateFormat : uint8_t {
    Date,
    YearMonth,
    MonthDay
};

enum class TemporalAnyProperties : bool {
    None,
    Some,
};

} // namespace JSC
