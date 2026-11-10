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
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WTF {

const StaticAtomString nullAtomData { nullptr };
const StaticAtomString emptyAtomData { &StringImpl::s_emptyAtomString };

RefPtr<AtomStringImpl> AtomStringImpl::number(int number)
{
    return numberToStringSigned<RefPtr<AtomStringImpl>>(number);
}

RefPtr<AtomStringImpl> AtomStringImpl::number(unsigned number)
{
    return numberToStringUnsigned<RefPtr<AtomStringImpl>>(number);
}

RefPtr<AtomStringImpl> AtomStringImpl::number(unsigned long number)
{
    return numberToStringUnsigned<RefPtr<AtomStringImpl>>(number);
}

RefPtr<AtomStringImpl> AtomStringImpl::number(unsigned long long number)
{
    return numberToStringUnsigned<RefPtr<AtomStringImpl>>(number);
}

RefPtr<AtomStringImpl> AtomStringImpl::number(float number)
{
    NumberToStringBuffer buffer;
    auto span = numberToStringAndSize(number, buffer);
    return AtomStringImpl::add(byteCast<Latin1Character>(span));
}

RefPtr<AtomStringImpl> AtomStringImpl::number(double number)
{
    NumberToStringBuffer buffer;
    auto span = numberToStringAndSize(number, buffer);
    return AtomStringImpl::add(byteCast<Latin1Character>(span));
}

RefPtr<AtomStringImpl> AtomStringImpl::fromUTF8(std::span<const char> characters)
{
    if (!characters.data())
        return nullptr;
    if (characters.empty())
        return emptyAtom();
    return add(byteCast<char8_t>(characters));
}

RefPtr<AtomStringImpl> AtomStringImpl::fromUTF8(const char* characters)
{
    if (!characters)
        return nullptr;
    if (!*characters)
        return emptyAtom();
    return add(byteCast<char8_t>(unsafeSpan(characters)));
}

static inline StringBuilder replaceUnpairedSurrogatesWithReplacementCharacterInternal(StringView view)
{
    // Slow path: https://infra.spec.whatwg.org/#javascript-string-convert
    // Replaces unpaired surrogates with the replacement character.
    StringBuilder result;
    result.reserveCapacity(view.length());
    for (auto codePoint : view.codePoints()) {
        if (U_IS_SURROGATE(codePoint))
            result.append(replacementCharacter);
        else
            result.append(codePoint);
    }
    return result;
}

RefPtr<AtomStringImpl> replaceUnpairedSurrogatesWithReplacementCharacter(RefPtr<AtomStringImpl>&& string)
{
    // Fast path for the case where there are no unpaired surrogates.
    if (!hasUnpairedSurrogate(string)) [[likely]]
        return WTF::move(string);
    return replaceUnpairedSurrogatesWithReplacementCharacterInternal(string).toAtomString();
}

RefPtr<StringImpl> replaceUnpairedSurrogatesWithReplacementCharacter(RefPtr<StringImpl>&& string)
{
    // Fast path for the case where there are no unpaired surrogates.
    if (!hasUnpairedSurrogate(string)) [[likely]]
        return WTF::move(string);
    return replaceUnpairedSurrogatesWithReplacementCharacterInternal(string).toString();
}

} // namespace WTF
