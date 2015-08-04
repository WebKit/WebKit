/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef StringConcatenate_h
#define StringConcatenate_h

#include <string.h>
#include <tuple>

#ifndef AtomicString_h
#include <wtf/text/AtomicString.h>
#endif

#ifndef StringView_h
#include <wtf/text/StringView.h>
#endif

// This macro is helpful for testing how many intermediate Strings are created while evaluating an
// expression containing operator+.
#ifndef WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING
#define WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING() ((void)0)
#endif

namespace WTF {

template<typename StringType>
class StringTypeAdapter;

template<>
class StringTypeAdapter<char> {
public:
    StringTypeAdapter<char>(char character)
        : m_character(character)
    {
    }

    unsigned length() const { return 1; }
    bool is8Bit() const { return true; }

    void writeTo(LChar* destination) const
    {
        *destination = m_character;
    }

    void writeTo(UChar* destination) const
    {
        *destination = m_character;
    }

private:
    char m_character;
};

template<>
class StringTypeAdapter<UChar> {
public:
    StringTypeAdapter<UChar>(UChar character)
        : m_character(character)
    {
    }

    unsigned length() const { return 1; }
    bool is8Bit() const { return m_character <= 0xff; }

    void writeTo(LChar* destination) const
    {
        ASSERT(is8Bit());
        *destination = static_cast<LChar>(m_character);
    }

    void writeTo(UChar* destination) const
    {
        *destination = m_character;
    }

private:
    UChar m_character;
};

template<size_t stringLiteralLength>
class StringTypeAdapter<char[stringLiteralLength]> {
public:
    StringTypeAdapter(const char (&stringLiteral)[stringLiteralLength])
        : m_string(reinterpret_cast<const LChar*>(stringLiteral))
    {
    }

    unsigned length() const { return stringLiteralLength - 1; }
    bool is8Bit() const { return true; }

    void writeTo(LChar* destination) const
    {
        StringView(m_string, length()).getCharactersWithUpconvert(destination);
    }

    void writeTo(UChar* destination) const
    {
        StringView(m_string, length()).getCharactersWithUpconvert(destination);
    }

private:
    const LChar* m_string;
};

template<size_t stringLiteralLength>
class StringTypeAdapter<const char[stringLiteralLength]> : public StringTypeAdapter<char[stringLiteralLength]> {
public:
    StringTypeAdapter(const char (&stringLiteral)[stringLiteralLength])
        : StringTypeAdapter<char[stringLiteralLength]>(stringLiteral)
    {
    }
};

template<size_t stringLiteralLength>
class StringTypeAdapter<UChar[stringLiteralLength]> {
public:
    StringTypeAdapter(const UChar (&stringLiteral)[stringLiteralLength])
        : m_string(reinterpret_cast<const UChar*>(stringLiteral))
    {
    }

    unsigned length() const { return stringLiteralLength - 1; }
    bool is8Bit() const { return false; }

    NO_RETURN_DUE_TO_CRASH void writeTo(LChar*) const
    {
        CRASH();
    }

    void writeTo(UChar* destination) const
    {
        StringView(m_string, length()).getCharactersWithUpconvert(destination);
    }

private:
    const UChar* m_string;
};

template<size_t stringLiteralLength>
class StringTypeAdapter<const UChar[stringLiteralLength]> : public StringTypeAdapter<UChar[stringLiteralLength]> {
public:
    StringTypeAdapter(const UChar (&stringLiteral)[stringLiteralLength])
        : StringTypeAdapter<UChar[stringLiteralLength]>(stringLiteral)
    {
    }
};

template<>
class StringTypeAdapter<const LChar*> {
public:
    StringTypeAdapter(const LChar* characters)
        : m_characters(characters)
        , m_length(strlen(reinterpret_cast<const char*>(characters)))
    {
    }

    unsigned length() const { return m_length; }
    bool is8Bit() const { return true; }

    void writeTo(LChar* destination) const
    {
        StringView(m_characters, m_length).getCharactersWithUpconvert(destination);
    }

    void writeTo(UChar* destination) const
    {
        StringView(m_characters, m_length).getCharactersWithUpconvert(destination);
    }

private:
    const LChar* m_characters;
    unsigned m_length;
};

template<>
class StringTypeAdapter<const UChar*> {
public:
    StringTypeAdapter(const UChar* characters)
        : m_characters(characters)
    {
        unsigned length = 0;
        while (m_characters[length])
            ++length;

        if (length > std::numeric_limits<unsigned>::max())
            CRASH();

        m_length = length;
    }

    unsigned length() const { return m_length; }
    bool is8Bit() const { return false; }

    NO_RETURN_DUE_TO_CRASH void writeTo(LChar*) const
    {
        CRASH();
    }

    void writeTo(UChar* destination) const
    {
        memcpy(destination, m_characters, m_length * sizeof(UChar));
    }

private:
    const UChar* m_characters;
    unsigned m_length;
};

template<>
class StringTypeAdapter<const char*> : public StringTypeAdapter<const LChar*> {
public:
    StringTypeAdapter(const char* characters)
        : StringTypeAdapter<const LChar*>(reinterpret_cast<const LChar*>(characters))
    {
    }
};

template<>
class StringTypeAdapter<char*> : public StringTypeAdapter<const char*> {
public:
    StringTypeAdapter(const char* characters)
        : StringTypeAdapter<const char*>(characters)
    {
    }
};

template<>
class StringTypeAdapter<ASCIILiteral> : public StringTypeAdapter<const char*> {
public:
    StringTypeAdapter(ASCIILiteral characters)
        : StringTypeAdapter<const char*>(characters)
    {
    }
};

template<>
class StringTypeAdapter<Vector<char>> {
public:
    StringTypeAdapter(const Vector<char>& vector)
        : m_vector(vector)
    {
    }

    size_t length() const { return m_vector.size(); }
    bool is8Bit() const { return true; }

    void writeTo(LChar* destination) const
    {
        StringView(reinterpret_cast<const LChar*>(m_vector.data()), m_vector.size()).getCharactersWithUpconvert(destination);
    }

    void writeTo(UChar* destination) const
    {
        StringView(reinterpret_cast<const LChar*>(m_vector.data()), m_vector.size()).getCharactersWithUpconvert(destination);
    }

private:
    const Vector<char>& m_vector;
};

template<>
class StringTypeAdapter<String> {
public:
    StringTypeAdapter<String>(const String& string)
        : m_string(string)
    {
    }

    unsigned length() const { return m_string.length(); }
    bool is8Bit() const { return m_string.isNull() || m_string.is8Bit(); }

    void writeTo(LChar* destination) const
    {
        StringView(m_string).getCharactersWithUpconvert(destination);
        WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING();
    }

    void writeTo(UChar* destination) const
    {
        StringView(m_string).getCharactersWithUpconvert(destination);
        WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING();
    }

private:
    const String& m_string;
};

template<>
class StringTypeAdapter<AtomicString> : public StringTypeAdapter<String> {
public:
    StringTypeAdapter(const AtomicString& string)
        : StringTypeAdapter<String>(string.string())
    {
    }
};

template<size_t index, typename... Adapters>
struct StringAdapterTuple {
    static inline Checked<unsigned, RecordOverflow> sumWithOverflow(const std::tuple<Adapters...>& tuple)
    {
        auto& adapter = std::get<sizeof...(Adapters) - index>(tuple);
        return Checked<unsigned, RecordOverflow>(adapter.length()) + StringAdapterTuple<index - 1, Adapters...>::sumWithOverflow(tuple);
    }

    static inline bool is8Bit(const std::tuple<Adapters...>& tuple)
    {
        auto& adapter = std::get<sizeof...(Adapters) - index>(tuple);
        return adapter.is8Bit() && StringAdapterTuple<index - 1, Adapters...>::is8Bit(tuple);
    }

    template<typename T>
    static inline void writeTo(T* buffer, const std::tuple<Adapters...>& tuple)
    {
        auto& adapter = std::get<sizeof...(Adapters) - index>(tuple);
        adapter.writeTo(buffer);
        StringAdapterTuple<index - 1, Adapters...>::writeTo(buffer + adapter.length(), tuple);
    }

    static inline RefPtr<StringImpl> createString(const std::tuple<Adapters...>& adapters)
    {
        unsigned length = 0;
        if (sumWithOverflow(adapters).safeGet(length) == CheckedState::DidOverflow)
            return nullptr;

        if (is8Bit(adapters)) {
            LChar* buffer;
            RefPtr<StringImpl> result = StringImpl::tryCreateUninitialized(length, buffer);
            if (!result)
                return nullptr;

            writeTo(buffer, adapters);
            return result;
        }

        UChar* buffer;
        RefPtr<StringImpl> result = StringImpl::tryCreateUninitialized(length, buffer);
        if (!result)
            return nullptr;

        writeTo(buffer, adapters);
        return result;
    }
};

template<typename... Adapters>
struct StringAdapterTuple<0, Adapters...> {
    static Checked<unsigned, RecordOverflow> sumWithOverflow(const std::tuple<Adapters...>&) { return 0; }
    static bool is8Bit(const std::tuple<Adapters...>&) { return true; }
    template<typename T> static void writeTo(T*, const std::tuple<Adapters...>&) { }
};

template<typename T>
using AdapterType = StringTypeAdapter<typename std::decay<T>::type>;

template<typename T1, typename T2, typename... Ts>
RefPtr<StringImpl> tryMakeString(const T1& string1, const T2& string2, const Ts&... strings)
{
    auto adapters = std::make_tuple(AdapterType<T1>(string1), AdapterType<T2>(string2), AdapterType<Ts>(strings)...);
    return StringAdapterTuple<2 + sizeof...(Ts), AdapterType<T1>, AdapterType<T2>, AdapterType<Ts>...>::createString(adapters);
}

// Convenience only.
template<typename T>
String makeString(T&& string)
{
    return std::forward<T>(string);
}

template<typename T1, typename T2, typename... Ts>
String makeString(const T1& string1, const T2& string2, const Ts&... strings)
{
    RefPtr<StringImpl> result = tryMakeString(string1, string2, strings...);
    if (!result)
        CRASH();
    return WTF::move(result);
}

} // namespace WTF

using WTF::makeString;

#include <wtf/text/StringOperators.h>
#endif
