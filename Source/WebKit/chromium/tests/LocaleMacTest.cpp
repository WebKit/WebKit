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

    double msForDate(int year, int month, int day)
    {
        return dateToDaysFrom1970(year, month, day) * msPerDay;
    }

    String formatDate(const String& localeString, int year, int month, int day)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->formatDate(dateComponents(year, month, day));
    }

    double parseDate(const String& localeString, const String& dateString)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->parseDate(dateString);
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
#endif

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
    String timeFormatText(const String& localeString)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->timeFormatText();
    }

    String shortTimeFormatText(const String& localeString)
    {
        OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
        return locale->shortTimeFormatText();
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
    EXPECT_STREQ("4/27/05", formatDate("en_US", 2005, April, 27).utf8().data());
    EXPECT_STREQ("27/04/05", formatDate("fr_FR", 2005, April, 27).utf8().data());
    EXPECT_STREQ("05/04/27", formatDate("ja_JP", 2005, April, 27).utf8().data());
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
#endif

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
TEST_F(LocaleMacTest, timeFormatText)
{
    EXPECT_STREQ("h:mm:ss a", timeFormatText("en_US").utf8().data());
    EXPECT_STREQ("HH:mm:ss", timeFormatText("fr_FR").utf8().data());
    EXPECT_STREQ("H:mm:ss", timeFormatText("ja_JP").utf8().data());
}

TEST_F(LocaleMacTest, shortTimeFormatText)
{
    EXPECT_STREQ("h:mm a", shortTimeFormatText("en_US").utf8().data());
    EXPECT_STREQ("HH:mm", shortTimeFormatText("fr_FR").utf8().data());
    EXPECT_STREQ("H:mm", shortTimeFormatText("ja_JP").utf8().data());
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

static void testNumberIsReversible(const String& localeString, const char* original, const char* shouldHave = 0)
{
    OwnPtr<LocaleMac> locale = LocaleMac::create(localeString);
    String localized = locale->convertToLocalizedNumber(original);
    if (shouldHave)
        EXPECT_TRUE(localized.contains(shouldHave));
    String converted = locale->convertFromLocalizedNumber(localized);
    EXPECT_STREQ(original, converted.utf8().data());
}

void testNumbers(const String& localeString, const char* decimalSeparatorShouldBe = 0)
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
