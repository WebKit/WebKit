/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

    LocalTimeOffset()
        : isDST(false)
        , offset(0)
    {
    }

    LocalTimeOffset(bool isDST, int offset)
        : isDST(isDST)
        , offset(offset)
    {
    }

    bool operator==(const LocalTimeOffset& other)
    {
        return isDST == other.isDST && offset == other.offset;
    }

    bool operator!=(const LocalTimeOffset& other)
    {
        return isDST != other.isDST || offset != other.offset;
    }

    bool isDST;
    int offset;
};

void initializeDates();
int equivalentYearForDST(int year);

// Not really math related, but this is currently the only shared place to put these.
WTF_EXPORT_PRIVATE double parseES5DateFromNullTerminatedCharacters(const char* dateString, bool& isLocalTime);
WTF_EXPORT_PRIVATE double parseDateFromNullTerminatedCharacters(const char* dateString);
WTF_EXPORT_PRIVATE double parseDateFromNullTerminatedCharacters(const char* dateString, bool& isLocalTime);
// dayOfWeek: [0, 6] 0 being Monday, day: [1, 31], month: [0, 11], year: ex: 2011, hours: [0, 23], minutes: [0, 59], seconds: [0, 59], utcOffset: [-720,720]. 
String makeRFC2822DateString(unsigned dayOfWeek, unsigned day, unsigned month, unsigned year, unsigned hours, unsigned minutes, unsigned seconds, int utcOffset);

inline double jsCurrentTime()
{
    // JavaScript doesn't recognize fractions of a millisecond.
    return floor(WallTime::now().secondsSinceEpoch().milliseconds());
}

extern WTF_EXPORT_PRIVATE const char* const weekdayName[7];
extern WTF_EXPORT_PRIVATE const char* const monthName[12];
extern WTF_EXPORT_PRIVATE const char* const monthFullName[12];
extern WTF_EXPORT_PRIVATE const int firstDayOfMonth[2][12];

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

class TimeClippedPositiveMilliseconds {
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

    explicit TimeClippedPositiveMilliseconds(int64_t value)
        : m_value(value)
    {
        ASSERT(value >= 0);
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

    const double yearMinusOne = year - 1;
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

inline int64_t msToDays(TimeClippedPositiveMilliseconds ms)
{
    return ms.value() / TimeClippedPositiveMilliseconds::msPerDay;
}

inline int dayInYear(int year, int month, int day)
{
    return firstDayOfMonth[isLeapYear(year)][month] + day - 1;
}

inline int dayInYear(double ms, int year)
{
    return static_cast<int>(msToDays(ms) - daysFrom1970ToYear(year));
}

inline int dayInYear(TimeClippedPositiveMilliseconds ms, int year)
{
    return static_cast<int>(msToDays(ms) - daysFrom1970ToYearTimeClippedPositive(year));
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
    int approxYear = static_cast<int>(floor(ms / (msPerDay * 365.2425)) + 1970);
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

inline int msToMinutes(TimeClippedPositiveMilliseconds ms)
{
    int64_t result = (ms.value() / TimeClippedPositiveMilliseconds::msPerMinute) % TimeClippedPositiveMilliseconds::minutesPerHour;
    ASSERT(result >= 0);
    return static_cast<int>(result);
}

inline int msToHours(double ms)
{
    double result = fmod(floor(ms / msPerHour), hoursPerDay);
    if (result < 0)
        result += hoursPerDay;
    return static_cast<int>(result);
}

inline int msToHours(TimeClippedPositiveMilliseconds ms)
{
    int64_t result = (ms.value() / TimeClippedPositiveMilliseconds::msPerHour) % TimeClippedPositiveMilliseconds::hoursPerDay;
    ASSERT(result >= 0);
    return static_cast<int>(result);
}

inline int msToSeconds(double ms)
{
    double result = fmod(floor(ms / msPerSecond), secondsPerMinute);
    if (result < 0)
        result += secondsPerMinute;
    return static_cast<int>(result);
}

inline int msToSeconds(TimeClippedPositiveMilliseconds ms)
{
    int64_t result = ms.value() / TimeClippedPositiveMilliseconds::msPerSecond % TimeClippedPositiveMilliseconds::secondsPerMinute;
    ASSERT(result >= 0);
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

inline int msToWeekDay(TimeClippedPositiveMilliseconds ms)
{
    int result = (static_cast<int>(msToDays(ms)) + 4) % 7;
    ASSERT(result >= 0);
    return result;
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
    if (d < (step += 30))
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

// Returns combined offset in millisecond (UTC + DST).
WTF_EXPORT_PRIVATE LocalTimeOffset calculateLocalTimeOffset(double utcInMilliseconds, TimeType = UTCTime);

} // namespace WTF

using WTF::isLeapYear;
using WTF::dateToDaysFrom1970;
using WTF::dayInMonthFromDayInYear;
using WTF::dayInYear;
using WTF::minutesPerHour;
using WTF::monthFromDayInYear;
using WTF::msPerDay;
using WTF::msPerHour;
using WTF::msPerMinute;
using WTF::msPerSecond;
using WTF::msToYear;
using WTF::msToDays;
using WTF::msToMinutes;
using WTF::msToHours;
using WTF::secondsPerDay;
using WTF::secondsPerMinute;
using WTF::parseDateFromNullTerminatedCharacters;
using WTF::makeRFC2822DateString;
using WTF::LocalTimeOffset;
using WTF::calculateLocalTimeOffset;
using WTF::timeClip;
using WTF::jsCurrentTime;
