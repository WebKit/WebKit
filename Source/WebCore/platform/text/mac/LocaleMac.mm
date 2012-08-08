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
#include "LocaleMac.h"

#import <Foundation/NSDateFormatter.h>
#import <Foundation/NSLocale.h>
#include "Language.h"
#include "LocalizedDate.h"
#include "LocalizedStrings.h"
#include <wtf/DateMath.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/StringBuilder.h>

using namespace std;

namespace WebCore {

static NSDateFormatter* createDateTimeFormatter(NSLocale* locale, NSDateFormatterStyle dateStyle, NSDateFormatterStyle timeStyle)
{
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setLocale:locale];
    [formatter setDateStyle:dateStyle];
    [formatter setTimeStyle:timeStyle];
    [formatter setTimeZone:[NSTimeZone timeZoneWithAbbreviation:@"UTC"]];
    [formatter setCalendar:[[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar]];
    return formatter;
}

LocaleMac::LocaleMac(NSLocale* locale)
    : m_locale(locale)
    , m_didInitializeNumberData(false)
{
}

LocaleMac::LocaleMac(const String& localeIdentifier)
    : m_locale([[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier])
    , m_didInitializeNumberData(false)
{
}

LocaleMac::~LocaleMac()
{
}

PassOwnPtr<LocaleMac> LocaleMac::create(const String& localeIdentifier)
{
    return adoptPtr(new LocaleMac(localeIdentifier));
}

static inline String languageFromLocale(const String& locale)
{
    String normalizedLocale = locale;
    normalizedLocale.replace('-', '_');
    size_t separatorPosition = normalizedLocale.find('_');
    if (separatorPosition == notFound)
        return normalizedLocale;
    return normalizedLocale.left(separatorPosition);
}

static NSLocale* determineLocale()
{
    NSLocale* currentLocale = [NSLocale currentLocale];
    String currentLocaleLanguage = languageFromLocale(String([currentLocale localeIdentifier]));
    String browserLanguage = languageFromLocale(defaultLanguage());
    if (equalIgnoringCase(currentLocaleLanguage, browserLanguage))
        return currentLocale;
    // It seems initWithLocaleIdentifier accepts dash-separated locale identifier.
    return [[NSLocale alloc] initWithLocaleIdentifier:defaultLanguage()];
}

LocaleMac* LocaleMac::currentLocale()
{
    static LocaleMac* currentLocale = new LocaleMac(determineLocale());
    return currentLocale;
}

NSDateFormatter* LocaleMac::createShortDateFormatter()
{
    return createDateTimeFormatter(m_locale.get(), NSDateFormatterShortStyle, NSDateFormatterNoStyle);
}

double LocaleMac::parseDate(const String& input)
{
    RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortDateFormatter());
    NSDate *date = [formatter.get() dateFromString:input];
    if (!date)
        return std::numeric_limits<double>::quiet_NaN();
    return [date timeIntervalSince1970] * msPerSecond;
}

String LocaleMac::formatDate(const DateComponents& dateComponents)
{
    RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortDateFormatter());
    NSTimeInterval interval = dateComponents.millisecondsSinceEpoch() / msPerSecond;
    return String([formatter.get() stringFromDate:[NSDate dateWithTimeIntervalSince1970:interval]]);
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

String LocaleMac::dateFormatText()
{
    if (!m_localizedDateFormatText.isEmpty())
        return m_localizedDateFormatText;
    RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortDateFormatter());
    m_localizedDateFormatText = localizeDateFormat(String([formatter.get() dateFormat]));
    return  m_localizedDateFormatText;
}

const Vector<String>& LocaleMac::monthLabels()
{
    if (!m_monthLabels.isEmpty())
        return m_monthLabels;
    m_monthLabels.reserveCapacity(12);
    RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortDateFormatter());
    NSArray *array = [formatter.get() monthSymbols];
    if ([array count] == 12) {
        for (unsigned i = 0; i < 12; ++i)
            m_monthLabels.append(String([array objectAtIndex:i]));
        return m_monthLabels;
    }
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(WTF::monthFullName); ++i)
        m_monthLabels.append(WTF::monthFullName[i]);
    return m_monthLabels;
}

const Vector<String>& LocaleMac::weekDayShortLabels()
{
    if (!m_weekDayShortLabels.isEmpty())
        return m_weekDayShortLabels;
    m_weekDayShortLabels.reserveCapacity(7);
    RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortDateFormatter());
    NSArray *array = [formatter.get() shortWeekdaySymbols];
    if ([array count] == 7) {
        for (unsigned i = 0; i < 7; ++i)
            m_weekDayShortLabels.append(String([array objectAtIndex:i]));
        return m_weekDayShortLabels;
    }
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(WTF::weekdayName); ++i) {
        // weekdayName starts with Monday.
        m_weekDayShortLabels.append(WTF::weekdayName[(i + 6) % 7]);
    }
    return m_weekDayShortLabels;
}

unsigned LocaleMac::firstDayOfWeek()
{
    RetainPtr<NSCalendar> calendar(AdoptNS, [[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar]);
    [calendar.get() setLocale:m_locale.get()];
    // The document for NSCalendar - firstWeekday doesn't have an explanation of
    // firstWeekday value. We can guess it by the document of NSDateComponents -
    // weekDay, so it can be 1 through 7 and 1 is Sunday.
    return [calendar.get() firstWeekday] - 1;
}
#endif

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
NSDateFormatter* LocaleMac::createTimeFormatter()
{
    return createDateTimeFormatter(m_locale.get(), NSDateFormatterNoStyle, NSDateFormatterMediumStyle);
}

NSDateFormatter* LocaleMac::createShortTimeFormatter()
{
    return createDateTimeFormatter(m_locale.get(), NSDateFormatterNoStyle, NSDateFormatterShortStyle);
}

String LocaleMac::timeFormatText()
{
    if (!m_localizedTimeFormatText.isEmpty())
        return m_localizedTimeFormatText;
    RetainPtr<NSDateFormatter> formatter(AdoptNS, createTimeFormatter());
    m_localizedTimeFormatText = String([formatter.get() dateFormat]);
    return m_localizedTimeFormatText;
}

String LocaleMac::shortTimeFormatText()
{
    if (!m_localizedShortTimeFormatText.isEmpty())
        return m_localizedShortTimeFormatText;
    RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortTimeFormatter());
    m_localizedShortTimeFormatText = String([formatter.get() dateFormat]);
    return m_localizedShortTimeFormatText;
}

const Vector<String>& LocaleMac::timeAMPMLabels()
{
    if (!m_timeAMPMLabels.isEmpty())
        return m_timeAMPMLabels;
    m_timeAMPMLabels.reserveCapacity(2);
    RetainPtr<NSDateFormatter> formatter(AdoptNS, createShortTimeFormatter());
    m_timeAMPMLabels.append(String([formatter.get() AMSymbol]));
    m_timeAMPMLabels.append(String([formatter.get() PMSymbol]));
    return m_timeAMPMLabels;
}
#endif

void LocaleMac::initializeNumberLocalizerData()
{
    if (m_didInitializeNumberData)
        return;
    m_didInitializeNumberData = true;

    RetainPtr<NSNumberFormatter> formatter(AdoptNS, [[NSNumberFormatter alloc] init]);
    [formatter.get() setLocale:m_locale.get()];
    [formatter.get() setNumberStyle:NSNumberFormatterDecimalStyle];
    [formatter.get() setUsesGroupingSeparator:NO];

    RetainPtr<NSNumber> sampleNumber(AdoptNS, [[NSNumber alloc] initWithDouble:9876543210]);
    String nineToZero([formatter.get() stringFromNumber:sampleNumber.get()]);
    if (nineToZero.length() != 10)
        return;
    Vector<String, DecimalSymbolsSize> symbols;
    for (unsigned i = 0; i < 10; ++i)
        symbols.append(nineToZero.substring(9 - i, 1));
    ASSERT(symbols.size() == DecimalSeparatorIndex);
    symbols.append(String([formatter.get() decimalSeparator]));
    ASSERT(symbols.size() == GroupSeparatorIndex);
    symbols.append(String([formatter.get() groupingSeparator]));
    ASSERT(symbols.size() == DecimalSymbolsSize);

    String positivePrefix([formatter.get() positivePrefix]);
    String positiveSuffix([formatter.get() positiveSuffix]);
    String negativePrefix([formatter.get() negativePrefix]);
    String negativeSuffix([formatter.get() negativeSuffix]);
    setNumberLocalizerData(symbols, positivePrefix, positiveSuffix, negativePrefix, negativeSuffix);
}

}
