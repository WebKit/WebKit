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
#include "Localizer.h"

#include <wtf/text/StringBuilder.h>

namespace WebCore {

Localizer::~Localizer()
{
}

void Localizer::setLocalizerData(const Vector<String, DecimalSymbolsSize>& symbols, const String& positivePrefix, const String& positiveSuffix, const String& negativePrefix, const String& negativeSuffix)
{
    for (size_t i = 0; i < symbols.size(); ++i) {
        ASSERT(!symbols[i].isEmpty());
        m_decimalSymbols[i] = symbols[i];
    }
    m_positivePrefix = positivePrefix;
    m_positiveSuffix = positiveSuffix;
    m_negativePrefix = negativePrefix;
    m_negativeSuffix = negativeSuffix;
    ASSERT(!m_positivePrefix.isEmpty() || !m_positiveSuffix.isEmpty() || !m_negativePrefix.isEmpty() || !m_negativeSuffix.isEmpty());
    m_hasLocalizerData = true;
}

String Localizer::convertToLocalizedNumber(const String& input)
{
    initializeLocalizerData();
    if (!m_hasLocalizerData || input.isEmpty())
        return input;

    unsigned i = 0;
    bool isNegative = false;
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

bool Localizer::detectSignAndGetDigitRange(const String& input, bool& isNegative, unsigned& startIndex, unsigned& endIndex)
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

unsigned Localizer::matchedDecimalSymbolIndex(const String& input, unsigned& position)
{
    for (unsigned symbolIndex = 0; symbolIndex < DecimalSymbolsSize; ++symbolIndex) {
        if (m_decimalSymbols[symbolIndex].length() && matches(input, position, m_decimalSymbols[symbolIndex])) {
            position += m_decimalSymbols[symbolIndex].length();
            return symbolIndex;
        }
    }
    return DecimalSymbolsSize;
}

String Localizer::convertFromLocalizedNumber(const String& localized)
{
    initializeLocalizerData();
    String input = localized.stripWhiteSpace();
    if (!m_hasLocalizerData || input.isEmpty())
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

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
String Localizer::localizedDecimalSeparator()
{
    initializeLocalizerData();
    return m_decimalSymbols[DecimalSeparatorIndex];
}

String Localizer::timeFormat()
{
    if (!m_localizedTimeFormatText.isEmpty())
        return m_localizedTimeFormatText;
    m_localizedTimeFormatText = "hh:mm:ss";
    return m_localizedTimeFormatText;
}

String Localizer::shortTimeFormat()
{
    if (!m_localizedShortTimeFormatText.isEmpty())
        return m_localizedShortTimeFormatText;
    m_localizedTimeFormatText = "hh:mm";
    return m_localizedShortTimeFormatText;
}

String Localizer::dateTimeFormatWithSecond()
{
    // FIXME: We should retreive the separator and the order from the system.
    StringBuilder builder;
    builder.append(dateFormat());
    builder.append(' ');
    builder.append(timeFormat());
    return builder.toString();
}

String Localizer::dateTimeFormatWithoutSecond()
{
    // FIXME: We should retreive the separator and the order from the system.
    StringBuilder builder;
    builder.append(dateFormat());
    builder.append(' ');
    builder.append(shortTimeFormat());
    return builder.toString();
}

const Vector<String>& Localizer::timeAMPMLabels()
{
    if (!m_timeAMPMLabels.isEmpty())
        return m_timeAMPMLabels;
    m_timeAMPMLabels.reserveCapacity(2);
    m_timeAMPMLabels.append("AM");
    m_timeAMPMLabels.append("PM");
    return m_timeAMPMLabels;
}
#endif

}
