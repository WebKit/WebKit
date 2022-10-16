/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSStringJoiner.h"

#include "JSCJSValueInlines.h"

namespace JSC {

JSStringJoiner::~JSStringJoiner()
{
}

template<typename CharacterType>
static inline void appendStringToData(CharacterType*& data, StringView string)
{
    string.getCharacters(data);
    data += string.length();
}

template<typename CharacterType>
static inline String joinStrings(const Vector<StringViewWithUnderlyingString>& strings, StringView separator, unsigned joinedLength)
{
    ASSERT(joinedLength);

    CharacterType* data;
    String result = StringImpl::tryCreateUninitialized(joinedLength, data);
    if (UNLIKELY(result.isNull()))
        return result;

    appendStringToData(data, strings[0].view);

    unsigned size = strings.size();

    switch (separator.length()) {
    case 0:
        for (unsigned i = 1; i < size; ++i)
            appendStringToData(data, strings[i].view);
        break;
    case 1: {
        CharacterType separatorCharacter = separator[0];
        for (unsigned i = 1; i < size; ++i) {
            *data++ = separatorCharacter;
            appendStringToData(data, strings[i].view);
        }
        break;
    }
    default:
        for (unsigned i = 1; i < size; ++i) {
            appendStringToData(data, separator);
            appendStringToData(data, strings[i].view);
        }
    }
    ASSERT(data == result.characters<CharacterType>() + joinedLength);

    return result;
}

inline unsigned JSStringJoiner::joinedLength(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned numberOfStrings = m_strings.size();
    if (!numberOfStrings)
        return 0;

    CheckedInt32 separatorLength = m_separator.length();
    CheckedInt32 totalSeparatorsLength = separatorLength * (numberOfStrings - 1);
    CheckedInt32 totalLength = totalSeparatorsLength + m_accumulatedStringsLength;
    if (totalLength.hasOverflowed()) {
        throwOutOfMemoryError(globalObject, scope);
        return 0;
    }
    return totalLength;
}

JSValue JSStringJoiner::join(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(m_strings.size() <= m_strings.capacity());

    unsigned length = joinedLength(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (!length)
        return jsEmptyString(vm);

    String result;
    if (m_isAll8Bit)
        result = joinStrings<LChar>(m_strings, m_separator, length);
    else
        result = joinStrings<UChar>(m_strings, m_separator, length);

    if (result.isNull())
        return throwOutOfMemoryError(globalObject, scope);

    return jsString(vm, WTFMove(result));
}

}
