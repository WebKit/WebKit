/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "LocaleMac.h"

#include "DateComponents.h"
#include <gtest/gtest.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;

class LocaleMacTest : public ::testing::Test {
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

    DateComponents dateComponents(int year, int month, int day)
    {
        DateComponents date;
        date.setMillisecondsSinceEpochForDate(msForDate(year, month, day));
        return date;
    }

    DateComponents timeComponents(int hour, int minute, int second, int millisecond)
    {
        DateComponents date;
        date.setMillisecondsSinceMidnight(hour * msPerHour + minute * msPerMinute + second * msPerSecond + millisecond);
        return date;
    }

    double msForDate(int year, int month, int day)
    {
        return dateToDaysFrom1970(year, month, day) * msPerDay;
    }

    String formatDate(const String& localeString, int year, int month, int day)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->formatDateTime(dateComponents(year, month, day));
    }

    String formatTime(const String& localeString, int hour, int minute, int second, int millisecond, bool useShortFormat)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->formatDateTime(timeComponents(hour, minute, second, millisecond), (useShortFormat ? Localizer::FormatTypeShort : Localizer::FormatTypeMedium));
    }

    double parseDate(const String& localeString, const String& dateString)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->parseDateTime(dateString, DateComponents::Date);
    }

#if ENABLE(CALENDAR_PICKER)
    String dateFormatText(const String& localeString)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->dateFormatText();
    }

    unsigned firstDayOfWeek(const String& localeString)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->firstDayOfWeek();
    }

    String monthLabel(const String& localeString, unsigned index)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->monthLabels()[index];
    }

    String weekDayShortLabel(const String& localeString, unsigned index)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->weekDayShortLabels()[index];
    }

    bool isRTL(const String& localeString)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->isRTL();
    }
#endif

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    String monthFormat(const String& localeString)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->monthFormat();
    }

    String timeFormat(const String& localeString)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->timeFormat();
    }

    String shortTimeFormat(const String& localeString)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->shortTimeFormat();
    }

    String shortMonthLabel(const String& localeString, unsigned index)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->shortMonthLabels()[index];
    }

    String shortStandAloneMonthLabel(const String& localeString, unsigned index)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->shortStandAloneMonthLabels()[index];
    }

    String timeAMPMLabel(const String& localeString, unsigned index)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->timeAMPMLabels()[index];
    }

    String decimalSeparator(const String& localeString)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->localizedDecimalSeparator();
    }
#endif
};

TEST_F(LocaleMacTest, formatDate)
{
    EXPECT_STREQ("04/27/2005", formatDate("en_US", 2005, April, 27).utf8().data());
    EXPECT_STREQ("27/04/2005", formatDate("fr_FR", 2005, April, 27).utf8().data());
    // Do not test ja_JP locale. OS X 10.8 and 10.7 have different formats.
}

TEST_F(LocaleMacTest, formatTime)
{
    EXPECT_STREQ("1:23 PM", formatTime("en_US", 13, 23, 00, 000, true).utf8().data());
    EXPECT_STREQ("13:23", formatTime("fr_FR", 13, 23, 00, 000, true).utf8().data());
    EXPECT_STREQ("13:23", formatTime("ja_JP", 13, 23, 00, 000, true).utf8().data());
    EXPECT_STREQ("\xD9\xA1:\xD9\xA2\xD9\xA3 \xD9\x85", formatTime("ar", 13, 23, 00, 000, true).utf8().data());
    EXPECT_STREQ("\xDB\xB1\xDB\xB3:\xDB\xB2\xDB\xB3", formatTime("fa", 13, 23, 00, 000, true).utf8().data());

    EXPECT_STREQ("12:00 AM", formatTime("en_US", 00, 00, 00, 000, true).utf8().data());
    EXPECT_STREQ("00:00", formatTime("fr_FR", 00, 00, 00, 000, true).utf8().data());
    EXPECT_STREQ("0:00", formatTime("ja_JP", 00, 00, 00, 000, true).utf8().data());
    EXPECT_STREQ("\xD9\xA1\xD9\xA2:\xD9\xA0\xD9\xA0 \xD8\xB5", formatTime("ar", 00, 00, 00, 000, true).utf8().data());
    EXPECT_STREQ("\xDB\xB0:\xDB\xB0\xDB\xB0", formatTime("fa", 00, 00, 00, 000, true).utf8().data());

    EXPECT_STREQ("7:07:07.007 AM", formatTime("en_US", 07, 07, 07, 007, false).utf8().data());
    EXPECT_STREQ("07:07:07,007", formatTime("fr_FR", 07, 07, 07, 007, false).utf8().data());
    EXPECT_STREQ("7:07:07.007", formatTime("ja_JP", 07, 07, 07, 007, false).utf8().data());
    EXPECT_STREQ("\xD9\xA7:\xD9\xA0\xD9\xA7:\xD9\xA0\xD9\xA7\xD9\xAB\xD9\xA0\xD9\xA0\xD9\xA7 \xD8\xB5", formatTime("ar", 07, 07, 07, 007, false).utf8().data());
    EXPECT_STREQ("\xDB\xB7:\xDB\xB0\xDB\xB7:\xDB\xB0\xDB\xB7\xD9\xAB\xDB\xB0\xDB\xB0\xDB\xB7", formatTime("fa", 07, 07, 07, 007, false).utf8().data());
}

TEST_F(LocaleMacTest, parseDate)
{
    EXPECT_EQ(msForDate(2005, April, 27), parseDate("en_US", "April 27, 2005"));
    EXPECT_EQ(msForDate(2005, April, 27), parseDate("fr_FR", "27 avril 2005"));
    EXPECT_EQ(msForDate(2005, April, 27), parseDate("ja_JP", "2005/04/27"));
}

#if ENABLE(CALENDAR_PICKER)
TEST_F(LocaleMacTest, dateFormatText)
{
    EXPECT_STREQ("Month/Day/Year", dateFormatText("en_US").utf8().data());
    EXPECT_STREQ("Day/Month/Year", dateFormatText("fr_FR").utf8().data());
    EXPECT_STREQ("Year/Month/Day", dateFormatText("ja_JP").utf8().data());
}

TEST_F(LocaleMacTest, firstDayOfWeek)
{
    EXPECT_EQ(Sunday, firstDayOfWeek("en_US"));
    EXPECT_EQ(Monday, firstDayOfWeek("fr_FR"));
    EXPECT_EQ(Sunday, firstDayOfWeek("ja_JP"));
}

TEST_F(LocaleMacTest, monthLabels)
{
    EXPECT_STREQ("January", monthLabel("en_US", January).utf8().data());
    EXPECT_STREQ("June", monthLabel("en_US", June).utf8().data());
    EXPECT_STREQ("December", monthLabel("en_US", December).utf8().data());

    EXPECT_STREQ("janvier", monthLabel("fr_FR", January).utf8().data());
    EXPECT_STREQ("juin", monthLabel("fr_FR", June).utf8().data());
    EXPECT_STREQ("d\xC3\xA9" "cembre", monthLabel("fr_FR", December).utf8().data());

    EXPECT_STREQ("1\xE6\x9C\x88", monthLabel("ja_JP", January).utf8().data());
    EXPECT_STREQ("6\xE6\x9C\x88", monthLabel("ja_JP", June).utf8().data());
    EXPECT_STREQ("12\xE6\x9C\x88", monthLabel("ja_JP", December).utf8().data());
}

TEST_F(LocaleMacTest, weekDayShortLabels)
{
    EXPECT_STREQ("Sun", weekDayShortLabel("en_US", Sunday).utf8().data());
    EXPECT_STREQ("Wed", weekDayShortLabel("en_US", Wednesday).utf8().data());
    EXPECT_STREQ("Sat", weekDayShortLabel("en_US", Saturday).utf8().data());

    EXPECT_STREQ("dim.", weekDayShortLabel("fr_FR", Sunday).utf8().data());
    EXPECT_STREQ("mer.", weekDayShortLabel("fr_FR", Wednesday).utf8().data());
    EXPECT_STREQ("sam.", weekDayShortLabel("fr_FR", Saturday).utf8().data());

    EXPECT_STREQ("\xE6\x97\xA5", weekDayShortLabel("ja_JP", Sunday).utf8().data());
    EXPECT_STREQ("\xE6\xB0\xB4", weekDayShortLabel("ja_JP", Wednesday).utf8().data());
    EXPECT_STREQ("\xE5\x9C\x9F", weekDayShortLabel("ja_JP", Saturday).utf8().data());
}

TEST_F(LocaleMacTest, isRTL)
{
    EXPECT_TRUE(isRTL("ar-eg"));
    EXPECT_FALSE(isRTL("en-us"));
    EXPECT_FALSE(isRTL("ja-jp"));
    EXPECT_FALSE(isRTL("**invalid**"));
}
#endif

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
TEST_F(LocaleMacTest, monthFormat)
{
    EXPECT_STREQ("MMM yyyy", monthFormat("en_US").utf8().data());
    EXPECT_STREQ("yyyy\xE5\xB9\xB4M\xE6\x9C\x88", monthFormat("ja_JP").utf8().data());

    // fr_FR and ru return different results on OS versions.
    //  "MMM yyyy" "LLL yyyy" on 10.6 and 10.7
    //  "MMM y" "LLL y" on 10.8
}

TEST_F(LocaleMacTest, timeFormat)
{
    EXPECT_STREQ("h:mm:ss a", timeFormat("en_US").utf8().data());
    EXPECT_STREQ("HH:mm:ss", timeFormat("fr_FR").utf8().data());
    EXPECT_STREQ("H:mm:ss", timeFormat("ja_JP").utf8().data());
}

TEST_F(LocaleMacTest, shortTimeFormat)
{
    EXPECT_STREQ("h:mm a", shortTimeFormat("en_US").utf8().data());
    EXPECT_STREQ("HH:mm", shortTimeFormat("fr_FR").utf8().data());
    EXPECT_STREQ("H:mm", shortTimeFormat("ja_JP").utf8().data());
}

TEST_F(LocaleMacTest, shortMonthLabels)
{
    EXPECT_STREQ("Jan", shortMonthLabel("en_US", 0).utf8().data());
    EXPECT_STREQ("Jan", shortStandAloneMonthLabel("en_US", 0).utf8().data());
    EXPECT_STREQ("Dec", shortMonthLabel("en_US", 11).utf8().data());
    EXPECT_STREQ("Dec", shortStandAloneMonthLabel("en_US", 11).utf8().data());

    EXPECT_STREQ("janv.", shortMonthLabel("fr_FR", 0).utf8().data());
    EXPECT_STREQ("janv.", shortStandAloneMonthLabel("fr_FR", 0).utf8().data());
    EXPECT_STREQ("d\xC3\xA9" "c.", shortMonthLabel("fr_FR", 11).utf8().data());
    EXPECT_STREQ("d\xC3\xA9" "c.", shortStandAloneMonthLabel("fr_FR", 11).utf8().data());

    EXPECT_STREQ("1\xE6\x9C\x88", shortMonthLabel("ja_JP", 0).utf8().data());
    EXPECT_STREQ("1\xE6\x9C\x88", shortStandAloneMonthLabel("ja_JP", 0).utf8().data());
    EXPECT_STREQ("12\xE6\x9C\x88", shortMonthLabel("ja_JP", 11).utf8().data());
    EXPECT_STREQ("12\xE6\x9C\x88", shortStandAloneMonthLabel("ja_JP", 11).utf8().data());

    EXPECT_STREQ("\xD0\xBC\xD0\xB0\xD1\x80\xD1\x82\xD0\xB0", shortMonthLabel("ru_RU", 2).utf8().data());
    EXPECT_STREQ("\xD0\xBC\xD0\xB0\xD1\x8F", shortMonthLabel("ru_RU", 4).utf8().data());
    // The ru_RU locale returns different stand-alone month labels on OS versions.
    //  "\xD0\xBC\xD0\xB0\xD1\x80\xD1\x82" "\xD0\xBC\xD0\xB0\xD0\xB9" on 10.6 and 10.7
    //  "\xD0\x9C\xD0\xB0\xD1\x80\xD1\x82" "\xD0\x9C\xD0\xB0\xD0\xB9" on 10.8
}

TEST_F(LocaleMacTest, timeAMPMLabels)
{
    EXPECT_STREQ("AM", timeAMPMLabel("en_US", 0).utf8().data());
    EXPECT_STREQ("PM", timeAMPMLabel("en_US", 1).utf8().data());

    EXPECT_STREQ("AM", timeAMPMLabel("fr_FR", 0).utf8().data());
    EXPECT_STREQ("PM", timeAMPMLabel("fr_FR", 1).utf8().data());

    EXPECT_STREQ("\xE5\x8D\x88\xE5\x89\x8D", timeAMPMLabel("ja_JP", 0).utf8().data());
    EXPECT_STREQ("\xE5\x8D\x88\xE5\xBE\x8C", timeAMPMLabel("ja_JP", 1).utf8().data());
}

TEST_F(LocaleMacTest, decimalSeparator)
{
    EXPECT_STREQ(".", decimalSeparator("en_US").utf8().data());
    EXPECT_STREQ(",", decimalSeparator("fr_FR").utf8().data());
}
#endif

TEST_F(LocaleMacTest, invalidLocale)
{
    EXPECT_STREQ(monthLabel("en_US", January).utf8().data(), monthLabel("foo", January).utf8().data());
    EXPECT_STREQ(decimalSeparator("en_US").utf8().data(), decimalSeparator("foo").utf8().data());
}

static void testNumberIsReversible(const AtomicString& localeString, const char* original, const char* shouldHave = 0)
{
    OwnPtr<Localizer> locale = Localizer::create(localeString);
    String localized = locale->convertToLocalizedNumber(original);
    if (shouldHave)
        EXPECT_TRUE(localized.contains(shouldHave));
    String converted = locale->convertFromLocalizedNumber(localized);
    EXPECT_STREQ(original, converted.utf8().data());
}

void testNumbers(const AtomicString& localeString, const char* decimalSeparatorShouldBe = 0)
{
    testNumberIsReversible(localeString, "123456789012345678901234567890");
    testNumberIsReversible(localeString, "-123.456", decimalSeparatorShouldBe);
    testNumberIsReversible(localeString, ".456", decimalSeparatorShouldBe);
    testNumberIsReversible(localeString, "-0.456", decimalSeparatorShouldBe);
}

TEST_F(LocaleMacTest, localizedNumberRoundTrip)
{
    // Test some of major locales.
    testNumbers("en_US", ".");
    testNumbers("fr_FR", ",");
    testNumbers("ar");
    testNumbers("de_DE");
    testNumbers("es_ES");
    testNumbers("fa");
    testNumbers("ja_JP");
    testNumbers("ko_KR");
    testNumbers("zh_CN");
    testNumbers("zh_HK");
    testNumbers("zh_TW");
}
