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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/PlainGregorianDateTime.h>

#include <wtf/DateMath.h>

#if OS(WINDOWS)
#include <windows.h>
#else
#include <time.h>
#endif

namespace WTF {

PlainGregorianDateTime PlainGregorianDateTime::fromMilliseconds(double millisecondsFromEpoch)
{
    // One day of slack past the ECMAScript limit, so that a time value shifted by a UTC offset
    // still decomposes. Beyond that the year no longer fits the payload.
    if (!std::isfinite(millisecondsFromEpoch) || std::abs(millisecondsFromEpoch) > maxECMAScriptTime + msPerDay)
        return { };

    Int64Milliseconds timeClipped(static_cast<int64_t>(millisecondsFromEpoch));
    int32_t days = msToDays(timeClipped);
    int32_t timeInDayMS = timeInDay(timeClipped, days);
    auto [year, month, day] = yearMonthDayFromDays(days);
    int32_t hour = timeInDayMS / (60 * 60 * 1000);
    int32_t minute = (timeInDayMS / (60 * 1000)) % 60;
    int32_t second = (timeInDayMS / 1000) % 60;
    return PlainGregorianDateTime(year, month, day, WTF::weekDay(days), hour, minute, second, 0, false);
}

PlainGregorianDateTime PlainGregorianDateTime::currentLocalTime()
{
#if OS(WINDOWS)
    SYSTEMTIME systemTime;
    GetLocalTime(&systemTime);
    TIME_ZONE_INFORMATION timeZoneInformation;
    DWORD timeZoneId = GetTimeZoneInformation(&timeZoneInformation);

    LONG bias = 0;
    if (timeZoneId != TIME_ZONE_ID_INVALID) {
        bias = timeZoneInformation.Bias;
        if (timeZoneId == TIME_ZONE_ID_DAYLIGHT)
            bias += timeZoneInformation.DaylightBias;
        else if ((timeZoneId == TIME_ZONE_ID_STANDARD) || (timeZoneId == TIME_ZONE_ID_UNKNOWN))
            bias += timeZoneInformation.StandardBias;
        else
            ASSERT(0);
    }

    return PlainGregorianDateTime(systemTime.wYear, systemTime.wMonth - 1, systemTime.wDay, systemTime.wDayOfWeek, systemTime.wHour, systemTime.wMinute,
        std::clamp<int32_t>(systemTime.wSecond, 0, 59),
        std::clamp<int32_t>(-bias, -maxUTCOffsetInMinute, maxUTCOffsetInMinute),
        timeZoneId == TIME_ZONE_ID_DAYLIGHT);
#else
    tm localTM;
    time_t localTime = time(0);
#if HAVE(LOCALTIME_R)
    if (!localtime_r(&localTime, &localTM))
        return { };
#else
    if (!localtime_s(&localTime, &localTM))
        return { };
#endif

    // A clock set far enough outside the payload's range has no representable answer, unlike a
    // second or an offset that merely needs pulling back to the nearest one.
    int32_t year = localTM.tm_year + 1900;
    if (year < minYear || year > maxYear)
        return { };

#if HAVE(TM_GMTOFF)
    int32_t utcOffsetInMinute = static_cast<int32_t>(localTM.tm_gmtoff / secondsPerMinute);
#else
    int32_t utcOffsetInMinute = static_cast<int32_t>(calculateLocalTimeOffset(localTime * msPerSecond).offset / msPerMinute);
#endif

    // struct tm is wider than the payload in three places: tm_sec may report a leap second,
    // tm_isdst may report -1 for "unknown", and tm_gmtoff goes past a day for a hand-written POSIX
    // TZ string even though no real zone reaches 15 hours.
    return PlainGregorianDateTime(year, localTM.tm_mon, localTM.tm_mday, localTM.tm_wday, localTM.tm_hour, localTM.tm_min,
        std::clamp(localTM.tm_sec, 0, 59),
        std::clamp(utcOffsetInMinute, -maxUTCOffsetInMinute, maxUTCOffsetInMinute),
        localTM.tm_isdst > 0);
#endif
}

} // namespace WTF
