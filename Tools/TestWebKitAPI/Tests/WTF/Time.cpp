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

using namespace WTF;

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
    EXPECT_EQ(false, s(2) < s(1));
    EXPECT_EQ(false, s(2) < s(2));
    EXPECT_EQ(true, s(2) < s(3));
    EXPECT_EQ(false, wt(2) < wt(1));
    EXPECT_EQ(false, wt(2) < wt(2));
    EXPECT_EQ(true, wt(2) < wt(3));
    EXPECT_EQ(false, mt(2) < mt(1));
    EXPECT_EQ(false, mt(2) < mt(2));
    EXPECT_EQ(true, mt(2) < mt(3));
    EXPECT_EQ(false, dtw(2) < dtw(1));
    EXPECT_EQ(false, dtw(2) < dtw(2));
    EXPECT_EQ(true, dtw(2) < dtw(3));
    EXPECT_EQ(false, dtm(2) < dtm(1));
    EXPECT_EQ(false, dtm(2) < dtm(2));
    EXPECT_EQ(true, dtm(2) < dtm(3));
}

TEST(WTF_Time, lessEqual)
{
    EXPECT_EQ(false, s(2) <= s(1));
    EXPECT_EQ(true, s(2) <= s(2));
    EXPECT_EQ(true, s(2) <= s(3));
    EXPECT_EQ(false, wt(2) <= wt(1));
    EXPECT_EQ(true, wt(2) <= wt(2));
    EXPECT_EQ(true, wt(2) <= wt(3));
    EXPECT_EQ(false, mt(2) <= mt(1));
    EXPECT_EQ(true, mt(2) <= mt(2));
    EXPECT_EQ(true, mt(2) <= mt(3));
    EXPECT_EQ(false, dtw(2) <= dtw(1));
    EXPECT_EQ(true, dtw(2) <= dtw(2));
    EXPECT_EQ(true, dtw(2) <= dtw(3));
    EXPECT_EQ(false, dtm(2) <= dtm(1));
    EXPECT_EQ(true, dtm(2) <= dtm(2));
    EXPECT_EQ(true, dtm(2) <= dtm(3));
}

TEST(WTF_Time, greater)
{
    EXPECT_EQ(true, s(2) > s(1));
    EXPECT_EQ(false, s(2) > s(2));
    EXPECT_EQ(false, s(2) > s(3));
    EXPECT_EQ(true, wt(2) > wt(1));
    EXPECT_EQ(false, wt(2) > wt(2));
    EXPECT_EQ(false, wt(2) > wt(3));
    EXPECT_EQ(true, mt(2) > mt(1));
    EXPECT_EQ(false, mt(2) > mt(2));
    EXPECT_EQ(false, mt(2) > mt(3));
    EXPECT_EQ(true, dtw(2) > dtw(1));
    EXPECT_EQ(false, dtw(2) > dtw(2));
    EXPECT_EQ(false, dtw(2) > dtw(3));
    EXPECT_EQ(true, dtm(2) > dtm(1));
    EXPECT_EQ(false, dtm(2) > dtm(2));
    EXPECT_EQ(false, dtm(2) > dtm(3));
}

TEST(WTF_Time, greaterEqual)
{
    EXPECT_EQ(true, s(2) >= s(1));
    EXPECT_EQ(true, s(2) >= s(2));
    EXPECT_EQ(false, s(2) >= s(3));
    EXPECT_EQ(true, wt(2) >= wt(1));
    EXPECT_EQ(true, wt(2) >= wt(2));
    EXPECT_EQ(false, wt(2) >= wt(3));
    EXPECT_EQ(true, mt(2) >= mt(1));
    EXPECT_EQ(true, mt(2) >= mt(2));
    EXPECT_EQ(false, mt(2) >= mt(3));
    EXPECT_EQ(true, dtw(2) >= dtw(1));
    EXPECT_EQ(true, dtw(2) >= dtw(2));
    EXPECT_EQ(false, dtw(2) >= dtw(3));
    EXPECT_EQ(true, dtm(2) >= dtm(1));
    EXPECT_EQ(true, dtm(2) >= dtm(2));
    EXPECT_EQ(false, dtm(2) >= dtm(3));
}

TEST(WTF_Time, equal)
{
    EXPECT_EQ(false, s(2) == s(1));
    EXPECT_EQ(true, s(2) == s(2));
    EXPECT_EQ(false, s(2) == s(3));
    EXPECT_EQ(false, wt(2) == wt(1));
    EXPECT_EQ(true, wt(2) == wt(2));
    EXPECT_EQ(false, wt(2) == wt(3));
    EXPECT_EQ(false, mt(2) == mt(1));
    EXPECT_EQ(true, mt(2) == mt(2));
    EXPECT_EQ(false, mt(2) == mt(3));
    EXPECT_EQ(false, dtw(2) == dtw(1));
    EXPECT_EQ(true, dtw(2) == dtw(2));
    EXPECT_EQ(false, dtw(2) == dtw(3));
    EXPECT_EQ(false, dtm(2) == dtm(1));
    EXPECT_EQ(true, dtm(2) == dtm(2));
    EXPECT_EQ(false, dtm(2) == dtm(3));
}

TEST(WTF_Time, notEqual)
{
    EXPECT_EQ(true, s(2) != s(1));
    EXPECT_EQ(false, s(2) != s(2));
    EXPECT_EQ(true, s(2) != s(3));
    EXPECT_EQ(true, wt(2) != wt(1));
    EXPECT_EQ(false, wt(2) != wt(2));
    EXPECT_EQ(true, wt(2) != wt(3));
    EXPECT_EQ(true, mt(2) != mt(1));
    EXPECT_EQ(false, mt(2) != mt(2));
    EXPECT_EQ(true, mt(2) != mt(3));
    EXPECT_EQ(true, dtw(2) != dtw(1));
    EXPECT_EQ(false, dtw(2) != dtw(2));
    EXPECT_EQ(true, dtw(2) != dtw(3));
    EXPECT_EQ(true, dtm(2) != dtm(1));
    EXPECT_EQ(false, dtm(2) != dtm(2));
    EXPECT_EQ(true, dtm(2) != dtm(3));
}

} // namespace TestWebKitAPI

