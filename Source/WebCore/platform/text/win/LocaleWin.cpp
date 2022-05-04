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
#include "DateTimeFormat.h"
#include "LocalizedStrings.h"
#include <limits>
#include <windows.h>
#include <wtf/DateMath.h>
#include <wtf/HashMap.h>
#include <wtf/Language.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/win/WCharStringExtras.h>

namespace WebCore {

typedef HashMap<String, LCID, ASCIICaseInsensitiveHash> NameToLCIDMap;

static String extractLanguageCode(const String& locale)
{
    size_t dashPosition = locale.find('-');
    if (dashPosition == notFound)
        return locale;
    return locale.left(dashPosition);
}

static LCID LCIDFromLocaleInternal(LCID userDefaultLCID, const String& userDefaultLanguageCode, const String& locale)
{
    if (equalIgnoringASCIICase(extractLanguageCode(locale), userDefaultLanguageCode))
        return userDefaultLCID;
    return LocaleNameToLCID(locale.wideCharacters().data(), 0);
}

static LCID LCIDFromLocale(const AtomString& locale)
{
    // According to MSDN, 9 is enough for LOCALE_SISO639LANGNAME.
    const size_t languageCodeBufferSize = 9;
    WCHAR lowercaseLanguageCode[languageCodeBufferSize];
    ::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lowercaseLanguageCode, languageCodeBufferSize);
    String userDefaultLanguageCode(lowercaseLanguageCode);

    LCID lcid = LCIDFromLocaleInternal(LOCALE_USER_DEFAULT, userDefaultLanguageCode, String(locale));
    if (!lcid)
        lcid = LCIDFromLocaleInternal(LOCALE_USER_DEFAULT, userDefaultLanguageCode, defaultLanguage());
    return lcid;
}

std::unique_ptr<Locale> Locale::create(const AtomString& locale)
{
    return makeUnique<LocaleWin>(LCIDFromLocale(locale));
}

inline LocaleWin::LocaleWin(LCID lcid)
    : m_lcid(lcid)
    , m_didInitializeNumberData(false)
{
}

LocaleWin::~LocaleWin() = default;

String LocaleWin::getLocaleInfoString(LCTYPE type)
{
    int bufferSizeWithNUL = ::GetLocaleInfo(m_lcid, type, 0, 0);
    if (bufferSizeWithNUL <= 0)
        return String();
    Vector<UChar> buffer(bufferSizeWithNUL);
    ::GetLocaleInfo(m_lcid, type, wcharFrom(buffer.data()), bufferSizeWithNUL);
    buffer.shrink(bufferSizeWithNUL - 1);
    return String::adopt(WTFMove(buffer));
}

void LocaleWin::getLocaleInfo(LCTYPE type, DWORD& result)
{
    ::GetLocaleInfo(m_lcid, type | LOCALE_RETURN_NUMBER, reinterpret_cast<LPWSTR>(&result), sizeof(DWORD) / sizeof(TCHAR));
}

void LocaleWin::ensureShortMonthLabels()
{
#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
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
#endif
}

// -------------------------------- Tokenized date format

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
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

static void commitLiteralToken(StringBuilder& literalBuffer, StringBuilder& converted)
{
    if (literalBuffer.length() <= 0)
        return;
    DateTimeFormat::quoteAndAppendLiteral(literalBuffer.toString(), converted);
    literalBuffer.clear();
}

// This function converts Windows date/time pattern format [1][2] into LDML date
// format pattern [3].
//
// i.e.
//   We set h, H, m, s, d, dd, M, or y as is. They have same meaning in both of
//   Windows and LDML.
//   We need to convert the following patterns:
//     t -> a
//     tt -> a
//     ddd -> EEE
//     dddd -> EEEE
//     g -> G
//     gg -> ignore
//
// [1] http://msdn.microsoft.com/en-us/library/dd317787(v=vs.85).aspx
// [2] http://msdn.microsoft.com/en-us/library/dd318148(v=vs.85).aspx
// [3] LDML http://unicode.org/reports/tr35/tr35-6.html#Date_Format_Patterns
static String convertWindowsDateTimeFormat(const String& format)
{
    StringBuilder converted;
    StringBuilder literalBuffer;
    bool inQuote = false;
    bool lastQuoteCanBeLiteral = false;
    for (unsigned i = 0; i < format.length(); ++i) {
        UChar ch = format[i];
        if (inQuote) {
            if (ch == '\'') {
                inQuote = false;
                ASSERT(i);
                if (lastQuoteCanBeLiteral && format[i - 1] == '\'') {
                    literalBuffer.append('\'');
                    lastQuoteCanBeLiteral = false;
                } else
                    lastQuoteCanBeLiteral = true;
            } else
                literalBuffer.append(ch);
            continue;
        }

        if (ch == '\'') {
            inQuote = true;
            if (lastQuoteCanBeLiteral && i > 0 && format[i - 1] == '\'') {
                literalBuffer.append(ch);
                lastQuoteCanBeLiteral = false;
            } else
                lastQuoteCanBeLiteral = true;
        } else if (isASCIIAlpha(ch)) {
            commitLiteralToken(literalBuffer, converted);
            unsigned symbolStart = i;
            unsigned count = countContinuousLetters(format, i);
            i += count - 1;
            if (ch == 'h' || ch == 'H' || ch == 'm' || ch == 's' || ch == 'M' || ch == 'y')
                converted.append(format, symbolStart, count);
            else if (ch == 'd') {
                if (count <= 2)
                    converted.append(format, symbolStart, count);
                else if (count == 3)
                    converted.append("EEE");
                else
                    converted.append("EEEE");
            } else if (ch == 'g') {
                if (count == 1)
                    converted.append('G');
                else {
                    // gg means imperial era in Windows.
                    // Just ignore it.
                }
            } else if (ch == 't')
                converted.append('a');
            else
                literalBuffer.append(format, symbolStart, count);
        } else
            literalBuffer.append(ch);
    }
    commitLiteralToken(literalBuffer, converted);
    return converted.toString();
}
#endif

void LocaleWin::ensureMonthLabels()
{
#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
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
#endif
}

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
const Vector<String>& LocaleWin::monthLabels()
{
    ensureMonthLabels();
    return m_monthLabels;
}

String LocaleWin::dateFormat()
{
    if (m_dateFormat.isNull())
        m_dateFormat = convertWindowsDateTimeFormat(getLocaleInfoString(LOCALE_SSHORTDATE));
    return m_dateFormat;
}

String LocaleWin::dateFormat(const String& windowsFormat)
{
    return convertWindowsDateTimeFormat(windowsFormat);
}

String LocaleWin::monthFormat()
{
    if (m_monthFormat.isNull())
        m_monthFormat = convertWindowsDateTimeFormat(getLocaleInfoString(LOCALE_SYEARMONTH));
    return m_monthFormat;
}

String LocaleWin::shortMonthFormat()
{
    if (m_shortMonthFormat.isNull())
        m_shortMonthFormat = makeStringByReplacingAll(convertWindowsDateTimeFormat(getLocaleInfoString(LOCALE_SYEARMONTH)), "MMMM"_s, "MMM"_s);
    return m_shortMonthFormat;
}

String LocaleWin::timeFormat()
{
    if (m_timeFormatWithSeconds.isNull())
        m_timeFormatWithSeconds = convertWindowsDateTimeFormat(getLocaleInfoString(LOCALE_STIMEFORMAT));
    return m_timeFormatWithSeconds;
}

String LocaleWin::shortTimeFormat()
{
    if (!m_timeFormatWithoutSeconds.isNull())
        return m_timeFormatWithoutSeconds;
    String format = getLocaleInfoString(LOCALE_SSHORTTIME);
    // Vista or older Windows doesn't support LOCALE_SSHORTTIME.
    if (format.isEmpty()) {
        format = getLocaleInfoString(LOCALE_STIMEFORMAT);
        StringBuilder builder;
        builder.append(getLocaleInfoString(LOCALE_STIME));
        builder.append("ss");
        size_t pos = format.reverseFind(builder.toString());
        if (pos != notFound)
            format = makeStringByRemoving(format, pos, builder.length());
    }
    m_timeFormatWithoutSeconds = convertWindowsDateTimeFormat(format);
    return m_timeFormatWithoutSeconds;
}

String LocaleWin::dateTimeFormatWithSeconds()
{
    if (!m_dateTimeFormatWithSeconds.isNull())
        return m_dateTimeFormatWithSeconds;
    StringBuilder builder;
    builder.append(dateFormat());
    builder.append(' ');
    builder.append(timeFormat());
    m_dateTimeFormatWithSeconds = builder.toString();
    return m_dateTimeFormatWithSeconds;
}

String LocaleWin::dateTimeFormatWithoutSeconds()
{
    if (!m_dateTimeFormatWithoutSeconds.isNull())
        return m_dateTimeFormatWithoutSeconds;
    StringBuilder builder;
    builder.append(dateFormat());
    builder.append(' ');
    builder.append(shortTimeFormat());
    m_dateTimeFormatWithoutSeconds = builder.toString();
    return m_dateTimeFormatWithoutSeconds;
}

const Vector<String>& LocaleWin::shortMonthLabels()
{
    ensureShortMonthLabels();
    return m_shortMonthLabels;
}

const Vector<String>& LocaleWin::standAloneMonthLabels()
{
    // Windows doesn't provide a way to get stand-alone month labels.
    return monthLabels();
}

const Vector<String>& LocaleWin::shortStandAloneMonthLabels()
{
    // Windows doesn't provide a way to get stand-alone month labels.
    return shortMonthLabels();
}

const Vector<String>& LocaleWin::timeAMPMLabels()
{
    if (m_timeAMPMLabels.isEmpty()) {
        m_timeAMPMLabels.append(getLocaleInfoString(LOCALE_S1159));
        m_timeAMPMLabels.append(getLocaleInfoString(LOCALE_S2359));
    }
    return m_timeAMPMLabels;
}
#endif

void LocaleWin::initializeLocaleData()
{
    if (m_didInitializeNumberData)
        return;

    Vector<String, DecimalSymbolsSize> symbols;
    enum DigitSubstitution {
        DigitSubstitutionContext = 0,
        DigitSubstitution0to9 = 1,
        DigitSubstitutionNative = 2,
    };
    DWORD digitSubstitution = DigitSubstitution0to9;
    getLocaleInfo(LOCALE_IDIGITSUBSTITUTION, digitSubstitution);
    if (digitSubstitution == DigitSubstitution0to9) {
        symbols.append("0"_s);
        symbols.append("1"_s);
        symbols.append("2"_s);
        symbols.append("3"_s);
        symbols.append("4"_s);
        symbols.append("5"_s);
        symbols.append("6"_s);
        symbols.append("7"_s);
        symbols.append("8"_s);
        symbols.append("9"_s);
    } else {
        String digits = getLocaleInfoString(LOCALE_SNATIVEDIGITS);
        ASSERT(digits.length() >= 10);
        for (unsigned i = 0; i < 10; ++i)
            symbols.append(digits.substring(i, 1));
    }
    ASSERT(symbols.size() == DecimalSeparatorIndex);
    symbols.append(getLocaleInfoString(LOCALE_SDECIMAL));
    ASSERT(symbols.size() == GroupSeparatorIndex);
    symbols.append(getLocaleInfoString(LOCALE_STHOUSAND));
    ASSERT(symbols.size() == DecimalSymbolsSize);

    String negativeSign = getLocaleInfoString(LOCALE_SNEGATIVESIGN);
    enum NegativeFormat {
        NegativeFormatParenthesis = 0,
        NegativeFormatSignPrefix = 1,
        NegativeFormatSignSpacePrefix = 2,
        NegativeFormatSignSuffix = 3,
        NegativeFormatSpaceSignSuffix = 4,
    };
    DWORD negativeFormat = NegativeFormatSignPrefix;
    getLocaleInfo(LOCALE_INEGNUMBER, negativeFormat);
    String negativePrefix = emptyString();
    String negativeSuffix = emptyString();
    switch (negativeFormat) {
    case NegativeFormatParenthesis:
        negativePrefix = "("_s;
        negativeSuffix = ")"_s;
        break;
    case NegativeFormatSignSpacePrefix:
        negativePrefix = negativeSign + " ";
        break;
    case NegativeFormatSignSuffix:
        negativeSuffix = negativeSign;
        break;
    case NegativeFormatSpaceSignSuffix:
        negativeSuffix = " " + negativeSign;
        break;
    case NegativeFormatSignPrefix:
        FALLTHROUGH;
    default:
        negativePrefix = negativeSign;
        break;
    }
    m_didInitializeNumberData = true;
    setLocaleData(symbols, emptyString(), emptyString(), negativePrefix, negativeSuffix);
}

}
