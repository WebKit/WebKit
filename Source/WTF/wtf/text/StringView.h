/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#include <unicode/utypes.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/ConversionMode.h>
#include <wtf/text/LChar.h>
#include <wtf/text/StringCommon.h>

// FIXME: Enabling the StringView lifetime checking causes the MSVC build to fail. Figure out why.
// FIXME: Enable StringView lifetime checking once the underlying assertions have been fixed.
#if defined(NDEBUG) || COMPILER(MSVC) || 1
#define CHECK_STRINGVIEW_LIFETIME 0
#else
#define CHECK_STRINGVIEW_LIFETIME 1
#endif

namespace WTF {

// StringView is a non-owning reference to a string, similar to the proposed std::string_view.
// Whether the string is 8-bit or 16-bit is encoded in the upper bit of the length member.
// This means that strings longer than 2 gigacharacters cannot be represented.

class StringView {
public:
    StringView();
    ~StringView();
    StringView(StringView&&);
    StringView(const StringView&);
    StringView& operator=(StringView&&);
    StringView& operator=(const StringView&);

    StringView(const String&);
    StringView(const StringImpl&);
    StringView(const StringImpl*);
    StringView(const LChar*, unsigned length);
    StringView(const UChar*, unsigned length);

    static StringView empty();

    unsigned length() const;
    bool isEmpty() const;

    explicit operator bool() const;
    bool isNull() const;

    UChar operator[](unsigned index) const;

    class CodeUnits;
    CodeUnits codeUnits() const;

    class CodePoints;
    CodePoints codePoints() const;

    bool is8Bit() const;
    const LChar* characters8() const;
    const UChar* characters16() const;

    String toString() const;
    String toStringWithoutCopying() const;

#if USE(CF)
    // This function converts null strings to empty strings.
    WTF_EXPORT_STRING_API RetainPtr<CFStringRef> createCFStringWithoutCopying() const;
#endif

#ifdef __OBJC__
    // These functions convert null strings to empty strings.
    WTF_EXPORT_STRING_API RetainPtr<NSString> createNSString() const;
    WTF_EXPORT_STRING_API RetainPtr<NSString> createNSStringWithoutCopying() const;
#endif

    WTF_EXPORT_STRING_API CString utf8(ConversionMode = LenientConversion) const;

    class UpconvertedCharacters;
    UpconvertedCharacters upconvertedCharacters() const;

    void getCharactersWithUpconvert(LChar*) const;
    void getCharactersWithUpconvert(UChar*) const;

    StringView substring(unsigned start, unsigned length = std::numeric_limits<unsigned>::max()) const;

    size_t find(UChar, unsigned start = 0) const;

    WTF_EXPORT_STRING_API size_t find(StringView, unsigned start) const;

    WTF_EXPORT_STRING_API size_t findIgnoringASCIICase(const StringView&) const;
    WTF_EXPORT_STRING_API size_t findIgnoringASCIICase(const StringView&, unsigned startOffset) const;

    bool contains(UChar) const;
    WTF_EXPORT_STRING_API bool containsIgnoringASCIICase(const StringView&) const;
    WTF_EXPORT_STRING_API bool containsIgnoringASCIICase(const StringView&, unsigned startOffset) const;

    WTF_EXPORT_STRING_API bool startsWith(const StringView&) const;
    WTF_EXPORT_STRING_API bool startsWithIgnoringASCIICase(const StringView&) const;

    WTF_EXPORT_STRING_API bool endsWith(const StringView&) const;
    WTF_EXPORT_STRING_API bool endsWithIgnoringASCIICase(const StringView&) const;

    int toInt(bool& isValid) const;
    float toFloat(bool& isValid) const;

    static void invalidate(const StringImpl&);

    struct UnderlyingString;

private:
    void initialize(const LChar*, unsigned length);
    void initialize(const UChar*, unsigned length);

#if CHECK_STRINGVIEW_LIFETIME
    WTF_EXPORT_STRING_API bool underlyingStringIsValid() const;
    WTF_EXPORT_STRING_API void setUnderlyingString(const StringImpl*);
    WTF_EXPORT_STRING_API void setUnderlyingString(const StringView&);
#else
    bool underlyingStringIsValid() const { return true; }
    void setUnderlyingString(const StringImpl*) { }
    void setUnderlyingString(const StringView&) { }
#endif

    static const unsigned is16BitStringFlag = 1u << 31;

    const void* m_characters { nullptr };
    unsigned m_length { 0 };

#if CHECK_STRINGVIEW_LIFETIME
    void adoptUnderlyingString(UnderlyingString*);
    UnderlyingString* m_underlyingString { nullptr };
#endif
};

template<typename CharacterType, size_t inlineCapacity> void append(Vector<CharacterType, inlineCapacity>&, StringView);

bool equal(StringView, StringView);
bool equal(StringView, const LChar*);
bool equal(StringView, const char*);

bool equalIgnoringASCIICase(StringView, StringView);
bool equalIgnoringASCIICase(StringView, const char*);

template<unsigned length> bool equalLettersIgnoringASCIICase(StringView, const char (&lowercaseLetters)[length]);

inline bool operator==(StringView a, StringView b) { return equal(a, b); }
inline bool operator==(StringView a, const LChar* b) { return equal(a, b); }
inline bool operator==(StringView a, const char* b) { return equal(a, b); }
inline bool operator==(const LChar* a, StringView b) { return equal(b, a); }
inline bool operator==(const char* a, StringView b) { return equal(b, a); }

inline bool operator!=(StringView a, StringView b) { return !equal(a, b); }
inline bool operator!=(StringView a, const LChar* b) { return !equal(a, b); }
inline bool operator!=(StringView a, const char* b) { return !equal(a, b); }
inline bool operator!=(const LChar* a, StringView b) { return !equal(b, a); }
inline bool operator!=(const char* a, StringView b) { return !equal(b, a); }

}

#include <wtf/text/WTFString.h>

namespace WTF {

inline StringView::StringView()
{
    // FIXME: It's peculiar that null strings are 16-bit and empty strings return 8-bit (according to the is8Bit function).
}

inline StringView::~StringView()
{
    setUnderlyingString(nullptr);
}

inline StringView::StringView(StringView&& other)
    : m_characters(other.m_characters)
    , m_length(other.m_length)
{
    ASSERT(other.underlyingStringIsValid());

    other.m_characters = nullptr;
    other.m_length = 0;

    setUnderlyingString(other);
    other.setUnderlyingString(nullptr);
}

inline StringView::StringView(const StringView& other)
    : m_characters(other.m_characters)
    , m_length(other.m_length)
{
    ASSERT(other.underlyingStringIsValid());

    setUnderlyingString(other);
}

inline StringView& StringView::operator=(StringView&& other)
{
    ASSERT(other.underlyingStringIsValid());

    m_characters = other.m_characters;
    m_length = other.m_length;

    other.m_characters = nullptr;
    other.m_length = 0;

    setUnderlyingString(other);
    other.setUnderlyingString(nullptr);

    return *this;
}

inline StringView& StringView::operator=(const StringView& other)
{
    ASSERT(other.underlyingStringIsValid());

    m_characters = other.m_characters;
    m_length = other.m_length;

    setUnderlyingString(other);

    return *this;
}

inline void StringView::initialize(const LChar* characters, unsigned length)
{
    // FIXME: We need a better solution here, because there is no guarantee that
    // the length here won't be too long. Maybe at least a RELEASE_ASSERT?
    ASSERT(!(length & is16BitStringFlag));
    m_characters = characters;
    m_length = length;
}

inline void StringView::initialize(const UChar* characters, unsigned length)
{
    // FIXME: We need a better solution here, because there is no guarantee that
    // the length here won't be too long. Maybe at least a RELEASE_ASSERT?
    ASSERT(!(length & is16BitStringFlag));
    m_characters = characters;
    m_length = is16BitStringFlag | length;
}

inline StringView::StringView(const LChar* characters, unsigned length)
{
    initialize(characters, length);
}

inline StringView::StringView(const UChar* characters, unsigned length)
{
    initialize(characters, length);
}

inline StringView::StringView(const StringImpl& string)
{
    setUnderlyingString(&string);
    if (string.is8Bit())
        initialize(string.characters8(), string.length());
    else
        initialize(string.characters16(), string.length());
}

inline StringView::StringView(const StringImpl* string)
{
    if (!string)
        return;

    setUnderlyingString(string);
    if (string->is8Bit())
        initialize(string->characters8(), string->length());
    else
        initialize(string->characters16(), string->length());
}

inline StringView::StringView(const String& string)
{
    setUnderlyingString(string.impl());
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

inline StringView StringView::empty()
{
    return StringView(reinterpret_cast<const LChar*>(""), 0);
}

inline const LChar* StringView::characters8() const
{
    ASSERT(is8Bit());
    ASSERT(underlyingStringIsValid());
    return static_cast<const LChar*>(m_characters);
}

inline const UChar* StringView::characters16() const
{
    ASSERT(!is8Bit());
    ASSERT(underlyingStringIsValid());
    return static_cast<const UChar*>(m_characters);
}

class StringView::UpconvertedCharacters {
public:
    explicit UpconvertedCharacters(const StringView&);
    operator const UChar*() const { return m_characters; }
    const UChar* get() const { return m_characters; }
private:
    Vector<UChar, 32> m_upconvertedCharacters;
    const UChar* m_characters;
};

inline StringView::UpconvertedCharacters StringView::upconvertedCharacters() const
{
    return UpconvertedCharacters(*this);
}

inline bool StringView::isNull() const
{
    return !m_characters;
}

inline bool StringView::isEmpty() const
{
    return !length();
}

inline unsigned StringView::length() const
{
    return m_length & ~is16BitStringFlag;
}

inline StringView::operator bool() const
{
    return !isNull();
}

inline bool StringView::is8Bit() const
{
    return !(m_length & is16BitStringFlag);
}

inline StringView StringView::substring(unsigned start, unsigned length) const
{
    if (start >= this->length())
        return empty();
    unsigned maxLength = this->length() - start;

    if (length >= maxLength) {
        if (!start)
            return *this;
        length = maxLength;
    }

    if (is8Bit()) {
        StringView result(characters8() + start, length);
        result.setUnderlyingString(*this);
        return result;
    }
    StringView result(characters16() + start, length);
    result.setUnderlyingString(*this);
    return result;
}

inline UChar StringView::operator[](unsigned index) const
{
    ASSERT(index < length());
    if (is8Bit())
        return characters8()[index];
    return characters16()[index];
}

inline bool StringView::contains(UChar character) const
{
    return find(character) != notFound;
}

inline void StringView::getCharactersWithUpconvert(LChar* destination) const
{
    ASSERT(is8Bit());
    auto characters8 = this->characters8();
    for (unsigned i = 0; i < m_length; ++i)
        destination[i] = characters8[i];
}

inline void StringView::getCharactersWithUpconvert(UChar* destination) const
{
    if (is8Bit()) {
        auto characters8 = this->characters8();
        for (unsigned i = 0; i < m_length; ++i)
            destination[i] = characters8[i];
        return;
    }
    auto characters16 = this->characters16();
    unsigned length = this->length();
    for (unsigned i = 0; i < length; ++i)
        destination[i] = characters16[i];
}

inline StringView::UpconvertedCharacters::UpconvertedCharacters(const StringView& string)
{
    if (!string.is8Bit()) {
        m_characters = string.characters16();
        return;
    }
    const LChar* characters8 = string.characters8();
    unsigned length = string.m_length;
    m_upconvertedCharacters.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i)
        m_upconvertedCharacters.uncheckedAppend(characters8[i]);
    m_characters = m_upconvertedCharacters.data();
}

inline String StringView::toString() const
{
    if (is8Bit())
        return String(characters8(), m_length);
    return String(characters16(), length());
}

inline float StringView::toFloat(bool& isValid) const
{
    if (is8Bit())
        return charactersToFloat(characters8(), m_length, &isValid);
    return charactersToFloat(characters16(), length(), &isValid);
}

inline int StringView::toInt(bool& isValid) const
{
    if (is8Bit())
        return charactersToInt(characters8(), m_length, &isValid);
    return charactersToInt(characters16(), length(), &isValid);
}

inline String StringView::toStringWithoutCopying() const
{
    if (is8Bit())
        return StringImpl::createWithoutCopying(characters8(), m_length);
    return StringImpl::createWithoutCopying(characters16(), length());
}

inline size_t StringView::find(UChar character, unsigned start) const
{
    if (is8Bit())
        return WTF::find(characters8(), m_length, character, start);
    return WTF::find(characters16(), length(), character, start);
}

#if !CHECK_STRINGVIEW_LIFETIME
inline void StringView::invalidate(const StringImpl&)
{
}
#endif

template<typename StringType> class StringTypeAdapter;

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

    String toString() const { return m_string.toString(); }

private:
    StringView m_string;
};

template<typename CharacterType, size_t inlineCapacity> void append(Vector<CharacterType, inlineCapacity>& buffer, StringView string)
{
    unsigned oldSize = buffer.size();
    buffer.grow(oldSize + string.length());
    string.getCharactersWithUpconvert(buffer.data() + oldSize);
}

inline bool equal(StringView a, StringView b)
{
    return equalCommon(a, b);
}

inline bool equal(StringView a, const LChar* b)
{
    if (!b)
        return !a.isEmpty();
    if (a.isEmpty())
        return !b;
    unsigned aLength = a.length();
    if (a.is8Bit())
        return equal(a.characters8(), b, aLength);
    return equal(a.characters16(), b, aLength);
}

inline bool equal(StringView a, const char* b) 
{
    return equal(a, reinterpret_cast<const LChar*>(b)); 
}

inline bool equalIgnoringASCIICase(StringView a, StringView b)
{
    return equalIgnoringASCIICaseCommon(a, b);
}

inline bool equalIgnoringASCIICase(StringView a, const char* b)
{
    return equalIgnoringASCIICaseCommon(a, b);
}

class StringView::CodePoints {
public:
    explicit CodePoints(const StringView&);

    class Iterator;
    Iterator begin() const;
    Iterator end() const;

private:
    StringView m_stringView;
};

class StringView::CodeUnits {
public:
    explicit CodeUnits(const StringView&);

    class Iterator;
    Iterator begin() const;
    Iterator end() const;

private:
    StringView m_stringView;
};

class StringView::CodePoints::Iterator {
public:
    Iterator(const StringView&, unsigned index);

    UChar32 operator*() const;
    Iterator& operator++();

    bool operator==(const Iterator&) const;
    bool operator!=(const Iterator&) const;

private:
    const StringView& m_stringView;
    mutable unsigned m_index;
#if !ASSERT_DISABLED
    mutable bool m_alreadyIncremented { false };
#endif
};

class StringView::CodeUnits::Iterator {
public:
    Iterator(const StringView&, unsigned index);

    UChar operator*() const;
    Iterator& operator++();

    bool operator==(const Iterator&) const;
    bool operator!=(const Iterator&) const;

private:
    const StringView& m_stringView;
    unsigned m_index;
};

inline auto StringView::codePoints() const -> CodePoints
{
    return CodePoints(*this);
}

inline auto StringView::codeUnits() const -> CodeUnits
{
    return CodeUnits(*this);
}

inline StringView::CodePoints::CodePoints(const StringView& stringView)
    : m_stringView(stringView)
{
}

inline StringView::CodePoints::Iterator::Iterator(const StringView& stringView, unsigned index)
    : m_stringView(stringView)
    , m_index(index)
{
}

inline auto StringView::CodePoints::Iterator::operator++() -> Iterator&
{
#if !ASSERT_DISABLED
    ASSERT(m_alreadyIncremented);
    m_alreadyIncremented = false;
#endif
    return *this;
}

inline UChar32 StringView::CodePoints::Iterator::operator*() const
{
#if !ASSERT_DISABLED
    ASSERT(!m_alreadyIncremented);
    m_alreadyIncremented = true;
#endif

    if (m_stringView.is8Bit())
        return m_stringView.characters8()[m_index++];

    UChar32 codePoint;
    U16_NEXT(m_stringView.characters16(), m_index, m_stringView.length(), codePoint);
    return codePoint;
}

inline bool StringView::CodePoints::Iterator::operator==(const Iterator& other) const
{
    ASSERT(&m_stringView == &other.m_stringView);
    ASSERT(!m_alreadyIncremented);
    return m_index == other.m_index;
}

inline bool StringView::CodePoints::Iterator::operator!=(const Iterator& other) const
{
    return !(*this == other);
}

inline auto StringView::CodePoints::begin() const -> Iterator
{
    return Iterator(m_stringView, 0);
}

inline auto StringView::CodePoints::end() const -> Iterator
{
    return Iterator(m_stringView, m_stringView.length());
}

inline StringView::CodeUnits::CodeUnits(const StringView& stringView)
    : m_stringView(stringView)
{
}

inline StringView::CodeUnits::Iterator::Iterator(const StringView& stringView, unsigned index)
    : m_stringView(stringView)
    , m_index(index)
{
}

inline auto StringView::CodeUnits::Iterator::operator++() -> Iterator&
{
    ++m_index;
    return *this;
}

inline UChar StringView::CodeUnits::Iterator::operator*() const
{
    return m_stringView[m_index];
}

inline bool StringView::CodeUnits::Iterator::operator==(const Iterator& other) const
{
    ASSERT(&m_stringView == &other.m_stringView);
    return m_index == other.m_index;
}

inline bool StringView::CodeUnits::Iterator::operator!=(const Iterator& other) const
{
    return !(*this == other);
}

inline auto StringView::CodeUnits::begin() const -> Iterator
{
    return Iterator(m_stringView, 0);
}

inline auto StringView::CodeUnits::end() const -> Iterator
{
    return Iterator(m_stringView, m_stringView.length());
}

template<unsigned length> inline bool equalLettersIgnoringASCIICase(StringView string, const char (&lowercaseLetters)[length])
{
    return equalLettersIgnoringASCIICaseCommon(string, lowercaseLetters);
}

} // namespace WTF

using WTF::append;
using WTF::equal;
using WTF::StringView;

#endif // StringView_h
