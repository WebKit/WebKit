/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

// JSC Temporal Core — Temporal enum types
// temporal_rs reference: src/options.rs

#include <cstdint>
#include <wtf/Int128.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringView.h>

namespace JSC {

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporaldisambiguationoption
// temporal_rs: Disambiguation
enum class TemporalDisambiguation : uint8_t {
    Compatible,
    Earlier,
    Later,
    Reject,
};

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporaloffsetoption
// temporal_rs: OffsetDisambiguation
enum class TemporalOffsetDisambiguation : uint8_t {
    Use,
    Prefer,
    Ignore,
    Reject,
};

// OffsetBehaviour encodes the offset source used by interpretISODateTimeOffset.
enum class OffsetBehaviour : uint8_t {
    Wall, // no inline offset in string — resolve via timezone + disambiguation
    Exact, // Z flag — UTC epoch directly
    Option, // +HH:MM present — use inlineOffsetNs
};

// -----------------------------------------------------------------------
// Temporal unit
// -----------------------------------------------------------------------

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

// temporal_rs: Unit (src/options.rs)
// https://tc39.es/proposal-temporal/#table-temporal-units
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

// https://tc39.es/proposal-temporal/#table-temporal-units
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

// https://tc39.es/proposal-temporal/#sec-temporal-iscalendarunit
constexpr bool isCalendarUnit(TemporalUnit unit) { return unit <= TemporalUnit::Week; }

// -----------------------------------------------------------------------
// Rounding enums
// -----------------------------------------------------------------------

// temporal_rs: RoundingMode (src/options.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalroundingmode
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

// temporal_rs: UnsignedRoundingMode (src/options.rs)
enum class UnsignedRoundingMode : uint8_t {
    Infinity,
    Zero,
    HalfInfinity,
    HalfZero,
    HalfEven
};

// https://tc39.es/proposal-temporal/#sec-getunsignedroundingmode
// temporal_rs: RoundingMode::get_unsigned_round_mode (src/options.rs)
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

// temporal_rs: no direct equivalent; used as parameter to ValidateTemporalRoundingIncrement.
enum class Inclusivity : bool {
    Inclusive,
    Exclusive
};

// -----------------------------------------------------------------------
// Arithmetic operation enums
// -----------------------------------------------------------------------

// temporal_rs: Overflow (src/options.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-totemporaloverflow
enum class TemporalOverflow : bool {
    Constrain,
    Reject,
};

// temporal_rs: DifferenceOperation (src/options.rs)
enum class DifferenceOperation : bool {
    Since,
    Until
};

} // namespace JSC
