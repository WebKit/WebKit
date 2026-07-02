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
#include "DurationArithmetic.h"

#include "CalendarICUBridge.h"
#include "DateConstructor.h"
#include "FractionToDouble.h"
#include "ISO8601.h"
#include "ISOArithmetic.h"
#include "Rounding.h"
#include "TemporalObject.h"
#include "TimeZoneICUBridge.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/DateMath.h>

namespace JSC {
namespace TemporalCore {

// durationSign — temporal_rs: Duration::sign()
int durationSign(const ISO8601::Duration& d)
{
#define JSC_CHECK_SIGN(field) \
    if (d.field() < 0)        \
        return -1;            \
    if (d.field() > 0)        \
        return 1;
    JSC_CHECK_SIGN(years)
    JSC_CHECK_SIGN(months)
    JSC_CHECK_SIGN(weeks)
    JSC_CHECK_SIGN(days)
    JSC_CHECK_SIGN(hours)
    JSC_CHECK_SIGN(minutes)
    JSC_CHECK_SIGN(seconds)
    JSC_CHECK_SIGN(milliseconds)
    JSC_CHECK_SIGN(microseconds)
    JSC_CHECK_SIGN(nanoseconds)
#undef JSC_CHECK_SIGN
    return 0;
}

// negateDuration — temporal_rs: Duration::negated()
ISO8601::Duration negateDuration(const ISO8601::Duration& d) { return -d; }

// absDuration — temporal_rs: Duration::abs()
ISO8601::Duration absDuration(const ISO8601::Duration& d)
{
    return ISO8601::Duration(
        std::abs(d.years()),
        std::abs(d.months()),
        std::abs(d.weeks()),
        std::abs(d.days()),
        std::abs(d.hours()),
        std::abs(d.minutes()),
        std::abs(d.seconds()),
        std::abs(d.milliseconds()),
        absInt128(d.microseconds()),
        absInt128(d.nanoseconds()));
}

// largestSubduration — temporal_rs: Duration::default_largest_unit
TemporalUnit NODELETE largestSubduration(const ISO8601::Duration& d)
{
    uint8_t index = 0;
    while (index < numberOfTemporalUnits - 1 && !d[index])
        index++;
    return static_cast<TemporalUnit>(index);
}

// totalSeconds — internal BalanceDuration helper; folds days/hours/minutes into total seconds for redistribution.
int64_t totalSeconds(const ISO8601::Duration& d)
{
    constexpr int64_t hourPerDay = 24;
    constexpr int64_t minPerHour = 60;
    constexpr int64_t secPerMin = 60;
    int64_t hours = hourPerDay * d.days() + d.hours();
    int64_t minutes = minPerHour * hours + d.minutes();
    return secPerMin * minutes + d.seconds();
}

// totalSubseconds — internal BalanceDuration helper; returns total sub-second nanoseconds (ms×1e6 + µs×1e3 + ns).
Int128 totalSubseconds(const ISO8601::Duration& d)
{
    constexpr int64_t usPerMs = 1000;
    constexpr int64_t nsPerUs = 1000;
    Int128 microseconds = Int128(usPerMs) * d.milliseconds() + d.microseconds();
    return Int128(nsPerUs) * microseconds + d.nanoseconds();
}

// balanceDuration — temporal_rs: balance is performed inline in Duration::compare.
// Redistributes time fields up to largestUnit. Returns std::nullopt (always).
std::optional<double> balanceDuration(ISO8601::Duration& duration, TemporalUnit largestUnit)
{
    constexpr int64_t nsPerUs = 1000;
    constexpr int64_t usPerMs = 1000;
    constexpr int64_t msPerSec = 1000;
    constexpr int64_t secPerMin = 60;
    constexpr int64_t minPerHour = 60;
    Int128 nanoseconds = totalSubseconds(duration);
    int64_t seconds = totalSeconds(duration);
    duration.clear();
    constexpr int64_t secondsPerDay = 86400;
    if (largestUnit <= TemporalUnit::Day) {
        duration.setDays(seconds / secondsPerDay);
        seconds = seconds % secondsPerDay;
    }
    Int128 microseconds = nanoseconds / nsPerUs;
    Int128 milliseconds = microseconds / Int128(usPerMs);
    // Fold ms overflow into seconds: ms >= 1000 must carry into seconds.
    int64_t secondsFromMs = static_cast<int64_t>(milliseconds / Int128(msPerSec));
    int64_t totalSecs = seconds + secondsFromMs;
    int64_t minutes = totalSecs / secPerMin;
    if (largestUnit <= TemporalUnit::Hour) {
        duration.setNanoseconds(nanoseconds % nsPerUs);
        duration.setMicroseconds(microseconds % Int128(usPerMs));
        duration.setMilliseconds(static_cast<int64_t>(milliseconds % Int128(msPerSec)));
        duration.setSeconds(totalSecs % secPerMin);
        duration.setMinutes(minutes % minPerHour);
        duration.setHours(minutes / minPerHour);
    } else if (largestUnit == TemporalUnit::Minute) {
        duration.setNanoseconds(nanoseconds % nsPerUs);
        duration.setMicroseconds(microseconds % Int128(usPerMs));
        duration.setMilliseconds(static_cast<int64_t>(milliseconds % Int128(msPerSec)));
        duration.setSeconds(totalSecs % secPerMin);
        duration.setMinutes(minutes);
    } else if (largestUnit == TemporalUnit::Second) {
        duration.setNanoseconds(nanoseconds % nsPerUs);
        duration.setMicroseconds(microseconds % Int128(usPerMs));
        duration.setMilliseconds(static_cast<int64_t>(milliseconds % Int128(msPerSec)));
        duration.setSeconds(totalSecs);
    } else if (largestUnit == TemporalUnit::Millisecond) {
        duration.setNanoseconds(nanoseconds % nsPerUs);
        duration.setMicroseconds(microseconds % Int128(usPerMs));
        duration.setMilliseconds(static_cast<int64_t>(milliseconds) + seconds * msPerSec);
    } else if (largestUnit == TemporalUnit::Microsecond) {
        duration.setNanoseconds(nanoseconds % nsPerUs);
        duration.setMicroseconds(microseconds + Int128(seconds) * (ISO8601::ExactTime::nsPerSecond / ISO8601::ExactTime::nsPerMicrosecond));
    } else
        duration.setNanoseconds(nanoseconds + Int128(seconds) * ISO8601::ExactTime::nsPerSecond);
    return std::nullopt;
}

// timeDurationFromComponents — temporal_rs: TimeDuration::from_components
// Converts duration time fields to a total nanosecond Int128.
Int128 timeDurationFromComponents(double hours, double minutes, double seconds, double milliseconds, double microseconds, double nanoseconds)
{
    constexpr int64_t minPerHour = 60;
    constexpr int64_t secPerMin = 60;
    constexpr int64_t msPerSec = 1000;
    constexpr int64_t usPerMs = 1000;
    constexpr int64_t nsPerUs = 1000;
    CheckedInt128 min = checkedCastDoubleToInt128(minutes) + checkedCastDoubleToInt128(hours) * Int128(minPerHour);
    CheckedInt128 sec = checkedCastDoubleToInt128(seconds) + min * Int128(secPerMin);
    CheckedInt128 millis = checkedCastDoubleToInt128(milliseconds) + sec * Int128(msPerSec);
    CheckedInt128 micros = checkedCastDoubleToInt128(microseconds) + millis * Int128(usPerMs);
    CheckedInt128 nanos = checkedCastDoubleToInt128(nanoseconds) + micros * Int128(nsPerUs);
    ASSERT(!nanos.hasOverflowed());
    ASSERT(absInt128(nanos) <= ISO8601::InternalDuration::maxTimeDuration);
    return nanos;
}

// splitTimeDuration — internal; splits a timeDuration (ns) into (overflowDays, subdayNs) using floor division.
std::pair<int64_t, Int128> splitTimeDuration(Int128 timeDuration)
{
    constexpr Int128 nsPerDay = ISO8601::ExactTime::nsPerDay;
    int64_t overflowDays = static_cast<int64_t>(timeDuration / nsPerDay);
    Int128 remainder = timeDuration % nsPerDay;
    if (remainder < 0) {
        remainder += nsPerDay;
        overflowDays--;
    }
    return { overflowDays, remainder };
}

// plainTimeFromSubdayNs — internal; decomposes sub-day nanoseconds into a PlainTime. ns must be in [0, nsPerDay).
ISO8601::PlainTime plainTimeFromSubdayNs(Int128 ns)
{
    ASSERT(ns >= 0 && ns < ISO8601::ExactTime::nsPerDay);
    int32_t nanosecond = static_cast<int32_t>(ns % 1000);
    Int128 remaining = ns / 1000;
    int32_t microsecond = static_cast<int32_t>(remaining % 1000);
    remaining /= 1000;
    int32_t millisecond = static_cast<int32_t>(remaining % 1000);
    remaining /= 1000;
    int32_t second = static_cast<int32_t>(remaining % 60);
    remaining /= 60;
    int32_t minute = static_cast<int32_t>(remaining % 60);
    int32_t hour = static_cast<int32_t>(remaining / 60);
    return ISO8601::PlainTime(hour, minute, second, millisecond, microsecond, nanosecond);
}

// totalTimeDuration — temporal_rs: TimeDuration::total (src/builtins/core/duration/normalized.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-totaltimeduration
double totalTimeDuration(Int128 timeDuration, TemporalUnit unit)
{
    Int128 unitLength = lengthInNanoseconds(unit);
    // All "Length in Nanoseconds" values in spec Table 21 are ≤ nsPerDay = 8.64e13 < 2^53.
    ASSERT(isSafeInteger(static_cast<double>(unitLength)));
    // Spec NOTE: timeDuration may exceed safe integer range; fractionToDouble
    // implements the required software emulation (double-double arithmetic).
    return fractionToDouble(timeDuration, static_cast<double>(unitLength));
}

// toInternalDuration — temporal_rs: Duration::to_internal_duration_record (src/builtins/core/duration.rs)
// Builds an InternalDuration from a Duration (time fields only, no day normalization).
ISO8601::InternalDuration toInternalDuration(ISO8601::Duration d)
{
    auto timeDuration = timeDurationFromComponents(d.hours(), d.minutes(), d.seconds(),
        d.milliseconds(), static_cast<double>(d.microseconds()), static_cast<double>(d.nanoseconds()));
    return ISO8601::InternalDuration::combineDateAndTimeDuration(d, timeDuration);
}

// temporalDurationFromInternal — temporal_rs: Duration::from_internal (src/builtins/core/duration.rs)
// Converts an InternalDuration back to a Duration given largestUnit.
TemporalResult<ISO8601::Duration> temporalDurationFromInternal(ISO8601::InternalDuration internalDuration, TemporalUnit largestUnit)
{
    int64_t days = 0;
    int64_t hours = 0;
    int64_t minutes = 0;
    int64_t seconds = 0;
    Int128 milliseconds = 0;
    Int128 microseconds = 0;

    int32_t sign = internalDuration.timeDurationSign();
    Int128 nanoseconds = absInt128(internalDuration.time());

    if (largestUnit <= TemporalUnit::Day) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = static_cast<int64_t>(milliseconds / 1000);
        milliseconds = milliseconds % 1000;
        minutes = seconds / 60;
        seconds = seconds % 60;
        hours = minutes / 60;
        minutes = minutes % 60;
        days = hours / 24;
        hours = hours % 24;
    } else if (largestUnit == TemporalUnit::Hour) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = static_cast<int64_t>(milliseconds / 1000);
        milliseconds = milliseconds % 1000;
        minutes = seconds / 60;
        seconds = seconds % 60;
        hours = minutes / 60;
        minutes = minutes % 60;
    } else if (largestUnit == TemporalUnit::Minute) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = static_cast<int64_t>(milliseconds / 1000);
        milliseconds = milliseconds % 1000;
        minutes = seconds / 60;
        seconds = seconds % 60;
    } else if (largestUnit == TemporalUnit::Second) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
        seconds = static_cast<int64_t>(milliseconds / 1000);
        milliseconds = milliseconds % 1000;
    } else if (largestUnit == TemporalUnit::Millisecond) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
        milliseconds = microseconds / 1000;
        microseconds = microseconds % 1000;
    } else if (largestUnit == TemporalUnit::Microsecond) {
        microseconds = nanoseconds / 1000;
        nanoseconds = nanoseconds % 1000;
    }

    if (hours)
        hours *= sign;
    if (minutes)
        minutes *= sign;
    if (seconds)
        seconds *= sign;
    if (milliseconds)
        milliseconds *= sign;
    if (microseconds)
        microseconds *= sign;
    if (nanoseconds)
        nanoseconds *= sign;
    // Apply ℝ(𝔽(x)) per spec CreateTemporalDuration step 12: round through float64 so that
    // Int128 values not exactly representable round past nanosecondsLimit → isValidDuration rejects.
    // milliseconds uses doubleToInt64Saturating because Int128 → double can still exceed int64_t
    // (e.g. largestUnit=millisecond, out-of-range input: ms ≈ 9e21 > INT64_MAX ≈ 9.2e18).
    ISO8601::Duration result { internalDuration.dateDuration().years(),
        internalDuration.dateDuration().months(),
        internalDuration.dateDuration().weeks(),
        static_cast<int64_t>(internalDuration.dateDuration().days() + days * sign),
        static_cast<int64_t>(hours), static_cast<int64_t>(minutes), static_cast<int64_t>(seconds),
        ISO8601::Duration::doubleToInt64Saturating(static_cast<double>(milliseconds)),
        Int128(static_cast<double>(microseconds)),
        Int128(static_cast<double>(nanoseconds)) };
    // CreateTemporalDuration step: If IsValidDuration is false, return error.
    if (!ISO8601::isValidDuration(result))
        return makeUnexpected(rangeError("Duration is outside the representable range"_s));
    return result;
}

// add24HourDaysToTimeDuration — temporal_rs: TimeDuration::add_days (src/builtins/core/duration/normalized.rs)
// Adds 24-hour days to a timeDuration (ns). Returns error if result exceeds maxTimeDuration.
TemporalResult<Int128> add24HourDaysToTimeDuration(Int128 d, double days)
{
    CheckedInt128 daysInNanoseconds = checkedCastDoubleToInt128(days) * ISO8601::ExactTime::nsPerDay;
    CheckedInt128 result = d + daysInNanoseconds;
    ASSERT(!result.hasOverflowed());
    if (absInt128(result) > ISO8601::InternalDuration::maxTimeDuration)
        return makeUnexpected(TemporalError { TemporalErrorKind::RangeError, "Total time in duration is out of range"_s });
    return Int128(result);
}

// toInternalDurationRecord — temporal_rs: InternalDurationRecord::from_duration (src/builtins/core/duration/normalized.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-tointernaldurationrecord
// Keeps days in [[Date]] and converts only time fields (hours..nanoseconds) to [[Time]] nanoseconds.
ISO8601::InternalDuration toInternalDurationRecord(ISO8601::Duration d)
{
    auto timeDuration = timeDurationFromComponents(d.hours(), d.minutes(), d.seconds(),
        d.milliseconds(), static_cast<double>(d.microseconds()), static_cast<double>(d.nanoseconds()));
    ISO8601::Duration dateDuration(d.years(), d.months(), d.weeks(), d.days(), 0, 0, 0, 0, Int128(0), Int128(0));
    return ISO8601::InternalDuration::combineDateAndTimeDuration(dateDuration, timeDuration);
}

// toInternalDurationRecordWith24HourDays — temporal_rs: InternalDurationRecord::from_duration_with_24_hour_days (src/builtins/core/duration/normalized.rs)
// Folds duration.days into timeDuration and returns InternalDuration with date-only datePart.
TemporalResult<ISO8601::InternalDuration> toInternalDurationRecordWith24HourDays(ISO8601::Duration d)
{
    Int128 timeDuration = timeDurationFromComponents(d.hours(), d.minutes(), d.seconds(),
        d.milliseconds(), static_cast<double>(d.microseconds()), static_cast<double>(d.nanoseconds()));
    auto result = add24HourDaysToTimeDuration(timeDuration, d.days());
    if (!result)
        return makeUnexpected(result.error());
    ISO8601::Duration dateDuration = ISO8601::Duration {
        d.years(), d.months(),
        d.weeks(), 0, 0, 0, 0, 0, Int128(0), Int128(0)
    };
    return ISO8601::InternalDuration::combineDateAndTimeDuration(dateDuration, *result);
}

// toDateDurationRecordWithoutTime — temporal_rs: InternalDurationRecord::to_date_duration_record_without_time (src/builtins/core/duration/normalized.rs)
// Strips time fields and folds days into the date part. Returns a date-only Duration.
TemporalResult<ISO8601::Duration> toDateDurationRecordWithoutTime(ISO8601::Duration duration)
{
    auto internalDuration = toInternalDurationRecordWith24HourDays(duration);
    if (!internalDuration)
        return makeUnexpected(internalDuration.error());
    auto days = internalDuration->time() / ISO8601::ExactTime::nsPerDay;
    return ISO8601::Duration {
        internalDuration->dateDuration().years(),
        internalDuration->dateDuration().months(),
        internalDuration->dateDuration().weeks(),
        static_cast<int64_t>(days), 0, 0, 0, 0, Int128(0), Int128(0)
    };
}

// getUTCEpochNanoseconds — temporal_rs: IsoDateTime::as_nanoseconds (UTC path)
// Converts (PlainDate, PlainTime) to epoch nanoseconds as if the datetime were UTC.
Int128 getUTCEpochNanoseconds(ISO8601::PlainDate date, ISO8601::PlainTime time)
{
    auto dayMs = makeDay(date.year(), date.month() - 1, date.day());
    auto timeMs = makeTime(time.hour(), time.minute(), time.second(), time.millisecond());
    auto ms = makeDate(dayMs, timeMs);
    ASSERT(isInteger(ms));
    return Int128(ms) * ISO8601::ExactTime::nsPerMillisecond
        + Int128(time.microsecond()) * ISO8601::ExactTime::nsPerMicrosecond
        + Int128(time.nanosecond());
}

constexpr int32_t unitIndexInTable(TemporalUnit unit)
{
    switch (unit) {
    case TemporalUnit::Year:
        return 0;
    case TemporalUnit::Month:
        return 1;
    case TemporalUnit::Week:
        return 2;
    case TemporalUnit::Day:
        return 3;
    case TemporalUnit::Hour:
        return 4;
    case TemporalUnit::Minute:
        return 5;
    case TemporalUnit::Second:
        return 6;
    case TemporalUnit::Millisecond:
        return 7;
    case TemporalUnit::Microsecond:
        return 8;
    case TemporalUnit::Nanosecond:
        return 9;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

constexpr TemporalUnit unitInTable(int32_t i)
{
    switch (i) {
    case 0:
        return TemporalUnit::Year;
    case 1:
        return TemporalUnit::Month;
    case 2:
        return TemporalUnit::Week;
    case 3:
        return TemporalUnit::Day;
    case 4:
        return TemporalUnit::Hour;
    case 5:
        return TemporalUnit::Minute;
    case 6:
        return TemporalUnit::Second;
    case 7:
        return TemporalUnit::Millisecond;
    case 8:
        return TemporalUnit::Microsecond;
    case 9:
        return TemporalUnit::Nanosecond;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

// epochNanosecondsForDateAndTime — internal; computes epoch nanoseconds for (date, time): UTC if timeZone==nullptr, TZ-aware otherwise.
static TemporalResult<Int128> epochNanosecondsForDateAndTime(
    const ISO8601::PlainDate& date, const ISO8601::PlainTime& time,
    const TimeZone* timeZone)
{
    if (!timeZone)
        return TemporalCore::getUTCEpochNanoseconds(date, time);
    auto result = TemporalCore::getEpochNanosecondsFor(*timeZone, date, time, TemporalDisambiguation::Compatible);
    if (!result)
        return makeUnexpected(result.error());
    return result->epochNanoseconds();
}

// adjustDateDurationRecord — internal helper; builds a new date duration with overridden days/weeks/months fields.
// https://tc39.es/proposal-temporal/#sec-temporal-adjustdatedurationrecord
TemporalResult<ISO8601::Duration> adjustDateDurationRecord(const ISO8601::Duration& dateDuration, int64_t days, std::optional<int64_t> weeks, std::optional<int64_t> months)
{
    // Step 1: If weeks is not present, set weeks to dateDuration.[[Weeks]].
    // Step 2: If months is not present, set months to dateDuration.[[Months]].
    // (Both handled implicitly by the std::optional parameters.)
    int64_t y = dateDuration.years();
    int64_t mo = months.value_or(dateDuration.months());
    int64_t w = weeks.value_or(dateDuration.weeks());
    int64_t d = days;
    // Step 3: Return ? CreateDateDurationRecord(dateDuration.[[Years]], months, weeks, days).
    auto result = ISO8601::Duration { y, mo, w, d, 0, 0, 0, 0, Int128(0), Int128(0) };

    // Skip isValidDuration by filtering the most of legit cases via quick comparison of specified fields.
    constexpr int64_t fieldLimit = static_cast<int64_t>(1) << 32;
    constexpr int64_t maxDays = (static_cast<int64_t>(1) << 53) / 86400;
    bool fastPath = (y > -fieldLimit && y < fieldLimit)
        && (mo > -fieldLimit && mo < fieldLimit)
        && (w > -fieldLimit && w < fieldLimit)
        && (d > -maxDays && d < maxDays);
    if (fastPath) {
        int sign = 0;
        auto check = [&](int64_t v) {
            if (!sign && v)
                sign = v > 0 ? 1 : -1;
            return !((v < 0 && sign > 0) || (v > 0 && sign < 0));
        };
        if (check(y) && check(mo) && check(w) && check(d))
            return result;
    }

    if (!ISO8601::isValidDuration(result))
        return makeUnexpected(rangeError("Temporal.Duration properties must be valid and of consistent sign"_s));
    return result;
}

// NudgeWindow — internal; holds the floor/ceiling epoch-ns bounds and durations for a single calendar nudge step.
struct NudgeWindow {
    double r1;
    double r2;
    Int128 startEpochNs;
    Int128 endEpochNs;
    ISO8601::Duration startDuration;
    ISO8601::Duration endDuration;
};

// computeNudgeWindow — temporal_rs: InternalDurationRecord::compute_nudge_window (src/builtins/core/duration/normalized.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-computenudgewindow
static TemporalResult<std::optional<NudgeWindow>> computeNudgeWindow(
    int32_t sign,
    const ISO8601::InternalDuration& duration, Int128 originEpochNs,
    ISO8601::PlainDate isoDate, ISO8601::PlainTime isoTime,
    double increment, TemporalUnit unit, bool additionalShift,
    const TimeZone* timeZone = nullptr, CalendarID calendarId = iso8601CalendarID())
{
    double r1 = 0, r2 = 0;
    ISO8601::Duration startDuration, endDuration;
    // Steps 1-4: compute r1, r2, startDuration, endDuration based on unit.
    switch (unit) {
    case TemporalUnit::Year: {
        // Step 1.a: years = RoundNumberToIncrement(duration.[[Date]].[[Years]], increment, trunc).
        Int128 years = roundNumberToIncrementInt128((Int128)duration.dateDuration().years(), (Int128)increment, RoundingMode::Trunc);
        // Step 1.b-c: r1 = years; or if additionalShift, r1 = years + increment × sign.
        r1 = additionalShift ? (double)years + increment * sign : (double)years;
        // Step 1.d: r2 = r1 + increment × sign.
        r2 = r1 + increment * sign;
        // Step 1.e: startDuration = CreateDateDurationRecord(r1, 0, 0, 0).
        startDuration = ISO8601::Duration { static_cast<int64_t>(r1), 0, 0, 0, 0, 0, 0, 0, Int128(0), Int128(0) };
        // Step 1.f: endDuration = CreateDateDurationRecord(r2, 0, 0, 0).
        endDuration = ISO8601::Duration { static_cast<int64_t>(r2), 0, 0, 0, 0, 0, 0, 0, Int128(0), Int128(0) };
        break;
    }
    case TemporalUnit::Month: {
        // Step 2.a: months = RoundNumberToIncrement(duration.[[Date]].[[Months]], increment, trunc).
        Int128 months = roundNumberToIncrementInt128((Int128)duration.dateDuration().months(), (Int128)increment, RoundingMode::Trunc);
        // Step 2.b-c: r1 = months; or if additionalShift, r1 = months + increment × sign.
        r1 = additionalShift ? (double)months + increment * sign : (double)months;
        // Step 2.d: r2 = r1 + increment × sign.
        r2 = r1 + increment * sign;
        // Step 2.e: startDuration = AdjustDateDurationRecord(duration.[[Date]], 0, 0, r1).
        auto sd = adjustDateDurationRecord(duration.dateDuration(), 0, 0, static_cast<int64_t>(r1));
        if (!sd)
            return makeUnexpected(sd.error());
        startDuration = *sd;
        // Step 2.f: endDuration = AdjustDateDurationRecord(duration.[[Date]], 0, 0, r2).
        auto ed = adjustDateDurationRecord(duration.dateDuration(), 0, 0, static_cast<int64_t>(r2));
        if (!ed)
            return makeUnexpected(ed.error());
        endDuration = *ed;
        break;
    }
    case TemporalUnit::Week: {
        // Step 3.a: yearsMonths = AdjustDateDurationRecord(duration.[[Date]], 0, 0).
        auto yearsMonths = adjustDateDurationRecord(duration.dateDuration(), 0, 0, std::nullopt);
        if (!yearsMonths)
            return makeUnexpected(yearsMonths.error());
        // Step 3.b: weeksStart = CalendarDateAdd(calendar, isoDateTime.[[ISODate]], yearsMonths, constrain).
        auto weeksStartResult = TemporalCore::calendarDateAdd(calendarId, isoDate, *yearsMonths, TemporalOverflow::Constrain);
        if (!weeksStartResult)
            return makeUnexpected(weeksStartResult.error());
        auto weeksStart = *weeksStartResult;
        // Step 3.c: weeksEnd = AddDaysToISODate(weeksStart, duration.[[Date]].[[Days]]).
        auto weeksEnd = TemporalCore::addDaysToISODate(weeksStart, duration.dateDuration().days());
        // Step 3.d: untilResult = CalendarDateUntil(calendar, weeksStart, weeksEnd, week).
        auto untilResult = TemporalCore::calendarDateUntil(calendarId, weeksStart, weeksEnd, TemporalUnit::Week);
        if (!untilResult)
            return makeUnexpected(untilResult.error());
        // Step 3.e: weeks = RoundNumberToIncrement(duration.[[Date]].[[Weeks]] + untilResult.[[Weeks]], increment, trunc).
        Int128 weeks = roundNumberToIncrementInt128((Int128)(duration.dateDuration().weeks() + untilResult->weeks()), (Int128)increment, RoundingMode::Trunc);
        // Step 3.f: r1 = weeks. Step 3.g: r2 = weeks + increment × sign.
        r1 = (double)weeks;
        r2 = (double)weeks + increment * sign;
        // Step 3.h: startDuration = AdjustDateDurationRecord(duration.[[Date]], 0, r1).
        auto sd = adjustDateDurationRecord(duration.dateDuration(), 0, static_cast<int64_t>(r1), std::nullopt);
        if (!sd)
            return makeUnexpected(sd.error());
        startDuration = *sd;
        // Step 3.i: endDuration = AdjustDateDurationRecord(duration.[[Date]], 0, r2).
        auto ed = adjustDateDurationRecord(duration.dateDuration(), 0, static_cast<int64_t>(r2), std::nullopt);
        if (!ed)
            return makeUnexpected(ed.error());
        endDuration = *ed;
        break;
    }
    default: {
        // Step 4.a: Assert unit is day.
        ASSERT(unit == TemporalUnit::Day);
        // Step 4.b: days = RoundNumberToIncrement(duration.[[Date]].[[Days]], increment, trunc).
        Int128 days = roundNumberToIncrementInt128((Int128)duration.dateDuration().days(), (Int128)increment, RoundingMode::Trunc);
        // Step 4.c: r1 = days. Step 4.d: r2 = days + increment × sign.
        r1 = (double)days;
        r2 = (double)days + increment * sign;
        // Step 4.e: startDuration = AdjustDateDurationRecord(duration.[[Date]], r1).
        auto sd = adjustDateDurationRecord(duration.dateDuration(), static_cast<int64_t>(r1), std::nullopt, std::nullopt);
        if (!sd)
            return makeUnexpected(sd.error());
        startDuration = *sd;
        // Step 4.f: endDuration = AdjustDateDurationRecord(duration.[[Date]], r2).
        auto ed = adjustDateDurationRecord(duration.dateDuration(), static_cast<int64_t>(r2), std::nullopt, std::nullopt);
        if (!ed)
            return makeUnexpected(ed.error());
        endDuration = *ed;
        break;
    }
    }

    // Step 5: Assert if sign=1, r1≥0 and r1<r2.
    ASSERT(sign != 1 || (r1 >= 0 && r1 < r2));
    // Step 6: Assert if sign=-1, r1≤0 and r1>r2.
    ASSERT(sign != -1 || (r1 <= 0 && r1 > r2));

    // Step 7: If r1=0, startEpochNs = originEpochNs.
    // Steps 7-8: startEpochNs. Use originEpochNs only when startDuration is entirely zero;
    // otherwise compute via CalendarDateAdd (fix for https://github.com/tc39/proposal-temporal/issues/3316).
    Int128 startEpochNs;
    if (!startDuration.years() && !startDuration.months() && !startDuration.weeks() && !startDuration.days())
        startEpochNs = originEpochNs;
    else {
        auto startResult = TemporalCore::calendarDateAdd(calendarId, isoDate, startDuration, TemporalOverflow::Constrain);
        if (!startResult)
            return makeUnexpected(startResult.error());
        auto start = *startResult;
        double startDayCount = dateToDaysFrom1970(start.year(), static_cast<int>(start.month()) - 1, static_cast<int>(start.day()));
        if (std::abs(startDayCount) > 1e8)
            return makeUnexpected(rangeError("date is outside the representable range"_s));
        auto startNsResult = epochNanosecondsForDateAndTime(start, isoTime, timeZone);
        if (!startNsResult)
            return makeUnexpected(startNsResult.error());
        startEpochNs = *startNsResult;
    }
    // Step 9: end = CalendarDateAdd(calendar, isoDateTime.[[ISODate]], endDuration, constrain).
    auto endResult = TemporalCore::calendarDateAdd(calendarId, isoDate, endDuration, TemporalOverflow::Constrain);
    if (!endResult)
        return makeUnexpected(endResult.error());
    auto end = *endResult;
    double endDayCount = dateToDaysFrom1970(end.year(), static_cast<int>(end.month()) - 1, static_cast<int>(end.day()));
    if (std::abs(endDayCount) > 1e8)
        return makeUnexpected(rangeError("date is outside the representable range"_s));
    // Steps 10-12: CombineISODateAndTimeRecord (step 10) + GetUTCEpochNanoseconds/GetEpochNanosecondsFor
    //              (steps 11/12) fused into epochNanosecondsForDateAndTime — avoids an intermediate endDateTime record.
    auto endNsResult = epochNanosecondsForDateAndTime(end, isoTime, timeZone);
    if (!endNsResult)
        return makeUnexpected(endNsResult.error());
    Int128 endEpochNs = *endNsResult;
    // Steps 13-14: startDuration/endDuration = CombineDateAndTimeDuration(dateDuration, 0).
    // Deferred to caller (nudgeToCalendarUnit) for efficiency — only the selected path needs the combine.
    // Step 15: Return the Record.
    return std::optional<NudgeWindow>(NudgeWindow { r1, r2, startEpochNs, endEpochNs, startDuration, endDuration });
}

// nudgeToCalendarUnit — temporal_rs: InternalDurationRecord::nudge_calendar_unit (internal step of round_relative_duration)
// https://tc39.es/proposal-temporal/#sec-temporal-nudgetocalendarunit
TemporalResult<Nudged> nudgeToCalendarUnit(int32_t sign,
    const ISO8601::InternalDuration& duration, Int128 originEpochNs, Int128 destEpochNs,
    ISO8601::PlainDate isoDate, ISO8601::PlainTime isoTime, double increment,
    TemporalUnit unit, RoundingMode roundingMode, const TimeZone* timeZone, CalendarID calendarId)
{
    // Step 1: Let didExpandCalendarUnit be false.
    // Step 2: Let nudgeWindow be ? ComputeNudgeWindow(sign, duration, originEpochNs, isoDateTime, timeZone, calendar, increment, unit, false).
    auto nudgeWindowResult = computeNudgeWindow(sign, duration, originEpochNs, isoDate, isoTime, increment, unit, false, timeZone, calendarId);
    if (!nudgeWindowResult)
        return makeUnexpected(nudgeWindowResult.error());
    ASSERT(nudgeWindowResult->has_value());
    auto nudgeWindow = **nudgeWindowResult;

    // Step 3-4: Let startEpochNs/endEpochNs be nudgeWindow.[[StartEpochNs]]/[[EndEpochNs]].
    bool didExpandCalendarUnit = false;
    // Step 5: If sign = 1, then / Step 6: Else, — check destEpochNs is in bounds; if not, retry with additionalShift=true.
    bool inBounds = (sign == 1)
        ? (nudgeWindow.startEpochNs <= destEpochNs && destEpochNs <= nudgeWindow.endEpochNs)
        : (nudgeWindow.endEpochNs <= destEpochNs && destEpochNs <= nudgeWindow.startEpochNs);
    if (!inBounds) {
        // Step 5.a / 6.a: Set nudgeWindow to ? ComputeNudgeWindow(..., true).
        auto retried = computeNudgeWindow(sign, duration, originEpochNs, isoDate, isoTime, increment, unit, true, timeZone, calendarId);
        if (!retried)
            return makeUnexpected(retried.error());
        ASSERT(retried->has_value());
        nudgeWindow = **retried;
        // Step 5.a.ii / 6.a.ii: Assert bounds hold after retry. Set didExpandCalendarUnit to true.
        ASSERT(sign != 1 || (nudgeWindow.startEpochNs <= destEpochNs && destEpochNs <= nudgeWindow.endEpochNs));
        ASSERT(sign != -1 || (nudgeWindow.endEpochNs <= destEpochNs && destEpochNs <= nudgeWindow.startEpochNs));
        didExpandCalendarUnit = true;
    }

    // Steps 7-12: Extract r1, r2, startEpochNs, endEpochNs, startDuration, endDuration from nudgeWindow.
    auto& startDuration = nudgeWindow.startDuration;
    auto& endDuration = nudgeWindow.endDuration;
    // Step 13: Assert: startEpochNs ≠ endEpochNs.
    ASSERT(nudgeWindow.startEpochNs != nudgeWindow.endEpochNs);
    // Step 14: Let progress be (destEpochNs - startEpochNs) / (endEpochNs - startEpochNs).
    Int128 progressNumerator = destEpochNs - nudgeWindow.startEpochNs;
    Int128 progressDenominator = nudgeWindow.endEpochNs - nudgeWindow.startEpochNs;
    // Step 15: Let total be r1 + progress × increment × sign.
    // (NOTE: computed via integer arithmetic before the final float division per spec note.)
    Int128 totalNumerator = Int128(static_cast<int64_t>(nudgeWindow.r1)) * progressDenominator + progressNumerator * Int128(static_cast<int64_t>(increment)) * Int128(sign);
    double total = fractionToDouble(totalNumerator, absInt128(progressDenominator)) * (progressDenominator < 0 ? -1.0 : 1.0);
    Int128 progress = progressNumerator / progressDenominator;
    // Step 16: Assert: 0 ≤ progress ≤ 1.
    ASSERT(0 <= progress && progress <= 1);
    // Step 17: Let unsignedRoundingMode be GetUnsignedRoundingMode(roundingMode, isNegative).
    UnsignedRoundingMode unsignedRoundingMode = getUnsignedRoundingMode(roundingMode, sign < 0);
    // Step 18-19: If progress = 1, roundedUnit = abs(r2); else apply unsigned rounding.
    double roundedUnit = std::abs(nudgeWindow.r2);
    if (progress != 1) {
        ASSERT(std::abs(nudgeWindow.r1) <= std::abs(total) && std::abs(total) < std::abs(nudgeWindow.r2));
        roundedUnit = applyUnsignedRoundingMode(std::abs(total), std::abs(nudgeWindow.r1), std::abs(nudgeWindow.r2), unsignedRoundingMode);
    }
    // Step 20: If roundedUnit = abs(r2), set didExpandCalendarUnit to true and use endDuration/endEpochNs; else use start.
    didExpandCalendarUnit |= (roundedUnit == std::abs(nudgeWindow.r2));
    ISO8601::Duration resultDuration = (roundedUnit == std::abs(nudgeWindow.r2)) ? endDuration : startDuration;
    Int128 nudgedEpochNs = (roundedUnit == std::abs(nudgeWindow.r2)) ? nudgeWindow.endEpochNs : nudgeWindow.startEpochNs;
    // Step 21: Let nudgeResult be Duration Nudge Result Record { [[Duration]]: resultDuration, [[NudgedEpochNs]]: nudgedEpochNs, [[DidExpandCalendarUnit]]: didExpandCalendarUnit }.
    // (computeNudgeWindow steps 13-14 deferred here: CombineDateAndTimeDuration applied only to the selected path.)
    auto resultDurationInternal = ISO8601::InternalDuration::combineDateAndTimeDuration(resultDuration, 0);
    // Step 22: Return the Record { [[NudgeResult]]: nudgeResult, [[Total]]: total }.
    return Nudged(NudgeResult(resultDurationInternal, nudgedEpochNs, didExpandCalendarUnit), total);
}

// nudgeToZonedTime — temporal_rs: InternalDurationRecord::nudge_to_zoned_time (src/builtins/core/duration/normalized.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-nudgetozonedtime
TemporalResult<NudgeResult> nudgeToZonedTime(int32_t sign,
    const ISO8601::InternalDuration& duration, ISO8601::PlainDate isoDate,
    ISO8601::PlainTime isoTime, const TimeZone& timeZone, double increment,
    TemporalUnit unit, RoundingMode roundingMode, CalendarID calendarId)
{
    // Step 1: Let start be ? CalendarDateAdd(calendar, isoDateTime.[[ISODate]], duration.[[Date]], ~constrain~).
    auto startResult = TemporalCore::calendarDateAdd(calendarId, isoDate, duration.dateDuration(), TemporalOverflow::Constrain);
    if (!startResult)
        return makeUnexpected(startResult.error());
    auto start = *startResult;
    // Step 2: startDateTime = CombineISODateAndTimeRecord(start, isoDateTime.[[Time]]).
    // Step 3: endDate = AddDaysToISODate(start, sign).
    // Step 4: endDateTime = CombineISODateAndTimeRecord(endDate, isoDateTime.[[Time]]).
    // (Steps 2/4 fused into epochNanosecondsForDateAndTime — no intermediate records.)
    auto endDate = TemporalCore::addDaysToISODate(start, sign);
    // Step 5: startEpochNs = GetEpochNanosecondsFor(timeZone, startDateTime, compatible). (steps 2+5 fused)
    auto startNsResult = epochNanosecondsForDateAndTime(start, isoTime, &timeZone);
    if (!startNsResult)
        return makeUnexpected(startNsResult.error());
    Int128 startEpochNs = *startNsResult;
    // Step 6: endEpochNs = GetEpochNanosecondsFor(timeZone, endDateTime, compatible). (steps 4+6 fused)
    auto endNsResult = epochNanosecondsForDateAndTime(endDate, isoTime, &timeZone);
    if (!endNsResult)
        return makeUnexpected(endNsResult.error());
    Int128 endEpochNs = *endNsResult;
    // Step 7: daySpan = TimeDurationFromEpochNanosecondsDifference(endEpochNs, startEpochNs).
    Int128 daySpan = endEpochNs - startEpochNs;
    // Step 8: Assert: TimeDurationSign(daySpan) = sign.
    ASSERT((daySpan < 0 ? -1 : daySpan > 0 ? 1 : 0) == sign);
    // Step 9: Let unitLength be the nanoseconds length of unit.
    Int128 unitLength = lengthInNanoseconds(unit);
    // Step 10: Let roundedTimeDuration be ? RoundTimeDurationToIncrement(duration.[[Time]], increment × unitLength, roundingMode).
    Int128 roundedTimeDuration = roundNumberToIncrementInt128(duration.time(), unitLength * (Int128)std::trunc(increment), roundingMode);
    // Step 11: Let beyondDaySpan be ! AddTimeDuration(roundedTimeDuration, -daySpan).
    Int128 beyondDaySpan = roundedTimeDuration - daySpan;
    int32_t beyondSign = beyondDaySpan < 0 ? -1 : beyondDaySpan > 0 ? 1 : 0;
    // Step 12: If TimeDurationSign(beyondDaySpan) ≠ -sign (didRoundBeyondDay already set above), then
    bool didRoundBeyondDay = (beyondSign != -sign);
    int32_t dayDelta = 0;
    Int128 nudgedEpochNs;
    if (didRoundBeyondDay) {
        // Step 12.b: dayDelta = sign.
        dayDelta = sign;
        // Step 12.c: Set roundedTimeDuration to ? RoundTimeDurationToIncrement(beyondDaySpan, ...).
        roundedTimeDuration = roundNumberToIncrementInt128(beyondDaySpan, unitLength * (Int128)std::trunc(increment), roundingMode);
        // Step 12.d: nudgedEpochNs = AddTimeDurationToEpochNanoseconds(roundedTimeDuration, endEpochNs).
        nudgedEpochNs = roundedTimeDuration + endEpochNs;
    } else {
        // Step 13.b: dayDelta = 0. (already initialized)
        // Step 13.c: nudgedEpochNs = AddTimeDurationToEpochNanoseconds(roundedTimeDuration, startEpochNs).
        nudgedEpochNs = roundedTimeDuration + startEpochNs;
    }
    // Step 14: Let dateDuration be ! AdjustDateDurationRecord(duration.[[Date]], duration.[[Date]].[[Days]] + dayDelta).
    auto dateDurationResult = adjustDateDurationRecord(duration.dateDuration(), duration.dateDuration().days() + dayDelta, std::nullopt, std::nullopt);
    if (!dateDurationResult)
        return makeUnexpected(dateDurationResult.error());
    // Step 15: Let resultDuration be CombineDateAndTimeDuration(dateDuration, roundedTimeDuration).
    auto resultDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(*dateDurationResult, roundedTimeDuration);
    // Step 16: Return Duration Nudge Result Record { [[Duration]]: resultDuration, [[NudgedEpochNs]]: nudgedEpochNs, [[DidExpandCalendarUnit]]: didRoundBeyondDay }.
    return NudgeResult(resultDuration, nudgedEpochNs, didRoundBeyondDay);
}

// nudgeToDayOrTime — temporal_rs: InternalDurationRecord::nudge_to_day_or_time (src/builtins/core/duration/normalized.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-nudgetodayortime
TemporalResult<NudgeResult> nudgeToDayOrTime(ISO8601::InternalDuration duration,
    Int128 destEpochNs, TemporalUnit largestUnit, double increment,
    TemporalUnit smallestUnit, RoundingMode roundingMode)
{
    // Step 1: Let timeDuration be ! Add24HourDaysToTimeDuration(duration.[[Time]], duration.[[Date]].[[Days]]).
    auto timeDurationResult = add24HourDaysToTimeDuration(duration.time(), duration.dateDuration().days());
    if (!timeDurationResult)
        return makeUnexpected(timeDurationResult.error());
    Int128 timeDuration = *timeDurationResult;
    // Step 2: Let unitLength be the nanoseconds length of smallestUnit.
    Int128 unitLength = lengthInNanoseconds(smallestUnit);
    // Step 3: Let roundedTime be ? RoundTimeDurationToIncrement(timeDuration, unitLength × increment, roundingMode).
    Int128 roundedTime = roundNumberToIncrementInt128(timeDuration, unitLength * (Int128)std::trunc(increment), roundingMode);
    // Step 4: Let diffTime be ! AddTimeDuration(roundedTime, -timeDuration).
    Int128 diffTime = roundedTime - timeDuration;
    // Step 5: wholeDays = truncate(TotalTimeDuration(timeDuration, day)).
    // (totalTimeDuration returns int64_t-truncated double, so truncate is implicit.)
    double wholeDays = totalTimeDuration(timeDuration, TemporalUnit::Day);
    // Step 6: roundedWholeDays = truncate(TotalTimeDuration(roundedTime, day)). (same)
    double roundedWholeDays = totalTimeDuration(roundedTime, TemporalUnit::Day);
    // Step 7: Let dayDelta be roundedWholeDays - wholeDays.
    auto dayDelta = roundedWholeDays - wholeDays;
    // Step 8: If dayDelta < 0, let dayDeltaSign be -1; else if dayDelta > 0, let dayDeltaSign be 1; else 0.
    auto dayDeltaSign = dayDelta < 0 ? -1 : dayDelta > 0 ? 1 : 0;
    // Step 9: If dayDeltaSign = TimeDurationSign(timeDuration), let didExpandDays be true; else false.
    bool didExpandDays = dayDeltaSign == (timeDuration < 0 ? -1 : timeDuration > 0 ? 1 : 0);
    // Step 10: Let nudgedEpochNs be AddTimeDurationToEpochNanoseconds(diffTime, destEpochNs).
    auto nudgedEpochNs = diffTime + destEpochNs;
    // Step 11: Let days be 0.
    // Step 12: Let remainder be roundedTime.
    int64_t days = 0;
    auto remainder = roundedTime;
    // Step 13: If TemporalUnitCategory(largestUnit) is ~date~, then
    if (largestUnit <= TemporalUnit::Day) {
        // Step 13.a: Set days to roundedWholeDays.
        days = static_cast<int64_t>(roundedWholeDays);
        // Step 13.b: remainder = roundedTime - roundedWholeDays×hoursPerDay (days==roundedWholeDays here).
        remainder = roundedTime + timeDurationFromComponents(-static_cast<double>(days) * WTF::hoursPerDay, 0, 0, 0, 0, 0);
    }
    // Step 14: Let dateDuration be ! AdjustDateDurationRecord(duration.[[Date]], days).
    auto dateDurationResult = adjustDateDurationRecord(duration.dateDuration(), days, std::nullopt, std::nullopt);
    if (!dateDurationResult)
        return makeUnexpected(dateDurationResult.error());
    // Step 15: Let resultDuration be CombineDateAndTimeDuration(dateDuration, remainder).
    auto resultDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(*dateDurationResult, remainder);
    // Step 16: Return Duration Nudge Result Record { [[Duration]]: resultDuration, [[NudgedEpochNs]]: nudgedEpochNs, [[DidExpandCalendarUnit]]: didExpandDays }.
    return NudgeResult(resultDuration, nudgedEpochNs, didExpandDays);
}

// bubbleRelativeDuration — temporal_rs: InternalDurationRecord::bubble_relative_duration (src/builtins/core/duration/normalized.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-bubblerelativeduration
TemporalResult<ISO8601::InternalDuration> bubbleRelativeDuration(
    int32_t sign, ISO8601::InternalDuration duration, Int128 nudgedEpochNs,
    ISO8601::PlainDate isoDate, ISO8601::PlainTime isoTime,
    TemporalUnit largestUnit, TemporalUnit smallestUnit,
    const TimeZone* timeZone, CalendarID calendarId)
{
    // Step 1: If smallestUnit is largestUnit, return duration.
    if (smallestUnit == largestUnit)
        return duration;
    // Step 2: Let largestUnitIndex be the ordinal index of largestUnit in the units table.
    auto largestUnitIndex = unitIndexInTable(largestUnit);
    // Step 3: Let smallestUnitIndex be the ordinal index of smallestUnit in the units table.
    auto smallestUnitIndex = unitIndexInTable(smallestUnit);
    // Step 4: Let unitIndex be smallestUnitIndex - 1.
    auto unitIndex = smallestUnitIndex - 1;
    // Step 5: Let done be false.
    bool done = false;
    ISO8601::Duration endDuration;
    // Step 6: Repeat, while unitIndex ≥ largestUnitIndex and done is false,
    while (unitIndex >= largestUnitIndex && !done) {
        // Step 6.a: Let unit be the unit at ordinal unitIndex in the units table.
        auto unit = unitInTable(unitIndex);
        // Step 6.b: If unit is not ~week~, or largestUnit is ~week~, then
        if (unit != TemporalUnit::Week || largestUnit == TemporalUnit::Week) {
            // Step 6.b.i: If unit is ~year~, let endDuration be ? CreateDateDurationRecord(years + sign, 0, 0, 0).
            if (unit == TemporalUnit::Year) {
                endDuration = ISO8601::Duration { duration.dateDuration().years() + sign, 0, 0, 0, 0, 0, 0, 0, Int128(0), Int128(0) };
            } else if (unit == TemporalUnit::Month) {
                // Step 6.b.ii: Else if unit is ~month~, let endDuration be ? AdjustDateDurationRecord(duration.[[Date]], 0, 0, months + sign).
                auto r = adjustDateDurationRecord(duration.dateDuration(), 0, 0, duration.dateDuration().months() + sign);
                if (!r)
                    return makeUnexpected(r.error());
                endDuration = *r;
            } else {
                // Step 6.b.iii: Else (unit is ~week~), let endDuration be ? AdjustDateDurationRecord(duration.[[Date]], 0, weeks + sign).
                auto r = adjustDateDurationRecord(duration.dateDuration(), 0, duration.dateDuration().weeks() + sign, std::nullopt);
                if (!r)
                    return makeUnexpected(r.error());
                endDuration = *r;
            }
            // Step 6.b.iv: Let end be ? CalendarDateAdd(calendar, isoDateTime.[[ISODate]], endDuration, ~constrain~).
            auto endResult = TemporalCore::calendarDateAdd(calendarId, isoDate, endDuration, TemporalOverflow::Constrain);
            if (!endResult)
                return makeUnexpected(endResult.error());
            // Step 6.b.v: endDateTime = CombineISODateAndTimeRecord(end, isoDateTime.[[Time]]).
            // Step 6.b.vi: endEpochNs = GetUTCEpochNanoseconds/GetEpochNanosecondsFor. (v+vi fused)
            auto endNsResult = epochNanosecondsForDateAndTime(*endResult, isoTime, timeZone);
            if (!endNsResult)
                return makeUnexpected(endNsResult.error());
            // Step 6.b.vii: Let beyondEnd be nudgedEpochNs - endEpochNs.
            auto beyondEnd = nudgedEpochNs - *endNsResult;
            // Step 6.b.viii: If beyondEnd < 0, beyondEndSign = -1; > 0, = 1; else 0.
            auto beyondEndSign = beyondEnd < 0 ? -1 : beyondEnd > 0 ? 1 : 0;
            // Step 6.b.ix: If beyondEndSign ≠ -sign, set duration to CombineDateAndTimeDuration(endDuration, 0).
            if (beyondEndSign != -sign)
                duration = ISO8601::InternalDuration::combineDateAndTimeDuration(endDuration, 0);
            // Step 6.b.x: Else, set done to true.
            else
                done = true;
        }
        // Step 6.c: Set unitIndex to unitIndex - 1.
        unitIndex--;
    }
    // Step 7: Return duration.
    return duration;
}

// roundRelativeDuration — temporal_rs: InternalDurationRecord::round_relative_duration (src/builtins/core/duration/normalized.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-roundrelativeduration
TemporalResult<void> roundRelativeDuration(ISO8601::InternalDuration& duration,
    Int128 originEpochNs, Int128 destEpochNs, ISO8601::PlainDate isoDate, ISO8601::PlainTime isoTime,
    TemporalUnit largestUnit, double increment, TemporalUnit smallestUnit, RoundingMode roundingMode,
    const TimeZone* timeZone, CalendarID calendarId)
{
    // Step 1: Let irregularLengthUnit be false.
    // Step 2: If IsCalendarUnit(smallestUnit) is true, set irregularLengthUnit to true.
    // Step 3: If timeZone is not ~unset~ and smallestUnit is ~day~, set irregularLengthUnit to true.
    bool irregularLengthUnit = isCalendarUnit(smallestUnit);
    if (timeZone && smallestUnit == TemporalUnit::Day)
        irregularLengthUnit = true;
    // Step 4: If InternalDurationSign(duration) < 0, let sign be -1; else let sign be 1.
    int32_t sign = (duration.sign() < 0) ? -1 : 1;

    NudgeResult nudgeResult;
    // Step 5: If irregularLengthUnit is true, then
    if (irregularLengthUnit) {
        // Step 5.a: Let record be ? NudgeToCalendarUnit(...).
        auto record = nudgeToCalendarUnit(sign, duration, originEpochNs, destEpochNs, isoDate, isoTime, increment, smallestUnit, roundingMode, timeZone, calendarId);
        if (!record)
            return makeUnexpected(record.error());
        // Step 5.b: Let nudgeResult be record.[[NudgeResult]].
        nudgeResult = record->nudgeResult;
        // Step 5.c: Let total be record.[[Total]]. (total not needed by round/until/since callers; dropped)
    } else if (timeZone) {
        // Step 6.a: Let nudgeResult be ? NudgeToZonedTime(...).
        auto result = nudgeToZonedTime(sign, duration, isoDate, isoTime, *timeZone, increment, smallestUnit, roundingMode, calendarId);
        if (!result)
            return makeUnexpected(result.error());
        nudgeResult = *result;
    } else {
        // Step 7.a: Let nudgeResult be ? NudgeToDayOrTime(...).
        auto result = nudgeToDayOrTime(duration, destEpochNs, largestUnit, increment, smallestUnit, roundingMode);
        if (!result)
            return makeUnexpected(result.error());
        nudgeResult = *result;
    }
    // Step 8: Set duration to nudgeResult.[[Duration]].
    duration = nudgeResult.duration;
    // Step 9: If nudgeResult.[[DidExpandCalendarUnit]] is true and smallestUnit is not ~week~, then
    if (nudgeResult.didExpandCalendarUnit && smallestUnit != TemporalUnit::Week) {
        // Step 9.a: Let startUnit be LargerOfTwoTemporalUnits(smallestUnit, ~day~).
        auto startUnit = smallestUnit <= TemporalUnit::Day ? smallestUnit : TemporalUnit::Day;
        // Step 9.b: Set duration to ? BubbleRelativeDuration(sign, duration, nudgeResult.[[NudgedEpochNs]], isoDateTime, timeZone, calendar, largestUnit, startUnit).
        auto bubbled = bubbleRelativeDuration(sign, duration, nudgeResult.nudgedEpochNs, isoDate, isoTime, largestUnit, startUnit, timeZone, calendarId);
        if (!bubbled)
            return makeUnexpected(bubbled.error());
        duration = *bubbled;
    }
    // Step 10: Return duration.
    return { };
}

} // namespace TemporalCore
} // namespace JSC
