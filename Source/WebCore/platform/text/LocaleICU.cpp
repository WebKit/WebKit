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
#include <unicode/udatpg.h>
#include <unicode/uloc.h>
#include <wtf/DateMath.h>
#include <wtf/text/StringBuilder.h>


namespace WebCore {
using namespace icu;

std::unique_ptr<Locale> Locale::create(const AtomicString& locale)
{
    return std::make_unique<LocaleICU>(locale.string().utf8().data());
}

LocaleICU::LocaleICU(const char* locale)
    : m_locale(locale)
{
}

LocaleICU::~LocaleICU()
{
#if !UCONFIG_NO_FORMATTING
    unum_close(m_numberFormat);
#endif
#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    udat_close(m_shortDateFormat);
    udat_close(m_mediumTimeFormat);
    udat_close(m_shortTimeFormat);
#endif
}

#if !UCONFIG_NO_FORMATTING
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
    return String::adopt(WTFMove(buffer));
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
    return String::adopt(WTFMove(buffer));
}
#endif

void LocaleICU::initializeLocaleData()
{
#if !UCONFIG_NO_FORMATTING
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
    setLocaleData(symbols, decimalTextAttribute(UNUM_POSITIVE_PREFIX), decimalTextAttribute(UNUM_POSITIVE_SUFFIX), decimalTextAttribute(UNUM_NEGATIVE_PREFIX), decimalTextAttribute(UNUM_NEGATIVE_SUFFIX));
#endif
}

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
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
    return String::adopt(WTFMove(buffer));
}

std::unique_ptr<Vector<String>> LocaleICU::createLabelVector(const UDateFormat* dateFormat, UDateFormatSymbolType type, int32_t startIndex, int32_t size)
{
    if (!dateFormat)
        return std::make_unique<Vector<String>>();
    if (udat_countSymbols(dateFormat, type) != startIndex + size)
        return std::make_unique<Vector<String>>();

    auto labels = std::make_unique<Vector<String>>();
    labels->reserveCapacity(size);
    for (int32_t i = 0; i < size; ++i) {
        UErrorCode status = U_ZERO_ERROR;
        int32_t length = udat_getSymbols(dateFormat, type, startIndex + i, 0, 0, &status);
        if (status != U_BUFFER_OVERFLOW_ERROR)
            return std::make_unique<Vector<String>>();
        Vector<UChar> buffer(length);
        status = U_ZERO_ERROR;
        udat_getSymbols(dateFormat, type, startIndex + i, buffer.data(), length, &status);
        if (U_FAILURE(status))
            return std::make_unique<Vector<String>>();
        labels->append(String::adopt(WTFMove(buffer)));
    }
    return WTFMove(labels);
}

static std::unique_ptr<Vector<String>> createFallbackMonthLabels()
{
    auto labels = std::make_unique<Vector<String>>();
    labels->reserveCapacity(WTF_ARRAY_LENGTH(WTF::monthFullName));
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(WTF::monthFullName); ++i)
        labels->append(WTF::monthFullName[i]);
    return WTFMove(labels);
}

const Vector<String>& LocaleICU::monthLabels()
{
    if (m_monthLabels)
        return *m_monthLabels;
    if (initializeShortDateFormat()) {
        m_monthLabels = createLabelVector(m_shortDateFormat, UDAT_MONTHS, UCAL_JANUARY, 12);
        if (m_monthLabels)
            return *m_monthLabels;
    }
    m_monthLabels = createFallbackMonthLabels();
    return *m_monthLabels;
}

static std::unique_ptr<Vector<String>> createFallbackAMPMLabels()
{
    auto labels = std::make_unique<Vector<String>>();
    labels->reserveCapacity(2);
    labels->append("AM");
    labels->append("PM");
    return WTFMove(labels);
}

void LocaleICU::initializeDateTimeFormat()
{
    if (m_didCreateTimeFormat)
        return;

    // We assume ICU medium time pattern and short time pattern are compatible
    // with LDML, because ICU specific pattern character "V" doesn't appear
    // in both medium and short time pattern.
    m_mediumTimeFormat = openDateFormat(UDAT_MEDIUM, UDAT_NONE);
    m_timeFormatWithSeconds = getDateFormatPattern(m_mediumTimeFormat);

    m_shortTimeFormat = openDateFormat(UDAT_SHORT, UDAT_NONE);
    m_timeFormatWithoutSeconds = getDateFormatPattern(m_shortTimeFormat);

    UDateFormat* dateTimeFormatWithSeconds = openDateFormat(UDAT_MEDIUM, UDAT_SHORT);
    m_dateTimeFormatWithSeconds = getDateFormatPattern(dateTimeFormatWithSeconds);
    udat_close(dateTimeFormatWithSeconds);

    UDateFormat* dateTimeFormatWithoutSeconds = openDateFormat(UDAT_SHORT, UDAT_SHORT);
    m_dateTimeFormatWithoutSeconds = getDateFormatPattern(dateTimeFormatWithoutSeconds);
    udat_close(dateTimeFormatWithoutSeconds);

    auto timeAMPMLabels = createLabelVector(m_mediumTimeFormat, UDAT_AM_PMS, UCAL_AM, 2);
    if (!timeAMPMLabels)
        timeAMPMLabels = createFallbackAMPMLabels();
    m_timeAMPMLabels = *timeAMPMLabels;

    m_didCreateTimeFormat = true;
}

String LocaleICU::dateFormat()
{
    if (!m_dateFormat.isNull())
        return m_dateFormat;
    if (!initializeShortDateFormat())
        return "yyyy-MM-dd"_s;
    m_dateFormat = getDateFormatPattern(m_shortDateFormat);
    return m_dateFormat;
}

static String getFormatForSkeleton(const char* locale, const UChar* skeleton, int32_t skeletonLength)
{
    String format = "yyyy-MM"_s;
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator* patternGenerator = udatpg_open(locale, &status);
    if (!patternGenerator)
        return format;
    status = U_ZERO_ERROR;
    int32_t length = udatpg_getBestPattern(patternGenerator, skeleton, skeletonLength, 0, 0, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR && length) {
        Vector<UChar> buffer(length);
        status = U_ZERO_ERROR;
        udatpg_getBestPattern(patternGenerator, skeleton, skeletonLength, buffer.data(), length, &status);
        if (U_SUCCESS(status))
            format = String::adopt(WTFMove(buffer));
    }
    udatpg_close(patternGenerator);
    return format;
}

String LocaleICU::monthFormat()
{
    if (!m_monthFormat.isNull())
        return m_monthFormat;
    // Gets a format for "MMMM" because Windows API always provides formats for
    // "MMMM" in some locales.
    const UChar skeleton[] = { 'y', 'y', 'y', 'y', 'M', 'M', 'M', 'M' };
    m_monthFormat = getFormatForSkeleton(m_locale.data(), skeleton, WTF_ARRAY_LENGTH(skeleton));
    return m_monthFormat;
}

String LocaleICU::shortMonthFormat()
{
    if (!m_shortMonthFormat.isNull())
        return m_shortMonthFormat;
    const UChar skeleton[] = { 'y', 'y', 'y', 'y', 'M', 'M', 'M' };
    m_shortMonthFormat = getFormatForSkeleton(m_locale.data(), skeleton, WTF_ARRAY_LENGTH(skeleton));
    return m_shortMonthFormat;
}

String LocaleICU::timeFormat()
{
    initializeDateTimeFormat();
    return m_timeFormatWithSeconds;
}

String LocaleICU::shortTimeFormat()
{
    initializeDateTimeFormat();
    return m_timeFormatWithoutSeconds;
}

String LocaleICU::dateTimeFormatWithSeconds()
{
    initializeDateTimeFormat();
    return m_dateTimeFormatWithSeconds;
}

String LocaleICU::dateTimeFormatWithoutSeconds()
{
    initializeDateTimeFormat();
    return m_dateTimeFormatWithoutSeconds;
}

const Vector<String>& LocaleICU::shortMonthLabels()
{
    if (!m_shortMonthLabels.isEmpty())
        return m_shortMonthLabels;
    if (initializeShortDateFormat()) {
        if (auto labels = createLabelVector(m_shortDateFormat, UDAT_SHORT_MONTHS, UCAL_JANUARY, 12)) {
            m_shortMonthLabels = *labels;
            return m_shortMonthLabels;
        }
    }
    m_shortMonthLabels.reserveCapacity(WTF_ARRAY_LENGTH(WTF::monthName));
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(WTF::monthName); ++i)
        m_shortMonthLabels.append(WTF::monthName[i]);
    return m_shortMonthLabels;
}

const Vector<String>& LocaleICU::standAloneMonthLabels()
{
    if (!m_standAloneMonthLabels.isEmpty())
        return m_standAloneMonthLabels;
    if (initializeShortDateFormat()) {
        if (auto labels = createLabelVector(m_shortDateFormat, UDAT_STANDALONE_MONTHS, UCAL_JANUARY, 12)) {
            m_standAloneMonthLabels = *labels;
            return m_standAloneMonthLabels;
        }
    }
    m_standAloneMonthLabels = monthLabels();
    return m_standAloneMonthLabels;
}

const Vector<String>& LocaleICU::shortStandAloneMonthLabels()
{
    if (!m_shortStandAloneMonthLabels.isEmpty())
        return m_shortStandAloneMonthLabels;
    if (initializeShortDateFormat()) {
        if (auto labels = createLabelVector(m_shortDateFormat, UDAT_STANDALONE_SHORT_MONTHS, UCAL_JANUARY, 12)) {
            m_shortStandAloneMonthLabels = *labels;
            return m_shortStandAloneMonthLabels;
        }
    }
    m_shortStandAloneMonthLabels = shortMonthLabels();
    return m_shortStandAloneMonthLabels;
}

const Vector<String>& LocaleICU::timeAMPMLabels()
{
    initializeDateTimeFormat();
    return m_timeAMPMLabels;
}

#endif

} // namespace WebCore

