/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006 Apple Computer
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

#include <DateMath.h>

#include <math.h>
#include <stdint.h>
#include <wtf/OwnPtr.h>

namespace KJS {

/* Constants */

static const double secondsPerHour = 60.0 * 60.0;
static const double minutesPerDay = 24.0 * 60.0;
static const double secondsPerDay = 24.0 * 60.0 * 60.0;
static const double secondsPerYear = 24.0 * 60.0 * 60.0 * 365.0;


static const double usecPerMsec = 1000.0;
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


/*
 * Years and leap years on which Jan 1 is a Sunday, Monday, etc.
 *
 * yearStartingWith[0][i] is an example non-leap year where
 * Jan 1 appears on Sunday (i == 0), Monday (i == 1), etc.
 *
 * yearStartingWith[1][i] is an example leap year where
 * Jan 1 appears on Sunday (i == 0), Monday (i == 1), etc.
 */
static int yearStartingWith[2][7] = {
    {1978, 1973, 1974, 1975, 1981, 1971, 1977},
    {1984, 1996, 1980, 1992, 1976, 1988, 1972}
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

static inline double timeToMseconds(double hour, double min, double sec, double ms)
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
static inline int equivalentYearForDST(int year)
{
    int day;

    day = (int) daysFrom1970ToYear(year) + 4;
    day = day % 7;

    if (day < 0)
        day += 7;

    return yearStartingWith[isLeapYear(year)][day];
}

/*
 * Get the difference in milliseconds between this time zone and UTC (GMT)
 * NOT including DST.
 */
double getUTCOffset() {
    static double utcOffset;
    static bool utcOffsetInitialized = false;
    if (!utcOffsetInitialized) {
        struct tm ltime;

        ltime.tm_sec = 0;
        ltime.tm_min = 0;
        ltime.tm_hour = 0;
        ltime.tm_mon = 0;
        ltime.tm_wday = 0;
        ltime.tm_yday = 0;
        ltime.tm_isdst = 0;

        /* get the difference between this time zone and GMT */
        ltime.tm_mday = 2;
        ltime.tm_year = 70;

        #if !PLATFORM(WIN_OS)
        ltime.tm_zone = 0;
        ltime.tm_gmtoff = 0;
        #endif

        utcOffset = mktime(&ltime) - (hoursPerDay * secondsPerHour);
        utcOffset *= -msPerSecond;

        utcOffsetInitialized = true;
    }
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
    else if(localTimeSeconds < 0) /*go ahead a day to make localtime work (does not work with 0) */
        localTimeSeconds = secondsPerDay;

    double offsetTime = (localTimeSeconds * usecPerMsec) + getUTCOffset() ;

    struct tm prtm;
    prtm.tm_min   =  msToMinutes(offsetTime);
    prtm.tm_hour  =  msToHours(offsetTime);

    // FIXME: time_t has a potential problem in 2038
    time_t localTime = static_cast<time_t>(localTimeSeconds);

    struct tm tm;
    #if PLATFORM(WIN_OS)
    localtime_s(&tm, &localTime);
    #else
    localtime_r(&localTime, &tm);
    #endif

    double diff = ((tm.tm_hour - prtm.tm_hour) * secondsPerHour) + ((tm.tm_min - prtm.tm_min) * 60);

    if(diff < 0)
        diff += secondsPerDay;

    return (diff * usecPerMsec);
}

// get the DST offset the time passed in
static double getDSTOffset(double ms)
{
    /*
     * If earlier than 1970 or after 2038, potentially beyond the ken of
     * many OSes, map it to an equivalent year before asking.
     */
    if (ms < 0.0 || ms > 2145916800000.0) {
        int year;
        int day;

        year = equivalentYearForDST(msToYear(ms));
        day = dateToDayInYear(year, msToMonth(ms), msToDayInMonth(ms));
        ms = (day * msPerDay) + msToMilliseconds(ms);
    }

    /* put our ms in an LL, and map it to usec for prtime */
    return getDSTOffsetSimple(ms / usecPerMsec);
}

static inline double localTimeToUTC(double ms)
{
    ms -= getUTCOffset();
    ms -= getDSTOffset(ms);
    return ms;
}

static inline double UTCToLocalTime(double ms)
{
    ms += getUTCOffset();
    ms += getDSTOffset(ms);
    return ms;
}

double dateToMseconds(tm* t, double ms, bool inputIsUTC)
{
    int day = dateToDayInYear(t->tm_year + 1900, t->tm_mon, t->tm_mday);
    double msec_time = timeToMseconds(t->tm_hour, t->tm_min, t->tm_sec, ms);
    double result = (day * msPerDay) + msec_time;

    if(!inputIsUTC)
        result = localTimeToUTC(result);

    return result;
}

void msToTM(double ms, bool outputIsUTC, struct tm& tm)
{
    //input is UTC
    if(!outputIsUTC)
        ms = UTCToLocalTime(ms);

    tm.tm_sec   =  msToSeconds(ms);
    tm.tm_min   =  msToMinutes(ms);
    tm.tm_hour  =  msToHours(ms);
    tm.tm_wday  =  msToWeekDay(ms);
    tm.tm_mday  =  msToDayInMonth(ms);
    tm.tm_yday  =  dayInYear(ms, msToYear(ms));
    tm.tm_mon   =  msToMonth(ms);
    tm.tm_year  =  msToYear(ms) - 1900;
    tm.tm_isdst =  isDST(ms);

    //everyone else seems to have these fields
    #if !PLATFORM(WIN_OS)
    struct tm xtm;
    // FIXME: time_t has a potential problem in 2038
    time_t seconds = static_cast<time_t>(ms/usecPerMsec);
    localtime_r(&seconds, &xtm);
    tm.tm_gmtoff = xtm.tm_gmtoff;
    tm.tm_zone = xtm.tm_zone;
    #endif
}

bool isDST(const double& ms)
{
    return getDSTOffset(ms) != 0;
}

}   //namespace KJS

