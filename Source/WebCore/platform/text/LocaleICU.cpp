/*
 * Copyright (C) 2011,2012 Google Inc. All rights reserved.
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

PassOwnPtr<Localizer> Localizer::create(const AtomicString& locale)
{
    return LocaleICU::create(locale.string().utf8().data());
}

LocaleICU::LocaleICU(const char* locale)
    : m_locale(locale)
    , m_numberFormat(0)
    , m_shortDateFormat(0)
    , m_didCreateDecimalFormat(false)
    , m_didCreateShortDateFormat(false)
#if ENABLE(CALENDAR_PICKER)
    , m_firstDayOfWeek(0)
#endif
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    , m_mediumTimeFormat(0)
    , m_shortTimeFormat(0)
    , m_didCreateTimeFormat(false)
#endif
{
}

LocaleICU::~LocaleICU()
{
    unum_close(m_numberFormat);
    udat_close(m_shortDateFormat);
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    udat_close(m_mediumTimeFormat);
    udat_close(m_shortTimeFormat);
#endif
}

PassOwnPtr<LocaleICU> LocaleICU::create(const char* localeString)
{
    return adoptPtr(new LocaleICU(localeString));
}

PassOwnPtr<LocaleICU> LocaleICU::createForCurrentLocale()
{
    return adoptPtr(new LocaleICU(0));
}

LocaleICU* LocaleICU::currentLocale()
{
    static LocaleICU* currentLocale = LocaleICU::createForCurrentLocale().leakPtr();
    return currentLocale;
}

String LocaleICU::decimalSymbol(UNumberFormatSymbol symbol)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t bufferLength = unum_getSymbol(m_numberFormat, symbol, 0, 0, &status);
    ASSERT(U_SUCCESS(status) || status == U_BUFFER_OVERFLOW_ERROR);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR)
        return String();
    Vector<UChar> buffer(bufferLength);
    status = U_ZERO_ERROR;
    unum_getSymbol(m_numberFormat, symbol, buffer.data(), bufferLength, &status);
    if (U_FAILURE(status))
        return String();
    return String::adopt(buffer);
}

String LocaleICU::decimalTextAttribute(UNumberFormatTextAttribute tag)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t bufferLength = unum_getTextAttribute(m_numberFormat, tag, 0, 0, &status);
    ASSERT(U_SUCCESS(status) || status == U_BUFFER_OVERFLOW_ERROR);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR)
        return String();
    Vector<UChar> buffer(bufferLength);
    status = U_ZERO_ERROR;
    unum_getTextAttribute(m_numberFormat, tag, buffer.data(), bufferLength, &status);
    ASSERT(U_SUCCESS(status));
    if (U_FAILURE(status))
        return String();
    return String::adopt(buffer);
}

void LocaleICU::initializeLocalizerData()
{
    if (m_didCreateDecimalFormat)
        return;
    m_didCreateDecimalFormat = true;
    UErrorCode status = U_ZERO_ERROR;
    m_numberFormat = unum_open(UNUM_DECIMAL, 0, 0, m_locale.data(), 0, &status);
    if (!U_SUCCESS(status))
        return;

    Vector<String, DecimalSymbolsSize> symbols;
    symbols.append(decimalSymbol(UNUM_ZERO_DIGIT_SYMBOL));
    symbols.append(decimalSymbol(UNUM_ONE_DIGIT_SYMBOL));
    symbols.append(decimalSymbol(UNUM_TWO_DIGIT_SYMBOL));
    symbols.append(decimalSymbol(UNUM_THREE_DIGIT_SYMBOL));
    symbols.append(decimalSymbol(UNUM_FOUR_DIGIT_SYMBOL));
    symbols.append(decimalSymbol(UNUM_FIVE_DIGIT_SYMBOL));
    symbols.append(decimalSymbol(UNUM_SIX_DIGIT_SYMBOL));
    symbols.append(decimalSymbol(UNUM_SEVEN_DIGIT_SYMBOL));
    symbols.append(decimalSymbol(UNUM_EIGHT_DIGIT_SYMBOL));
    symbols.append(decimalSymbol(UNUM_NINE_DIGIT_SYMBOL));
    symbols.append(decimalSymbol(UNUM_DECIMAL_SEPARATOR_SYMBOL));
    symbols.append(decimalSymbol(UNUM_GROUPING_SEPARATOR_SYMBOL));
    ASSERT(symbols.size() == DecimalSymbolsSize);
    setLocalizerData(symbols, decimalTextAttribute(UNUM_POSITIVE_PREFIX), decimalTextAttribute(UNUM_POSITIVE_SUFFIX), decimalTextAttribute(UNUM_NEGATIVE_PREFIX), decimalTextAttribute(UNUM_NEGATIVE_SUFFIX));
}

bool LocaleICU::initializeShortDateFormat()
{
    if (m_didCreateShortDateFormat)
        return m_shortDateFormat;
    m_shortDateFormat = openDateFormat(UDAT_NONE, UDAT_SHORT);
    m_didCreateShortDateFormat = true;
    return m_shortDateFormat;
}

UDateFormat* LocaleICU::openDateFormat(UDateFormatStyle timeStyle, UDateFormatStyle dateStyle) const
{
    const UChar gmtTimezone[3] = {'G', 'M', 'T'};
    UErrorCode status = U_ZERO_ERROR;
    return udat_open(timeStyle, dateStyle, m_locale.data(), gmtTimezone, WTF_ARRAY_LENGTH(gmtTimezone), 0, -1, &status);
}

double LocaleICU::parseDateTime(const String& input, DateComponents::Type type)
{
    if (type != DateComponents::Date)
        return std::numeric_limits<double>::quiet_NaN();
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

String LocaleICU::formatDateTime(const DateComponents& dateComponents, FormatType formatType)
{
    if (dateComponents.type() != DateComponents::Date)
        return Localizer::formatDateTime(dateComponents, formatType);
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

#if ENABLE(CALENDAR_PICKER) || ENABLE(INPUT_MULTIPLE_FIELDS_UI)
static String getDateFormatPattern(const UDateFormat* dateFormat)
{
    if (!dateFormat)
        return emptyString();

    UErrorCode status = U_ZERO_ERROR;
    int32_t length = udat_toPattern(dateFormat, TRUE, 0, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR || !length)
        return emptyString();
    Vector<UChar> buffer(length);
    status = U_ZERO_ERROR;
    udat_toPattern(dateFormat, TRUE, buffer.data(), length, &status);
    if (U_FAILURE(status))
        return emptyString();
    return String::adopt(buffer);
}
#endif

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
static String localizeFormat(const String& buffer)
{
    StringBuilder builder;
    UChar lastChar = 0;
    bool inQuote = false;
    for (unsigned i = 0; i < buffer.length(); ++i) {
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

void LocaleICU::initializeLocalizedDateFormatText()
{
    if (!m_localizedDateFormatText.isNull())
        return;
    m_localizedDateFormatText = emptyString();
    if (!initializeShortDateFormat())
        return;
    m_localizedDateFormatText = localizeFormat(getDateFormatPattern(m_shortDateFormat));
}

String LocaleICU::dateFormatText()
{
    initializeLocalizedDateFormatText();
    return m_localizedDateFormatText;
}

PassOwnPtr<Vector<String> > LocaleICU::createLabelVector(const UDateFormat* dateFormat, UDateFormatSymbolType type, int32_t startIndex, int32_t size)
{
    if (!dateFormat)
        return PassOwnPtr<Vector<String> >();
    if (udat_countSymbols(dateFormat, type) != startIndex + size)
        return PassOwnPtr<Vector<String> >();

    OwnPtr<Vector<String> > labels = adoptPtr(new Vector<String>());
    labels->reserveCapacity(size);
    for (int32_t i = 0; i < size; ++i) {
        UErrorCode status = U_ZERO_ERROR;
        int32_t length = udat_getSymbols(dateFormat, type, startIndex + i, 0, 0, &status);
        if (status != U_BUFFER_OVERFLOW_ERROR)
            return PassOwnPtr<Vector<String> >();
        Vector<UChar> buffer(length);
        status = U_ZERO_ERROR;
        udat_getSymbols(dateFormat, type, startIndex + i, buffer.data(), length, &status);
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

void LocaleICU::initializeCalendar()
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

    m_monthLabels = createLabelVector(m_shortDateFormat, UDAT_MONTHS, UCAL_JANUARY, 12);
    if (!m_monthLabels)
        m_monthLabels = createFallbackMonthLabels();

    m_weekDayShortLabels = createLabelVector(m_shortDateFormat, UDAT_SHORT_WEEKDAYS, UCAL_SUNDAY, 7);
    if (!m_weekDayShortLabels)
        m_weekDayShortLabels = createFallbackWeekDayShortLabels();
}

const Vector<String>& LocaleICU::monthLabels()
{
    initializeCalendar();
    return *m_monthLabels;
}

const Vector<String>& LocaleICU::weekDayShortLabels()
{
    initializeCalendar();
    return *m_weekDayShortLabels;
}

unsigned LocaleICU::firstDayOfWeek()
{
    initializeCalendar();
    return m_firstDayOfWeek;
}
#endif

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
static PassOwnPtr<Vector<String> > createFallbackAMPMLabels()
{
    OwnPtr<Vector<String> > labels = adoptPtr(new Vector<String>());
    labels->reserveCapacity(2);
    labels->append("AM");
    labels->append("PM");
    return labels.release();
}

void LocaleICU::initializeDateTimeFormat()
{
    if (m_didCreateTimeFormat)
        return;

    // We assume ICU medium time pattern and short time pattern are compatible
    // with LDML, because ICU specific pattern character "V" doesn't appear
    // in both medium and short time pattern.
    m_mediumTimeFormat = openDateFormat(UDAT_MEDIUM, UDAT_NONE);
    m_localizedTimeFormatText = getDateFormatPattern(m_mediumTimeFormat);

    m_shortTimeFormat = openDateFormat(UDAT_SHORT, UDAT_NONE);
    m_localizedShortTimeFormatText = getDateFormatPattern(m_shortTimeFormat);

    OwnPtr<Vector<String> > timeAMPMLabels = createLabelVector(m_mediumTimeFormat, UDAT_AM_PMS, UCAL_AM, 2);
    if (!timeAMPMLabels)
        timeAMPMLabels = createFallbackAMPMLabels();
    m_timeAMPMLabels = *timeAMPMLabels;

    m_didCreateTimeFormat = true;
}

String LocaleICU::dateFormat()
{
    if (!m_dateFormat.isEmpty())
        return m_dateFormat;
    if (!initializeShortDateFormat())
        return ASCIILiteral("dd/MM/yyyy");
    m_dateFormat = getDateFormatPattern(m_shortDateFormat);
    return m_dateFormat;
}

String LocaleICU::timeFormat()
{
    initializeDateTimeFormat();
    return m_localizedTimeFormatText;
}

String LocaleICU::shortTimeFormat()
{
    initializeDateTimeFormat();
    return m_localizedShortTimeFormatText;
}

const Vector<String>& LocaleICU::timeAMPMLabels()
{
    initializeDateTimeFormat();
    return m_timeAMPMLabels;
}

#endif

} // namespace WebCore

