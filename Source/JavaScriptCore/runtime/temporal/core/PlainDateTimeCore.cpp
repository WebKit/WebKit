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
#include "PlainDateTimeCore.h"

#include "CalendarICUBridge.h"
#include "DurationArithmetic.h"
#include "ISO8601.h"
#include "ISOArithmetic.h"
#include "TemporalObject.h"

namespace JSC {
namespace TemporalCore {

// CompareISODateTime — temporal_rs: PlainDateTime::compare_iso (pure)
// https://tc39.es/proposal-temporal/#sec-temporal-compareisodatetime
int32_t compareISODateTime(ISO8601::PlainDate d1, ISO8601::PlainTime t1, ISO8601::PlainDate d2, ISO8601::PlainTime t2)
{
    // Step 1: Let dateResult be CompareISODate(isoDateTime1.[[ISODate]], isoDateTime2.[[ISODate]]).
    // Step 2: If dateResult ≠ 0, return dateResult.
    if (auto r = isoDateCompare(d1, d2))
        return r;
    // Step 3: Return CompareTimeRecord(isoDateTime1.[[Time]], isoDateTime2.[[Time]]).
    return isoTimeCompare(t1, t2);
}

// DifferencePlainDateTimeWithRounding — temporal_rs: PlainDateTime::diff inner path
// https://tc39.es/proposal-temporal/#sec-temporal-differenceplaindatetimewithrounding
TemporalResult<ISO8601::InternalDuration> differencePlainDateTimeWithRounding(
    ISO8601::PlainDate thisDate, ISO8601::PlainTime thisTime,
    ISO8601::PlainDate otherDate, ISO8601::PlainTime otherTime,
    CalendarID calendarId,
    TemporalUnit largestUnit, TemporalUnit smallestUnit,
    RoundingMode roundingMode, double increment)
{
    // Step 1: If CompareISODateTime(isoDateTime1, isoDateTime2) = 0, return zero InternalDuration.
    if (thisDate == otherDate && thisTime == otherTime)
        return ISO8601::InternalDuration();

    // Step 2: If ISODateTimeWithinLimits is false for either endpoint, throw a RangeError.
    if (!ISO8601::isDateTimeWithinLimits(thisDate.year(), thisDate.month(), thisDate.day(), thisTime.hour(), thisTime.minute(), thisTime.second(), thisTime.millisecond(), thisTime.microsecond(), thisTime.nanosecond())
        || !ISO8601::isDateTimeWithinLimits(otherDate.year(), otherDate.month(), otherDate.day(), otherTime.hour(), otherTime.minute(), otherTime.second(), otherTime.millisecond(), otherTime.microsecond(), otherTime.nanosecond()))
        return makeUnexpected(rangeError("date-time is outside the representable range for Temporal"_s));

    // Step 3: diff = DifferenceISODateTime(isoDateTime1, isoDateTime2, calendar, largestUnit).
    ISO8601::InternalDuration diff;
    if (calendarId != iso8601CalendarID()) {
        Int128 timeDiff = timeDurationFromComponents(
            static_cast<double>(otherTime.hour()) - static_cast<double>(thisTime.hour()),
            static_cast<double>(otherTime.minute()) - static_cast<double>(thisTime.minute()),
            static_cast<double>(otherTime.second()) - static_cast<double>(thisTime.second()),
            static_cast<double>(otherTime.millisecond()) - static_cast<double>(thisTime.millisecond()),
            static_cast<double>(otherTime.microsecond()) - static_cast<double>(thisTime.microsecond()),
            static_cast<double>(otherTime.nanosecond()) - static_cast<double>(thisTime.nanosecond()));
        int32_t timeSign = timeDiff < 0 ? -1 : timeDiff > 0 ? 1 : 0;
        int32_t dateSign = isoDateCompare(thisDate, otherDate);
        ISO8601::PlainDate adjustedD2 = otherDate;
        if (dateSign && timeSign && dateSign == timeSign) {
            adjustedD2 = addDaysToISODate(adjustedD2, dateSign);
            timeDiff -= Int128(dateSign) * ISO8601::ExactTime::nsPerDay;
        }
        TemporalUnit dateLargestUnit = (largestUnit > TemporalUnit::Day) ? TemporalUnit::Day : largestUnit;
        auto dateDiffResult = TemporalCore::calendarDateUntil(calendarId, thisDate, adjustedD2, dateLargestUnit);
        if (!dateDiffResult)
            return makeUnexpected(dateDiffResult.error());
        auto& dateDiff = *dateDiffResult;
        if (largestUnit > TemporalUnit::Day)
            timeDiff += Int128(static_cast<int64_t>(dateDiff.days())) * ISO8601::ExactTime::nsPerDay;
        diff = ISO8601::InternalDuration::combineDateAndTimeDuration(
            ISO8601::Duration(dateDiff.years(), dateDiff.months(), dateDiff.weeks(), largestUnit > TemporalUnit::Day ? 0 : dateDiff.days(), 0, 0, 0, 0, Int128(0), Int128(0)), timeDiff);
    } else
        diff = diffISODateTime(thisDate, thisTime, otherDate, otherTime, largestUnit);

    // Step 4: If smallestUnit=nanosecond and increment=1, return diff.
    if (smallestUnit == TemporalUnit::Nanosecond && increment == 1)
        return diff;

    // Step 5: originEpochNs = GetUTCEpochNanoseconds(isoDateTime1).
    Int128 originEpochNs = getUTCEpochNanoseconds(thisDate, thisTime);
    // Step 6: destEpochNs = GetUTCEpochNanoseconds(isoDateTime2).
    Int128 destEpochNs = getUTCEpochNanoseconds(otherDate, otherTime);
    // Step 7: Return ? RoundRelativeDuration(diff, originEpochNs, destEpochNs, isoDateTime1, unset, ...).
    auto roundResult = roundRelativeDuration(diff, originEpochNs, destEpochNs,
        thisDate, thisTime, largestUnit, increment, smallestUnit, roundingMode, nullptr, calendarId);
    if (!roundResult)
        return makeUnexpected(roundResult.error());
    return diff;
}

// DifferenceTemporalPlainDateTime — temporal_rs: PlainDateTime::diff (src/builtins/core/plain_date_time.rs; until/since call this)
// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplaindatetime
// Steps 1-4 (ToTemporalDateTime, CalendarEquals, GetOptionsObject, GetDifferenceSettings)
// are handled by the JS layer before calling this function.
TemporalResult<ISO8601::Duration> differenceTemporalPlainDateTime(
    DifferenceOperation op,
    ISO8601::PlainDate thisDate, ISO8601::PlainTime thisTime,
    ISO8601::PlainDate otherDate, ISO8601::PlainTime otherTime,
    CalendarID calendarId,
    TemporalUnit smallestUnit, TemporalUnit largestUnit,
    RoundingMode roundingMode, double increment)
{
    ASSERT(largestUnit <= smallestUnit);
    ASSERT(increment >= 1);
    // Step 5: If CompareISODateTime = 0, return zero duration.
    if (thisDate == otherDate && thisTime == otherTime)
        return ISO8601::Duration();

    // Step 6: internalDuration = DifferencePlainDateTimeWithRounding(...).
    auto internalDuration = differencePlainDateTimeWithRounding(thisDate, thisTime, otherDate, otherTime, calendarId, largestUnit, smallestUnit, roundingMode, increment);
    if (!internalDuration)
        return makeUnexpected(internalDuration.error());

    // Step 7: result = TemporalDurationFromInternal(internalDuration, largestUnit).
    auto result = temporalDurationFromInternal(*internalDuration, largestUnit);
    if (!result)
        return makeUnexpected(result.error());
    // Step 8: If operation is since, set result to CreateNegatedTemporalDuration(result).
    if (op == DifferenceOperation::Since)
        *result = -*result;
    // Step 9: Return result.
    return result;
}

} // namespace TemporalCore
} // namespace JSC
