/*
 * Copyright (C) 2010-2015 Apple Inc. All rights reserved.
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

    unsigned length() { return 1; }
    bool is8Bit() { return true; }

    void writeTo(LChar* destination) const
    {
        *destination = m_character;
    }

    void writeTo(UChar* destination) const
    {
        *destination = m_character;
    }

    String toString() const { return String(&m_character, 1); }

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

    String toString() const { return String(&m_character, 1); }

private:
    UChar m_character;
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

    String toString() const { return String(m_characters, m_length); }

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

    String toString() const { return String(m_characters, m_length); }

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

    String toString() const { return String(m_vector.data(), m_vector.size()); }

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

    String toString() const { return m_string; }

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

inline void sumWithOverflow(unsigned& total, unsigned addend, bool& overflow)
{
    unsigned oldTotal = total;
    total = oldTotal + addend;
    if (total < oldTotal)
        overflow = true;
}

template<typename StringType1, typename StringType2>
String tryMakeString(StringType1 string1, StringType2 string2)
{
    StringTypeAdapter<StringType1> adapter1(string1);
    StringTypeAdapter<StringType2> adapter2(string2);

    if (adapter1.length() && !adapter2.length())
        return adapter1.toString();
    if (!adapter1.length() && adapter2.length())
        return adapter2.toString();

    bool overflow = false;
    unsigned length = adapter1.length();
    sumWithOverflow(length, adapter2.length(), overflow);
    if (overflow)
        return String();

    if (adapter1.is8Bit() && adapter2.is8Bit()) {
        LChar* buffer;
        RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
        if (!resultImpl)
            return String();

        LChar* result = buffer;
        adapter1.writeTo(result);
        result += adapter1.length();
        adapter2.writeTo(result);

        return WTFMove(resultImpl);
    }

    UChar* buffer;
    RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
    if (!resultImpl)
        return String();

    UChar* result = buffer;
    adapter1.writeTo(result);
    result += adapter1.length();
    adapter2.writeTo(result);

    return WTFMove(resultImpl);
}

template<typename StringType1, typename StringType2, typename StringType3>
String tryMakeString(StringType1 string1, StringType2 string2, StringType3 string3)
{
    StringTypeAdapter<StringType1> adapter1(string1);
    StringTypeAdapter<StringType2> adapter2(string2);
    StringTypeAdapter<StringType3> adapter3(string3);

    bool overflow = false;
    unsigned length = adapter1.length();
    sumWithOverflow(length, adapter2.length(), overflow);
    sumWithOverflow(length, adapter3.length(), overflow);
    if (overflow)
        return String();

    if (adapter1.is8Bit() && adapter2.is8Bit() && adapter3.is8Bit()) {
        LChar* buffer;
        RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
        if (!resultImpl)
            return String();

        LChar* result = buffer;
        adapter1.writeTo(result);
        result += adapter1.length();
        adapter2.writeTo(result);
        result += adapter2.length();
        adapter3.writeTo(result);

        return WTFMove(resultImpl);
    }

    UChar* buffer = 0;
    RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
    if (!resultImpl)
        return String();

    UChar* result = buffer;
    adapter1.writeTo(result);
    result += adapter1.length();
    adapter2.writeTo(result);
    result += adapter2.length();
    adapter3.writeTo(result);

    return WTFMove(resultImpl);
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4>
String tryMakeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4)
{
    StringTypeAdapter<StringType1> adapter1(string1);
    StringTypeAdapter<StringType2> adapter2(string2);
    StringTypeAdapter<StringType3> adapter3(string3);
    StringTypeAdapter<StringType4> adapter4(string4);

    bool overflow = false;
    unsigned length = adapter1.length();
    sumWithOverflow(length, adapter2.length(), overflow);
    sumWithOverflow(length, adapter3.length(), overflow);
    sumWithOverflow(length, adapter4.length(), overflow);
    if (overflow)
        return String();

    if (adapter1.is8Bit() && adapter2.is8Bit() && adapter3.is8Bit() && adapter4.is8Bit()) {
        LChar* buffer;
        RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
        if (!resultImpl)
            return String();

        LChar* result = buffer;
        adapter1.writeTo(result);
        result += adapter1.length();
        adapter2.writeTo(result);
        result += adapter2.length();
        adapter3.writeTo(result);
        result += adapter3.length();
        adapter4.writeTo(result);

        return WTFMove(resultImpl);
    }

    UChar* buffer;
    RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
    if (!resultImpl)
        return String();

    UChar* result = buffer;
    adapter1.writeTo(result);
    result += adapter1.length();
    adapter2.writeTo(result);
    result += adapter2.length();
    adapter3.writeTo(result);
    result += adapter3.length();
    adapter4.writeTo(result);

    return WTFMove(resultImpl);
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4, typename StringType5>
String tryMakeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4, StringType5 string5)
{
    StringTypeAdapter<StringType1> adapter1(string1);
    StringTypeAdapter<StringType2> adapter2(string2);
    StringTypeAdapter<StringType3> adapter3(string3);
    StringTypeAdapter<StringType4> adapter4(string4);
    StringTypeAdapter<StringType5> adapter5(string5);

    bool overflow = false;
    unsigned length = adapter1.length();
    sumWithOverflow(length, adapter2.length(), overflow);
    sumWithOverflow(length, adapter3.length(), overflow);
    sumWithOverflow(length, adapter4.length(), overflow);
    sumWithOverflow(length, adapter5.length(), overflow);
    if (overflow)
        return String();

    if (adapter1.is8Bit() && adapter2.is8Bit() && adapter3.is8Bit() && adapter4.is8Bit() && adapter5.is8Bit()) {
        LChar* buffer;
        RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
        if (!resultImpl)
            return String();

        LChar* result = buffer;
        adapter1.writeTo(result);
        result += adapter1.length();
        adapter2.writeTo(result);
        result += adapter2.length();
        adapter3.writeTo(result);
        result += adapter3.length();
        adapter4.writeTo(result);
        result += adapter4.length();
        adapter5.writeTo(result);

        return WTFMove(resultImpl);
    }

    UChar* buffer;
    RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
    if (!resultImpl)
        return String();

    UChar* result = buffer;
    adapter1.writeTo(result);
    result += adapter1.length();
    adapter2.writeTo(result);
    result += adapter2.length();
    adapter3.writeTo(result);
    result += adapter3.length();
    adapter4.writeTo(result);
    result += adapter4.length();
    adapter5.writeTo(result);

    return WTFMove(resultImpl);
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4, typename StringType5, typename StringType6>
String tryMakeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4, StringType5 string5, StringType6 string6)
{
    StringTypeAdapter<StringType1> adapter1(string1);
    StringTypeAdapter<StringType2> adapter2(string2);
    StringTypeAdapter<StringType3> adapter3(string3);
    StringTypeAdapter<StringType4> adapter4(string4);
    StringTypeAdapter<StringType5> adapter5(string5);
    StringTypeAdapter<StringType6> adapter6(string6);

    bool overflow = false;
    unsigned length = adapter1.length();
    sumWithOverflow(length, adapter2.length(), overflow);
    sumWithOverflow(length, adapter3.length(), overflow);
    sumWithOverflow(length, adapter4.length(), overflow);
    sumWithOverflow(length, adapter5.length(), overflow);
    sumWithOverflow(length, adapter6.length(), overflow);
    if (overflow)
        return String();

    if (adapter1.is8Bit() && adapter2.is8Bit() && adapter3.is8Bit() && adapter4.is8Bit() && adapter5.is8Bit() && adapter6.is8Bit()) {
        LChar* buffer;
        RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
        if (!resultImpl)
            return String();

        LChar* result = buffer;
        adapter1.writeTo(result);
        result += adapter1.length();
        adapter2.writeTo(result);
        result += adapter2.length();
        adapter3.writeTo(result);
        result += adapter3.length();
        adapter4.writeTo(result);
        result += adapter4.length();
        adapter5.writeTo(result);
        result += adapter5.length();
        adapter6.writeTo(result);

        return WTFMove(resultImpl);
    }

    UChar* buffer;
    RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
    if (!resultImpl)
        return String();

    UChar* result = buffer;
    adapter1.writeTo(result);
    result += adapter1.length();
    adapter2.writeTo(result);
    result += adapter2.length();
    adapter3.writeTo(result);
    result += adapter3.length();
    adapter4.writeTo(result);
    result += adapter4.length();
    adapter5.writeTo(result);
    result += adapter5.length();
    adapter6.writeTo(result);

    return WTFMove(resultImpl);
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4, typename StringType5, typename StringType6, typename StringType7>
String tryMakeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4, StringType5 string5, StringType6 string6, StringType7 string7)
{
    StringTypeAdapter<StringType1> adapter1(string1);
    StringTypeAdapter<StringType2> adapter2(string2);
    StringTypeAdapter<StringType3> adapter3(string3);
    StringTypeAdapter<StringType4> adapter4(string4);
    StringTypeAdapter<StringType5> adapter5(string5);
    StringTypeAdapter<StringType6> adapter6(string6);
    StringTypeAdapter<StringType7> adapter7(string7);

    bool overflow = false;
    unsigned length = adapter1.length();
    sumWithOverflow(length, adapter2.length(), overflow);
    sumWithOverflow(length, adapter3.length(), overflow);
    sumWithOverflow(length, adapter4.length(), overflow);
    sumWithOverflow(length, adapter5.length(), overflow);
    sumWithOverflow(length, adapter6.length(), overflow);
    sumWithOverflow(length, adapter7.length(), overflow);
    if (overflow)
        return String();

    if (adapter1.is8Bit() && adapter2.is8Bit() && adapter3.is8Bit() && adapter4.is8Bit() && adapter5.is8Bit() && adapter6.is8Bit() && adapter7.is8Bit()) {
        LChar* buffer;
        RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
        if (!resultImpl)
            return String();

        LChar* result = buffer;
        adapter1.writeTo(result);
        result += adapter1.length();
        adapter2.writeTo(result);
        result += adapter2.length();
        adapter3.writeTo(result);
        result += adapter3.length();
        adapter4.writeTo(result);
        result += adapter4.length();
        adapter5.writeTo(result);
        result += adapter5.length();
        adapter6.writeTo(result);
        result += adapter6.length();
        adapter7.writeTo(result);

        return WTFMove(resultImpl);
    }

    UChar* buffer;
    RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
    if (!resultImpl)
        return String();

    UChar* result = buffer;
    adapter1.writeTo(result);
    result += adapter1.length();
    adapter2.writeTo(result);
    result += adapter2.length();
    adapter3.writeTo(result);
    result += adapter3.length();
    adapter4.writeTo(result);
    result += adapter4.length();
    adapter5.writeTo(result);
    result += adapter5.length();
    adapter6.writeTo(result);
    result += adapter6.length();
    adapter7.writeTo(result);

    return WTFMove(resultImpl);
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4, typename StringType5, typename StringType6, typename StringType7, typename StringType8>
String tryMakeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4, StringType5 string5, StringType6 string6, StringType7 string7, StringType8 string8)
{
    StringTypeAdapter<StringType1> adapter1(string1);
    StringTypeAdapter<StringType2> adapter2(string2);
    StringTypeAdapter<StringType3> adapter3(string3);
    StringTypeAdapter<StringType4> adapter4(string4);
    StringTypeAdapter<StringType5> adapter5(string5);
    StringTypeAdapter<StringType6> adapter6(string6);
    StringTypeAdapter<StringType7> adapter7(string7);
    StringTypeAdapter<StringType8> adapter8(string8);

    bool overflow = false;
    unsigned length = adapter1.length();
    sumWithOverflow(length, adapter2.length(), overflow);
    sumWithOverflow(length, adapter3.length(), overflow);
    sumWithOverflow(length, adapter4.length(), overflow);
    sumWithOverflow(length, adapter5.length(), overflow);
    sumWithOverflow(length, adapter6.length(), overflow);
    sumWithOverflow(length, adapter7.length(), overflow);
    sumWithOverflow(length, adapter8.length(), overflow);
    if (overflow)
        return String();

    if (adapter1.is8Bit() && adapter2.is8Bit() && adapter3.is8Bit() && adapter4.is8Bit() && adapter5.is8Bit() && adapter6.is8Bit() && adapter7.is8Bit() && adapter8.is8Bit()) {
        LChar* buffer;
        RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
        if (!resultImpl)
            return String();

        LChar* result = buffer;
        adapter1.writeTo(result);
        result += adapter1.length();
        adapter2.writeTo(result);
        result += adapter2.length();
        adapter3.writeTo(result);
        result += adapter3.length();
        adapter4.writeTo(result);
        result += adapter4.length();
        adapter5.writeTo(result);
        result += adapter5.length();
        adapter6.writeTo(result);
        result += adapter6.length();
        adapter7.writeTo(result);
        result += adapter7.length();
        adapter8.writeTo(result);

        return WTFMove(resultImpl);
    }

    UChar* buffer;
    RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
    if (!resultImpl)
        return String();

    UChar* result = buffer;
    adapter1.writeTo(result);
    result += adapter1.length();
    adapter2.writeTo(result);
    result += adapter2.length();
    adapter3.writeTo(result);
    result += adapter3.length();
    adapter4.writeTo(result);
    result += adapter4.length();
    adapter5.writeTo(result);
    result += adapter5.length();
    adapter6.writeTo(result);
    result += adapter6.length();
    adapter7.writeTo(result);
    result += adapter7.length();
    adapter8.writeTo(result);

    return WTFMove(resultImpl);
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4, typename StringType5, typename StringType6, typename StringType7, typename StringType8, typename StringType9>
String tryMakeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4, StringType5 string5, StringType6 string6, StringType7 string7, StringType8 string8, StringType9 string9)
{
    StringTypeAdapter<StringType1> adapter1(string1);
    StringTypeAdapter<StringType2> adapter2(string2);
    StringTypeAdapter<StringType3> adapter3(string3);
    StringTypeAdapter<StringType4> adapter4(string4);
    StringTypeAdapter<StringType5> adapter5(string5);
    StringTypeAdapter<StringType6> adapter6(string6);
    StringTypeAdapter<StringType7> adapter7(string7);
    StringTypeAdapter<StringType8> adapter8(string8);
    StringTypeAdapter<StringType9> adapter9(string9);

    bool overflow = false;
    unsigned length = adapter1.length();
    sumWithOverflow(length, adapter2.length(), overflow);
    sumWithOverflow(length, adapter3.length(), overflow);
    sumWithOverflow(length, adapter4.length(), overflow);
    sumWithOverflow(length, adapter5.length(), overflow);
    sumWithOverflow(length, adapter6.length(), overflow);
    sumWithOverflow(length, adapter7.length(), overflow);
    sumWithOverflow(length, adapter8.length(), overflow);
    sumWithOverflow(length, adapter9.length(), overflow);
    if (overflow)
        return String();

    if (adapter1.is8Bit() && adapter2.is8Bit() && adapter3.is8Bit() && adapter4.is8Bit() && adapter5.is8Bit() && adapter6.is8Bit() && adapter7.is8Bit() && adapter8.is8Bit() && adapter9.is8Bit()) {
        LChar* buffer;
        RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
        if (!resultImpl)
            return String();

        LChar* result = buffer;
        adapter1.writeTo(result);
        result += adapter1.length();
        adapter2.writeTo(result);
        result += adapter2.length();
        adapter3.writeTo(result);
        result += adapter3.length();
        adapter4.writeTo(result);
        result += adapter4.length();
        adapter5.writeTo(result);
        result += adapter5.length();
        adapter6.writeTo(result);
        result += adapter6.length();
        adapter7.writeTo(result);
        result += adapter7.length();
        adapter8.writeTo(result);
        result += adapter8.length();
        adapter9.writeTo(result);

        return WTFMove(resultImpl);
    }

    UChar* buffer;
    RefPtr<StringImpl> resultImpl = StringImpl::tryCreateUninitialized(length, buffer);
    if (!resultImpl)
        return String();

    UChar* result = buffer;
    adapter1.writeTo(result);
    result += adapter1.length();
    adapter2.writeTo(result);
    result += adapter2.length();
    adapter3.writeTo(result);
    result += adapter3.length();
    adapter4.writeTo(result);
    result += adapter4.length();
    adapter5.writeTo(result);
    result += adapter5.length();
    adapter6.writeTo(result);
    result += adapter6.length();
    adapter7.writeTo(result);
    result += adapter7.length();
    adapter8.writeTo(result);
    result += adapter8.length();
    adapter9.writeTo(result);

    return WTFMove(resultImpl);
}


// Convenience only.
template<typename StringType1>
String makeString(StringType1 string1)
{
    return String(string1);
}

template<typename StringType1, typename StringType2>
String makeString(StringType1 string1, StringType2 string2)
{
    String result = tryMakeString(string1, string2);
    if (!result)
        CRASH();
    return result;
}

template<typename StringType1, typename StringType2, typename StringType3>
String makeString(StringType1 string1, StringType2 string2, StringType3 string3)
{
    String result = tryMakeString(string1, string2, string3);
    if (!result)
        CRASH();
    return result;
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4>
String makeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4)
{
    String result = tryMakeString(string1, string2, string3, string4);
    if (!result)
        CRASH();
    return result;
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4, typename StringType5>
String makeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4, StringType5 string5)
{
    String result = tryMakeString(string1, string2, string3, string4, string5);
    if (!result)
        CRASH();
    return result;
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4, typename StringType5, typename StringType6>
String makeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4, StringType5 string5, StringType6 string6)
{
    String result = tryMakeString(string1, string2, string3, string4, string5, string6);
    if (!result)
        CRASH();
    return result;
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4, typename StringType5, typename StringType6, typename StringType7>
String makeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4, StringType5 string5, StringType6 string6, StringType7 string7)
{
    String result = tryMakeString(string1, string2, string3, string4, string5, string6, string7);
    if (!result)
        CRASH();
    return result;
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4, typename StringType5, typename StringType6, typename StringType7, typename StringType8>
String makeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4, StringType5 string5, StringType6 string6, StringType7 string7, StringType8 string8)
{
    String result = tryMakeString(string1, string2, string3, string4, string5, string6, string7, string8);
    if (!result)
        CRASH();
    return result;
}

template<typename StringType1, typename StringType2, typename StringType3, typename StringType4, typename StringType5, typename StringType6, typename StringType7, typename StringType8, typename StringType9>
String makeString(StringType1 string1, StringType2 string2, StringType3 string3, StringType4 string4, StringType5 string5, StringType6 string6, StringType7 string7, StringType8 string8, StringType9 string9)
{
    String result = tryMakeString(string1, string2, string3, string4, string5, string6, string7, string8, string9);
    if (!result)
        CRASH();
    return result;
}

} // namespace WTF

using WTF::makeString;

#include <wtf/text/StringOperators.h>
#endif
