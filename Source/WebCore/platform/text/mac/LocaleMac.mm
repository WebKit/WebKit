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
#include "LocalizedDate.h"

#import <Foundation/NSDateFormatter.h>
#include "LocalizedStrings.h"
#include <limits>
#include <wtf/DateMath.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/StringBuilder.h>

using namespace std;

namespace WebCore {

static NSDateFormatter *createShortDateFormatter()
{
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateStyle:NSDateFormatterShortStyle];
    [formatter setTimeStyle:NSDateFormatterNoStyle];
    [formatter setTimeZone:[NSTimeZone timeZoneWithAbbreviation:@"UTC"]];
    [formatter setCalendar:[[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar]];
    return formatter;
}

double parseLocalizedDate(const String& input, DateComponents::Type type)
{
    switch (type) {
    case DateComponents::Date: {
        RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortDateFormatter());
        NSDate *date = [formatter.get() dateFromString:input];
        if (!date)
            break;
        return [date timeIntervalSince1970] * msPerSecond;
    }
    case DateComponents::DateTime:
    case DateComponents::DateTimeLocal:
    case DateComponents::Month:
    case DateComponents::Time:
    case DateComponents::Week:
    case DateComponents::Invalid:
        break;
    }
    return numeric_limits<double>::quiet_NaN();
}

String formatLocalizedDate(const DateComponents& dateComponents)
{
    switch (dateComponents.type()) {
    case DateComponents::Date: {
        RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortDateFormatter());
        NSTimeInterval interval = dateComponents.millisecondsSinceEpoch() / msPerSecond;
        return String([formatter.get() stringFromDate:[NSDate dateWithTimeIntervalSince1970:interval]]);
    }
    case DateComponents::DateTime:
    case DateComponents::DateTimeLocal:
    case DateComponents::Month:
    case DateComponents::Time:
    case DateComponents::Week:
    case DateComponents::Invalid:
        break;
    }
    return String();
}

#if ENABLE(CALENDAR_PICKER)
static bool isYearSymbol(UChar letter) { return letter == 'y' || letter == 'Y' || letter == 'u'; }
static bool isMonthSymbol(UChar letter) { return letter == 'M' || letter == 'L'; }
static bool isDaySymbol(UChar letter) { return letter == 'd'; }

// http://unicode.org/reports/tr35/tr35-6.html#Date_Format_Patterns
static String localizeDateFormat(const String& format)
{
    String yearText = dateFormatYearText().isEmpty() ? "Year" : dateFormatYearText();
    String monthText = dateFormatMonthText().isEmpty() ? "Month" : dateFormatMonthText();
    String dayText = dateFormatDayInMonthText().isEmpty() ? "Day" : dateFormatDayInMonthText();
    StringBuilder buffer;
    bool inQuote = false;
    for (unsigned i = 0; i < format.length(); ++i) {
        UChar ch = format[i];
        if (inQuote) {
            if (ch == '\'') {
                inQuote = false;
                ASSERT(i);
                if (format[i - 1] == '\'')
                    buffer.append('\'');
            } else
                buffer.append(ch);
            continue;
        }

        if (ch == '\'') {
            inQuote = true;
            if (i > 0 && format[i - 1] == '\'')
                buffer.append(ch);
        } else if (isYearSymbol(ch)) {
            if (i > 0 && format[i - 1] == ch)
                continue;
            buffer.append(yearText);
        } else if (isMonthSymbol(ch)) {
            if (i > 0 && format[i - 1] == ch)
                continue;
            buffer.append(monthText);
        } else if (isDaySymbol(ch)) {
            if (i > 0 && format[i - 1] == ch)
                continue;
            buffer.append(dayText);
        } else
            buffer.append(ch);
    }
    return buffer.toString();
}

String localizedDateFormatText()
{
    DEFINE_STATIC_LOCAL(String, text, ());
    if (!text.isEmpty())
        return text;
    RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortDateFormatter());
    text = localizeDateFormat(String([formatter.get() dateFormat]));
    return text;
}

const Vector<String>& monthLabels()
{
    DEFINE_STATIC_LOCAL(Vector<String>, labels, ());
    if (!labels.isEmpty())
        return labels;
    labels.reserveCapacity(12);
    RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortDateFormatter());
    NSArray *array = [formatter.get() monthSymbols];
    if ([array count] == 12) {
        for (unsigned i = 0; i < 12; ++i)
            labels.append(String([array objectAtIndex:i]));
        return labels;
    }
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(WTF::monthFullName); ++i)
        labels.append(WTF::monthFullName[i]);
    return labels;
}

const Vector<String>& weekDayShortLabels()
{
    DEFINE_STATIC_LOCAL(Vector<String>, labels, ());
    if (!labels.isEmpty())
        return labels;
    labels.reserveCapacity(7);
    RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortDateFormatter());
    NSArray *array = [formatter.get() shortWeekdaySymbols];
    if ([array count] == 7) {
        for (unsigned i = 0; i < 7; ++i)
            labels.append(String([array objectAtIndex:i]));
        return labels;
    }
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(WTF::weekdayName); ++i) {
        // weekdayName starts with Monday.
        labels.append(WTF::weekdayName[(i + 6) % 7]);
    }
    return labels;
}

unsigned firstDayOfWeek()
{
    RetainPtr<NSCalendar> calendar(AdoptNS, [[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar]);
    // The document for NSCalendar - firstWeekday doesn't have an explanation of
    // firstWeekday value. We can guess it by the document of NSDateComponents -
    // weekDay, so it can be 1 through 7 and 1 is Sunday.
    return [calendar.get() firstWeekday] - 1;
}
#endif

}
