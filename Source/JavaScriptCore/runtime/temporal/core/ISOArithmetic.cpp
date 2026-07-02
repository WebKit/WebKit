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
#include "ISOArithmetic.h"

#include "DateConstructor.h"
#include "Rounding.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/DateMath.h>

namespace JSC {
namespace TemporalCore {

// BalanceISOYearMonth — temporal_rs: balance_iso_year_month_with_clamp — uses i64 + div_euclid/rem_euclid
// https://tc39.es/proposal-temporal/#sec-temporal-balanceisoyearmonth
ISO8601::PlainYearMonth balanceISOYearMonth(int64_t year, int64_t month)
{
    // 1. Set year to year + floor((month - 1) / 12).
    // NOTE: C++ integer division truncates toward zero; apply floor correction for negative month-1.
    int64_t m0 = month - 1;
    int64_t yearAdd = m0 / ISO8601::monthsPerYear;
    int64_t newM0 = m0 % ISO8601::monthsPerYear;
    if (newM0 < 0) {
        yearAdd--;
        newM0 += ISO8601::monthsPerYear; // adjust for C++ truncation toward zero
    }
    year += yearAdd;
    // 2. Set month to ((month - 1) modulo 12) + 1.
    month = newM0 + 1;
    // 3. Return ISO Year-Month Record { [[Year]]: year, [[Month]]: month }.
    // NOTE: clamp year to representable range (outOfRangeYear sentinel); callers check.
    if (year < ISO8601::minYear || year > ISO8601::maxYear) [[unlikely]]
        year = ISO8601::outOfRangeYear;
    return ISO8601::PlainYearMonth(static_cast<int32_t>(year), static_cast<unsigned>(month));
}

// AddDaysToISODate — temporal_rs: IsoDate::balance (with implicit day fold-in)
// https://tc39.es/proposal-temporal/#sec-temporal-adddaystoisodate
// NOTE: Returns { outOfRangeYear, 1, 1 } for out-of-bounds dates instead of throwing (callers check).
ISO8601::PlainDate addDaysToISODate(const ISO8601::PlainDate& date, int64_t days)
{
    int32_t year = date.year();
    int32_t month = static_cast<int32_t>(date.month());
    if (year == ISO8601::outOfRangeYear) [[unlikely]]
        return ISO8601::PlainDate { ISO8601::outOfRangeYear, 1, 1 };

    // PlainDate's month invariant (set by regulateISODate / balanceISOYearMonth at every
    // construction site reachable from public Temporal entry points) is [1, 12]. We rely
    // on it for the daysInMonth bound below and for the makeDay(month-1) slow-path math.
    ASSERT(month >= 1 && month <= 12);

    int64_t newDay = static_cast<int64_t>(date.day()) + days;

    // The new day-of-month already lands in [1, daysInMonth(year, month)] - no overflow into
    // adjacent months/years, so skip the days-from-1970 + yearMonthDayFromDays roundtrip.
    if (newDay >= 1 && (newDay <= 31 && newDay <= ISO8601::daysInMonth(year, static_cast<uint8_t>(month)))) [[likely]]
        return ISO8601::PlainDate { year, static_cast<unsigned>(month), static_cast<unsigned>(newDay) };

    // 1. Let epochDays be ISODateToEpochDays(year, month - 1, day).
    // (WTF makeDay takes 0-based month, matching ISODateToEpochDays(year, month-1, day))
    auto epochDays = makeDay(static_cast<double>(year), static_cast<double>(month - 1), static_cast<double>(newDay));
    // 2. Let ms be EpochDaysToEpochMs(epochDays, 0).
    double ms = makeDate(epochDays, 0);
    double daysToUse = msToDays(ms);
    if (!isInBounds<int32_t>(daysToUse)) [[unlikely]]
        return ISO8601::PlainDate { ISO8601::outOfRangeYear, 1, 1 };
    // 3. Return CreateISODateRecord(EpochTimeToEpochYear(ms), EpochTimeToMonthInYear(ms) + 1, EpochTimeToDate(ms)).
    auto [y, m, d] = WTF::yearMonthDayFromDays(static_cast<int32_t>(daysToUse));
    if (!ISO8601::isYearWithinLimits(y)) [[unlikely]]
        return ISO8601::PlainDate { ISO8601::outOfRangeYear, 1, 1 };
    return ISO8601::PlainDate { y, static_cast<unsigned>(m + 1), static_cast<unsigned>(d) };
}

// RegulateISODate — temporal_rs: IsoDate::regulate (overflow constrain/reject)
// https://tc39.es/proposal-temporal/#sec-temporal-regulateisodate
TemporalResult<ISO8601::PlainDate> regulateISODate(int32_t year, int32_t month, int64_t day, TemporalOverflow overflow)
{
    // 1. If overflow is ~constrain~, then
    if (overflow == TemporalOverflow::Constrain) {
        // a. Set month to the result of clamping month between 1 and 12.
        if (month < 1)
            month = 1;
        else if (month > 12)
            month = 12;
        // b. Let daysInMonth be ISODaysInMonth(year, month).
        auto maxDay = static_cast<int64_t>(ISO8601::daysInMonth(year, static_cast<uint8_t>(month)));
        // c. Set day to the result of clamping day between 1 and daysInMonth.
        if (day < 1)
            day = 1;
        else if (day > maxDay)
            day = maxDay;
    } else {
        // 2.a. Assert: overflow is ~reject~.
        // 2.b. If IsValidISODate(year, month, day) is false, throw a RangeError exception.
        if (month < 1 || month > 12
            || day < 1 || day > ISO8601::daysInMonth(year, static_cast<uint8_t>(month)))
            return makeUnexpected(rangeError("date time is out of range of ECMAScript representation"_s));
    }
    // 3. Return CreateISODateRecord(year, month, day).
    return ISO8601::PlainDate(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
}

// AddISODate (ISO8601 path of CalendarDateAdd) — temporal_rs: IsoDate::add_date_duration — uses i64 arithmetic throughout
// https://tc39.es/proposal-temporal/#sec-temporal-calendardateadd
TemporalResult<ISO8601::PlainDate> isoDateAdd(const ISO8601::PlainDate& plainDate, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    // 1.a. Let intermediate be BalanceISOYearMonth(isoDate.[[Year]] + duration.[[Years]], isoDate.[[Month]] + duration.[[Months]]).
    int64_t years = static_cast<int64_t>(plainDate.year()) + duration.years();
    int64_t months = static_cast<int64_t>(plainDate.month()) + duration.months();
    ISO8601::PlainYearMonth intermediate = balanceISOYearMonth(years, months);
    // 1.b. Set intermediate to ? RegulateISODate(intermediate.[[Year]], intermediate.[[Month]], isoDate.[[Day]], overflow).
    auto intermediate1 = regulateISODate(intermediate.year(), static_cast<int32_t>(intermediate.month()), static_cast<int64_t>(plainDate.day()), overflow);
    if (!intermediate1)
        return makeUnexpected(rangeError("date time is out of range of ECMAScript representation"_s));
    // 1.c. Let days be duration.[[Days]] + 7 × duration.[[Weeks]].
    // 1.d. Let result be AddDaysToISODate(intermediate, days).
    int64_t days = duration.days() + static_cast<int64_t>(ISO8601::daysPerWeek) * duration.weeks();
    auto result = addDaysToISODate(*intermediate1, days);
    // 3. If ISODateWithinLimits(result) is false, throw a RangeError exception.
    if (!ISO8601::isDateTimeWithinLimits(result.year(), result.month(), result.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]]
        return makeUnexpected(rangeError("date time is out of range of ECMAScript representation"_s));
    // 4. Return result.
    return result;
}

// CompareISODate — temporal_rs: IsoDate::cmp
// https://tc39.es/proposal-temporal/#sec-temporal-compareisodate
int32_t NODELETE isoDateCompare(const ISO8601::PlainDate& d1, const ISO8601::PlainDate& d2)
{
    if (d1.year() > d2.year())
        return 1;
    if (d1.year() < d2.year())
        return -1;
    if (d1.month() > d2.month())
        return 1;
    if (d1.month() < d2.month())
        return -1;
    if (d1.day() > d2.day())
        return 1;
    if (d1.day() < d2.day())
        return -1;
    return 0;
}

// CompareTimeRecord — temporal_rs: IsoTime::cmp (derived Ord, src/iso.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-comparetimerecord
int32_t NODELETE isoTimeCompare(const ISO8601::PlainTime& t1, const ISO8601::PlainTime& t2)
{
    if (t1.hour() != t2.hour())
        return t1.hour() > t2.hour() ? 1 : -1;
    if (t1.minute() != t2.minute())
        return t1.minute() > t2.minute() ? 1 : -1;
    if (t1.second() != t2.second())
        return t1.second() > t2.second() ? 1 : -1;
    if (t1.millisecond() != t2.millisecond())
        return t1.millisecond() > t2.millisecond() ? 1 : -1;
    if (t1.microsecond() != t2.microsecond())
        return t1.microsecond() > t2.microsecond() ? 1 : -1;
    if (t1.nanosecond() != t2.nanosecond())
        return t1.nanosecond() > t2.nanosecond() ? 1 : -1;
    return 0;
}

// ISODateSurpasses — temporal_rs: iso_date_surpasses (src/iso.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-isodatesurpasses
static bool isoDateSurpasses(int32_t sign, int32_t y1, int32_t m1, int32_t d1, const ISO8601::PlainDate& isoDate2)
{
    if (y1 != isoDate2.year())
        return sign * (y1 - isoDate2.year()) > 0;
    if (m1 != static_cast<int32_t>(isoDate2.month()))
        return sign * (m1 - static_cast<int32_t>(isoDate2.month())) > 0;
    if (d1 != static_cast<int32_t>(isoDate2.day()))
        return sign * (d1 - static_cast<int32_t>(isoDate2.day())) > 0;
    return false;
}

// DiffISODate — temporal_rs: IsoDate::diff_iso_date
// https://tc39.es/proposal-temporal/#sec-temporal-calendardateuntil (ISO8601 path)
ISO8601::Duration diffISODate(const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit)
{
    // 1. Let sign be CompareISODate(one, two).
    // 3.a. Set sign to -sign.  (combined: negate upfront, zero check is equivalent since -0 = 0)
    auto sign = -1 * isoDateCompare(one, two);
    // 2. If sign = 0, return ZeroDateDuration().
    if (!sign)
        return { };

    // 3.b. Let years be 0.
    int years = 0;
    // 3.d. Let months be 0.
    int months = 0;

    // 3.c. If largestUnit is ~year~, then
    // 3.e. If largestUnit is either ~year~ or ~month~, then
    // NOTE: polyfill optimization runs year loop for both ~year~ and ~month~, then folds into months below.
    if (largestUnit == TemporalUnit::Year || largestUnit == TemporalUnit::Month) {
        // NOTE: candidateYears starts at two.year()-one.year() instead of sign (polyfill optimization, saves iterations).
        // 3.c.i. Let candidateYears be sign.
        auto candidateYears = two.year() - one.year();
        if (candidateYears)
            candidateYears -= sign;
        // 3.c.ii. Repeat, while ISODateSurpasses(sign, one, two, candidateYears, 0, 0, 0) is false,
        while (!isoDateSurpasses(sign, one.year() + candidateYears, static_cast<int32_t>(one.month()), static_cast<int32_t>(one.day()), two)) {
            // 1. Set years to candidateYears.
            years = candidateYears;
            // 2. Set candidateYears to candidateYears + sign.
            candidateYears += sign;
        }

        // 3.e.i. Let candidateMonths be sign.
        auto candidateMonths = sign;
        // 3.e.ii. Repeat, while ISODateSurpasses(sign, one, two, years, candidateMonths, 0, 0) is false,
        auto intermediate = balanceISOYearMonth(one.year() + years, static_cast<int64_t>(one.month()) + candidateMonths);
        while (!isoDateSurpasses(sign, intermediate.year(), static_cast<int32_t>(intermediate.month()), static_cast<int32_t>(one.day()), two)) {
            // 1. Set months to candidateMonths.
            months = candidateMonths;
            // 2. Set candidateMonths to candidateMonths + sign.
            candidateMonths += sign;
            // 3. Set intermediate to BalanceISOYearMonth(intermediate.[[Year]], intermediate.[[Month]] + sign).
            intermediate = balanceISOYearMonth(intermediate.year(), static_cast<int64_t>(intermediate.month()) + sign);
        }

        // Fold years into months when largestUnit is ~month~ (polyfill optimization result).
        if (largestUnit == TemporalUnit::Month) {
            months += years * ISO8601::monthsPerYear;
            years = 0;
        }
    }

    // NOTE: steps 3.f–3.j (weeks/days via ISODateSurpasses loops) are replaced by epoch-day
    // subtraction (polyfill optimization). Compute the base date after adding years+months first.
    auto intermediate = balanceISOYearMonth(one.year() + years, static_cast<int64_t>(one.month()) + months);
    auto constrained = regulateISODate(intermediate.year(), static_cast<int32_t>(intermediate.month()), static_cast<int64_t>(one.day()), TemporalOverflow::Constrain);
    ASSERT(constrained); // Constrain mode cannot fail

    // 3.f. Let weeks be 0.
    double weeks = 0;
    double days = makeDay(two.year(), two.month() - 1, two.day()) - makeDay(constrained->year(), constrained->month() - 1, constrained->day());

    // 3.g. If largestUnit is ~week~, then
    if (largestUnit == TemporalUnit::Week) {
        weeks = std::trunc(std::abs(days) / ISO8601::daysPerWeek);
        days = std::trunc((double)(((Int128)std::trunc(days)) % ISO8601::daysPerWeek));
        if (weeks)
            weeks *= sign; // Avoid -0
    }

    // 3.k. Return ! CreateDateDurationRecord(years, months, weeks, days).
    return ISO8601::Duration { years, months, static_cast<int64_t>(weeks), static_cast<int64_t>(days), 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0) };
}

// DiffISODateTime — ISO8601 path of DifferenceISODateTime (combines diffISODate + DifferenceTime).
// temporal_rs: IsoDateTime::diff (src/iso.rs) — no single fn; uses IsoDate::diff_iso_date + IsoTime::diff
// https://tc39.es/proposal-temporal/#sec-temporal-differenceisodatetime
ISO8601::InternalDuration diffISODateTime(const ISO8601::PlainDate& d1, const ISO8601::PlainTime& t1, const ISO8601::PlainDate& d2, const ISO8601::PlainTime& t2, TemporalUnit largestUnit)
{
    // 1. Assert: ISODateTimeWithinLimits(d1, t1) is true.
    ASSERT(ISO8601::isDateTimeWithinLimits(d1.year(), d1.month(), d1.day(), t1.hour(), t1.minute(), t1.second(), t1.millisecond(), t1.microsecond(), t1.nanosecond()));
    // 2. Assert: ISODateTimeWithinLimits(d2, t2) is true.
    ASSERT(ISO8601::isDateTimeWithinLimits(d2.year(), d2.month(), d2.day(), t2.hour(), t2.minute(), t2.second(), t2.millisecond(), t2.microsecond(), t2.nanosecond()));
    // 3. Let timeDuration be DifferenceTime(t1, t2).
    CheckedInt128 h = CheckedInt128(static_cast<Int128>(t2.hour()) - static_cast<Int128>(t1.hour()));
    CheckedInt128 m = CheckedInt128(static_cast<Int128>(t2.minute()) - static_cast<Int128>(t1.minute())) + h * Int128(60);
    CheckedInt128 s = CheckedInt128(static_cast<Int128>(t2.second()) - static_cast<Int128>(t1.second())) + m * Int128(60);
    CheckedInt128 ms = CheckedInt128(static_cast<Int128>(t2.millisecond()) - static_cast<Int128>(t1.millisecond())) + s * Int128(1000);
    CheckedInt128 us = CheckedInt128(static_cast<Int128>(t2.microsecond()) - static_cast<Int128>(t1.microsecond())) + ms * Int128(1000);
    CheckedInt128 ns = CheckedInt128(static_cast<Int128>(t2.nanosecond()) - static_cast<Int128>(t1.nanosecond())) + us * Int128(1000);
    ASSERT(!ns.hasOverflowed());
    Int128 timeDiff = ns;

    // 4. Let timeSign be TimeDurationSign(timeDuration).
    int32_t timeSign = timeDiff < 0 ? -1 : timeDiff > 0 ? 1 : 0;
    // 5. Let dateSign be CompareISODate(d1, d2).
    int32_t dateSign = isoDateCompare(d1, d2);
    // 6. Let adjustedDate be d2.
    ISO8601::PlainDate adjustedD2 = d2;

    // 7. If timeSign = dateSign, then
    if (dateSign && timeSign && dateSign == timeSign) {
        // a. Set adjustedDate to AddDaysToISODate(adjustedDate, timeSign).
        adjustedD2 = addDaysToISODate(adjustedD2, timeSign);
        // b. Set timeDuration to ! Add24HourDaysToTimeDuration(timeDuration, -timeSign).
        timeDiff -= Int128(timeSign) * ISO8601::ExactTime::nsPerDay;
    }

    // 8. Let dateLargestUnit be LargerOfTwoTemporalUnits(~day~, largestUnit).
    TemporalUnit dateLargestUnit = (largestUnit > TemporalUnit::Day) ? TemporalUnit::Day : largestUnit;

    // 9. Let dateDifference be CalendarDateUntil(calendar, d1, adjustedDate, dateLargestUnit).
    ISO8601::Duration dateDiff = diffISODate(d1, adjustedD2, dateLargestUnit);

    // 10. If largestUnit is not dateLargestUnit, then
    double remainingDays = static_cast<double>(dateDiff.days());
    Int128 timeDuration = timeDiff;
    if (largestUnit != dateLargestUnit) {
        // a. Set timeDuration to ! Add24HourDaysToTimeDuration(timeDuration, dateDifference.[[Days]]).
        timeDuration = timeDiff + Int128(dateDiff.days()) * ISO8601::ExactTime::nsPerDay;
        // b. Set dateDifference.[[Days]] to 0.
        remainingDays = 0;
    }

    // 11. Return CombineDateAndTimeDuration(dateDifference, timeDuration).
    ISO8601::Duration datePart(
        static_cast<double>(dateDiff.years()),
        static_cast<double>(dateDiff.months()),
        static_cast<double>(dateDiff.weeks()),
        remainingDays, 0, 0, 0, 0, 0, 0);
    return ISO8601::InternalDuration::combineDateAndTimeDuration(datePart, timeDuration);
}

// RoundTime steps 1–6 (quantity only) — temporal_rs: IsoTime::round (src/iso.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-roundtime
static std::pair<Int128, Int128> roundTime(const ISO8601::PlainTime& t, TemporalUnit unit)
{
    using ET = ISO8601::ExactTime;
    const Int128 lH = Int128(t.hour());
    const Int128 lMi = Int128(t.minute());
    const Int128 lS = Int128(t.second());
    const Int128 lMs = Int128(t.millisecond());
    const Int128 lUs = Int128(t.microsecond());
    const Int128 lNs = Int128(t.nanosecond());
    switch (unit) {
    // Step 1: If unit is ~day~ or ~hour~, quantity = full time from midnight in nanoseconds.
    case TemporalUnit::Day:
    case TemporalUnit::Hour:
        return { lH * ET::nsPerHour + lMi * ET::nsPerMinute + lS * ET::nsPerSecond + lMs * ET::nsPerMillisecond + lUs * ET::nsPerMicrosecond + lNs, 0 };
    // Step 2: If unit is ~minute~, quantity = time within the current hour.
    case TemporalUnit::Minute:
        return { lMi * ET::nsPerMinute + lS * ET::nsPerSecond + lMs * ET::nsPerMillisecond + lUs * ET::nsPerMicrosecond + lNs, lH * ET::nsPerHour };
    // Step 3: If unit is ~second~, quantity = time within the current minute.
    case TemporalUnit::Second:
        return { lS * ET::nsPerSecond + lMs * ET::nsPerMillisecond + lUs * ET::nsPerMicrosecond + lNs, lH * ET::nsPerHour + lMi * ET::nsPerMinute };
    // Step 4: If unit is ~millisecond~, quantity = time within the current second.
    case TemporalUnit::Millisecond:
        return { lMs * ET::nsPerMillisecond + lUs * ET::nsPerMicrosecond + lNs, lH * ET::nsPerHour + lMi * ET::nsPerMinute + lS * ET::nsPerSecond };
    // Step 5: If unit is ~microsecond~, quantity = time within the current millisecond.
    case TemporalUnit::Microsecond:
        return { lUs * ET::nsPerMicrosecond + lNs, lH * ET::nsPerHour + lMi * ET::nsPerMinute + lS * ET::nsPerSecond + lMs * ET::nsPerMillisecond };
    // Step 6: Assert unit is ~nanosecond~. quantity = nanosecond value.
    default:
        return { lNs, lH * ET::nsPerHour + lMi * ET::nsPerMinute + lS * ET::nsPerSecond + lMs * ET::nsPerMillisecond + lUs * ET::nsPerMicrosecond };
    }
}

// RoundISODateTime — temporal_rs: IsoDateTime::round (src/iso.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-roundisodatetime
RoundedISODateTime roundISODateTime(ISO8601::PlainDate date, ISO8601::PlainTime time, Int128 incrementNs, TemporalUnit unit, RoundingMode mode)
{
    using ET = ISO8601::ExactTime;

    // Step 1: Assert ISODateTimeWithinLimits(isoDateTime).
    ASSERT(ISO8601::isDateTimeWithinLimits(date.year(), date.month(), date.day(), time.hour(), time.minute(), time.second(), time.millisecond(), time.microsecond(), time.nanosecond()));

    // Step 2: roundedTime = RoundTime(time, increment, unit, roundingMode).
    auto [quantity, baseOffset] = roundTime(time, unit);
    Int128 roundedLocalNs = baseOffset + roundNumberToIncrementInt128(quantity, incrementNs, mode);

    // Step 3: balanceResult = AddDaysToISODate(isoDate, roundedTime.[[Days]]).
    if (roundedLocalNs < 0) {
        roundedLocalNs += ET::nsPerDay;
        int32_t days = WTF::daysFromYearMonth(date.year(), date.month() - 1) + (date.day() - 1) - 1;
        auto [y, m, d] = WTF::yearMonthDayFromDays(days);
        date = ISO8601::PlainDate(y, static_cast<uint8_t>(m + 1), static_cast<uint8_t>(d));
    } else if (roundedLocalNs >= ET::nsPerDay) {
        roundedLocalNs -= ET::nsPerDay;
        int32_t days = WTF::daysFromYearMonth(date.year(), date.month() - 1) + (date.day() - 1) + 1;
        auto [y, m, d] = WTF::yearMonthDayFromDays(days);
        date = ISO8601::PlainDate(y, static_cast<uint8_t>(m + 1), static_cast<uint8_t>(d));
    }

    // Step 4: CombineISODateAndTimeRecord(balanceResult, roundedTime).
    Int128 rem = roundedLocalNs;
    unsigned h = static_cast<unsigned>(rem / ET::nsPerHour);
    rem %= ET::nsPerHour;
    unsigned mi = static_cast<unsigned>(rem / ET::nsPerMinute);
    rem %= ET::nsPerMinute;
    unsigned s = static_cast<unsigned>(rem / ET::nsPerSecond);
    rem %= ET::nsPerSecond;
    unsigned ms = static_cast<unsigned>(rem / ET::nsPerMillisecond);
    rem %= ET::nsPerMillisecond;
    unsigned us = static_cast<unsigned>(rem / ET::nsPerMicrosecond);
    unsigned ns = static_cast<unsigned>(rem % ET::nsPerMicrosecond);
    return { date, ISO8601::PlainTime(h, mi, s, ms, us, ns) };
}

} // namespace TemporalCore
} // namespace JSC
