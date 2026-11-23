/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include <wtf/text/AtomString.h>

#include <wtf/dtoa.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/StringCommon.h>

namespace WTF {

const StaticAtomString nullAtomData { nullptr };
const StaticAtomString emptyAtomData { &StringImpl::s_emptyAtomString };

template<AtomString::CaseConvertType type>
ALWAYS_INLINE AtomString AtomString::convertASCIICase() const
{
    StringImpl* impl = this->impl();
    if (!impl) [[unlikely]]
        return nullAtom();

    // Convert short strings without allocating a new StringImpl, since
    // there's a good chance these strings are already in the atom
    // string table and so no memory allocation will be required.
    unsigned length;
    const unsigned localBufferSize = 100;
    if (impl->is8Bit() && (length = impl->length()) <= localBufferSize) {
        auto characters = impl->span8();
        unsigned failingIndex;
        for (unsigned i = 0; i < length; ++i) {
            if constexpr (type == CaseConvertType::Lower) {
                if (isASCIIUpper(characters[i])) [[unlikely]] {
                    failingIndex = i;
                    goto SlowPath;
                }
            } else {
                if (isASCIILower(characters[i])) [[likely]] {
                    failingIndex = i;
                    goto SlowPath;
                }
            }
        }
        return *this;
SlowPath:
        std::array<Latin1Character, localBufferSize> localBuffer;
        std::span<Latin1Character> localSpan { localBuffer };
        StringImpl::copyCharacters(localSpan, characters.first(failingIndex));
        for (unsigned i = failingIndex; i < length; ++i)
            localBuffer[i] = type == CaseConvertType::Lower ? toASCIILower(characters[i]) : toASCIIUpper(characters[i]);
        return std::span<const Latin1Character> { localBuffer }.first(length);
    }

    Ref<StringImpl> convertedString = type == CaseConvertType::Lower ? impl->convertToASCIILowercase() : impl->convertToASCIIUppercase();
    if (convertedString.ptr() == impl) [[likely]]
        return *this;

    AtomString result;
    result.m_string = AtomStringImpl::add(convertedString.ptr());
    return result;
}

AtomString AtomString::convertToASCIILowercase() const
{
    return convertASCIICase<CaseConvertType::Lower>();
}

AtomString AtomString::convertToASCIIUppercase() const
{
    return convertASCIICase<CaseConvertType::Upper>();
}

AtomString AtomString::number(int number)
{
    return numberToStringSigned<AtomString>(number);
}

AtomString AtomString::number(unsigned number)
{
    return numberToStringUnsigned<AtomString>(number);
}

AtomString AtomString::number(unsigned long number)
{
    return numberToStringUnsigned<AtomString>(number);
}

AtomString AtomString::number(unsigned long long number)
{
    return numberToStringUnsigned<AtomString>(number);
}

AtomString AtomString::number(float number)
{
    NumberToStringBuffer buffer;
    auto span = numberToStringAndSize(number, buffer);
    return AtomString { byteCast<Latin1Character>(span) };
}

AtomString AtomString::number(double number)
{
    NumberToStringBuffer buffer;
    auto span = numberToStringAndSize(number, buffer);
    return AtomString { byteCast<Latin1Character>(span) };
}

AtomString AtomString::fromUTF8Internal(std::span<const char> characters)
{
    ASSERT(!characters.empty());
    return AtomStringImpl::add(byteCast<char8_t>(characters));
}

#ifndef NDEBUG

void AtomString::show() const
{
    m_string.show();
}

#endif

static String toWellFormedString(std::span<const char16_t> characters)
{
    std::span<char16_t> buffer;
    auto result = String::createUninitialized(characters.size(), buffer);
    toWellFormedUTF16(characters, buffer);
    return result;
}

// https://infra.spec.whatwg.org/#javascript-string-convert
String replaceUnpairedSurrogatesWithReplacementCharacterIn16BitString(String&& string)
{
    auto characters = string.span16();
    if (isWellFormedUTF16(characters)) [[likely]]
        return WTF::move(string);
    return toWellFormedString(characters);
}

AtomString replaceUnpairedSurrogatesWithReplacementCharacterIn16BitString(AtomString&& string)
{
    auto characters = string.span16();
    if (isWellFormedUTF16(characters)) [[likely]]
        return WTF::move(string);
    return AtomString { toWellFormedString(characters) };
}

} // namespace WTF
