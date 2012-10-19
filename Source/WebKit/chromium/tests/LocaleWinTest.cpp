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

#include "DateComponents.h"
#include <gtest/gtest.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace std;

class LocaleWinTest : public ::testing::Test {
protected:
    enum {
        January = 0, February, March,
        April, May, June,
        July, August, September,
        October, November, December,
    };

    enum {
        Sunday = 0, Monday, Tuesday,
        Wednesday, Thursday, Friday,
        Saturday,
    };

    // See http://msdn.microsoft.com/en-us/goglobal/bb964664.aspx
    // Note that some locales are country-neutral.
    enum {
        ArabicEG = 0x0C01, // ar-eg
        ChineseCN = 0x0804, // zh-cn
        ChineseHK = 0x0C04, // zh-hk
        ChineseTW = 0x0404, // zh-tw
        German = 0x0407, // de
        EnglishUS = 0x409, // en-us
        FrenchFR = 0x40C, // fr
        JapaneseJP = 0x411, // ja
        KoreanKR = 0x0412, // ko
        Persian = 0x0429, // fa
        Spanish = 0x040A, // es
    };

    DateComponents dateComponents(int year, int month, int day)
    {
        DateComponents date;
        date.setMillisecondsSinceEpochForDate(msForDate(year, month, day));
        return date;
    }

    double msForDate(int year, int month, int day)
    {
        return dateToDaysFrom1970(year, month, day) * msPerDay;
    }

    String formatDate(LCID lcid, int year, int month, int day)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->formatDateTime(dateComponents(year, month, day));
    }

    double parseDate(LCID lcid, const String& dateString)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->parseDateTime(dateString, DateComponents::Date);
    }

#if ENABLE(CALENDAR_PICKER)
    String dateFormatText(LCID lcid)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->dateFormatText();
    }

    unsigned firstDayOfWeek(LCID lcid)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->firstDayOfWeek();
    }

    String monthLabel(LCID lcid, unsigned index)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->monthLabels()[index];
    }

    String weekDayShortLabel(LCID lcid, unsigned index)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->weekDayShortLabels()[index];
    }

    bool isRTL(LCID lcid)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->isRTL();
    }
#endif

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    String monthFormat(LCID lcid)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->monthFormat();
    }

    String timeFormat(LCID lcid)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->timeFormat();
    }

    String shortTimeFormat(LCID lcid)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->shortTimeFormat();
    }

    String shortMonthLabel(LCID lcid, unsigned index)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->shortMonthLabels()[index];
    }

    String timeAMPMLabel(LCID lcid, unsigned index)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->timeAMPMLabels()[index];
    }

    String decimalSeparator(LCID lcid)
    {
        OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
        return locale->localizedDecimalSeparator();
    }
#endif
};

TEST_F(LocaleWinTest, TestLocalizedDateFormatText)
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

TEST_F(LocaleWinTest, TestParse)
{
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

    OwnPtr<LocaleWin> persian = LocaleWin::create(Persian);
    // U+06F1 U+06F6 / U+06F0 U+06F8 / 2012
    EXPECT_EQ(msForDate(2012, August, 16), persian->parseDate("dd/MM/yyyy", 2012, String::fromUTF8("\xDB\xB1\xDB\xB6/\xDB\xB0\xDB\xB8/2012")));
}

TEST_F(LocaleWinTest, formatDate)
{
    EXPECT_STREQ("04/27/2005", formatDate(EnglishUS, 2005, April, 27).utf8().data());
    EXPECT_STREQ("27/04/2005", formatDate(FrenchFR, 2005, April, 27).utf8().data());
    EXPECT_STREQ("2005/04/27", formatDate(JapaneseJP, 2005, April, 27).utf8().data());
}

TEST_F(LocaleWinTest, parseDate)
{
    EXPECT_EQ(msForDate(2005, April, 27), parseDate(EnglishUS, "April/27/2005"));
    EXPECT_EQ(msForDate(2005, April, 27), parseDate(FrenchFR, "27/avril/2005"));
    EXPECT_EQ(msForDate(2005, April, 27), parseDate(JapaneseJP, "2005/04/27"));
}

#if ENABLE(CALENDAR_PICKER)
TEST_F(LocaleWinTest, dateFormatText)
{
    EXPECT_STREQ("Month/Day/Year", dateFormatText(EnglishUS).utf8().data());
    EXPECT_STREQ("Day/Month/Year", dateFormatText(FrenchFR).utf8().data());
    EXPECT_STREQ("Year/Month/Day", dateFormatText(JapaneseJP).utf8().data());
}

TEST_F(LocaleWinTest, firstDayOfWeek)
{
    EXPECT_EQ(Sunday, firstDayOfWeek(EnglishUS));
    EXPECT_EQ(Monday, firstDayOfWeek(FrenchFR));
    EXPECT_EQ(Sunday, firstDayOfWeek(JapaneseJP));
}

TEST_F(LocaleWinTest, monthLabels)
{
    EXPECT_STREQ("January", monthLabel(EnglishUS, January).utf8().data());
    EXPECT_STREQ("June", monthLabel(EnglishUS, June).utf8().data());
    EXPECT_STREQ("December", monthLabel(EnglishUS, December).utf8().data());

    EXPECT_STREQ("janvier", monthLabel(FrenchFR, January).utf8().data());
    EXPECT_STREQ("juin", monthLabel(FrenchFR, June).utf8().data());
    EXPECT_STREQ("d\xC3\xA9" "cembre", monthLabel(FrenchFR, December).utf8().data());

    EXPECT_STREQ("1\xE6\x9C\x88", monthLabel(JapaneseJP, January).utf8().data());
    EXPECT_STREQ("6\xE6\x9C\x88", monthLabel(JapaneseJP, June).utf8().data());
    EXPECT_STREQ("12\xE6\x9C\x88", monthLabel(JapaneseJP, December).utf8().data());
}

TEST_F(LocaleWinTest, weekDayShortLabels)
{
    EXPECT_STREQ("Sun", weekDayShortLabel(EnglishUS, Sunday).utf8().data());
    EXPECT_STREQ("Wed", weekDayShortLabel(EnglishUS, Wednesday).utf8().data());
    EXPECT_STREQ("Sat", weekDayShortLabel(EnglishUS, Saturday).utf8().data());

    EXPECT_STREQ("dim.", weekDayShortLabel(FrenchFR, Sunday).utf8().data());
    EXPECT_STREQ("mer.", weekDayShortLabel(FrenchFR, Wednesday).utf8().data());
    EXPECT_STREQ("sam.", weekDayShortLabel(FrenchFR, Saturday).utf8().data());

    EXPECT_STREQ("\xE6\x97\xA5", weekDayShortLabel(JapaneseJP, Sunday).utf8().data());
    EXPECT_STREQ("\xE6\xB0\xB4", weekDayShortLabel(JapaneseJP, Wednesday).utf8().data());
    EXPECT_STREQ("\xE5\x9C\x9F", weekDayShortLabel(JapaneseJP, Saturday).utf8().data());
}

TEST_F(LocaleWinTest, isRTL)
{
    EXPECT_TRUE(isRTL(ArabicEG));
    EXPECT_FALSE(isRTL(EnglishUS));
}

#endif

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
TEST_F(LocaleWinTest, dateFormat)
{
    EXPECT_STREQ("y'-'M'-'d", LocaleWin::dateFormat("y-M-d").utf8().data());
    EXPECT_STREQ("''yy'-'''MM'''-'dd", LocaleWin::dateFormat("''yy-''MM''-dd").utf8().data());
    EXPECT_STREQ("yyyy'-''''-'MMM'''''-'dd", LocaleWin::dateFormat("yyyy-''''-MMM''''-dd").utf8().data());
    EXPECT_STREQ("yyyy'-'''''MMMM'-'dd", LocaleWin::dateFormat("yyyy-''''MMMM-dd").utf8().data());
}

TEST_F(LocaleWinTest, monthFormat)
{
    EXPECT_STREQ("MMMM', 'yyyy", monthFormat(EnglishUS).utf8().data());
    EXPECT_STREQ("MMMM' 'yyyy", monthFormat(FrenchFR).utf8().data());
    EXPECT_STREQ("yyyy'\xE5\xB9\xB4'M'\xE6\x9C\x88'", monthFormat(JapaneseJP).utf8().data());
}

TEST_F(LocaleWinTest, timeFormat)
{
    EXPECT_STREQ("h:mm:ss a", timeFormat(EnglishUS).utf8().data());
    EXPECT_STREQ("HH:mm:ss", timeFormat(FrenchFR).utf8().data());
    EXPECT_STREQ("H:mm:ss", timeFormat(JapaneseJP).utf8().data());
}

TEST_F(LocaleWinTest, shortTimeFormat)
{
    EXPECT_STREQ("h:mm:ss a", shortTimeFormat(EnglishUS).utf8().data());
    EXPECT_STREQ("HH:mm:ss", shortTimeFormat(FrenchFR).utf8().data());
    EXPECT_STREQ("H:mm:ss", shortTimeFormat(JapaneseJP).utf8().data());
}

TEST_F(LocaleWinTest, shortMonthLabels)
{
    EXPECT_STREQ("Jan", shortMonthLabel(EnglishUS, 0).utf8().data());
    EXPECT_STREQ("Dec", shortMonthLabel(EnglishUS, 11).utf8().data());
    EXPECT_STREQ("janv.", shortMonthLabel(FrenchFR, 0).utf8().data());
    EXPECT_STREQ("d\xC3\xA9" "c.", shortMonthLabel(FrenchFR, 11).utf8().data());
    EXPECT_STREQ("1", shortMonthLabel(JapaneseJP, 0).utf8().data());
    EXPECT_STREQ("12", shortMonthLabel(JapaneseJP, 11).utf8().data());
}

TEST_F(LocaleWinTest, timeAMPMLabels)
{
    EXPECT_STREQ("AM", timeAMPMLabel(EnglishUS, 0).utf8().data());
    EXPECT_STREQ("PM", timeAMPMLabel(EnglishUS, 1).utf8().data());

    EXPECT_STREQ("", timeAMPMLabel(FrenchFR, 0).utf8().data());
    EXPECT_STREQ("", timeAMPMLabel(FrenchFR, 1).utf8().data());

    EXPECT_STREQ("\xE5\x8D\x88\xE5\x89\x8D", timeAMPMLabel(JapaneseJP, 0).utf8().data());
    EXPECT_STREQ("\xE5\x8D\x88\xE5\xBE\x8C", timeAMPMLabel(JapaneseJP, 1).utf8().data());
}

TEST_F(LocaleWinTest, decimalSeparator)
{
    EXPECT_STREQ(".", decimalSeparator(EnglishUS).utf8().data());
    EXPECT_STREQ(",", decimalSeparator(FrenchFR).utf8().data());
}
#endif

static void testNumberIsReversible(LCID lcid, const char* original, const char* shouldHave = 0)
{
    OwnPtr<LocaleWin> locale = LocaleWin::create(lcid);
    String localized = locale->convertToLocalizedNumber(original);
    if (shouldHave)
        EXPECT_TRUE(localized.contains(shouldHave));
    String converted = locale->convertFromLocalizedNumber(localized);
    EXPECT_STREQ(original, converted.utf8().data());
}

void testNumbers(LCID lcid)
{
    testNumberIsReversible(lcid, "123456789012345678901234567890");
    testNumberIsReversible(lcid, "-123.456");
    testNumberIsReversible(lcid, ".456");
    testNumberIsReversible(lcid, "-0.456");
}

TEST_F(LocaleWinTest, localizedNumberRoundTrip)
{
    testNumberIsReversible(EnglishUS, "123456789012345678901234567890");
    testNumberIsReversible(EnglishUS, "-123.456", ".");
    testNumberIsReversible(EnglishUS, ".456", ".");
    testNumberIsReversible(EnglishUS, "-0.456", ".");

    testNumberIsReversible(FrenchFR, "123456789012345678901234567890");
    testNumberIsReversible(FrenchFR, "-123.456", ",");
    testNumberIsReversible(FrenchFR, ".456", ",");
    testNumberIsReversible(FrenchFR, "-0.456", ",");

    // Test some of major locales.
    testNumbers(ArabicEG);
    testNumbers(German);
    testNumbers(Spanish);
    testNumbers(Persian);
    testNumbers(JapaneseJP);
    testNumbers(KoreanKR);
    testNumbers(ChineseCN);
    testNumbers(ChineseHK);
    testNumbers(ChineseTW);
}
