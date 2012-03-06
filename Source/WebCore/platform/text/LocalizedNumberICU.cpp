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
#include "LocalizedNumberICU.h"

#include "LocalizedNumber.h"
#include <unicode/decimfmt.h>
#include <unicode/numfmt.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringBuilder.h>

using namespace icu;

namespace WebCore {

ICULocale::ICULocale(const Locale& locale)
    : m_locale(locale)
    , m_didCreateDecimalFormat(false)
{
}

PassOwnPtr<ICULocale> ICULocale::create(const char* localeString)
{
    return adoptPtr(new ICULocale(Locale::createCanonical(localeString)));
}

PassOwnPtr<ICULocale> ICULocale::createForCurrentLocale()
{
    return adoptPtr(new ICULocale(Locale::getDefault()));
}

void ICULocale::setDecimalSymbol(unsigned index, DecimalFormatSymbols::ENumberFormatSymbol symbol)
{
    UnicodeString ustring = m_decimalFormat->getDecimalFormatSymbols()->getSymbol(symbol);
    m_decimalSymbols[index] = String(ustring.getBuffer(), ustring.length());
}

void ICULocale::initializeDecimalFormat()
{
    if (m_didCreateDecimalFormat)
        return;
    m_didCreateDecimalFormat = true;
    UErrorCode status = U_ZERO_ERROR;
    NumberFormat* format = NumberFormat::createInstance(m_locale, NumberFormat::kNumberStyle, status);
    if (!U_SUCCESS(status))
        return;
    m_decimalFormat = adoptPtr(static_cast<DecimalFormat*>(format));

    setDecimalSymbol(0, DecimalFormatSymbols::kZeroDigitSymbol);
    setDecimalSymbol(1, DecimalFormatSymbols::kOneDigitSymbol);
    setDecimalSymbol(2, DecimalFormatSymbols::kTwoDigitSymbol);
    setDecimalSymbol(3, DecimalFormatSymbols::kThreeDigitSymbol);
    setDecimalSymbol(4, DecimalFormatSymbols::kFourDigitSymbol);
    setDecimalSymbol(5, DecimalFormatSymbols::kFiveDigitSymbol);
    setDecimalSymbol(6, DecimalFormatSymbols::kSixDigitSymbol);
    setDecimalSymbol(7, DecimalFormatSymbols::kSevenDigitSymbol);
    setDecimalSymbol(8, DecimalFormatSymbols::kEightDigitSymbol);
    setDecimalSymbol(9, DecimalFormatSymbols::kNineDigitSymbol);
    setDecimalSymbol(DecimalSeparatorIndex, DecimalFormatSymbols::kDecimalSeparatorSymbol);
    setDecimalSymbol(GroupSeparatorIndex, DecimalFormatSymbols::kGroupingSeparatorSymbol);
}

String ICULocale::convertToLocalizedNumber(const String& input)
{
    initializeDecimalFormat();
    if (!m_decimalFormat || input.isEmpty())
        return input;

    unsigned i = 0;
    bool isNegative = false;
    UnicodeString ustring;

    if (input[0] == '-') {
        ++i;
        isNegative = true;
        m_decimalFormat->getNegativePrefix(ustring);
    } else
        m_decimalFormat->getPositivePrefix(ustring);
    StringBuilder builder;
    builder.reserveCapacity(input.length());
    builder.append(ustring.getBuffer(), ustring.length());

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

    if (isNegative)
        m_decimalFormat->getNegativeSuffix(ustring);
    else
        m_decimalFormat->getPositiveSuffix(ustring);
    builder.append(ustring.getBuffer(), ustring.length());

    return builder.toString();
}

template <class StringType> static bool matches(const String& text, unsigned position, const StringType& part)
{
    if (part.isEmpty())
        return true;
    if (position + part.length() > text.length())
        return false;
    for (unsigned i = 0; i < static_cast<unsigned>(part.length()); ++i) {
        if (text[position + i] != part[i])
            return false;
    }
    return true;
}

static bool endsWith(const String& text, const UnicodeString& suffix)
{
    if (suffix.length() <= 0)
        return true;
    unsigned suffixLength = static_cast<unsigned>(suffix.length());
    if (suffixLength > text.length())
        return false;
    unsigned start = text.length() - suffixLength;
    for (unsigned i = 0; i < suffixLength; ++i) {
        if (text[start + i] != suffix[i])
            return false;
    }
    return true;
}

bool ICULocale::detectSignAndGetDigitRange(const String& input, bool& isNegative, unsigned& startIndex, unsigned& endIndex)
{
    startIndex = 0;
    endIndex = input.length();
    UnicodeString prefix;
    m_decimalFormat->getNegativePrefix(prefix);
    UnicodeString suffix;
    m_decimalFormat->getNegativeSuffix(suffix);
    if (prefix.isEmpty() && suffix.isEmpty()) {
        m_decimalFormat->getPositivePrefix(prefix);
        m_decimalFormat->getPositiveSuffix(suffix);
        ASSERT(!(prefix.isEmpty() && suffix.isEmpty()));
        if (matches(input, 0, prefix) && endsWith(input, suffix)) {
            isNegative = false;
            startIndex = prefix.length();
            endIndex -= suffix.length();
        } else
            isNegative = true;
    } else {
        if (matches(input, 0, prefix) && endsWith(input, suffix)) {
            isNegative = true;
            startIndex = prefix.length();
            endIndex -= suffix.length();
        } else {
            isNegative = false;
            m_decimalFormat->getPositivePrefix(prefix);
            m_decimalFormat->getPositiveSuffix(suffix);
            if (matches(input, 0, prefix) && endsWith(input, suffix)) {
                startIndex = prefix.length();
                endIndex -= suffix.length();
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
    if (!m_decimalFormat || input.isEmpty())
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

static ICULocale* currentLocale()
{
    static ICULocale* currentICULocale = ICULocale::createForCurrentLocale().leakPtr();
    return currentICULocale;
}

String convertToLocalizedNumber(const String& canonicalNumberString, unsigned fractionDigits)
{
    return currentLocale()->convertToLocalizedNumber(canonicalNumberString);
}

String convertFromLocalizedNumber(const String& localizedNumberString)
{
    return currentLocale()->convertFromLocalizedNumber(localizedNumberString);
}

} // namespace WebCore
