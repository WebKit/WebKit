/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LocaleWin.h"

#include <gtest/gtest.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace std;

enum {
    January = 0, February, March,
    April, May, June,
    July, August, September,
    October, November, December,
};

static double msForDate(int year, int month, int day)
{
    return dateToDaysFrom1970(year, month, day) * msPerDay;
}

TEST(LocaleWinTest, TestLocalizedDateFormatText)
{
    EXPECT_STREQ("year/month/day", LocaleWin::dateFormatText("y/M/d", "year", "month", "day").utf8().data());
    EXPECT_STREQ("year/month/day", LocaleWin::dateFormatText("yy/MM/dd", "year", "month", "day").utf8().data());
    EXPECT_STREQ("year/month/day", LocaleWin::dateFormatText("yyy/MMM/ddd", "year", "month", "day").utf8().data());
    EXPECT_STREQ("year/month/day", LocaleWin::dateFormatText("yyyy/MMMM/dddd", "year", "month", "day").utf8().data());
    EXPECT_STREQ("/month/day, year", LocaleWin::dateFormatText("/MM/dd, yyyy", "year", "month", "day").utf8().data());
    EXPECT_STREQ("month/day, year=year.", LocaleWin::dateFormatText("MM/dd, 'year='yyyy.", "year", "month", "day").utf8().data());
    EXPECT_STREQ("month-day 'year", LocaleWin::dateFormatText("MM-dd ''yyy", "year", "month", "day").utf8().data());
    EXPECT_STREQ("month-day aaa'bbb year", LocaleWin::dateFormatText("MM-dd 'aaa''bbb' yyy", "year", "month", "day").utf8().data());
    EXPECT_STREQ("year/month/day/year/month/day", LocaleWin::dateFormatText("yyyy/MMMM/dddd/yyyy/MMMM/dddd", "year", "month", "day").utf8().data());
    EXPECT_STREQ("YY/mm/DD", LocaleWin::dateFormatText("YY/mm/DD", "year", "month", "day").utf8().data());
}

TEST(LocaleWinTest, TestFormat)
{
    const LCID EnglishUS = 0x0409;
    OwnPtr<LocaleWin> locale = LocaleWin::create(EnglishUS);

    EXPECT_STREQ("4/7/2", locale->formatDate("M/d/y", 2012, 2012, April, 7).utf8().data());
    EXPECT_STREQ("4/7/2007", locale->formatDate("M/d/y", 2012, 2007, April, 7).utf8().data());
    EXPECT_STREQ("4/7/8", locale->formatDate("M/d/y", 2012, 2008, April, 7).utf8().data());
    EXPECT_STREQ("4/7/7", locale->formatDate("M/d/y", 2012, 2017, April, 7).utf8().data());
    EXPECT_STREQ("4/7/2018", locale->formatDate("M/d/y", 2012, 2018, April, 7).utf8().data());
    EXPECT_STREQ("12/31/2062", locale->formatDate("M/d/y", 2012, 2062, December, 31).utf8().data());
    EXPECT_STREQ("4/7/0002", locale->formatDate("M/d/y", 2012, 2, April, 7).utf8().data());

    EXPECT_STREQ("04/27/12", locale->formatDate("MM/dd/yy", 2012, 2012, April, 27).utf8().data());
    EXPECT_STREQ("04/07/1962", locale->formatDate("MM/dd/yy", 2012, 1962, April, 7).utf8().data());
    EXPECT_STREQ("04/07/63", locale->formatDate("MM/dd/yy", 2012, 1963, April, 7).utf8().data());
    EXPECT_STREQ("01/31/00", locale->formatDate("MM/dd/yy", 2012, 2000, January, 31).utf8().data());
    EXPECT_STREQ("04/07/62", locale->formatDate("MM/dd/yy", 2012, 2062, April, 7).utf8().data());
    EXPECT_STREQ("04/07/2063", locale->formatDate("MM/dd/yy", 2012, 2063, April, 7).utf8().data());
    EXPECT_STREQ("04/07/0001", locale->formatDate("MM/dd/yy", 2012, 1, April, 7).utf8().data());

    EXPECT_STREQ("Jan/7/2012", locale->formatDate("MMM/d/yyyy", 2012, 2012, January, 7).utf8().data());
    EXPECT_STREQ("Feb/7/2008", locale->formatDate("MMM/d/yyyy", 2012, 2008, February, 7).utf8().data());
    EXPECT_STREQ("Mar/7/2017", locale->formatDate("MMM/d/yyyy", 2012, 2017, March, 7).utf8().data());
    EXPECT_STREQ("Apr/7/2012", locale->formatDate("MMM/d/yyyy", 2012, 2012, April, 7).utf8().data());
    EXPECT_STREQ("May/7/0002", locale->formatDate("MMM/d/yyyy", 2012, 2, May, 7).utf8().data());
    EXPECT_STREQ("Jun/7/2008", locale->formatDate("MMM/d/yyyy", 2012, 2008, June, 7).utf8().data());
    EXPECT_STREQ("Jul/7/2017", locale->formatDate("MMM/d/yyyy", 2012, 2017, July, 7).utf8().data());
    EXPECT_STREQ("Aug/31/2062", locale->formatDate("MMM/d/yyyy", 2012, 2062, August, 31).utf8().data());
    EXPECT_STREQ("Sep/7/0002", locale->formatDate("MMM/d/yyyy", 2012, 2, September, 7).utf8().data());
    EXPECT_STREQ("Oct/7/2012", locale->formatDate("MMM/d/yyyy", 2012, 2012, October, 7).utf8().data());
    EXPECT_STREQ("Nov/7/2008", locale->formatDate("MMM/d/yyyy", 2012, 2008, November, 7).utf8().data());
    EXPECT_STREQ("Dec/31/2062", locale->formatDate("MMM/d/yyyy", 2012, 2062, December, 31).utf8().data());

    EXPECT_STREQ("January-7-2017", locale->formatDate("MMMM-d-yyyy", 2012, 2017, January, 7).utf8().data());
    EXPECT_STREQ("February-31-2062", locale->formatDate("MMMM-d-yyyy", 2012, 2062, February, 31).utf8().data());
    EXPECT_STREQ("March-7-0002", locale->formatDate("MMMM-d-yyyy", 2012, 2, March, 7).utf8().data());
    EXPECT_STREQ("April-7-22012", locale->formatDate("MMMM-d-yyyy", 2012, 22012, April, 7).utf8().data());
    EXPECT_STREQ("May-7-12008", locale->formatDate("MMMM-d-yyyy", 2012, 12008, May, 7).utf8().data());
    EXPECT_STREQ("June-7-22012", locale->formatDate("MMMM-d-yyyy", 2012, 22012, June, 7).utf8().data());
    EXPECT_STREQ("July-7-12008", locale->formatDate("MMMM-d-yyyy", 2012, 12008, July, 7).utf8().data());
    EXPECT_STREQ("August-7-2017", locale->formatDate("MMMM-d-yyyy", 2012, 2017, August, 7).utf8().data());
    EXPECT_STREQ("September-31-2062", locale->formatDate("MMMM-d-yyyy", 2012, 2062, September, 31).utf8().data());
    EXPECT_STREQ("October-7-0002", locale->formatDate("MMMM-d-yyyy", 2012, 2, October, 7).utf8().data());
    EXPECT_STREQ("November-7-22012", locale->formatDate("MMMM-d-yyyy", 2012, 22012, November, 7).utf8().data());
    EXPECT_STREQ("December-7-12008", locale->formatDate("MMMM-d-yyyy", 2012, 12008, December, 7).utf8().data());

    EXPECT_STREQ("Jan-1-0001", locale->formatDate("MMM-d-yyyy", 2012, 1, January, 1).utf8().data());
    EXPECT_STREQ("Sep-13-275760", locale->formatDate("MMM-d-yyyy", 2012, 275760, September, 13).utf8().data());
    

    // For the following test, we'd like to confirm they don't crash and their
    // results are not important because we can assume invalid arguments are
    // never passed.
    EXPECT_STREQ("2012-13-00", locale->formatDate("yyyy-MM-dd", -1, 2012, December + 1, 0).utf8().data());
    EXPECT_STREQ("-1-00--1", locale->formatDate("y-MM-dd", -1, -1, -1, -1).utf8().data());
    EXPECT_STREQ("-1-124-33", locale->formatDate("y-MM-dd", 2012, -1, 123, 33).utf8().data());
    
}

TEST(LocaleWinTest, TestParse)
{
    const LCID EnglishUS = 0x0409;
    OwnPtr<LocaleWin> locale = LocaleWin::create(EnglishUS);

    EXPECT_EQ(msForDate(2012, April, 27), locale->parseDate("MM/dd/yy", 2012, "04/27/12"));
    EXPECT_EQ(msForDate(2062, April, 27), locale->parseDate("MM/dd/yy", 2012, "04/27/62"));
    EXPECT_EQ(msForDate(1963, April, 27), locale->parseDate("MM/dd/yy", 2012, "04/27/63"));
    EXPECT_EQ(msForDate(2012, April, 27), locale->parseDate("MM/dd/yy", 2012, "4/27/2012"));
    EXPECT_EQ(msForDate(2012, April, 27), locale->parseDate("MM/dd/yy", 2012, "Apr/27/2012"));
    EXPECT_EQ(msForDate(2, April, 27), locale->parseDate("MM/d/yy", 2012, "April/27/2"));
    EXPECT_EQ(msForDate(2, April, 27), locale->parseDate("MM/d/yy", 2012, "april/27/2"));
    EXPECT_TRUE(isnan(locale->parseDate("MM/d/yy", 2012, "april/27")));
    EXPECT_TRUE(isnan(locale->parseDate("MM/d/yy", 2012, "april/27/")));
    EXPECT_TRUE(isnan(locale->parseDate("MM/d/yy", 2012, " april/27/")));

    EXPECT_EQ(msForDate(12, April, 7), locale->parseDate("MMM/d/yyyy", 2012, "04/7/12"));
    EXPECT_EQ(msForDate(62, April, 7), locale->parseDate("MMM/d/yyyy", 2012, "04/07/62"));
    EXPECT_EQ(msForDate(63, April, 7), locale->parseDate("MMM/d/yyyy", 2012, "04/07/63"));
    EXPECT_EQ(msForDate(2012, April, 7), locale->parseDate("MMM/d/yyyy", 2012, "4/7/2012"));
    EXPECT_EQ(msForDate(2012, May, 7), locale->parseDate("MMM/d/yyyy", 2012, "May/007/2012"));
    EXPECT_EQ(msForDate(2, May, 27), locale->parseDate("MM/d/yyyy", 2012, "May/0027/2"));
    EXPECT_TRUE(isnan(locale->parseDate("MM/d/yyyy", 2012, "May///0027///2")));
    EXPECT_TRUE(isnan(locale->parseDate("MM/d/yyyy", 2012, "Mayyyyyy/0027/2")));

    EXPECT_EQ(msForDate(2012, April, 27), locale->parseDate("MMMM/dd/y", 2012, "04/27/2"));
    EXPECT_EQ(msForDate(2017, April, 27), locale->parseDate("MMMM/dd/y", 2012, "04/27/7"));
    EXPECT_EQ(msForDate(2008, April, 27), locale->parseDate("MMMM/dd/y", 2012, "04/27/8"));
    EXPECT_EQ(msForDate(2012, April, 27), locale->parseDate("MMMM/dd/y", 2012, "4/27/2012"));
    EXPECT_EQ(msForDate(2012, December, 27), locale->parseDate("MMMM/dd/y", 2012, "December/27/2012"));
    EXPECT_EQ(msForDate(2012, November, 27), locale->parseDate("MMMM/d/y", 2012, "November/27/2"));
    EXPECT_TRUE(isnan(locale->parseDate("MMMM/d/y", 2012, "November 27 2")));
    EXPECT_TRUE(isnan(locale->parseDate("MMMM/d/y", 2012, "November 32 2")));
    EXPECT_TRUE(isnan(locale->parseDate("MMMM/d/y", 2012, "-1/-1/-1")));
}
