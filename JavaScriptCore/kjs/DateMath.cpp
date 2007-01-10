/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DateMath.h"

#include <math.h>
#include <stdint.h>
#include <wtf/OwnPtr.h>

namespace KJS {

/* Constants */

static const double minutesPerDay = 24.0 * 60.0;
static const double secondsPerDay = 24.0 * 60.0 * 60.0;
static const double secondsPerYear = 24.0 * 60.0 * 60.0 * 365.0;

static const double usecPerSec = 1000000.0;

static const double maxUnixTime = 2145859200.0; /*equivalent to 12/31/2037 */

/*
 * The following array contains the day of year for the first day of
 * each month, where index 0 is January, and day 0 is January 1.
 */
static int firstDayOfMonth[2][12] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
};

static inline int daysInYear(int year)
{
    if (year % 4 != 0)
        return 365;
    if (year % 400 == 0)
        return 366;
    if (year % 100 == 0)
        return 365;
    return 366;
}

static inline double daysFrom1970ToYear(int year)
{
    return 365.0 * (year - 1970)
        + floor((year - 1969) / 4.0)
        - floor((year - 1901) / 100.0)
        + floor((year - 1601) / 400.0);
}

static inline double msFrom1970ToYear(int year)
{
    return msPerDay * daysFrom1970ToYear(year);
}

static inline double msToDays(double ms)
{
    return floor(ms / msPerDay);
}

static inline int msToYear(double ms)
{
    int y = static_cast<int>(floor(ms /(msPerDay*365.2425)) + 1970);
    double t2 = msFrom1970ToYear(y);

    if (t2 > ms) {
        y--;
    } else {
        if (t2 + msPerDay * daysInYear(y) <= ms)
            y++;
    }
    return y;
}

static inline bool isLeapYear(int year)
{
    if (year % 4 != 0)
        return false;
    if (year % 400 == 0)
        return true;
    if (year % 100 == 0)
        return false;
    return true;
}

static inline bool isInLeapYear(double ms)
{
    return isLeapYear(msToYear(ms));
}

static inline int dayInYear(double ms, int year)
{
    return static_cast<int>(msToDays(ms) - daysFrom1970ToYear(year));
}

static inline double msToMilliseconds(double ms)
{
    double result;
    result = fmod(ms, msPerDay);
    if (result < 0)
        result += msPerDay;
    return result;
}

// 0: Sunday, 1: Monday, etc.
static inline int msToWeekDay(double ms)
{
    int wd = ((int)msToDays(ms) + 4) % 7;
    if (wd < 0)
        wd += 7;
    return wd;
}

static inline int msToSeconds(double ms)
{
    int result = (int) fmod(floor(ms / msPerSecond), secondsPerMinute);
    if (result < 0)
        result += (int)secondsPerMinute;
    return result;
}

static inline int msToMinutes(double ms)
{
    int result = (int) fmod(floor(ms / msPerMinute), minutesPerHour);
    if (result < 0)
        result += (int)minutesPerHour;
    return result;
}

static inline int msToHours(double ms)
{
    int result = (int) fmod(floor(ms/msPerHour), hoursPerDay);
    if (result < 0)
        result += (int)hoursPerDay;
    return result;
}

static inline int msToMonth(double ms)
{
    int d, step;
    int year = msToYear(ms);
    d = dayInYear(ms, year);

    if (d < (step = 31))
        return 0;
    step += (isInLeapYear(ms) ? 29 : 28);
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

static inline int msToDayInMonth(double ms)
{
    int d, step, next;
    int year = msToYear(ms);
    d = dayInYear(ms, year);

    if (d <= (next = 30))
        return d + 1;
    step = next;
    next += (isInLeapYear(ms) ? 29 : 28);
    if (d <= next)
        return d - step;
    step = next;
    if (d <= (next += 31))
        return d - step;
    step = next;
    if (d <= (next += 30))
        return d - step;
    step = next;
    if (d <= (next += 31))
        return d - step;
    step = next;
    if (d <= (next += 30))
        return d - step;
    step = next;
    if (d <= (next += 31))
        return d - step;
    step = next;
    if (d <= (next += 31))
        return d - step;
    step = next;
    if (d <= (next += 30))
        return d - step;
    step = next;
    if (d <= (next += 31))
        return d - step;
    step = next;
    if (d <= (next += 30))
        return d - step;
    step = next;
    return d - step;
}

static inline int monthToDayInYear(int month, bool isLeapYear)
{
    return firstDayOfMonth[isLeapYear][month];
}

static inline double timeToMS(double hour, double min, double sec, double ms)
{
    return (((hour * minutesPerHour + min) * secondsPerMinute + sec) * msPerSecond + ms);
}

static int dateToDayInYear(int year, int month, int day)
{
    year += month / 12;

    month %= 12;
    if (month < 0) {
        month += 12;
        --year;
    }

    int yearday = static_cast<int>(floor(msFrom1970ToYear(year) / msPerDay));
    int monthday = monthToDayInYear(month, isLeapYear(year));

    return yearday + monthday + day - 1;
}

/*
 * Find a year for which any given date will fall on the same weekday.
 *
 * This function should be used with caution when used other than
 * for determining DST; it hasn't been proven not to produce an
 * incorrect year for times near year boundaries.
 */
int equivalentYearForDST(int year)
{
    int difference = 2000 - year;   // Arbitrary year around which most dates equivalence is correct
    int quotient = difference / 28; // Integer division, no remainder.
    int product = quotient * 28;
    return year + product;
}

/*
 * Get the difference in milliseconds between this time zone and UTC (GMT)
 * NOT including DST.
 */
double getUTCOffset() {
    tm localt;

    memset(&localt, 0, sizeof(localt));

    // get the difference between this time zone and UTC on Jan 01, 2000 12:00:00 AM
    localt.tm_mday = 1;
    localt.tm_year = 100;
    double utcOffset = 946684800.0 - mktime(&localt);

    utcOffset *= msPerSecond;

    return utcOffset;
}

/*
 * Get the DST offset for the time passed in.  Takes
 * seconds (not milliseconds) and cannot handle dates before 1970
 * on some OS'
 */
static double getDSTOffsetSimple(double localTimeSeconds)
{
    if(localTimeSeconds > maxUnixTime)
        localTimeSeconds = maxUnixTime;
    else if(localTimeSeconds < 0) // Go ahead a day to make localtime work (does not work with 0)
        localTimeSeconds += secondsPerDay;

    //input is UTC so we have to shift back to local time to determine DST thus the + getUTCOffset()
    double offsetTime = (localTimeSeconds * msPerSecond) + getUTCOffset() ;

    // Offset from UTC but doesn't include DST obviously
    int offsetHour =  msToHours(offsetTime);
    int offsetMinute =  msToMinutes(offsetTime);

    // FIXME: time_t has a potential problem in 2038
    time_t localTime = static_cast<time_t>(localTimeSeconds);

    tm localTM;
    #if PLATFORM(WIN_OS)
    localtime_s(&localTM, &localTime);
    #else
    localtime_r(&localTime, &localTM);
    #endif
    
    double diff = ((localTM.tm_hour - offsetHour) * secondsPerHour) + ((localTM.tm_min - offsetMinute) * 60);

    if(diff < 0)
        diff += secondsPerDay;

    return (diff * msPerSecond);
}

// Get the DST offset the time passed in
// ms is in UTC
static double getDSTOffset(double ms)
{
    // On mac the call to localtime (see getDSTOffsetSimple) will return historically accurate
    // DST information (e.g. New Zealand did not have DST from 1946 to 1974) however the JavaScript
    // standard explicitly dictates that historical information should not be considered when
    // determining DST.  For this reason we shift years that localtime can handle but would
    // return historically accurate information.

    // if before Jan 01, 2000 12:00:00 AM UTC or after Jan 01, 2038 12:00:00 AM UTC
    if (ms < 946684800000.0 || ms > 2145916800000.0) {
        int year;
        int day;

        year = equivalentYearForDST(msToYear(ms));
        day = dateToDayInYear(year, msToMonth(ms), msToDayInMonth(ms));
        ms = (day * msPerDay) + msToMilliseconds(ms);
    }

    return getDSTOffsetSimple(ms / msPerSecond);
}

double gregorianDateTimeToMS(const GregorianDateTime& t, double milliSeconds, bool inputIsUTC)
{

    int day = dateToDayInYear(t.year + 1900, t.month, t.monthDay);
    double ms = timeToMS(t.hour, t.minute, t.second, milliSeconds);
    double result = (day * msPerDay) + ms;

    if(!inputIsUTC) { // convert to UTC
        result -= getUTCOffset();       
        result -= getDSTOffset(result);
    }

    return result;
}

void msToGregorianDateTime(double ms, bool outputIsUTC, struct GregorianDateTime& tm)
{
    // input is UTC
    double dstOff = 0.0;
    
    if(!outputIsUTC) {  // convert to local time
        dstOff = getDSTOffset(ms);
        ms += dstOff + getUTCOffset();
    }

    tm.second   =  msToSeconds(ms);
    tm.minute   =  msToMinutes(ms);
    tm.hour     =  msToHours(ms);
    tm.weekDay  =  msToWeekDay(ms);
    tm.monthDay =  msToDayInMonth(ms);
    tm.yearDay  =  dayInYear(ms, msToYear(ms));
    tm.month    =  msToMonth(ms);
    tm.year     =  msToYear(ms) - 1900;
    tm.isDST =  dstOff != 0.0;

    tm.utcOffset = static_cast<long>((dstOff + getUTCOffset()) / msPerSecond);
    tm.timeZone = NULL;
}

}   // namespace KJS
