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
#include "LocalizedStrings.h"
#include <limits>
#include <windows.h>
#include <wtf/CurrentTime.h>
#include <wtf/DateMath.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringBuilder.h>

using namespace std;

namespace WebCore {

// Windows doesn't have an API to parse locale-specific date string,
// and GetDateFormat() and GetDateFormatEx() don't support years older
// than 1600, which we should support according to the HTML
// standard. So, we obtain the date format from the system, but we
// format/parse a date by our own code.

inline LocaleWin::LocaleWin(LCID lcid)
    : m_lcid(lcid)
{
    struct tm tm;
    getCurrentLocalTime(&tm);
    m_baseYear = tm.tm_year + 1900;

#if ENABLE(CALENDAR_PICKER)
    DWORD value = 0;
    ::GetLocaleInfo(m_lcid, LOCALE_IFIRSTDAYOFWEEK | LOCALE_RETURN_NUMBER, reinterpret_cast<LPWSTR>(&value), sizeof(value) / sizeof(TCHAR));
    // 0:Monday, ..., 6:Sunday.
    // We need 1 for Monday, 0 for Sunday.
    m_firstDayOfWeek = (value + 1) % 7;
#endif
}

PassOwnPtr<LocaleWin> LocaleWin::create(LCID lcid)
{
    return adoptPtr(new LocaleWin(lcid));
}

LocaleWin* LocaleWin::currentLocale()
{
    // Ideally we should make LCID from defaultLanguage(). But
    // ::LocaleNameToLCID() is available since Windows Vista.
    static LocaleWin* currentLocale = LocaleWin::create(LOCALE_USER_DEFAULT).leakPtr();
    return currentLocale;
}

LocaleWin::~LocaleWin()
{
}

String LocaleWin::getLocaleInfoString(LCTYPE type)
{
    int bufferSizeWithNUL = ::GetLocaleInfo(m_lcid, type, 0, 0);
    if (bufferSizeWithNUL <= 0)
        return String();
    Vector<UChar> buffer(bufferSizeWithNUL);
    ::GetLocaleInfo(m_lcid, type, buffer.data(), bufferSizeWithNUL);
    buffer.shrink(bufferSizeWithNUL - 1);
    return String::adopt(buffer);
}

void LocaleWin::ensureShortMonthLabels()
{
    if (!m_shortMonthLabels.isEmpty())
        return;
    const LCTYPE types[12] = {
        LOCALE_SABBREVMONTHNAME1,
        LOCALE_SABBREVMONTHNAME2,
        LOCALE_SABBREVMONTHNAME3,
        LOCALE_SABBREVMONTHNAME4,
        LOCALE_SABBREVMONTHNAME5,
        LOCALE_SABBREVMONTHNAME6,
        LOCALE_SABBREVMONTHNAME7,
        LOCALE_SABBREVMONTHNAME8,
        LOCALE_SABBREVMONTHNAME9,
        LOCALE_SABBREVMONTHNAME10,
        LOCALE_SABBREVMONTHNAME11,
        LOCALE_SABBREVMONTHNAME12,
    };
    m_shortMonthLabels.reserveCapacity(WTF_ARRAY_LENGTH(types));
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(types); ++i) {
        m_shortMonthLabels.append(getLocaleInfoString(types[i]));
        if (m_shortMonthLabels.last().isEmpty()) {
            m_shortMonthLabels.shrink(0);
            m_shortMonthLabels.reserveCapacity(WTF_ARRAY_LENGTH(WTF::monthName));
            for (unsigned m = 0; m < WTF_ARRAY_LENGTH(WTF::monthName); ++m)
                m_shortMonthLabels.append(WTF::monthName[m]);
            return;
        }
    }
}

// -------------------------------- Tokenized date format

struct DateFormatToken {
    enum Type {
        Literal,
        Day1,
        Day2,
        Month1,
        Month2,
        Month3,
        Month4,
        Year1,
        Year2,
        Year4,
    };
    Type type;
    String data; // This is valid only if type==Literal.

    DateFormatToken(Type type)
        : type(type)
    { }

    DateFormatToken(const String& data)
        : type(Literal)
        , data(data)
    { }

    DateFormatToken(const DateFormatToken& token)
        : type(token.type)
        , data(token.data)
    { }
};

static inline bool isEraSymbol(UChar letter) { return letter == 'g'; }
static inline bool isYearSymbol(UChar letter) { return letter == 'y'; }
static inline bool isMonthSymbol(UChar letter) { return letter == 'M'; }
static inline bool isDaySymbol(UChar letter) { return letter == 'd'; }

static unsigned countContinuousLetters(const String& format, unsigned index)
{
    unsigned count = 1;
    UChar reference = format[index];
    while (index + 1 < format.length()) {
        if (format[++index] != reference)
            break;
        ++count;
    }
    return count;
}

static void commitLiteralToken(StringBuilder& literalBuffer, Vector<DateFormatToken>& tokens)
{
    if (literalBuffer.length() <= 0)
        return;
    tokens.append(DateFormatToken(literalBuffer.toString()));
    literalBuffer.clear();
}

// See http://msdn.microsoft.com/en-us/library/dd317787(v=vs.85).aspx
static Vector<DateFormatToken> parseDateFormat(const String format)
{
    Vector<DateFormatToken> tokens;
    StringBuilder literalBuffer;
    bool inQuote = false;
    for (unsigned i = 0; i < format.length(); ++i) {
        UChar ch = format[i];
        if (inQuote) {
            if (ch == '\'') {
                inQuote = false;
                ASSERT(i);
                if (format[i - 1] == '\'')
                    literalBuffer.append('\'');
            } else
                literalBuffer.append(ch);
            continue;
        }

        if (ch == '\'') {
            inQuote = true;
            if (i > 0 && format[i - 1] == '\'')
                literalBuffer.append(ch);
        } else if (isYearSymbol(ch)) {
            commitLiteralToken(literalBuffer, tokens);
            unsigned count = countContinuousLetters(format, i);
            i += count - 1;
            if (count == 1)
                tokens.append(DateFormatToken(DateFormatToken::Year1));
            else if (count == 2)
                tokens.append(DateFormatToken(DateFormatToken::Year2));
            else
                tokens.append(DateFormatToken(DateFormatToken::Year4));
        } else if (isMonthSymbol(ch)) {
            commitLiteralToken(literalBuffer, tokens);
            unsigned count = countContinuousLetters(format, i);
            i += count - 1;
            if (count == 1)
                tokens.append(DateFormatToken(DateFormatToken::Month1));
            else if (count == 2)
                tokens.append(DateFormatToken(DateFormatToken::Month2));
            else if (count == 3)
                tokens.append(DateFormatToken(DateFormatToken::Month3));
            else
                tokens.append(DateFormatToken(DateFormatToken::Month4));
        } else if (isDaySymbol(ch)) {
            commitLiteralToken(literalBuffer, tokens);
            unsigned count = countContinuousLetters(format, i);
            i += count - 1;
            if (count == 1)
                tokens.append(DateFormatToken(DateFormatToken::Day1));
            else
                tokens.append(DateFormatToken(DateFormatToken::Day2));
        } else if (isEraSymbol(ch)) {
            // Just ignore era.
            // HTML5 date supports only A.D.
        } else
            literalBuffer.append(ch);
    }
    commitLiteralToken(literalBuffer, tokens);
    return tokens;
}

// -------------------------------- Parsing

// Returns -1 if parsing fails.
static int parseNumber(const String& input, unsigned& index)
{
    unsigned digitsStart = index;
    while (index < input.length() && isASCIIDigit(input[index]))
        index++;
    if (digitsStart == index)
        return -1;
    bool ok = false;
    int number = input.substring(digitsStart, index - digitsStart).toInt(&ok);
    return ok ? number : -1;
}

// Returns 0-based month number. Returns -1 if parsing fails.
int LocaleWin::parseNumberOrMonth(const String& input, unsigned& index)
{
    int result = parseNumber(input, index);
    if (result >= 0) {
        if (result < 1 || result > 12)
            return -1;
        return result - 1;
    }
    for (unsigned m = 0; m < m_monthLabels.size(); ++m) {
        unsigned labelLength = m_monthLabels[m].length();
        if (equalIgnoringCase(input.substring(index, labelLength), m_monthLabels[m])) {
            index += labelLength;
            return m;
        }
    }
    for (unsigned m = 0; m < m_shortMonthLabels.size(); ++m) {
        unsigned labelLength = m_shortMonthLabels[m].length();
        if (equalIgnoringCase(input.substring(index, labelLength), m_shortMonthLabels[m])) {
            index += labelLength;
            return m;
        }
    }
    return -1;
}

double LocaleWin::parseDate(const String& input)
{
    ensureShortDateTokens();
    return parseDate(m_shortDateTokens, m_baseYear, input);
}

double LocaleWin::parseDate(const String& format, int baseYear, const String& input)
{
    return parseDate(parseDateFormat(format), baseYear, input);
}

double LocaleWin::parseDate(const Vector<DateFormatToken>& tokens, int baseYear, const String& input)
{
    ensureShortMonthLabels();
    ensureMonthLabels();
    const double NaN = numeric_limits<double>::quiet_NaN();
    unsigned inputIndex = 0;
    int day = -1, month = -1, year = -1;
    for (unsigned i = 0; i < tokens.size(); ++i) {
        switch (tokens[i].type) {
        case DateFormatToken::Literal: {
            String data = tokens[i].data;
            unsigned literalLength = data.length();
            if (input.substring(inputIndex, literalLength) == data)
                inputIndex += literalLength;
            // Go ahead even if the input doesn't have this string.
            break;
        }
        case DateFormatToken::Day1:
        case DateFormatToken::Day2:
            day = parseNumber(input, inputIndex);
            if (day < 1 || day > 31)
                return NaN;
            break;
        case DateFormatToken::Month1:
        case DateFormatToken::Month2:
        case DateFormatToken::Month3:
        case DateFormatToken::Month4:
            month = parseNumberOrMonth(input, inputIndex);
            if (month < 0 || month > 11)
                return NaN;
            break;
        case DateFormatToken::Year1: {
            unsigned oldIndex = inputIndex;
            year = parseNumber(input, inputIndex);
            if (year <= 0)
                return NaN;
            if (inputIndex - oldIndex == 1) {
                int shortYear = baseYear % 10;
                int decade = baseYear - shortYear;
                if (shortYear >= 5)
                    year += shortYear - 4 <= year ? decade : decade + 10;
                else
                    year += shortYear + 5 >= year ? decade : decade - 10;
            }
            break;
        }
        case DateFormatToken::Year2: {
            unsigned oldIndex = inputIndex;
            year = parseNumber(input, inputIndex);
            if (year <= 0)
                return NaN;
            if (inputIndex - oldIndex == 2) {
                int shortYear = baseYear % 100;
                int century = baseYear - shortYear;
                if (shortYear >= 50)
                    year += shortYear - 49 <= year ? century : century + 100;
                else
                    year += shortYear + 50 >= year ? century : century - 100;
            }
            break;
        }
        case DateFormatToken::Year4:
            year = parseNumber(input, inputIndex);
            if (year <= 0)
                return NaN;
            break;
        }
    }
    if (year <= 0 || month < 0 || day <= 0)
        return NaN;
    return dateToDaysFrom1970(year, month, day) * msPerDay;
}

// -------------------------------- Formatting

static inline void appendNumber(int value, StringBuilder& buffer)
{
    buffer.append(String::number(value));
}

static void appendTwoDigitsNumber(int value, StringBuilder& buffer)
{
    if (value >= 0 && value < 10)
        buffer.append("0");
    buffer.append(String::number(value));
}

static void appendFourDigitsNumber(int value, StringBuilder& buffer)
{
    if (value < 0) {
        buffer.append(String::number(value));
        return;
    }
    if (value < 10)
        buffer.append("000");
    else if (value < 100)
        buffer.append("00");
    else if (value < 1000)
        buffer.append("0");
    buffer.append(String::number(value));
}

String LocaleWin::formatDate(const DateComponents& dateComponents)
{
    ensureShortDateTokens();
    return formatDate(m_shortDateTokens, m_baseYear, dateComponents.fullYear(), dateComponents.month(), dateComponents.monthDay());
}

String LocaleWin::formatDate(const String& format, int baseYear, int year, int month, int day)
{
    return formatDate(parseDateFormat(format), baseYear, year, month, day);
}

String LocaleWin::formatDate(const Vector<DateFormatToken>& tokens, int baseYear, int year, int month, int day)
{
    ensureShortMonthLabels();
    ensureMonthLabels();
    StringBuilder buffer;
    for (unsigned i = 0; i < tokens.size(); ++i) {
        switch (tokens[i].type) {
        case DateFormatToken::Literal:
            buffer.append(tokens[i].data);
            break;
        case DateFormatToken::Day1:
            appendNumber(day, buffer);
            break;
        case DateFormatToken::Day2:
            appendTwoDigitsNumber(day, buffer);
            break;
        case DateFormatToken::Month1:
            appendNumber(month + 1, buffer);
            break;
        case DateFormatToken::Month2:
            appendTwoDigitsNumber(month + 1, buffer);
            break;
        case DateFormatToken::Month3:
            if (0 <= month && month < static_cast<int>(m_shortMonthLabels.size()))
                buffer.append(m_shortMonthLabels[month]);
            else
                appendNumber(month + 1, buffer);
            break;
        case DateFormatToken::Month4:
            if (0 <= month && month < static_cast<int>(m_monthLabels.size()))
                buffer.append(m_monthLabels[month]);
            else
                appendNumber(month + 1, buffer);
            break;
        case DateFormatToken::Year1: {
            if (baseYear - 4 <= year && year <= baseYear + 5)
                appendNumber(year % 10, buffer);
            else
                appendFourDigitsNumber(year, buffer);
            break;
        }
        case DateFormatToken::Year2: {
            if (baseYear - 49 <= year && year <= baseYear + 50)
                appendTwoDigitsNumber(year % 100, buffer);
            else
                appendFourDigitsNumber(year, buffer);
            break;
        }
        case DateFormatToken::Year4:
            appendFourDigitsNumber(year, buffer);
            break;
        }
    }
    return buffer.toString();
}

void LocaleWin::ensureShortDateTokens()
{
    if (!m_shortDateTokens.isEmpty())
        return;
    m_shortDateTokens = parseDateFormat(getLocaleInfoString(LOCALE_SSHORTDATE));
}

static String substituteLabelsIntoFormat(const Vector<DateFormatToken>& tokens, const String& yearText, const String& monthText, const String& dayText)
{
    String nonEmptyDayText = dayText.isEmpty() ? "Day" : dayText;
    String nonEmptyMonthText = monthText.isEmpty() ? "Month" : monthText;
    String nonEmptyYearText = yearText.isEmpty() ? "Year" : yearText;
    StringBuilder buffer;
    for (unsigned i = 0; i < tokens.size(); ++i) {
        switch (tokens[i].type) {
        case DateFormatToken::Literal:
            buffer.append(tokens[i].data);
            break;
        case DateFormatToken::Day1:
        case DateFormatToken::Day2:
            buffer.append(nonEmptyDayText);
            break;
        case DateFormatToken::Month1:
        case DateFormatToken::Month2:
        case DateFormatToken::Month3:
        case DateFormatToken::Month4:
            buffer.append(nonEmptyMonthText);
            break;
        case DateFormatToken::Year1:
        case DateFormatToken::Year2:
        case DateFormatToken::Year4:
            buffer.append(nonEmptyYearText);
            break;
        }
    }
    return buffer.toString();
}

String LocaleWin::dateFormatText()
{
    ensureShortDateTokens();
    return substituteLabelsIntoFormat(m_shortDateTokens, dateFormatYearText(), dateFormatMonthText(), dateFormatDayInMonthText());
}

String LocaleWin::dateFormatText(const String& format, const String& yearText, const String& monthText, const String& dayText)
{
    return substituteLabelsIntoFormat(parseDateFormat(format), yearText, monthText, dayText);
}

void LocaleWin::ensureMonthLabels()
{
    if (!m_monthLabels.isEmpty())
        return;
    const LCTYPE types[12] = {
        LOCALE_SMONTHNAME1,
        LOCALE_SMONTHNAME2,
        LOCALE_SMONTHNAME3,
        LOCALE_SMONTHNAME4,
        LOCALE_SMONTHNAME5,
        LOCALE_SMONTHNAME6,
        LOCALE_SMONTHNAME7,
        LOCALE_SMONTHNAME8,
        LOCALE_SMONTHNAME9,
        LOCALE_SMONTHNAME10,
        LOCALE_SMONTHNAME11,
        LOCALE_SMONTHNAME12,
    };
    m_monthLabels.reserveCapacity(WTF_ARRAY_LENGTH(types));
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(types); ++i) {
        m_monthLabels.append(getLocaleInfoString(types[i]));
        if (m_monthLabels.last().isEmpty()) {
            m_monthLabels.shrink(0);
            m_monthLabels.reserveCapacity(WTF_ARRAY_LENGTH(WTF::monthFullName));
            for (unsigned m = 0; m < WTF_ARRAY_LENGTH(WTF::monthFullName); ++m)
                m_monthLabels.append(WTF::monthFullName[m]);
            return;
        }
    }
}

void LocaleWin::ensureWeekDayShortLabels()
{
    if (!m_weekDayShortLabels.isEmpty())
        return;
    const LCTYPE types[7] = {
        LOCALE_SABBREVDAYNAME7, // Sunday
        LOCALE_SABBREVDAYNAME1, // Monday
        LOCALE_SABBREVDAYNAME2,
        LOCALE_SABBREVDAYNAME3,
        LOCALE_SABBREVDAYNAME4,
        LOCALE_SABBREVDAYNAME5,
        LOCALE_SABBREVDAYNAME6
    };
    m_weekDayShortLabels.reserveCapacity(WTF_ARRAY_LENGTH(types));
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(types); ++i) {
        m_weekDayShortLabels.append(getLocaleInfoString(types[i]));
        if (m_weekDayShortLabels.last().isEmpty()) {
            m_weekDayShortLabels.shrink(0);
            m_weekDayShortLabels.reserveCapacity(WTF_ARRAY_LENGTH(WTF::weekdayName));
            for (unsigned w = 0; w < WTF_ARRAY_LENGTH(WTF::weekdayName); ++w) {
                // weekdayName starts with Monday.
                m_weekDayShortLabels.append(WTF::weekdayName[(w + 6) % 7]);
            }
            return;
        }
    }
}

#if ENABLE(CALENDAR_PICKER)
const Vector<String>& LocaleWin::monthLabels()
{
    ensureMonthLabels();
    return m_monthLabels;
}

const Vector<String>& LocaleWin::weekDayShortLabels()
{
    ensureWeekDayShortLabels();
    return m_weekDayShortLabels;
}
#endif

}
