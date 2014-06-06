/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef StringView_h
#define StringView_h

#include <wtf/text/StringConcatenate.h>

namespace WTF {

// StringView is a non-owning reference to a string, similar to the proposed std::string_view.
// Whether the string is 8-bit or 16-bit is encoded in the upper bit of the length member.
// This means that strings longer than 2 Gigabytes can not be represented. If that turns out to be
// a problem we can investigate alternative solutions.

class StringView {
public:
    StringView()
        : m_characters(nullptr)
        , m_length(0)
    {
    }

    StringView(const LChar* characters, unsigned length)
    {
        initialize(characters, length);
    }

    StringView(const UChar* characters, unsigned length)
    {
        initialize(characters, length);
    }

    StringView(const StringImpl& string)
    {
        if (string.is8Bit())
            initialize(string.characters8(), string.length());
        else
            initialize(string.characters16(), string.length());
    }

    StringView(const String& string)
    {
        if (!string.impl()) {
            m_characters = nullptr;
            m_length = 0;
            return;
        }
        if (string.is8Bit()) {
            initialize(string.characters8(), string.length());
            return;
        }
        initialize(string.characters16(), string.length());
    }

    static StringView empty()
    {
        return StringView(reinterpret_cast<const LChar*>(""), 0);
    }

    const LChar* characters8() const
    {
        ASSERT(is8Bit());

        return static_cast<const LChar*>(m_characters);
    }

    const UChar* characters16() const
    {
        ASSERT(!is8Bit());

        return static_cast<const UChar*>(m_characters);
    }

    void getCharactersWithUpconvert(LChar*) const;
    void getCharactersWithUpconvert(UChar*) const;

    class UpconvertedCharacters {
    public:
        explicit UpconvertedCharacters(const StringView&);
        operator const UChar*() const { return m_characters; }
        const UChar* get() const { return m_characters; }
    private:
        Vector<UChar, 32> m_upconvertedCharacters;
        const UChar* m_characters;
    };
    UpconvertedCharacters upconvertedCharacters() const { return UpconvertedCharacters(*this); }

    bool isNull() const { return !m_characters; }
    bool isEmpty() const { return !length(); }
    unsigned length() const { return m_length & ~is16BitStringFlag; }

    explicit operator bool() const { return !isNull(); }

    bool is8Bit() const { return !(m_length & is16BitStringFlag); }

    StringView substring(unsigned start, unsigned length = std::numeric_limits<unsigned>::max()) const
    {
        if (start >= this->length())
            return empty();
        unsigned maxLength = this->length() - start;

        if (length >= maxLength) {
            if (!start)
                return *this;
            length = maxLength;
        }

        if (is8Bit())
            return StringView(characters8() + start, length);

        return StringView(characters16() + start, length);
    }

    String toString() const
    {
        if (is8Bit())
            return String(characters8(), length());

        return String(characters16(), length());
    }

    float toFloat(bool& isValid)
    {
        if (is8Bit())
            return charactersToFloat(characters8(), length(), &isValid);
        return charactersToFloat(characters16(), length(), &isValid);
    }

    int toInt(bool& isValid)
    {
        if (is8Bit())
            return charactersToInt(characters8(), length(), &isValid);
        return charactersToInt(characters16(), length(), &isValid);
    }

    String toStringWithoutCopying() const
    {
        if (is8Bit())
            return StringImpl::createWithoutCopying(characters8(), length());

        return StringImpl::createWithoutCopying(characters16(), length());
    }

    UChar operator[](unsigned index) const
    {
        ASSERT(index < length());
        if (is8Bit())
            return characters8()[index];
        return characters16()[index];
    }

    size_t find(UChar character, unsigned start = 0) const
    {
        if (is8Bit())
            return WTF::find(characters8(), length(), character, start);
        return WTF::find(characters16(), length(), character, start);
    }

#if USE(CF)
    // This function converts null strings to empty strings.
    WTF_EXPORT_STRING_API RetainPtr<CFStringRef> createCFStringWithoutCopying() const;
#endif

#ifdef __OBJC__
    // These functions convert null strings to empty strings.
    WTF_EXPORT_STRING_API RetainPtr<NSString> createNSString() const;
    WTF_EXPORT_STRING_API RetainPtr<NSString> createNSStringWithoutCopying() const;
#endif

private:
    void initialize(const LChar* characters, unsigned length)
    {
        ASSERT(!(length & is16BitStringFlag));
        
        m_characters = characters;
        m_length = length;
    }

    void initialize(const UChar* characters, unsigned length)
    {
        ASSERT(!(length & is16BitStringFlag));
        
        m_characters = characters;
        m_length = is16BitStringFlag | length;
    }

    static const unsigned is16BitStringFlag = 1u << 31;

    const void* m_characters;
    unsigned m_length;
};

inline void StringView::getCharactersWithUpconvert(LChar* destination) const
{
    ASSERT(is8Bit());
    memcpy(destination, characters8(), length());
}

inline void StringView::getCharactersWithUpconvert(UChar* destination) const
{
    if (is8Bit()) {
        const LChar* characters8 = this->characters8();
        unsigned length = this->length();
        for (unsigned i = 0; i < length; ++i)
            destination[i] = characters8[i];
        return;
    }
    memcpy(destination, characters16(), length() * sizeof(UChar));
}

inline StringView::UpconvertedCharacters::UpconvertedCharacters(const StringView& string)
{
    if (!string.is8Bit()) {
        m_characters = string.characters16();
        return;
    }
    const LChar* characters8 = string.characters8();
    unsigned length = string.length();
    m_upconvertedCharacters.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i)
        m_upconvertedCharacters.uncheckedAppend(characters8[i]);
    m_characters = m_upconvertedCharacters.data();
}

template<> class StringTypeAdapter<StringView> {
public:
    StringTypeAdapter<StringView>(StringView string)
        : m_string(string)
    {
    }

    unsigned length() { return m_string.length(); }
    bool is8Bit() { return m_string.is8Bit(); }
    void writeTo(LChar* destination) { m_string.getCharactersWithUpconvert(destination); }
    void writeTo(UChar* destination) { m_string.getCharactersWithUpconvert(destination); }

private:
    StringView m_string;
};

template<typename CharacterType, size_t inlineCapacity> void append(Vector<CharacterType, inlineCapacity>& buffer, StringView string)
{
    unsigned oldSize = buffer.size();
    buffer.grow(oldSize + string.length());
    string.getCharactersWithUpconvert(buffer.data() + oldSize);
}

} // namespace WTF

using WTF::append;
using WTF::StringView;

#endif // StringView_h
