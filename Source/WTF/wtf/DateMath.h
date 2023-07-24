/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 the V8 project authors. All rights reserved.
 * Copyright (C) 2010 Research In Motion Limited. All rights reserved.
 *
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 */

#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WTF {

enum TimeType {
    UTCTime = 0,
    LocalTime
};

struct LocalTimeOffset {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    LocalTimeOffset() = default;
    constexpr LocalTimeOffset(bool isDST, int offset)
        : isDST(isDST)
        , offset(offset)
    {
    }

    bool operator==(const LocalTimeOffset& other) const
    {
        return isDST == other.isDST && offset == other.offset;
    }

    bool isDST { false };
    int offset { 0 };
};

void initializeDates();
int equivalentYearForDST(int year);

// Not really math related, but this is currently the only shared place to put these.
WTF_EXPORT_PRIVATE double parseES5DateFromNullTerminatedCharacters(const char* dateString, bool& isLocalTime);
WTF_EXPORT_PRIVATE double parseDateFromNullTerminatedCharacters(const char* dateString);
WTF_EXPORT_PRIVATE double parseDateFromNullTerminatedCharacters(const char* dateString, bool& isLocalTime);
// dayOfWeek: [0, 6] 0 being Monday, day: [1, 31], month: [0, 11], year: ex: 2011, hours: [0, 23], minutes: [0, 59], seconds: [0, 59], utcOffset: [-720,720]. 
WTF_EXPORT_PRIVATE String makeRFC2822DateString(unsigned dayOfWeek, unsigned day, unsigned month, unsigned year, unsigned hours, unsigned minutes, unsigned seconds, int utcOffset);

inline double jsCurrentTime()
{
    // JavaScript doesn't recognize fractions of a millisecond.
    return floor(WallTime::now().secondsSinceEpoch().milliseconds());
}

extern WTF_EXPORT_PRIVATE const ASCIILiteral weekdayName[7];
extern WTF_EXPORT_PRIVATE const ASCIILiteral monthName[12];
extern WTF_EXPORT_PRIVATE const ASCIILiteral monthFullName[12];
extern WTF_EXPORT_PRIVATE const int firstDayOfMonth[2][12];
extern WTF_EXPORT_PRIVATE const int8_t daysInMonths[12];

static constexpr double hoursPerDay = 24.0;
static constexpr double minutesPerHour = 60.0;
static constexpr double secondsPerMinute = 60.0;
static constexpr double msPerSecond = 1000.0;
static constexpr double msPerMonth = 2592000000.0;
static constexpr double secondsPerHour = secondsPerMinute * minutesPerHour;
static constexpr double secondsPerDay = secondsPerHour * hoursPerDay;
static constexpr double msPerMinute = msPerSecond * secondsPerMinute;
static constexpr double msPerHour = msPerSecond * secondsPerHour;
static constexpr double msPerDay = msPerSecond * secondsPerDay;

static constexpr double maxUnixTime = 2145859200.0; // 12/31/2037
// ECMAScript asks not to support for a date of which total
// millisecond value is larger than the following value.
// See 15.9.1.14 of ECMA-262 5th edition.
static constexpr double maxECMAScriptTime = 8.64E15;

class Int64Milliseconds {
public:
    static constexpr int64_t hoursPerDay = 24;
    static constexpr int64_t minutesPerHour = 60;
    static constexpr int64_t secondsPerMinute = 60;
    static constexpr int64_t msPerSecond = 1000;
    static constexpr int64_t msPerMonth = 2592000000;
    static constexpr int64_t secondsPerHour = secondsPerMinute * minutesPerHour;
    static constexpr int64_t secondsPerDay = secondsPerHour * hoursPerDay;
    static constexpr int64_t msPerMinute = msPerSecond * secondsPerMinute;
    static constexpr int64_t msPerHour = msPerSecond * secondsPerHour;
    static constexpr int64_t msPerDay = msPerSecond * secondsPerDay;
    static constexpr int64_t maxECMAScriptTime = 8.64E15;
    static constexpr int64_t minECMAScriptTime = -8.64E15;

    static constexpr int64_t daysIn4Years = 4 * 365 + 1;
    static constexpr int64_t daysIn100Years = 25 * daysIn4Years - 1;
    static constexpr int64_t daysIn400Years = 4 * daysIn100Years + 1;
    static constexpr int64_t days1970to2000 = 30 * 365 + 7;
    static constexpr int32_t daysOffset = 1000 * daysIn400Years + 5 * daysIn400Years - days1970to2000;
    static constexpr int32_t yearsOffset = 400000;

    explicit Int64Milliseconds(int64_t value)
        : m_value(value)
    {
    }

    int64_t value() const { return m_value; }
    double asDouble() const { return static_cast<double>(m_value); }
private:
    int64_t m_value;
};

inline double timeClip(double t)
{
    if (std::abs(t) > maxECMAScriptTime)
        return std::numeric_limits<double>::quiet_NaN();
    return std::trunc(t) + 0.0;
}

inline double daysFrom1970ToYear(int year)
{
    // The Gregorian Calendar rules for leap years:
    // Every fourth year is a leap year. 2004, 2008, and 2012 are leap years.
    // However, every hundredth year is not a leap year. 1900 and 2100 are not leap years.
    // Every four hundred years, there's a leap year after all. 2000 and 2400 are leap years.

    static constexpr int leapDaysBefore1971By4Rule = 1970 / 4;
    static constexpr int excludedLeapDaysBefore1971By100Rule = 1970 / 100;
    static constexpr int leapDaysBefore1971By400Rule = 1970 / 400;

    const double yearMinusOne = static_cast<double>(year) - 1;
    const double yearsToAddBy4Rule = floor(yearMinusOne / 4.0) - leapDaysBefore1971By4Rule;
    const double yearsToExcludeBy100Rule = floor(yearMinusOne / 100.0) - excludedLeapDaysBefore1971By100Rule;
    const double yearsToAddBy400Rule = floor(yearMinusOne / 400.0) - leapDaysBefore1971By400Rule;

    return 365.0 * (year - 1970.0) + yearsToAddBy4Rule - yearsToExcludeBy100Rule + yearsToAddBy400Rule;
}

inline int64_t daysFrom1970ToYearTimeClippedPositive(int year)
{
    static constexpr int leapDaysBefore1971By4Rule = 1970 / 4;
    static constexpr int excludedLeapDaysBefore1971By100Rule = 1970 / 100;
    static constexpr int leapDaysBefore1971By400Rule = 1970 / 400;

    ASSERT(year >= 1970);
    const int64_t yearMinusOne = year - 1;
    const int64_t yearsToAddBy4Rule = yearMinusOne / 4.0 - leapDaysBefore1971By4Rule;
    const int64_t yearsToExcludeBy100Rule = yearMinusOne / 100.0 - excludedLeapDaysBefore1971By100Rule;
    const int64_t yearsToAddBy400Rule = yearMinusOne / 400.0 - leapDaysBefore1971By400Rule;

    return 365 * (year - 1970) + yearsToAddBy4Rule - yearsToExcludeBy100Rule + yearsToAddBy400Rule;
}

inline bool isLeapYear(int year)
{
    if (year % 4 != 0)
        return false;
    if (year % 400 == 0)
        return true;
    if (year % 100 == 0)
        return false;
    return true;
}

inline int daysInYear(int year)
{
    return 365 + isLeapYear(year);
}

inline double msToDays(double ms)
{
    return floor(ms / msPerDay);
}

inline int32_t msToDays(Int64Milliseconds ms)
{
    int64_t time = ms.value();
    if (time < 0)
        time -= (Int64Milliseconds::msPerDay - 1);
    return static_cast<int>(time / Int64Milliseconds::msPerDay);
}

inline int32_t timeInDay(Int64Milliseconds ms, int days)
{
    return static_cast<int32_t>(ms.value() - days * Int64Milliseconds::msPerDay);
}

inline std::tuple<int32_t, int32_t, int32_t> yearMonthDayFromDays(int32_t passedDays)
{
    int32_t days = passedDays;
    days += Int64Milliseconds::daysOffset;
    int32_t year = 400 * (days / Int64Milliseconds::daysIn400Years) - Int64Milliseconds::yearsOffset;
    days %= Int64Milliseconds::daysIn400Years;

    days--;
    int32_t yd1 = days / Int64Milliseconds::daysIn100Years;
    days %= Int64Milliseconds::daysIn100Years;
    year += 100 * yd1;

    days++;
    int yd2 = days / Int64Milliseconds::daysIn4Years;
    days %= Int64Milliseconds::daysIn4Years;
    year += 4 * yd2;

    days--;
    int yd3 = days / 365;
    days %= 365;
    year += yd3;

    bool isLeap = (!yd1 || yd2) && !yd3;

    days += isLeap;

    // Check if the date is after February.
    int32_t month = 0;
    int32_t day = 0;
    if (days >= 31 + 28 + (isLeap ? 1 : 0)) {
        days -= 31 + 28 + (isLeap ? 1 : 0);
        // Find the date starting from March.
        for (int i = 2; i < 12; i++) {
            if (days < daysInMonths[i]) {
                month = i;
                day = days + 1;
                break;
            }
            days -= daysInMonths[i];
        }
    } else {
        // Check January and February.
        if (days < 31) {
            month = 0;
            day = days + 1;
        } else {
            month = 1;
            day = days - 31 + 1;
        }
    }

    return std::tuple { year, month, day };
}

inline int32_t daysFromYearMonth(int32_t year, int32_t month)
{
    year += month / 12;
    month %= 12;
    if (month < 0) {
        year--;
        month += 12;
    }

    ASSERT(month >= 0);
    ASSERT(month < 12);

    // yearDelta is an arbitrary number such that:
    // a) yearDelta = -1 (mod 400)
    // b) year + yearDelta > 0 for years in the range defined by
    //    ECMA 262 - 15.9.1.1, i.e. upto 100,000,000 days on either side of
    //    Jan 1 1970. This is required so that we don't run into integer
    //    division of negative numbers.
    // c) there shouldn't be an overflow for 32-bit integers in the following
    //    operations.
    static const int32_t yearDelta = 399999;
    static const int32_t baseDay =
        365 * (1970 + yearDelta) + (1970 + yearDelta) / 4 -
        (1970 + yearDelta) / 100 + (1970 + yearDelta) / 400;

    int32_t year1 = year + yearDelta;
    int32_t dayFromYear = 365 * year1 + year1 / 4 - year1 / 100 + year1 / 400 - baseDay;

    if ((year % 4 != 0) || (year % 100 == 0 && year % 400 != 0))
        return dayFromYear + firstDayOfMonth[0][month];
    return dayFromYear + firstDayOfMonth[1][month];
}

inline int dayInYear(int year, int month, int day)
{
    return firstDayOfMonth[isLeapYear(year)][month] + day - 1;
}

inline int dayInYear(double ms, int year)
{
    double result = msToDays(ms) - daysFrom1970ToYear(year);
    return std::isnan(result) ? 0 : static_cast<int>(result);
}

// Returns the number of days from 1970-01-01 to the specified date.
inline double dateToDaysFrom1970(int year, int month, int day)
{
    year += month / 12;

    month %= 12;
    if (month < 0) {
        month += 12;
        --year;
    }

    double yearday = floor(daysFrom1970ToYear(year));
    ASSERT((year >= 1970 && yearday >= 0) || (year < 1970 && yearday < 0));
    return yearday + dayInYear(year, month, day);
}

inline int msToYear(double ms)
{
    double msAsYears = std::floor(ms / (msPerDay * 365.2425));
    if (std::isnan(msAsYears))
        msAsYears = 0;
    int approxYear = static_cast<int>(msAsYears + 1970);
    double msFromApproxYearTo1970 = msPerDay * daysFrom1970ToYear(approxYear);
    if (msFromApproxYearTo1970 > ms)
        return approxYear - 1;
    if (msFromApproxYearTo1970 + msPerDay * daysInYear(approxYear) <= ms)
        return approxYear + 1;
    return approxYear;
}

inline int msToMinutes(double ms)
{
    double result = fmod(floor(ms / msPerMinute), minutesPerHour);
    if (result < 0)
        result += minutesPerHour;
    return static_cast<int>(result);
}

inline int msToHours(double ms)
{
    double result = fmod(floor(ms / msPerHour), hoursPerDay);
    if (result < 0)
        result += hoursPerDay;
    return static_cast<int>(result);
}

inline int msToSeconds(double ms)
{
    double result = fmod(floor(ms / msPerSecond), secondsPerMinute);
    if (result < 0)
        result += secondsPerMinute;
    return static_cast<int>(result);
}

// 0: Sunday, 1: Monday, etc.
inline int msToWeekDay(double ms)
{
    int wd = (static_cast<int>(msToDays(ms)) + 4) % 7;
    if (wd < 0)
        wd += 7;
    return wd;
}

inline int32_t weekDay(int32_t days)
{
    int32_t result = (days + 4) % 7;
    return result >= 0 ? result : result + 7;
}

inline int monthFromDayInYear(int dayInYear, bool leapYear)
{
    const int d = dayInYear;
    int step;

    if (d < (step = 31))
        return 0;
    step += (leapYear ? 29 : 28);
    if (d < step)
        return 1;
    if (d < (step += 31))
        return 2;
    if (d < (step += 30))
        return 3;
    if (d < (step += 31))
        return 4;
    if (d < (step += 30))
        return 5;
    if (d < (step += 31))
        return 6;
    if (d < (step += 31))
        return 7;
    if (d < (step += 30))
        return 8;
    if (d < (step += 31))
        return 9;
    if (d < step + 30)
        return 10;
    return 11;
}

inline int dayInMonthFromDayInYear(int dayInYear, bool leapYear)
{
    auto checkMonth = [] (int dayInYear, int& startDayOfThisMonth, int& startDayOfNextMonth, int daysInThisMonth) -> bool {
        startDayOfThisMonth = startDayOfNextMonth;
        startDayOfNextMonth += daysInThisMonth;
        return (dayInYear <= startDayOfNextMonth);
    };

    const int d = dayInYear;
    int step;
    int next = 30;

    if (d <= next)
        return d + 1;
    const int daysInFeb = (leapYear ? 29 : 28);
    if (checkMonth(d, step, next, daysInFeb))
        return d - step;
    if (checkMonth(d, step, next, 31))
        return d - step;
    if (checkMonth(d, step, next, 30))
        return d - step;
    if (checkMonth(d, step, next, 31))
        return d - step;
    if (checkMonth(d, step, next, 30))
        return d - step;
    if (checkMonth(d, step, next, 31))
        return d - step;
    if (checkMonth(d, step, next, 31))
        return d - step;
    if (checkMonth(d, step, next, 30))
        return d - step;
    if (checkMonth(d, step, next, 31))
        return d - step;
    if (checkMonth(d, step, next, 30))
        return d - step;
    step = next;
    return d - step;
}

inline double timeToMS(double hour, double min, double sec, double ms)
{
    return (((hour * WTF::minutesPerHour + min) * WTF::secondsPerMinute + sec) * WTF::msPerSecond + ms);
}

// Returns an equivalent year in the range [2008-2035] matching
// - leap year,
// - week day of first day.
// ECMA 262 - 15.9.1.9.
inline int32_t equivalentYear(int32_t year)
{
    int weekDay = WTF::weekDay(daysFromYearMonth(year, 0));
    int recentYear = (isLeapYear(year) ? 1956 : 1967) + (weekDay * 12) % 28;
    // Find the year in the range 2008..2037 that is equivalent mod 28.
    // Add 3*28 to give a positive argument to the modulus operator.
    return 2008 + (recentYear + 3 * 28 - 2008) % 28;
}

// Computes a time equivalent to the given time according
// to ECMA 262 - 15.9.1.9.
// The issue here is that some library calls don't work right for dates
// that cannot be represented using a non-negative signed 32 bit integer
// (measured in whole seconds based on the 1970 epoch).
// We solve this by mapping the time to a year with same leap-year-ness
// and same starting day for the year. The ECMAscript specification says
// we must do this, but for compatibility with other browsers, we use
// the actual year if it is in the range 1970..2037
inline int64_t equivalentTime(int64_t ms)
{
    Int64Milliseconds timeMS(ms);
    int32_t days = msToDays(timeMS);
    int32_t timeWithinDayMS = static_cast<int32_t>(timeMS.value() - days * Int64Milliseconds::msPerDay);
    auto [year, month, day] = yearMonthDayFromDays(days);
    int32_t newDays = daysFromYearMonth(equivalentYear(year), month) + day - 1;
    return static_cast<int64_t>(newDays) * Int64Milliseconds::msPerDay + timeWithinDayMS;
}

WTF_EXPORT_PRIVATE bool isTimeZoneValid(StringView);
WTF_EXPORT_PRIVATE bool setTimeZoneOverride(StringView);
WTF_EXPORT_PRIVATE void getTimeZoneOverride(Vector<UChar, 32>& timeZoneID);

// Returns combined offset in millisecond (UTC + DST).
WTF_EXPORT_PRIVATE LocalTimeOffset calculateLocalTimeOffset(double utcInMilliseconds, TimeType = UTCTime);

} // namespace WTF

using WTF::calculateLocalTimeOffset;
using WTF::dateToDaysFrom1970;
using WTF::dayInMonthFromDayInYear;
using WTF::dayInYear;
using WTF::daysFrom1970ToYear;
using WTF::daysInYear;
using WTF::getTimeZoneOverride;
using WTF::isLeapYear;
using WTF::isTimeZoneValid;
using WTF::jsCurrentTime;
using WTF::LocalTimeOffset;
using WTF::makeRFC2822DateString;
using WTF::minutesPerHour;
using WTF::monthFromDayInYear;
using WTF::msPerDay;
using WTF::msPerHour;
using WTF::msPerMinute;
using WTF::msPerSecond;
using WTF::msToDays;
using WTF::msToHours;
using WTF::msToMinutes;
using WTF::msToYear;
using WTF::parseDateFromNullTerminatedCharacters;
using WTF::secondsPerDay;
using WTF::secondsPerMinute;
using WTF::setTimeZoneOverride;
using WTF::timeClip;
using WTF::timeToMS;
