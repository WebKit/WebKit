/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <wtf/text/WTFString.h>

#include <cmath>
#include <wtf/ASCIICType.h>
#include <wtf/DataLog.h>
#include <wtf/Function.h>
#include <wtf/HexNumber.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/dtoa.h>
#include <wtf/text/CString.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/unicode/UTF8Conversion.h>

namespace WTF {

// String Operations

enum class WhitespacePolicy : bool { Skip, Preserve };
enum class ParseMode : bool { General, Fixed };

template<typename CharacterType, TrailingJunkPolicy trailingJunkPolicy, WhitespacePolicy whitespacePolicy, ParseMode mode>
static inline double toDoubleType(std::span<const CharacterType> data, bool* ok, size_t& parsedLength)
{
    size_t leadingSpacesLength = 0;
    if constexpr (whitespacePolicy == WhitespacePolicy::Skip) {
        while (leadingSpacesLength < data.size() && isUnicodeCompatibleASCIIWhitespace(data[leadingSpacesLength]))
            ++leadingSpacesLength;
    }

    double number;
    if constexpr (mode == ParseMode::Fixed)
        number = parseFixedDouble(data.subspan(leadingSpacesLength), parsedLength);
    else
        number = parseDouble(data.subspan(leadingSpacesLength), parsedLength);

    if (!parsedLength) {
        if (ok)
            *ok = false;
        return 0.0;
    }

    parsedLength += leadingSpacesLength;
    if (ok)
        *ok = trailingJunkPolicy == TrailingJunkPolicy::Allow || parsedLength == data.size();
    return number;
}

double charactersToDouble(std::span<const Latin1Character> data, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<Latin1Character, TrailingJunkPolicy::Disallow, WhitespacePolicy::Skip, ParseMode::General>(data, ok, parsedLength);
}

double charactersToDouble(std::span<const char16_t> data, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<char16_t, TrailingJunkPolicy::Disallow, WhitespacePolicy::Skip, ParseMode::General>(data, ok, parsedLength);
}

double charactersToFixedDouble(std::span<const Latin1Character> data, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<Latin1Character, TrailingJunkPolicy::Disallow, WhitespacePolicy::Preserve, ParseMode::Fixed>(data, ok, parsedLength);
}

double charactersToFixedDouble(std::span<const char16_t> data, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<char16_t, TrailingJunkPolicy::Disallow, WhitespacePolicy::Preserve, ParseMode::Fixed>(data, ok, parsedLength);
}

NODELETE static inline float doubleToFloatCheckingOverflow(double number, bool* isValid)
{
    float result = static_cast<float>(number);
    if (isValid && *isValid && std::isfinite(number) && !std::isfinite(result))
        *isValid = false;
    return result;
}

float charactersToFloat(std::span<const Latin1Character> data, bool* ok)
{
    size_t parsedLength;
    double number = toDoubleType<Latin1Character, TrailingJunkPolicy::Disallow, WhitespacePolicy::Skip, ParseMode::General>(data, ok, parsedLength);
    return doubleToFloatCheckingOverflow(number, ok);
}

float charactersToFloat(std::span<const char16_t> data, bool* ok)
{
    size_t parsedLength;
    double number = toDoubleType<char16_t, TrailingJunkPolicy::Disallow, WhitespacePolicy::Skip, ParseMode::General>(data, ok, parsedLength);
    return doubleToFloatCheckingOverflow(number, ok);
}

float charactersToFloat(std::span<const Latin1Character> data, size_t& parsedLength)
{
    double number = toDoubleType<Latin1Character, TrailingJunkPolicy::Allow, WhitespacePolicy::Skip, ParseMode::General>(data, nullptr, parsedLength);
    return static_cast<float>(number);
}

float charactersToFloat(std::span<const char16_t> data, size_t& parsedLength)
{
    double number = toDoubleType<char16_t, TrailingJunkPolicy::Allow, WhitespacePolicy::Skip, ParseMode::General>(data, nullptr, parsedLength);
    return static_cast<float>(number);
}

RefPtr<StringImpl> makeString(int number)
{
    return numberToStringSigned<RefPtr<StringImpl>>(number);
}

RefPtr<StringImpl> makeString(unsigned number)
{
    return numberToStringUnsigned<RefPtr<StringImpl>>(number);
}

RefPtr<StringImpl> makeString(long number)
{
    return numberToStringSigned<RefPtr<StringImpl>>(number);
}

RefPtr<StringImpl> makeString(unsigned long number)
{
    return numberToStringUnsigned<RefPtr<StringImpl>>(number);
}

RefPtr<StringImpl> makeString(long long number)
{
    return numberToStringSigned<RefPtr<StringImpl>>(number);
}

RefPtr<StringImpl> makeString(unsigned long long number)
{
    return numberToStringUnsigned<RefPtr<StringImpl>>(number);
}

RefPtr<StringImpl> makeString(float number)
{
    NumberToStringBuffer buffer;
    return StringImpl::create(numberToStringAndSize(number, buffer));
}

RefPtr<StringImpl> makeString(double number)
{
    NumberToStringBuffer buffer;
    return StringImpl::create(numberToStringAndSize(number, buffer));
}

const StaticString nullStringData { nullptr };
const StaticString emptyStringData { &StringImpl::s_emptyAtomString };

} // namespace WTF

#ifndef NDEBUG

// For use in the debugger.
StringImpl* string(const char*);
Vector<char> asciiDebug(StringImpl*);
Vector<char> asciiDebug(RefPtr<StringImpl>&);

StringImpl* string(const char* s)
{
    // Intentionally leaks memory!
    if (RefPtr string = StringImpl::fromLatin1(s))
        return &string.releaseNonNull().leakRef();
    return nullptr;
}

Vector<char> asciiDebug(StringImpl* impl)
{
    if (!impl)
        return asciiDebug("[null]"_si.get());

    StringBuilder buffer;
    for (unsigned i = 0; i < impl->length(); ++i) {
        char16_t ch = (*impl)[i];
        if (isASCIIPrintable(ch)) {
            if (ch == '\\')
                buffer.append(ch);
            buffer.append(ch);
        } else {
            buffer.append('\\', 'u', hex(ch, 4));
        }
    }
    return { ascii(buffer.toString()).spanIncludingNullTerminator() };
}

Vector<char> asciiDebug(RefPtr<StringImpl>& string)
{
    return asciiDebug(string.get());
}

#endif
