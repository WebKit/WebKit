/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "LocaleICU.h"

#include "LocalizedStrings.h"
#include <limits>
#include <wtf/DateMath.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringBuilder.h>

using namespace icu;
using namespace std;

namespace WebCore {

ICULocale::ICULocale(const char* locale)
    : m_locale(locale)
    , m_numberFormat(0)
    , m_shortDateFormat(0)
    , m_didCreateDecimalFormat(false)
    , m_didCreateShortDateFormat(false)
#if ENABLE(CALENDAR_PICKER)
    , m_firstDayOfWeek(0)
#endif
{
}

ICULocale::~ICULocale()
{
    unum_close(m_numberFormat);
    udat_close(m_shortDateFormat);
}

PassOwnPtr<ICULocale> ICULocale::create(const char* localeString)
{
    return adoptPtr(new ICULocale(localeString));
}

PassOwnPtr<ICULocale> ICULocale::createForCurrentLocale()
{
    return adoptPtr(new ICULocale(0));
}

ICULocale* ICULocale::currentLocale()
{
    static ICULocale* currentICULocale = ICULocale::createForCurrentLocale().leakPtr();
    return currentICULocale;
}

void ICULocale::setDecimalSymbol(unsigned index, UNumberFormatSymbol symbol)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t bufferLength = unum_getSymbol(m_numberFormat, symbol, 0, 0, &status);
    ASSERT(U_SUCCESS(status) || status == U_BUFFER_OVERFLOW_ERROR);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR)
        return;
    Vector<UChar> buffer(bufferLength);
    status = U_ZERO_ERROR;
    unum_getSymbol(m_numberFormat, symbol, buffer.data(), bufferLength, &status);
    if (U_FAILURE(status))
        return;
    m_decimalSymbols[index] = String::adopt(buffer);
}

void ICULocale::setDecimalTextAttribute(String& destination, UNumberFormatTextAttribute tag)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t bufferLength = unum_getTextAttribute(m_numberFormat, tag, 0, 0, &status);
    ASSERT(U_SUCCESS(status) || status == U_BUFFER_OVERFLOW_ERROR);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR)
        return;
    Vector<UChar> buffer(bufferLength);
    status = U_ZERO_ERROR;
    unum_getTextAttribute(m_numberFormat, tag, buffer.data(), bufferLength, &status);
    ASSERT(U_SUCCESS(status));
    if (U_FAILURE(status))
        return;
    destination = String::adopt(buffer);
}

void ICULocale::initializeDecimalFormat()
{
    if (m_didCreateDecimalFormat)
        return;
    m_didCreateDecimalFormat = true;
    UErrorCode status = U_ZERO_ERROR;
    m_numberFormat = unum_open(UNUM_DECIMAL, 0, 0, m_locale.data(), 0, &status);
    if (!U_SUCCESS(status))
        return;

    setDecimalSymbol(0, UNUM_ZERO_DIGIT_SYMBOL);
    setDecimalSymbol(1, UNUM_ONE_DIGIT_SYMBOL);
    setDecimalSymbol(2, UNUM_TWO_DIGIT_SYMBOL);
    setDecimalSymbol(3, UNUM_THREE_DIGIT_SYMBOL);
    setDecimalSymbol(4, UNUM_FOUR_DIGIT_SYMBOL);
    setDecimalSymbol(5, UNUM_FIVE_DIGIT_SYMBOL);
    setDecimalSymbol(6, UNUM_SIX_DIGIT_SYMBOL);
    setDecimalSymbol(7, UNUM_SEVEN_DIGIT_SYMBOL);
    setDecimalSymbol(8, UNUM_EIGHT_DIGIT_SYMBOL);
    setDecimalSymbol(9, UNUM_NINE_DIGIT_SYMBOL);
    setDecimalSymbol(DecimalSeparatorIndex, UNUM_DECIMAL_SEPARATOR_SYMBOL);
    setDecimalSymbol(GroupSeparatorIndex, UNUM_GROUPING_SEPARATOR_SYMBOL);
    setDecimalTextAttribute(m_positivePrefix, UNUM_POSITIVE_PREFIX);
    setDecimalTextAttribute(m_positiveSuffix, UNUM_POSITIVE_SUFFIX);
    setDecimalTextAttribute(m_negativePrefix, UNUM_NEGATIVE_PREFIX);
    setDecimalTextAttribute(m_negativeSuffix, UNUM_NEGATIVE_SUFFIX);
    ASSERT(!m_positivePrefix.isEmpty() || !m_positiveSuffix.isEmpty() || !m_negativePrefix.isEmpty() || !m_negativeSuffix.isEmpty());
}

String ICULocale::convertToLocalizedNumber(const String& input)
{
    initializeDecimalFormat();
    if (!m_numberFormat || input.isEmpty())
        return input;

    unsigned i = 0;
    bool isNegative = false;
    UnicodeString ustring;
    StringBuilder builder;
    builder.reserveCapacity(input.length());

    if (input[0] == '-') {
        ++i;
        isNegative = true;
        builder.append(m_negativePrefix);
    } else
        builder.append(m_positivePrefix);

    for (; i < input.length(); ++i) {
        switch (input[i]) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            builder.append(m_decimalSymbols[input[i] - '0']);
            break;
        case '.':
            builder.append(m_decimalSymbols[DecimalSeparatorIndex]);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    builder.append(isNegative ? m_negativeSuffix : m_positiveSuffix);

    return builder.toString();
}

static bool matches(const String& text, unsigned position, const String& part)
{
    if (part.isEmpty())
        return true;
    if (position + part.length() > text.length())
        return false;
    for (unsigned i = 0; i < part.length(); ++i) {
        if (text[position + i] != part[i])
            return false;
    }
    return true;
}

bool ICULocale::detectSignAndGetDigitRange(const String& input, bool& isNegative, unsigned& startIndex, unsigned& endIndex)
{
    startIndex = 0;
    endIndex = input.length();
    if (m_negativePrefix.isEmpty() && m_negativeSuffix.isEmpty()) {
        if (input.startsWith(m_positivePrefix) && input.endsWith(m_positiveSuffix)) {
            isNegative = false;
            startIndex = m_positivePrefix.length();
            endIndex -= m_positiveSuffix.length();
        } else
            isNegative = true;
    } else {
        if (input.startsWith(m_negativePrefix) && input.endsWith(m_negativeSuffix)) {
            isNegative = true;
            startIndex = m_negativePrefix.length();
            endIndex -= m_negativeSuffix.length();
        } else {
            isNegative = false;
            if (input.startsWith(m_positivePrefix) && input.endsWith(m_positiveSuffix)) {
                startIndex = m_positivePrefix.length();
                endIndex -= m_positiveSuffix.length();
            } else
                return false;
        }
    }
    return true;
}

unsigned ICULocale::matchedDecimalSymbolIndex(const String& input, unsigned& position)
{
    for (unsigned symbolIndex = 0; symbolIndex < DecimalSymbolsSize; ++symbolIndex) {
        if (m_decimalSymbols[symbolIndex].length() && matches(input, position, m_decimalSymbols[symbolIndex])) {
            position += m_decimalSymbols[symbolIndex].length();
            return symbolIndex;
        }
    }
    return DecimalSymbolsSize;
}

String ICULocale::convertFromLocalizedNumber(const String& localized)
{
    initializeDecimalFormat();
    String input = localized.stripWhiteSpace();
    if (!m_numberFormat || input.isEmpty())
        return input;

    bool isNegative;
    unsigned startIndex;
    unsigned endIndex;
    if (!detectSignAndGetDigitRange(input, isNegative, startIndex, endIndex)) {
        // Input is broken. Returning an invalid number string.
        return "*";
    }

    StringBuilder builder;
    builder.reserveCapacity(input.length());
    if (isNegative)
        builder.append("-");
    for (unsigned i = startIndex; i < endIndex;) {
        unsigned symbolIndex = matchedDecimalSymbolIndex(input, i);
        if (symbolIndex >= DecimalSymbolsSize)
            return "*";
        if (symbolIndex == DecimalSeparatorIndex)
            builder.append('.');
        else if (symbolIndex == GroupSeparatorIndex) {
            // Ignore group separators.

        } else
            builder.append(static_cast<UChar>('0' + symbolIndex));
    }
    return builder.toString();
}

bool ICULocale::initializeShortDateFormat()
{
    if (m_didCreateShortDateFormat)
        return m_shortDateFormat;
    const UChar gmtTimezone[3] = {'G', 'M', 'T'};
    UErrorCode status = U_ZERO_ERROR;
    m_shortDateFormat = udat_open(UDAT_NONE, UDAT_SHORT, m_locale.data(), gmtTimezone, WTF_ARRAY_LENGTH(gmtTimezone), 0, -1, &status);
    m_didCreateShortDateFormat = true;
    return m_shortDateFormat;
}

double ICULocale::parseLocalizedDate(const String& input)
{
    if (!initializeShortDateFormat())
        return numeric_limits<double>::quiet_NaN();
    if (input.length() > static_cast<unsigned>(numeric_limits<int32_t>::max()))
        return numeric_limits<double>::quiet_NaN();
    int32_t inputLength = static_cast<int32_t>(input.length());
    UErrorCode status = U_ZERO_ERROR;
    int32_t parsePosition = 0;
    UDate date = udat_parse(m_shortDateFormat, input.characters(), inputLength, &parsePosition, &status);
    if (parsePosition != inputLength || U_FAILURE(status))
        return numeric_limits<double>::quiet_NaN();
    // UDate, which is an alias of double, is compatible with our expectation.
    return date;
}

String ICULocale::formatLocalizedDate(const DateComponents& dateComponents)
{
    if (!initializeShortDateFormat())
        return String();
    double input = dateComponents.millisecondsSinceEpoch();
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = udat_format(m_shortDateFormat, input, 0, 0, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR)
        return String();
    Vector<UChar> buffer(length);
    status = U_ZERO_ERROR;
    udat_format(m_shortDateFormat, input, buffer.data(), length, 0, &status);
    if (U_FAILURE(status))
        return String();
    return String::adopt(buffer);
}

#if ENABLE(CALENDAR_PICKER)
static inline bool isICUYearSymbol(UChar letter)
{
    return letter == 'y' || letter == 'Y';
}

static inline bool isICUMonthSymbol(UChar letter)
{
    return letter == 'M';
}

static inline bool isICUDayInMonthSymbol(UChar letter)
{
    return letter == 'd';
}

// Specification of the input:
// http://icu-project.org/apiref/icu4c/classSimpleDateFormat.html#details
static String localizeFormat(const Vector<UChar>& buffer)
{
    StringBuilder builder;
    UChar lastChar = 0;
    bool inQuote = false;
    for (unsigned i = 0; i < buffer.size(); ++i) {
        if (inQuote) {
            if (buffer[i] == '\'') {
                inQuote = false;
                lastChar = 0;
                ASSERT(i);
                if (buffer[i - 1] == '\'')
                    builder.append('\'');
            } else
                builder.append(buffer[i]);
        } else {
            if (isASCIIAlpha(lastChar) && lastChar == buffer[i])
                continue;
            lastChar = buffer[i];
            if (isICUYearSymbol(lastChar)) {
                String text = dateFormatYearText();
                builder.append(text.isEmpty() ? "Year" : text);
            } else if (isICUMonthSymbol(lastChar)) {
                String text = dateFormatMonthText();
                builder.append(text.isEmpty() ? "Month" : text);
            } else if (isICUDayInMonthSymbol(lastChar)) {
                String text = dateFormatDayInMonthText();
                builder.append(text.isEmpty() ? "Day" : text);
            } else if (lastChar == '\'')
                inQuote = true;
            else
                builder.append(lastChar);
        }
    }
    return builder.toString();
}

void ICULocale::initializeLocalizedDateFormatText()
{
    if (!m_localizedDateFormatText.isNull())
        return;
    m_localizedDateFormatText = String("");
    if (!initializeShortDateFormat())
        return;
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = udat_toPattern(m_shortDateFormat, TRUE, 0, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR)
        return;
    Vector<UChar> buffer(length);
    status = U_ZERO_ERROR;
    udat_toPattern(m_shortDateFormat, TRUE, buffer.data(), length, &status);
    if (U_FAILURE(status))
        return;
    m_localizedDateFormatText = localizeFormat(buffer);
}

String ICULocale::localizedDateFormatText()
{
    initializeLocalizedDateFormatText();
    return m_localizedDateFormatText;
}

PassOwnPtr<Vector<String> > ICULocale::createLabelVector(UDateFormatSymbolType type, int32_t startIndex, int32_t size)
{
    if (!m_shortDateFormat)
        return PassOwnPtr<Vector<String> >();
    if (udat_countSymbols(m_shortDateFormat, type) != startIndex + size)
        return PassOwnPtr<Vector<String> >();

    OwnPtr<Vector<String> > labels = adoptPtr(new Vector<String>());
    labels->reserveCapacity(size);
    for (int32_t i = 0; i < size; ++i) {
        UErrorCode status = U_ZERO_ERROR;
        int32_t length = udat_getSymbols(m_shortDateFormat, type, startIndex + i, 0, 0, &status);
        if (status != U_BUFFER_OVERFLOW_ERROR)
            return PassOwnPtr<Vector<String> >();
        Vector<UChar> buffer(length);
        status = U_ZERO_ERROR;
        udat_getSymbols(m_shortDateFormat, type, startIndex + i, buffer.data(), length, &status);
        if (U_FAILURE(status))
            return PassOwnPtr<Vector<String> >();
        labels->append(String::adopt(buffer));
    }
    return labels.release();
}

static PassOwnPtr<Vector<String> > createFallbackMonthLabels()
{
    OwnPtr<Vector<String> > labels = adoptPtr(new Vector<String>());
    labels->reserveCapacity(WTF_ARRAY_LENGTH(WTF::monthFullName));
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(WTF::monthFullName); ++i)
        labels->append(WTF::monthFullName[i]);
    return labels.release();
}

static PassOwnPtr<Vector<String> > createFallbackWeekDayShortLabels()
{
    OwnPtr<Vector<String> > labels = adoptPtr(new Vector<String>());
    labels->reserveCapacity(7);
    labels->append("Sun");
    labels->append("Mon");
    labels->append("Tue");
    labels->append("Wed");
    labels->append("Thu");
    labels->append("Fri");
    labels->append("Sat");
    return labels.release();
}

void ICULocale::initializeCalendar()
{
    if (m_monthLabels && m_weekDayShortLabels)
        return;

    if (!initializeShortDateFormat()) {
        m_firstDayOfWeek = 0;
        m_monthLabels = createFallbackMonthLabels();
        m_weekDayShortLabels = createFallbackWeekDayShortLabels();
        return;
    }
    m_firstDayOfWeek = ucal_getAttribute(udat_getCalendar(m_shortDateFormat), UCAL_FIRST_DAY_OF_WEEK) - UCAL_SUNDAY;

    m_monthLabels = createLabelVector(UDAT_MONTHS, UCAL_JANUARY, 12);
    if (!m_monthLabels)
        m_monthLabels = createFallbackMonthLabels();

    m_weekDayShortLabels = createLabelVector(UDAT_SHORT_WEEKDAYS, UCAL_SUNDAY, 7);
    if (!m_weekDayShortLabels)
        m_weekDayShortLabels = createFallbackWeekDayShortLabels();
}

const Vector<String>& ICULocale::monthLabels()
{
    initializeCalendar();
    return *m_monthLabels;
}

const Vector<String>& ICULocale::weekDayShortLabels()
{
    initializeCalendar();
    return *m_weekDayShortLabels;
}

unsigned ICULocale::firstDayOfWeek()
{
    initializeCalendar();
    return m_firstDayOfWeek;
}
#endif

} // namespace WebCore

