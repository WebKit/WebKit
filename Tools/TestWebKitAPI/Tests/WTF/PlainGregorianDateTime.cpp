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
#include <wtf/PlainGregorianDateTime.h>

#include "Helpers/Test.h"
#include <limits>
#include <wtf/DateMath.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/CString.h>

#if !OS(WINDOWS)
#include <stdlib.h>
#include <time.h>
#endif

namespace TestWebKitAPI {

TEST(WTF_PlainGregorianDateTime, DefaultIsEmpty)
{
    PlainGregorianDateTime date;
    EXPECT_FALSE(static_cast<bool>(date));
    EXPECT_TRUE(date.hasNeverBeenComputed());
}

TEST(WTF_PlainGregorianDateTime, StaleMarkerIsEmptyButComputed)
{
    auto date = PlainGregorianDateTime::staleMarker();
    EXPECT_FALSE(static_cast<bool>(date));
    EXPECT_FALSE(date.hasNeverBeenComputed());
}

TEST(WTF_PlainGregorianDateTime, FieldRoundTrip)
{
    PlainGregorianDateTime date(2026, 8, 17, 4, 13, 45, 59, -480, true);
    EXPECT_TRUE(static_cast<bool>(date));
    EXPECT_FALSE(date.hasNeverBeenComputed());
    EXPECT_EQ(2026, date.year());
    EXPECT_EQ(8, date.month());
    EXPECT_EQ(17, date.monthDay());
    EXPECT_EQ(4, date.weekDay());
    EXPECT_EQ(13, date.hour());
    EXPECT_EQ(45, date.minute());
    EXPECT_EQ(59, date.second());
    EXPECT_EQ(-480, date.utcOffsetInMinute());
    EXPECT_TRUE(date.isDST());
}

TEST(WTF_PlainGregorianDateTime, ExtremeFieldsRoundTrip)
{
    PlainGregorianDateTime min(PlainGregorianDateTime::minYear, 0, 1, 0, 0, 0, 0, 1440, false);
    EXPECT_EQ(PlainGregorianDateTime::minYear, min.year());
    EXPECT_EQ(1440, min.utcOffsetInMinute());
    EXPECT_FALSE(min.isDST());

    PlainGregorianDateTime max(PlainGregorianDateTime::maxYear, 11, 31, 6, 23, 59, 59, -1440, true);
    EXPECT_EQ(PlainGregorianDateTime::maxYear, max.year());
    EXPECT_EQ(-1440, max.utcOffsetInMinute());
    EXPECT_TRUE(max.isDST());
}

TEST(WTF_PlainGregorianDateTime, FromMillisecondsAtEpoch)
{
    auto date = PlainGregorianDateTime::fromMilliseconds(0);
    EXPECT_TRUE(static_cast<bool>(date));
    EXPECT_EQ(1970, date.year());
    EXPECT_EQ(0, date.month());
    EXPECT_EQ(1, date.monthDay());
    EXPECT_EQ(4, date.weekDay()); // 1970-01-01 was a Thursday, and 0 is Sunday.
    EXPECT_EQ(0, date.hour());
    EXPECT_EQ(0, date.minute());
    EXPECT_EQ(0, date.second());
    EXPECT_EQ(0, date.utcOffsetInMinute());
    EXPECT_FALSE(date.isDST());
}

TEST(WTF_PlainGregorianDateTime, FromMilliseconds)
{
    // 2026-09-17T13:45:59Z, a Thursday.
    auto date = PlainGregorianDateTime::fromMilliseconds(1789652759000);
    EXPECT_EQ(2026, date.year());
    EXPECT_EQ(8, date.month());
    EXPECT_EQ(17, date.monthDay());
    EXPECT_EQ(4, date.weekDay());
    EXPECT_EQ(13, date.hour());
    EXPECT_EQ(45, date.minute());
    EXPECT_EQ(59, date.second());

    // 1969-12-31T23:59:59Z, a Wednesday: the last second before the epoch.
    auto beforeEpoch = PlainGregorianDateTime::fromMilliseconds(-1000);
    EXPECT_EQ(1969, beforeEpoch.year());
    EXPECT_EQ(11, beforeEpoch.month());
    EXPECT_EQ(31, beforeEpoch.monthDay());
    EXPECT_EQ(3, beforeEpoch.weekDay());
    EXPECT_EQ(23, beforeEpoch.hour());
    EXPECT_EQ(59, beforeEpoch.minute());
    EXPECT_EQ(59, beforeEpoch.second());
}

TEST(WTF_PlainGregorianDateTime, FromMillisecondsAtTheLimits)
{
    auto max = PlainGregorianDateTime::fromMilliseconds(WTF::maxECMAScriptTime);
    EXPECT_TRUE(static_cast<bool>(max));
    EXPECT_EQ(275760, max.year());
    EXPECT_EQ(8, max.month());
    EXPECT_EQ(13, max.monthDay());

    auto min = PlainGregorianDateTime::fromMilliseconds(-WTF::maxECMAScriptTime);
    EXPECT_TRUE(static_cast<bool>(min));
    EXPECT_EQ(-271821, min.year());
    EXPECT_EQ(3, min.month());
    EXPECT_EQ(20, min.monthDay());

    // A time value shifted by a UTC offset stays representable, one whole day past the limit.
    auto pastMax = PlainGregorianDateTime::fromMilliseconds(WTF::maxECMAScriptTime + msPerDay);
    EXPECT_EQ(275760, pastMax.year());
    EXPECT_EQ(8, pastMax.month());
    EXPECT_EQ(14, pastMax.monthDay());

    auto pastMin = PlainGregorianDateTime::fromMilliseconds(-WTF::maxECMAScriptTime - msPerDay);
    EXPECT_EQ(-271821, pastMin.year());
    EXPECT_EQ(3, pastMin.month());
    EXPECT_EQ(19, pastMin.monthDay());
}

TEST(WTF_PlainGregorianDateTime, FromMillisecondsOutOfRange)
{
    EXPECT_FALSE(static_cast<bool>(PlainGregorianDateTime::fromMilliseconds(WTF::maxECMAScriptTime + msPerDay + 1)));
    EXPECT_FALSE(static_cast<bool>(PlainGregorianDateTime::fromMilliseconds(-WTF::maxECMAScriptTime - msPerDay - 1)));
    EXPECT_FALSE(static_cast<bool>(PlainGregorianDateTime::fromMilliseconds(std::numeric_limits<double>::infinity())));
    EXPECT_FALSE(static_cast<bool>(PlainGregorianDateTime::fromMilliseconds(-std::numeric_limits<double>::infinity())));
    EXPECT_FALSE(static_cast<bool>(PlainGregorianDateTime::fromMilliseconds(std::numeric_limits<double>::quiet_NaN())));
}

TEST(WTF_PlainGregorianDateTime, CurrentLocalTime)
{
    auto date = PlainGregorianDateTime::currentLocalTime();
    EXPECT_TRUE(static_cast<bool>(date));
    EXPECT_GE(date.year(), 2026);
    EXPECT_LE(date.month(), 11);
    EXPECT_GE(date.monthDay(), 1);
    EXPECT_LE(date.weekDay(), 6);
    EXPECT_LE(date.hour(), 23);

#if !OS(WINDOWS)
    // The clock is read inside currentLocalTime(), so bracket the call and accept whichever of the
    // two samples it landed on.
    time_t before = time(0);
    auto bracketed = PlainGregorianDateTime::currentLocalTime();
    time_t after = time(0);

    bool matchedASample = false;
    for (time_t sample : { before, after }) {
        tm expected;
        localtime_r(&sample, &expected);
        bool matched = bracketed.year() == expected.tm_year + 1900
            && bracketed.month() == expected.tm_mon
            && bracketed.monthDay() == expected.tm_mday
            && bracketed.weekDay() == expected.tm_wday
            && bracketed.hour() == expected.tm_hour
            && bracketed.minute() == expected.tm_min;
#if HAVE(TM_GMTOFF)
        matched = matched && bracketed.utcOffsetInMinute() == expected.tm_gmtoff / 60;
#endif
        if (matched)
            matchedASample = true;
    }
    EXPECT_TRUE(matchedASample);
#endif
}

#if !OS(WINDOWS) && !PLATFORM(PLAYSTATION)
class ScopedTimeZone {
    WTF_MAKE_NONCOPYABLE(ScopedTimeZone);
public:
    explicit ScopedTimeZone(const char* timeZone)
    {
        if (const char* previous = getenv("TZ"))
            m_previous = ASCIICString { previous };
        setenv("TZ", timeZone, 1);
        tzset();
    }

    ~ScopedTimeZone()
    {
        if (m_previous.isNull())
            unsetenv("TZ");
        else
            setenv("TZ", m_previous.data(), 1);
        tzset();
    }

private:
    ASCIICString m_previous;
};

TEST(WTF_PlainGregorianDateTime, CurrentLocalTimeOffsetIsBounded)
{
    {
        // The widest offset any real zone uses, and well inside the payload's range.
        ScopedTimeZone kiritimati("Pacific/Kiritimati");
        EXPECT_EQ(840, PlainGregorianDateTime::currentLocalTime().utcOffsetInMinute());
    }

    // A hand-written POSIX TZ string reaches offsets struct tm can carry and the payload cannot.
    static constexpr int32_t maxOffset = PlainGregorianDateTime::maxUTCOffsetInMinute;
    std::pair<const char*, int32_t> unrepresentable[] = { { "XXX-25", maxOffset }, { "XXX-167", maxOffset }, { "XXX167", -maxOffset } };
    for (auto [timeZone, expected] : unrepresentable) {
        ScopedTimeZone scope(timeZone);
        auto date = PlainGregorianDateTime::currentLocalTime();
        EXPECT_TRUE(static_cast<bool>(date));
        EXPECT_EQ(expected, date.utcOffsetInMinute());
        EXPECT_GE(date.year(), 2026);
        EXPECT_LE(date.month(), 11);
        EXPECT_GE(date.monthDay(), 1);
        EXPECT_LE(date.hour(), 23);
    }
}
#endif

} // namespace TestWebKitAPI
