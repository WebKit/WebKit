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
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "DateTimeFormat.h"
#endif
#include "Language.h"
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

typedef LCID (WINAPI* LocaleNameToLCIDPtr)(LPCWSTR, DWORD);

static String extractLanguageCode(const String& locale)
{
    size_t dashPosition = locale.find('-');
    if (dashPosition == notFound)
        return locale;
    return locale.left(dashPosition);
}

static LCID LCIDFromLocaleInternal(LCID userDefaultLCID, const String& userDefaultLanguageCode, LocaleNameToLCIDPtr localeNameToLCID, String& locale)
{
    String localeLanguageCode = extractLanguageCode(locale);
    if (equalIgnoringCase(localeLanguageCode, userDefaultLanguageCode))
        return userDefaultLCID;
    return localeNameToLCID(locale.charactersWithNullTermination(), 0);
}

static LCID LCIDFromLocale(const AtomicString& locale)
{
    // LocaleNameToLCID() is available since Windows Vista.
    LocaleNameToLCIDPtr localeNameToLCID = reinterpret_cast<LocaleNameToLCIDPtr>(::GetProcAddress(::GetModuleHandle(L"kernel32"), "LocaleNameToLCID"));
    if (!localeNameToLCID)
        return LOCALE_USER_DEFAULT;

    // According to MSDN, 9 is enough for LOCALE_SISO639LANGNAME.
    const size_t languageCodeBufferSize = 9;
    WCHAR lowercaseLanguageCode[languageCodeBufferSize];
    ::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lowercaseLanguageCode, languageCodeBufferSize);
    String userDefaultLanguageCode = String(lowercaseLanguageCode);

    LCID lcid = LCIDFromLocaleInternal(LOCALE_USER_DEFAULT, userDefaultLanguageCode, localeNameToLCID, String(locale));
    if (!lcid)
        lcid = LCIDFromLocaleInternal(LOCALE_USER_DEFAULT, userDefaultLanguageCode, localeNameToLCID, defaultLanguage());
    return lcid;
}

PassOwnPtr<Locale> Locale::create(const AtomicString& locale)
{
    return LocaleWin::create(LCIDFromLocale(locale));
}

inline LocaleWin::LocaleWin(LCID lcid)
    : m_lcid(lcid)
    , m_didInitializeNumberData(false)
{
#if ENABLE(CALENDAR_PICKER)
    DWORD value = 0;
    getLocaleInfo(LOCALE_IFIRSTDAYOFWEEK, value);
    // 0:Monday, ..., 6:Sunday.
    // We need 1 for Monday, 0 for Sunday.
    m_firstDayOfWeek = (value + 1) % 7;
#endif
}

PassOwnPtr<LocaleWin> LocaleWin::create(LCID lcid)
{
    return adoptPtr(new LocaleWin(lcid));
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

void LocaleWin::getLocaleInfo(LCTYPE type, DWORD& result)
{
    ::GetLocaleInfo(m_lcid, type | LOCALE_RETURN_NUMBER, reinterpret_cast<LPWSTR>(&result), sizeof(DWORD) / sizeof(TCHAR));
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

void LocaleWin::ensureShortDateTokens()
{
    if (!m_shortDateTokens.isEmpty())
        return;
    m_shortDateTokens = parseDateFormat(getLocaleInfoString(LOCALE_SSHORTDATE));
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

#if ENABLE(CALENDAR_PICKER) || ENABLE(INPUT_MULTIPLE_FIELDS_UI)
const Vector<String>& LocaleWin::monthLabels()
{
    ensureMonthLabels();
    return m_monthLabels;
}
#endif

#if ENABLE(CALENDAR_PICKER)
const Vector<String>& LocaleWin::weekDayShortLabels()
{
    ensureWeekDayShortLabels();
    return m_weekDayShortLabels;
}

unsigned LocaleWin::firstDayOfWeek()
{
    return m_firstDayOfWeek;
}

bool LocaleWin::isRTL()
{
    WTF::Unicode::Direction dir = WTF::Unicode::direction(monthLabels()[0][0]);
    return dir == WTF::Unicode::RightToLeft || dir == WTF::Unicode::RightToLeftArabic;
}
#endif

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
static String convertWindowsDateFormatToLDML(const Vector<DateFormatToken>& tokens)
{
    StringBuilder buffer;
    for (unsigned i = 0; i < tokens.size(); ++i) {
        switch (tokens[i].type) {
        case DateFormatToken::Literal:
            DateTimeFormat::quoteAndAppendLiteral(tokens[i].data, buffer);
            break;

        case DateFormatToken::Day2:
            buffer.append(static_cast<char>(DateTimeFormat::FieldTypeDayOfMonth));
            // Fallthrough.
        case DateFormatToken::Day1:
            buffer.append(static_cast<char>(DateTimeFormat::FieldTypeDayOfMonth));
            break;

        case DateFormatToken::Month4:
            buffer.append(static_cast<char>(DateTimeFormat::FieldTypeMonth));
            // Fallthrough.
        case DateFormatToken::Month3:
            buffer.append(static_cast<char>(DateTimeFormat::FieldTypeMonth));
            // Fallthrough.
        case DateFormatToken::Month2:
            buffer.append(static_cast<char>(DateTimeFormat::FieldTypeMonth));
            // Fallthrough.
        case DateFormatToken::Month1:
            buffer.append(static_cast<char>(DateTimeFormat::FieldTypeMonth));
            break;

        case DateFormatToken::Year4:
            buffer.append(static_cast<char>(DateTimeFormat::FieldTypeYear));
            buffer.append(static_cast<char>(DateTimeFormat::FieldTypeYear));
            // Fallthrough.
        case DateFormatToken::Year2:
            buffer.append(static_cast<char>(DateTimeFormat::FieldTypeYear));
            // Fallthrough.
        case DateFormatToken::Year1:
            buffer.append(static_cast<char>(DateTimeFormat::FieldTypeYear));
            break;
        }
    }
    return buffer.toString();
}

static DateTimeFormat::FieldType mapCharacterToDateTimeFieldType(UChar ch)
{
    switch (ch) {
    case 'h':
        return DateTimeFormat::FieldTypeHour12;

    case 'H':
        return DateTimeFormat::FieldTypeHour23;

    case 'm':
        return DateTimeFormat::FieldTypeMinute;

    case 's':
        return DateTimeFormat::FieldTypeSecond;

    case 't':
        return DateTimeFormat::FieldTypePeriod;

    default:
        return DateTimeFormat::FieldTypeLiteral;
    }
}

// This class used for converting Windows time pattern format[1] into LDML[2]
// time format string.
// [1] http://msdn.microsoft.com/en-us/library/windows/desktop/dd318148(v=vs.85).aspx
// [2] LDML http://unicode.org/reports/tr35/tr35-6.html#Date_Format_Patterns
static String convertWindowsTimeFormatToLDML(const String& windowsTimeFormat)
{
    StringBuilder builder;
    int counter = 0;
    DateTimeFormat::FieldType lastFieldType = DateTimeFormat::FieldTypeLiteral;
    for (unsigned index = 0; index < windowsTimeFormat.length(); ++index) {
        UChar const ch = windowsTimeFormat[index];
        DateTimeFormat::FieldType fieldType = mapCharacterToDateTimeFieldType(ch);
        if (fieldType == DateTimeFormat::FieldTypeLiteral)
            builder.append(ch);
        else if (fieldType == lastFieldType) {
            ++counter;
            if (counter == 2 && lastFieldType != DateTimeFormat::FieldTypePeriod)
                builder.append(static_cast<UChar>(lastFieldType));
        } else {
            if (lastFieldType != DateTimeFormat::FieldTypeLiteral)
                builder.append(static_cast<UChar>(lastFieldType));
            builder.append(static_cast<UChar>(fieldType));
            counter = 1;
        }
        lastFieldType = fieldType;
    }
    return builder.toString();
}

String LocaleWin::dateFormat()
{
    if (!m_dateFormat.isNull())
        return m_dateFormat;
    ensureShortDateTokens();
    m_dateFormat = convertWindowsDateFormatToLDML(m_shortDateTokens);
    return m_dateFormat;
}

String LocaleWin::dateFormat(const String& windowsFormat)
{
    return convertWindowsDateFormatToLDML(parseDateFormat(windowsFormat));
}

String LocaleWin::monthFormat()
{
    if (!m_monthFormat.isNull())
        return m_monthFormat;
    m_monthFormat = convertWindowsDateFormatToLDML(parseDateFormat(getLocaleInfoString(LOCALE_SYEARMONTH)));
    return m_monthFormat;
}

String LocaleWin::timeFormat()
{
    if (m_timeFormatWithSeconds.isNull())
        m_timeFormatWithSeconds = convertWindowsTimeFormatToLDML(getLocaleInfoString(LOCALE_STIMEFORMAT));
    return m_timeFormatWithSeconds;
}

String LocaleWin::shortTimeFormat()
{
    if (!m_timeFormatWithoutSeconds.isNull())
        return m_timeFormatWithoutSeconds;
    String format = getLocaleInfoString(LOCALE_SSHORTTIME);
    // Vista or older Windows doesn't support LOCALE_SSHORTTIME.
    if (format.isEmpty()) {
        format = timeFormat();
        StringBuilder builder;
        builder.append(getLocaleInfoString(LOCALE_STIME));
        builder.append("ss");
        size_t pos = format.reverseFind(builder.toString());
        if (pos != notFound)
            format.remove(pos, builder.length());
    }
    m_timeFormatWithoutSeconds = convertWindowsTimeFormatToLDML(format);
    return m_timeFormatWithoutSeconds;
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
        symbols.append("0");
        symbols.append("1");
        symbols.append("2");
        symbols.append("3");
        symbols.append("4");
        symbols.append("5");
        symbols.append("6");
        symbols.append("7");
        symbols.append("8");
        symbols.append("9");
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
        negativePrefix = "(";
        negativeSuffix = ")";
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
    case NegativeFormatSignPrefix: // Fall through.
    default:
        negativePrefix = negativeSign;
        break;
    }
    m_didInitializeNumberData = true;
    setLocaleData(symbols, emptyString(), emptyString(), negativePrefix, negativeSuffix);
}

}
