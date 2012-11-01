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

#if ENABLE(CALENDAR_PICKER)
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

TEST_F(LocaleWinTest, formatDate)
{
    EXPECT_STREQ("04/27/2005", formatDate(EnglishUS, 2005, April, 27).utf8().data());
    EXPECT_STREQ("27/04/2005", formatDate(FrenchFR, 2005, April, 27).utf8().data());
    EXPECT_STREQ("2005/04/27", formatDate(JapaneseJP, 2005, April, 27).utf8().data());
}

#if ENABLE(CALENDAR_PICKER)
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
    EXPECT_STREQ("h:mm a", shortTimeFormat(EnglishUS).utf8().data());
    EXPECT_STREQ("HH:mm", shortTimeFormat(FrenchFR).utf8().data());
    EXPECT_STREQ("H:mm", shortTimeFormat(JapaneseJP).utf8().data());
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
