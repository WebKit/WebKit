/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <wtf/ClockType.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>
#include <wtf/StringPrintStream.h>
#include <wtf/TimeWithDynamicClockType.h>
#include <wtf/WallTime.h>

namespace WTF {

std::basic_ostream<char>& operator<<(std::basic_ostream<char>& out, Seconds value)
{
    out << toCString(value).data();
    return out;
}

std::basic_ostream<char>& operator<<(std::basic_ostream<char>& out, WallTime value)
{
    out << toCString(value).data();
    return out;
}

std::basic_ostream<char>& operator<<(std::basic_ostream<char>& out, MonotonicTime value)
{
    out << toCString(value).data();
    return out;
}

std::basic_ostream<char>& operator<<(std::basic_ostream<char>& out, TimeWithDynamicClockType value)
{
    out << toCString(value).data();
    return out;
}

} // namespace WTF

namespace TestWebKitAPI {

namespace {

Seconds s(double value)
{
    return Seconds(value);
}

WallTime wt(double value)
{
    return WallTime::fromRawSeconds(value);
}

MonotonicTime mt(double value)
{
    return MonotonicTime::fromRawSeconds(value);
}

TimeWithDynamicClockType dt(double value, ClockType type)
{
    return TimeWithDynamicClockType::fromRawSeconds(value, type);
}

TimeWithDynamicClockType dtw(double value)
{
    return dt(value, ClockType::Wall);
}

TimeWithDynamicClockType dtm(double value)
{
    return dt(value, ClockType::Monotonic);
}

} // anonymous namespace

TEST(WTF_Time, units)
{
    EXPECT_EQ(s(60), Seconds::fromMinutes(1));
    EXPECT_EQ(s(0.001), Seconds::fromMilliseconds(1));
    EXPECT_EQ(s(0.000001), Seconds::fromMicroseconds(1));
    EXPECT_EQ(s(0.0000005), Seconds::fromNanoseconds(500));

    EXPECT_EQ(s(120).minutes(), 2);
    EXPECT_EQ(s(2).seconds(), 2);
    EXPECT_EQ(s(2).milliseconds(), 2000);
    EXPECT_EQ(s(2).microseconds(), 2000000);
    EXPECT_EQ(s(2).nanoseconds(), 2000000000);
}

TEST(WTF_Time, plus)
{
    EXPECT_EQ(s(6), s(1) + s(5));
    EXPECT_EQ(s(6), s(5) + s(1));
    EXPECT_EQ(wt(6), s(1) + wt(5));
    EXPECT_EQ(wt(6), s(5) + wt(1));
    EXPECT_EQ(wt(6), wt(1) + s(5));
    EXPECT_EQ(wt(6), wt(5) + s(1));
    EXPECT_EQ(mt(6), s(1) + mt(5));
    EXPECT_EQ(mt(6), s(5) + mt(1));
    EXPECT_EQ(mt(6), mt(1) + s(5));
    EXPECT_EQ(mt(6), mt(5) + s(1));
    EXPECT_EQ(dtw(6), s(1) + dtw(5));
    EXPECT_EQ(dtw(6), s(5) + dtw(1));
    EXPECT_EQ(dtw(6), dtw(1) + s(5));
    EXPECT_EQ(dtw(6), dtw(5) + s(1));
    EXPECT_EQ(dtm(6), s(1) + dtm(5));
    EXPECT_EQ(dtm(6), s(5) + dtm(1));
    EXPECT_EQ(dtm(6), dtm(1) + s(5));
    EXPECT_EQ(dtm(6), dtm(5) + s(1));
}

TEST(WTF_Time, minus)
{
    EXPECT_EQ(s(-4), s(1) - s(5));
    EXPECT_EQ(s(4), s(5) - s(1));
    EXPECT_EQ(wt(-4), s(1) - wt(5));
    EXPECT_EQ(wt(4), s(5) - wt(1));
    EXPECT_EQ(wt(-4), wt(1) - s(5));
    EXPECT_EQ(wt(4), wt(5) - s(1));
    EXPECT_EQ(mt(-4), s(1) - mt(5));
    EXPECT_EQ(mt(4), s(5) - mt(1));
    EXPECT_EQ(mt(-4), mt(1) - s(5));
    EXPECT_EQ(mt(4), mt(5) - s(1));
    EXPECT_EQ(dtw(-4), s(1) - dtw(5));
    EXPECT_EQ(dtw(4), s(5) - dtw(1));
    EXPECT_EQ(dtw(-4), dtw(1) - s(5));
    EXPECT_EQ(dtw(4), dtw(5) - s(1));
    EXPECT_EQ(dtm(-4), s(1) - dtm(5));
    EXPECT_EQ(dtm(4), s(5) - dtm(1));
    EXPECT_EQ(dtm(-4), dtm(1) - s(5));
    EXPECT_EQ(dtm(4), dtm(5) - s(1));
}

TEST(WTF_Time, negate)
{
    EXPECT_EQ(s(-7), -s(7));
    EXPECT_EQ(s(7), -s(-7));
    EXPECT_EQ(wt(-7), -wt(7));
    EXPECT_EQ(wt(7), -wt(-7));
    EXPECT_EQ(mt(-7), -mt(7));
    EXPECT_EQ(mt(7), -mt(-7));
    EXPECT_EQ(dtw(-7), -dtw(7));
    EXPECT_EQ(dtw(7), -dtw(-7));
    EXPECT_EQ(dtm(-7), -dtm(7));
    EXPECT_EQ(dtm(7), -dtm(-7));
}

TEST(WTF_Time, times)
{
    EXPECT_EQ(s(15), s(3) * 5);
}

TEST(WTF_Time, divide)
{
    EXPECT_EQ(s(3), s(15) / 5);
}

TEST(WTF_Time, less)
{
    EXPECT_FALSE(s(2) < s(1));
    EXPECT_FALSE(s(2) < s(2));
    EXPECT_TRUE(s(2) < s(3));
    EXPECT_FALSE(wt(2) < wt(1));
    EXPECT_FALSE(wt(2) < wt(2));
    EXPECT_TRUE(wt(2) < wt(3));
    EXPECT_FALSE(mt(2) < mt(1));
    EXPECT_FALSE(mt(2) < mt(2));
    EXPECT_TRUE(mt(2) < mt(3));
    EXPECT_FALSE(dtw(2) < dtw(1));
    EXPECT_FALSE(dtw(2) < dtw(2));
    EXPECT_TRUE(dtw(2) < dtw(3));
    EXPECT_FALSE(dtm(2) < dtm(1));
    EXPECT_FALSE(dtm(2) < dtm(2));
    EXPECT_TRUE(dtm(2) < dtm(3));
}

TEST(WTF_Time, lessEqual)
{
    EXPECT_FALSE(s(2) <= s(1));
    EXPECT_TRUE(s(2) <= s(2));
    EXPECT_TRUE(s(2) <= s(3));
    EXPECT_FALSE(wt(2) <= wt(1));
    EXPECT_TRUE(wt(2) <= wt(2));
    EXPECT_TRUE(wt(2) <= wt(3));
    EXPECT_FALSE(mt(2) <= mt(1));
    EXPECT_TRUE(mt(2) <= mt(2));
    EXPECT_TRUE(mt(2) <= mt(3));
    EXPECT_FALSE(dtw(2) <= dtw(1));
    EXPECT_TRUE(dtw(2) <= dtw(2));
    EXPECT_TRUE(dtw(2) <= dtw(3));
    EXPECT_FALSE(dtm(2) <= dtm(1));
    EXPECT_TRUE(dtm(2) <= dtm(2));
    EXPECT_TRUE(dtm(2) <= dtm(3));
}

TEST(WTF_Time, greater)
{
    EXPECT_TRUE(s(2) > s(1));
    EXPECT_FALSE(s(2) > s(2));
    EXPECT_FALSE(s(2) > s(3));
    EXPECT_TRUE(wt(2) > wt(1));
    EXPECT_FALSE(wt(2) > wt(2));
    EXPECT_FALSE(wt(2) > wt(3));
    EXPECT_TRUE(mt(2) > mt(1));
    EXPECT_FALSE(mt(2) > mt(2));
    EXPECT_FALSE(mt(2) > mt(3));
    EXPECT_TRUE(dtw(2) > dtw(1));
    EXPECT_FALSE(dtw(2) > dtw(2));
    EXPECT_FALSE(dtw(2) > dtw(3));
    EXPECT_TRUE(dtm(2) > dtm(1));
    EXPECT_FALSE(dtm(2) > dtm(2));
    EXPECT_FALSE(dtm(2) > dtm(3));
}

TEST(WTF_Time, greaterEqual)
{
    EXPECT_TRUE(s(2) >= s(1));
    EXPECT_TRUE(s(2) >= s(2));
    EXPECT_FALSE(s(2) >= s(3));
    EXPECT_TRUE(wt(2) >= wt(1));
    EXPECT_TRUE(wt(2) >= wt(2));
    EXPECT_FALSE(wt(2) >= wt(3));
    EXPECT_TRUE(mt(2) >= mt(1));
    EXPECT_TRUE(mt(2) >= mt(2));
    EXPECT_FALSE(mt(2) >= mt(3));
    EXPECT_TRUE(dtw(2) >= dtw(1));
    EXPECT_TRUE(dtw(2) >= dtw(2));
    EXPECT_FALSE(dtw(2) >= dtw(3));
    EXPECT_TRUE(dtm(2) >= dtm(1));
    EXPECT_TRUE(dtm(2) >= dtm(2));
    EXPECT_FALSE(dtm(2) >= dtm(3));
}

TEST(WTF_Time, equal)
{
    EXPECT_FALSE(s(2) == s(1));
    EXPECT_TRUE(s(2) == s(2));
    EXPECT_FALSE(s(2) == s(3));
    EXPECT_FALSE(wt(2) == wt(1));
    EXPECT_TRUE(wt(2) == wt(2));
    EXPECT_FALSE(wt(2) == wt(3));
    EXPECT_FALSE(mt(2) == mt(1));
    EXPECT_TRUE(mt(2) == mt(2));
    EXPECT_FALSE(mt(2) == mt(3));
    EXPECT_FALSE(dtw(2) == dtw(1));
    EXPECT_TRUE(dtw(2) == dtw(2));
    EXPECT_FALSE(dtw(2) == dtw(3));
    EXPECT_FALSE(dtm(2) == dtm(1));
    EXPECT_TRUE(dtm(2) == dtm(2));
    EXPECT_FALSE(dtm(2) == dtm(3));
}

TEST(WTF_Time, notEqual)
{
    EXPECT_TRUE(s(2) != s(1));
    EXPECT_FALSE(s(2) != s(2));
    EXPECT_TRUE(s(2) != s(3));
    EXPECT_TRUE(wt(2) != wt(1));
    EXPECT_FALSE(wt(2) != wt(2));
    EXPECT_TRUE(wt(2) != wt(3));
    EXPECT_TRUE(mt(2) != mt(1));
    EXPECT_FALSE(mt(2) != mt(2));
    EXPECT_TRUE(mt(2) != mt(3));
    EXPECT_TRUE(dtw(2) != dtw(1));
    EXPECT_FALSE(dtw(2) != dtw(2));
    EXPECT_TRUE(dtw(2) != dtw(3));
    EXPECT_TRUE(dtm(2) != dtm(1));
    EXPECT_FALSE(dtm(2) != dtm(2));
    EXPECT_TRUE(dtm(2) != dtm(3));
}

TEST(WTF_Time, literals)
{
    EXPECT_TRUE(s(120) == 2_min);
    EXPECT_TRUE(s(2) == 2_s);
    EXPECT_TRUE(s(2) == 2000_ms);
    EXPECT_TRUE(s(2) - 1000_ms == s(1));
    EXPECT_TRUE(2_s - s(1) == 1000_ms);

    EXPECT_TRUE(Seconds::fromMinutes(2) == 2_min);
    EXPECT_TRUE(Seconds(2) == 2_s);
    EXPECT_TRUE(Seconds::fromMilliseconds(2) == 2_ms);
    EXPECT_TRUE(Seconds::fromMicroseconds(2) == 2_us);
    EXPECT_TRUE(Seconds::fromNanoseconds(2) == 2_ns);

    EXPECT_TRUE(Seconds::fromMinutes(2.5) == 2.5_min);
    EXPECT_TRUE(Seconds(2.5) == 2.5_s);
    EXPECT_TRUE(Seconds::fromMilliseconds(2.5) == 2.5_ms);
    EXPECT_TRUE(Seconds::fromMicroseconds(2.5) == 2.5_us);
    EXPECT_TRUE(Seconds::fromNanoseconds(2.5) == 2.5_ns);
}

TEST(WTF_Time, clamp)
{
    Seconds positiveInfinity = Seconds::infinity();
    EXPECT_TRUE(positiveInfinity.secondsAs<int32_t>() == INT32_MAX);
    EXPECT_TRUE(positiveInfinity.secondsAs<uint32_t>() == UINT32_MAX);
    EXPECT_TRUE(positiveInfinity.secondsAs<int64_t>() == INT64_MAX);
    EXPECT_TRUE(positiveInfinity.secondsAs<uint64_t>() == UINT64_MAX);
    EXPECT_TRUE(positiveInfinity.millisecondsAs<int32_t>() == INT32_MAX);
    EXPECT_TRUE(positiveInfinity.millisecondsAs<uint32_t>() == UINT32_MAX);
    EXPECT_TRUE(positiveInfinity.millisecondsAs<int64_t>() == INT64_MAX);
    EXPECT_TRUE(positiveInfinity.millisecondsAs<uint64_t>() == UINT64_MAX);
    EXPECT_TRUE(positiveInfinity.microsecondsAs<int32_t>() == INT32_MAX);
    EXPECT_TRUE(positiveInfinity.microsecondsAs<uint32_t>() == UINT32_MAX);
    EXPECT_TRUE(positiveInfinity.microsecondsAs<int64_t>() == INT64_MAX);
    EXPECT_TRUE(positiveInfinity.microsecondsAs<uint64_t>() == UINT64_MAX);
    EXPECT_TRUE(positiveInfinity.nanosecondsAs<int32_t>() == INT32_MAX);
    EXPECT_TRUE(positiveInfinity.nanosecondsAs<uint32_t>() == UINT32_MAX);
    EXPECT_TRUE(positiveInfinity.nanosecondsAs<int64_t>() == INT64_MAX);
    EXPECT_TRUE(positiveInfinity.nanosecondsAs<uint64_t>() == UINT64_MAX);

    Seconds negativeInfinity = -Seconds::infinity();
    EXPECT_TRUE(negativeInfinity.secondsAs<int32_t>() == INT32_MIN);
    EXPECT_TRUE(negativeInfinity.secondsAs<uint32_t>() == 0);
    EXPECT_TRUE(negativeInfinity.secondsAs<int64_t>() == INT64_MIN);
    EXPECT_TRUE(negativeInfinity.secondsAs<uint64_t>() == 0);
    EXPECT_TRUE(negativeInfinity.millisecondsAs<int32_t>() == INT32_MIN);
    EXPECT_TRUE(negativeInfinity.millisecondsAs<uint32_t>() == 0);
    EXPECT_TRUE(negativeInfinity.millisecondsAs<int64_t>() == INT64_MIN);
    EXPECT_TRUE(negativeInfinity.millisecondsAs<uint64_t>() == 0);
    EXPECT_TRUE(negativeInfinity.microsecondsAs<int32_t>() == INT32_MIN);
    EXPECT_TRUE(negativeInfinity.microsecondsAs<uint32_t>() == 0);
    EXPECT_TRUE(negativeInfinity.microsecondsAs<int64_t>() == INT64_MIN);
    EXPECT_TRUE(negativeInfinity.microsecondsAs<uint64_t>() == 0);
    EXPECT_TRUE(negativeInfinity.nanosecondsAs<int32_t>() == INT32_MIN);
    EXPECT_TRUE(negativeInfinity.nanosecondsAs<uint32_t>() == 0);
    EXPECT_TRUE(negativeInfinity.nanosecondsAs<int64_t>() == INT64_MIN);
    EXPECT_TRUE(negativeInfinity.nanosecondsAs<uint64_t>() == 0);
}

// Test MonotonicTime constexpr features. If they are not calculated in constexpr,
// they invokes global constructors and becomes compile errors.
static const MonotonicTime NaN = MonotonicTime::nan();
static const MonotonicTime Infinity = MonotonicTime::infinity();
static const MonotonicTime Zero = MonotonicTime::fromRawSeconds(0);
static const MonotonicTime One = Zero + Seconds(1);
static const MonotonicTime NegativeOne = Zero - Seconds(1);
static const bool ZeroIsFalse = !!Zero;
static const bool Equal = Zero == Zero;
static const bool NotEqual = Zero != One;
static const bool LessThan = Zero < One;
static const bool GreaterThan = One > Zero;
static const bool LessThanOrEqual = Zero <= Zero;
static const bool GreaterThanOrEqual = Zero >= Zero;

TEST(WTF_Time, constexprMonotonicTime)
{
    EXPECT_TRUE(std::isnan(NaN));
    EXPECT_TRUE(std::isinf(Infinity));
    EXPECT_TRUE(Zero.secondsSinceEpoch().value() == 0.0);
    EXPECT_TRUE(One.secondsSinceEpoch().value() == 1.0);
    EXPECT_TRUE(NegativeOne.secondsSinceEpoch().value() == -1.0);
    EXPECT_FALSE(ZeroIsFalse);
    EXPECT_TRUE(Equal);
    EXPECT_TRUE(NotEqual);
    EXPECT_TRUE(LessThan);
    EXPECT_TRUE(GreaterThan);
    EXPECT_TRUE(LessThanOrEqual);
    EXPECT_TRUE(GreaterThanOrEqual);
}


} // namespace TestWebKitAPI

