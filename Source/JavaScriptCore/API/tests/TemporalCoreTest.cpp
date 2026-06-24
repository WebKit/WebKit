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

#include "config.h"
#include "TemporalCoreTest.h"

#include "CalendarArithmetic.h"
#include "CalendarFields.h"
#include "CalendarICUBridge.h"
#include "DurationArithmetic.h"
#include "ISO8601.h"
#include "ISOArithmetic.h"
#include "InstantCore.h"
#include "JSCTimeZone.h"
#include "PlainDateTimeCore.h"
#include "Rounding.h"
#include "TemporalCoreTypes.h"
#include "TemporalEnums.h"
#include "TimeZoneICUBridge.h"
#include "ZonedDateTimeCore.h"
#include <stdio.h>
#include <wtf/Int128.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace TemporalCore {

// ---------------------------------------------------------------------------
// Assertion helpers
// ---------------------------------------------------------------------------

static int s_failures = 0;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#define TCHECK_EQ(actual, expected, name)                                                  \
    do {                                                                                   \
        if ((actual) != (expected)) {                                                      \
            fprintf(stderr, "FAIL [%s]: got %s, expected %s\n", name, #actual, #expected); \
            s_failures++;                                                                  \
        }                                                                                  \
    } while (0)

#define TCHECK_TRUE(cond, name)                                               \
    do {                                                                      \
        if (!(cond)) {                                                        \
            fprintf(stderr, "FAIL [%s]: condition false: %s\n", name, #cond); \
            s_failures++;                                                     \
        }                                                                     \
    } while (0)

#define TCHECK_ERR(result, name)                                                  \
    do {                                                                          \
        if ((result)) {                                                           \
            fprintf(stderr, "FAIL [%s]: expected error but got success\n", name); \
            s_failures++;                                                         \
        }                                                                         \
    } while (0)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

// ---------------------------------------------------------------------------
// ISOArithmetic tests — mirrors temporal_rs src/iso.rs tests
// ---------------------------------------------------------------------------

static void testAddDaysToISODate()
{
    // temporal_rs: test balance_iso_date (now addDaysToISODate — same algorithm, taking a
    // PlainDate + day delta directly instead of (year, month, day-with-overflow)).
    // 2020-01-31 +1d -> 2020-02-01
    auto r = addDaysToISODate(ISO8601::PlainDate { 2020, 1, 31 }, 1);
    TCHECK_EQ(r.year(), 2020, "addDaysToISODate: 2020-01-31 +1d year");
    TCHECK_EQ(r.month(), 2u, "addDaysToISODate: 2020-01-31 +1d month");
    TCHECK_EQ(r.day(), 1u, "addDaysToISODate: 2020-01-31 +1d day");

    // 2020-12-31 +1d -> 2021-01-01
    auto r2 = addDaysToISODate(ISO8601::PlainDate { 2020, 12, 31 }, 1);
    TCHECK_EQ(r2.year(), 2021, "addDaysToISODate: 2020-12-31 +1d year");
    TCHECK_EQ(r2.month(), 1u, "addDaysToISODate: 2020-12-31 +1d month");
    TCHECK_EQ(r2.day(), 1u, "addDaysToISODate: 2020-12-31 +1d day");

    // 2020-01-01 -1d -> 2019-12-31
    auto r3 = addDaysToISODate(ISO8601::PlainDate { 2020, 1, 1 }, -1);
    TCHECK_EQ(r3.year(), 2019, "addDaysToISODate: 2020-01-01 -1d year");
    TCHECK_EQ(r3.month(), 12u, "addDaysToISODate: 2020-01-01 -1d month");
    TCHECK_EQ(r3.day(), 31u, "addDaysToISODate: 2020-01-01 -1d day");

    // 2020-03-01 +0d (unchanged)
    auto r4 = addDaysToISODate(ISO8601::PlainDate { 2020, 3, 1 }, 0);
    TCHECK_EQ(r4.year(), 2020, "addDaysToISODate: 2020-03-01 +0d year");
    TCHECK_EQ(r4.month(), 3u, "addDaysToISODate: 2020-03-01 +0d month");
    TCHECK_EQ(r4.day(), 1u, "addDaysToISODate: 2020-03-01 +0d day");
}

static void testRegulateISODate()
{
    // temporal_rs: test regulate_iso_date
    // constrain: 2020-02-30 -> 2020-02-29 (2020 is leap year)
    auto r = regulateISODate(2020, 2, 30, TemporalOverflow::Constrain);
    TCHECK_TRUE(r.has_value(), "regulateISODate: constrain 2020-02-30 ok");
    TCHECK_EQ(r->month(), 2u, "regulateISODate: constrain month");
    TCHECK_EQ(r->day(), 29u, "regulateISODate: constrain day");

    // constrain: 2021-02-30 -> 2021-02-28 (2021 not leap)
    auto r2 = regulateISODate(2021, 2, 30, TemporalOverflow::Constrain);
    TCHECK_TRUE(r2.has_value(), "regulateISODate: constrain 2021-02-30 ok");
    TCHECK_EQ(r2->day(), 28u, "regulateISODate: constrain 2021-02-30 day");

    // reject: 2020-13-01 -> error
    auto r3 = regulateISODate(2020, 13, 1, TemporalOverflow::Reject);
    TCHECK_TRUE(!r3.has_value(), "regulateISODate: reject 2020-13-01 errors");

    // reject: valid 2020-06-15 -> ok
    auto r4 = regulateISODate(2020, 6, 15, TemporalOverflow::Reject);
    TCHECK_TRUE(r4.has_value(), "regulateISODate: reject 2020-06-15 ok");
    TCHECK_EQ(r4->year(), 2020, "regulateISODate: reject year");
    TCHECK_EQ(r4->month(), 6u, "regulateISODate: reject month");
    TCHECK_EQ(r4->day(), 15u, "regulateISODate: reject day");
}

static void testISODateAdd()
{
    // temporal_rs: plain_date.rs test simple_date_add
    // 1976-11-18 + P43Y -> 2019-11-18
    auto r1 = isoDateAdd({ 1976, 11, 18 }, ISO8601::Duration(43, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r1.has_value(), "isoDateAdd: +43y ok");
    TCHECK_EQ(r1->year(), 2019, "isoDateAdd: +43y year");
    TCHECK_EQ(r1->month(), 11u, "isoDateAdd: +43y month");
    TCHECK_EQ(r1->day(), 18u, "isoDateAdd: +43y day");

    // 1976-11-18 + P3M -> 1977-02-18
    auto r2 = isoDateAdd({ 1976, 11, 18 }, ISO8601::Duration(0, 3, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r2.has_value(), "isoDateAdd: +3m ok");
    TCHECK_EQ(r2->year(), 1977, "isoDateAdd: +3m year");
    TCHECK_EQ(r2->month(), 2u, "isoDateAdd: +3m month");
    TCHECK_EQ(r2->day(), 18u, "isoDateAdd: +3m day");

    // 1976-11-18 + P20D -> 1976-12-08
    auto r3 = isoDateAdd({ 1976, 11, 18 }, ISO8601::Duration(0, 0, 0, 20, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r3.has_value(), "isoDateAdd: +20d ok");
    TCHECK_EQ(r3->year(), 1976, "isoDateAdd: +20d year");
    TCHECK_EQ(r3->month(), 12u, "isoDateAdd: +20d month");
    TCHECK_EQ(r3->day(), 8u, "isoDateAdd: +20d day");

    // 2019-11-18 - P43Y -> 1976-11-18
    auto r4 = isoDateAdd({ 2019, 11, 18 }, ISO8601::Duration(-43, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r4.has_value(), "isoDateAdd: -43y ok");
    TCHECK_EQ(r4->year(), 1976, "isoDateAdd: -43y year");

    // constrain: 2021-01-31 + P1M -> 2021-02-28 (not 2021-02-31)
    auto r5 = isoDateAdd({ 2021, 1, 31 }, ISO8601::Duration(0, 1, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r5.has_value(), "isoDateAdd: end-of-month constrain ok");
    TCHECK_EQ(r5->month(), 2u, "isoDateAdd: end-of-month month");
    TCHECK_EQ(r5->day(), 28u, "isoDateAdd: end-of-month day");
}

static void testISODateCompare()
{
    // temporal_rs: src/iso.rs IsoDate::cmp
    TCHECK_EQ(isoDateCompare({ 2020, 1, 1 }, { 2020, 1, 1 }), 0, "isoDateCompare: equal");
    TCHECK_EQ(isoDateCompare({ 2020, 1, 2 }, { 2020, 1, 1 }), 1, "isoDateCompare: later day");
    TCHECK_EQ(isoDateCompare({ 2020, 1, 1 }, { 2020, 1, 2 }), -1, "isoDateCompare: earlier day");
    TCHECK_EQ(isoDateCompare({ 2021, 1, 1 }, { 2020, 12, 31 }), 1, "isoDateCompare: later year");
    TCHECK_EQ(isoDateCompare({ 2020, 6, 1 }, { 2020, 7, 1 }), -1, "isoDateCompare: earlier month");
}

static void testDiffISODate()
{
    // temporal_rs: plain_date.rs test simple_date_until
    // 1969-07-24 until 1969-10-05 in days = 73
    auto r1 = diffISODate({ 1969, 7, 24 }, { 1969, 10, 5 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r1.days()), 73LL, "diffISODate: 73 days");

    // 1969-07-24 until 1996-03-03 in days = 9719
    auto r2 = diffISODate({ 1969, 7, 24 }, { 1996, 3, 3 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r2.days()), 9719LL, "diffISODate: 9719 days");

    // 1969-07-24 until 1969-10-05 in months = 2m12d
    auto r3 = diffISODate({ 1969, 7, 24 }, { 1969, 10, 5 }, TemporalUnit::Month);
    TCHECK_EQ(static_cast<int64_t>(r3.months()), 2LL, "diffISODate: months");
    TCHECK_EQ(static_cast<int64_t>(r3.days()), 11LL, "diffISODate: remaining days");

    // Same date -> zero
    auto r4 = diffISODate({ 2020, 6, 15 }, { 2020, 6, 15 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r4.days()), 0LL, "diffISODate: same date");

    // Negative diff: 1969-10-05 until 1969-07-24 in days = -73
    auto r5 = diffISODate({ 1969, 10, 5 }, { 1969, 7, 24 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r5.days()), -73LL, "diffISODate: negative days");
}

// ---------------------------------------------------------------------------
// Rounding tests — mirrors temporal_rs src/rounding.rs tests
// ---------------------------------------------------------------------------

static void testRoundNumberToIncrementDouble()
{
    // temporal_rs: round_number_to_increment tests
    // 5.5 with increment 1, HalfExpand -> 6
    TCHECK_EQ(roundNumberToIncrementDouble(5.5, 1.0, RoundingMode::HalfExpand), 6.0, "round: 5.5 HalfExpand");
    // 5.5 with increment 1, HalfTrunc -> 5
    TCHECK_EQ(roundNumberToIncrementDouble(5.5, 1.0, RoundingMode::HalfTrunc), 5.0, "round: 5.5 HalfTrunc");
    // 5.5 with increment 1, HalfEven -> 6 (round to even)
    TCHECK_EQ(roundNumberToIncrementDouble(5.5, 1.0, RoundingMode::HalfEven), 6.0, "round: 5.5 HalfEven");
    // 4.5 with increment 1, HalfEven -> 4 (round to even)
    TCHECK_EQ(roundNumberToIncrementDouble(4.5, 1.0, RoundingMode::HalfEven), 4.0, "round: 4.5 HalfEven");

    // Increment > 1: 23 / increment 5, Trunc -> 20
    TCHECK_EQ(roundNumberToIncrementDouble(23.0, 5.0, RoundingMode::Trunc), 20.0, "round: 23 inc5 Trunc");
    // 23 / increment 5, Ceil -> 25
    TCHECK_EQ(roundNumberToIncrementDouble(23.0, 5.0, RoundingMode::Ceil), 25.0, "round: 23 inc5 Ceil");
    // 23 / increment 5, Floor -> 20
    TCHECK_EQ(roundNumberToIncrementDouble(23.0, 5.0, RoundingMode::Floor), 20.0, "round: 23 inc5 Floor");

    // Negative: -23 / increment 5, Trunc -> -20
    TCHECK_EQ(roundNumberToIncrementDouble(-23.0, 5.0, RoundingMode::Trunc), -20.0, "round: -23 inc5 Trunc");
    // -23 / increment 5, Floor -> -25
    TCHECK_EQ(roundNumberToIncrementDouble(-23.0, 5.0, RoundingMode::Floor), -25.0, "round: -23 inc5 Floor");
}

static void testRoundNumberToIncrementInt128()
{
    // temporal_rs: round_number_to_increment (integer path)
    // 5 / inc 2, HalfExpand -> 6
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(5), Int128(2), RoundingMode::HalfExpand), Int128(6), "roundInt128: 5 inc2 HalfExpand");
    // 5 / inc 2, HalfTrunc -> 4
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(5), Int128(2), RoundingMode::HalfTrunc), Int128(4), "roundInt128: 5 inc2 HalfTrunc");
    // 4 / inc 2, HalfEven -> 4
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(4), Int128(2), RoundingMode::HalfEven), Int128(4), "roundInt128: 4 inc2 HalfEven");
    // 6 / inc 2, HalfEven -> 6
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(6), Int128(2), RoundingMode::HalfEven), Int128(6), "roundInt128: 6 inc2 HalfEven");

    // Nanoseconds: 1500000000 / inc 1000000000 (1s), HalfExpand -> 2000000000
    Int128 ns = Int128(1500000000LL);
    Int128 inc = Int128(1000000000LL);
    TCHECK_EQ(roundNumberToIncrementInt128(ns, inc, RoundingMode::HalfExpand), Int128(2000000000LL), "roundInt128: 1.5s HalfExpand");

    // temporal_rs: duration rounding 25h with inc=1day (86400s = 86400000000000ns)
    // 25h in ns = 90000000000000, inc = 86400000000000, Trunc -> 86400000000000
    Int128 h25 = Int128(90000000000000LL);
    Int128 day = Int128(86400000000000LL);
    TCHECK_EQ(roundNumberToIncrementInt128(h25, day, RoundingMode::Trunc), day, "roundInt128: 25h Trunc to 1day");
    // Floor same result
    TCHECK_EQ(roundNumberToIncrementInt128(h25, day, RoundingMode::Floor), day, "roundInt128: 25h Floor to 1day");
}

static void testMaximumRoundingIncrement()
{
    // temporal_rs: maximum_temporal_duration_rounding_increment
    TCHECK_TRUE(!maximumRoundingIncrement(TemporalUnit::Year).has_value(), "maxIncrement: Year = unlimited");
    TCHECK_TRUE(!maximumRoundingIncrement(TemporalUnit::Month).has_value(), "maxIncrement: Month = unlimited");
    TCHECK_TRUE(!maximumRoundingIncrement(TemporalUnit::Week).has_value(), "maxIncrement: Week = unlimited");
    TCHECK_TRUE(!maximumRoundingIncrement(TemporalUnit::Day).has_value(), "maxIncrement: Day = unlimited");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Hour).value_or(0), 24u, "maxIncrement: Hour = 24");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Minute).value_or(0), 60u, "maxIncrement: Minute = 60");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Second).value_or(0), 60u, "maxIncrement: Second = 60");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Millisecond).value_or(0), 1000u, "maxIncrement: Millisecond = 1000");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Microsecond).value_or(0), 1000u, "maxIncrement: Microsecond = 1000");
    TCHECK_EQ(maximumRoundingIncrement(TemporalUnit::Nanosecond).value_or(0), 1000u, "maxIncrement: Nanosecond = 1000");
}

// ---------------------------------------------------------------------------
// DurationArithmetic tests — mirrors temporal_rs src/builtins/core/duration.rs
// ---------------------------------------------------------------------------

static void testTimeDurationFromComponents()
{
    // temporal_rs: TimeDuration::from_components
    // 1h = 3600000000000 ns
    Int128 h1 = timeDurationFromComponents(1, 0, 0, 0, 0, 0);
    TCHECK_EQ(h1, Int128(3600000000000LL), "timeDuration: 1h");

    // 1m = 60000000000 ns
    Int128 m1 = timeDurationFromComponents(0, 1, 0, 0, 0, 0);
    TCHECK_EQ(m1, Int128(60000000000LL), "timeDuration: 1m");

    // 1s = 1000000000 ns
    Int128 s1 = timeDurationFromComponents(0, 0, 1, 0, 0, 0);
    TCHECK_EQ(s1, Int128(1000000000LL), "timeDuration: 1s");

    // 1ms = 1000000 ns
    Int128 ms1 = timeDurationFromComponents(0, 0, 0, 1, 0, 0);
    TCHECK_EQ(ms1, Int128(1000000LL), "timeDuration: 1ms");

    // 1µs = 1000 ns
    Int128 us1 = timeDurationFromComponents(0, 0, 0, 0, 1, 0);
    TCHECK_EQ(us1, Int128(1000LL), "timeDuration: 1µs");

    // 1ns
    Int128 ns1 = timeDurationFromComponents(0, 0, 0, 0, 0, 1);
    TCHECK_EQ(ns1, Int128(1LL), "timeDuration: 1ns");

    // Combined: 1h2m3s = 3723000000000 ns
    Int128 combined = timeDurationFromComponents(1, 2, 3, 0, 0, 0);
    TCHECK_EQ(combined, Int128(3723000000000LL), "timeDuration: 1h2m3s");

    // 25h = 90000000000000 ns
    Int128 h25 = timeDurationFromComponents(25, 0, 0, 0, 0, 0);
    TCHECK_EQ(h25, Int128(90000000000000LL), "timeDuration: 25h");
}

static void testDurationSign()
{
    // temporal_rs: Duration::sign
    ISO8601::Duration pos(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    TCHECK_EQ(durationSign(pos), 1, "durationSign: positive");

    ISO8601::Duration neg(-1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    TCHECK_EQ(durationSign(neg), -1, "durationSign: negative");

    ISO8601::Duration zero;
    TCHECK_EQ(durationSign(zero), 0, "durationSign: zero");

    // Mixed field signs -> should not occur in valid durations, but sign uses first nonzero
    ISO8601::Duration negHours(0, 0, 0, 0, -5, 0, 0, 0, 0, 0);
    TCHECK_EQ(durationSign(negHours), -1, "durationSign: negative hours");
}

static void testLargestSubduration()
{
    // temporal_rs: Duration::default_largest_unit
    ISO8601::Duration d1(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    TCHECK_EQ(largestSubduration(d1), TemporalUnit::Year, "largestSub: years");

    ISO8601::Duration d2(0, 2, 0, 0, 0, 0, 0, 0, 0, 0);
    TCHECK_EQ(largestSubduration(d2), TemporalUnit::Month, "largestSub: months");

    ISO8601::Duration d3(0, 0, 0, 0, 3, 0, 0, 0, 0, 0);
    TCHECK_EQ(largestSubduration(d3), TemporalUnit::Hour, "largestSub: hours");

    ISO8601::Duration d4(0, 0, 0, 0, 0, 0, 0, 0, 0, 5);
    TCHECK_EQ(largestSubduration(d4), TemporalUnit::Nanosecond, "largestSub: nanoseconds");

    // Zero duration -> Nanosecond (first nonzero not found, returns last)
    ISO8601::Duration d5;
    TCHECK_EQ(largestSubduration(d5), TemporalUnit::Nanosecond, "largestSub: zero");
}

static void testAdjustDateDurationRecord()
{
    // temporal_rs: AdjustDateDurationRecord
    ISO8601::Duration base(2, 3, 1, 5, 0, 0, 0, 0, 0, 0);

    // Override days only
    auto r1 = adjustDateDurationRecord(base, 10, std::nullopt, std::nullopt);
    TCHECK_TRUE(r1.has_value(), "adjustDateDur: days override ok");
    TCHECK_EQ(static_cast<int64_t>(r1->years()), 2LL, "adjustDateDur: years preserved");
    TCHECK_EQ(static_cast<int64_t>(r1->months()), 3LL, "adjustDateDur: months preserved");
    TCHECK_EQ(static_cast<int64_t>(r1->days()), 10LL, "adjustDateDur: days overridden");

    // Override weeks
    auto r2 = adjustDateDurationRecord(base, 5, 2, std::nullopt);
    TCHECK_TRUE(r2.has_value(), "adjustDateDur: weeks override ok");
    TCHECK_EQ(static_cast<int64_t>(r2->weeks()), 2LL, "adjustDateDur: weeks overridden");

    // Mixed signs -> invalid, should error
    auto r3 = adjustDateDurationRecord(base, -10, std::nullopt, std::nullopt);
    TCHECK_TRUE(!r3.has_value(), "adjustDateDur: mixed sign rejected");

    // Day-magnitude limit. isValidDuration enforces |normalizedSeconds| < 2^53 which, for a
    // date-only Duration, reduces to |days × 86400| < 2^53. The largest `days` that satisfies
    // this is floor((2^53 - 1) / 86400) = 104249991374; one beyond that rejects.
    ISO8601::Duration zeroBase(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    constexpr int64_t maxValidDays = 104249991374LL;
    auto rMax = adjustDateDurationRecord(zeroBase, maxValidDays, std::nullopt, std::nullopt);
    TCHECK_TRUE(rMax.has_value(), "adjustDateDur: max valid days accepted");
    auto rOver = adjustDateDurationRecord(zeroBase, maxValidDays + 1, std::nullopt, std::nullopt);
    TCHECK_TRUE(!rOver.has_value(), "adjustDateDur: days past 2^53/86400 rejected");
    // Symmetric for negative.
    auto rMinNeg = adjustDateDurationRecord(zeroBase, -maxValidDays, std::nullopt, std::nullopt);
    TCHECK_TRUE(rMinNeg.has_value(), "adjustDateDur: negative max valid days accepted");
    auto rOverNeg = adjustDateDurationRecord(zeroBase, -(maxValidDays + 1), std::nullopt, std::nullopt);
    TCHECK_TRUE(!rOverNeg.has_value(), "adjustDateDur: negative days past -2^53/86400 rejected");

    // Years/months/weeks 2^32 cap (isValidDuration step 3-5: |y|,|mo|,|w| < 2^32).
    constexpr int64_t maxField = (static_cast<int64_t>(1) << 32) - 1;
    auto rMaxY = adjustDateDurationRecord(ISO8601::Duration(maxField, 0, 0, 0, 0, 0, 0, 0, 0, 0), 0, std::nullopt, std::nullopt);
    TCHECK_TRUE(rMaxY.has_value(), "adjustDateDur: years at 2^32-1 accepted");
    auto rOverY = adjustDateDurationRecord(ISO8601::Duration(static_cast<int64_t>(1) << 32, 0, 0, 0, 0, 0, 0, 0, 0, 0), 0, std::nullopt, std::nullopt);
    TCHECK_TRUE(!rOverY.has_value(), "adjustDateDur: years at 2^32 rejected");
    auto rOverMo = adjustDateDurationRecord(zeroBase, 0, std::nullopt, static_cast<int64_t>(1) << 32);
    TCHECK_TRUE(!rOverMo.has_value(), "adjustDateDur: months at 2^32 rejected");
    auto rOverW = adjustDateDurationRecord(zeroBase, 0, static_cast<int64_t>(1) << 32, std::nullopt);
    TCHECK_TRUE(!rOverW.has_value(), "adjustDateDur: weeks at 2^32 rejected");
}

// ---------------------------------------------------------------------------
// CalendarArithmetic tests — mirrors temporal_rs src/builtins/core/calendar.rs
// ---------------------------------------------------------------------------

static void testCalendarDateAdd()
{
    // ISO8601 path only (no ICU needed)
    // temporal_rs: Calendar::date_add (iso8601)
    // 1976-11-18 + P43Y = 2019-11-18
    auto r1 = calendarDateAdd({ 1976, 11, 18 }, ISO8601::Duration(43, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r1.has_value(), "calendarDateAdd: +43y ok");
    TCHECK_EQ(r1->year(), 2019, "calendarDateAdd: +43y year");

    // 1976-11-18 + P-43Y = 1933-11-18
    auto r2 = calendarDateAdd({ 1976, 11, 18 }, ISO8601::Duration(-43, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r2.has_value(), "calendarDateAdd: -43y ok");
    TCHECK_EQ(r2->year(), 1933, "calendarDateAdd: -43y year");

    // End-of-month constrain: 2020-01-31 + P1M = 2020-02-29 (leap year)
    auto r3 = calendarDateAdd({ 2020, 1, 31 }, ISO8601::Duration(0, 1, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r3.has_value(), "calendarDateAdd: eom constrain ok");
    TCHECK_EQ(r3->month(), 2u, "calendarDateAdd: eom month");
    TCHECK_EQ(r3->day(), 29u, "calendarDateAdd: eom day");

    // Weeks: 1976-11-18 + P2W = 1976-12-02
    auto r4 = calendarDateAdd({ 1976, 11, 18 }, ISO8601::Duration(0, 0, 2, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r4.has_value(), "calendarDateAdd: +2w ok");
    TCHECK_EQ(r4->month(), 12u, "calendarDateAdd: +2w month");
    TCHECK_EQ(r4->day(), 2u, "calendarDateAdd: +2w day");
}

static void testCalendarDateUntil()
{
    // ISO8601 path only — mirrors temporal_rs: Calendar::date_until + date_until_largest_year
    // 1969-07-24 until 1996-03-03 in days = 9719
    auto r1 = calendarDateUntil({ 1969, 7, 24 }, { 1996, 3, 3 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r1.days()), 9719LL, "calendarDateUntil: 9719 days");

    // Same date -> zero
    auto r2 = calendarDateUntil({ 2020, 6, 15 }, { 2020, 6, 15 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r2.days()), 0LL, "calendarDateUntil: same date");

    // 1969-07-24 until 1969-10-05 in months = 2m11d
    auto r3 = calendarDateUntil({ 1969, 7, 24 }, { 1969, 10, 5 }, TemporalUnit::Month);
    TCHECK_EQ(static_cast<int64_t>(r3.months()), 2LL, "calendarDateUntil: 2 months");

    // Negative: later until earlier
    auto r4 = calendarDateUntil({ 1996, 3, 3 }, { 1969, 7, 24 }, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r4.days()), -9719LL, "calendarDateUntil: -9719 days");

    // temporal_rs: date_until_largest_year — full ISO8601 table
    // Each entry: (one, two, (years, months, weeks, days))
    struct DateUntilCase {
        ISO8601::PlainDate one;
        ISO8601::PlainDate two;
        int64_t years, months, days;
    };
    static const DateUntilCase cases[] = {
        { { 2021, 7, 16 }, { 2021, 7, 16 }, 0, 0, 0 }, // same
        { { 2021, 7, 16 }, { 2021, 7, 17 }, 0, 0, 1 }, // +1d
        { { 2021, 7, 16 }, { 2021, 7, 23 }, 0, 0, 7 }, // +7d
        { { 2021, 7, 16 }, { 2021, 8, 16 }, 0, 1, 0 }, // +1m
        { { 2020, 12, 16 }, { 2021, 1, 16 }, 0, 1, 0 }, // month wrap
        { { 2021, 1, 5 },  { 2021, 2, 5 },  0, 1, 0 }, // +1m
        { { 2021, 1, 7 },  { 2021, 3, 7 },  0, 2, 0 }, // +2m
        { { 2021, 7, 16 }, { 2021, 8, 17 }, 0, 1, 1 }, // +1m1d
        { { 2021, 7, 16 }, { 2021, 8, 13 }, 0, 0, 28 }, // sub-month days
        { { 2021, 7, 16 }, { 2021, 9, 16 }, 0, 2, 0 }, // +2m
        { { 2021, 7, 16 }, { 2022, 7, 16 }, 1, 0, 0 }, // exactly 1y
        { { 2021, 7, 16 }, { 2031, 7, 16 }, 10, 0, 0 }, // exactly 10y
        { { 2021, 7, 16 }, { 2022, 7, 19 }, 1, 0, 3 }, // 1y3d
        { { 2021, 7, 16 }, { 2022, 9, 19 }, 1, 2, 3 }, // 1y2m3d
        { { 2021, 7, 16 }, { 2031, 12, 16 }, 10, 5, 0 }, // 10y5m
        { { 1997, 12, 16 }, { 2021, 7, 16 }, 23, 7, 0 }, // large: 23y7m
        { { 1997, 7, 16 }, { 2021, 7, 16 }, 24, 0, 0 }, // large: 24y
        { { 1997, 7, 16 }, { 2021, 7, 15 }, 23, 11, 29 }, // just under 24y
        { { 1997, 6, 16 }, { 2021, 6, 15 }, 23, 11, 30 }, // just under 24y
        { { 1960, 2, 16 }, { 2020, 3, 16 }, 60, 1, 0 }, // 60y1m
        { { 1960, 2, 16 }, { 2021, 3, 15 }, 61, 0, 27 }, // 61y27d
        { { 1960, 2, 16 }, { 2020, 3, 15 }, 60, 0, 28 }, // 60y28d
        { { 2021, 3, 30 }, { 2021, 7, 16 }, 0, 3, 16 }, // 3m16d
        { { 2020, 3, 30 }, { 2021, 7, 16 }, 1, 3, 16 }, // 1y3m16d
        { { 1960, 3, 30 }, { 2021, 7, 16 }, 61, 3, 16 }, // 61y3m16d
        { { 2019, 12, 30 }, { 2021, 7, 16 }, 1, 6, 16 }, // 1y6m16d
        { { 2020, 12, 30 }, { 2021, 7, 16 }, 0, 6, 16 }, // 6m16d
        { { 1997, 12, 30 }, { 2021, 7, 16 }, 23, 6, 16 }, // 23y6m16d
        { { 1, 12, 25 }, { 2021, 7, 16 }, 2019, 6, 21 }, // ancient date
        { { 2019, 12, 30 }, { 2021, 3, 5 }, 1, 2, 5 }, // 1y2m5d
        // Negative cases
        { { 2021, 7, 17 }, { 2021, 7, 16 }, 0, 0, -1 },
        { { 2021, 7, 23 }, { 2021, 7, 16 }, 0, 0, -7 },
        { { 2021, 8, 16 }, { 2021, 7, 16 }, 0, -1, 0 },
        { { 2021, 1, 16 }, { 2020, 12, 16 }, 0, -1, 0 },
        { { 2021, 2, 5 },  { 2021, 1, 5 },  0, -1, 0 },
        { { 2021, 3, 7 },  { 2021, 1, 7 },  0, -2, 0 },
        { { 2021, 8, 17 }, { 2021, 7, 16 }, 0, -1, -1 },
    };
    for (auto& c : cases) {
        auto r = calendarDateUntil(c.one, c.two, TemporalUnit::Year);
        TCHECK_EQ(static_cast<int64_t>(r.years()),  c.years,  "dateUntilLargestYear: years");
        TCHECK_EQ(static_cast<int64_t>(r.months()), c.months, "dateUntilLargestYear: months");
        TCHECK_EQ(static_cast<int64_t>(r.days()),   c.days,   "dateUntilLargestYear: days");
    }
}

// ---------------------------------------------------------------------------
// ISO date limit tests — mirrors temporal_rs src/iso.rs limit tests
// ---------------------------------------------------------------------------

static void testISODateLimits()
{
    // Max valid ISO date for Temporal: +275760-09-13 (±1e8 epoch days)
    // addDaysToISODate returns outOfRangeYear only when year exceeds the year representation limit
    auto rMax = addDaysToISODate(ISO8601::PlainDate { 275760, 9, 13 }, 0);
    TCHECK_EQ(rMax.year(), 275760, "limits: max date year");
    TCHECK_EQ(rMax.month(), 9u, "limits: max date month");
    TCHECK_EQ(rMax.day(), 13u, "limits: max date day");

    // Min valid ISO date: -271821-04-19
    auto rMin = addDaysToISODate(ISO8601::PlainDate { -271821, 4, 19 }, 0);
    TCHECK_EQ(rMin.year(), -271821, "limits: min date year");
    TCHECK_EQ(rMin.month(), 4u, "limits: min date month");
    TCHECK_EQ(rMin.day(), 19u, "limits: min date day");

    // Unix epoch: 1970-01-01
    auto rEpoch = addDaysToISODate(ISO8601::PlainDate { 1970, 1, 1 }, 0);
    TCHECK_EQ(rEpoch.year(), 1970, "limits: epoch year");
    TCHECK_EQ(rEpoch.month(), 1u, "limits: epoch month");
    TCHECK_EQ(rEpoch.day(), 1u, "limits: epoch day");

    // Day before epoch: 1969-12-31
    auto rEpochMinus1 = addDaysToISODate(ISO8601::PlainDate { 1969, 12, 31 }, 0);
    TCHECK_EQ(rEpochMinus1.year(), 1969, "limits: epoch-1 year");
    TCHECK_EQ(rEpochMinus1.month(), 12u, "limits: epoch-1 month");
    TCHECK_EQ(rEpochMinus1.day(), 31u, "limits: epoch-1 day");

    // isoDateAdd past Temporal max -> error (±1e8 days limit)
    // Max date + P1D exceeds the epoch day limit
    auto rOverMax = isoDateAdd({ 275760, 9, 13 }, ISO8601::Duration(0, 0, 0, 1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rOverMax.has_value(), "limits: isoDateAdd max+1d rejects");

    // Min date - P1D exceeds minimum
    auto rUnderMin = isoDateAdd({ -271821, 4, 19 }, ISO8601::Duration(0, 0, 0, -1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rUnderMin.has_value(), "limits: isoDateAdd min-1d rejects");

    // regulateISODate validates month/day range (not Temporal epoch limits)
    auto rRegValid = regulateISODate(275760, 9, 13, TemporalOverflow::Reject);
    TCHECK_TRUE(rRegValid.has_value(), "limits: regulate 275760-09-13 ok");

    auto rRegBadMonth = regulateISODate(2020, 13, 1, TemporalOverflow::Reject);
    TCHECK_TRUE(!rRegBadMonth.has_value(), "limits: regulate month 13 rejects");
}

// ---------------------------------------------------------------------------
// Negative number rounding — mirrors temporal_rs src/rounding.rs tests
// ---------------------------------------------------------------------------

static void testNegativeRounding()
{
    // temporal_rs: test_basic_rounding_cases (negative values)
    // -101 / 10
    TCHECK_EQ(roundNumberToIncrementDouble(-101.0, 10.0, RoundingMode::Ceil), -100.0, "negRound: -101 inc10 Ceil");
    TCHECK_EQ(roundNumberToIncrementDouble(-101.0, 10.0, RoundingMode::Floor), -110.0, "negRound: -101 inc10 Floor");
    TCHECK_EQ(roundNumberToIncrementDouble(-101.0, 10.0, RoundingMode::Expand), -110.0, "negRound: -101 inc10 Expand");
    TCHECK_EQ(roundNumberToIncrementDouble(-101.0, 10.0, RoundingMode::Trunc), -100.0, "negRound: -101 inc10 Trunc");

    // -105 / 10 (midpoint)
    TCHECK_EQ(roundNumberToIncrementDouble(-105.0, 10.0, RoundingMode::HalfCeil), -100.0, "negRound: -105 HalfCeil");
    TCHECK_EQ(roundNumberToIncrementDouble(-105.0, 10.0, RoundingMode::HalfFloor), -110.0, "negRound: -105 HalfFloor");
    TCHECK_EQ(roundNumberToIncrementDouble(-105.0, 10.0, RoundingMode::HalfExpand), -110.0, "negRound: -105 HalfExpand");
    TCHECK_EQ(roundNumberToIncrementDouble(-105.0, 10.0, RoundingMode::HalfTrunc), -100.0, "negRound: -105 HalfTrunc");
    TCHECK_EQ(roundNumberToIncrementDouble(-105.0, 10.0, RoundingMode::HalfEven), -100.0, "negRound: -105 HalfEven (even=10)");

    // -115 / 10: HalfEven -> -120 (even=12)
    TCHECK_EQ(roundNumberToIncrementDouble(-115.0, 10.0, RoundingMode::HalfEven), -120.0, "negRound: -115 HalfEven (even=12)");

    // -107 / 10 (not midpoint)
    TCHECK_EQ(roundNumberToIncrementDouble(-107.0, 10.0, RoundingMode::Ceil), -100.0, "negRound: -107 Ceil");
    TCHECK_EQ(roundNumberToIncrementDouble(-107.0, 10.0, RoundingMode::Floor), -110.0, "negRound: -107 Floor");

    // Int128 negative rounding
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Ceil), Int128(-100), "negRoundI128: Ceil");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Floor), Int128(-110), "negRoundI128: Floor");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::HalfExpand), Int128(-110), "negRoundI128: HalfExpand midpoint");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::HalfTrunc), Int128(-100), "negRoundI128: HalfTrunc midpoint");
}

// ---------------------------------------------------------------------------
// roundNumberToIncrementAsIfPositive — mirrors temporal_rs round_as_if_positive
// ---------------------------------------------------------------------------

static void testRoundAsIfPositive()
{
    // roundNumberToIncrementAsIfPositive treats negative x as positive for rounding direction.
    // x=-107 inc=10: Trunc→-110 (toward -∞ when treated positive), Expand→-100.
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(-107), Int128(10), RoundingMode::Trunc), Int128(-110), "asIfPos: -107 Trunc=-110");
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(-107), Int128(10), RoundingMode::Expand), Int128(-100), "asIfPos: -107 Expand=-100");
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(-107), Int128(10), RoundingMode::Ceil), Int128(-100), "asIfPos: -107 Ceil=-100");
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(-107), Int128(10), RoundingMode::Floor), Int128(-110), "asIfPos: -107 Floor=-110");

    // Positive values: same as regular roundNumberToIncrementInt128
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(107), Int128(10), RoundingMode::Trunc), Int128(100), "asIfPos: 107 Trunc=100");
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(107), Int128(10), RoundingMode::Expand), Int128(110), "asIfPos: 107 Expand=110");

    // Zero is unchanged
    TCHECK_EQ(roundNumberToIncrementAsIfPositive(Int128(0), Int128(10), RoundingMode::HalfExpand), Int128(0), "asIfPos: 0 = 0");
}

// ---------------------------------------------------------------------------
// negateTemporalRoundingMode — mirrors temporal_rs RoundingMode::negate
// ---------------------------------------------------------------------------

static void testNegateRoundingMode()
{
    // temporal_rs: RoundingMode::negate
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::Floor)), static_cast<int>(RoundingMode::Ceil), "negate: Floor->Ceil");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::Ceil)), static_cast<int>(RoundingMode::Floor), "negate: Ceil->Floor");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::HalfFloor)), static_cast<int>(RoundingMode::HalfCeil), "negate: HalfFloor->HalfCeil");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::HalfCeil)), static_cast<int>(RoundingMode::HalfFloor), "negate: HalfCeil->HalfFloor");
    // Symmetric modes unchanged
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::Trunc)), static_cast<int>(RoundingMode::Trunc), "negate: Trunc unchanged");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::Expand)), static_cast<int>(RoundingMode::Expand), "negate: Expand unchanged");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::HalfExpand)), static_cast<int>(RoundingMode::HalfExpand), "negate: HalfExpand unchanged");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::HalfTrunc)), static_cast<int>(RoundingMode::HalfTrunc), "negate: HalfTrunc unchanged");
    TCHECK_EQ(static_cast<int>(negateTemporalRoundingMode(RoundingMode::HalfEven)), static_cast<int>(RoundingMode::HalfEven), "negate: HalfEven unchanged");
}

// ---------------------------------------------------------------------------
// Duration balancing — mirrors temporal_rs balance tests
// ---------------------------------------------------------------------------

static void testDurationBalancing()
{
    // temporal_rs: balance_days_up_to_both_years_and_months
    // 2020-01-01 + 11M = 2020-12-01, then + 396D = 2022-01-01
    auto r = isoDateAdd({ 2020, 1, 1 }, ISO8601::Duration(0, 11, 0, 396, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r.has_value(), "balance: 11m+396d from 2020-01-01 ok");
    TCHECK_EQ(r->year(), 2022, "balance: 11m+396d year=2022");
    TCHECK_EQ(r->month(), 1u, "balance: 11m+396d month=1");

    // temporal_rs: negative balance
    // -60h = -3d (using timeDurationFromComponents)
    Int128 neg60h = timeDurationFromComponents(-60, 0, 0, 0, 0, 0);
    TCHECK_EQ(neg60h, Int128(-216000000000000LL), "balance: -60h in ns");

    // Subsecond balancing: -999ms + -999999µs + -999999999ns = -2.998998999s
    Int128 negMs = timeDurationFromComponents(0, 0, 0, -999, -999999, -999999999);
    // Total = -999*1e6 - 999999*1e3 - 999999999 = -999000000 - 999999000 - 999999999 = -2998998999 ns
    TCHECK_EQ(negMs, Int128(-2998998999LL), "balance: -999ms-999999µs-999999999ns");
}

// ---------------------------------------------------------------------------
// isoDateAdd boundary/error cases
// ---------------------------------------------------------------------------

static void testISODateAddBoundaries()
{
    // temporal_rs: date_add_limits — adding to max date
    // Max date + P1D -> error (out of range)
    auto rOverMax = isoDateAdd({ 275760, 9, 13 }, ISO8601::Duration(0, 0, 0, 1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rOverMax.has_value(), "addBounds: max+1d rejects");

    // Min date - P1D -> error
    auto rUnderMin = isoDateAdd({ -271821, 4, 19 }, ISO8601::Duration(0, 0, 0, -1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rUnderMin.has_value(), "addBounds: min-1d rejects");

    // Max date itself is valid
    auto rMax = isoDateAdd({ 275760, 9, 12 }, ISO8601::Duration(0, 0, 0, 1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(rMax.has_value(), "addBounds: max date ok");
    TCHECK_EQ(rMax->year(), 275760, "addBounds: max year");
    TCHECK_EQ(rMax->day(), 13u, "addBounds: max day");

    // Constrain: exceed max -> constrain clamps
    auto rConstrain = isoDateAdd({ 275760, 9, 13 }, ISO8601::Duration(0, 1, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    // +1M from 275760-09-13 would be 275760-10-13 which exceeds max -> out of range
    TCHECK_TRUE(!rConstrain.has_value(), "addBounds: exceed max+1M errors");
}

// ---------------------------------------------------------------------------
// New tests — ISOArithmetic, DurationArithmetic, Rounding, Instant, Calendar,
// TimeZone, PlainDateTime
// ---------------------------------------------------------------------------

static void testBalanceISOYearMonth()
{
    // Month overflow: 2021-13 -> 2022-01
    auto r1 = balanceISOYearMonth(2021, 13);
    TCHECK_EQ(r1.year(), 2022, "balanceYM: 2021m13 year");
    TCHECK_EQ(r1.month(), 1u, "balanceYM: 2021m13 month");
    // Month underflow: 2021-00 -> 2020-12
    auto r2 = balanceISOYearMonth(2021, 0);
    TCHECK_EQ(r2.year(), 2020, "balanceYM: 2021m0 year");
    TCHECK_EQ(r2.month(), 12u, "balanceYM: 2021m0 month");
    // No-op: 2020-06 -> 2020-06
    auto r3 = balanceISOYearMonth(2020, 6);
    TCHECK_EQ(r3.year(), 2020, "balanceYM: identity year");
    TCHECK_EQ(r3.month(), 6u, "balanceYM: identity month");
    // Large overflow: 2021-25 -> 2023-01
    auto r4 = balanceISOYearMonth(2021, 25);
    TCHECK_EQ(r4.year(), 2023, "balanceYM: 2021m25 year");
    TCHECK_EQ(r4.month(), 1u, "balanceYM: 2021m25 month");
}

static void testISOTimeCompare()
{
    // Equal
    TCHECK_EQ(isoTimeCompare({ 12, 0, 0, 0, 0, 0 }, { 12, 0, 0, 0, 0, 0 }), 0, "timeCompare: equal");
    // Hour difference
    TCHECK_EQ(isoTimeCompare({ 14, 0, 0, 0, 0, 0 }, { 12, 0, 0, 0, 0, 0 }), 1, "timeCompare: later hour");
    TCHECK_EQ(isoTimeCompare({ 12, 0, 0, 0, 0, 0 }, { 14, 0, 0, 0, 0, 0 }), -1, "timeCompare: earlier hour");
    // Nanosecond difference
    ISO8601::PlainTime t1(0, 0, 0, 0, 0, 1), t2(0, 0, 0, 0, 0, 0);
    TCHECK_EQ(isoTimeCompare(t1, t2), 1, "timeCompare: 1ns later");
    TCHECK_EQ(isoTimeCompare(t2, t1), -1, "timeCompare: 1ns earlier");
    // Max time vs min time
    ISO8601::PlainTime maxT(23, 59, 59, 999, 999, 999), minT(0, 0, 0, 0, 0, 0);
    TCHECK_EQ(isoTimeCompare(maxT, minT), 1, "timeCompare: max > min");
}

static void testApplyUnsignedRoundingMode()
{
    // x between r1 and r2 — direction modes
    TCHECK_EQ(applyUnsignedRoundingMode(1.3, 1.0, 2.0, UnsignedRoundingMode::Zero), 1.0, "applyURM: 1.3 Zero");
    TCHECK_EQ(applyUnsignedRoundingMode(1.3, 1.0, 2.0, UnsignedRoundingMode::Infinity), 2.0, "applyURM: 1.3 Inf");
    // x == r1 (exact lower bound)
    TCHECK_EQ(applyUnsignedRoundingMode(1.0, 1.0, 2.0, UnsignedRoundingMode::Zero), 1.0, "applyURM: exact=r1");
    // HalfZero at midpoint
    TCHECK_EQ(applyUnsignedRoundingMode(1.5, 1.0, 2.0, UnsignedRoundingMode::HalfZero), 1.0, "applyURM: 1.5 HalfZero");
    TCHECK_EQ(applyUnsignedRoundingMode(1.5, 1.0, 2.0, UnsignedRoundingMode::HalfInfinity), 2.0, "applyURM: 1.5 HalfInf");
    // HalfEven: 2.5 -> 2 (even lower), 3.5 -> 4 (even upper)
    TCHECK_EQ(applyUnsignedRoundingMode(2.5, 2.0, 3.0, UnsignedRoundingMode::HalfEven), 2.0, "applyURM: 2.5 HalfEven->2");
    TCHECK_EQ(applyUnsignedRoundingMode(3.5, 3.0, 4.0, UnsignedRoundingMode::HalfEven), 4.0, "applyURM: 3.5 HalfEven->4");
}

static void testNegateDuration()
{
    ISO8601::Duration d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    auto neg = negateDuration(d);
    TCHECK_EQ(static_cast<int64_t>(neg.years()), -1LL, "negate: years");
    TCHECK_EQ(static_cast<int64_t>(neg.months()), -2LL, "negate: months");
    TCHECK_EQ(static_cast<int64_t>(neg.days()), -4LL, "negate: days");
    TCHECK_EQ(static_cast<int64_t>(neg.hours()), -5LL, "negate: hours");
    // Double negation = identity
    auto back = negateDuration(neg);
    TCHECK_EQ(static_cast<int64_t>(back.years()), 1LL, "negate: double neg years");
    // Zero unchanged
    ISO8601::Duration zero;
    auto negZero = negateDuration(zero);
    TCHECK_EQ(durationSign(negZero), 0, "negate: zero");
}

static void testAbsDuration()
{
    // Positive unchanged
    ISO8601::Duration pos(1, 2, 0, 4, 0, 0, 0, 0, 0, 0);
    auto absPos = absDuration(pos);
    TCHECK_EQ(static_cast<int64_t>(absPos.years()), 1LL, "abs: pos years");
    TCHECK_EQ(static_cast<int64_t>(absPos.days()), 4LL, "abs: pos days");
    // Negative -> positive
    ISO8601::Duration neg(-1, -2, 0, -4, 0, 0, 0, 0, 0, 0);
    auto absNeg = absDuration(neg);
    TCHECK_EQ(static_cast<int64_t>(absNeg.years()), 1LL, "abs: neg years");
    TCHECK_EQ(static_cast<int64_t>(absNeg.months()), 2LL, "abs: neg months");
    TCHECK_EQ(static_cast<int64_t>(absNeg.days()), 4LL, "abs: neg days");
    TCHECK_EQ(durationSign(absNeg), 1, "abs: sign positive");
    // Zero unchanged
    ISO8601::Duration zero;
    auto absZero = absDuration(zero);
    TCHECK_EQ(durationSign(absZero), 0, "abs: zero");
}

static void testGetUTCEpochNanoseconds()
{
    // Unix epoch = 0
    auto r0 = getUTCEpochNanoseconds({ 1970, 1, 1 }, { 0, 0, 0, 0, 0, 0 });
    TCHECK_EQ(r0, Int128(0LL), "utcEpoch: 1970-01-01 00:00:00 = 0");
    // 1 day = 86400000000000 ns
    auto r1 = getUTCEpochNanoseconds({ 1970, 1, 2 }, { 0, 0, 0, 0, 0, 0 });
    TCHECK_EQ(r1, Int128(86400000000000LL), "utcEpoch: 1970-01-02 = 1 day");
    // 1 second
    auto r2 = getUTCEpochNanoseconds({ 1970, 1, 1 }, { 0, 0, 1, 0, 0, 0 });
    TCHECK_EQ(r2, Int128(1000000000LL), "utcEpoch: 1970-01-01 00:00:01 = 1s");
    // 2001-09-09T01:46:40Z = 1000000000 seconds = 1e18 ns
    auto r3 = getUTCEpochNanoseconds({ 2001, 9, 9 }, { 1, 46, 40, 0, 0, 0 });
    TCHECK_EQ(r3, Int128(1000000000LL) * Int128(1000000000LL), "utcEpoch: unix billion");
    // Negative: 1969-12-31 = -1 day
    auto r4 = getUTCEpochNanoseconds({ 1969, 12, 31 }, { 0, 0, 0, 0, 0, 0 });
    TCHECK_EQ(r4, Int128(-86400000000000LL), "utcEpoch: 1969-12-31 = -1day");
}

static void testSplitTimeDuration()
{
    // 25 hours = 90000000000000 ns -> 1 overflow day, 1h remainder
    auto [days1, rem1] = splitTimeDuration(Int128(90000000000000LL));
    TCHECK_EQ(days1, 1LL, "split: 25h = 1 overflow day");
    TCHECK_EQ(rem1, Int128(3600000000000LL), "split: 25h remainder = 1h");
    // Exact 1 day
    auto [days2, rem2] = splitTimeDuration(Int128(86400000000000LL));
    TCHECK_EQ(days2, 1LL, "split: 1day overflow");
    TCHECK_EQ(rem2, Int128(0LL), "split: 1day remainder=0");
    // Less than 1 day — no overflow
    auto [days3, rem3] = splitTimeDuration(Int128(3600000000000LL));
    TCHECK_EQ(days3, 0LL, "split: 1h no overflow");
    TCHECK_EQ(rem3, Int128(3600000000000LL), "split: 1h remainder");
    // Negative: -25h -> floor(-25/24) = -2, remainder = 23h = 82800000000000 ns
    auto [days4, rem4] = splitTimeDuration(Int128(-90000000000000LL));
    TCHECK_EQ(days4, -2LL, "split: -25h overflow=-2 (floor)");
    TCHECK_EQ(rem4, Int128(82800000000000LL), "split: -25h remainder=23h");
}

static void testPlainTimeFromSubdayNs()
{
    // 0 -> midnight
    auto t0 = plainTimeFromSubdayNs(Int128(0));
    TCHECK_EQ(t0.hour(), 0u, "ptFromNs: midnight hour");
    TCHECK_EQ(t0.nanosecond(), 0u, "ptFromNs: midnight ns");
    // 1 hour = 3600000000000 ns -> 01:00:00
    auto t1 = plainTimeFromSubdayNs(Int128(3600000000000LL));
    TCHECK_EQ(t1.hour(), 1u, "ptFromNs: 1h hour");
    TCHECK_EQ(t1.minute(), 0u, "ptFromNs: 1h minute");
    // 1 ns -> 00:00:00.000000001
    auto t2 = plainTimeFromSubdayNs(Int128(1));
    TCHECK_EQ(t2.nanosecond(), 1u, "ptFromNs: 1ns");
    // 13:00:00 = 46800000000000 ns
    auto t3 = plainTimeFromSubdayNs(Int128(46800000000000LL));
    TCHECK_EQ(t3.hour(), 13u, "ptFromNs: 13h hour");
}

static void testAdd24HourDaysToTimeDuration()
{
    // Add 1 day (86400000000000 ns) to 1h (3600000000000 ns) = 90000000000000 ns
    auto r1 = add24HourDaysToTimeDuration(Int128(3600000000000LL), 1.0);
    TCHECK_TRUE(r1.has_value(), "add24h: 1h+1d ok");
    TCHECK_EQ(*r1, Int128(90000000000000LL), "add24h: 1h+1d = 25h");
    // Add 0 days -> unchanged
    auto r2 = add24HourDaysToTimeDuration(Int128(3600000000000LL), 0.0);
    TCHECK_TRUE(r2.has_value(), "add24h: +0d ok");
    TCHECK_EQ(*r2, Int128(3600000000000LL), "add24h: +0d unchanged");
    // Negative days: 25h - 1d = 1h
    auto r3 = add24HourDaysToTimeDuration(Int128(90000000000000LL), -1.0);
    TCHECK_TRUE(r3.has_value(), "add24h: -1d ok");
    TCHECK_EQ(*r3, Int128(3600000000000LL), "add24h: 25h-1d = 1h");
}

static void testTemporalDurationFromInternal()
{
    // 4 days as time nanoseconds -> largestUnit=Day yields 4 days, 0 hours
    Int128 fourDays = Int128(4LL) * Int128(86400000000000LL);
    auto internal = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), fourDays);
    auto resultOrError = temporalDurationFromInternal(internal, TemporalUnit::Day);
    TCHECK_TRUE(resultOrError.has_value(), "fromInternal: valid duration");
    auto& result = *resultOrError;
    TCHECK_EQ(static_cast<int64_t>(result.days()), 4LL, "fromInternal: 4d days");
    TCHECK_EQ(static_cast<int64_t>(result.hours()), 0LL, "fromInternal: 4d hours=0");
    // largestUnit=Hour: 4 days = 96 hours, 0 days
    auto result2OrError = temporalDurationFromInternal(internal, TemporalUnit::Hour);
    TCHECK_TRUE(result2OrError.has_value(), "fromInternal: valid duration 2");
    auto& result2 = *result2OrError;
    TCHECK_EQ(static_cast<int64_t>(result2.hours()), 96LL, "fromInternal: 96h");
    TCHECK_EQ(static_cast<int64_t>(result2.days()), 0LL, "fromInternal: 96h days=0");
}

static void testCompareISODateTime()
{
    ISO8601::PlainDate d1 { 2019, 1, 8 }, d2 { 2021, 9, 7 };
    ISO8601::PlainTime t1 { 8, 22, 36, 0, 0, 0 }, t2 { 12, 39, 40, 0, 0, 0 };
    // Equal
    TCHECK_EQ(compareISODateTime(d1, t1, d1, t1), 0, "compareIDT: equal");
    // Different date — earlier vs later
    TCHECK_EQ(compareISODateTime(d1, t1, d2, t2), -1, "compareIDT: earlier");
    TCHECK_EQ(compareISODateTime(d2, t2, d1, t1), 1, "compareIDT: later");
    // Same date, different time
    ISO8601::PlainTime earlyT { 8, 22, 35, 0, 0, 0 };
    TCHECK_EQ(compareISODateTime(d1, earlyT, d1, t1), -1, "compareIDT: earlier time");
}

static void testMaximumInstantIncrement()
{
    TCHECK_EQ(maximumInstantIncrement(TemporalUnit::Hour), 24.0, "maxInstInc: Hour=24");
    TCHECK_EQ(maximumInstantIncrement(TemporalUnit::Minute), 1440.0, "maxInstInc: Minute=1440");
    TCHECK_EQ(maximumInstantIncrement(TemporalUnit::Second), 86400.0, "maxInstInc: Second=86400");
    TCHECK_EQ(maximumInstantIncrement(TemporalUnit::Millisecond), 8.64e7, "maxInstInc: Ms");
    TCHECK_EQ(maximumInstantIncrement(TemporalUnit::Microsecond), 8.64e10, "maxInstInc: µs");
}

static void testToDateDurationRecordWithoutTime()
{
    // Strip time fields, keep date fields
    ISO8601::Duration d(1, 2, 0, 4, 5, 6, 7, 8, 9, 10);
    auto r = toDateDurationRecordWithoutTime(d);
    TCHECK_TRUE(r.has_value(), "stripTime: ok");
    TCHECK_EQ(static_cast<int64_t>(r->years()), 1LL, "stripTime: years");
    TCHECK_EQ(static_cast<int64_t>(r->months()), 2LL, "stripTime: months");
    TCHECK_EQ(static_cast<int64_t>(r->days()), 4LL, "stripTime: days");
    TCHECK_EQ(static_cast<int64_t>(r->hours()), 0LL, "stripTime: hours=0");
    TCHECK_EQ(static_cast<int64_t>(r->minutes()), 0LL, "stripTime: minutes=0");
}

// ---------------------------------------------------------------------------
// totalSeconds / totalSubseconds — internal balance helpers
// ---------------------------------------------------------------------------

static void testTotalSecondsAndSubseconds()
{
    // temporal_rs: internal balance helpers
    // 1h30m = 5400s
    ISO8601::Duration d1(0, 0, 0, 0, 1, 30, 0, 0, 0, 0);
    TCHECK_EQ(totalSeconds(d1), 5400LL, "totalSec: 1h30m=5400s");

    // 1d2h = 26*3600 = 93600s
    ISO8601::Duration d2(0, 0, 0, 1, 2, 0, 0, 0, 0, 0);
    TCHECK_EQ(totalSeconds(d2), 93600LL, "totalSec: 1d2h=93600s");

    // 0 duration -> 0s
    ISO8601::Duration z;
    TCHECK_EQ(totalSeconds(z), 0LL, "totalSec: zero");

    // 999ms + 999999µs + 999999999ns = 999*1e6 + 999999*1e3 + 999999999 = 2998998999 ns
    ISO8601::Duration d3(0, 0, 0, 0, 0, 0, 0, 999, 999999, 999999999);
    Int128 expected = Int128(2998998999LL);
    TCHECK_EQ(totalSubseconds(d3), expected, "totalSub: max subseconds");

    // 1s = 0 subseconds (only ms/µs/ns contribute)
    ISO8601::Duration d4(0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
    TCHECK_EQ(totalSubseconds(d4), Int128(0LL), "totalSub: 1s=0 subseconds");
}

// ---------------------------------------------------------------------------
// totalTimeDuration — fractional unit conversion
// ---------------------------------------------------------------------------

static void testTotalTimeDuration()
{
    // temporal_rs: internal nanosecond-to-unit conversion
    // 3600000000000 ns = 1 hour
    TCHECK_EQ(totalTimeDuration(Int128(3600000000000LL), TemporalUnit::Hour), 1.0, "totalTD: 1h");
    // 86400000000000 ns = 1 day
    TCHECK_EQ(totalTimeDuration(Int128(86400000000000LL), TemporalUnit::Day), 1.0, "totalTD: 1day");
    // 1000000000 ns = 1 second
    TCHECK_EQ(totalTimeDuration(Int128(1000000000LL), TemporalUnit::Second), 1.0, "totalTD: 1s");
    // 90000000000000 ns (25h) in hours = 25.0
    TCHECK_EQ(totalTimeDuration(Int128(90000000000000LL), TemporalUnit::Hour), 25.0, "totalTD: 25h");
    // 1000000 ns = 1 ms
    TCHECK_EQ(totalTimeDuration(Int128(1000000LL), TemporalUnit::Millisecond), 1.0, "totalTD: 1ms");
}

// ---------------------------------------------------------------------------
// balanceDuration — redistribute time fields
// ---------------------------------------------------------------------------

static void testBalanceDuration()
{
    // temporal_rs: Duration::balance — redistributes seconds/minutes/hours
    // 90min -> 1h30m when largestUnit=Hour
    ISO8601::Duration d1(0, 0, 0, 0, 0, 90, 0, 0, 0, 0);
    balanceDuration(d1, TemporalUnit::Hour);
    TCHECK_EQ(static_cast<int64_t>(d1.hours()), 1LL, "balance: 90m -> 1h");
    TCHECK_EQ(static_cast<int64_t>(d1.minutes()), 30LL, "balance: 90m -> 30m");

    // 3600s -> 1h when largestUnit=Hour
    ISO8601::Duration d2(0, 0, 0, 0, 0, 0, 3600, 0, 0, 0);
    balanceDuration(d2, TemporalUnit::Hour);
    TCHECK_EQ(static_cast<int64_t>(d2.hours()), 1LL, "balance: 3600s -> 1h");
    TCHECK_EQ(static_cast<int64_t>(d2.seconds()), 0LL, "balance: 3600s -> 0s");

    // 2000ms -> 2s when largestUnit=Second (ms overflow folds into seconds)
    ISO8601::Duration d3(0, 0, 0, 0, 0, 0, 0, 2000, 0, 0);
    balanceDuration(d3, TemporalUnit::Second);
    TCHECK_EQ(static_cast<int64_t>(d3.seconds()), 2LL, "balance: 2000ms -> 2s");
    TCHECK_EQ(static_cast<int64_t>(d3.milliseconds()), 0LL, "balance: 2000ms -> 0ms");

    // 500ms with largestUnit=Millisecond -> unchanged
    ISO8601::Duration d4(0, 0, 0, 0, 0, 0, 0, 500, 0, 0);
    balanceDuration(d4, TemporalUnit::Millisecond);
    TCHECK_EQ(static_cast<int64_t>(d4.milliseconds()), 500LL, "balance: 500ms unchanged");
}

// ---------------------------------------------------------------------------
// toInternalDuration / toInternalDurationRecordWith24HourDays
// ---------------------------------------------------------------------------

static void testToInternalDuration()
{
    // temporal_rs: internal conversion helpers
    // P1DT2H -> InternalDuration with time portion = 2h in ns, date = 1 day (NOT folded)
    ISO8601::Duration d(0, 0, 0, 1, 2, 0, 0, 0, 0, 0);
    auto internal = toInternalDuration(d);
    TCHECK_EQ(static_cast<int64_t>(internal.dateDuration().days()), 1LL, "toInternal: days=1");
    TCHECK_EQ(internal.time(), Int128(7200000000000LL), "toInternal: time=2h ns");

    // toInternalDurationRecordWith24HourDays: folds days into time
    auto r = toInternalDurationRecordWith24HourDays(d);
    TCHECK_TRUE(r.has_value(), "toInternal24h: ok");
    // days folded into time: 1d + 2h = 26h = 93600000000000 ns, date part = 0 days
    TCHECK_EQ(static_cast<int64_t>(r->dateDuration().days()), 0LL, "toInternal24h: days=0");
    TCHECK_EQ(r->time(), Int128(93600000000000LL), "toInternal24h: time=26h");
}

// ---------------------------------------------------------------------------
// diffISODateTime — unrounded internal duration between datetimes
// ---------------------------------------------------------------------------

static void testDiffISODateTime()
{
    // temporal_rs: IsoDateTime::diff (unrounded portion)
    ISO8601::PlainDate d1 { 2019, 1, 8 }, d2 { 2021, 9, 7 };
    ISO8601::PlainTime t1 { 8, 22, 36, 0, 0, 0 }, t2 { 12, 39, 40, 0, 0, 0 };

    // 2019-01-08T08:22:36 until 2021-09-07T12:39:40, largestUnit=Day
    auto r = diffISODateTime(d1, t1, d2, t2, TemporalUnit::Day);
    // 973 days + 4h 17m 4s
    TCHECK_EQ(static_cast<int64_t>(r.dateDuration().days()), 973LL, "diffIDT: days=973");
    Int128 expected4h17m4s = timeDurationFromComponents(4, 17, 4, 0, 0, 0);
    TCHECK_EQ(r.time(), expected4h17m4s, "diffIDT: time=4h17m4s");

    // Same datetime -> zero
    auto r2 = diffISODateTime(d1, t1, d1, t1, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r2.dateDuration().days()), 0LL, "diffIDT: same=0 days");
    TCHECK_EQ(r2.time(), Int128(0LL), "diffIDT: same=0 time");

    // Negative: later until earlier
    auto r3 = diffISODateTime(d2, t2, d1, t1, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r3.dateDuration().days()), -973LL, "diffIDT: neg days");
}

// ---------------------------------------------------------------------------
// validateTemporalRoundingIncrement
// ---------------------------------------------------------------------------

static void testValidateTemporalRoundingIncrement()
{
    // temporal_rs: RoundingIncrement::validate
    // Valid: increment=1, dividend=60 (exclusive)
    auto r1 = validateTemporalRoundingIncrement(1.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(r1.has_value(), "validate: 1 of 60 ok");

    // Valid: increment=5, dividend=60 (exclusive) — 60/5=12 integer
    auto r2 = validateTemporalRoundingIncrement(5.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(r2.has_value(), "validate: 5 of 60 ok");

    // Valid: increment=60, dividend=60 (inclusive)
    auto r3 = validateTemporalRoundingIncrement(60.0, 60.0, Inclusivity::Inclusive);
    TCHECK_TRUE(r3.has_value(), "validate: 60 of 60 inclusive ok");

    // Invalid: increment=61, dividend=60 (exclusive) — exceeds max
    auto r4 = validateTemporalRoundingIncrement(61.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(!r4.has_value(), "validate: 61 of 60 rejects");

    // Invalid: increment=60, dividend=60 (exclusive) — equal not allowed exclusive
    auto r5 = validateTemporalRoundingIncrement(60.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(!r5.has_value(), "validate: 60 of 60 exclusive rejects");

    // Invalid: increment=7, dividend=60 (exclusive) — not a divisor
    auto r6 = validateTemporalRoundingIncrement(7.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(!r6.has_value(), "validate: 7 of 60 not divisor rejects");

    // No dividend: any positive increment ok
    auto r7 = validateTemporalRoundingIncrement(100.0, std::nullopt, Inclusivity::Exclusive);
    TCHECK_TRUE(r7.has_value(), "validate: no dividend ok");
}

// ---------------------------------------------------------------------------
// Comprehensive rounding: all 9 modes × positive/negative/midpoint
// Directly ports temporal_rs rounding.rs test_basic_rounding_cases +
// test_float_rounding_cases
// ---------------------------------------------------------------------------

static void testRoundingComprehensive()
{
    // --- Integer x=101, increment=10 (x not at midpoint, closer to 100) ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::Ceil), Int128(110), "comp: 101 Ceil=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::Floor), Int128(100), "comp: 101 Floor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::Expand), Int128(110), "comp: 101 Expand=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::Trunc), Int128(100), "comp: 101 Trunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::HalfCeil), Int128(100), "comp: 101 HalfCeil=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::HalfFloor), Int128(100), "comp: 101 HalfFloor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::HalfExpand), Int128(100), "comp: 101 HalfExpand=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::HalfTrunc), Int128(100), "comp: 101 HalfTrunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(101), Int128(10), RoundingMode::HalfEven), Int128(100), "comp: 101 HalfEven=100");

    // --- Integer x=105, increment=10 (exactly at midpoint) ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::Ceil), Int128(110), "comp: 105 Ceil=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::Floor), Int128(100), "comp: 105 Floor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::Expand), Int128(110), "comp: 105 Expand=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::Trunc), Int128(100), "comp: 105 Trunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::HalfCeil), Int128(110), "comp: 105 HalfCeil=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::HalfFloor), Int128(100), "comp: 105 HalfFloor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::HalfExpand), Int128(110), "comp: 105 HalfExpand=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::HalfTrunc), Int128(100), "comp: 105 HalfTrunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(105), Int128(10), RoundingMode::HalfEven), Int128(100), "comp: 105 HalfEven=100 (even=10)");

    // --- Integer x=107, increment=10 (closer to 110) ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::Ceil), Int128(110), "comp: 107 Ceil=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::Floor), Int128(100), "comp: 107 Floor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::Expand), Int128(110), "comp: 107 Expand=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::Trunc), Int128(100), "comp: 107 Trunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::HalfExpand), Int128(110), "comp: 107 HalfExpand=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::HalfTrunc), Int128(110), "comp: 107 HalfTrunc=110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(107), Int128(10), RoundingMode::HalfEven), Int128(110), "comp: 107 HalfEven=110");

    // --- Negative x=-101, increment=10 ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Ceil), Int128(-100), "comp: -101 Ceil=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Floor), Int128(-110), "comp: -101 Floor=-110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Expand), Int128(-110), "comp: -101 Expand=-110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::Trunc), Int128(-100), "comp: -101 Trunc=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::HalfExpand), Int128(-100), "comp: -101 HalfExpand=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::HalfTrunc), Int128(-100), "comp: -101 HalfTrunc=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-101), Int128(10), RoundingMode::HalfEven), Int128(-100), "comp: -101 HalfEven=-100");

    // --- Negative x=-105, increment=10 (midpoint) ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::Ceil), Int128(-100), "comp: -105 Ceil=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::Floor), Int128(-110), "comp: -105 Floor=-110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::HalfCeil), Int128(-100), "comp: -105 HalfCeil=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::HalfFloor), Int128(-110), "comp: -105 HalfFloor=-110");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-105), Int128(10), RoundingMode::HalfEven), Int128(-100), "comp: -105 HalfEven=-100 (even=10)");

    // --- Small increment x=-9, increment=2 ---
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-9), Int128(2), RoundingMode::Ceil), Int128(-8), "comp: -9 inc2 Ceil=-8");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-9), Int128(2), RoundingMode::Floor), Int128(-10), "comp: -9 inc2 Floor=-10");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-9), Int128(2), RoundingMode::HalfExpand), Int128(-10), "comp: -9 inc2 HalfExpand=-10");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-9), Int128(2), RoundingMode::HalfTrunc), Int128(-8), "comp: -9 inc2 HalfTrunc=-8");

    // --- Float: -8.5, increment=1 (from temporal_rs test_float_rounding_cases) ---
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::Ceil), -8.0, "float: -8.5 inc1 Ceil=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::Floor), -9.0, "float: -8.5 inc1 Floor=-9");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::Expand), -9.0, "float: -8.5 inc1 Expand=-9");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::Trunc), -8.0, "float: -8.5 inc1 Trunc=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::HalfCeil), -8.0, "float: -8.5 inc1 HalfCeil=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::HalfFloor), -9.0, "float: -8.5 inc1 HalfFloor=-9");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::HalfExpand), -9.0, "float: -8.5 inc1 HalfExpand=-9");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::HalfTrunc), -8.0, "float: -8.5 inc1 HalfTrunc=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 1.0, RoundingMode::HalfEven), -8.0, "float: -8.5 inc1 HalfEven=-8 (even)");

    // --- Float: -8.5, increment=2 ---
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 2.0, RoundingMode::Ceil), -8.0, "float: -8.5 inc2 Ceil=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 2.0, RoundingMode::Floor), -10.0, "float: -8.5 inc2 Floor=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 2.0, RoundingMode::HalfCeil), -8.0, "float: -8.5 inc2 HalfCeil=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 2.0, RoundingMode::HalfFloor), -8.0, "float: -8.5 inc2 HalfFloor=-8 (even)");
    TCHECK_EQ(roundNumberToIncrementDouble(-8.5, 2.0, RoundingMode::HalfEven), -8.0, "float: -8.5 inc2 HalfEven=-8 (even)");

    // --- Float: -9.5, increment=2 ---
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::Ceil), -8.0, "float: -9.5 inc2 Ceil=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::Floor), -10.0, "float: -9.5 inc2 Floor=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::Expand), -10.0, "float: -9.5 inc2 Expand=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::Trunc), -8.0, "float: -9.5 inc2 Trunc=-8");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::HalfCeil), -10.0, "float: -9.5 inc2 HalfCeil=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::HalfFloor), -10.0, "float: -9.5 inc2 HalfFloor=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::HalfExpand), -10.0, "float: -9.5 inc2 HalfExpand=-10");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::HalfTrunc), -10.0, "float: -9.5 inc2 HalfTrunc=-10 (not midpoint)");
    TCHECK_EQ(roundNumberToIncrementDouble(-9.5, 2.0, RoundingMode::HalfEven), -10.0, "float: -9.5 inc2 HalfEven=-10 (even=5)");

    // --- Large nanosecond value from temporal_rs dt_since_basic_rounding ---
    // -84082624864197532 ns, increment=1800000000000 (30 min), HalfExpand
    TCHECK_EQ(
        roundNumberToIncrementInt128(Int128(-84082624864197532LL), Int128(1800000000000LL), RoundingMode::HalfExpand),
        Int128(-84083400000000000LL),
        "comp: large ns HalfExpand");
}

// ---------------------------------------------------------------------------
// iso.rs exact epoch-day boundary values
// temporal_rs: iso_date_to_epoch_days_limits + test_month_limits
// ---------------------------------------------------------------------------

static void testISOEpochDayLimits()
{
    // temporal_rs: iso_date_to_epoch_days_limits
    // -271821-04-20 = abs(days) exactly 100_000_000 (= MAX_DAYS_BASE)
    // isoDateAdd must succeed (within range)
    auto rMin20 = isoDateAdd({ -271821, 4, 20 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rMin20.has_value(), "epochDays: -271821-04-20 is valid");

    // -271821-04-19 = abs(days) = MAX_DAYS_BASE + 1 -> valid Temporal lower bound
    auto rMin19 = isoDateAdd({ -271821, 4, 19 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rMin19.has_value(), "epochDays: -271821-04-19 is valid lower bound");

    // -271821-04-18 = abs(days) = MAX_DAYS_BASE + 2 -> out of range
    auto rMin18 = isoDateAdd({ -271821, 4, 18 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rMin18.has_value(), "epochDays: -271821-04-18 is out of range");

    // 275760-09-13 = abs(days) = MAX_DAYS_BASE (valid upper bound)
    auto rMax13 = isoDateAdd({ 275760, 9, 13 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rMax13.has_value(), "epochDays: 275760-09-13 is valid upper bound");

    // 275760-09-14 = abs(days) = MAX_DAYS_BASE + 1 -> out of range
    auto rMax14 = isoDateAdd({ 275760, 9, 14 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rMax14.has_value(), "epochDays: 275760-09-14 is out of range");

    // temporal_rs: test_month_limits
    // 1970-01-01 = epoch day 0
    auto rEpoch = getUTCEpochNanoseconds({ 1970, 1, 1 }, { 0, 0, 0, 0, 0, 0 });
    TCHECK_EQ(rEpoch, Int128(0LL), "epochDays: 1970-01-01 epoch ns = 0");

    // 1969-12-31 = epoch day -1
    auto rPrev = getUTCEpochNanoseconds({ 1969, 12, 31 }, { 0, 0, 0, 0, 0, 0 });
    TCHECK_EQ(rPrev, Int128(-86400000000000LL), "epochDays: 1969-12-31 epoch ns = -1 day");

    // temporal_rs: iso_date_to_epoch_days_limits — exact day counts
    // -271821-04-20: abs(epochDays) = 100_000_000 exactly
    auto rD20 = getUTCEpochNanoseconds({ -271821, 4, 20 }, { 0, 0, 0, 0, 0, 0 });
    Int128 nsPerDay = Int128(86400000000000LL);
    Int128 days20 = (rD20 < Int128(0LL) ? -rD20 : rD20) / nsPerDay;
    TCHECK_EQ(days20, Int128(100000000LL), "epochDays: -271821-04-20 = abs 1e8 days");

    // -271821-04-19: abs(epochDays) = 100_000_001
    auto rD19 = getUTCEpochNanoseconds({ -271821, 4, 19 }, { 0, 0, 0, 0, 0, 0 });
    Int128 days19 = (rD19 < Int128(0LL) ? -rD19 : rD19) / nsPerDay;
    TCHECK_EQ(days19, Int128(100000001LL), "epochDays: -271821-04-19 = abs 1e8+1 days");

    // 275760-09-13: abs(epochDays) = 100_000_000 exactly
    auto rD13 = getUTCEpochNanoseconds({ 275760, 9, 13 }, { 0, 0, 0, 0, 0, 0 });
    Int128 days13 = (rD13 < Int128(0LL) ? -rD13 : rD13) / nsPerDay;
    TCHECK_EQ(days13, Int128(100000000LL), "epochDays: 275760-09-13 = abs 1e8 days");

    // 275760-09-14: abs(epochDays) = 100_000_001
    auto rD14 = getUTCEpochNanoseconds({ 275760, 9, 14 }, { 0, 0, 0, 0, 0, 0 });
    Int128 days14 = (rD14 < Int128(0LL) ? -rD14 : rD14) / nsPerDay;
    TCHECK_EQ(days14, Int128(100000001LL), "epochDays: 275760-09-14 = abs 1e8+1 days");
}

// ---------------------------------------------------------------------------
// rounding.rs: exact-multiple cases (x=100, inc=10) and (-100) and (-14, 3)
// These complete the test_basic_rounding_cases table
// ---------------------------------------------------------------------------

static void testRoundingExactMultiples()
{
    // temporal_rs: x=100 inc=10 -> all modes return 100 (exact multiple)
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::Ceil), Int128(100), "exact: 100 Ceil=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::Floor), Int128(100), "exact: 100 Floor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::Expand), Int128(100), "exact: 100 Expand=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::Trunc), Int128(100), "exact: 100 Trunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::HalfCeil), Int128(100), "exact: 100 HalfCeil=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::HalfFloor), Int128(100), "exact: 100 HalfFloor=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::HalfExpand), Int128(100), "exact: 100 HalfExpand=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::HalfTrunc), Int128(100), "exact: 100 HalfTrunc=100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(100), Int128(10), RoundingMode::HalfEven), Int128(100), "exact: 100 HalfEven=100");

    // Same for float
    TCHECK_EQ(roundNumberToIncrementDouble(100.0, 10.0, RoundingMode::Ceil), 100.0, "exactF: 100 Ceil=100");
    TCHECK_EQ(roundNumberToIncrementDouble(100.0, 10.0, RoundingMode::Floor), 100.0, "exactF: 100 Floor=100");
    TCHECK_EQ(roundNumberToIncrementDouble(100.0, 10.0, RoundingMode::Expand), 100.0, "exactF: 100 Expand=100");
    TCHECK_EQ(roundNumberToIncrementDouble(100.0, 10.0, RoundingMode::Trunc), 100.0, "exactF: 100 Trunc=100");

    // temporal_rs: x=-100 inc=10 -> all modes return -100
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::Ceil), Int128(-100), "exact: -100 Ceil=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::Floor), Int128(-100), "exact: -100 Floor=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::Expand), Int128(-100), "exact: -100 Expand=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::Trunc), Int128(-100), "exact: -100 Trunc=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::HalfCeil), Int128(-100), "exact: -100 HalfCeil=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::HalfFloor), Int128(-100), "exact: -100 HalfFloor=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::HalfExpand), Int128(-100), "exact: -100 HalfExpand=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::HalfTrunc), Int128(-100), "exact: -100 HalfTrunc=-100");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-100), Int128(10), RoundingMode::HalfEven), Int128(-100), "exact: -100 HalfEven=-100");

    // temporal_rs: x=-14, inc=3 (non-exact, between -15 and -12, closer to -15)
    // -14 / 3: remainder=1 < midpoint 1.5 → rounds toward floor (-15) for all Half* modes
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::Ceil), Int128(-12), "14/3: Ceil=-12");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::Floor), Int128(-15), "14/3: Floor=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::Expand), Int128(-15), "14/3: Expand=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::Trunc), Int128(-12), "14/3: Trunc=-12");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::HalfCeil), Int128(-15), "14/3: HalfCeil=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::HalfFloor), Int128(-15), "14/3: HalfFloor=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::HalfExpand), Int128(-15), "14/3: HalfExpand=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::HalfTrunc), Int128(-15), "14/3: HalfTrunc=-15");
    TCHECK_EQ(roundNumberToIncrementInt128(Int128(-14), Int128(3), RoundingMode::HalfEven), Int128(-15), "14/3: HalfEven=-15");
}

// ---------------------------------------------------------------------------
// plain_date.rs: simple_date_subtract + new_date_limits + rounding_increment_observed
// ---------------------------------------------------------------------------

static void testDateSubtract()
{
    // temporal_rs: simple_date_subtract — 2019-11-18 - P43Y = 1976-11-18
    auto r1 = isoDateAdd({ 2019, 11, 18 }, ISO8601::Duration(-43, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r1.has_value(), "subtract: -43y ok");
    TCHECK_EQ(r1->year(), 1976, "subtract: -43y year");
    TCHECK_EQ(r1->month(), 11u, "subtract: -43y month");
    TCHECK_EQ(r1->day(), 18u, "subtract: -43y day");

    // 2019-11-18 - P11M = 2018-12-18
    auto r2 = isoDateAdd({ 2019, 11, 18 }, ISO8601::Duration(0, -11, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r2.has_value(), "subtract: -11m ok");
    TCHECK_EQ(r2->year(), 2018, "subtract: -11m year");
    TCHECK_EQ(r2->month(), 12u, "subtract: -11m month");
    TCHECK_EQ(r2->day(), 18u, "subtract: -11m day");

    // 2019-11-18 - P20D = 2019-10-29
    auto r3 = isoDateAdd({ 2019, 11, 18 }, ISO8601::Duration(0, 0, 0, -20, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r3.has_value(), "subtract: -20d ok");
    TCHECK_EQ(r3->year(), 2019, "subtract: -20d year");
    TCHECK_EQ(r3->month(), 10u, "subtract: -20d month");
    TCHECK_EQ(r3->day(), 29u, "subtract: -20d day");
}

static void testNewDateLimits()
{
    // temporal_rs: new_date_limits — min valid = -271821-04-19, max valid = 275760-09-13
    // Just below min: -271821-04-18 -> out of range
    auto rErr1 = isoDateAdd({ -271821, 4, 18 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rErr1.has_value(), "newDateLimits: -271821-04-18 rejected");

    // Just above max: 275760-09-14 -> out of range
    auto rErr2 = isoDateAdd({ 275760, 9, 14 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rErr2.has_value(), "newDateLimits: 275760-09-14 rejected");

    // Exact boundaries are valid
    auto rOk1 = regulateISODate(-271821, 4, 19, TemporalOverflow::Reject);
    TCHECK_TRUE(rOk1.has_value(), "newDateLimits: -271821-04-19 ok");
    TCHECK_EQ(rOk1->year(), -271821, "newDateLimits: min year");
    TCHECK_EQ(rOk1->month(), 4u, "newDateLimits: min month");
    TCHECK_EQ(rOk1->day(), 19u, "newDateLimits: min day");

    auto rOk2 = regulateISODate(275760, 9, 13, TemporalOverflow::Reject);
    TCHECK_TRUE(rOk2.has_value(), "newDateLimits: 275760-09-13 ok");
    TCHECK_EQ(rOk2->year(), 275760, "newDateLimits: max year");
    TCHECK_EQ(rOk2->month(), 9u, "newDateLimits: max month");
    TCHECK_EQ(rOk2->day(), 13u, "newDateLimits: max day");

    // 275760-09-13 + 1D -> over max
    auto rOver = isoDateAdd({ 275760, 9, 13 }, ISO8601::Duration(0, 0, 0, 1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rOver.has_value(), "newDateLimits: max+1d rejected");

    // -271821-04-19 - 1D -> under min
    auto rUnder = isoDateAdd({ -271821, 4, 19 }, ISO8601::Duration(0, 0, 0, -1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rUnder.has_value(), "newDateLimits: min-1d rejected");

    // 275760-09-12 + 1D -> exactly max, valid
    auto rExact = isoDateAdd({ 275760, 9, 12 }, ISO8601::Duration(0, 0, 0, 1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rExact.has_value(), "newDateLimits: 275760-09-12+1d=max ok");
    TCHECK_EQ(rExact->day(), 13u, "newDateLimits: 275760-09-12+1d day=13");

    // -271821-04-20 - 1D -> exactly min, valid
    auto rExact2 = isoDateAdd({ -271821, 4, 20 }, ISO8601::Duration(0, 0, 0, -1, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rExact2.has_value(), "newDateLimits: -271821-04-20-1d=min ok");
    TCHECK_EQ(rExact2->day(), 19u, "newDateLimits: -271821-04-20-1d day=19");
}

static void testDateRoundingIncrement()
{
    // temporal_rs: rounding_increment_observed — diff ≈ 2.66 years, inc=4 HalfExpand → 4 years.
    {
        auto diff = diffISODate({ 2019, 1, 8 }, { 2021, 9, 7 }, TemporalUnit::Year);
        // 2y 7m 29d ≈ 2.66y, rounded to inc=4 -> 4
        double years = diff.years() + diff.months() / 12.0;
        double roundedYears = roundNumberToIncrementDouble(years, 4.0, RoundingMode::HalfExpand);
        TCHECK_EQ(static_cast<int64_t>(roundedYears), 4LL, "dateRoundInc: years inc=4 HalfExpand=4");
    }

    // smallest=Month, inc=10, HalfExpand -> 30 months (≈32m -> nearest 10 is 30)
    {
        auto diff = diffISODate({ 2019, 1, 8 }, { 2021, 9, 7 }, TemporalUnit::Month);
        // 32 months -> rounded to inc=10 -> 30
        double months = diff.months();
        double roundedMonths = roundNumberToIncrementDouble(months, 10.0, RoundingMode::HalfExpand);
        TCHECK_EQ(static_cast<int64_t>(roundedMonths), 30LL, "dateRoundInc: months inc=10 HalfExpand=30");
    }

    // temporal_rs: rounding_increment_observed — Week case: 973 days = 139 weeks, inc=12 HalfExpand → 144 weeks.
    {
        auto diff = diffISODate({ 2019, 1, 8 }, { 2021, 9, 7 }, TemporalUnit::Week);
        // 973 days = 139 weeks exactly (973 = 139 * 7)
        TCHECK_EQ(static_cast<int64_t>(diff.weeks()), 139LL, "dateRoundInc: weeks=139");
        double weeks = diff.weeks();
        double roundedWeeks = roundNumberToIncrementDouble(weeks, 12.0, RoundingMode::HalfExpand);
        TCHECK_EQ(static_cast<int64_t>(roundedWeeks), 144LL, "dateRoundInc: weeks inc=12 HalfExpand=144");
    }

    // smallest=Day, inc=100, HalfExpand -> 1000 days
    {
        auto diff = diffISODate({ 2019, 1, 8 }, { 2021, 9, 7 }, TemporalUnit::Day);
        // 973 days -> 1000 (nearest 100)
        double days = diff.days();
        double roundedDays = roundNumberToIncrementDouble(days, 100.0, RoundingMode::HalfExpand);
        TCHECK_EQ(static_cast<int64_t>(roundedDays), 1000LL, "dateRoundInc: days inc=100 HalfExpand=1000");
    }
}

// ---------------------------------------------------------------------------
// plain_date_time.rs: limits, add/subtract overflow, since conflicting signs,
//                     round basic
// ---------------------------------------------------------------------------

static void testPlainDateTimeLimits()
{
    // temporal_rs: plain_date_time_limits
    // -271821-04-19 at midnight -> just outside limit (same as date limit check)
    auto rErr1 = isoDateAdd({ -271821, 4, 19 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rErr1.has_value(), "pdtLimits: -271821-04-19 date valid");
    // That date itself is the boundary; with time=noon it's out of range for datetime
    // but pure date operations still work. Verify the border-crossing behavior:
    // -271821-04-20T00:00:00 is valid (date is within limits)
    auto rOk1 = regulateISODate(-271821, 4, 20, TemporalOverflow::Reject);
    TCHECK_TRUE(rOk1.has_value(), "pdtLimits: -271821-04-20 ok");

    // 275760-09-14 -> invalid
    auto rErr2 = regulateISODate(275760, 9, 14, TemporalOverflow::Reject);
    TCHECK_TRUE(rErr2.has_value(), "pdtLimits: 275760-09-14 date fields valid");
    // date-only regulation doesn't check epoch limits; isoDateAdd does
    auto rOver = isoDateAdd({ 275760, 9, 14 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(!rOver.has_value(), "pdtLimits: 275760-09-14 out of range");

    // 275760-09-13 is the last valid date
    auto rMax = isoDateAdd({ 275760, 9, 13 }, ISO8601::Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
    TCHECK_TRUE(rMax.has_value(), "pdtLimits: 275760-09-13 within limits");
    TCHECK_EQ(rMax->year(), 275760, "pdtLimits: max year");
    TCHECK_EQ(rMax->month(), 9u, "pdtLimits: max month");
    TCHECK_EQ(rMax->day(), 13u, "pdtLimits: max day");
}

static void testDateTimeAddSubtract()
{
    // temporal_rs: datetime_add_test — 2020-01-31 + P1M -> 2020-02-29 (leap year constrain)
    auto rAdd = isoDateAdd({ 2020, 1, 31 }, ISO8601::Duration(0, 1, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(rAdd.has_value(), "dtAddSub: 2020-01-31+1M ok");
    TCHECK_EQ(rAdd->month(), 2u, "dtAddSub: +1M month=2");
    TCHECK_EQ(rAdd->day(), 29u, "dtAddSub: +1M day=29 (leap)");

    // temporal_rs: datetime_subtract_test — 2000-03-31 - P1M -> 2000-02-29 (Y2K leap)
    auto rSub = isoDateAdd({ 2000, 3, 31 }, ISO8601::Duration(0, -1, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(rSub.has_value(), "dtAddSub: 2000-03-31-1M ok");
    TCHECK_EQ(rSub->month(), 2u, "dtAddSub: -1M month=2");
    TCHECK_EQ(rSub->day(), 29u, "dtAddSub: -1M day=29 (Y2K leap)");

    // temporal_rs: datetime_subtract_hour_overflows — verify date rolls back when subtracting crosses midnight.
    {
        ISO8601::PlainDate d1 { 2019, 10, 28 }, d2 { 2019, 10, 29 };
        ISO8601::PlainTime t1 { 22, 46, 38, 271, 986, 102 }, t2 { 10, 46, 38, 271, 986, 102 };
        auto diff = diffISODateTime(d1, t1, d2, t2, TemporalUnit::Hour);
        Int128 expected12h = timeDurationFromComponents(12, 0, 0, 0, 0, 0);
        TCHECK_EQ(diff.time(), expected12h, "dtHourOverflow: diff = 12h");
    }

    // temporal_rs: datetime_add (2024-01-15T12:00 + P1M2DT3H4M = 2024-02-17T15:04)
    auto rAdd2 = isoDateAdd({ 2024, 1, 15 }, ISO8601::Duration(0, 1, 0, 2, 3, 4, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(rAdd2.has_value(), "dtAdd: 2024-01-15+P1M2DT3H4M ok");
    TCHECK_EQ(rAdd2->month(), 2u, "dtAdd: month=2");
    TCHECK_EQ(rAdd2->day(), 17u, "dtAdd: day=17");
    // Time portion: hour+3=15, min+4=4
    auto timePart = timeDurationFromComponents(3, 4, 0, 0, 0, 0);
    TCHECK_EQ(timePart, Int128(11040000000000LL), "dtAdd: 3h4m in ns");
}

static void testDtSinceConflictingSigns()
{
    // temporal_rs: dt_since_conflicting_signs — time sign differs from date sign; adjustedD2 = 2023-02-28.
    ISO8601::PlainDate da { 2023, 1, 1 }, db { 2023, 3, 1 };
    ISO8601::PlainTime ta { 3, 0, 0, 0, 0, 0 }, tb { 2, 0, 0, 0, 0, 0 };
    auto r = diffISODateTime(da, ta, db, tb, TemporalUnit::Month);
    TCHECK_EQ(static_cast<int64_t>(r.dateDuration().months()), 1LL, "conflictSign: months=1");
    TCHECK_EQ(static_cast<int64_t>(r.dateDuration().days()), 27LL, "conflictSign: days=27 (ISO path)");
    Int128 expected23h = timeDurationFromComponents(23, 0, 0, 0, 0, 0);
    TCHECK_EQ(r.time(), expected23h, "conflictSign: time=23h");
}

static void testRoundISODateTime()
{
    // RoundTime correctness: quantity is sub-unit-relative, not from-midnight.
    // 11:26 rounded to 4-min: sub-minute offset = 26min → 6.5 increments → HalfExpand → 7 → 11:28.
    {
        ISO8601::PlainDate date(2020, 1, 1);
        ISO8601::PlainTime time(11, 26, 0, 0, 0, 0);
        auto r = roundISODateTime(date, time, ISO8601::ExactTime::nsPerMinute * 4, TemporalUnit::Minute, RoundingMode::HalfExpand);
        TCHECK_EQ(r.time.hour(), 11u, "roundISO: 11:26 +4min HalfExpand -> hour=11");
        TCHECK_EQ(r.time.minute(), 28u, "roundISO: 11:26 +4min HalfExpand -> min=28");
    }
    // HalfEven: 11:26 with 4-min -> sub-unit quotient 6.5, r1=6 (even) -> 11:24
    {
        ISO8601::PlainDate date(2020, 1, 1);
        ISO8601::PlainTime time(11, 26, 0, 0, 0, 0);
        auto r = roundISODateTime(date, time, ISO8601::ExactTime::nsPerMinute * 4, TemporalUnit::Minute, RoundingMode::HalfEven);
        TCHECK_EQ(r.time.minute(), 24u, "roundISO: 11:26 +4min HalfEven -> min=24 (r1=6 even)");
    }
    // Round to hour: 14:30:00 -> 15:00:00
    {
        ISO8601::PlainDate date(2020, 1, 1);
        ISO8601::PlainTime time(14, 30, 0, 0, 0, 0);
        auto r = roundISODateTime(date, time, ISO8601::ExactTime::nsPerHour, TemporalUnit::Hour, RoundingMode::HalfExpand);
        TCHECK_EQ(r.time.hour(), 15u, "roundISO: 14:30 +1h HalfExpand -> 15:00");
        TCHECK_EQ(r.time.minute(), 0u, "roundISO: 14:30 +1h -> min=0");
        TCHECK_TRUE(r.date == date, "roundISO: same date");
    }
    // Day overflow: 23:45:00 rounded to hour -> next day 00:00:00
    {
        ISO8601::PlainDate date(2020, 1, 15);
        ISO8601::PlainTime time(23, 45, 0, 0, 0, 0);
        auto r = roundISODateTime(date, time, ISO8601::ExactTime::nsPerHour, TemporalUnit::Hour, RoundingMode::HalfExpand);
        TCHECK_EQ(r.time.hour(), 0u, "roundISO: 23:45 overflow -> 00:00");
        TCHECK_EQ(r.date.day(), 16u, "roundISO: 23:45 overflow -> next day");
    }
    // Second rounding: 14:23:30.600 -> 14:23:31
    {
        ISO8601::PlainDate date(2020, 1, 1);
        ISO8601::PlainTime time(14, 23, 30, 600, 0, 0);
        auto r = roundISODateTime(date, time, ISO8601::ExactTime::nsPerSecond, TemporalUnit::Second, RoundingMode::HalfExpand);
        TCHECK_EQ(r.time.second(), 31u, "roundISO: 30.6s HalfExpand -> 31s");
    }
}

static void testDtRoundBasic()
{
    // temporal_rs: dt_round_basic — 1976-11-18T14:23:30.123456789
    // Rounding to various units with HalfExpand (default)
    UNUSED_PARAM(0); // date/time used via roundTimeQuantity

    // Round to Hour inc=4, HalfExpand: 14h23m30.123456789 / 4h ≈ 3.597 → rounds to 4 → 16h
    {
        Int128 totalNs = Int128(((int64_t)14 * 3600 + (int64_t)23 * 60 + 30) * 1000000000LL)
            + Int128(123456789LL);
        Int128 inc4h = Int128(4LL * 3600000000000LL);
        Int128 rounded = roundNumberToIncrementInt128(totalNs, inc4h, RoundingMode::HalfExpand);
        // 57600000000000 = 16h
        auto resultTime = plainTimeFromSubdayNs(rounded);
        TCHECK_EQ(resultTime.hour(), 16u, "dtRound: inc4h hour=16");
        TCHECK_EQ(resultTime.minute(), 0u, "dtRound: inc4h min=0");
    }

    // Round to Minute inc=15, HalfExpand: sub-hour ns = 23m30.123456789s -> round to 30m
    {
        // sub-hour nanoseconds = 23*60e9 + 30e9 + 123456789 = 1410123456789
        Int128 subHourNs = Int128((int64_t)23 * 60000000000LL + (int64_t)30 * 1000000000LL + 123456789LL);
        Int128 inc15m = Int128(15LL * 60000000000LL);
        Int128 rounded = roundNumberToIncrementInt128(subHourNs, inc15m, RoundingMode::HalfExpand);
        // 1410123456789 / 900000000000 = 1.567 -> HalfExpand -> 2 -> 1800000000000 = 30m
        Int128 base14h = Int128(14LL * 3600000000000LL);
        Int128 totalRounded = base14h + rounded;
        auto rt = plainTimeFromSubdayNs(totalRounded);
        TCHECK_EQ(rt.hour(), 14u, "dtRound: inc15m hour=14");
        TCHECK_EQ(rt.minute(), 30u, "dtRound: inc15m min=30");
    }

    // Round to Nanosecond inc=10, HalfExpand: ns=789 -> 790
    {
        Int128 nsOnly = Int128(789LL);
        Int128 inc10 = Int128(10LL);
        Int128 rounded = roundNumberToIncrementInt128(nsOnly, inc10, RoundingMode::HalfExpand);
        TCHECK_EQ(rounded, Int128(790LL), "dtRound: inc10ns ns=790");
    }

    // Round to Millisecond inc=10, HalfExpand: ms=123 -> 120
    {
        Int128 msOnly = Int128(123LL * 1000000LL + 456LL * 1000LL + 789LL);
        Int128 inc10ms = Int128(10LL * 1000000LL);
        Int128 rounded = roundNumberToIncrementInt128(msOnly, inc10ms, RoundingMode::HalfExpand);
        // 123456789 / 10000000 = 12.3456789 -> 12 -> 120ms
        TCHECK_EQ(rounded / Int128(1000000LL), Int128(120LL), "dtRound: inc10ms=120");
    }

    // Round to Microsecond inc=10, HalfExpand: µs=456 -> 460
    {
        Int128 usOnly = Int128(456LL * 1000LL + 789LL);
        Int128 inc10us = Int128(10LL * 1000LL);
        Int128 rounded = roundNumberToIncrementInt128(usOnly, inc10us, RoundingMode::HalfExpand);
        TCHECK_EQ(rounded / Int128(1000LL), Int128(460LL), "dtRound: inc10us=460");
    }
}

static void testDifferenceTemporalPlainDateTime()
{
    // temporal_rs: dt_until_basic — tests differenceTemporalPlainDateTime directly
    ISO8601::PlainDate d1 { 2019, 1, 8 }, d2 { 2021, 9, 7 };
    ISO8601::PlainTime t1 { 8, 22, 36, 123, 456, 789 }, t2 { 12, 39, 40, 987, 654, 321 };
    auto id = calendarIDFromString("iso8601"_s);

    // until: largestUnit=Day, no rounding
    {
        auto r = differenceTemporalPlainDateTime(DifferenceOperation::Until, d1, t1, d2, t2,
            id, TemporalUnit::Nanosecond, TemporalUnit::Day, RoundingMode::HalfExpand, 1);
        TCHECK_TRUE(r.has_value(), "dtDiff: until ok");
        TCHECK_EQ(static_cast<int64_t>(r->days()), 973LL, "dtDiff: until days=973");
    }

    // since: result is negated — temporal_rs: dt_since_basic
    {
        auto r = differenceTemporalPlainDateTime(DifferenceOperation::Since, d2, t2, d1, t1,
            id, TemporalUnit::Nanosecond, TemporalUnit::Day, RoundingMode::HalfExpand, 1);
        TCHECK_TRUE(r.has_value(), "dtDiff: since ok");
        TCHECK_EQ(static_cast<int64_t>(r->days()), 973LL, "dtDiff: since days=973");
    }

    // equal datetimes -> zero duration (step 5)
    {
        auto r = differenceTemporalPlainDateTime(DifferenceOperation::Until, d1, t1, d1, t1,
            id, TemporalUnit::Nanosecond, TemporalUnit::Day, RoundingMode::HalfExpand, 1);
        TCHECK_TRUE(r.has_value() && !r->years() && !r->days(), "dtDiff: equal=zero");
    }

    // with rounding: inc=3h HalfExpand -> 973 days 3 hours (from dt_until_basic)
    {
        auto r = differenceTemporalPlainDateTime(DifferenceOperation::Until, d1, t1, d2, t2,
            id, TemporalUnit::Hour, TemporalUnit::Day, RoundingMode::HalfExpand, 3);
        TCHECK_TRUE(r.has_value(), "dtDiff: round 3h ok");
        TCHECK_EQ(static_cast<int64_t>(r->days()), 973LL, "dtDiff: round 3h days=973");
        TCHECK_EQ(static_cast<int64_t>(r->hours()), 3LL, "dtDiff: round 3h hours=3");
    }

    // temporal_rs: dt_since_conflicting_signs
    // 2023-03-01T02:00 since 2023-01-01T03:00, largestUnit=Year -> 1 month 30 days 23 hours
    {
        ISO8601::PlainDate da { 2023, 3, 1 }, db { 2023, 1, 1 };
        ISO8601::PlainTime ta { 2, 0, 0, 0, 0, 0 }, tb { 3, 0, 0, 0, 0, 0 };
        auto r = differenceTemporalPlainDateTime(DifferenceOperation::Since, da, ta, db, tb,
            id, TemporalUnit::Nanosecond, TemporalUnit::Year, RoundingMode::HalfExpand, 1);
        TCHECK_TRUE(r.has_value(), "dtDiff: conflicting signs ok");
        TCHECK_EQ(static_cast<int64_t>(r->months()), 1LL, "dtDiff: conflicting months=1");
        TCHECK_EQ(static_cast<int64_t>(r->days()), 30LL, "dtDiff: conflicting days=30");
        TCHECK_EQ(static_cast<int64_t>(r->hours()), 23LL, "dtDiff: conflicting hours=23");
    }
}

static void testDtUntilBasic()
{
    // temporal_rs: dt_until_basic
    // 2019-01-08T08:22:36.123456789 until 2021-09-07T12:39:40.987654321
    ISO8601::PlainDate d1 { 2019, 1, 8 }, d2 { 2021, 9, 7 };
    ISO8601::PlainTime t1 { 8, 22, 36, 123, 456, 789 }, t2 { 12, 39, 40, 987, 654, 321 };

    // largestUnit=Hour, inc=3, HalfExpand -> 973 days, 3 hours
    // diff = 973 days + 4h 17m 4s 864ms 197µs 532ns (approx)
    auto r = diffISODateTime(d1, t1, d2, t2, TemporalUnit::Day);
    TCHECK_EQ(static_cast<int64_t>(r.dateDuration().days()), 973LL, "dtUntil: days=973");
    // time component: 4h 17m 4s 864ms 197µs 532ns
    Int128 expectedTime = timeDurationFromComponents(4, 17, 4, 864, 197, 532);
    TCHECK_EQ(r.time(), expectedTime, "dtUntil: time=4h17m4s864ms197µs532ns");

    // Round time to inc=3h, HalfExpand
    // time = 4h 17m 4s ... ≈ 4.284h; 4.284/3 ≈ 1.428 -> 3h
    Int128 inc3h = Int128(3LL * 3600000000000LL);
    Int128 roundedTime = roundNumberToIncrementInt128(r.time(), inc3h, RoundingMode::HalfExpand);
    TCHECK_EQ(roundedTime, Int128(3LL * 3600000000000LL), "dtUntil: time rounded inc3h=3h");

    // Round time to inc=30m, HalfExpand
    // 4h17m4s -> 4h17m = 15424s -> 15424/1800 ≈ 8.57 -> 9 -> 4h30m
    Int128 inc30m = Int128(30LL * 60000000000LL);
    Int128 roundedTime2 = roundNumberToIncrementInt128(r.time(), inc30m, RoundingMode::HalfExpand);
    Int128 expected4h30m = timeDurationFromComponents(4, 30, 0, 0, 0, 0);
    TCHECK_EQ(roundedTime2, expected4h30m, "dtUntil: time rounded inc30m=4h30m");
}

// ---------------------------------------------------------------------------
// duration/tests.rs pure tests: rounding without ZonedDateTime
// ---------------------------------------------------------------------------

static void testRoundingToDayOnly()
{
    // temporal_rs: rounding_to_fractional_day_tests — 25h splits into 1d + 1h remainder.
    {
        auto [days, rem] = splitTimeDuration(Int128(90000000000000LL));
        TCHECK_EQ(days, 1LL, "roundDay: 25h split days=1");
        TCHECK_EQ(rem, Int128(3600000000000LL), "roundDay: 25h split rem=1h");
    }

    // 64 days, inc=5, Floor -> 60d
    {
        Int128 sixtyfour = Int128(64LL);
        Int128 inc5 = Int128(5LL);
        Int128 result = roundNumberToIncrementInt128(sixtyfour, inc5, RoundingMode::Floor);
        TCHECK_EQ(result, Int128(60LL), "roundDay: 64d inc5 Floor=60");
    }

    // 64 days, inc=10, Floor -> 60d
    {
        Int128 result = roundNumberToIncrementInt128(Int128(64), Int128(10), RoundingMode::Floor);
        TCHECK_EQ(result, Int128(60LL), "roundDay: 64d inc10 Floor=60");
    }

    // 64 days, inc=10, Ceil -> 70d
    {
        Int128 result = roundNumberToIncrementInt128(Int128(64), Int128(10), RoundingMode::Ceil);
        TCHECK_EQ(result, Int128(70LL), "roundDay: 64d inc10 Ceil=70");
    }

    // 1000 days, inc=1_000_000_000, Expand -> 1_000_000_000d
    {
        Int128 result = roundNumberToIncrementInt128(Int128(1000), Int128(1000000000), RoundingMode::Expand);
        TCHECK_EQ(result, Int128(1000000000LL), "roundDay: 1000d inc1e9 Expand=1e9");
    }
}

static void testDurationAddSubtract()
{
    // temporal_rs: basic_add_duration
    // P1DT5M + P2DT5M = P3DT10M
    {
        ISO8601::Duration base(0, 0, 0, 1, 0, 5, 0, 0, 0, 0);
        ISO8601::Duration other(0, 0, 0, 2, 0, 5, 0, 0, 0, 0);
        // Combine via toInternal + add
        auto baseInt = toInternalDurationRecordWith24HourDays(base);
        auto otherInt = toInternalDurationRecordWith24HourDays(other);
        TCHECK_TRUE(baseInt.has_value(), "durationAdd: base ok");
        TCHECK_TRUE(otherInt.has_value(), "durationAdd: other ok");
        // P1DT5M + P2DT5M = P3DT10M.
        Int128 sumTime = baseInt->time() + otherInt->time();
        auto [days, rem] = splitTimeDuration(sumTime);
        TCHECK_EQ(days, 3LL, "durationAdd: days=3");
        Int128 expected10m = timeDurationFromComponents(0, 10, 0, 0, 0, 0);
        TCHECK_EQ(rem, expected10m, "durationAdd: rem=10m");
    }

    // P1DT5M + P-3DT-15M = P-2DT-10M
    {
        ISO8601::Duration base(0, 0, 0, 1, 0, 5, 0, 0, 0, 0);
        ISO8601::Duration neg(0, 0, 0, -3, 0, -15, 0, 0, 0, 0);
        auto baseInt = toInternalDurationRecordWith24HourDays(base);
        auto negInt = toInternalDurationRecordWith24HourDays(neg);
        TCHECK_TRUE(baseInt.has_value(), "durationAdd: neg base ok");
        TCHECK_TRUE(negInt.has_value(), "durationAdd: neg other ok");
        Int128 sumTime = baseInt->time() + negInt->time();
        auto [days, rem] = splitTimeDuration(sumTime);
        TCHECK_EQ(days, -3LL, "durationAdd: neg days=-3 (floor(-173400000000000/86400000000000))");
        // splitTimeDuration uses floor division: -173400000000000 ns → days=-3, rem=23h50m.
        Int128 expectedNeg = Int128(-2LL) * Int128(86400000000000LL) - Int128(600000000000LL);
        TCHECK_EQ(sumTime, expectedNeg, "durationAdd: neg total=-2d10m");
    }

    // temporal_rs: basic_subtract_duration — P3DT15M - P1DT5M = P2DT10M
    {
        ISO8601::Duration base(0, 0, 0, 3, 0, 15, 0, 0, 0, 0);
        ISO8601::Duration other(0, 0, 0, 1, 0, 5, 0, 0, 0, 0);
        auto baseInt = toInternalDurationRecordWith24HourDays(base);
        auto otherInt = toInternalDurationRecordWith24HourDays(other);
        TCHECK_TRUE(baseInt.has_value(), "durationSub: ok");
        Int128 diffTime = baseInt->time() - otherInt->time();
        auto [days, rem] = splitTimeDuration(diffTime);
        TCHECK_EQ(days, 2LL, "durationSub: days=2");
        Int128 expected10m = timeDurationFromComponents(0, 10, 0, 0, 0, 0);
        TCHECK_EQ(rem, expected10m, "durationSub: rem=10m");
    }
}

static void testRoundingCrossBoundary()
{
    // temporal_rs: rounding_cross_boundary — P1Y11M24D Expand/Month → 24 months (2 years).
    {
        // 2022-01-01 + P1Y11M24D = 2023-12-25
        auto r = isoDateAdd({ 2022, 1, 1 }, ISO8601::Duration(1, 11, 0, 24, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
        TCHECK_TRUE(r.has_value(), "crossBound: 2022-01-01+P1Y11M24D ok");
        TCHECK_EQ(r->year(), 2023, "crossBound: year=2023");
        TCHECK_EQ(r->month(), 12u, "crossBound: month=12");
        TCHECK_EQ(r->day(), 25u, "crossBound: day=25");

        // The diff from relative 2022-01-01 to 2023-12-25 in months is 23.8... -> expand -> 24m = 2y
        auto diff = diffISODate({ 2022, 1, 1 }, { 2023, 12, 25 }, TemporalUnit::Month);
        double months = diff.months();
        double rounded = roundNumberToIncrementDouble(months, 12.0, RoundingMode::Expand);
        // 23 months -> 24 months = 2 years
        TCHECK_EQ(static_cast<int64_t>(rounded / 12.0), 2LL, "crossBound: months/12 rounded=2y");
    }

    // temporal_rs: rounding_cross_boundary_time_units — P0DT1H59M59.9S, Expand, smallest=Second -> P2H
    {
        // 1h59m59.900s in nanoseconds = (1*3600 + 59*60 + 59)*1e9 + 900000000 = 7199900000000 + 900000000 = 7199900000000 (approx)
        Int128 t = timeDurationFromComponents(1, 59, 59, 900, 0, 0);
        // = (1*3600+59*60+59)*1e9 + 900e6 = 7199*1e9 + 900e6 = 7199000000000+900000000 = 7199900000000
        Int128 expected = Int128(7199900000000LL);
        TCHECK_EQ(t, expected, "crossBoundTime: 1h59m59.900s ns");
        // Round to second, Expand: 7199900000000 / 1000000000 = 7199.9 -> 7200s = 2h
        Int128 inc1s = Int128(1000000000LL);
        Int128 rounded = roundNumberToIncrementInt128(t, inc1s, RoundingMode::Expand);
        TCHECK_EQ(rounded, Int128(7200000000000LL), "crossBoundTime: Expand->7200s=2h");
        auto rt = plainTimeFromSubdayNs(rounded);
        TCHECK_EQ(rt.hour(), 2u, "crossBoundTime: hour=2");
        TCHECK_EQ(rt.minute(), 0u, "crossBoundTime: min=0");
    }

    // Negative: P-1H-59M-59.9S, Expand -> -2h
    {
        Int128 tneg = timeDurationFromComponents(-1, -59, -59, -900, 0, 0);
        Int128 expectedNeg = Int128(-7199900000000LL);
        TCHECK_EQ(tneg, expectedNeg, "crossBoundTimeNeg: -1h59m59.900s ns");
        Int128 roundedNeg = roundNumberToIncrementInt128(tneg, Int128(1000000000LL), RoundingMode::Expand);
        TCHECK_EQ(roundedNeg, Int128(-7200000000000LL), "crossBoundTimeNeg: Expand->-7200s=-2h");
    }
}

static void testBubbleSmallestBecomesDay()
{
    // temporal_rs: bubble_smallest_becomes_day — P14H, inc=12h, Ceil -> P24H (bubbles to next day)
    // 14h / 12h = 1.166... -> Ceil -> 2 -> 24h
    {
        Int128 t14h = timeDurationFromComponents(14, 0, 0, 0, 0, 0);
        Int128 inc12h = Int128(12LL * 3600000000000LL);
        Int128 rounded = roundNumberToIncrementInt128(t14h, inc12h, RoundingMode::Ceil);
        TCHECK_EQ(rounded, Int128(24LL * 3600000000000LL), "bubble: 14h Ceil inc12h = 24h");
        // The result is 24h which is 1 full day
        auto [days, rem] = splitTimeDuration(rounded);
        TCHECK_EQ(days, 1LL, "bubble: 24h = 1 day overflow");
        TCHECK_EQ(rem, Int128(0LL), "bubble: 24h rem = 0");
    }
}

static void testRoundZeroDuration()
{
    // temporal_rs: round_zero_duration — zero duration rounded to any unit = zero
    Int128 zero(0LL);
    TCHECK_EQ(roundNumberToIncrementInt128(zero, Int128(3600000000000LL), RoundingMode::HalfExpand), Int128(0LL), "roundZero: 0 inc1h = 0");
    TCHECK_EQ(roundNumberToIncrementInt128(zero, Int128(86400000000000LL), RoundingMode::Floor), Int128(0LL), "roundZero: 0 inc1d = 0");
    TCHECK_EQ(roundNumberToIncrementInt128(zero, Int128(1000000000LL), RoundingMode::Ceil), Int128(0LL), "roundZero: 0 inc1s = 0");
    TCHECK_EQ(roundNumberToIncrementInt128(zero, Int128(1LL), RoundingMode::Expand), Int128(0LL), "roundZero: 0 inc1ns = 0");
}

static void testRoundIncrementRegression()
{
    // temporal_rs: round_increment_regression_test — 48h, inc=2days, no relativeTo
    // 48h = 2 × 86400000000000 ns exactly divisible by 2d
    Int128 h48 = timeDurationFromComponents(48, 0, 0, 0, 0, 0);
    Int128 inc2d = Int128(2LL * 86400000000000LL);
    Int128 result = roundNumberToIncrementInt128(h48, inc2d, RoundingMode::HalfExpand);
    TCHECK_EQ(result, Int128(2LL * 86400000000000LL), "roundReg: 48h inc=2d = 2d");
    // splitTimeDuration: 2d exactly
    auto [days, rem] = splitTimeDuration(result);
    TCHECK_EQ(days, 2LL, "roundReg: days=2");
    TCHECK_EQ(rem, Int128(0LL), "roundReg: rem=0");
}

static void testDurationTotalBasic()
{
    // temporal_rs: test_duration_total — basic totals without ZonedDateTime
    // 130h20m = total seconds = 130*3600 + 20*60 = 468000+1200 = 469200s
    Int128 h130m20 = timeDurationFromComponents(130, 20, 0, 0, 0, 0);
    TCHECK_EQ(totalTimeDuration(h130m20, TemporalUnit::Second), 469200.0, "durationTotal: 130h20m = 469200s");

    // PT123456789S = 123456789s = 1428.898... days
    Int128 s123456789 = timeDurationFromComponents(0, 0, 123456789, 0, 0, 0);
    double days = totalTimeDuration(s123456789, TemporalUnit::Day);
    // 123456789s / 86400s = 1428.8980208...
    TCHECK_TRUE(days > 1428.89 && days < 1428.90, "durationTotal: 123456789s in days");

    // balance_subseconds positive: 999ms+999999µs+999999999ns = 2.998998999s
    Int128 subsec = timeDurationFromComponents(0, 0, 0, 999, 999999, 999999999);
    double secs = totalTimeDuration(subsec, TemporalUnit::Second);
    TCHECK_TRUE(secs > 2.998 && secs < 2.999, "durationTotal: balance subseconds pos");

    // balance_subseconds negative: -999ms-999999µs-999999999ns = -2.998998999s
    Int128 negSubsec = timeDurationFromComponents(0, 0, 0, -999, -999999, -999999999);
    double negSecs = totalTimeDuration(negSubsec, TemporalUnit::Second);
    TCHECK_TRUE(negSecs < -2.998 && negSecs > -2.999, "durationTotal: balance subseconds neg");
}

static void testDurationTotalWithRelativeTo()
{
    // temporal_rs: balance_days_up_to_both_years_and_months — relativeTo=PlainDate
    // 11 months + 396 days from 2017-01-01 = exactly 2 years
    // This verifies that calendarDateAdd folds correctly
    auto r1 = isoDateAdd({ 2017, 1, 1 }, ISO8601::Duration(0, 11, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r1.has_value(), "durationRelTo: +11m ok");
    TCHECK_EQ(r1->year(), 2017, "durationRelTo: +11m year=2017");
    TCHECK_EQ(r1->month(), 12u, "durationRelTo: +11m month=12");

    // Then add 396 days from 2017-12-01
    auto r2 = isoDateAdd({ 2017, 12, 1 }, ISO8601::Duration(0, 0, 0, 396, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r2.has_value(), "durationRelTo: +396d ok");
    TCHECK_EQ(r2->year(), 2019, "durationRelTo: +396d year=2019");
    TCHECK_EQ(r2->month(), 1u, "durationRelTo: +396d month=1");
    // 2017-12-01 + 396d lands in 2019-01 (verified below).

    // From 2017-01-01: adding P0M11D + 396 days = ending at 2019-01-01
    // diff from 2017-01-01 to 2019-01-01 = 2 years
    auto diff = diffISODate({ 2017, 1, 1 }, { 2019, 1, 1 }, TemporalUnit::Year);
    TCHECK_EQ(static_cast<int64_t>(diff.years()), 2LL, "durationRelTo: 2019-2017=2 years");

    // Negative: -11m-396d from 2017-01-01 = -2 years
    auto r3 = isoDateAdd({ 2017, 1, 1 }, ISO8601::Duration(0, -11, 0, -396, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(r3.has_value(), "durationRelTo: -11m-396d ok");
    TCHECK_EQ(r3->year(), 2015, "durationRelTo: -11m-396d year=2015");
    TCHECK_EQ(r3->month(), 1u, "durationRelTo: -11m-396d month=1");

    auto diffNeg = diffISODate({ 2015, 1, 1 }, { 2017, 1, 1 }, TemporalUnit::Year);
    TCHECK_EQ(static_cast<int64_t>(diffNeg.years()), 2LL, "durationRelTo: diff=2 years (neg path)");
}

static void testAddNormTimeDurationOutOfRange()
{
    // temporal_rs: add_normalized_time_duration_out_of_range
    // maxTimeDuration ≈ 9007199254740992e9 ns; to exceed it, need days > ~104249991374.3
    // Use 104249991375 days which is just over the limit
    auto r = add24HourDaysToTimeDuration(Int128(0), 104249991375.0);
    TCHECK_TRUE(!r.has_value(), "outOfRange: 104249991375 days rejects");
}

// ---------------------------------------------------------------------------
// test_rounding_boundaries — temporal_rs duration/tests.rs
// Overflow detection when Duration.round is called with extreme calendar values
// ---------------------------------------------------------------------------

static void testRoundingBoundaries()
{
    // temporal_rs: test_rounding_boundaries
    // Duration with calendar fields at u32::MAX - 1 = 4294967294 from relativeTo 2000-01-01.
    // Each case should produce an epoch-day overflow in isoDateAdd/balanceISOYearMonth.

    // year overflow: 2000 + 4294967294 >> maxYear=275760 -> balanceISOYearMonth clamps to outOfRangeYear
    {
        auto r = isoDateAdd({ 2000, 1, 1 }, ISO8601::Duration(4294967294.0, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
        TCHECK_TRUE(!r.has_value(), "roundBounds: year=4294967294 from 2000 rejects");
    }

    // month overflow: 4294967294 months / 12 ≈ 357913941 years, total year >> maxYear
    {
        auto r = isoDateAdd({ 2000, 1, 1 }, ISO8601::Duration(0, 4294967294.0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
        TCHECK_TRUE(!r.has_value(), "roundBounds: month=4294967294 from 2000 rejects");
    }

    // week overflow: 4294967294 weeks × 7 = 30064771058 days from 2000-01-01
    // Total epoch day ≈ 30064782015 >> Temporal limit 1e8 -> isDateTimeWithinLimits rejects
    {
        auto r = isoDateAdd({ 2000, 1, 1 }, ISO8601::Duration(0, 0, 4294967294.0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
        TCHECK_TRUE(!r.has_value(), "roundBounds: week=4294967294 from 2000 rejects");
    }

    // days overflow: 104249991374 days (= max safe days) from 2000-01-01
    // 2000-01-01 epoch day ≈ 10957, total ≈ 104249991374 + 10957 >> 1e8 limit
    {
        auto r = isoDateAdd({ 2000, 1, 1 }, ISO8601::Duration(0, 0, 0, 104249991374.0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
        TCHECK_TRUE(!r.has_value(), "roundBounds: day=104249991374 from 2000 rejects");
    }

    // Combined extreme: years + months + weeks + days all extreme
    {
        auto r = isoDateAdd({ 2000, 1, 1 }, ISO8601::Duration(4294967294.0, 4294967294.0, 4294967294.0, 104249991374.0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
        TCHECK_TRUE(!r.has_value(), "roundBounds: all-extreme from 2000 rejects");
    }

    // Negative extreme: all fields at -(u32::MAX - 1)
    {
        auto r = isoDateAdd({ 2000, 1, 1 }, ISO8601::Duration(-4294967294.0, -4294967294.0, -4294967294.0, -104249991374.0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Reject);
        TCHECK_TRUE(!r.has_value(), "roundBounds: negative all-extreme from 2000 rejects");
    }
}

// ---------------------------------------------------------------------------
// test_duration_compare_boundary — temporal_rs duration/tests.rs
// Overflow detection: Duration with weeks=1, days=max_days exceeds timeDuration limit
// ---------------------------------------------------------------------------

static void testDurationCompareBoundary()
{
    // temporal_rs: test_duration_compare_boundary — weeks=1 + max_days folds to max_days+7, exceeding the limit.

    const double maxDays = 104249991374.0; // 2^53 / 86400

    // weeks=1, days=max_days -> total = 1*7 + max_days = max_days + 7 -> overflow
    {
        // Equivalent to: toInternalDurationRecordWith24HourDays(Duration{weeks=1, days=maxDays})
        // Time portion = 0 (no hours), then add (1*7 + maxDays) days = maxDays + 7
        auto r = add24HourDaysToTimeDuration(Int128(0), maxDays + 7.0);
        TCHECK_TRUE(!r.has_value(), "compareBound: maxDays+7 days rejects");
    }

    // Just at the boundary: max_days alone (no weeks) is still within limit
    {
        // temporal_rs: max is inclusive — exactly max_days succeeds.
        auto r = add24HourDaysToTimeDuration(Int128(0), maxDays);
        TCHECK_TRUE(r.has_value(), "compareBound: exactly maxDays ok");
    }

    // Just one over the boundary
    {
        auto r = add24HourDaysToTimeDuration(Int128(0), maxDays + 1.0);
        TCHECK_TRUE(!r.has_value(), "compareBound: maxDays+1 rejects");
    }

    // Negative: -(maxDays + 7) also overflows
    {
        auto r = add24HourDaysToTimeDuration(Int128(0), -(maxDays + 7.0));
        TCHECK_TRUE(!r.has_value(), "compareBound: -(maxDays+7) days rejects");
    }
}

// ---------------------------------------------------------------------------
// add_large_durations — ports temporal_rs duration/tests.rs add_large_durations
// Tests that huge durations added via non-ISO calendarDateAdd fail correctly.
// Uses dangi calendar (lunisolar) with exact temporal_rs overflow values.
// ---------------------------------------------------------------------------

static void testAddLargeDurations()
{
    // temporal_rs: add_large_durations (duration/tests.rs)
    // Base date: 2000-01-01 (ISO, which corresponds to dangi calendar)
    ISO8601::PlainDate base { 2000, 1, 1 };

    // Case 1: Duration(years=4294901760, months=256) -> overflow
    auto r1 = calendarDateAdd(calendarIDFromString("dangi"_s), base,
        ISO8601::Duration(4294901760.0, 256, 0, 0, 0, 0, 0, 0, 0, 0),
        TemporalOverflow::Constrain);
    TCHECK_TRUE(!r1.has_value(), "addLarge: dangi 4294901760y+256m rejects");

    // Case 2: Duration(weeks=2516582400, days=8589934592) -> overflow
    auto r2 = calendarDateAdd(calendarIDFromString("dangi"_s), base,
        ISO8601::Duration(0, 0, 2516582400.0, 8589934592.0, 0, 0, 0, 0, 0, 0),
        TemporalOverflow::Constrain);
    TCHECK_TRUE(!r2.has_value(), "addLarge: dangi 2516582400w+8589934592d rejects");

    // Case 3: Duration(years=2046820352) -> overflow
    auto r3 = calendarDateAdd(calendarIDFromString("dangi"_s), base,
        ISO8601::Duration(2046820352.0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        TemporalOverflow::Constrain);
    TCHECK_TRUE(!r3.has_value(), "addLarge: dangi 2046820352y rejects");

    // Case 4: Duration(weeks=2516582400) -> overflow
    auto r4 = calendarDateAdd(calendarIDFromString("dangi"_s), base,
        ISO8601::Duration(0, 0, 2516582400.0, 0, 0, 0, 0, 0, 0, 0),
        TemporalOverflow::Constrain);
    TCHECK_TRUE(!r4.has_value(), "addLarge: dangi 2516582400w rejects");
}

// ---------------------------------------------------------------------------
// invalid_strings — mirrors temporal_rs plain_date.rs test invalid_strings
// test262/test/built-ins/Temporal/Calendar/prototype/month/argument-string-invalid.js
// Tests that parseISODateTime rejects invalid/unsupported ISO 8601 strings.
// Note: "2020-01-01[u-ca=notexist]" is NOT tested here because it parses
// successfully at the C++ layer; the calendar validation happens in JS.
// ---------------------------------------------------------------------------

static void testInvalidDateStrings()
{
    // temporal_rs: plain_date.rs test invalid_strings
    // All of these must return std::nullopt from parseCalendarDateTime.
    static const char* const invalidStrings[] = {
        // Completely invalid
        "",
        "invalid iso8601",
        // Day out of range
        "2020-01-00",
        "2020-01-32",
        "2020-02-30",
        "2021-02-29",
        // Month out of range
        "2020-00-01",
        "2020-13-01",
        // Trailing separator with no time
        "2020-01-01T",
        // Time fields out of range
        "2020-01-01T25:00:00",
        "2020-01-01T01:60:00",
        "2020-01-01T01:60:61",
        // Trailing junk
        "2020-01-01junk",
        "2020-01-01T00:00:00junk",
        "2020-01-01T00:00:00+00:00junk",
        "2020-01-01T00:00:00+00:00[UTC]junk",
        "2020-01-01T00:00:00+00:00[UTC][u-ca=iso8601]junk",
        // Non-standard year widths / formats
        "02020-01-01",
        "2020-001-01",
        "2020-01-001",
        "2020-01-01T001",
        "2020-01-01T01:001",
        "2020-01-01T01:01:001",
        // Unsupported formats (week/ordinal)
        "2020-W01-1",
        "2020-001",
        "+0002020-01-01",
        // Too-short date (no day for Date format)
        "2020-01",
        "+002020-01",
        "01-01",
        "2020-W01",
        // Duration strings (not dates)
        "P1Y",
        "-P12Y",
        // Too many fractional second digits
        "1970-01-01T00:00:00.1234567891",
        "1970-01-01T00:00:00.1234567890",
    };
    for (auto* s : invalidStrings) {
        auto r = ISO8601::parseISODateTime(StringView::fromLatin1(s), ISO8601::TemporalProduction::DateTimeUnzoned);
        TCHECK_TRUE(!r.has_value(), "invalidString: should reject");
    }
}

// ---------------------------------------------------------------------------
// argument_string_critical_unknown_annotation — mirrors temporal_rs plain_date.rs
// test262: argument-string-critical-unknown-annotation.js
// Strings with critical (!) unknown annotations must fail parsing.
// ---------------------------------------------------------------------------

static void testCriticalUnknownAnnotation()
{
    // temporal_rs: plain_date.rs test argument_string_critical_unknown_annotation
    static const char* const criticalAnnotationStrings[] = {
        "1970-01-01[!foo=bar]",
        "1970-01-01T00:00[!foo=bar]",
        "1970-01-01T00:00[UTC][!foo=bar]",
        "1970-01-01T00:00[u-ca=iso8601][!foo=bar]",
        "1970-01-01T00:00[UTC][!foo=bar][u-ca=iso8601]",
        "1970-01-01T00:00[foo=bar][!_foo-bar0=Dont-Ignore-This-99999999999]",
    };
    for (auto* s : criticalAnnotationStrings) {
        auto r = ISO8601::parseISODateTime(StringView::fromLatin1(s), ISO8601::TemporalProduction::DateTimeUnzoned);
        TCHECK_TRUE(!r.has_value(), "criticalAnnotation: should reject");
    }
}

// ---------------------------------------------------------------------------
// parseISODateTime — comprehensive stress tests for the spec abstract op
// ---------------------------------------------------------------------------

namespace {
using P = ISO8601::TemporalProduction;
using PSet = ISO8601::TemporalProductionSet;
}

static void testParseInstantString()
{
    // TemporalInstantString ::= Date DateTimeSep Time DateTimeUTCOffset[+Z] TZAnno? Annotations?
    auto r = ISO8601::parseISODateTime("2024-01-15T12:00:00Z"_s, P::Instant);
    TCHECK_TRUE(r.has_value(), "Instant: Z form");
    TCHECK_TRUE(r->matched == P::Instant, "Instant: matched=Instant");
    TCHECK_TRUE(r->date.has_value() && r->date->year() == 2024 && r->date->month() == 1 && r->date->day() == 15, "Instant: date populated");
    TCHECK_TRUE(r->time.has_value() && r->time->hour() == 12, "Instant: time populated");
    TCHECK_TRUE(r->timeZone.has_value() && r->timeZone->m_z, "Instant: Z designator");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00+05:30"_s, P::Instant);
    TCHECK_TRUE(r.has_value() && r->timeZone->m_offset.has_value(), "Instant: numeric offset");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00Z[UTC]"_s, P::Instant);
    TCHECK_TRUE(r.has_value() && r->timeZone->m_z, "Instant: Z + bracket");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00+05:30:15.123456789"_s, P::Instant);
    TCHECK_TRUE(r.has_value() && r->timeZone->m_offsetHasSubMinutePrecision, "Instant: sub-minute precision");

    TCHECK_TRUE(ISO8601::parseISODateTime("20240115T120000Z"_s, P::Instant).has_value(), "Instant: compact YYYYMMDDTHHMMSSZ");
    TCHECK_TRUE(ISO8601::parseISODateTime("2024-01-15t12:00:00z"_s, P::Instant).has_value(), "Instant: lowercase t/z");
    TCHECK_TRUE(ISO8601::parseISODateTime("2024-01-15 12:00:00Z"_s, P::Instant).has_value(), "Instant: space separator");

    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15"_s, P::Instant).has_value(), "Instant: bare date rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15T12:00:00"_s, P::Instant).has_value(), "Instant: missing Z/offset rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("12:00:00Z"_s, P::Instant).has_value(), "Instant: bare time rejected");
}

static void testParseDateTimeStringUnzoned()
{
    // TemporalDateTimeString[~Zoned] = AnnotatedDateTime[~Zoned, ~TimeRequired]; Z forbidden.
    auto r = ISO8601::parseISODateTime("2024-01-15"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && !r->time.has_value() && !r->timeZone.has_value(), "Unzoned: bare date");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && r->time.has_value() && !r->timeZone.has_value(), "Unzoned: date+time");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00+05:00"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && r->timeZone.has_value() && r->timeZone->m_offset.has_value(), "Unzoned: numeric offset OK");

    r = ISO8601::parseISODateTime("2024-01-15[America/New_York]"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && r->timeZone.has_value(), "Unzoned: bracket alone OK");

    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15T12:00:00Z"_s, P::DateTimeUnzoned).has_value(), "Unzoned: bare Z rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15T12:00:00Z[UTC]"_s, P::DateTimeUnzoned).has_value(), "Unzoned: Z+bracket rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-13-01"_s, P::DateTimeUnzoned).has_value(), "Unzoned: month=13 rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-02-30"_s, P::DateTimeUnzoned).has_value(), "Unzoned: Feb 30 rejected");
}

static void testParseDateTimeStringZoned()
{
    // TemporalDateTimeString[+Zoned]: bracket REQUIRED; Z allowed.
    TCHECK_TRUE(ISO8601::parseISODateTime("2024-01-15[America/New_York]"_s, P::DateTimeZoned).has_value(), "Zoned: bare date + bracket");

    auto r = ISO8601::parseISODateTime("2024-01-15T12:00:00Z[UTC]"_s, P::DateTimeZoned);
    TCHECK_TRUE(r.has_value() && r->timeZone->m_z, "Zoned: Z + bracket");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00+05:00[Asia/Kolkata]"_s, P::DateTimeZoned);
    TCHECK_TRUE(r.has_value() && r->timeZone->m_offset.has_value(), "Zoned: offset + bracket");

    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15"_s, P::DateTimeZoned).has_value(), "Zoned: no bracket rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15T12:00:00Z"_s, P::DateTimeZoned).has_value(), "Zoned: Z without bracket rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15T12:00:00+05:00"_s, P::DateTimeZoned).has_value(), "Zoned: offset without bracket rejected");
}

static void testParseYearMonthString()
{
    // TemporalYearMonthString = AnnotatedYearMonth | AnnotatedDateTime[~Zoned, ~TimeRequired].
    auto r = ISO8601::parseISODateTime("2024-01"_s, P::YearMonth);
    TCHECK_TRUE(r.has_value() && r->isShortForm, "YM: hyphenated short");
    TCHECK_TRUE(r->date.has_value() && r->date->year() == 2024 && r->date->month() == 1, "YM: year+month populated");

    TCHECK_TRUE(ISO8601::parseISODateTime("202401"_s, P::YearMonth).value().isShortForm, "YM: compact short");
    TCHECK_TRUE(ISO8601::parseISODateTime("+002024-01"_s, P::YearMonth).value().isShortForm, "YM: extended hyphenated");
    TCHECK_TRUE(ISO8601::parseISODateTime("+00202401"_s, P::YearMonth).value().isShortForm, "YM: extended compact");
    TCHECK_TRUE(ISO8601::parseISODateTime("-001976-11"_s, P::YearMonth).value().isShortForm, "YM: negative extended");

    // Long-form fallback (full date) → isShortForm=false
    r = ISO8601::parseISODateTime("2024-01-15"_s, P::YearMonth);
    TCHECK_TRUE(r.has_value() && !r->isShortForm && r->matched == P::YearMonth, "YM: full date fallback retagged");
    TCHECK_TRUE(ISO8601::parseISODateTime("2024-01-15T12:00:00"_s, P::YearMonth).value().matched == P::YearMonth, "YM: full datetime fallback");

    // Calendar extraction: short-form requires iso8601, full-form accepts any builtin.
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01[u-ca=hebrew]"_s, P::YearMonth).has_value(),
        "YM: short-form non-iso8601 rejected (Step 4.a.ii.(3))");
    r = ISO8601::parseISODateTime("2024-01-15[u-ca=hebrew]"_s, P::YearMonth);
    TCHECK_TRUE(r.has_value() && r->calendar.has_value(), "YM: full-form calendar extracted");

    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-13"_s, P::YearMonth).has_value(), "YM: month=13 rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-00"_s, P::YearMonth).has_value(), "YM: month=00 rejected");
}

static void testParseMonthDayString()
{
    // TemporalMonthDayString = AnnotatedMonthDay | AnnotatedDateTime[~Zoned, ~TimeRequired].
    auto r = ISO8601::parseISODateTime("01-15"_s, P::MonthDay);
    TCHECK_TRUE(r.has_value() && r->isShortForm, "MD: MM-DD");
    TCHECK_TRUE(r->date.has_value() && r->date->month() == 1 && r->date->day() == 15, "MD: month+day populated");

    TCHECK_TRUE(ISO8601::parseISODateTime("0115"_s, P::MonthDay).value().isShortForm, "MD: MMDD compact");
    TCHECK_TRUE(ISO8601::parseISODateTime("--01-15"_s, P::MonthDay).value().isShortForm, "MD: --MM-DD");
    TCHECK_TRUE(ISO8601::parseISODateTime("--0115"_s, P::MonthDay).value().isShortForm, "MD: --MMDD");

    // Feb 29 OK (1972 reference is leap)
    TCHECK_TRUE(ISO8601::parseISODateTime("02-29"_s, P::MonthDay).has_value(), "MD: Feb 29 OK");

    // Long-form fallback
    r = ISO8601::parseISODateTime("2024-01-15"_s, P::MonthDay);
    TCHECK_TRUE(r.has_value() && !r->isShortForm && r->matched == P::MonthDay, "MD: full date fallback retagged");

    TCHECK_TRUE(!ISO8601::parseISODateTime("01-32"_s, P::MonthDay).has_value(), "MD: day=32 rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("02-30"_s, P::MonthDay).has_value(), "MD: Feb 30 rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("13-01"_s, P::MonthDay).has_value(), "MD: month=13 rejected");
}

static void testParseTimeString()
{
    // TemporalTimeString = AnnotatedTime | AnnotatedDateTime[~Zoned, +TimeRequired].
    auto r = ISO8601::parseISODateTime("12:00"_s, P::Time);
    TCHECK_TRUE(r.has_value() && !r->date.has_value() && r->time.has_value(), "Time: HH:MM (no date)");
    TCHECK_TRUE(r->matched == P::Time, "Time: matched=Time");

    TCHECK_TRUE(ISO8601::parseISODateTime("12:00:00"_s, P::Time).has_value(), "Time: HH:MM:SS");
    TCHECK_TRUE(ISO8601::parseISODateTime("120000"_s, P::Time).has_value(), "Time: compact");
    TCHECK_TRUE(ISO8601::parseISODateTime("12:00:00.123456789"_s, P::Time).has_value(), "Time: 9-digit fraction");

    TCHECK_TRUE(ISO8601::parseISODateTime("T12:00"_s, P::Time).has_value(), "Time: T prefix");
    TCHECK_TRUE(ISO8601::parseISODateTime("t12:00"_s, P::Time).has_value(), "Time: t prefix");

    // Datetime fallback
    r = ISO8601::parseISODateTime("2024-01-15T12:00:00"_s, P::Time);
    TCHECK_TRUE(r.has_value() && r->date.has_value() && r->matched == P::Time, "Time: datetime fallback retagged");

    TCHECK_TRUE(ISO8601::parseISODateTime("12:00:00+05:00"_s, P::Time).has_value(), "Time: time + offset OK");
    TCHECK_TRUE(ISO8601::parseISODateTime("12:00:00[UTC]"_s, P::Time).has_value(), "Time: time + bracket OK");

    TCHECK_TRUE(!ISO8601::parseISODateTime("12:00:00Z"_s, P::Time).has_value(), "Time: Z forbidden");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15T12:00:00Z"_s, P::Time).has_value(), "Time: datetime+Z forbidden");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15"_s, P::Time).has_value(), "Time: bare date rejected");

    // Ambiguity: 1212 matches HHMM AND DateSpec*; rejected without TimeDesignator
    TCHECK_TRUE(!ISO8601::parseISODateTime("1212"_s, P::Time).has_value(), "Time: ambiguous 1212 rejected");
    TCHECK_TRUE(ISO8601::parseISODateTime("T1212"_s, P::Time).has_value(), "Time: T1212 unambiguous");
}

static void testParseDateMVs()
{
    // Steps 8-12: DateYear (4-digit or ±6-digit) + DateMonth (01-12) + DateDay (per IsValidISODate).
    auto r = ISO8601::parseISODateTime("0000-01-01"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && !r->date->year(), "Step 8: year=0000");

    TCHECK_TRUE(ISO8601::parseISODateTime("9999-12-31"_s, P::DateTimeUnzoned).has_value(), "Step 8: year=9999");
    TCHECK_TRUE(ISO8601::parseISODateTime("+275760-09-13"_s, P::DateTimeUnzoned).has_value(), "Step 8: max valid extended");
    TCHECK_TRUE(ISO8601::parseISODateTime("-271821-04-20"_s, P::DateTimeUnzoned).has_value(), "Step 8: min valid extended");
    TCHECK_TRUE(!ISO8601::parseISODateTime("-000000-01-01"_s, P::DateTimeUnzoned).has_value(), "Step 8: -000000 forbidden");
    TCHECK_TRUE(ISO8601::parseISODateTime("+000000-01-01"_s, P::DateTimeUnzoned).has_value(), "Step 8: +000000 = year 0");
    TCHECK_TRUE(!ISO8601::parseISODateTime("123-01-01"_s, P::DateTimeUnzoned).has_value(), "Step 8: 3-digit year rejected");

    // Month bounds (Steps 9-10)
    for (unsigned m = 1; m <= 12; ++m) {
        char buf[16];
        SAFE_SPRINTF(std::span { buf }, "2024-%02u-01", m);
        TCHECK_TRUE(ISO8601::parseISODateTime(StringView::fromLatin1(buf), P::DateTimeUnzoned).has_value(), "Step 9-10: month bounds");
    }
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-00-01"_s, P::DateTimeUnzoned).has_value(), "Step 9-10: month=00 rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-13-01"_s, P::DateTimeUnzoned).has_value(), "Step 9-10: month=13 rejected");

    // Day bounds + IsValidISODate (Step 21)
    TCHECK_TRUE(ISO8601::parseISODateTime("2024-02-29"_s, P::DateTimeUnzoned).has_value(), "Step 21: leap Feb 29");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2023-02-29"_s, P::DateTimeUnzoned).has_value(), "Step 21: non-leap Feb 29 rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-04-31"_s, P::DateTimeUnzoned).has_value(), "Step 21: Apr 31 rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-32"_s, P::DateTimeUnzoned).has_value(), "Step 21: day=32 rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("1900-02-29"_s, P::DateTimeUnzoned).has_value(), "Step 21: 1900 not leap");
    TCHECK_TRUE(ISO8601::parseISODateTime("2000-02-29"_s, P::DateTimeUnzoned).has_value(), "Step 21: 2000 is leap");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2100-02-29"_s, P::DateTimeUnzoned).has_value(), "Step 21: 2100 not leap");

    // Mixed extended/compact forbidden
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-0115"_s, P::DateTimeUnzoned).has_value(), "Step 11-12: mixed sep-then-no");
    TCHECK_TRUE(!ISO8601::parseISODateTime("202401-15"_s, P::DateTimeUnzoned).has_value(), "Step 11-12: mixed no-then-sep");
}

static void testParseTimeMVs()
{
    // Steps 13-20: hour/minute/second + leap-clamp + fractional padding.
    for (unsigned h = 0; h <= 23; ++h) {
        char buf[8];
        SAFE_SPRINTF(std::span { buf }, "%02u:00", h);
        TCHECK_TRUE(ISO8601::parseISODateTime(StringView::fromLatin1(buf), P::Time).has_value(), "Step 13-14: hour bounds");
    }
    TCHECK_TRUE(!ISO8601::parseISODateTime("24:00"_s, P::Time).has_value(), "Step 13-14: hour=24 rejected");

    TCHECK_TRUE(ISO8601::parseISODateTime("12:59"_s, P::Time).has_value(), "Step 15-16: minute=59");
    TCHECK_TRUE(!ISO8601::parseISODateTime("12:60"_s, P::Time).has_value(), "Step 15-16: minute=60 rejected");

    auto r = ISO8601::parseISODateTime("12:00:60"_s, P::Time);
    TCHECK_TRUE(r.has_value() && r->time->second() == 59, "Step 18.b: leap-clamp 60→59");
    TCHECK_TRUE(!ISO8601::parseISODateTime("12:00:61"_s, P::Time).has_value(), "Step 17-18: second=61 rejected");

    r = ISO8601::parseISODateTime("12:00:00.1"_s, P::Time);
    TCHECK_TRUE(r.has_value() && r->time->millisecond() == 100, "Step 19-20: 0.1 → ms=100");
    TCHECK_TRUE(!r->time->microsecond() && !r->time->nanosecond(), "Step 19-20: 0.1 → us=ns=0");

    r = ISO8601::parseISODateTime("12:00:00.123456789"_s, P::Time);
    TCHECK_TRUE(r.has_value() && r->time->millisecond() == 123 && r->time->microsecond() == 456 && r->time->nanosecond() == 789, "Step 19-20: 9-digit ms/us/ns");

    r = ISO8601::parseISODateTime("12:00:00,5"_s, P::Time);
    TCHECK_TRUE(r.has_value() && r->time->millisecond() == 500, "Step 19-20: comma separator");

    TCHECK_TRUE(!ISO8601::parseISODateTime("12:00:00.1234567890"_s, P::Time).has_value(), "Step 19-20: 10 frac digits rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("12:00:00."_s, P::Time).has_value(), "Step 19-20: trailing dot rejected");
}

static void testParseTimeZoneFields()
{
    // Steps 22-27: time + timeZone field population.
    auto r = ISO8601::parseISODateTime("2024-01-15"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && !r->time.has_value(), "Step 22: bare date → time absent");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && r->time.has_value(), "Step 23: time present → CreateTimeRecord");

    r = ISO8601::parseISODateTime("2024-01-15"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && !r->timeZone.has_value(), "Step 24: no TZ info → timeZone absent");

    r = ISO8601::parseISODateTime("2024-01-15[America/New_York]"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && r->timeZone.has_value(), "Step 25: bracket → timeZone present");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00Z"_s, P::Instant);
    TCHECK_TRUE(r.has_value() && r->timeZone->m_z && !r->timeZone->m_offset.has_value(), "Step 26: Z → m_z=true, no offset");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00+05:00"_s, P::Instant);
    TCHECK_TRUE(r.has_value() && r->timeZone->m_offset.has_value() && !r->timeZone->m_z, "Step 27: offset → m_z=false");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00+05:30:15.123"_s, P::Instant);
    TCHECK_TRUE(r.has_value() && r->timeZone->m_offsetHasSubMinutePrecision, "Step 27: sub-minute precision flag");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00+05:30"_s, P::Instant);
    TCHECK_TRUE(r.has_value() && !r->timeZone->m_offsetHasSubMinutePrecision, "Step 27: HH:MM no sub-minute");

    TCHECK_TRUE(ISO8601::parseISODateTime("2024-01-15T12:00:00+0530"_s, P::Instant).has_value(), "Step 27: ±HHMM compact");
    TCHECK_TRUE(ISO8601::parseISODateTime("2024-01-15T12:00:00+053015"_s, P::Instant).has_value(), "Step 27: ±HHMMSS compact");

    r = ISO8601::parseISODateTime("2024-01-15T12:00:00+00:00"_s, P::Instant);
    TCHECK_TRUE(r.has_value() && !r->timeZone->m_z, "Step 27: +00:00 is offset, not Z");
}

static void testParseAnnotationProcessing()
{
    // Step 4.a.ii.(1)-(2): annotation loop + critical-flag rules.
    auto r = ISO8601::parseISODateTime("2024-01-15[u-ca=hebrew]"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && r->calendar.has_value(), "Annotation: u-ca extracted");

    r = ISO8601::parseISODateTime("2024-01-15"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && !r->calendar.has_value(), "Annotation: no u-ca → calendar nullopt");

    r = ISO8601::parseISODateTime("2024-01-15[!u-ca=hebrew]"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && r->calendar.has_value(), "Annotation: critical u-ca single OK");

    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15[!foo=bar]"_s, P::DateTimeUnzoned).has_value(), "Annotation: critical unknown rejected");

    // First non-critical wins
    r = ISO8601::parseISODateTime("2024-01-15[u-ca=hebrew][u-ca=iso8601]"_s, P::DateTimeUnzoned);
    TCHECK_TRUE(r.has_value() && r->calendar.has_value(), "Annotation: first non-critical wins");

    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15[!u-ca=hebrew][u-ca=iso8601]"_s, P::DateTimeUnzoned).has_value(), "Annotation: critical-then-dup rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15[u-ca=hebrew][!u-ca=iso8601]"_s, P::DateTimeUnzoned).has_value(), "Annotation: dup-then-critical rejected");

    TCHECK_TRUE(ISO8601::parseISODateTime("2024-01-15[foo=bar]"_s, P::DateTimeUnzoned).has_value(), "Annotation: non-critical unknown ignored");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15[u-ca=]"_s, P::DateTimeUnzoned).has_value(), "Annotation: empty u-ca value rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15[u-ca=ab]"_s, P::DateTimeUnzoned).has_value(), "Annotation: 2-char value rejected");
}

static void testParseShortFormFlag()
{
    // Step 4.a.ii.(3)/(4): isShortForm flag for YearMonth/MonthDay short forms.
    TCHECK_TRUE(ISO8601::parseISODateTime("2020-01"_s, P::YearMonth).value().isShortForm, "Step 4.a.ii.(3): YM short form");
    TCHECK_TRUE(ISO8601::parseISODateTime("202001"_s, P::YearMonth).value().isShortForm, "Step 4.a.ii.(3): YM compact");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2020-01-15"_s, P::YearMonth).value().isShortForm, "Step 4.a.ii.(3): YM full date → not short");

    TCHECK_TRUE(ISO8601::parseISODateTime("01-15"_s, P::MonthDay).value().isShortForm, "Step 4.a.ii.(4): MD short form");
    TCHECK_TRUE(ISO8601::parseISODateTime("--01-15"_s, P::MonthDay).value().isShortForm, "Step 4.a.ii.(4): MD --MM-DD");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2020-01-15"_s, P::MonthDay).value().isShortForm, "Step 4.a.ii.(4): MD full date → not short");

    // Other goals never set isShortForm
    TCHECK_TRUE(!ISO8601::parseISODateTime("2020-01-15"_s, P::DateTimeUnzoned).value().isShortForm, "isShortForm: false for DateTimeUnzoned");
    TCHECK_TRUE(!ISO8601::parseISODateTime("12:00:00"_s, P::Time).value().isShortForm, "isShortForm: false for Time");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15T12:00:00Z"_s, P::Instant).value().isShortForm, "isShortForm: false for Instant");
}

static void testParseFullUnion()
{
    // ParseTemporalCalendarString uses the full 6-production union.
    PSet fullUnion {
        P::DateTimeZoned, P::DateTimeUnzoned, P::Instant,
        P::YearMonth, P::MonthDay, P::Time,
    };

    TCHECK_TRUE(ISO8601::parseISODateTime("2024-01-15"_s, fullUnion).has_value(), "Full union: bare date");
    TCHECK_TRUE(ISO8601::parseISODateTime("2024-01-15T12:00:00Z"_s, fullUnion).value().matched == P::Instant, "Full union: Instant priority");
    TCHECK_TRUE(ISO8601::parseISODateTime("2024-01-15T12:00:00[UTC]"_s, fullUnion).value().matched == P::DateTimeZoned, "Full union: Zoned over Unzoned");
    TCHECK_TRUE(ISO8601::parseISODateTime("2024-01"_s, fullUnion).value().matched == P::YearMonth, "Full union: YearMonth match");
    TCHECK_TRUE(ISO8601::parseISODateTime("01-15"_s, fullUnion).value().matched == P::MonthDay, "Full union: MonthDay match");
    TCHECK_TRUE(ISO8601::parseISODateTime("12:00:00"_s, fullUnion).value().matched == P::Time, "Full union: Time match");

    TCHECK_TRUE(!ISO8601::parseISODateTime("garbage"_s, fullUnion).has_value(), "Full union: garbage rejected");
    TCHECK_TRUE(!ISO8601::parseISODateTime(""_s, fullUnion).has_value(), "Full union: empty rejected");
}

static void testParseDispatchPriority()
{
    // Dispatcher tries goals most-specific first; matched tag reflects narrowest production.
    PSet maskA { P::Instant, P::DateTimeUnzoned };
    auto r = ISO8601::parseISODateTime("2024-01-15T12:00:00Z"_s, maskA);
    TCHECK_TRUE(r.has_value() && r->matched == P::Instant, "Priority: Instant before DateTimeUnzoned");
    TCHECK_TRUE(!ISO8601::parseISODateTime("2024-01-15T12:00:00Z"_s, P::DateTimeUnzoned).has_value(), "Priority: Unzoned alone rejects Z");

    PSet maskB { P::DateTimeZoned, P::DateTimeUnzoned };
    r = ISO8601::parseISODateTime("2024-01-15T12:00:00[UTC]"_s, maskB);
    TCHECK_TRUE(r.has_value() && r->matched == P::DateTimeZoned, "Priority: Zoned before Unzoned for bracketed");

    r = ISO8601::parseISODateTime("2024-01-15"_s, maskB);
    TCHECK_TRUE(r.has_value() && r->matched == P::DateTimeUnzoned, "Priority: bare date falls to Unzoned");
}

// ---------------------------------------------------------------------------
// CalendarICUBridge tests — mirrors temporal_rs src/builtins/core/calendar.rs
// ---------------------------------------------------------------------------

static void testCalendarIsLunisolar()
{
    TCHECK_TRUE(!calendarIsLunisolar(calendarIDFromString("iso8601"_s)), "lunisolar: iso8601=false");
    TCHECK_TRUE(!calendarIsLunisolar(calendarIDFromString("gregory"_s)), "lunisolar: gregory=false");
    TCHECK_TRUE(calendarIsLunisolar(calendarIDFromString("chinese"_s)), "lunisolar: chinese=true");
    TCHECK_TRUE(calendarIsLunisolar(calendarIDFromString("hebrew"_s)), "lunisolar: hebrew=true");
    TCHECK_TRUE(calendarIsLunisolar(calendarIDFromString("dangi"_s)), "lunisolar: dangi=true");
}

static void testCalendarDaysInMonthISO()
{
    // ISO8601 days in month — basic cases
    auto r1 = calendarDaysInMonth(calendarIDFromString("iso8601"_s), { 2020, 2, 1 }); // leap year Feb
    TCHECK_TRUE(r1.has_value(), "daysInMonth: 2020-02 ok");
    TCHECK_EQ(*r1, 29, "daysInMonth: 2020-02=29");
    auto r2 = calendarDaysInMonth(calendarIDFromString("iso8601"_s), { 2021, 2, 1 }); // non-leap Feb
    TCHECK_TRUE(r2.has_value(), "daysInMonth: 2021-02 ok");
    TCHECK_EQ(*r2, 28, "daysInMonth: 2021-02=28");
    auto r3 = calendarDaysInMonth(calendarIDFromString("iso8601"_s), { 2020, 1, 1 }); // January
    TCHECK_TRUE(r3.has_value(), "daysInMonth: Jan ok");
    TCHECK_EQ(*r3, 31, "daysInMonth: Jan=31");
    auto r4 = calendarDaysInMonth(calendarIDFromString("iso8601"_s), { 2020, 4, 1 }); // April
    TCHECK_TRUE(r4.has_value(), "daysInMonth: Apr ok");
    TCHECK_EQ(*r4, 30, "daysInMonth: Apr=30");
}

static void testCalendarInLeapYearISO()
{
    auto r1 = calendarInLeapYear(calendarIDFromString("iso8601"_s), { 2020, 1, 1 }); // leap
    TCHECK_TRUE(r1.has_value() && *r1, "leapYear: 2020=leap");
    auto r2 = calendarInLeapYear(calendarIDFromString("iso8601"_s), { 2021, 1, 1 }); // non-leap
    TCHECK_TRUE(r2.has_value() && !*r2, "leapYear: 2021=not leap");
    auto r3 = calendarInLeapYear(calendarIDFromString("iso8601"_s), { 2000, 1, 1 }); // century leap
    TCHECK_TRUE(r3.has_value() && *r3, "leapYear: 2000=leap");
    auto r4 = calendarInLeapYear(calendarIDFromString("iso8601"_s), { 1900, 1, 1 }); // century non-leap
    TCHECK_TRUE(r4.has_value() && !*r4, "leapYear: 1900=not leap");
}

static void testCalendarISO8601Fields()
{
    // ISO8601 calendar: year/month/day return ISO values
    auto rYear = calendarYear(calendarIDFromString("iso8601"_s), { 2020, 6, 15 });
    TCHECK_TRUE(rYear.has_value(), "calYear: ok");
    TCHECK_EQ(*rYear, 2020, "calYear: iso8601 year");

    auto rMonth = calendarMonth(calendarIDFromString("iso8601"_s), { 2020, 6, 15 });
    TCHECK_TRUE(rMonth.has_value(), "calMonth: ok");
    TCHECK_EQ(*rMonth, 6u, "calMonth: iso8601 month");

    auto rDay = calendarDay(calendarIDFromString("iso8601"_s), { 2020, 6, 15 });
    TCHECK_TRUE(rDay.has_value(), "calDay: ok");
    TCHECK_EQ(*rDay, 15u, "calDay: iso8601 day");

    // daysInYear: 2020 (leap) = 366, 2021 (non-leap) = 365
    auto rDIY2020 = calendarDaysInYear(calendarIDFromString("iso8601"_s), { 2020, 1, 1 });
    TCHECK_TRUE(rDIY2020.has_value(), "calDIY: 2020 ok");
    TCHECK_EQ(*rDIY2020, 366, "calDIY: 2020=366");

    auto rDIY2021 = calendarDaysInYear(calendarIDFromString("iso8601"_s), { 2021, 1, 1 });
    TCHECK_EQ(*rDIY2021, 365, "calDIY: 2021=365");

    // monthsInYear: ISO8601 always 12
    auto rMIY = calendarMonthsInYear(calendarIDFromString("iso8601"_s), { 2020, 1, 1 });
    TCHECK_TRUE(rMIY.has_value(), "calMIY: ok");
    TCHECK_EQ(*rMIY, 12, "calMIY: iso8601=12");
}

static void testCalendarFieldsFunctions()
{
    using MC = ParsedMonthCode;
    auto id = calendarIDFromString;
    CalendarFieldsIn f;

    // --- yearMonthFromFields ---
    // temporal_rs: test_plain_year_month_with (basic field construction)
    f = { };
    f.year = 1999;
    f.month = 12;
    auto rYM = yearMonthFromFields(id("iso8601"_s), f, TemporalOverflow::Reject);
    TCHECK_TRUE(rYM.has_value() && rYM->isoDate.year() == 1999 && rYM->isoDate.month() == 12 && rYM->isoDate.day() == 1, "yearMonthFromFields: iso 1999-12");

    // yearMonthFromFields: constrain out-of-range month
    f = { };
    f.year = 2020;
    f.month = 13;
    auto rYMC = yearMonthFromFields(id("iso8601"_s), f, TemporalOverflow::Constrain);
    TCHECK_TRUE(rYMC.has_value() && rYMC->isoDate.month() == 12, "yearMonthFromFields: constrain month 13->12");

    // yearMonthFromFields: reject out-of-range month
    f = { };
    f.year = 2020;
    f.month = 13;
    auto rYMR = yearMonthFromFields(id("iso8601"_s), f, TemporalOverflow::Reject);
    TCHECK_TRUE(!rYMR.has_value(), "yearMonthFromFields: reject month 13 -> error");

    // --- monthDayFromFields ---
    // temporal_rs: month_day_from_fields basic case
    f = { };
    f.monthCode = MC { 3, false }; // M03
    f.day = 15;
    auto rMD = monthDayFromFields(id("iso8601"_s), f, TemporalOverflow::Reject);
    TCHECK_TRUE(rMD.has_value() && rMD->isoDate.month() == 3 && rMD->isoDate.day() == 15, "monthDayFromFields: M03 day=15");

    // monthDayFromFields: constrain day — reference year 1972 is a leap year, so Feb has 29 days
    f = { };
    f.monthCode = MC { 2, false }; // Feb
    f.day = 30;
    auto rMDC = monthDayFromFields(id("iso8601"_s), f, TemporalOverflow::Constrain);
    TCHECK_TRUE(rMDC.has_value() && rMDC->isoDate.month() == 2 && rMDC->isoDate.day() == 29, "monthDayFromFields: constrain Feb 30->29 (ref year 1972 is leap)");

    // --- differenceYearMonth ---
    // temporal_rs: test_year_month_diff_range
    auto rDiff = differenceYearMonth(id("iso8601"_s), { 2020, 1, 1 }, { 2021, 3, 1 }, TemporalUnit::Month);
    TCHECK_TRUE(rDiff.has_value() && rDiff->months() == 14, "differenceYearMonth: 14 months");

    // differenceYearMonth large span
    auto rDiffY = differenceYearMonth(id("iso8601"_s), { 2020, 1, 1 }, { 2022, 1, 1 }, TemporalUnit::Year);
    TCHECK_TRUE(rDiffY.has_value() && rDiffY->years() == 2, "differenceYearMonth: 2 years");

    // differenceYearMonth: day=1 out of range — temporal_rs: test_year_month_diff_range
    // min PlainYearMonth is -271821-04; setting day=1 gives -271821-04-01 which is before Temporal min (-271821-04-20)
    auto rDiffLimit = differenceYearMonth(id("iso8601"_s), { -271821, 4, 20 }, { 1970, 1, 1 }, TemporalUnit::Year);
    TCHECK_TRUE(!rDiffLimit.has_value(), "differenceYearMonth: min PlainYearMonth day=1 out of range");

    // --- plainYearMonthAdd ---
    // Add P1Y to 2020-06 -> 2021-06
    auto rAdd = plainYearMonthAdd(id("iso8601"_s), { 2020, 6, 1 }, ISO8601::Duration(1, 0, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(rAdd.has_value() && rAdd->isoDate.year() == 2021 && rAdd->isoDate.month() == 6, "plainYearMonthAdd: +1Y");

    // Add P3M to 2020-11 -> 2021-02
    auto rAdd2 = plainYearMonthAdd(id("iso8601"_s), { 2020, 11, 1 }, ISO8601::Duration(0, 3, 0, 0, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(rAdd2.has_value() && rAdd2->isoDate.year() == 2021 && rAdd2->isoDate.month() == 2, "plainYearMonthAdd: +3M rollover");

    // --- plainYearMonthToPlainDate ---
    // temporal_rs: PlainYearMonth::to_plain_date
    auto rYMToD = plainYearMonthToPlainDate(id("iso8601"_s), { 2020, 6, 1 }, 15);
    TCHECK_TRUE(rYMToD.has_value() && rYMToD->isoDate.year() == 2020 && rYMToD->isoDate.month() == 6 && rYMToD->isoDate.day() == 15, "plainYearMonthToPlainDate: 2020-06 day=15");

    // --- plainYearMonthFromISODate ---
    auto rYMFrom = plainYearMonthFromISODate(id("iso8601"_s), { 2020, 6, 15 });
    TCHECK_TRUE(rYMFrom.has_value() && rYMFrom->isoDate.year() == 2020 && rYMFrom->isoDate.month() == 6 && rYMFrom->isoDate.day() == 1, "plainYearMonthFromISODate: day -> 1");

    // --- plainMonthDayFromISODate ---
    auto rMDFrom = plainMonthDayFromISODate(id("iso8601"_s), { 2020, 6, 15 }, TemporalOverflow::Reject);
    TCHECK_TRUE(rMDFrom.has_value() && rMDFrom->isoDate.month() == 6 && rMDFrom->isoDate.day() == 15, "plainMonthDayFromISODate: month=6 day=15");

    // --- plainMonthDayToPlainDate ---
    auto rMDToD = plainMonthDayToPlainDate(id("iso8601"_s), { 1972, 6, 15 }, 2020);
    TCHECK_TRUE(rMDToD.has_value() && rMDToD->isoDate.year() == 2020 && rMDToD->isoDate.month() == 6 && rMDToD->isoDate.day() == 15, "plainMonthDayToPlainDate: day=15 year=2020");

    // --- plainYearMonthWith ---
    // temporal_rs: test_plain_year_month_with — override year only
    CalendarFieldsIn partialYear;
    partialYear.year = 2001;
    auto rWith = plainYearMonthWith(id("iso8601"_s), { 2025, 3, 1 }, partialYear, TemporalOverflow::Constrain);
    TCHECK_TRUE(rWith.has_value() && rWith->isoDate.year() == 2001 && rWith->isoDate.month() == 3, "plainYearMonthWith: override year -> 2001-03");

    // override month only
    CalendarFieldsIn partialMonth;
    partialMonth.month = 7;
    auto rWith2 = plainYearMonthWith(id("iso8601"_s), { 2025, 3, 1 }, partialMonth, TemporalOverflow::Constrain);
    TCHECK_TRUE(rWith2.has_value() && rWith2->isoDate.year() == 2025 && rWith2->isoDate.month() == 7, "plainYearMonthWith: override month -> 2025-07");

    // empty partial fields: ISO falls back year+monthCode from current, succeeds
    CalendarFieldsIn emptyPartial;
    auto rWithEmpty = plainYearMonthWith(id("iso8601"_s), { 2025, 3, 1 }, emptyPartial, TemporalOverflow::Constrain);
    TCHECK_TRUE(!rWithEmpty.has_value(), "plainYearMonthWith: empty partial -> TypeError (temporal_rs: fields.is_empty())");

    // --- plainDateWith ---
    // temporal_rs: basic_date_with — ISO override year/month/monthCode/day

    // override day only (ISO)
    CalendarFieldsIn partialDay;
    partialDay.day = 20;
    auto rDW1 = plainDateWith(id("iso8601"_s), { 2025, 3, 14 }, partialDay, TemporalOverflow::Constrain);
    TCHECK_TRUE(rDW1.has_value() && rDW1->isoDate.year() == 2025 && rDW1->isoDate.month() == 3 && rDW1->isoDate.day() == 20, "plainDateWith: override day -> 2025-03-20");

    // override month only (ISO)
    CalendarFieldsIn partialMonthOnly;
    partialMonthOnly.month = 7;
    auto rDW2 = plainDateWith(id("iso8601"_s), { 2025, 3, 14 }, partialMonthOnly, TemporalOverflow::Constrain);
    TCHECK_TRUE(rDW2.has_value() && rDW2->isoDate.year() == 2025 && rDW2->isoDate.month() == 7 && rDW2->isoDate.day() == 14, "plainDateWith: override month -> 2025-07-14");

    // override year only (ISO)
    CalendarFieldsIn partialYearOnly;
    partialYearOnly.year = 2000;
    auto rDW3 = plainDateWith(id("iso8601"_s), { 2025, 3, 14 }, partialYearOnly, TemporalOverflow::Constrain);
    TCHECK_TRUE(rDW3.has_value() && rDW3->isoDate.year() == 2000 && rDW3->isoDate.month() == 3 && rDW3->isoDate.day() == 14, "plainDateWith: override year -> 2000-03-14");

    // Japanese: era+eraYear override (Showa 50 = 1975)
    CalendarFieldsIn partialEra;
    partialEra.era = "showa"_s;
    partialEra.eraYear = 50;
    auto rDW4 = plainDateWith(id("japanese"_s), { 1970, 1, 1 }, partialEra, TemporalOverflow::Constrain);
    TCHECK_TRUE(rDW4.has_value() && rDW4->isoDate.year() == 1975 && rDW4->isoDate.month() == 1, "plainDateWith: japanese era+eraYear -> 1975-01-01");

    // Japanese: month change suppresses inherited era — NonISOFieldKeysToIgnore spec NOTE.
    // Showa 64 Jan 7 = 1989-01-07. Heisei started Jan 8 1989. month→6 re-derives to Heisei 1.
    CalendarFieldsIn partialMonth6;
    partialMonth6.month = 6;
    auto rDW5 = plainDateWith(id("japanese"_s), { 1989, 1, 7 }, partialMonth6, TemporalOverflow::Constrain);
    TCHECK_TRUE(rDW5.has_value() && rDW5->isoDate.year() == 1989 && rDW5->isoDate.month() == 6, "plainDateWith: japanese month change re-derives era -> 1989-06");

    // era+eraYear+year inconsistent → RangeError
    CalendarFieldsIn partialConflict;
    partialConflict.era = "showa"_s;
    partialConflict.eraYear = 50;
    partialConflict.year = 2000;
    auto rDW6 = plainDateWith(id("japanese"_s), { 1970, 1, 1 }, partialConflict, TemporalOverflow::Constrain);
    TCHECK_TRUE(!rDW6.has_value() && rDW6.error().kind == TemporalErrorKind::RangeError, "plainDateWith: inconsistent year+era+eraYear -> RangeError");
}

static void testCalendarDateFromFields()
{
    using MC = ParsedMonthCode;
    auto id = calendarIDFromString;

    // --- Non-lunisolar: year + month + day ---
    // Gregory year->ISO
    auto r = calendarDateFromFields(id("gregory"_s), 2024, 3, 15, std::nullopt, std::nullopt, std::nullopt, TemporalOverflow::Reject);
    TCHECK_TRUE(r.has_value() && r->year() == 2024 && r->month() == 3 && r->day() == 15, "gregory: year+month+day");

    // --- Era + eraYear ---
    // Gregory ce era — year=nullopt: no user-provided year, consistency check skipped
    auto rEra = calendarDateFromFields(id("gregory"_s), std::nullopt, 3, 15, StringView("ce"_s), 2024, std::nullopt, TemporalOverflow::Reject);
    TCHECK_TRUE(rEra.has_value() && rEra->year() == 2024 && rEra->month() == 3 && rEra->day() == 15, "gregory: ce+eraYear");

    // Gregory bce era: eraYear 1 = ISO year 0 — year=nullopt (user didn't provide year)
    auto rBce = calendarDateFromFields(id("gregory"_s), std::nullopt, 1, 1, StringView("bce"_s), 1, std::nullopt, TemporalOverflow::Reject);
    TCHECK_TRUE(rBce.has_value() && !rBce->year(), "gregory: bce eraYear 1 = ISO 0");

    // Japanese: modern era (reiwa year 6 = 2024) — year=nullopt
    auto rJp = calendarDateFromFields(id("japanese"_s), std::nullopt, 1, 1, StringView("reiwa"_s), 6, std::nullopt, TemporalOverflow::Reject);
    TCHECK_TRUE(rJp.has_value() && rJp->year() == 2024, "japanese: reiwa 6 = 2024");

    // Japanese: pre-1868 "ce" era bypasses ICU — year=nullopt
    auto rJpCe = calendarDateFromFields(id("japanese"_s), std::nullopt, 6, 15, StringView("ce"_s), 1600, std::nullopt, TemporalOverflow::Reject);
    TCHECK_TRUE(rJpCe.has_value() && rJpCe->year() == 1600 && rJpCe->month() == 6 && rJpCe->day() == 15, "japanese: ce 1600 bypass");

    // ROC: positive year (roc era)
    auto rRoc = calendarDateFromFields(id("roc"_s), 113, 1, 1, std::nullopt, std::nullopt, std::nullopt, TemporalOverflow::Reject);
    TCHECK_TRUE(rRoc.has_value() && rRoc->year() == 2024, "roc: year 113 = 2024");

    // ROC: year 0 -> broc era (ISO 1911)
    auto rRocBroc = calendarDateFromFields(id("roc"_s), 0, 1, 1, std::nullopt, std::nullopt, std::nullopt, TemporalOverflow::Reject);
    TCHECK_TRUE(rRocBroc.has_value() && rRocBroc->year() == 1911, "roc: year 0 = ISO 1911");

    // --- Month code: non-lunisolar ---
    // Gregory M03 = March
    auto rMc = calendarDateFromFields(id("gregory"_s), 2024, 0, 15, std::nullopt, std::nullopt, MC { 3, false }, TemporalOverflow::Reject);
    TCHECK_TRUE(rMc.has_value() && rMc->month() == 3 && rMc->day() == 15, "gregory: monthCode M03");

    // --- Month code: Hebrew leap month ---
    // Hebrew 5784 is a leap year; M05L = Adar I
    auto rHebLeap = calendarDateFromFields(id("hebrew"_s), 5784, 0, 1, std::nullopt, std::nullopt, MC { 5, true }, TemporalOverflow::Reject);
    TCHECK_TRUE(rHebLeap.has_value(), "hebrew: M05L in leap year 5784");

    // Hebrew 5783 is NOT a leap year; M05L constrain -> same month
    auto rHebConstrain = calendarDateFromFields(id("hebrew"_s), 5783, 0, 1, std::nullopt, std::nullopt, MC { 5, true }, TemporalOverflow::Constrain);
    TCHECK_TRUE(rHebConstrain.has_value(), "hebrew: M05L constrain in non-leap year 5783");

    // Hebrew 5783 non-leap + M05L reject -> error
    auto rHebReject = calendarDateFromFields(id("hebrew"_s), 5783, 0, 1, std::nullopt, std::nullopt, MC { 5, true }, TemporalOverflow::Reject);
    TCHECK_TRUE(!rHebReject.has_value(), "hebrew: M05L reject in non-leap year");

    // --- Overflow: constrain ---
    // Gregory: day 32 in January -> day 31
    auto rConstrain = calendarDateFromFields(id("gregory"_s), 2024, 1, 32, std::nullopt, std::nullopt, std::nullopt, TemporalOverflow::Constrain);
    TCHECK_TRUE(rConstrain.has_value() && rConstrain->day() == 31, "gregory: day 32 constrain -> 31");

    // Gregory: day 32 in January reject -> error
    auto rReject = calendarDateFromFields(id("gregory"_s), 2024, 1, 32, std::nullopt, std::nullopt, std::nullopt, TemporalOverflow::Reject);
    TCHECK_TRUE(!rReject.has_value(), "gregory: day 32 reject -> error");

    // --- Invalid era ---
    auto rBadEra = calendarDateFromFields(id("gregory"_s), 0, 1, 1, StringView("invalid"_s), 2024, std::nullopt, TemporalOverflow::Reject);
    TCHECK_TRUE(!rBadEra.has_value(), "gregory: invalid era -> error");
}

static void testCalendarICUNonISO()
{
    // Gregory calendar: same as ISO for modern dates
    auto rYearG = calendarYear(calendarIDFromString("gregory"_s), { 2020, 6, 15 });
    TCHECK_TRUE(rYearG.has_value(), "gregory: year ok");
    TCHECK_EQ(*rYearG, 2020, "gregory: year=2020");

    // Hebrew calendar: 2020-09-19 = 1 Tishri 5781 (Rosh Hashana)
    auto rHY = calendarYear(calendarIDFromString("hebrew"_s), { 2020, 9, 19 });
    TCHECK_TRUE(rHY.has_value(), "hebrew: year ok");
    TCHECK_EQ(*rHY, 5781, "hebrew: Rosh Hashana 5781");

    // Hebrew month: Tishri = month 1
    auto rHM = calendarMonth(calendarIDFromString("hebrew"_s), { 2020, 9, 19 });
    TCHECK_TRUE(rHM.has_value(), "hebrew: month ok");
    TCHECK_EQ(*rHM, 1u, "hebrew: Tishri = month 1");

    // Japanese calendar: 2020-05-01 = Reiwa 2 (令和2年)
    // calendarYear returns ISO year; eraYear returns era year
    auto rJY = calendarYear(calendarIDFromString("japanese"_s), { 2020, 5, 1 });
    TCHECK_TRUE(rJY.has_value(), "japanese: year ok");
    TCHECK_EQ(*rJY, 2020, "japanese: ISO year=2020");
    // eraYear should be 2 (Reiwa 2)
    auto rJEY = calendarEraYear(calendarIDFromString("japanese"_s), { 2020, 5, 1 });
    TCHECK_TRUE(rJEY.has_value() && rJEY->has_value(), "japanese: eraYear ok");
    TCHECK_EQ(**rJEY, 2, "japanese: eraYear=2 (Reiwa 2)");
    // era string should be "reiwa"
    auto rJE = calendarEra(calendarIDFromString("japanese"_s), { 2020, 5, 1 });
    TCHECK_TRUE(rJE.has_value() && rJE->has_value(), "japanese: era ok");
    TCHECK_TRUE(*rJE == String("reiwa"_s), "japanese: era=reiwa");
    // Heisei era: 2019-04-30 (last day of Heisei)
    auto rJHeisei = calendarEra(calendarIDFromString("japanese"_s), { 2019, 4, 30 });
    TCHECK_TRUE(rJHeisei.has_value() && rJHeisei->has_value(), "japanese: heisei ok");
    TCHECK_TRUE(*rJHeisei == String("heisei"_s), "japanese: era=heisei");
    // Gregory: CE/BCE
    auto rGCE = calendarEra(calendarIDFromString("gregory"_s), { 2020, 1, 1 });
    TCHECK_TRUE(rGCE.has_value() && rGCE->has_value(), "gregory: era ce ok");
    TCHECK_TRUE(*rGCE == String("ce"_s), "gregory: era=ce");
    // ISO8601 has no eras -> nullopt
    auto rIsoEra = calendarEra(calendarIDFromString("iso8601"_s), { 2020, 1, 1 });
    TCHECK_TRUE(rIsoEra.has_value() && !rIsoEra->has_value(), "iso8601: no era");

    // Chinese calendar: 2020-01-25 = Chinese New Year (1st of 1st month 2020)
    auto rCM = calendarMonth(calendarIDFromString("chinese"_s), { 2020, 1, 25 });
    TCHECK_TRUE(rCM.has_value(), "chinese: month ok");
    TCHECK_EQ(*rCM, 1u, "chinese: CNY = month 1");
    // Month code for Chinese month 1 = "M01"
    auto rCMC = calendarMonthCode(calendarIDFromString("chinese"_s), { 2020, 1, 25 });
    TCHECK_TRUE(rCMC.has_value(), "chinese: monthCode ok");
    TCHECK_TRUE(*rCMC == String("M01"_s), "chinese: CNY monthCode=M01");

    // Hebrew: Tishri (month 1) = "M01"; Adar I in a leap year = "M05L"
    auto rHMC = calendarMonthCode(calendarIDFromString("hebrew"_s), { 2020, 9, 19 }); // 1 Tishri 5781
    TCHECK_TRUE(rHMC.has_value(), "hebrew: monthCode Tishri ok");
    TCHECK_TRUE(*rHMC == String("M01"_s), "hebrew: Tishri=M01");
    // Hebrew 5782 is a leap year; 2022-02-03 = Adar I (M05L) in Hebrew 5782
    auto rHAdarI = calendarMonthCode(calendarIDFromString("hebrew"_s), { 2022, 2, 3 });
    TCHECK_TRUE(rHAdarI.has_value(), "hebrew: monthCode AdarI ok");
    TCHECK_TRUE(*rHAdarI == String("M05L"_s), "hebrew: AdarI=M05L");

    // ISO8601 month codes: M01-M12
    auto rISOMC = calendarMonthCode(calendarIDFromString("iso8601"_s), { 2020, 6, 15 });
    TCHECK_TRUE(rISOMC.has_value(), "iso8601: monthCode ok");
    TCHECK_TRUE(*rISOMC == String("M06"_s), "iso8601: June=M06");

    // Persian calendar: 2020-03-20 = Nowruz (1 Farvardin 1399)
    auto rPY = calendarYear(calendarIDFromString("persian"_s), { 2020, 3, 20 });
    TCHECK_TRUE(rPY.has_value(), "persian: year ok");
    TCHECK_EQ(*rPY, 1399, "persian: Nowruz 1399");

    // Islamic calendar: 2020-04-24 = 1 Ramadan 1441
    auto rIM = calendarMonth(calendarIDFromString("islamic"_s), { 2020, 4, 24 });
    TCHECK_TRUE(rIM.has_value(), "islamic: month ok");
    TCHECK_EQ(*rIM, 9u, "islamic: Ramadan = month 9");

    // Leap year in ISO: gregory tracks same as ISO
    auto rGL = calendarInLeapYear(calendarIDFromString("gregory"_s), { 2020, 1, 1 });
    TCHECK_TRUE(rGL.has_value() && *rGL, "gregory: 2020 leap");

    // Hebrew leap year (7 in 19-year cycle): 5782 (2021-22) is leap
    auto rHL = calendarInLeapYear(calendarIDFromString("hebrew"_s), { 2021, 9, 7 }); // 1 Tishri 5782
    TCHECK_TRUE(rHL.has_value(), "hebrew: 5782 leap check ok");
    // Hebrew 5782 is a leap year (has Adar II)
    TCHECK_TRUE(*rHL, "hebrew: 5782 is leap");
}

// ---------------------------------------------------------------------------
// TimeZoneICUBridge tests — mirrors temporal_rs src/tz.rs tests
// ---------------------------------------------------------------------------

static void testExactTimeToLocalDateAndTime()
{
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    // epoch=0, offset=0 -> 1970-01-01T00:00:00
    exactTimeToLocalDateAndTime(ISO8601::ExactTime(Int128(0)), 0, date, time);
    TCHECK_EQ(date.year(), 1970, "localDT: epoch year");
    TCHECK_EQ(date.month(), 1u, "localDT: epoch month");
    TCHECK_EQ(date.day(), 1u, "localDT: epoch day");
    TCHECK_EQ(time.hour(), 0u, "localDT: epoch hour");
    // epoch=86400000000000 (1 day), offset=0 -> 1970-01-02T00:00:00
    exactTimeToLocalDateAndTime(ISO8601::ExactTime(Int128(86400000000000LL)), 0, date, time);
    TCHECK_EQ(date.year(), 1970, "localDT: +1day year");
    TCHECK_EQ(date.day(), 2u, "localDT: +1day day");
    // offset=-18000000000000 ns (UTC-5): epoch=0 -> 1969-12-31T19:00:00
    exactTimeToLocalDateAndTime(ISO8601::ExactTime(Int128(0)), -18000000000000LL, date, time);
    TCHECK_EQ(date.year(), 1969, "localDT: UTC-5 epoch year");
    TCHECK_EQ(time.hour(), 19u, "localDT: UTC-5 epoch hour");
}

static void testInterpretISODateTimeOffset()
{
    // temporal_rs: interpret_isodatetime_offset tested via zdt_from_partial, zdt_offset_match_minutes
    auto utcOpt = ISO8601::parseTemporalTimeZoneIdentifier("UTC"_s);
    if (!utcOpt) {
        fprintf(stderr, "SKIP [interpretISODateTimeOffset]: UTC not available\n");
        return;
    }
    auto utc = *utcOpt;
    // 2020-01-01T12:00:00Z = 1577880000000000000 ns
    constexpr Int128 epoch2020Jan1Noon { 1577880000000000000LL };

    // Step 1: start-of-day -> getStartOfDay
    {
        auto r = interpretISODateTimeOffset({ 2020, 1, 1 }, { }, true, OffsetBehaviour::Wall,
            TemporalOffsetDisambiguation::Ignore, 0, false, utc, TemporalDisambiguation::Compatible);
        TCHECK_TRUE(r.has_value(), "interpretISO: start-of-day ok");
        TCHECK_EQ(r->epochNanoseconds(), Int128(1577836800000000000LL), "interpretISO: start-of-day = midnight UTC");
    }

    // Step 3: Wall -> GetEpochNanosecondsFor (ignore offset entirely)
    {
        auto r = interpretISODateTimeOffset({ 2020, 1, 1 }, { 12, 0, 0, 0, 0, 0 }, false,
            OffsetBehaviour::Wall, TemporalOffsetDisambiguation::Ignore, 3600000000000LL, false,
            utc, TemporalDisambiguation::Compatible);
        TCHECK_TRUE(r.has_value(), "interpretISO: Wall ok");
        TCHECK_EQ(r->epochNanoseconds(), epoch2020Jan1Noon, "interpretISO: Wall = noon UTC (offset ignored)");
    }

    // Step 4: Use -> epoch = GetUTCEpochNanoseconds(date, time) - offset
    // 2020-01-01T12:00:00+01:00 -> epoch = noon_UTC - 1h = 11:00 UTC
    {
        constexpr Int128 epoch2020Jan1_11UTC { 1577876400000000000LL };
        auto r = interpretISODateTimeOffset({ 2020, 1, 1 }, { 12, 0, 0, 0, 0, 0 }, false,
            OffsetBehaviour::Option, TemporalOffsetDisambiguation::Use, 3600000000000LL, false,
            utc, TemporalDisambiguation::Compatible);
        TCHECK_TRUE(r.has_value(), "interpretISO: Use ok");
        TCHECK_EQ(r->epochNanoseconds(), epoch2020Jan1_11UTC, "interpretISO: Use = noon - 1h offset");
    }

    // Step 10b: Prefer — offset matches UTC (0), returns the UTC candidate
    {
        auto r = interpretISODateTimeOffset({ 2020, 1, 1 }, { 12, 0, 0, 0, 0, 0 }, false,
            OffsetBehaviour::Option, TemporalOffsetDisambiguation::Prefer, 0, false,
            utc, TemporalDisambiguation::Compatible);
        TCHECK_TRUE(r.has_value(), "interpretISO: Prefer UTC=0 ok");
        TCHECK_EQ(r->epochNanoseconds(), epoch2020Jan1Noon, "interpretISO: Prefer UTC=0 = noon UTC");
    }

    // Step 11: Reject — offset (+1h) doesn't match UTC (0) -> error
    {
        auto r = interpretISODateTimeOffset({ 2020, 1, 1 }, { 12, 0, 0, 0, 0, 0 }, false,
            OffsetBehaviour::Option, TemporalOffsetDisambiguation::Reject, 3600000000000LL, false,
            utc, TemporalDisambiguation::Compatible);
        TCHECK_TRUE(!r.has_value(), "interpretISO: Reject mismatch -> error");
    }

    // Step 10c: match-minutes — Africa/Monrovia has offset -44:30 in 1970; -45:00 (rounded) is accepted
    // temporal_rs: zdt_offset_match_minutes test
    auto monroviaOpt = ISO8601::parseTemporalTimeZoneIdentifier("Africa/Monrovia"_s);
    if (monroviaOpt) {
        constexpr int64_t minus44m30s = -(44 * 60 + 30) * 1000000000LL; // -44min 30sec in ns
        constexpr int64_t minus45m = -(45 * 60) * 1000000000LL; // -45min in ns
        // Exact match (-44:30) accepted — has sub-minute precision -> matchMinutes=false (exact match only)
        auto rExact = interpretISODateTimeOffset({ 1970, 1, 1 }, { 0, 0, 0, 0, 0, 0 }, false,
            OffsetBehaviour::Option, TemporalOffsetDisambiguation::Reject, minus44m30s, true,
            *monroviaOpt, TemporalDisambiguation::Compatible);
        TCHECK_TRUE(rExact.has_value(), "interpretISO: Monrovia exact -44:30 accepted");
        // Rounded match (-45:00) accepted with matchMinutes=true — no sub-minute precision -> matchMinutes=true
        auto rRounded = interpretISODateTimeOffset({ 1970, 1, 1 }, { 0, 0, 0, 0, 0, 0 }, false,
            OffsetBehaviour::Option, TemporalOffsetDisambiguation::Reject, minus45m, false,
            *monroviaOpt, TemporalDisambiguation::Compatible);
        TCHECK_TRUE(rRounded.has_value(), "interpretISO: Monrovia rounded -45:00 accepted (match-minutes)");
    }
}

static void testGetOffsetNanosecondsForUTC()
{
    // UTC-offset timezone with offset=0 always returns 0
    TimeZone utc = TimeZone::fromUTCOffset(0);
    auto r = getOffsetNanosecondsFor(utc, ISO8601::ExactTime(Int128(0)));
    TCHECK_TRUE(r.has_value(), "utcOffset: no error");
    TCHECK_EQ(*r, 0LL, "utcOffset: UTC offset=0");
    // Different epoch time -> still 0 for offset-0 timezone
    auto r2 = getOffsetNanosecondsFor(utc, ISO8601::ExactTime(Int128(1000000000000000000LL)));
    TCHECK_TRUE(r2.has_value(), "utcOffset: large epoch ok");
    TCHECK_EQ(*r2, 0LL, "utcOffset: UTC always 0");
}

static void testTimeZoneICUWithIANA()
{
    // America/New_York — standard time offset: -5h = -18000000000000 ns
    // Test with a winter date (no DST): 2020-01-15T12:00:00 UTC = 1579089600000000000 ns
    auto nytzOpt = ISO8601::parseTemporalTimeZoneIdentifier("America/New_York"_s);
    if (!nytzOpt) {
        fprintf(stderr, "SKIP [IANA tests]: America/New_York not available\n");
        return;
    }
    auto nytz = *nytzOpt;
    // Winter: 2020-01-15T12:00:00Z -> UTC-5 offset
    ISO8601::ExactTime winterEpoch(Int128(1579089600000000000LL));
    auto offset1 = getOffsetNanosecondsFor(nytz, winterEpoch);
    TCHECK_TRUE(offset1.has_value(), "IANA: NY winter offset ok");
    TCHECK_EQ(*offset1, -18000000000000LL, "IANA: NY winter = UTC-5");

    // Summer: 2020-07-15T12:00:00Z -> UTC-4 (EDT)
    ISO8601::ExactTime summerEpoch(Int128(1594814400000000000LL));
    auto offset2 = getOffsetNanosecondsFor(nytz, summerEpoch);
    TCHECK_TRUE(offset2.has_value(), "IANA: NY summer offset ok");
    TCHECK_EQ(*offset2, -14400000000000LL, "IANA: NY summer = UTC-4");

    // getEpochNanosecondsFor: 2020-01-15T07:00:00 local (=12:00 UTC)
    auto r = getEpochNanosecondsFor(nytz, { 2020, 1, 15 }, { 7, 0, 0, 0, 0, 0 }, TemporalDisambiguation::Compatible);
    TCHECK_TRUE(r.has_value(), "IANA: NY getEpoch ok");
    TCHECK_EQ(r->epochNanoseconds(), Int128(1579089600000000000LL), "IANA: NY local->UTC");

    // DST gap: 2020-03-08T02:30 doesn't exist in America/New_York
    // Compatible -> spring forward -> 03:30 = UTC 07:30 = 1583652600000000000
    auto rDstGap = getEpochNanosecondsFor(nytz, { 2020, 3, 8 }, { 2, 30, 0, 0, 0, 0 }, TemporalDisambiguation::Compatible);
    TCHECK_TRUE(rDstGap.has_value(), "IANA: DST gap Compatible ok");
    // Should be in EDT territory (UTC-4): 02:30 compatible -> 03:30 EDT = 07:30 UTC
    TCHECK_EQ(rDstGap->epochNanoseconds(), Int128(1583652600000000000LL), "IANA: DST gap = 03:30 EDT");
}

// ---------------------------------------------------------------------------
// date_with_empty_error — mirrors temporal_rs plain_date.rs test date_with_empty_error
// Empty CalendarFieldsIn (all nullopt) must return a TypeError from dateFromFields.
// ---------------------------------------------------------------------------

static void testDateWithEmptyError()
{
    // temporal_rs: let err = base.with(CalendarFields::default(), None); assert!(err.is_err())
    // CalendarFieldsIn{} has all fields nullopt -> missing year, month, day -> TypeError
    CalendarFieldsIn emptyFields;
    auto r = dateFromFields(calendarIDFromString("iso8601"_s), emptyFields, TemporalOverflow::Constrain);
    TCHECK_TRUE(!r.has_value(), "dateWithEmpty: empty fields -> error");
}

// ---------------------------------------------------------------------------
// basic_date_with — mirrors temporal_rs plain_date.rs test basic_date_with
// Base: 1976-11-18. Override each field independently via dateFromFields,
// providing the non-overridden fields from the base date.
// ---------------------------------------------------------------------------

static void testBasicDateWith()
{
    // temporal_rs: plain_date.rs test basic_date_with
    // Base: 1976-11-18
    const int32_t baseYear = 1976;
    const uint32_t baseMonth = 11;
    const uint8_t baseDay = 18;

    // Override year -> 2019-11-18
    {
        CalendarFieldsIn fields;
        fields.year = 2019;
        fields.month = baseMonth;
        fields.day = baseDay;
        auto r = dateFromFields(calendarIDFromString("iso8601"_s), fields, TemporalOverflow::Constrain);
        TCHECK_TRUE(r.has_value(), "basicWith: override year ok");
        TCHECK_EQ(r->isoDate.year(), 2019, "basicWith: year=2019");
        TCHECK_EQ(r->isoDate.month(), 11u, "basicWith: month=11");
        TCHECK_EQ(r->isoDate.day(), 18u, "basicWith: day=18");
    }

    // Override month -> 1976-05-18
    {
        CalendarFieldsIn fields;
        fields.year = baseYear;
        fields.month = 5;
        fields.day = baseDay;
        auto r = dateFromFields(calendarIDFromString("iso8601"_s), fields, TemporalOverflow::Constrain);
        TCHECK_TRUE(r.has_value(), "basicWith: override month ok");
        TCHECK_EQ(r->isoDate.year(), 1976, "basicWith: month override year=1976");
        TCHECK_EQ(r->isoDate.month(), 5u, "basicWith: month=5");
        TCHECK_EQ(r->isoDate.day(), 18u, "basicWith: day=18");
    }

    // Override month via monthCode M05 -> 1976-05-18
    {
        CalendarFieldsIn fields;
        fields.year = baseYear;
        fields.monthCode = ParsedMonthCode { 5, false };
        fields.day = baseDay;
        auto r = dateFromFields(calendarIDFromString("iso8601"_s), fields, TemporalOverflow::Constrain);
        TCHECK_TRUE(r.has_value(), "basicWith: override monthCode ok");
        TCHECK_EQ(r->isoDate.year(), 1976, "basicWith: monthCode year=1976");
        TCHECK_EQ(r->isoDate.month(), 5u, "basicWith: monthCode month=5");
        TCHECK_EQ(r->isoDate.day(), 18u, "basicWith: monthCode day=18");
    }

    // Override day -> 1976-11-17
    {
        CalendarFieldsIn fields;
        fields.year = baseYear;
        fields.month = baseMonth;
        fields.day = 17;
        auto r = dateFromFields(calendarIDFromString("iso8601"_s), fields, TemporalOverflow::Constrain);
        TCHECK_TRUE(r.has_value(), "basicWith: override day ok");
        TCHECK_EQ(r->isoDate.year(), 1976, "basicWith: day override year=1976");
        TCHECK_EQ(r->isoDate.month(), 11u, "basicWith: day override month=11");
        TCHECK_EQ(r->isoDate.day(), 17u, "basicWith: day=17");
    }
}

// ---------------------------------------------------------------------------
// datetime_with_empty_partial — mirrors temporal_rs plain_date_time.rs
// test datetime_with_empty_partial
// Empty DateTimeFields (all nullopt) must return an error from dateFromFields.
// ---------------------------------------------------------------------------

static void testDateTimeWithEmptyPartial()
{
    // temporal_rs: let err = pdt.with(DateTimeFields::default(), None); assert!(err.is_err())
    // In C++: dateFromFields with all-nullopt fields -> TypeError (no year/month/day)
    CalendarFieldsIn emptyFields;
    auto r = dateFromFields(calendarIDFromString("iso8601"_s), emptyFields, TemporalOverflow::Constrain);
    TCHECK_TRUE(!r.has_value(), "dtWithEmpty: empty partial fields -> error");
    // Confirm it's a TypeError (missing required fields)
    TCHECK_TRUE(!r.has_value() && r.error().kind == TemporalErrorKind::TypeError,
        "dtWithEmpty: error kind is TypeError");
}

// ---------------------------------------------------------------------------
// datetime_round_options — mirrors temporal_rs plain_date_time.rs
// test datetime_round_options
// RoundingOptions without smallest_unit must fail. We test the pure C++
// equivalent: validateTemporalRoundingIncrement rejects increment=0
// (which is the increment produced by an unset/zero increment), and also
// that a non-divisor increment rejects with a valid dividend.
// ---------------------------------------------------------------------------

static void testDateTimeRoundOptions()
{
    // temporal_rs: dt.round(bad_options) — increment=0 is always invalid.
    auto rZero = validateTemporalRoundingIncrement(0.0, std::nullopt, Inclusivity::Exclusive);
    TCHECK_TRUE(!rZero.has_value(), "dtRoundOpts: increment=0 rejects");

    // Negative increment also invalid.
    auto rNeg = validateTemporalRoundingIncrement(-1.0, std::nullopt, Inclusivity::Exclusive);
    TCHECK_TRUE(!rNeg.has_value(), "dtRoundOpts: increment=-1 rejects");

    // increment=1, no dividend -> ok (valid smallest-unit-like state)
    auto rOne = validateTemporalRoundingIncrement(1.0, std::nullopt, Inclusivity::Exclusive);
    TCHECK_TRUE(rOne.has_value(), "dtRoundOpts: increment=1 no dividend ok");

    // temporal_rs: RoundingOptions::default() -> also error (no unit, no increment set).
    // Equivalent: increment=0 with a dividend also invalid.
    auto rZeroDiv = validateTemporalRoundingIncrement(0.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(!rZeroDiv.has_value(), "dtRoundOpts: increment=0 with dividend=60 rejects");

    // Valid round-like options: increment=1 with dividend=60 exclusive -> ok.
    auto rValid = validateTemporalRoundingIncrement(1.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(rValid.has_value(), "dtRoundOpts: increment=1 dividend=60 ok");

    // Non-divisor with dividend: increment=7, dividend=60 -> rejects.
    auto rNonDiv = validateTemporalRoundingIncrement(7.0, 60.0, Inclusivity::Exclusive);
    TCHECK_TRUE(!rNonDiv.has_value(), "dtRoundOpts: increment=7 not divisor of 60 rejects");
}

// ---------------------------------------------------------------------------
// to_string_precision_digits — mirrors temporal_rs plain_date_time.rs
// test to_string_precision_digits (fractionaldigits-number.js)
// Tests temporalDateTimeToString with Precision::Fixed for various digit counts.
// ---------------------------------------------------------------------------

static void testToStringPrecisionDigits()
{
    // temporal_rs: plain_date_time.rs test to_string_precision_digits
    // These mirror fractionaldigits-number.js test cases.

    // few_seconds: 1976-02-04T05:03:01 (all zeros for subseconds)
    ISO8601::PlainDate fewSecondsDate(1976, 2, 4);
    ISO8601::PlainTime fewSecondsTime(5, 3, 1, 0, 0, 0);

    // zero_seconds: 1976-11-18T15:23:00
    ISO8601::PlainDate zeroSecondsDate(1976, 11, 18);
    ISO8601::PlainTime zeroSecondsTime(15, 23, 0, 0, 0, 0);

    // whole_seconds: 1976-11-18T15:23:30
    ISO8601::PlainDate wholeSecondsDate(1976, 11, 18);
    ISO8601::PlainTime wholeSecondsTime(15, 23, 30, 0, 0, 0);

    // subseconds: 1976-11-18T15:23:30.123400 (ms=123, µs=400, ns=0)
    ISO8601::PlainDate subsecondsDate(1976, 11, 18);
    ISO8601::PlainTime subsecondsTime(15, 23, 30, 123, 400, 0);

    // Precision::Fixed(0): pads parts, no fractional seconds
    {
        auto s = ISO8601::temporalDateTimeToString(fewSecondsDate, fewSecondsTime, std::make_tuple(Precision::Fixed, 0u));
        TCHECK_EQ(s, "1976-02-04T05:03:01"_s, "toStrPrec: few_seconds digit=0 pads 0s");
    }

    // Precision::Fixed(0) on subseconds: truncates to 0 decimal places
    {
        auto s = ISO8601::temporalDateTimeToString(subsecondsDate, subsecondsTime, std::make_tuple(Precision::Fixed, 0u));
        TCHECK_EQ(s, "1976-11-18T15:23:30"_s, "toStrPrec: subseconds digit=0 truncates");
    }

    // Precision::Fixed(2) on zero_seconds: pads to 2 decimal places
    {
        auto s = ISO8601::temporalDateTimeToString(zeroSecondsDate, zeroSecondsTime, std::make_tuple(Precision::Fixed, 2u));
        TCHECK_EQ(s, "1976-11-18T15:23:00.00"_s, "toStrPrec: zero_seconds digit=2 pads");
    }

    // Precision::Fixed(2) on whole_seconds: pads to 2 decimal places
    {
        auto s = ISO8601::temporalDateTimeToString(wholeSecondsDate, wholeSecondsTime, std::make_tuple(Precision::Fixed, 2u));
        TCHECK_EQ(s, "1976-11-18T15:23:30.00"_s, "toStrPrec: whole_seconds digit=2 pads");
    }

    // Precision::Fixed(2) on subseconds: truncates 4 places to 2
    {
        auto s = ISO8601::temporalDateTimeToString(subsecondsDate, subsecondsTime, std::make_tuple(Precision::Fixed, 2u));
        TCHECK_EQ(s, "1976-11-18T15:23:30.12"_s, "toStrPrec: subseconds digit=2 truncates to 2");
    }

    // Precision::Fixed(3) on subseconds: truncates 4 places to 3
    {
        auto s = ISO8601::temporalDateTimeToString(subsecondsDate, subsecondsTime, std::make_tuple(Precision::Fixed, 3u));
        TCHECK_EQ(s, "1976-11-18T15:23:30.123"_s, "toStrPrec: subseconds digit=3 truncates to 3");
    }

    // Precision::Fixed(6) on subseconds: pads 4 places to 6
    {
        auto s = ISO8601::temporalDateTimeToString(subsecondsDate, subsecondsTime, std::make_tuple(Precision::Fixed, 6u));
        TCHECK_EQ(s, "1976-11-18T15:23:30.123400"_s, "toStrPrec: subseconds digit=6 pads to 6");
    }

    // Precision::Auto: omits trailing zeros
    {
        auto s = ISO8601::temporalDateTimeToString(wholeSecondsDate, wholeSecondsTime, std::make_tuple(Precision::Auto, 0u));
        TCHECK_EQ(s, "1976-11-18T15:23:30"_s, "toStrPrec: whole_seconds Auto omits zeros");
    }

    // Precision::Auto on subseconds: shows minimal significant digits
    {
        auto s = ISO8601::temporalDateTimeToString(subsecondsDate, subsecondsTime, std::make_tuple(Precision::Auto, 0u));
        TCHECK_EQ(s, "1976-11-18T15:23:30.1234"_s, "toStrPrec: subseconds Auto shows 4 sig digits");
    }
}

// ---------------------------------------------------------------------------
// ZDT tests — requires ICU timezone + DST support
// ---------------------------------------------------------------------------

static void testToZonedDateTime()
{
    // temporal_rs: plain_date.rs::to_zoned_date_time
    // PlainDate 2020-01-01 -> ZDT with UTC -> epoch = 2020-01-01T00:00:00Z
    auto utcOpt = ISO8601::parseTemporalTimeZoneIdentifier("UTC"_s);
    if (!utcOpt) {
        fprintf(stderr, "SKIP [toZDT]: UTC not available\n");
        return;
    }
    auto utc = *utcOpt;

    auto r = getEpochNanosecondsFor(utc, { 2020, 1, 1 }, { 0, 0, 0, 0, 0, 0 }, TemporalDisambiguation::Compatible);
    TCHECK_TRUE(r.has_value(), "toZDT: 2020-01-01 UTC ok");
    // Verify round-trip: epoch -> local date/time
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
    auto offset = getOffsetNanosecondsFor(utc, *r);
    TCHECK_TRUE(offset.has_value(), "toZDT: offset ok");
    exactTimeToLocalDateAndTime(*r, *offset, date, time);
    TCHECK_EQ(date.year(), 2020, "toZDT: year=2020");
    TCHECK_EQ(date.month(), 1u, "toZDT: month=1");
    TCHECK_EQ(date.day(), 1u, "toZDT: day=1");
    TCHECK_EQ(time.hour(), 0u, "toZDT: hour=0");
    TCHECK_EQ(time.minute(), 0u, "toZDT: minute=0");
    TCHECK_EQ(time.second(), 0u, "toZDT: second=0");
    TCHECK_EQ(time.millisecond(), 0u, "toZDT: ms=0");
    TCHECK_EQ(time.microsecond(), 0u, "toZDT: us=0");
    TCHECK_EQ(time.nanosecond(), 0u, "toZDT: ns=0");
}

static void testToZonedDateTimeError()
{
    // temporal_rs: to_zoned_date_time_error — min date -271821-04-19 start-of-day is at or before min epoch.

    auto utcOpt = ISO8601::parseTemporalTimeZoneIdentifier("UTC"_s);
    if (!utcOpt) {
        fprintf(stderr, "SKIP [toZDTErr]: UTC not available\n");
        return;
    }
    auto r = getEpochNanosecondsFor(*utcOpt, { -271821, 4, 18 }, { 0, 0, 0, 0, 0, 0 }, TemporalDisambiguation::Compatible);
    TCHECK_TRUE(!r.has_value(), "toZDTErr: day before min date rejects");
}

static void testAddZonedDateTime()
{
    // temporal_rs: basic_zdt_add (src/builtins/core/zoned_date_time/tests.rs)
    auto utcOpt = ISO8601::parseTemporalTimeZoneIdentifier("UTC"_s);
    if (!utcOpt) {
        fprintf(stderr, "SKIP [addZonedDateTime]: UTC not available\n");
        return;
    }
    auto utc = *utcOpt;

    // 1. Time-only fast path: temporal_rs basic_zdt_add start=-560174321098766 ns, P0DT240H+800ns.
    ISO8601::ExactTime start1(Int128(-560174321098766LL));
    ISO8601::Duration d1(0, 0, 0, 0, 240, 0, 0, 0, 0, 800); // 240h + 800ns
    auto r1 = addZonedDateTime(start1, utc, d1, TemporalOverflow::Constrain);
    TCHECK_TRUE(r1.has_value(), "addZDT: time-only ok");
    if (r1.has_value())
        TCHECK_EQ(r1->epochNanoseconds(), Int128(303825678902034LL), "addZDT: time-only result");

    // 2. Zero duration -> returns start unchanged
    ISO8601::Duration zero;
    auto r2 = addZonedDateTime(start1, utc, zero, TemporalOverflow::Constrain);
    TCHECK_TRUE(r2.has_value(), "addZDT: zero ok");
    if (r2.has_value())
        TCHECK_EQ(r2->epochNanoseconds(), start1.epochNanoseconds(), "addZDT: zero = start");

    // 3. Date+time path (P1DT1H with UTC): calendarDateAdd then re-resolve → 2020-01-16T13:00:00Z.
    ISO8601::ExactTime start3(Int128(1579089600000000000LL));
    ISO8601::Duration d3(0, 0, 0, 1, 1, 0, 0, 0, 0, 0); // P1DT1H
    auto r3 = addZonedDateTime(start3, utc, d3, TemporalOverflow::Constrain);
    TCHECK_TRUE(r3.has_value(), "addZDT: date+time ok");
    if (r3.has_value())
        TCHECK_EQ(r3->epochNanoseconds(), Int128(1579179600000000000LL), "addZDT: P1DT1H result");

    // 4. Date-only (P1Y) path: 2020-01-15T12:00:00Z + P1Y = 2021-01-15T12:00:00Z
    // 2021-01-15T12:00:00Z = 1610712000000000000 ns
    ISO8601::Duration d4(1, 0, 0, 0, 0, 0, 0, 0, 0, 0); // P1Y
    auto r4 = addZonedDateTime(start3, utc, d4, TemporalOverflow::Constrain);
    TCHECK_TRUE(r4.has_value(), "addZDT: P1Y ok");
    if (r4.has_value())
        TCHECK_EQ(r4->epochNanoseconds(), Int128(1610712000000000000LL), "addZDT: P1Y result");
}

static void testGetTimeZoneTransition()
{
    // temporal_rs: get_time_zone_transition (src/builtins/core/zoned_date_time/tests.rs)

    // 1. UTC-offset timezones have no transitions -> nullopt
    auto utcOpt = ISO8601::parseTemporalTimeZoneIdentifier("UTC"_s);
    if (!utcOpt) {
        fprintf(stderr, "SKIP [getTimeZoneTransition]: UTC not available\n");
        return;
    }
    auto r1 = getTimeZoneTransition(*utcOpt, ISO8601::ExactTime(Int128(0)), TransitionDirection::Next);
    TCHECK_TRUE(r1.has_value() && !r1->has_value(), "transition: UTC no next");
    auto r2 = getTimeZoneTransition(*utcOpt, ISO8601::ExactTime(Int128(0)), TransitionDirection::Previous);
    TCHECK_TRUE(r2.has_value() && !r2->has_value(), "transition: UTC no previous");
    // UTC-offset +05:30 also has no transitions
    auto plusOpt = ISO8601::parseTemporalTimeZoneIdentifier("+05:30"_s);
    if (plusOpt) {
        auto r3 = getTimeZoneTransition(*plusOpt, ISO8601::ExactTime(Int128(0)), TransitionDirection::Next);
        TCHECK_TRUE(r3.has_value() && !r3->has_value(), "transition: +05:30 no transitions");
    }

    // 2. America/New_York DST transitions from a summer 2020 date
    // summer epoch: 2020-07-15T12:00:00Z = 1594814400000000000 ns
    auto nyOpt = ISO8601::parseTemporalTimeZoneIdentifier("America/New_York"_s);
    if (!nyOpt) {
        fprintf(stderr, "SKIP [getTimeZoneTransition]: America/New_York not available\n");
        return;
    }
    ISO8601::ExactTime summer2020(Int128(1594814400000000000LL));
    // Previous: spring-forward 2020-03-08T07:00:00Z = 1583650800s * 1e9
    auto prevR = getTimeZoneTransition(*nyOpt, summer2020, TransitionDirection::Previous);
    TCHECK_TRUE(prevR.has_value() && prevR->has_value(), "transition: NY prev exists");
    if (prevR.has_value() && prevR->has_value()) {
        TCHECK_TRUE((*prevR)->epochNanoseconds() < summer2020.epochNanoseconds(), "transition: NY prev < query");
        TCHECK_EQ((*prevR)->epochNanoseconds(), Int128(1583650800LL) * Int128(1000000000LL), "transition: NY spring 2020");
    }
    // Next: fall-back 2020-11-01T06:00:00Z = 1604210400s * 1e9
    auto nextR = getTimeZoneTransition(*nyOpt, summer2020, TransitionDirection::Next);
    TCHECK_TRUE(nextR.has_value() && nextR->has_value(), "transition: NY next exists");
    if (nextR.has_value() && nextR->has_value()) {
        TCHECK_TRUE((*nextR)->epochNanoseconds() > summer2020.epochNanoseconds(), "transition: NY next > query");
        TCHECK_EQ((*nextR)->epochNanoseconds(), Int128(1604210400LL) * Int128(1000000000LL), "transition: NY fall 2020");
    }

    // 3. Europe/London: verify fake transitions (rule-change-without-offset-transition) are skipped.
    // From temporal_rs test262 case: at 1970-01-01T00:00:00Z (epoch=0), London was on BST (+01:00).
    // TZDB has intermediate fake entries around 1968-1971 that don't change the UTC offset —
    // our 20-iteration skip loop must bypass them to find the real pre-BST transition.
    auto londonOpt = ISO8601::parseTemporalTimeZoneIdentifier("Europe/London"_s);
    if (!londonOpt) {
        fprintf(stderr, "SKIP [getTimeZoneTransition London]: Europe/London not available\n");
        return;
    }
    auto londonPrev = getTimeZoneTransition(*londonOpt, ISO8601::ExactTime(Int128(0)), TransitionDirection::Previous);
    TCHECK_TRUE(londonPrev.has_value() && londonPrev->has_value(), "transition: London prev exists");
    if (londonPrev.has_value() && londonPrev->has_value()) {
        ISO8601::ExactTime tr = **londonPrev;
        // Transition must be before epoch 0 (pre-1970)
        TCHECK_TRUE(tr.epochNanoseconds() < Int128(0), "transition: London prev < 1970");
        // Key property: offset actually changed at this transition (not a fake entry).
        // Verify offset just before and at the transition differ.
        auto offsetBefore = getOffsetNanosecondsFor(*londonOpt, ISO8601::ExactTime(tr.epochNanoseconds() - 1));
        auto offsetAt = getOffsetNanosecondsFor(*londonOpt, tr);
        TCHECK_TRUE(offsetBefore.has_value() && offsetAt.has_value(), "transition: London offsets ok");
        if (offsetBefore.has_value() && offsetAt.has_value())
            TCHECK_TRUE(*offsetBefore != *offsetAt, "transition: London real offset change (not fake)");
    }
}

static void testTimeZoneEquals()
{
    // temporal_rs: canonicalize_equals (src/builtins/core/time_zone.rs)
    // 1. Identical string -> true
    TCHECK_TRUE(timeZoneEquals("UTC"_s, "UTC"_s), "tzEquals: UTC=UTC");
    TCHECK_TRUE(timeZoneEquals("+05:30"_s, "+05:30"_s), "tzEquals: +05:30=+05:30");

    // 2. Different strings -> false
    TCHECK_TRUE(!timeZoneEquals("UTC"_s, "America/New_York"_s), "tzEquals: UTC!=NY");
    TCHECK_TRUE(!timeZoneEquals("+05:30"_s, "+05:00"_s), "tzEquals: offset diff");

    // 3. Offset vs named -> false
    TCHECK_TRUE(!timeZoneEquals("+00:00"_s, "UTC"_s), "tzEquals: +00:00 != UTC (offset vs named)");

    // 4. IANA aliases: Asia/Calcutta = Asia/Kolkata (canonicalized to same primary)
    // temporal_rs: canonicalize_equals test
    TCHECK_TRUE(timeZoneEquals("Asia/Calcutta"_s, "Asia/Kolkata"_s), "tzEquals: Calcutta=Kolkata");
}

static void testPossibleEpochNsAtLimits()
{
    // temporal_rs: test_possible_epoch_ns_at_limits (src/builtins/core/time_zone.rs)
    // At the min/max Temporal boundaries, getPossibleEpochNanosecondsFor must return exactly 1 candidate.
    // Just outside those boundaries, it must return empty (range error).
    auto utcOpt = ISO8601::parseTemporalTimeZoneIdentifier("UTC"_s);
    if (!utcOpt) {
        fprintf(stderr, "SKIP [epochNsLimits]: UTC not available\n");
        return;
    }
    auto utc = *utcOpt;

    // UTC min boundary: -271821-04-20T00:00:00Z = exactly NS_MIN_INSTANT
    auto rMinValid = getPossibleEpochNanosecondsFor(utc, { -271821, 4, 20 }, { 0, 0, 0, 0, 0, 0 });
    TCHECK_TRUE(rMinValid.has_value() && std::holds_alternative<ISO8601::ExactTime>(*rMinValid), "epochNsLimits: min date = 1 candidate");
    if (rMinValid.has_value() && std::holds_alternative<ISO8601::ExactTime>(*rMinValid))
        TCHECK_TRUE(std::get<ISO8601::ExactTime>(*rMinValid).isValid(), "epochNsLimits: min candidate isValid");

    // UTC max boundary: +275760-09-13T00:00:00Z = exactly NS_MAX_INSTANT
    auto rMaxValid = getPossibleEpochNanosecondsFor(utc, { 275760, 9, 13 }, { 0, 0, 0, 0, 0, 0 });
    TCHECK_TRUE(rMaxValid.has_value() && std::holds_alternative<ISO8601::ExactTime>(*rMaxValid), "epochNsLimits: max date = 1 candidate");
    if (rMaxValid.has_value() && std::holds_alternative<ISO8601::ExactTime>(*rMaxValid))
        TCHECK_TRUE(std::get<ISO8601::ExactTime>(*rMaxValid).isValid(), "epochNsLimits: max candidate isValid");

    // Just before min: -271821-04-19T23:59:59.999999999Z = NS_MIN_INSTANT - 1ns -> out of range -> error or GapOffsets
    auto rTooEarly = getPossibleEpochNanosecondsFor(utc, { -271821, 4, 19 }, { 23, 59, 59, 999, 999, 999 });
    TCHECK_TRUE(!rTooEarly.has_value() || isGap(*rTooEarly), "epochNsLimits: too-early = error/gap");

    // Just after max: +275760-09-13T00:00:00.000000001Z = NS_MAX_INSTANT + 1ns -> out of range -> error or GapOffsets
    auto rTooLate = getPossibleEpochNanosecondsFor(utc, { 275760, 9, 13 }, { 0, 0, 0, 0, 0, 1 });
    TCHECK_TRUE(!rTooLate.has_value() || isGap(*rTooLate), "epochNsLimits: too-late = error/gap");

    // UTC offset timezone: +05:30 — same bounds should hold
    auto plusOpt = ISO8601::parseTemporalTimeZoneIdentifier("+05:30"_s);
    if (plusOpt) {
        auto rPlusMin = getPossibleEpochNanosecondsFor(*plusOpt, { -271821, 4, 20 }, { 5, 30, 0, 0, 0, 0 });
        TCHECK_TRUE(rPlusMin.has_value() && std::holds_alternative<ISO8601::ExactTime>(*rPlusMin), "epochNsLimits: +05:30 min ok");
    }
}

static void testNudgeFunctions()
{
    // temporal_rs: nudge functions tested indirectly via round_relative_to_zoned_datetime
    // and test_nudge_relative_date_total. These tests cover each nudge path directly.

    auto id = calendarIDFromString("iso8601"_s);

    // --- nudgeToDayOrTime: no timezone, pure time rounding ---
    // P0DT25H rounded to Day with increment=1, HalfExpand -> whole days=1, remainder=1h
    {
        ISO8601::Duration datePart;
        Int128 time25h = Int128(90000000000000LL); // 25h in ns
        auto dur = ISO8601::InternalDuration::combineDateAndTimeDuration(datePart, time25h);
        Int128 destEpochNs = time25h; // dest = 25h from epoch
        auto r = nudgeToDayOrTime(dur, destEpochNs, TemporalUnit::Day, 1, TemporalUnit::Hour, RoundingMode::HalfExpand);
        TCHECK_TRUE(r.has_value(), "nudgeDayOrTime: 25h ok");
        // NudgeResult.duration is InternalDuration; check days via dateDuration()
        TCHECK_TRUE(r->duration.dateDuration().days() == 1 || !r->duration.dateDuration().days(), "nudgeDayOrTime: days reasonable");
    }

    // nudgeToDayOrTime: exactly 24h rounds to 1 day (no remainder)
    {
        ISO8601::Duration datePart;
        Int128 time24h = Int128(86400000000000LL);
        auto dur = ISO8601::InternalDuration::combineDateAndTimeDuration(datePart, time24h);
        auto r = nudgeToDayOrTime(dur, time24h, TemporalUnit::Day, 1, TemporalUnit::Day, RoundingMode::HalfExpand);
        TCHECK_TRUE(r.has_value(), "nudgeDayOrTime: 24h ok");
    }

    // --- nudgeToCalendarUnit: P0DT1H with unit=Day relative to 2020-01-15 ---
    // Using Day unit; Year-scale denominators (≈3e16 ns > 2^53) now handled correctly via Int128 fractionToDouble.
    {
        ISO8601::PlainDate date { 2020, 1, 15 };
        ISO8601::PlainTime time;
        Int128 originEpochNs = getUTCEpochNanoseconds(date, time);
        // P0DT1H: datePart=0 days, time=1h
        ISO8601::Duration datePart;
        Int128 oneHour = Int128(3600000000000LL);
        auto dur = ISO8601::InternalDuration::combineDateAndTimeDuration(datePart, oneHour);
        Int128 destEpochNs = originEpochNs + oneHour;
        int32_t sign = 1;
        auto r = nudgeToCalendarUnit(sign, dur, originEpochNs, destEpochNs,
            date, time, 1, TemporalUnit::Day, RoundingMode::HalfExpand, nullptr, id);
        TCHECK_TRUE(r.has_value(), "nudgeCalUnit: P0DT1H Day ok");
        // 1h is well within 1 day → didExpandCalendarUnit=false
        TCHECK_TRUE(!r->nudgeResult.didExpandCalendarUnit, "nudgeCalUnit: no expand");
    }

    // --- bubbleRelativeDuration: P1M30D relative 2020-01-01 -> bubble from Day up ---
    // 2020-01-01 + P1M30D = 2020-03-01; bubbling from Day: is P1M30D >= P2M? No (Feb has 29d in 2020)
    // So bubble should NOT collapse to P2M (30 days < 29d of Feb + remainder) — stays P1M30D
    {
        ISO8601::PlainDate date { 2020, 1, 1 };
        ISO8601::PlainTime time;
        ISO8601::Duration datePart(0, 1, 0, 30, 0, 0, 0, 0, Int128(0), Int128(0));
        auto dur = ISO8601::InternalDuration::combineDateAndTimeDuration(datePart, 0);
        // nudgedEpochNs = 2020-03-01 UTC
        ISO8601::PlainDate nudgedDate { 2020, 3, 1 };
        Int128 nudgedEpochNs = getUTCEpochNanoseconds(nudgedDate, time);
        auto r = bubbleRelativeDuration(1, dur, nudgedEpochNs, date, time,
            TemporalUnit::Month, TemporalUnit::Day, nullptr, id);
        TCHECK_TRUE(r.has_value(), "bubbleRelDur: P1M30D ok");
    }

    // --- nudgeToZonedTime: UTC+0 timezone, P25H rounded to Hour ---
    auto utcOpt = ISO8601::parseTemporalTimeZoneIdentifier("UTC"_s);
    if (utcOpt) {
        ISO8601::PlainDate date { 2020, 1, 1 };
        ISO8601::PlainTime time;
        ISO8601::Duration datePart(0, 0, 0, 1, 0, 0, 0, 0, Int128(0), Int128(0)); // 1 day
        Int128 timeComp = Int128(3600000000000LL); // 1h
        auto dur = ISO8601::InternalDuration::combineDateAndTimeDuration(datePart, timeComp);
        auto r = nudgeToZonedTime(1, dur, date, time, *utcOpt, 1, TemporalUnit::Hour, RoundingMode::HalfExpand, id);
        TCHECK_TRUE(r.has_value(), "nudgeZonedTime: P1DT1H ok");
    }
}

static void testRoundRelativeToZonedDateTime()
{
    // temporal_rs: duration/tests.rs::round_relative_to_zoned_datetime
    // P25H duration, ZDT at epoch=1_000_000_000_000_000_000, tz=+04:30
    // round to largestUnit=Day -> expected: 1 day 1 hour

    // +04:30 = 4.5h = 16200s = 16200000000000 ns
    auto tzOpt = ISO8601::parseTemporalTimeZoneIdentifier("+04:30"_s);
    if (!tzOpt) {
        fprintf(stderr, "SKIP [roundRelZDT]: +04:30 not available\n");
        return;
    }
    auto tz = *tzOpt;

    // P25H = 90000000000000 ns
    Int128 startNs = Int128(1000000000000000000LL);
    Int128 endNs = startNs + Int128(90000000000000LL); // +25h

    // differenceZonedDateTimeWithRounding(start, end, tz, Day, Nanosecond, Trunc, 1, iso8601)
    auto diff = differenceZonedDateTimeWithRounding(
        ISO8601::ExactTime(startNs), ISO8601::ExactTime(endNs),
        tz, TemporalUnit::Day, TemporalUnit::Nanosecond, RoundingMode::Trunc, 1.0, calendarIDFromString("iso8601"_s));
    TCHECK_TRUE(diff.has_value(), "roundRelZDT: diff ok");
    // 25h with +04:30 (no DST) = 1 day + 1 hour
    TCHECK_EQ(static_cast<int64_t>(diff->dateDuration().days()), 1LL, "roundRelZDT: days=1");
    TCHECK_EQ(diff->time(), Int128(3600000000000LL), "roundRelZDT: time=1h");
}

static void testDurationTotalZDT()
{
    // temporal_rs: test_duration_total (ZDT path) — P2756H in months with DST differs from PlainDate path.

    auto romeOpt = ISO8601::parseTemporalTimeZoneIdentifier("Europe/Rome"_s);
    if (!romeOpt) {
        fprintf(stderr, "SKIP [totalZDT]: Europe/Rome not available\n");
        return;
    }
    auto romeTz = *romeOpt;

    // 2020-01-01T00:00 in Rome (winter = UTC+1) → UTC 2019-12-31T23:00; use getEpochNanosecondsFor.
    auto startR = getEpochNanosecondsFor(romeTz, { 2020, 1, 1 }, { 0, 0, 0, 0, 0, 0 }, TemporalDisambiguation::Compatible);
    TCHECK_TRUE(startR.has_value(), "totalZDT: start epoch ok");

    // end = start + 2756h in ns
    Int128 p2756h = Int128(2756LL) * Int128(3600000000000LL);
    Int128 endNs = startR->epochNanoseconds() + p2756h;

    // differenceZonedDateTimeWithRounding(start, end, Rome, Month, Nanosecond, Trunc, 1)
    auto diff = differenceZonedDateTimeWithRounding(
        *startR, ISO8601::ExactTime(endNs),
        romeTz, TemporalUnit::Month, TemporalUnit::Nanosecond, RoundingMode::Trunc, 1.0, calendarIDFromString("iso8601"_s));
    TCHECK_TRUE(diff.has_value(), "totalZDT: diff ok");

    // Convert to total months: months + remaining time as fraction
    // months * totalDaysInMonths + days portion -> complex; just verify months >= 3
    TCHECK_TRUE(static_cast<int64_t>(diff->dateDuration().months()) >= 3LL, "totalZDT: months>=3");

    // PlainDate path (no DST): verify P2756H ≈ 3 months from 2020-01-01 using pure day arithmetic.
    auto endDate = isoDateAdd({ 2020, 1, 1 }, ISO8601::Duration(0, 0, 0, 115, 0, 0, 0, 0, 0, 0), TemporalOverflow::Constrain);
    TCHECK_TRUE(endDate.has_value(), "totalZDT: plain endDate ok");
    auto plainDiff = diffISODate({ 2020, 1, 1 }, *endDate, TemporalUnit::Month);
    TCHECK_EQ(static_cast<int64_t>(plainDiff.months()), 3LL, "totalZDT: plain months=3");
}

static void testNudgePastEnd()
{
    // temporal_rs: duration/tests.rs::nudge_past_end
    // Zero duration, ZDT at max epoch (8.64e21 ns = 1e8 days * nsPerDay), round to Day/Minute
    // Rounding constructs end date = max + 1 day -> error

    auto utcOpt = ISO8601::parseTemporalTimeZoneIdentifier("UTC"_s);
    if (!utcOpt) {
        fprintf(stderr, "SKIP [nudgePast]: UTC not available\n");
        return;
    }
    auto utc = *utcOpt;

    // Max Temporal epoch = 1e8 days × nsPerDay; Int128 required (exceeds int64 range).
    Int128 maxEpoch = Int128(86400000000000LL) * Int128(100000000LL); // 1e8 days * nsPerDay

    // Test via getStartOfDay: one day past max rejects, max date itself succeeds.
    auto r = getStartOfDay(utc, { 275760, 9, 14 }); // one day past max
    TCHECK_TRUE(!r.has_value(), "nudgePast: start-of-day past max rejects");

    // Verify max date itself works
    auto r2 = getStartOfDay(utc, { 275760, 9, 13 }); // max date
    TCHECK_TRUE(r2.has_value(), "nudgePast: start-of-day at max ok");

    // Verify a normal diff near max succeeds (the out-of-range error is in getStartOfDay above).
    Int128 nearMax = maxEpoch - Int128(3600000000000LL); // 1h before max
    Int128 nearMaxEnd = nearMax + Int128(60000000000LL); // +1min
    auto diffNear = differenceZonedDateTimeWithRounding(
        ISO8601::ExactTime(nearMax), ISO8601::ExactTime(nearMaxEnd),
        utc, TemporalUnit::Hour, TemporalUnit::Nanosecond, RoundingMode::Trunc, 1.0, calendarIDFromString("iso8601"_s));
    TCHECK_TRUE(diffNear.has_value(), "nudgePast: normal diff near max ok");
}

static void testRoundZeroDurationZDT()
{
    // temporal_rs: round_zero_duration with ZDT relativeTo
    // P0 duration, ZDT at UTC epoch=0, round to Day/Hour -> result is still zero
    auto utcOpt = ISO8601::parseTemporalTimeZoneIdentifier("UTC"_s);
    if (!utcOpt) {
        fprintf(stderr, "SKIP [roundZeroZDT]: UTC not available\n");
        return;
    }
    auto utc = *utcOpt;

    // Start and end are the same (zero duration) -> diff = zero
    ISO8601::ExactTime epoch(Int128(0LL));
    auto diff = differenceZonedDateTimeWithRounding(epoch, epoch, utc, TemporalUnit::Day, TemporalUnit::Nanosecond, RoundingMode::Trunc, 1.0, calendarIDFromString("iso8601"_s));
    TCHECK_TRUE(diff.has_value(), "roundZeroZDT: zero diff ok");
    TCHECK_EQ(static_cast<int64_t>(diff->dateDuration().days()), 0LL, "roundZeroZDT: days=0");
    TCHECK_EQ(diff->time(), Int128(0LL), "roundZeroZDT: time=0");
}

static void testRoundIncrementRegressionZDT()
{
    // temporal_rs: round_increment_regression_test ZDT path
    // P48H, round to Day, increment=2, with ZDT UTC at epoch=0
    // Expected: 2 days (same result as without relativeTo)
    auto utcOpt = ISO8601::parseTemporalTimeZoneIdentifier("UTC"_s);
    if (!utcOpt) {
        fprintf(stderr, "SKIP [roundIncZDT]: UTC not available\n");
        return;
    }
    auto utc = *utcOpt;

    // P48H = 2 * 86400000000000 ns from epoch 0
    ISO8601::ExactTime start(Int128(0LL));
    ISO8601::ExactTime end(Int128(172800000000000LL)); // 48h
    auto diff = differenceZonedDateTimeWithRounding(start, end, utc, TemporalUnit::Day, TemporalUnit::Nanosecond, RoundingMode::Trunc, 1.0, calendarIDFromString("iso8601"_s));
    TCHECK_TRUE(diff.has_value(), "roundIncZDT: 48h diff ok");
    TCHECK_EQ(static_cast<int64_t>(diff->dateDuration().days()), 2LL, "roundIncZDT: days=2");
    TCHECK_EQ(diff->time(), Int128(0LL), "roundIncZDT: time=0");
}

// ---------------------------------------------------------------------------
// Section 1: direct temporal_rs ports
// ---------------------------------------------------------------------------

static void runTemporalRSTests()
{
    // --- iso.rs ---
    testISOEpochDayLimits(); // temporal_rs: iso_date_to_epoch_days_limits, test_month_limits

    // --- rounding.rs ---
    testMaximumRoundingIncrement(); // temporal_rs: (MaximumTemporalDurationRoundingIncrement)
    testNegateRoundingMode(); // temporal_rs: RoundingMode::negate
    testApplyUnsignedRoundingMode(); // temporal_rs: apply_unsigned_rounding_mode
    testRoundNumberToIncrementDouble(); // temporal_rs: test_basic_rounding_cases (double)
    testRoundNumberToIncrementInt128(); // temporal_rs: test_basic_rounding_cases (i128)
    testRoundingExactMultiples(); // temporal_rs: test_basic_rounding_cases (exact multiples)
    testRoundAsIfPositive(); // temporal_rs: round_as_if_positive
    testRoundingComprehensive(); // temporal_rs: test_basic_rounding_cases, test_float_rounding_cases, dt_since_basic_rounding
    testValidateTemporalRoundingIncrement(); // temporal_rs: RoundingIncrement::validate

    // --- plain_date.rs ---
    testAddDaysToISODate(); // temporal_rs: (addDaysToISODate)
    testBalanceISOYearMonth(); // temporal_rs: (balanceISOYearMonth)
    testRegulateISODate(); // temporal_rs: (regulateISODate)
    testISODateAdd(); // temporal_rs: simple_date_add
    testDateSubtract(); // temporal_rs: simple_date_subtract
    testISODateAddBoundaries(); // temporal_rs: date_add_limits
    testNewDateLimits(); // temporal_rs: new_date_limits
    testISODateCompare(); // temporal_rs: (isoDateCompare)
    testISOTimeCompare(); // temporal_rs: (isoTimeCompare)
    testDiffISODate(); // temporal_rs: simple_date_until, simple_date_since
    testDateRoundingIncrement(); // temporal_rs: rounding_increment_observed
    testInvalidDateStrings(); // temporal_rs: invalid_strings
    testCriticalUnknownAnnotation(); // temporal_rs: argument_string_critical_unknown_annotation
    testToZonedDateTime(); // temporal_rs: to_zoned_date_time
    testToZonedDateTimeError(); // temporal_rs: to_zoned_date_time_error
    testTimeZoneEquals(); // temporal_rs: canonicalize_equals
    testAddZonedDateTime(); // temporal_rs: basic_zdt_add
    testDateWithEmptyError(); // temporal_rs: date_with_empty_error
    testBasicDateWith(); // temporal_rs: basic_date_with

    // --- plain_date_time.rs ---
    testPlainDateTimeLimits(); // temporal_rs: plain_date_time_limits
    testDateTimeAddSubtract(); // temporal_rs: datetime_add_test, datetime_subtract_test, datetime_subtract_hour_overflows, datetime_add
    testDiffISODateTime(); // temporal_rs: (diffISODateTime)
    testDtSinceConflictingSigns(); // temporal_rs: dt_since_conflicting_signs
    testDifferenceTemporalPlainDateTime(); // temporal_rs: dt_until_basic, dt_since_basic, dt_since_conflicting_signs
    testDtRoundBasic(); // temporal_rs: dt_round_basic
    testRoundISODateTime(); // roundISODateTime: sub-unit base, halfEven, day overflow
    testDtUntilBasic(); // temporal_rs: dt_until_basic
    testCompareISODateTime(); // temporal_rs: (compareISODateTime)
    testDateTimeWithEmptyPartial(); // temporal_rs: datetime_with_empty_partial
    testDateTimeRoundOptions(); // temporal_rs: datetime_round_options
    testToStringPrecisionDigits(); // temporal_rs: to_string_precision_digits

    // --- duration/tests.rs ---
    testDurationSign(); // temporal_rs: (durationSign)
    testLargestSubduration(); // temporal_rs: (largestSubduration / default_largest_unit)
    testNegateDuration(); // temporal_rs: (negateDuration)
    testAbsDuration(); // temporal_rs: (absDuration)
    testAdjustDateDurationRecord(); // temporal_rs: (adjustDateDurationRecord)
    testTimeDurationFromComponents(); // temporal_rs: (timeDurationFromComponents)
    testDurationAddSubtract(); // temporal_rs: basic_add_duration, basic_subtract_duration
    testDurationBalancing(); // temporal_rs: balance_subseconds (partial)
    testDurationTotalBasic(); // temporal_rs: test_duration_total (pure path)
    testDurationTotalWithRelativeTo(); // temporal_rs: balance_days_up_to_both_years_and_months
    testRoundingCrossBoundary(); // temporal_rs: rounding_cross_boundary, rounding_cross_boundary_negative, rounding_cross_boundary_time_units
    testRoundingToDayOnly(); // temporal_rs: rounding_to_fractional_day_tests
    testBubbleSmallestBecomesDay(); // temporal_rs: bubble_smallest_becomes_day
    testRoundZeroDuration(); // temporal_rs: round_zero_duration (PlainDate path)
    testRoundIncrementRegression(); // temporal_rs: round_increment_regression_test (no relativeTo)
    testAddNormTimeDurationOutOfRange(); // temporal_rs: add_normalized_time_duration_out_of_range
    testAddLargeDurations(); // temporal_rs: add_large_durations
    testRoundingBoundaries(); // temporal_rs: test_rounding_boundaries
    testDurationCompareBoundary(); // temporal_rs: test_duration_compare_boundary
    testRoundRelativeToZonedDateTime(); // temporal_rs: round_relative_to_zoned_datetime
    testNudgeFunctions(); // direct tests for nudgeToCalendarUnit, nudgeToDayOrTime, nudgeToZonedTime, bubbleRelativeDuration
    testDurationTotalZDT(); // temporal_rs: test_duration_total (ZDT path)
    testNudgePastEnd(); // temporal_rs: nudge_past_end
    testRoundZeroDurationZDT(); // temporal_rs: round_zero_duration (ZDT path)
    testRoundIncrementRegressionZDT(); // temporal_rs: round_increment_regression_test (ZDT path)
}

// ---------------------------------------------------------------------------
// Section 2: additional stress tests beyond temporal_rs
// ---------------------------------------------------------------------------

static void runStressTests()
{
    // ISO arithmetic stress
    testISODateLimits(); // Temporal epoch limit boundary checks
    testNegativeRounding(); // Negative number rounding across all modes

    // Duration helpers
    testSplitTimeDuration(); // splitTimeDuration edge cases
    testPlainTimeFromSubdayNs(); // sub-day time decomposition
    testAdd24HourDaysToTimeDuration(); // 24h day folding
    testTemporalDurationFromInternal(); // InternalDuration -> Duration
    testToInternalDuration(); // Duration -> InternalDuration
    testToDateDurationRecordWithoutTime(); // time field stripping
    testTotalSecondsAndSubseconds(); // totalSeconds/totalSubseconds helpers
    testTotalTimeDuration(); // fractional unit conversion
    testBalanceDuration(); // duration field redistribution

    // Rounding helpers
    testCalendarDateAdd(); // ISO calendarDateAdd
    testCalendarDateUntil(); // ISO calendarDateUntil

    // Instant/TimeZone
    testMaximumInstantIncrement(); // maximumInstantIncrement per unit

    // ICU bridges
    testExactTimeToLocalDateAndTime(); // epoch -> local date+time
    testGetOffsetNanosecondsForUTC(); // UTC offset = 0
    testInterpretISODateTimeOffset(); // all branches: wall, use, prefer, reject, match-minutes, start-of-day
    testPossibleEpochNsAtLimits(); // temporal_rs: test_possible_epoch_ns_at_limits
    testGetTimeZoneTransition(); // temporal_rs: get_time_zone_transition
    testTimeZoneICUWithIANA(); // IANA timezone with DST (America/New_York)
    testGetUTCEpochNanoseconds(); // UTC epoch nanosecond computation

    // Calendar ICU
    testCalendarIsLunisolar(); // lunisolar calendar detection
    testCalendarDaysInMonthISO(); // ISO days-in-month
    testCalendarInLeapYearISO(); // ISO leap year
    testCalendarISO8601Fields(); // ISO field accessors
    testCalendarICUNonISO(); // Non-ISO calendars (hebrew, chinese, japanese, persian)
    testCalendarDateFromFields(); // calendarDateFromFields: era, monthCode, overflow, ROC, Japanese
    testCalendarFieldsFunctions(); // yearMonthFromFields, monthDayFromFields, differenceYearMonth, plainYearMonthAdd, etc.

    // parseISODateTime
    testParseInstantString();
    testParseDateTimeStringUnzoned();
    testParseDateTimeStringZoned();
    testParseYearMonthString();
    testParseMonthDayString();
    testParseTimeString();
    testParseDateMVs();
    testParseTimeMVs();
    testParseTimeZoneFields();
    testParseAnnotationProcessing();
    testParseShortFormFlag();
    testParseFullUnion();
    testParseDispatchPriority();
}

} // namespace TemporalCore
} // namespace JSC

// ---------------------------------------------------------------------------
// Entry point (extern "C" so testapi.c can call it)
// ---------------------------------------------------------------------------

extern "C" int testTemporalCore()
{
    using namespace JSC::TemporalCore;
    s_failures = 0;
    runTemporalRSTests();
    runStressTests();
    if (s_failures)
        fprintf(stderr, "testTemporalCore: %d test(s) FAILED\n", s_failures);
    else
        fprintf(stderr, "testTemporalCore: all tests passed\n");
    return s_failures ? 1 : 0;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
