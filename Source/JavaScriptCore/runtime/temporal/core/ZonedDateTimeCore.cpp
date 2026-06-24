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
#include "ZonedDateTimeCore.h"

#include "CalendarICUBridge.h"
#include "DurationArithmetic.h"
#include "ISO8601.h"
#include "ISOArithmetic.h"
#include "Rounding.h"
#include "TemporalObject.h"
#include "TimeZoneICUBridge.h"
#include <span>
#include <wtf/DateMath.h>

namespace JSC {
namespace TemporalCore {

// InterpretISODateTimeOffset — temporal_rs: interpret_isodatetime_offset in zoned_date_time.rs
// https://tc39.es/proposal-temporal/#sec-temporal-interpretisodatetimeoffset
TemporalResult<ISO8601::ExactTime> interpretISODateTimeOffset(
    const ISO8601::PlainDate& date,
    const ISO8601::PlainTime& time,
    bool useStartOfDay,
    OffsetBehaviour offsetBehaviour,
    TemporalOffsetDisambiguation offsetOpt,
    int64_t inlineOffsetNs,
    bool offsetHasSubMinutePrecision,
    const TimeZone& timeZone,
    TemporalDisambiguation disambiguation)
{
    // Step 1: If time is start-of-day, then
    if (useStartOfDay) {
        ASSERT(offsetBehaviour == OffsetBehaviour::Wall); // 1.a
        ASSERT(!inlineOffsetNs); // 1.b
        return getStartOfDay(timeZone, date); // 1.c
    }

    // Step 2: isoDateTime = CombineISODateAndTimeRecord(isoDate, time). (implicit — date+time passed separately)
    // Step 3: If offsetBehaviour is wall, or offsetBehaviour is option and offsetOption is ignore.
    if (offsetBehaviour == OffsetBehaviour::Wall
        || (offsetBehaviour == OffsetBehaviour::Option && offsetOpt == TemporalOffsetDisambiguation::Ignore))
        return getEpochNanosecondsFor(timeZone, date, time, disambiguation);

    // Step 4: offsetBehaviour is exact or option+use: compute epoch ns = UTC(date,time) - offset.
    // Steps 4a-4d fused: GetUTCEpochNanoseconds(date,time) - offset ≡ BalanceISODateTime + GetUTCEpochNanoseconds
    // since both are linear; IsValidEpochNanoseconds ↔ CheckISODaysRange (same ±10^8 day bound).
    if (offsetBehaviour == OffsetBehaviour::Exact
        || (offsetBehaviour == OffsetBehaviour::Option && offsetOpt == TemporalOffsetDisambiguation::Use)) {
        Int128 naiveNs = getUTCEpochNanoseconds(date, time);
        ISO8601::ExactTime result(naiveNs - Int128(inlineOffsetNs));
        if (!result.isValid())
            return makeUnexpected(rangeError("date/time offset combination is outside the supported range for Temporal.ZonedDateTime"_s));
        return result;
    }

    // Step 5: Assert offsetBehaviour is option.
    ASSERT(offsetBehaviour == OffsetBehaviour::Option);
    // Step 6: Assert offsetOption is prefer or reject.
    ASSERT(offsetOpt == TemporalOffsetDisambiguation::Prefer || offsetOpt == TemporalOffsetDisambiguation::Reject);
    // Step 7: CheckISODaysRange(isoDate).
    if (std::abs(dateToDaysFrom1970(date.year(), static_cast<int>(date.month()) - 1, static_cast<int>(date.day()))) > 1e8)
        return makeUnexpected(rangeError("wall-clock date is outside the representable range for Temporal.ZonedDateTime"_s));

    // Step 8: utcEpochNanoseconds = GetUTCEpochNanoseconds(isoDateTime).
    Int128 utcEpochNs = getUTCEpochNanoseconds(date, time);

    // Step 9: possibleEpochNs = GetPossibleEpochNanoseconds(timeZone, isoDateTime).
    auto possible = getPossibleEpochNanosecondsFor(timeZone, date, time);
    if (!possible)
        return makeUnexpected(possible.error());

    // Step 10: For each element candidate of possibleEpochNs.
    bool matchMinutes = !offsetHasSubMinutePrecision;
    for (auto& candidate : epochCandidates(*possible)) {
        // Step 10a: candidateOffset = utcEpochNanoseconds - candidate.
        Int128 candidateOffset = utcEpochNs - candidate.epochNanoseconds();
        // Step 10b: If candidateOffset = offsetNanoseconds, return candidate.
        if (candidateOffset == Int128(inlineOffsetNs))
            return candidate;
        // Step 10c: If matchBehaviour is match-minutes, check rounded.
        if (matchMinutes) {
            if (roundNumberToIncrementInt128(candidateOffset, ISO8601::ExactTime::nsPerMinute, RoundingMode::HalfExpand) == Int128(inlineOffsetNs))
                return candidate;
        }
    }

    // Step 11: If offsetOption is reject, throw RangeError.
    if (offsetOpt == TemporalOffsetDisambiguation::Reject)
        return makeUnexpected(rangeError("offset does not agree with timezone for the given date/time"_s));

    // Step 12: Return ? DisambiguatePossibleEpochNanoseconds(possibleEpochNs, timeZone, isoDateTime, disambiguation).
    // Note: getEpochNanosecondsFor recomputes possibleEpochNs; functionally equivalent.
    return getEpochNanosecondsFor(timeZone, date, time, disambiguation);
}

// GetStartOfDay — temporal_rs: TimeZone::get_start_of_day(iso_date, provider)
// https://tc39.es/proposal-temporal/#sec-temporal-getstartofday
TemporalResult<ISO8601::ExactTime> getStartOfDay(const TimeZone& tz, ISO8601::PlainDate date)
{
    // Step 1: isoDateTime = CombineISODateAndTimeRecord(isoDate, MidnightTimeRecord()).
    ISO8601::PlainTime midnight;
    // Step 2: possibleEpochNs = GetPossibleEpochNanoseconds(timeZone, isoDateTime).
    auto possible = getPossibleEpochNanosecondsFor(tz, date, midnight);
    if (!possible)
        return makeUnexpected(possible.error());
    // Step 3: If possibleEpochNs is not empty, return possibleEpochNs[0].
    if (!isGap(*possible)) {
        auto candidate = epochCandidates(*possible)[0];
        // Validate: the start-of-day epoch must be within Temporal range.
        // e.g. midnight of +275760-09-13 in America/Vancouver = UTC+7h > max epoch.
        if (!candidate.isValid())
            return makeUnexpected(rangeError("start of day is outside the representable range of Temporal.ZonedDateTime"_s));
        return candidate;
    }
    // Step 4: Assert IsOffsetTimeZoneIdentifier(timeZone) is false (gap is only possible for IANA zones).
    ASSERT(tz.isID());
    // Step 5: possibleEpochNsAfter = first instant after the gap transition.
    // We find it via: getEpochNanosecondsFor(Earlier) -> last instant before gap,
    // then getTimeZoneTransition(Next) -> first instant after gap.
    auto beforeGap = getEpochNanosecondsFor(tz, date, midnight, TemporalDisambiguation::Earlier);
    if (!beforeGap)
        return makeUnexpected(beforeGap.error());
    auto transition = getTimeZoneTransition(tz, *beforeGap, TransitionDirection::Next);
    if (!transition)
        return makeUnexpected(transition.error());
    if (!transition->has_value())
        return makeUnexpected(rangeError("no start of day: time zone has no future transitions"_s));
    // Step 6: Assert: The number of elements in possibleEpochNsAfter = 1.
    // Step 7: Return the sole element of possibleEpochNsAfter.
    return transition->value();
}

// DifferenceZonedDateTime — temporal_rs: ZonedDateTime::diff_zoned_datetime inner path
// https://tc39.es/proposal-temporal/#sec-temporal-differencezoneddatetime
static TemporalResult<ISO8601::InternalDuration> differenceZonedDateTime(ISO8601::ExactTime ns1, ISO8601::ExactTime ns2, const TimeZone& timeZone, CalendarID calendarId, TemporalUnit largestUnit)
{
    Int128 nsA = ns1.epochNanoseconds();
    Int128 nsB = ns2.epochNanoseconds();

    // Step 1: If ns1 = ns2, return zero.
    if (nsA == nsB)
        return ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), 0);

    // Steps 2–3: GetISODateTimeFor(timeZone, ns1/ns2). (split: getOffsetNanosecondsFor + exactTimeToLocalDateAndTime)
    auto offset1Result = getOffsetNanosecondsFor(timeZone, ns1);
    if (!offset1Result)
        return makeUnexpected(offset1Result.error());
    auto offset2Result = getOffsetNanosecondsFor(timeZone, ns2);
    if (!offset2Result)
        return makeUnexpected(offset2Result.error());
    ISO8601::PlainDate startDate, endDate;
    ISO8601::PlainTime startTime, endTime;
    exactTimeToLocalDateAndTime(ns1, *offset1Result, startDate, startTime);
    exactTimeToLocalDateAndTime(ns2, *offset2Result, endDate, endTime);

    // Step 4: If startDate = endDate -> timeDuration = ns2 - ns1, return.
    if (!isoDateCompare(startDate, endDate))
        return ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), nsB - nsA);

    // Step 5: sign = (ns2 - ns1 < 0) ? 1 : -1.
    int32_t sign = (nsB - nsA < Int128 { 0 }) ? 1 : -1;
    // Step 6: maxDayCorrection = (sign=-1) ? 2 : 1.
    int32_t maxDayCorrection = (sign == -1) ? 2 : 1;
    // Step 7: dayCorrection = 0.
    int32_t dayCorrection = 0;
    // Step 8: timeDuration = DifferenceTime(startTime, endTime).
    Int128 timeDiff = timeDurationFromComponents(
        static_cast<double>(endTime.hour()) - static_cast<double>(startTime.hour()),
        static_cast<double>(endTime.minute()) - static_cast<double>(startTime.minute()),
        static_cast<double>(endTime.second()) - static_cast<double>(startTime.second()),
        static_cast<double>(endTime.millisecond()) - static_cast<double>(startTime.millisecond()),
        static_cast<double>(endTime.microsecond()) - static_cast<double>(startTime.microsecond()),
        static_cast<double>(endTime.nanosecond()) - static_cast<double>(startTime.nanosecond()));
    // Step 9: If TimeDurationSign(timeDuration) = sign -> dayCorrection++.
    int32_t timeSign = (timeDiff < 0) ? -1 : (timeDiff > 0) ? 1 : 0;
    if (timeSign == sign)
        dayCorrection++;

    // Step 10: success = false.
    // Step 11: Repeat while dayCorrection ≤ maxDayCorrection and success = false.
    ISO8601::PlainDate intermediateDate = endDate;
    Int128 adjustedTimeDiff = timeDiff;
    bool success = false;
    while (dayCorrection <= maxDayCorrection && !success) {
        // Step 11.a: intermediateDate = AddDaysToISODate(endDate, dayCorrection × sign).
        intermediateDate = addDaysToISODate(endDate, static_cast<int64_t>(dayCorrection) * sign);
        // Step 11.b: intermediateDateTime = CombineISODateAndTimeRecord(intermediateDate, startTime).
        // Step 11.c: intermediateNs = GetEpochNanosecondsFor(timeZone, intermediateDateTime, compatible). (11.b+11.c fused)
        auto intermediateNsResult = getEpochNanosecondsFor(timeZone, intermediateDate, startTime, TemporalDisambiguation::Compatible);
        if (!intermediateNsResult)
            return makeUnexpected(intermediateNsResult.error());
        // Step 11.d: timeDuration = TimeDurationFromEpochNanosecondsDifference(ns2, intermediateNs).
        adjustedTimeDiff = nsB - intermediateNsResult->epochNanoseconds();
        // Step 11.e: timeSign = TimeDurationSign(timeDuration).
        int32_t adjTimeSign = (adjustedTimeDiff < 0) ? -1 : (adjustedTimeDiff > 0) ? 1 : 0;
        // Step 11.f: If sign ≠ timeSign, success = true.
        if (sign != adjTimeSign)
            success = true;
        // Step 11.g: dayCorrection++.
        dayCorrection++;
    }
    // Step 12: Assert: success is true.
    ASSERT(success);

    // Step 13: dateLargestUnit = LargerOfTwoTemporalUnits(largestUnit, day).
    TemporalUnit dateLargestUnit = (largestUnit > TemporalUnit::Day) ? TemporalUnit::Day : largestUnit;
    // Step 14: dateDifference = CalendarDateUntil(startDate, intermediateDate, dateLargestUnit).
    auto dateDiffResult = TemporalCore::calendarDateUntil(calendarId, startDate, intermediateDate, dateLargestUnit);
    if (!dateDiffResult)
        return makeUnexpected(dateDiffResult.error());
    auto& dateDiff = *dateDiffResult;

    // Step 15: Return CombineDateAndTimeDuration(dateDifference, timeDuration).
    ISO8601::Duration datePart(static_cast<double>(dateDiff.years()), static_cast<double>(dateDiff.months()),
        static_cast<double>(dateDiff.weeks()), static_cast<double>(dateDiff.days()), 0, 0, 0, 0, 0, 0);
    return ISO8601::InternalDuration::combineDateAndTimeDuration(datePart, adjustedTimeDiff);
}

// DifferenceZonedDateTimeWithRounding — temporal_rs: ZonedDateTime::diff_zoned_datetime (src/builtins/core/zoned_date_time.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-differencezoneddatetimewithrounding
TemporalResult<ISO8601::InternalDuration> differenceZonedDateTimeWithRounding(
    ISO8601::ExactTime ns1,
    ISO8601::ExactTime ns2,
    const TimeZone& timeZone,
    TemporalUnit largestUnit,
    TemporalUnit smallestUnit,
    RoundingMode roundingMode,
    double increment,
    CalendarID calendarId)
{
    Int128 nsA = ns1.epochNanoseconds();
    Int128 nsB = ns2.epochNanoseconds();

    // Step 1: If largestUnit is time category -> DifferenceInstant (pure nanosecond arithmetic).
    if (largestUnit > TemporalUnit::Day) {
        Int128 nsTotal = nsB - nsA;
        ISO8601::InternalDuration internalDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), nsTotal);
        // Step 3: If smallestUnit=nanosecond and increment=1 -> return difference without rounding.
        if (smallestUnit != TemporalUnit::Nanosecond || increment != 1) {
            Int128 roundedTime = roundNumberToIncrementInt128(internalDuration.time(),
                lengthInNanoseconds(smallestUnit) * (Int128)std::trunc(increment), roundingMode);
            internalDuration = ISO8601::InternalDuration::combineDateAndTimeDuration(ISO8601::Duration(), roundedTime);
        }
        return internalDuration;
    }

    // Step 2: difference = DifferenceZonedDateTime(ns1, ns2, timeZone, calendar, largestUnit).
    auto differenceResult = differenceZonedDateTime(ns1, ns2, timeZone, calendarId, largestUnit);
    if (!differenceResult)
        return makeUnexpected(differenceResult.error());
    auto internalDuration = *differenceResult;

    // Step 3: If smallestUnit=nanosecond and increment=1 -> return difference.
    if (smallestUnit == TemporalUnit::Nanosecond && increment == 1)
        return internalDuration;

    // Step 4: dateTime = GetISODateTimeFor(timeZone, ns1). (split: getOffsetNanosecondsFor + exactTimeToLocalDateAndTime)
    auto offset1 = getOffsetNanosecondsFor(timeZone, ns1);
    if (!offset1)
        return makeUnexpected(offset1.error());
    ISO8601::PlainDate startDate;
    ISO8601::PlainTime startTime;
    exactTimeToLocalDateAndTime(ns1, *offset1, startDate, startTime);

    // Step 5: Return RoundRelativeDuration(difference, ns1, ns2, dateTime, ...).
    auto roundResult = roundRelativeDuration(internalDuration, nsA, nsB, startDate, startTime,
        largestUnit, increment, smallestUnit, roundingMode, &timeZone, calendarId);
    if (!roundResult)
        return makeUnexpected(roundResult.error());
    return internalDuration;
}

} // namespace TemporalCore
} // namespace JSC
