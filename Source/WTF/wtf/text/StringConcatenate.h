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
#include <wtf/text/AtomString.h>
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

template<> class StringTypeAdapter<AtomString, void> : public StringTypeAdapter<String, void> {
public:
    StringTypeAdapter(const AtomString& string)
        : StringTypeAdapter<String, void> { string.string() }
    {
    }
};

template<typename... StringTypes> class StringTypeAdapter<std::tuple<StringTypes...>, void> {
public:
    StringTypeAdapter(std::tuple<StringTypes...> tuple)
        : m_tuple { tuple }
        , m_length { std::apply(computeLength, tuple) }
        , m_is8Bit { std::apply(computeIs8Bit, tuple) }
    {
    }

    unsigned length() const { return m_length; }
    bool is8Bit() const { return m_is8Bit; }
    template<typename CharacterType> void writeTo(CharacterType* destination) const
    {
        std::apply([&](StringTypes... strings) {
            unsigned offset = 0;
            (..., (
                StringTypeAdapter<StringTypes>(strings).writeTo(destination + (offset * sizeof(CharacterType))),
                offset += StringTypeAdapter<StringTypes>(strings).length()
            ));
        }, m_tuple);
    }

private:
    static unsigned computeLength(StringTypes... strings)
    {
        return (... + StringTypeAdapter<StringTypes>(strings).length());
    }

    static bool computeIs8Bit(StringTypes... strings)
    {
        return (... && StringTypeAdapter<StringTypes>(strings).is8Bit());
    }

    std::tuple<StringTypes...> m_tuple;
    unsigned m_length;
    bool m_is8Bit;
};

template<typename UnderlyingElementType> struct PaddingSpecification {
    LChar character;
    unsigned length;
    UnderlyingElementType underlyingElement;
};

template<typename UnderlyingElementType> PaddingSpecification<UnderlyingElementType> pad(char character, unsigned length, UnderlyingElementType element)
{
    return { static_cast<LChar>(character), length, element };
}

template<typename UnderlyingElementType> class StringTypeAdapter<PaddingSpecification<UnderlyingElementType>> {
public:
    StringTypeAdapter(const PaddingSpecification<UnderlyingElementType>& padding)
        : m_padding { padding }
        , m_underlyingAdapter { m_padding.underlyingElement }
    {
    }

    unsigned length() const { return std::max(m_padding.length, m_underlyingAdapter.length()); }
    bool is8Bit() const { return m_underlyingAdapter.is8Bit(); }
    template<typename CharacterType> void writeTo(CharacterType* destination) const
    {
        unsigned underlyingLength = m_underlyingAdapter.length();
        unsigned count = 0;
        if (underlyingLength < m_padding.length) {
            count = m_padding.length - underlyingLength;
            for (unsigned i = 0; i < count; ++i)
                destination[i] = m_padding.character;
        }
        m_underlyingAdapter.writeTo(destination + count);
    }

private:
    const PaddingSpecification<UnderlyingElementType>& m_padding;
    StringTypeAdapter<UnderlyingElementType> m_underlyingAdapter;
};

template<unsigned N>
struct Indentation {
    unsigned operator++() { return ++value; }
    unsigned operator++(int) { return value++; }
    unsigned operator--() { return --value; }
    unsigned operator--(int) { return value--; }

    unsigned value { 0 };
};


template<unsigned N>
struct IndentationScope {
    IndentationScope(Indentation<N>& indentation)
        : m_indentation(indentation)
    {
        ++m_indentation;
    }
    ~IndentationScope()
    {
        --m_indentation;
    }

    Indentation<N>& m_indentation;
};

template<unsigned N> class StringTypeAdapter<Indentation<N>, void> {
public:
    StringTypeAdapter(Indentation<N> indentation)
        : m_indentation { indentation }
    {
    }

    unsigned length()
    {
        return m_indentation.value * N;
    }

    bool is8Bit()
    {
        return true;
    }

    template<typename CharacterType> void writeTo(CharacterType* destination)
    {
        std::fill_n(destination, m_indentation.value * N, ' ');
    }

private:
    Indentation<N> m_indentation;
};

struct ASCIICaseConverter {
    StringView::CaseConvertType type;
    StringView string;
};

inline ASCIICaseConverter lowercase(const StringView& stringView)
{
    return { StringView::CaseConvertType::Lower, stringView };
}

inline ASCIICaseConverter uppercase(const StringView& stringView)
{
    return { StringView::CaseConvertType::Upper, stringView };
}

template<> class StringTypeAdapter<ASCIICaseConverter, void> {
public:
    StringTypeAdapter(const ASCIICaseConverter& converter)
        : m_converter { converter }
    {
    }

    unsigned length() const { return m_converter.string.length(); }
    bool is8Bit() const { return m_converter.string.is8Bit(); }
    template<typename CharacterType> void writeTo(CharacterType* destination) const
    {
        m_converter.string.getCharactersWithASCIICase(m_converter.type, destination);
    }

private:
    const ASCIICaseConverter& m_converter;
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
inline void stringTypeAdapterAccumulator(ResultType* result, Adapter adapter)
{
    adapter.writeTo(result);
}

template<typename ResultType, typename Adapter, typename... Adapters>
inline void stringTypeAdapterAccumulator(ResultType* result, Adapter adapter, Adapters ...adapters)
{
    adapter.writeTo(result);
    stringTypeAdapterAccumulator(result + adapter.length(), adapters...);
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

        stringTypeAdapterAccumulator(buffer, adapter, adapters...);

        return resultImpl;
    }

    UChar* buffer;
    RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
    if (!resultImpl)
        return String();

    stringTypeAdapterAccumulator(buffer, adapter, adapters...);

    return resultImpl;
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

using WTF::Indentation;
using WTF::IndentationScope;
using WTF::makeString;
using WTF::pad;
using WTF::lowercase;
using WTF::tryMakeString;
using WTF::uppercase;

#include <wtf/text/StringOperators.h>
