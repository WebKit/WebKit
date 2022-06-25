/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#pragma once

#include <wtf/text/WTFString.h>

namespace WTF {

template<typename StringType1, typename StringType2> class StringAppend {
public:
    StringAppend(StringType1 string1, StringType2 string2)
        : m_string1 { string1 }
        , m_string2 { string2 }
    {
    }

    operator String() const
    {
        String result = tryMakeString(m_string1, m_string2);
        if (!result)
            CRASH();
        return result;
    }

    operator AtomString() const
    {
        return operator String();
    }

    bool is8Bit()
    {
        StringTypeAdapter<StringType1> adapter1(m_string1);
        StringTypeAdapter<StringType2> adapter2(m_string2);
        return adapter1.is8Bit() && adapter2.is8Bit();
    }

    void writeTo(LChar* destination)
    {
        ASSERT(is8Bit());
        StringTypeAdapter<StringType1> adapter1(m_string1);
        StringTypeAdapter<StringType2> adapter2(m_string2);
        adapter1.writeTo(destination);
        adapter2.writeTo(destination + adapter1.length());
    }

    void writeTo(UChar* destination)
    {
        StringTypeAdapter<StringType1> adapter1(m_string1);
        StringTypeAdapter<StringType2> adapter2(m_string2);
        adapter1.writeTo(destination);
        adapter2.writeTo(destination + adapter1.length());
    }

    unsigned length()
    {
        StringTypeAdapter<StringType1> adapter1(m_string1);
        StringTypeAdapter<StringType2> adapter2(m_string2);
        return adapter1.length() + adapter2.length();
    }    

private:
    StringType1 m_string1;
    StringType2 m_string2;
};

template<typename StringType1, typename StringType2>
class StringTypeAdapter<StringAppend<StringType1, StringType2>> {
public:
    StringTypeAdapter(StringAppend<StringType1, StringType2>& buffer)
        : m_buffer { buffer }
    {
    }

    unsigned length() const { return m_buffer.length(); }
    bool is8Bit() const { return m_buffer.is8Bit(); }
    template<typename CharacterType> void writeTo(CharacterType* destination) const { m_buffer.writeTo(destination); }

private:
    StringAppend<StringType1, StringType2>& m_buffer;
};

inline StringAppend<const char*, String> operator+(const char* string1, const String& string2)
{
    return StringAppend<const char*, String>(string1, string2);
}

inline StringAppend<const char*, AtomString> operator+(const char* string1, const AtomString& string2)
{
    return StringAppend<const char*, AtomString>(string1, string2);
}

template<typename T, typename = std::enable_if_t<std::is_same<std::decay_t<T>, StringView>::value>>
inline StringAppend<const char*, StringView> operator+(const char* string1, T string2)
{
    return StringAppend<const char*, StringView>(string1, string2);
}

template<typename U, typename V>
inline StringAppend<const char*, StringAppend<U, V>> operator+(const char* string1, const StringAppend<U, V>& string2)
{
    return StringAppend<const char*, StringAppend<U, V>>(string1, string2);
}

inline StringAppend<const UChar*, String> operator+(const UChar* string1, const String& string2)
{
    return StringAppend<const UChar*, String>(string1, string2);
}

inline StringAppend<const UChar*, AtomString> operator+(const UChar* string1, const AtomString& string2)
{
    return StringAppend<const UChar*, AtomString>(string1, string2);
}

template<typename T, typename = std::enable_if_t<std::is_same<std::decay_t<T>, StringView>::value>>
inline StringAppend<const UChar*, StringView> operator+(const UChar* string1, T string2)
{
    return StringAppend<const UChar*, StringView>(string1, string2);
}

template<typename U, typename V>
inline StringAppend<const UChar*, StringAppend<U, V>> operator+(const UChar* string1, const StringAppend<U, V>& string2)
{
    return StringAppend<const UChar*, StringAppend<U, V>>(string1, string2);
}

inline StringAppend<ASCIILiteral, String> operator+(const ASCIILiteral& string1, const String& string2)
{
    return StringAppend<ASCIILiteral, String>(string1, string2);
}

inline StringAppend<ASCIILiteral, AtomString> operator+(const ASCIILiteral& string1, const AtomString& string2)
{
    return StringAppend<ASCIILiteral, AtomString>(string1, string2);
}

template<typename T, typename = std::enable_if_t<std::is_same<std::decay_t<T>, StringView>::value>>
inline StringAppend<ASCIILiteral, StringView> operator+(const ASCIILiteral& string1, T string2)
{
    return StringAppend<ASCIILiteral, StringView>(string1, string2);
}

template<typename U, typename V>
inline StringAppend<ASCIILiteral, StringAppend<U, V>> operator+(const ASCIILiteral& string1, const StringAppend<U, V>& string2)
{
    return StringAppend<ASCIILiteral, StringAppend<U, V>>(string1, string2);
}

template<typename T>
StringAppend<String, T> operator+(const String& string1, T string2)
{
    return StringAppend<String, T>(string1, string2);
}

template<typename T, typename U, typename = std::enable_if_t<std::is_same<std::decay_t<T>, StringView>::value>>
StringAppend<StringView, U> operator+(T string1, U string2)
{
    return StringAppend<StringView, U>(string1, string2);
}

template<typename U, typename V, typename W>
StringAppend<StringAppend<U, V>, W> operator+(const StringAppend<U, V>& string1, W string2)
{
    return StringAppend<StringAppend<U, V>, W>(string1, string2);
}

} // namespace WTF
