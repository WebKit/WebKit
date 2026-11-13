/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/unicode/ListFormatter.h>

#include <unicode/uformattedvalue.h>
#include <unicode/ulistformatter.h>
#include <wtf/Borrow.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringImpl.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace WTF {

void ListFormatter::Deleter::operator()(UListFormatter* formatter) const
{
    if (formatter)
        ulistfmt_close(formatter);
}

static UListFormatterType icuType(ListFormatter::Type type)
{
    switch (type) {
    case ListFormatter::Type::Conjunction:
        return ULISTFMT_TYPE_AND;
    case ListFormatter::Type::Disjunction:
        return ULISTFMT_TYPE_OR;
    case ListFormatter::Type::Unit:
        return ULISTFMT_TYPE_UNITS;
    }
    return ULISTFMT_TYPE_AND;
}

static UListFormatterWidth icuWidth(ListFormatter::Style style)
{
    switch (style) {
    case ListFormatter::Style::Long:
        return ULISTFMT_WIDTH_WIDE;
    case ListFormatter::Style::Short:
        return ULISTFMT_WIDTH_SHORT;
    case ListFormatter::Style::Narrow:
        return ULISTFMT_WIDTH_NARROW;
    }
    return ULISTFMT_WIDTH_WIDE;
}

ListFormatter::ListFormatter(const ASCIICString& locale, Type type, Style style)
{
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<UListFormatter, Deleter> formatter { ulistfmt_openForType(locale.data(), icuType(type), icuWidth(style), &status) };
    if (U_SUCCESS(status)) [[likely]]
        m_formatter = WTF::move(formatter);
}

namespace {

// ICU takes the strings as UTF-16 pointers with lengths.
class FormatterInput {
    WTF_MAKE_NONCOPYABLE(FormatterInput);
public:
    explicit FormatterInput(std::span<const String> strings)
        : m_strings(ReserveCapacityTag { }, strings.size())
        , m_stringPointers(ReserveCapacityTag { }, strings.size())
        , m_stringLengths(ReserveCapacityTag { }, strings.size())
    {
        for (auto& string : strings) {
            String string16 = string;
            string16.convertTo16Bit();
            append(string16.span16());
            m_strings.append(WTF::move(string16));
        }
    }

    explicit FormatterInput(std::span<const std::span<const char16_t>> strings)
        : m_stringPointers(ReserveCapacityTag { }, strings.size())
        , m_stringLengths(ReserveCapacityTag { }, strings.size())
    {
        for (auto string : strings)
            append(string);
    }

    int32_t size() const { return m_stringPointers.size(); }
    const char16_t* const* stringPointers() const LIFETIME_BOUND { return m_stringPointers.span().data(); }
    const int32_t* stringLengths() const LIFETIME_BOUND { return m_stringLengths.span().data(); }

private:
    void append(std::span<const char16_t> string)
    {
        m_stringPointers.append(string.data());
        m_stringLengths.append(static_cast<int32_t>(string.size()));
    }

    static constexpr size_t inlineCapacity = 10;
    Vector<String, inlineCapacity> m_strings;
    Vector<const char16_t*, inlineCapacity> m_stringPointers;
    Vector<int32_t, inlineCapacity> m_stringLengths;
};

} // namespace

static std::optional<String> formatInput(const UListFormatter* formatter, const FormatterInput& input)
{
    Vector<char16_t, 128> result;
    auto status = callBufferProducingFunction(ulistfmt_format, formatter, input.stringPointers(), input.stringLengths(), input.size(), result);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    return String { StringImpl::create8BitIfPossible(borrow(result)->span()) };
}

std::optional<String> ListFormatter::format(std::span<const String> strings) const
{
    if (!m_formatter) [[unlikely]]
        return std::nullopt;
    return formatInput(m_formatter.get(), FormatterInput { strings });
}

std::optional<String> ListFormatter::format(std::span<const std::span<const char16_t>> strings) const
{
    if (!m_formatter) [[unlikely]]
        return std::nullopt;
    return formatInput(m_formatter.get(), FormatterInput { strings });
}

std::optional<ListFormatter::FormattedList> ListFormatter::formatToParts(std::span<const String> strings) const
{
    if (!m_formatter) [[unlikely]]
        return std::nullopt;

    FormatterInput input(strings);
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<UFormattedList, ICUDeleter<ulistfmt_closeResult>> result { ulistfmt_openResult(&status) };
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;

    ulistfmt_formatStringsToResult(m_formatter.get(), input.stringPointers(), input.stringLengths(), input.size(), result.get(), &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;

    // The UFormattedList owns the UFormattedValue.
    auto* formattedValue = ulistfmt_resultAsValue(result.get(), &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;

    int32_t length = 0;
    auto* characters = ufmtval_getString(formattedValue, &length, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;

    std::unique_ptr<UConstrainedFieldPosition, ICUDeleter<ucfpos_close>> position { ucfpos_open(&status) };
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;

    ucfpos_constrainField(position.get(), UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;

    FormattedList formattedList { String { unsafeMakeSpan(characters, length) }, { } };
    while (ufmtval_nextPosition(formattedValue, position.get(), &status)) {
        int32_t begin = 0;
        int32_t end = 0;
        ucfpos_getIndexes(position.get(), &begin, &end, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return std::nullopt;
        formattedList.elementRanges.append({ static_cast<unsigned>(begin), static_cast<unsigned>(end) });
    }
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    return formattedList;
}

} // namespace WTF
