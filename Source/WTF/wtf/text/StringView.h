/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#pragma once

#include <limits.h>
#include <unicode/utypes.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/CString.h>
#include <wtf/text/ConversionMode.h>
#include <wtf/text/LChar.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/UTF8ConversionError.h>

// FIXME: Enabling the StringView lifetime checking causes the MSVC build to fail. Figure out why.
#if defined(NDEBUG) || COMPILER(MSVC)
#define CHECK_STRINGVIEW_LIFETIME 0
#else
#define CHECK_STRINGVIEW_LIFETIME 1
#endif

namespace WTF {

// StringView is a non-owning reference to a string, similar to the proposed std::string_view.

class StringView final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StringView();
#if CHECK_STRINGVIEW_LIFETIME
    ~StringView();
    StringView(StringView&&);
    StringView(const StringView&);
    StringView& operator=(StringView&&);
    StringView& operator=(const StringView&);
#endif

    StringView(const AtomString&);
    StringView(const String&);
    StringView(const StringImpl&);
    StringView(const StringImpl*);
    StringView(const LChar*, unsigned length);
    StringView(const UChar*, unsigned length);
    StringView(const char*, unsigned length);
    StringView(ASCIILiteral);
    ALWAYS_INLINE StringView(Span<const LChar> characters) : StringView(characters.data(), characters.size()) { }
    ALWAYS_INLINE StringView(Span<const UChar> characters) : StringView(characters.data(), characters.size()) { }

    ALWAYS_INLINE static StringView fromLatin1(const char* characters) { return StringView { characters }; }

    static StringView empty();

    unsigned length() const;
    bool isEmpty() const;

    explicit operator bool() const;
    bool isNull() const;

    UChar characterAt(unsigned index) const;
    UChar operator[](unsigned index) const;

    class CodeUnits;
    CodeUnits codeUnits() const;

    class CodePoints;
    CodePoints codePoints() const;

    class GraphemeClusters;
    GraphemeClusters graphemeClusters() const;

    bool is8Bit() const;
    const LChar* characters8() const;
    const UChar* characters16() const;
    Span<const LChar> span8() const { return { characters8(), length() }; }
    Span<const UChar> span16() const { return { characters16(), length() }; }

    unsigned hash() const;

    // Return characters8() or characters16() depending on CharacterType.
    template<typename CharacterType> const CharacterType* characters() const;

    bool isAllASCII() const;

    String toString() const;
    String toStringWithoutCopying() const;
    AtomString toAtomString() const;
    AtomString toExistingAtomString() const;

#if USE(CF)
    // These functions convert null strings to empty strings.
    WTF_EXPORT_PRIVATE RetainPtr<CFStringRef> createCFString() const;
    WTF_EXPORT_PRIVATE RetainPtr<CFStringRef> createCFStringWithoutCopying() const;
#endif

#ifdef __OBJC__
    // These functions convert null strings to empty strings.
    WTF_EXPORT_PRIVATE RetainPtr<NSString> createNSString() const;
    WTF_EXPORT_PRIVATE RetainPtr<NSString> createNSStringWithoutCopying() const;
#endif

    WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> tryGetUTF8(ConversionMode = LenientConversion) const;
    WTF_EXPORT_PRIVATE CString utf8(ConversionMode = LenientConversion) const;

    template<typename Func>
    Expected<std::invoke_result_t<Func, Span<const char>>, UTF8ConversionError> tryGetUTF8(const Func&, ConversionMode = LenientConversion) const;

    class UpconvertedCharacters;
    UpconvertedCharacters upconvertedCharacters() const;

    template<typename CharacterType> void getCharacters(CharacterType*) const;
    template<typename CharacterType> void getCharacters8(CharacterType*) const;
    template<typename CharacterType> void getCharacters16(CharacterType*) const;

    enum class CaseConvertType { Upper, Lower };
    WTF_EXPORT_PRIVATE void getCharactersWithASCIICase(CaseConvertType, LChar*) const;
    WTF_EXPORT_PRIVATE void getCharactersWithASCIICase(CaseConvertType, UChar*) const;

    StringView substring(unsigned start, unsigned length = std::numeric_limits<unsigned>::max()) const;
    StringView left(unsigned length) const { return substring(0, length); }
    StringView right(unsigned length) const { return substring(this->length() - length, length); }

    template<typename MatchedCharacterPredicate>
    StringView stripLeadingAndTrailingMatchedCharacters(const MatchedCharacterPredicate&) const;
    WTF_EXPORT_PRIVATE StringView stripWhiteSpace() const;

    class SplitResult;
    SplitResult split(UChar) const;
    SplitResult splitAllowingEmptyEntries(UChar) const;

    size_t find(UChar, unsigned start = 0) const;
    template<typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, UChar>>* = nullptr>
    size_t find(CodeUnitMatchFunction&&, unsigned start = 0) const;
    ALWAYS_INLINE size_t find(ASCIILiteral literal, unsigned start = 0) const { return find(literal.characters8(), literal.length(), start); }
    WTF_EXPORT_PRIVATE size_t find(StringView, unsigned start = 0) const;

    size_t reverseFind(UChar, unsigned index = std::numeric_limits<unsigned>::max()) const;
    ALWAYS_INLINE size_t reverseFind(ASCIILiteral literal, unsigned start = std::numeric_limits<unsigned>::max()) const { return reverseFind(literal.characters8(), literal.length(), start); }
    WTF_EXPORT_PRIVATE size_t reverseFind(StringView, unsigned start = std::numeric_limits<unsigned>::max()) const;

    WTF_EXPORT_PRIVATE size_t findIgnoringASCIICase(StringView) const;
    WTF_EXPORT_PRIVATE size_t findIgnoringASCIICase(StringView, unsigned start) const;

    WTF_EXPORT_PRIVATE String convertToASCIILowercase() const;
    WTF_EXPORT_PRIVATE String convertToASCIIUppercase() const;
    WTF_EXPORT_PRIVATE AtomString convertToASCIILowercaseAtom() const;

    bool contains(UChar) const;
    template<typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, UChar>>* = nullptr>
    bool contains(CodeUnitMatchFunction&&) const;
    bool contains(ASCIILiteral literal) const { return find(literal) != notFound; }
    bool contains(StringView string) const { return find(string) != notFound; }

    WTF_EXPORT_PRIVATE bool containsIgnoringASCIICase(StringView) const;
    WTF_EXPORT_PRIVATE bool containsIgnoringASCIICase(StringView, unsigned start) const;

    template<bool isSpecialCharacter(UChar)> bool isAllSpecialCharacters() const;

    WTF_EXPORT_PRIVATE bool startsWith(UChar) const;
    WTF_EXPORT_PRIVATE bool startsWith(StringView) const;
    WTF_EXPORT_PRIVATE bool startsWithIgnoringASCIICase(StringView) const;

    WTF_EXPORT_PRIVATE bool endsWith(UChar) const;
    WTF_EXPORT_PRIVATE bool endsWith(StringView) const;
    WTF_EXPORT_PRIVATE bool endsWithIgnoringASCIICase(StringView) const;

    float toFloat(bool& isValid) const;
    double toDouble(bool& isValid) const;

    static void invalidate(const StringImpl&);

    struct UnderlyingString;

private:
    // Clients should use StringView(ASCIILiteral) or StringView::fromLatin1() instead.
    explicit StringView(const char*);

    friend bool equal(StringView, StringView);
    friend bool equal(StringView, StringView, unsigned length);
    friend WTF_EXPORT_PRIVATE bool equalRespectingNullity(StringView, StringView);

    void initialize(const LChar*, unsigned length);
    void initialize(const UChar*, unsigned length);

    WTF_EXPORT_PRIVATE size_t find(const LChar* match, unsigned matchLength, unsigned start) const;
    WTF_EXPORT_PRIVATE size_t reverseFind(const LChar* match, unsigned matchLength, unsigned start) const;

    template<typename CharacterType, typename MatchedCharacterPredicate>
    StringView stripLeadingAndTrailingMatchedCharacters(const CharacterType*, const MatchedCharacterPredicate&) const;

    WTF_EXPORT_PRIVATE bool underlyingStringIsValidImpl() const;
    WTF_EXPORT_PRIVATE void setUnderlyingStringImpl(const StringImpl*);
    WTF_EXPORT_PRIVATE void setUnderlyingStringImpl(const StringView&);

#if CHECK_STRINGVIEW_LIFETIME
    bool underlyingStringIsValid() const { return underlyingStringIsValidImpl(); }
    void setUnderlyingString(const StringImpl* stringImpl) { setUnderlyingStringImpl(stringImpl); }
    void setUnderlyingString(const StringView& stringView) { setUnderlyingStringImpl(stringView); }
    void adoptUnderlyingString(UnderlyingString*);
#else
    bool underlyingStringIsValid() const { return true; }
    void setUnderlyingString(const StringImpl*) { }
    void setUnderlyingString(const StringView&) { }
#endif

    void clear();

    const void* m_characters { nullptr };
    unsigned m_length { 0 };
    bool m_is8Bit { true };

#if CHECK_STRINGVIEW_LIFETIME
    UnderlyingString* m_underlyingString { nullptr };
#endif
};

template<typename CharacterType, size_t inlineCapacity> void append(Vector<CharacterType, inlineCapacity>&, StringView);

bool equal(StringView, StringView);
bool equal(StringView, const LChar* b);

bool equalIgnoringASCIICase(StringView, StringView);
bool equalIgnoringASCIICase(StringView, ASCIILiteral);

WTF_EXPORT_PRIVATE bool equalRespectingNullity(StringView, StringView);
bool equalIgnoringNullity(StringView, StringView);

bool equalLettersIgnoringASCIICase(StringView, ASCIILiteral);
bool startsWithLettersIgnoringASCIICase(StringView, ASCIILiteral);

inline bool operator==(StringView a, StringView b) { return equal(a, b); }
inline bool operator==(StringView a, ASCIILiteral b) { return equal(a, b); }
inline bool operator==(ASCIILiteral a, StringView b) { return equal(b, a); }

inline bool operator!=(StringView a, StringView b) { return !equal(a, b); }
inline bool operator!=(StringView a, ASCIILiteral b) { return !equal(a, b); }
inline bool operator!=(ASCIILiteral a, StringView b) { return !equal(b, a); }

struct StringViewWithUnderlyingString;

// This returns a StringView of the normalized result, and a String that is either
// null, if the input was already normalized, or contains the normalized result
// and needs to be kept around so the StringView remains valid. Typically the
// easiest way to use it correctly is to put it into a local and use the StringView.
WTF_EXPORT_PRIVATE StringViewWithUnderlyingString normalizedNFC(StringView);

WTF_EXPORT_PRIVATE String normalizedNFC(const String&);

}

#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

namespace WTF {

struct StringViewWithUnderlyingString {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    StringView view;
    String underlyingString;
};

inline StringView::StringView()
{
}

#if CHECK_STRINGVIEW_LIFETIME

inline StringView::~StringView()
{
    setUnderlyingString(nullptr);
}

inline StringView::StringView(StringView&& other)
    : m_characters(other.m_characters)
    , m_length(other.m_length)
    , m_is8Bit(other.m_is8Bit)
{
    ASSERT(other.underlyingStringIsValid());

    other.clear();

    setUnderlyingString(other);
    other.setUnderlyingString(nullptr);
}

inline StringView::StringView(const StringView& other)
    : m_characters(other.m_characters)
    , m_length(other.m_length)
    , m_is8Bit(other.m_is8Bit)
{
    ASSERT(other.underlyingStringIsValid());

    setUnderlyingString(other);
}

inline StringView& StringView::operator=(StringView&& other)
{
    ASSERT(other.underlyingStringIsValid());

    m_characters = other.m_characters;
    m_length = other.m_length;
    m_is8Bit = other.m_is8Bit;

    other.clear();

    setUnderlyingString(other);
    other.setUnderlyingString(nullptr);

    return *this;
}

inline StringView& StringView::operator=(const StringView& other)
{
    ASSERT(other.underlyingStringIsValid());

    m_characters = other.m_characters;
    m_length = other.m_length;
    m_is8Bit = other.m_is8Bit;

    setUnderlyingString(other);

    return *this;
}

#endif // CHECK_STRINGVIEW_LIFETIME

inline void StringView::initialize(const LChar* characters, unsigned length)
{
    m_characters = characters;
    m_length = length;
    m_is8Bit = true;
}

inline void StringView::initialize(const UChar* characters, unsigned length)
{
    m_characters = characters;
    m_length = length;
    m_is8Bit = false;
}

inline StringView::StringView(const LChar* characters, unsigned length)
{
    initialize(characters, length);
}

inline StringView::StringView(const UChar* characters, unsigned length)
{
    initialize(characters, length);
}

inline StringView::StringView(const char* characters)
{
    initialize(reinterpret_cast<const LChar*>(characters), characters ? strlen(characters) : 0);
}

inline StringView::StringView(const char* characters, unsigned length)
{
    initialize(reinterpret_cast<const LChar*>(characters), length);
}

inline StringView::StringView(ASCIILiteral string)
{
    initialize(string.characters8(), string.length());
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
        clear();
        return;
    }
    if (string.is8Bit()) {
        initialize(string.characters8(), string.length());
        return;
    }
    initialize(string.characters16(), string.length());
}

inline StringView::StringView(const AtomString& atomString)
    : StringView(atomString.string())
{
}

inline void StringView::clear()
{
    m_characters = nullptr;
    m_length = 0;
    m_is8Bit = true;
}

inline StringView StringView::empty()
{
    return StringView("", 0);
}

inline const LChar* StringView::characters8() const
{
    ASSERT(is8Bit());
    ASSERT(underlyingStringIsValid());
    return static_cast<const LChar*>(m_characters);
}

inline const UChar* StringView::characters16() const
{
    ASSERT(!is8Bit() || isEmpty());
    ASSERT(underlyingStringIsValid());
    return static_cast<const UChar*>(m_characters);
}

inline unsigned StringView::hash() const
{
    if (is8Bit())
        return StringHasher::computeHashAndMaskTop8Bits(characters8(), length());
    return StringHasher::computeHashAndMaskTop8Bits(characters16(), length());
}

template<> ALWAYS_INLINE const LChar* StringView::characters<LChar>() const
{
    return characters8();
}

template<> ALWAYS_INLINE const UChar* StringView::characters<UChar>() const
{
    return characters16();
}

inline bool StringView::isAllASCII() const
{
    if (is8Bit())
        return charactersAreAllASCII(characters8(), length());
    return charactersAreAllASCII(characters16(), length());
}

class StringView::UpconvertedCharacters {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit UpconvertedCharacters(StringView);
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
    return m_length;
}

inline StringView::operator bool() const
{
    return !isNull();
}

inline bool StringView::is8Bit() const
{
    return m_is8Bit;
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

inline UChar StringView::characterAt(unsigned index) const
{
    ASSERT(index < length());
    if (is8Bit())
        return characters8()[index];
    return characters16()[index];
}

inline UChar StringView::operator[](unsigned index) const
{
    return characterAt(index);
}

inline bool StringView::contains(UChar character) const
{
    return find(character) != notFound;
}

template<typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, UChar>>*>
inline bool StringView::contains(CodeUnitMatchFunction&& function) const
{
    return find(std::forward<CodeUnitMatchFunction>(function)) != notFound;
}

template<bool isSpecialCharacter(UChar)> inline bool StringView::isAllSpecialCharacters() const
{
    if (is8Bit())
        return WTF::isAllSpecialCharacters<isSpecialCharacter>(characters8(), length());
    return WTF::isAllSpecialCharacters<isSpecialCharacter>(characters16(), length());
}

template<typename CharacterType> inline void StringView::getCharacters8(CharacterType* destination) const
{
    StringImpl::copyCharacters(destination, characters8(), m_length);
}

template<typename CharacterType> inline void StringView::getCharacters16(CharacterType* destination) const
{
    StringImpl::copyCharacters(destination, characters16(), m_length);
}

template<typename CharacterType> inline void StringView::getCharacters(CharacterType* destination) const
{
    if (is8Bit())
        getCharacters8(destination);
    else
        getCharacters16(destination);
}

inline StringView::UpconvertedCharacters::UpconvertedCharacters(StringView string)
{
    if (!string.is8Bit()) {
        m_characters = string.characters16();
        return;
    }
    const LChar* characters8 = string.characters8();
    unsigned length = string.m_length;
    m_upconvertedCharacters.resize(length);
    StringImpl::copyCharacters(m_upconvertedCharacters.data(), characters8, length);
    m_characters = m_upconvertedCharacters.data();
}

inline String StringView::toString() const
{
    if (is8Bit())
        return String(characters8(), m_length);
    return String(characters16(), m_length);
}

inline AtomString StringView::toAtomString() const
{
    if (is8Bit())
        return AtomString(characters8(), m_length);
    return AtomString(characters16(), m_length);
}

inline AtomString StringView::toExistingAtomString() const
{
    if (is8Bit())
        return AtomStringImpl::lookUp(characters8(), m_length);
    return AtomStringImpl::lookUp(characters16(), m_length);
}

inline float StringView::toFloat(bool& isValid) const
{
    if (is8Bit())
        return charactersToFloat(characters8(), m_length, &isValid);
    return charactersToFloat(characters16(), m_length, &isValid);
}

inline double StringView::toDouble(bool& isValid) const
{
    if (is8Bit())
        return charactersToDouble(characters8(), m_length, &isValid);
    return charactersToDouble(characters16(), m_length, &isValid);
}

inline String StringView::toStringWithoutCopying() const
{
    if (is8Bit())
        return StringImpl::createWithoutCopying(characters8(), m_length);
    return StringImpl::createWithoutCopying(characters16(), m_length);
}

inline size_t StringView::find(UChar character, unsigned start) const
{
    if (is8Bit())
        return WTF::find(characters8(), m_length, character, start);
    return WTF::find(characters16(), m_length, character, start);
}

template<typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, UChar>>*>
inline size_t StringView::find(CodeUnitMatchFunction&& matchFunction, unsigned start) const
{
    if (is8Bit())
        return WTF::find(characters8(), m_length, std::forward<CodeUnitMatchFunction>(matchFunction), start);
    return WTF::find(characters16(), m_length, std::forward<CodeUnitMatchFunction>(matchFunction), start);
}

inline size_t StringView::reverseFind(UChar character, unsigned start) const
{
    if (is8Bit())
        return WTF::reverseFind(characters8(), m_length, character, start);
    return WTF::reverseFind(characters16(), m_length, character, start);
}

#if !CHECK_STRINGVIEW_LIFETIME

inline void StringView::invalidate(const StringImpl&)
{
}

#endif

template<> class StringTypeAdapter<StringView, void> {
public:
    StringTypeAdapter(StringView string)
        : m_string { string }
    {
    }

    unsigned length() { return m_string.length(); }
    bool is8Bit() { return m_string.is8Bit(); }
    template<typename CharacterType> void writeTo(CharacterType* destination) { m_string.getCharacters(destination); }

private:
    StringView m_string;
};

template<typename CharacterType, size_t inlineCapacity> void append(Vector<CharacterType, inlineCapacity>& buffer, StringView string)
{
    unsigned oldSize = buffer.size();
    buffer.grow(oldSize + string.length());
    string.getCharacters(buffer.data() + oldSize);
}

ALWAYS_INLINE bool equal(StringView a, StringView b, unsigned length)
{
    if (a.m_characters == b.m_characters) {
        ASSERT(a.is8Bit() == b.is8Bit());
        return true;
    }

    return equalCommon(a, b, length);
}

ALWAYS_INLINE bool equal(StringView a, StringView b)
{
    if (a.m_characters == b.m_characters) {
        ASSERT(a.is8Bit() == b.is8Bit());
        return a.length() == b.length();
    }

    return equalCommon(a, b);
}

inline bool equal(StringView a, const LChar* b)
{
    if (!b)
        return !a.isEmpty();
    if (a.isEmpty())
        return !b;

    unsigned aLength = a.length();
    if (aLength != strlen(reinterpret_cast<const char*>(b)))
        return false;

    if (a.is8Bit())
        return equal(a.characters8(), b, aLength);
    return equal(a.characters16(), b, aLength);
}

ALWAYS_INLINE bool equal(StringView a, ASCIILiteral b)
{
    return equal(a, b.characters8());
}

inline bool equalIgnoringASCIICase(StringView a, StringView b)
{
    return equalIgnoringASCIICaseCommon(a, b);
}

inline bool equalIgnoringASCIICase(StringView a, ASCIILiteral b)
{
    return equalIgnoringASCIICaseCommon(a, b.characters());
}

class StringView::SplitResult {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SplitResult(StringView, UChar separator, bool allowEmptyEntries);

    class Iterator;
    Iterator begin() const;
    Iterator end() const;

private:
    StringView m_string;
    UChar m_separator;
    bool m_allowEmptyEntries;
};

class StringView::GraphemeClusters {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit GraphemeClusters(StringView);

    class Iterator;
    Iterator begin() const;
    Iterator end() const;

private:
    StringView m_stringView;
};

class StringView::CodePoints {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CodePoints(StringView);

    class Iterator;
    Iterator begin() const;
    Iterator end() const;

private:
    StringView m_stringView;
};

class StringView::CodeUnits {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CodeUnits(StringView);

    class Iterator;
    Iterator begin() const;
    Iterator end() const;

private:
    StringView m_stringView;
};

class StringView::SplitResult::Iterator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = StringView;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    StringView operator*() const;

    WTF_EXPORT_PRIVATE Iterator& operator++();

    bool operator==(const Iterator&) const;
    bool operator!=(const Iterator&) const;

private:
    enum PositionTag { AtEnd };
    Iterator(const SplitResult&);
    Iterator(const SplitResult&, PositionTag);

    WTF_EXPORT_PRIVATE void findNextSubstring();

    friend SplitResult;

    const SplitResult& m_result;
    unsigned m_position { 0 };
    unsigned m_length;
    bool m_isDone;
};

class StringView::GraphemeClusters::Iterator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Iterator() = delete;
    WTF_EXPORT_PRIVATE Iterator(StringView, unsigned index);
    WTF_EXPORT_PRIVATE ~Iterator();

    Iterator(const Iterator&) = delete;
    WTF_EXPORT_PRIVATE Iterator(Iterator&&);
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(Iterator&&) = delete;

    WTF_EXPORT_PRIVATE StringView operator*() const;
    WTF_EXPORT_PRIVATE Iterator& operator++();

    WTF_EXPORT_PRIVATE bool operator==(const Iterator&) const;
    WTF_EXPORT_PRIVATE bool operator!=(const Iterator&) const;

private:
    class Impl;

    std::unique_ptr<Impl> m_impl;
};

class StringView::CodePoints::Iterator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Iterator(StringView, unsigned index);

    UChar32 operator*() const;
    Iterator& operator++();

    bool operator==(const Iterator&) const;
    bool operator!=(const Iterator&) const;

private:
    const void* m_current;
    const void* m_end;
    bool m_is8Bit;
#if CHECK_STRINGVIEW_LIFETIME
    StringView m_stringView;
#endif
};

class StringView::CodeUnits::Iterator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Iterator(StringView, unsigned index);

    UChar operator*() const;
    Iterator& operator++();

    bool operator==(const Iterator&) const;
    bool operator!=(const Iterator&) const;

private:
    StringView m_stringView;
    unsigned m_index;
};

inline auto StringView::graphemeClusters() const -> GraphemeClusters
{
    return GraphemeClusters(*this);
}

inline auto StringView::codePoints() const -> CodePoints
{
    return CodePoints(*this);
}

inline auto StringView::codeUnits() const -> CodeUnits
{
    return CodeUnits(*this);
}

inline StringView::GraphemeClusters::GraphemeClusters(StringView stringView)
    : m_stringView(stringView)
{
}

inline auto StringView::GraphemeClusters::begin() const -> Iterator
{
    return Iterator(m_stringView, 0);
}

inline auto StringView::GraphemeClusters::end() const -> Iterator
{
    return Iterator(m_stringView, m_stringView.length());
}

inline StringView::CodePoints::CodePoints(StringView stringView)
    : m_stringView(stringView)
{
}

inline StringView::CodePoints::Iterator::Iterator(StringView stringView, unsigned index)
    : m_is8Bit(stringView.is8Bit())
#if CHECK_STRINGVIEW_LIFETIME
    , m_stringView(stringView)
#endif
{
    if (m_is8Bit) {
        const LChar* begin = stringView.characters8();
        m_current = begin + index;
        m_end = begin + stringView.length();
    } else {
        const UChar* begin = stringView.characters16();
        m_current = begin + index;
        m_end = begin + stringView.length();
    }
}

inline auto StringView::CodePoints::Iterator::operator++() -> Iterator&
{
#if CHECK_STRINGVIEW_LIFETIME
    ASSERT(m_stringView.underlyingStringIsValid());
#endif
    ASSERT(m_current < m_end);
    if (m_is8Bit)
        m_current = static_cast<const LChar*>(m_current) + 1;
    else {
        unsigned i = 0;
        size_t length = static_cast<const UChar*>(m_end) - static_cast<const UChar*>(m_current);
        U16_FWD_1(static_cast<const UChar*>(m_current), i, length);
        m_current = static_cast<const UChar*>(m_current) + i;
    }
    return *this;
}

inline UChar32 StringView::CodePoints::Iterator::operator*() const
{
#if CHECK_STRINGVIEW_LIFETIME
    ASSERT(m_stringView.underlyingStringIsValid());
#endif
    ASSERT(m_current < m_end);
    if (m_is8Bit)
        return *static_cast<const LChar*>(m_current);
    UChar32 codePoint;
    size_t length = static_cast<const UChar*>(m_end) - static_cast<const UChar*>(m_current);
    U16_GET(static_cast<const UChar*>(m_current), 0, 0, length, codePoint);
    return codePoint;
}

inline bool StringView::CodePoints::Iterator::operator==(const Iterator& other) const
{
#if CHECK_STRINGVIEW_LIFETIME
    ASSERT(m_stringView.underlyingStringIsValid());
#endif
    ASSERT(m_is8Bit == other.m_is8Bit);
    ASSERT(m_end == other.m_end);
    return m_current == other.m_current;
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

inline StringView::CodeUnits::CodeUnits(StringView stringView)
    : m_stringView(stringView)
{
}

inline StringView::CodeUnits::Iterator::Iterator(StringView stringView, unsigned index)
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
    ASSERT(m_stringView.m_characters == other.m_stringView.m_characters);
    ASSERT(m_stringView.m_length == other.m_stringView.m_length);
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

inline auto StringView::split(UChar separator) const -> SplitResult
{
    return SplitResult { *this, separator, false };
}

inline auto StringView::splitAllowingEmptyEntries(UChar separator) const -> SplitResult
{
    return SplitResult { *this, separator, true };
}

inline StringView::SplitResult::SplitResult(StringView stringView, UChar separator, bool allowEmptyEntries)
    : m_string { stringView }
    , m_separator { separator }
    , m_allowEmptyEntries { allowEmptyEntries }
{
}

inline auto StringView::SplitResult::begin() const -> Iterator
{
    return Iterator { *this };
}

inline auto StringView::SplitResult::end() const -> Iterator
{
    return Iterator { *this, Iterator::AtEnd };
}

inline StringView::SplitResult::Iterator::Iterator(const SplitResult& result)
    : m_result { result }
    , m_isDone { result.m_string.isEmpty() && !result.m_allowEmptyEntries }
{
    findNextSubstring();
}

inline StringView::SplitResult::Iterator::Iterator(const SplitResult& result, PositionTag)
    : m_result { result }
    , m_position { result.m_string.length() }
    , m_isDone { true }
{
}

inline StringView StringView::SplitResult::Iterator::operator*() const
{
    ASSERT(m_position <= m_result.m_string.length());
    ASSERT(!m_isDone);
    return m_result.m_string.substring(m_position, m_length);
}

inline bool StringView::SplitResult::Iterator::operator==(const Iterator& other) const
{
    ASSERT(&m_result == &other.m_result);
    return m_position == other.m_position && m_isDone == other.m_isDone;
}

inline bool StringView::SplitResult::Iterator::operator!=(const Iterator& other) const
{
    return !(*this == other);
}

template<typename CharacterType, typename MatchedCharacterPredicate>
inline StringView StringView::stripLeadingAndTrailingMatchedCharacters(const CharacterType* characters, const MatchedCharacterPredicate& predicate) const
{
    if (!m_length)
        return *this;

    unsigned start = 0;
    unsigned end = m_length - 1;

    while (start <= end && predicate(characters[start]))
        ++start;

    if (start > end)
        return StringView::empty();

    while (end && predicate(characters[end]))
        --end;

    if (!start && end == m_length - 1)
        return *this;

    StringView result(characters + start, end + 1 - start);
    result.setUnderlyingString(*this);
    return result;
}

template<typename MatchedCharacterPredicate>
StringView StringView::stripLeadingAndTrailingMatchedCharacters(const MatchedCharacterPredicate& predicate) const
{
    if (is8Bit())
        return stripLeadingAndTrailingMatchedCharacters<LChar>(characters8(), predicate);
    return stripLeadingAndTrailingMatchedCharacters<UChar>(characters16(), predicate);
}

inline bool equalLettersIgnoringASCIICase(StringView string, ASCIILiteral literal)
{
    return equalLettersIgnoringASCIICaseCommon(string, literal);
}

inline bool startsWithLettersIgnoringASCIICase(StringView string, ASCIILiteral literal)
{
    return startsWithLettersIgnoringASCIICaseCommon(string, literal);
}

inline bool equalIgnoringNullity(StringView a, StringView b)
{
    // FIXME: equal(StringView, StringView) ignores nullity; consider changing to be like other string classes and respecting it.
    return equal(a, b);
}

WTF_EXPORT_PRIVATE int codePointCompare(StringView, StringView);

inline bool hasUnpairedSurrogate(StringView string)
{
    // Fast path for 8-bit strings; they can't have any surrogates.
    if (string.is8Bit())
        return false;
    for (auto codePoint : string.codePoints()) {
        if (U_IS_SURROGATE(codePoint))
            return true;
    }
    return false;
}

inline size_t findCommon(StringView haystack, StringView needle, unsigned start)
{
    unsigned needleLength = needle.length();

    if (needleLength == 1) {
        if (haystack.is8Bit())
            return WTF::find(haystack.characters8(), haystack.length(), needle[0], start);
        return WTF::find(haystack.characters16(), haystack.length(), needle[0], start);
    }

    if (start > haystack.length())
        return notFound;

    if (!needleLength)
        return start;

    unsigned searchLength = haystack.length() - start;
    if (needleLength > searchLength)
        return notFound;

    if (haystack.is8Bit()) {
        if (needle.is8Bit())
            return findInner(haystack.characters8() + start, needle.characters8(), start, searchLength, needleLength);
        return findInner(haystack.characters8() + start, needle.characters16(), start, searchLength, needleLength);
    }

    if (needle.is8Bit())
        return findInner(haystack.characters16() + start, needle.characters8(), start, searchLength, needleLength);

    return findInner(haystack.characters16() + start, needle.characters16(), start, searchLength, needleLength);
}

inline size_t findIgnoringASCIICase(StringView source, StringView stringToFind, unsigned start)
{
    unsigned sourceStringLength = source.length();
    unsigned matchLength = stringToFind.length();
    if (!matchLength)
        return std::min(start, sourceStringLength);

    // Check start & matchLength are in range.
    if (start > sourceStringLength)
        return notFound;
    unsigned searchLength = sourceStringLength - start;
    if (matchLength > searchLength)
        return notFound;

    if (source.is8Bit()) {
        if (stringToFind.is8Bit())
            return findIgnoringASCIICase(source.characters8(), stringToFind.characters8(), start, searchLength, matchLength);
        return findIgnoringASCIICase(source.characters8(), stringToFind.characters16(), start, searchLength, matchLength);
    }

    if (stringToFind.is8Bit())
        return findIgnoringASCIICase(source.characters16(), stringToFind.characters8(), start, searchLength, matchLength);

    return findIgnoringASCIICase(source.characters16(), stringToFind.characters16(), start, searchLength, matchLength);
}

inline bool startsWith(StringView reference, StringView prefix)
{
    unsigned prefixLength = prefix.length();
    if (prefixLength > reference.length())
        return false;

    if (reference.is8Bit()) {
        if (prefix.is8Bit())
            return equal(reference.characters8(), prefix.characters8(), prefixLength);
        return equal(reference.characters8(), prefix.characters16(), prefixLength);
    }
    if (prefix.is8Bit())
        return equal(reference.characters16(), prefix.characters8(), prefixLength);
    return equal(reference.characters16(), prefix.characters16(), prefixLength);
}

inline bool startsWithIgnoringASCIICase(StringView reference, StringView prefix)
{
    unsigned prefixLength = prefix.length();
    if (prefixLength > reference.length())
        return false;

    if (reference.is8Bit()) {
        if (prefix.is8Bit())
            return equalIgnoringASCIICase(reference.characters8(), prefix.characters8(), prefixLength);
        return equalIgnoringASCIICase(reference.characters8(), prefix.characters16(), prefixLength);
    }
    if (prefix.is8Bit())
        return equalIgnoringASCIICase(reference.characters16(), prefix.characters8(), prefixLength);
    return equalIgnoringASCIICase(reference.characters16(), prefix.characters16(), prefixLength);
}

inline bool endsWith(StringView reference, StringView suffix)
{
    unsigned suffixLength = suffix.length();
    unsigned referenceLength = reference.length();
    if (suffixLength > referenceLength)
        return false;

    unsigned startOffset = referenceLength - suffixLength;

    if (reference.is8Bit()) {
        if (suffix.is8Bit())
            return equal(reference.characters8() + startOffset, suffix.characters8(), suffixLength);
        return equal(reference.characters8() + startOffset, suffix.characters16(), suffixLength);
    }
    if (suffix.is8Bit())
        return equal(reference.characters16() + startOffset, suffix.characters8(), suffixLength);
    return equal(reference.characters16() + startOffset, suffix.characters16(), suffixLength);
}

inline bool endsWithIgnoringASCIICase(StringView reference, StringView suffix)
{
    unsigned suffixLength = suffix.length();
    unsigned referenceLength = reference.length();
    if (suffixLength > referenceLength)
        return false;

    unsigned startOffset = referenceLength - suffixLength;

    if (reference.is8Bit()) {
        if (suffix.is8Bit())
            return equalIgnoringASCIICase(reference.characters8() + startOffset, suffix.characters8(), suffixLength);
        return equalIgnoringASCIICase(reference.characters8() + startOffset, suffix.characters16(), suffixLength);
    }
    if (suffix.is8Bit())
        return equalIgnoringASCIICase(reference.characters16() + startOffset, suffix.characters8(), suffixLength);
    return equalIgnoringASCIICase(reference.characters16() + startOffset, suffix.characters16(), suffixLength);
}

inline size_t String::find(StringView string) const
{
    return m_impl ? m_impl->find(string) : notFound;
}
inline size_t String::find(StringView string, unsigned start) const
{
    return m_impl ? m_impl->find(string, start) : notFound;
}

inline size_t String::findIgnoringASCIICase(StringView string) const
{
    return m_impl ? m_impl->findIgnoringASCIICase(string) : notFound;
}

inline size_t String::findIgnoringASCIICase(StringView string, unsigned start) const
{
    return m_impl ? m_impl->findIgnoringASCIICase(string, start) : notFound;
}

inline size_t String::reverseFind(StringView string, unsigned start) const
{
    return m_impl ? m_impl->reverseFind(string, start) : notFound;
}

inline bool String::contains(StringView string) const
{
    return find(string) != notFound;
}

inline bool String::containsIgnoringASCIICase(StringView string) const
{
    return findIgnoringASCIICase(string) != notFound;
}

inline bool String::containsIgnoringASCIICase(StringView string, unsigned start) const
{
    return findIgnoringASCIICase(string, start) != notFound;
}

inline String WARN_UNUSED_RETURN makeStringByReplacingAll(const String& string, StringView target, StringView replacement)
{
    if (auto* impl = string.impl())
        return String { impl->replace(target, replacement) };
    return string;
}

inline String WARN_UNUSED_RETURN makeStringByReplacing(const String& string, unsigned start, unsigned length, StringView replacement)
{
    if (auto* impl = string.impl())
        return String { impl->replace(start, length, replacement) };
    return string;
}

inline String WARN_UNUSED_RETURN makeStringByReplacingAll(const String& string, UChar target, StringView replacement)
{
    if (auto* impl = string.impl())
        return String { impl->replace(target, replacement) };
    return string;
}

WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN makeStringByReplacingAll(StringView, UChar target, UChar replacement);
WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN makeStringBySimplifyingNewLinesSlowCase(const String&, unsigned firstCarriageReturnOffset);

inline String WARN_UNUSED_RETURN makeStringBySimplifyingNewLines(const String& string)
{
    auto firstCarriageReturn = string.find('\r');
    if (firstCarriageReturn == notFound)
        return string;
    return makeStringBySimplifyingNewLinesSlowCase(string, firstCarriageReturn);
}

inline bool String::startsWith(StringView string) const
{
    return m_impl ? m_impl->startsWith(string) : string.isEmpty();
}

inline bool String::startsWithIgnoringASCIICase(StringView string) const
{
    return m_impl ? m_impl->startsWithIgnoringASCIICase(string) : string.isEmpty();
}

inline bool String::endsWith(StringView string) const
{
    return m_impl ? m_impl->endsWith(string) : string.isEmpty();
}

inline bool String::endsWithIgnoringASCIICase(StringView string) const
{
    return m_impl ? m_impl->endsWithIgnoringASCIICase(string) : string.isEmpty();
}

inline bool String::hasInfixStartingAt(StringView prefix, unsigned start) const
{
    return m_impl && prefix && m_impl->hasInfixStartingAt(prefix, start);
}

inline bool String::hasInfixEndingAt(StringView suffix, unsigned end) const
{
    return m_impl && suffix && m_impl->hasInfixEndingAt(suffix, end);
}

inline size_t AtomString::find(StringView string, unsigned start) const
{
    return m_string.find(string, start);
}

inline size_t AtomString::findIgnoringASCIICase(StringView string) const
{
    return m_string.findIgnoringASCIICase(string);
}

inline size_t AtomString::findIgnoringASCIICase(StringView string, unsigned start) const
{
    return m_string.findIgnoringASCIICase(string, start);
}

inline bool AtomString::contains(StringView string) const
{
    return m_string.contains(string);
}

inline bool AtomString::containsIgnoringASCIICase(StringView string) const
{
    return m_string.containsIgnoringASCIICase(string);
}

inline bool AtomString::startsWith(StringView string) const
{
    return m_string.startsWith(string);
}

inline bool AtomString::startsWithIgnoringASCIICase(StringView string) const
{
    return m_string.startsWithIgnoringASCIICase(string);
}

inline bool AtomString::endsWith(StringView string) const
{
    return m_string.endsWith(string);
}

inline bool AtomString::endsWithIgnoringASCIICase(StringView string) const
{
    return m_string.endsWithIgnoringASCIICase(string);
}

template<typename Func>
inline Expected<std::invoke_result_t<Func, Span<const char>>, UTF8ConversionError> StringView::tryGetUTF8(const Func& function, ConversionMode mode) const
{
    if (is8Bit())
        return StringImpl::tryGetUTF8ForCharacters(function, characters8(), length());
    return StringImpl::tryGetUTF8ForCharacters(function, characters16(), length(), mode);
}

} // namespace WTF

using WTF::append;
using WTF::equal;
using WTF::makeStringByReplacing;
using WTF::makeStringBySimplifyingNewLines;
using WTF::StringView;
using WTF::StringViewWithUnderlyingString;
using WTF::hasUnpairedSurrogate;
