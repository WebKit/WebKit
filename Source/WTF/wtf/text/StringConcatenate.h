/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#pragma once

#include <cstring>
#include <wtf/CheckedArithmetic.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/StringView.h>

// This macro is helpful for testing how many intermediate Strings are created while evaluating an
// expression containing operator+.
#ifndef WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING
#define WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING() ((void)0)
#endif

namespace WTF {

template<> class StringTypeAdapter<char, void> {
public:
    StringTypeAdapter(char character)
        : m_character { character }
    {
    }

    unsigned length() { return 1; }
    bool is8Bit() { return true; }
    template<typename CharacterType> void writeTo(CharacterType* destination) const { *destination = m_character; }

private:
    char m_character;
};

template<> class StringTypeAdapter<UChar, void> {
public:
    StringTypeAdapter(UChar character)
        : m_character { character }
    {
    }

    unsigned length() const { return 1; }
    bool is8Bit() const { return isLatin1(m_character); }

    void writeTo(LChar* destination) const
    {
        ASSERT(is8Bit());
        *destination = m_character;
    }

    void writeTo(UChar* destination) const { *destination = m_character; }

private:
    UChar m_character;
};

template<> class StringTypeAdapter<const LChar*, void> {
public:
    StringTypeAdapter(const LChar* characters)
        : m_characters { characters }
        , m_length { computeLength(characters) }
    {
    }

    unsigned length() const { return m_length; }
    bool is8Bit() const { return true; }
    template<typename CharacterType> void writeTo(CharacterType* destination) const { StringImpl::copyCharacters(destination, m_characters, m_length); }

private:
    static unsigned computeLength(const LChar* characters)
    {
        size_t length = std::strlen(reinterpret_cast<const char*>(characters));
        RELEASE_ASSERT(length <= String::MaxLength);
        return static_cast<unsigned>(length);
    }

    const LChar* m_characters;
    unsigned m_length;
};

template<> class StringTypeAdapter<const UChar*, void> {
public:
    StringTypeAdapter(const UChar* characters)
        : m_characters { characters }
        , m_length { computeLength(characters) }
    {
    }

    unsigned length() const { return m_length; }
    bool is8Bit() const { return !m_length; }
    void writeTo(LChar*) const { ASSERT(!m_length); }
    void writeTo(UChar* destination) const { StringImpl::copyCharacters(destination, m_characters, m_length); }

private:
    static unsigned computeLength(const UChar* characters)
    {
        size_t length = 0;
        while (characters[length])
            ++length;
        RELEASE_ASSERT(length <= String::MaxLength);
        return static_cast<unsigned>(length);
    }

    const UChar* m_characters;
    unsigned m_length;
};

template<> class StringTypeAdapter<const char*, void> : public StringTypeAdapter<const LChar*, void> {
public:
    StringTypeAdapter(const char* characters)
        : StringTypeAdapter<const LChar*, void> { reinterpret_cast<const LChar*>(characters) }
    {
    }
};

template<> class StringTypeAdapter<char*, void> : public StringTypeAdapter<const char*, void> {
public:
    StringTypeAdapter(const char* characters)
        : StringTypeAdapter<const char*, void> { characters }
    {
    }
};

template<> class StringTypeAdapter<ASCIILiteral, void> : public StringTypeAdapter<const char*, void> {
public:
    StringTypeAdapter(ASCIILiteral characters)
        : StringTypeAdapter<const char*, void> { characters }
    {
    }
};

template<> class StringTypeAdapter<Vector<char>, void> {
public:
    StringTypeAdapter(const Vector<char>& vector)
        : m_vector { vector }
    {
    }

    size_t length() const { return m_vector.size(); }
    bool is8Bit() const { return true; }
    template<typename CharacterType> void writeTo(CharacterType* destination) const { StringImpl::copyCharacters(destination, characters(), length()); }

private:
    const LChar* characters() const
    {
        return reinterpret_cast<const LChar*>(m_vector.data());
    }

    const Vector<char>& m_vector;
};

template<> class StringTypeAdapter<String, void> {
public:
    StringTypeAdapter(const String& string)
        : m_string { string }
    {
    }

    unsigned length() const { return m_string.length(); }
    bool is8Bit() const { return m_string.isNull() || m_string.is8Bit(); }
    template<typename CharacterType> void writeTo(CharacterType* destination) const
    {
        StringView { m_string }.getCharactersWithUpconvert(destination);
        WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING();
    }

private:
    const String& m_string;
};

template<> class StringTypeAdapter<AtomicString, void> : public StringTypeAdapter<String, void> {
public:
    StringTypeAdapter(const AtomicString& string)
        : StringTypeAdapter<String, void> { string.string() }
    {
    }
};

template<typename UnderlyingAdapterType> struct PaddingSpecification {
    LChar character;
    unsigned length;
    UnderlyingAdapterType underlyingAdapter;
};

template<typename StringType> PaddingSpecification<StringTypeAdapter<StringType>> pad(char character, unsigned length, StringType adapter)
{
    return { static_cast<LChar>(character), length, StringTypeAdapter<StringType> { adapter } };
}

template<typename UnderlyingAdapterType> class StringTypeAdapter<PaddingSpecification<UnderlyingAdapterType>> {
public:
    StringTypeAdapter(const PaddingSpecification<UnderlyingAdapterType>& padding)
        : m_padding { padding }
    {
    }

    unsigned length() const { return std::max(m_padding.length, m_padding.underlyingAdapter.length()); }
    bool is8Bit() const { return m_padding.underlyingAdapter.is8Bit(); }
    template<typename CharacterType> void writeTo(CharacterType* destination) const
    {
        unsigned underlyingLength = m_padding.underlyingAdapter.length();
        unsigned count = 0;
        if (underlyingLength < m_padding.length) {
            count = m_padding.length - underlyingLength;
            for (unsigned i = 0; i < count; ++i)
                destination[i] = m_padding.character;
        }
        m_padding.underlyingAdapter.writeTo(destination + count);
    }

private:
    const PaddingSpecification<UnderlyingAdapterType>& m_padding;
};

template<typename Adapter>
inline bool are8Bit(Adapter adapter)
{
    return adapter.is8Bit();
}

template<typename Adapter, typename... Adapters>
inline bool are8Bit(Adapter adapter, Adapters ...adapters)
{
    return adapter.is8Bit() && are8Bit(adapters...);
}

template<typename ResultType, typename Adapter>
inline void makeStringAccumulator(ResultType* result, Adapter adapter)
{
    adapter.writeTo(result);
}

template<typename ResultType, typename Adapter, typename... Adapters>
inline void makeStringAccumulator(ResultType* result, Adapter adapter, Adapters ...adapters)
{
    adapter.writeTo(result);
    makeStringAccumulator(result + adapter.length(), adapters...);
}

template<typename StringTypeAdapter, typename... StringTypeAdapters>
String tryMakeStringFromAdapters(StringTypeAdapter adapter, StringTypeAdapters ...adapters)
{
    static_assert(String::MaxLength == std::numeric_limits<int32_t>::max(), "");
    auto sum = checkedSum<int32_t>(adapter.length(), adapters.length()...);
    if (sum.hasOverflowed())
        return String();

    unsigned length = sum.unsafeGet();
    ASSERT(length <= String::MaxLength);
    if (are8Bit(adapter, adapters...)) {
        LChar* buffer;
        RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
        if (!resultImpl)
            return String();

        makeStringAccumulator(buffer, adapter, adapters...);

        return WTFMove(resultImpl);
    }

    UChar* buffer;
    RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
    if (!resultImpl)
        return String();

    makeStringAccumulator(buffer, adapter, adapters...);

    return WTFMove(resultImpl);
}

template<typename... StringTypes>
String tryMakeString(StringTypes ...strings)
{
    return tryMakeStringFromAdapters(StringTypeAdapter<StringTypes>(strings)...);
}

template<typename... StringTypes>
String makeString(StringTypes... strings)
{
    String result = tryMakeString(strings...);
    if (!result)
        CRASH();
    return result;
}

} // namespace WTF

using WTF::makeString;
using WTF::pad;
using WTF::tryMakeString;

#include <wtf/text/StringOperators.h>
