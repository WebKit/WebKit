/*
 * Copyright (C) 2011-2016 Google Inc. All rights reserved.
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
#include "PlatformLocale.h"

#include "DateComponents.h"
#include "DateTimeFormat.h"
#include "LocalizedStrings.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Locale);

class DateTimeStringBuilder : private DateTimeFormat::TokenHandler {
    WTF_MAKE_NONCOPYABLE(DateTimeStringBuilder);

public:
    // The argument objects must be alive until this object dies.
    DateTimeStringBuilder(Locale&, const DateComponents&);

    bool build(StringImpl*);
    RefPtr<StringImpl> toString();

private:
    // DateTimeFormat::TokenHandler functions.
    void visitField(DateTimeFormat::FieldType, int) final;
    void visitLiteral(StringImpl*) final;

    RefPtr<StringImpl> zeroPadString(StringImpl*, size_t width);
    void appendNumber(int number, size_t width);

    StringBuilder m_builder;
    const CheckedRef<Locale> m_localizer;
    const DateComponents& m_date;
};

DateTimeStringBuilder::DateTimeStringBuilder(Locale& localizer, const DateComponents& date)
    : m_localizer(localizer)
    , m_date(date)
{
}

bool DateTimeStringBuilder::build(StringImpl* formatString)
{
    m_builder.reserveCapacity(length(formatString));
    return DateTimeFormat::parse(formatString, *this);
}

RefPtr<StringImpl> DateTimeStringBuilder::zeroPadString(StringImpl* string, size_t width)
{
    if (length(string) >= width)
        return string;
    StringBuilder zeroPaddedStringBuilder;
    zeroPaddedStringBuilder.reserveCapacity(width);
    for (size_t i = length(string); i < width; ++i)
        zeroPaddedStringBuilder.append('0');
    zeroPaddedStringBuilder.append(string);
    return zeroPaddedStringBuilder.toString();
}

void DateTimeStringBuilder::appendNumber(int number, size_t width)
{
    RefPtr<StringImpl> zeroPaddedNumberString = zeroPadString(makeString(number), width);
    m_builder.append(m_localizer->convertToLocalizedNumber(zeroPaddedNumberString));
}

void DateTimeStringBuilder::visitField(DateTimeFormat::FieldType fieldType, int numberOfPatternCharacters)
{
    switch (fieldType) {
    case DateTimeFormat::FieldTypeYear:
        // Always use padding width of 4 so it matches DateTimeEditElement.
        appendNumber(m_date.fullYear(), 4);
        return;
    case DateTimeFormat::FieldTypeMonth:
        if (numberOfPatternCharacters == 3)
            m_builder.append(m_localizer->shortMonthLabels()[m_date.month()]);
        else if (numberOfPatternCharacters == 4)
            m_builder.append(m_localizer->monthLabels()[m_date.month()]);
        else {
            // Always use padding width of 2 so it matches DateTimeEditElement.
            appendNumber(m_date.month() + 1, 2);
        }
        return;
    case DateTimeFormat::FieldTypeMonthStandAlone:
        if (numberOfPatternCharacters == 3)
            m_builder.append(m_localizer->shortStandAloneMonthLabels()[m_date.month()]);
        else if (numberOfPatternCharacters == 4)
            m_builder.append(m_localizer->standAloneMonthLabels()[m_date.month()]);
        else {
            // Always use padding width of 2 so it matches DateTimeEditElement.
            appendNumber(m_date.month() + 1, 2);
        }
        return;
    case DateTimeFormat::FieldTypeDayOfMonth:
        // Always use padding width of 2 so it matches DateTimeEditElement.
        appendNumber(m_date.monthDay(), 2);
        return;
    case DateTimeFormat::FieldTypeWeekOfYear:
        // Always use padding width of 2 so it matches DateTimeEditElement.
        appendNumber(m_date.week(), 2);
        return;
    case DateTimeFormat::FieldTypePeriod:
        m_builder.append(m_localizer->timeAMPMLabels()[(m_date.hour() >= 12 ? 1 : 0)]);
        return;
    case DateTimeFormat::FieldTypeHour12: {
        int hour12 = m_date.hour() % 12;
        if (!hour12)
            hour12 = 12;
        appendNumber(hour12, numberOfPatternCharacters);
        return;
    }
    case DateTimeFormat::FieldTypeHour23:
        appendNumber(m_date.hour(), numberOfPatternCharacters);
        return;
    case DateTimeFormat::FieldTypeHour11:
        appendNumber(m_date.hour() % 12, numberOfPatternCharacters);
        return;
    case DateTimeFormat::FieldTypeHour24: {
        int hour24 = m_date.hour();
        if (!hour24)
            hour24 = 24;
        appendNumber(hour24, numberOfPatternCharacters);
        return;
    }
    case DateTimeFormat::FieldTypeMinute:
        appendNumber(m_date.minute(), numberOfPatternCharacters);
        return;
    case DateTimeFormat::FieldTypeSecond:
        if (!m_date.millisecond())
            appendNumber(m_date.second(), numberOfPatternCharacters);
        else {
            double second = m_date.second() + m_date.millisecond() / 1000.0;
            RefPtr<StringImpl> zeroPaddedSecondString = zeroPadString(StringImpl::numberToStringFixedWidth(second, 3), numberOfPatternCharacters + 4);
            m_builder.append(m_localizer->convertToLocalizedNumber(zeroPaddedSecondString));
        }
        return;
    default:
        return;
    }
}

void DateTimeStringBuilder::visitLiteral(StringImpl* text)
{
    ASSERT(length(text));
    m_builder.append(text);
}

RefPtr<StringImpl> DateTimeStringBuilder::toString()
{
    return m_builder.toString();
}

Locale::~Locale() = default;

void Locale::setLocaleData(const Vector<RefPtr<StringImpl>, DecimalSymbolsSize>& symbols, StringImpl* positivePrefix, StringImpl* positiveSuffix, StringImpl* negativePrefix, StringImpl* negativeSuffix)
{
    for (size_t i = 0; i < symbols.size(); ++i) {
        ASSERT(!isEmpty(symbols[i]));
        m_decimalSymbols[i] = symbols[i];
    }
    m_positivePrefix = positivePrefix;
    m_positiveSuffix = positiveSuffix;
    m_negativePrefix = negativePrefix;
    m_negativeSuffix = negativeSuffix;
    ASSERT(!isEmpty(m_positivePrefix) || !isEmpty(m_positiveSuffix) || !isEmpty(m_negativePrefix) || !isEmpty(m_negativeSuffix));
    m_hasLocaleData = true;
}

RefPtr<StringImpl> Locale::convertToLocalizedNumber(StringImpl* input)
{
    initializeLocaleData();
    if (!m_hasLocaleData || isEmpty(input))
        return input;

    unsigned i = 0;
    bool isNegative = false;
    StringBuilder builder;
    builder.reserveCapacity(length(input));

    if (codeUnitAt(input, 0) == '-') {
        ++i;
        isNegative = true;
        builder.append(m_negativePrefix);
    } else
        builder.append(m_positivePrefix);

    for (; i < length(input); ++i) {
        switch (codeUnitAt(input, i)) {
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
            builder.append(m_decimalSymbols[codeUnitAt(input, i) - '0']);
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

NODELETE static bool matches(StringImpl* text, unsigned position, StringImpl* part)
{
    if (isEmpty(part))
        return true;
    if (position + length(part) > length(text))
        return false;
    for (unsigned i = 0; i < length(part); ++i) {
        if (codeUnitAt(text, position + i) != codeUnitAt(part, i))
            return false;
    }
    return true;
}

bool Locale::detectSignAndGetDigitRange(StringImpl* input, bool& isNegative, unsigned& startIndex, unsigned& endIndex)
{
    startIndex = 0;
    endIndex = length(input);
    if (isEmpty(m_negativePrefix) && isEmpty(m_negativeSuffix)) {
        if (startsWith(input, m_positivePrefix) && endsWith(input, m_positiveSuffix)) {
            isNegative = false;
            startIndex = length(m_positivePrefix);
            endIndex -= length(m_positiveSuffix);
        } else
            isNegative = true;
    } else {
        if (startsWith(input, m_negativePrefix) && endsWith(input, m_negativeSuffix)) {
            isNegative = true;
            startIndex = length(m_negativePrefix);
            endIndex -= length(m_negativeSuffix);
        } else {
            isNegative = false;
            if (startsWith(input, m_positivePrefix) && endsWith(input, m_positiveSuffix)) {
                startIndex = length(m_positivePrefix);
                endIndex -= length(m_positiveSuffix);
            } else
                return false;
        }
    }
    return true;
}

unsigned Locale::matchedDecimalSymbolIndex(StringImpl* input, unsigned& position)
{
    for (unsigned symbolIndex = 0; symbolIndex < DecimalSymbolsSize; ++symbolIndex) {
        if (length(m_decimalSymbols[symbolIndex]) && matches(input, position, m_decimalSymbols[symbolIndex])) {
            position += length(m_decimalSymbols[symbolIndex]);
            return symbolIndex;
        }
    }
    return DecimalSymbolsSize;
}

RefPtr<StringImpl> Locale::convertFromLocalizedNumber(StringImpl* localized)
{
    initializeLocaleData();
    auto input = trim(localized, deprecatedIsSpaceOrNewline);
    if (!m_hasLocaleData || isEmpty(input))
        return input;

    bool isNegative;
    unsigned startIndex;
    unsigned endIndex;
    if (!detectSignAndGetDigitRange(input, isNegative, startIndex, endIndex))
        return input;

    // Ignore leading '+', but will reject '+'-only string later.
    if (!isNegative && endIndex - startIndex >= 2 && codeUnitAt(input, startIndex) == '+')
        ++startIndex;

    StringBuilder builder;
    builder.reserveCapacity(length(input));
    if (isNegative)
        builder.append('-');
    for (unsigned i = startIndex; i < endIndex;) {
        unsigned symbolIndex = matchedDecimalSymbolIndex(input, i);
        if (symbolIndex >= DecimalSymbolsSize)
            return input;
        if (symbolIndex == DecimalSeparatorIndex)
            builder.append('.');
        else if (symbolIndex == GroupSeparatorIndex)
            return input;
        else
            builder.append(static_cast<char16_t>('0' + symbolIndex));
    }
    RefPtr<StringImpl> converted = builder.toString();
    // Ignore trailing '.', but will reject '.'-only string later.
    if (length(converted) >= 2 && codeUnitAt(converted, length(converted) - 1) == '.')
        converted = left(converted, length(converted) - 1);
    return converted;
}

Locale::WritingDirection Locale::defaultWritingDirection() const
{
    return WritingDirection::Default;
}

RefPtr<StringImpl> Locale::formatDateTime(const DateComponents& date, FormatType formatType)
{
    if (date.type() == DateComponentsType::Invalid)
        return nullptr;

    DateTimeStringBuilder builder(*this, date);
    switch (date.type()) {
    case DateComponentsType::Time:
        builder.build(formatType == FormatTypeShort ? shortTimeFormat() : timeFormat());
        break;
    case DateComponentsType::Date:
        builder.build(dateFormat());
        break;
    case DateComponentsType::Month:
        builder.build(formatType == FormatTypeShort ? shortMonthFormat() : monthFormat());
        break;
    case DateComponentsType::Week:    
        // FIXME: Add support for formatting weeks.
        break;
    case DateComponentsType::DateTimeLocal:
        builder.build(formatType == FormatTypeShort ? dateTimeFormatWithoutSeconds() : dateTimeFormatWithSeconds());
        break;
    case DateComponentsType::Invalid:
        ASSERT_NOT_REACHED();
        break;
    }
    return builder.toString();
}

const RefPtr<StringImpl>& Locale::localizedDecimalSeparator()
{
    initializeLocaleData();
    return m_decimalSymbols[DecimalSeparatorIndex];
}

RefPtr<StringImpl> Locale::localizeNumberCharacters(StringImpl* input)
{
    initializeLocaleData();
    if (!m_hasLocaleData || isEmpty(input))
        return input;

    StringBuilder builder;
    builder.reserveCapacity(length(input));
    for (unsigned i = 0; i < length(input); ++i) {
        auto character = codeUnitAt(input, i);
        if (character >= '0' && character <= '9')
            builder.append(m_decimalSymbols[character - '0']);
        else if (character == '.')
            builder.append(m_decimalSymbols[DecimalSeparatorIndex]);
        else
            builder.append(character);
    }
    return builder.toString();
}

}
