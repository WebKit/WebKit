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

OBJC_CLASS NSString;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

class AdaptiveStringSearcherTables;

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
    StringView(std::span<const LChar>);
    StringView(std::span<const UChar>);
    StringView(std::span<const char>); // FIXME: Consider dropping this overload. Callers should pass LChars/UChars instead.
    StringView(const void*, unsigned length, bool is8bit);
    StringView(ASCIILiteral);

    ALWAYS_INLINE static StringView fromLatin1(const char* characters) { return StringView { characters }; }

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
    const void* rawCharacters() const { return m_characters; }
    std::span<const LChar> span8() const;
    std::span<const UChar> span16() const;
    template<typename CharacterType> std::span<const CharacterType> span() const;

    unsigned hash() const;

    bool containsOnlyASCII() const;

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
    Expected<std::invoke_result_t<Func, std::span<const char8_t>>, UTF8ConversionError> tryGetUTF8(const Func&, ConversionMode = LenientConversion) const;

    template<size_t N>
    class UpconvertedCharactersWithSize;
    using UpconvertedCharacters = UpconvertedCharactersWithSize<32>;

    template<size_t N = 32>
    UpconvertedCharactersWithSize<N> upconvertedCharacters() const;

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
    StringView trim(const MatchedCharacterPredicate&) const;

    class SplitResult;
    SplitResult split(UChar) const;
    SplitResult splitAllowingEmptyEntries(UChar) const;

    size_t find(UChar, unsigned start = 0) const;
    size_t find(LChar, unsigned start = 0) const;
    ALWAYS_INLINE size_t find(char c, unsigned start = 0) const { return find(byteCast<LChar>(c), start); }
    template<typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, UChar>>* = nullptr>
    size_t find(CodeUnitMatchFunction&&, unsigned start = 0) const;
    ALWAYS_INLINE size_t find(ASCIILiteral literal, unsigned start = 0) const { return find(literal.span8(), start); }
    WTF_EXPORT_PRIVATE size_t find(StringView, unsigned start = 0) const;
    WTF_EXPORT_PRIVATE size_t find(AdaptiveStringSearcherTables&, StringView, unsigned start = 0) const;

    size_t reverseFind(UChar, unsigned index = std::numeric_limits<unsigned>::max()) const;
    ALWAYS_INLINE size_t reverseFind(ASCIILiteral literal, unsigned start = std::numeric_limits<unsigned>::max()) const { return reverseFind(literal.span8(), start); }
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

    template<bool isSpecialCharacter(UChar)> bool containsOnly() const;

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

#ifndef NDEBUG
    WTF_EXPORT_PRIVATE void show() const;
#endif

private:
    // Clients should use StringView(ASCIILiteral) or StringView::fromLatin1() instead.
    explicit StringView(const char*);

    UChar unsafeCharacterAt(unsigned index) const;

    friend bool equal(StringView, StringView);
    friend bool equal(StringView, StringView, unsigned length);
    friend WTF_EXPORT_PRIVATE bool equalRespectingNullity(StringView, StringView);
    friend size_t findCommon(StringView haystack, StringView needle, unsigned start);

    void initialize(std::span<const LChar>);
    void initialize(std::span<const UChar>);

    WTF_EXPORT_PRIVATE size_t find(std::span<const LChar> match, unsigned start) const;
    WTF_EXPORT_PRIVATE size_t reverseFind(std::span<const LChar> match, unsigned start) const;

    template<typename CharacterType, typename MatchedCharacterPredicate>
    StringView trim(std::span<const CharacterType>, const MatchedCharacterPredicate&) const;

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

struct StringViewWithUnderlyingString;

// This returns a StringView of the normalized result, and a String that is either
// null, if the input was already normalized, or contains the normalized result
// and needs to be kept around so the StringView remains valid. Typically the
// easiest way to use it correctly is to put it into a local and use the StringView.
WTF_EXPORT_PRIVATE StringViewWithUnderlyingString normalizedNFC(StringView);

WTF_EXPORT_PRIVATE String normalizedNFC(const String&);

inline StringView nullStringView() { return { }; }
inline StringView emptyStringView() { return ""_span; }

} // namespace WTF

#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

namespace WTF {

struct StringViewWithUnderlyingString {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    StringViewWithUnderlyingString() = default;

    StringViewWithUnderlyingString(StringView passedView, String passedUnderlyingString)
        : underlyingString(WTFMove(passedUnderlyingString))
        , view(WTFMove(passedView))
    { }

    String underlyingString;
    StringView view;

    String toString() const;
    AtomString toAtomString() const;
};

inline StringView::StringView()
{
}

inline String StringViewWithUnderlyingString::toString() const
{
    if (LIKELY(view.length() == underlyingString.length()))
        return underlyingString;
    return view.toString();
}

inline AtomString StringViewWithUnderlyingString::toAtomString() const
{
    if (LIKELY(view.length() == underlyingString.length()))
        return AtomString { underlyingString };
    return view.toAtomString();
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

inline void StringView::initialize(std::span<const LChar> characters)
{
    m_characters = characters.data();
    m_length = characters.size();
    m_is8Bit = true;
}

inline void StringView::initialize(std::span<const UChar> characters)
{
    m_characters = characters.data();
    m_length = characters.size();
    m_is8Bit = false;
}

inline StringView::StringView(std::span<const LChar> characters)
{
    initialize(characters);
}

inline StringView::StringView(std::span<const UChar> characters)
{
    initialize(characters);
}

inline StringView::StringView(const char* characters)
{
    initialize(WTF::span8(characters));
}

inline StringView::StringView(std::span<const char> characters)
{
    initialize(byteCast<LChar>(characters));
}

inline StringView::StringView(const void* characters, unsigned length, bool is8bit)
    : m_characters(characters)
    , m_length(length)
    , m_is8Bit(is8bit)
{
}

inline StringView::StringView(ASCIILiteral string)
{
    initialize(string.span8());
}

inline StringView::StringView(const StringImpl& string)
{
    setUnderlyingString(&string);
    if (string.is8Bit())
        initialize(string.span8());
    else
        initialize(string.span16());
}

inline StringView::StringView(const StringImpl* string)
{
    if (!string)
        return;

    setUnderlyingString(string);
    if (string->is8Bit())
        initialize(string->span8());
    else
        initialize(string->span16());
}

inline StringView::StringView(const String& string)
{
    setUnderlyingString(string.impl());
    if (!string.impl()) {
        clear();
        return;
    }
    if (string.is8Bit()) {
        initialize(string.span8());
        return;
    }
    initialize(string.span16());
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

inline std::span<const LChar> StringView::span8() const
{
    ASSERT(is8Bit());
    ASSERT(underlyingStringIsValid());
    return { static_cast<const LChar*>(m_characters), m_length };
}

inline std::span<const UChar> StringView::span16() const
{
    ASSERT(!is8Bit() || isEmpty());
    ASSERT(underlyingStringIsValid());
    return { static_cast<const UChar*>(m_characters), m_length };
}

inline unsigned StringView::hash() const
{
    if (is8Bit())
        return StringHasher::computeHashAndMaskTop8Bits(span8());
    return StringHasher::computeHashAndMaskTop8Bits(span16());
}

template<> ALWAYS_INLINE std::span<const LChar> StringView::span<LChar>() const
{
    return span8();
}

template<> ALWAYS_INLINE std::span<const UChar> StringView::span<UChar>() const
{
    return span16();
}

inline bool StringView::containsOnlyASCII() const
{
    if (is8Bit())
        return charactersAreAllASCII(span8());
    return charactersAreAllASCII(span16());
}

template<size_t N>
class StringView::UpconvertedCharactersWithSize {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit UpconvertedCharactersWithSize(StringView);
    operator const UChar*() const { return m_characters.data(); }
    const UChar* get() const { return m_characters.data(); }
    operator std::span<const UChar>() const { return m_characters; }
    std::span<const UChar> span() const { return m_characters; }

private:
    Vector<UChar, N> m_upconvertedCharacters;
    std::span<const UChar> m_characters;
};

template<size_t N>
inline StringView::UpconvertedCharactersWithSize<N> StringView::upconvertedCharacters() const
{
    return UpconvertedCharactersWithSize<N>(*this);
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
        return emptyStringView();
    unsigned maxLength = this->length() - start;

    if (length >= maxLength) {
        if (!start)
            return *this;
        length = maxLength;
    }

    if (is8Bit()) {
        StringView result(span8().subspan(start, length));
        result.setUnderlyingString(*this);
        return result;
    }
    StringView result(span16().subspan(start, length));
    result.setUnderlyingString(*this);
    return result;
}

inline UChar StringView::characterAt(unsigned index) const
{
    if (is8Bit())
        return span8()[index];
    return span16()[index];
}

inline UChar StringView::unsafeCharacterAt(unsigned index) const
{
    ASSERT(index < length());
    if (is8Bit())
        return span8().data()[index];
    return span16().data()[index];
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

template<bool isSpecialCharacter(UChar)> inline bool StringView::containsOnly() const
{
    if (is8Bit())
        return WTF::containsOnly<isSpecialCharacter>(span8());
    return WTF::containsOnly<isSpecialCharacter>(span16());
}

template<typename CharacterType> inline void StringView::getCharacters8(CharacterType* destination) const
{
    StringImpl::copyCharacters(destination, span8());
}

template<typename CharacterType> inline void StringView::getCharacters16(CharacterType* destination) const
{
    StringImpl::copyCharacters(destination, span16());
}

template<typename CharacterType> inline void StringView::getCharacters(CharacterType* destination) const
{
    if (is8Bit())
        getCharacters8(destination);
    else
        getCharacters16(destination);
}

template<size_t N>
inline StringView::UpconvertedCharactersWithSize<N>::UpconvertedCharactersWithSize(StringView string)
{
    if (!string.is8Bit()) {
        m_characters = string.span16();
        return;
    }
    m_upconvertedCharacters.grow(string.m_length);
    StringImpl::copyCharacters(m_upconvertedCharacters.data(), string.span8());
    m_characters = m_upconvertedCharacters.span();
}

inline String StringView::toString() const
{
    if (is8Bit())
        return span8();
    return span16();
}

inline AtomString StringView::toAtomString() const
{
    if (is8Bit())
        return span8();
    return span16();
}

inline AtomString StringView::toExistingAtomString() const
{
    if (is8Bit())
        return AtomStringImpl::lookUp(span8());
    return AtomStringImpl::lookUp(span16());
}

inline float StringView::toFloat(bool& isValid) const
{
    if (is8Bit())
        return charactersToFloat(span8(), &isValid);
    return charactersToFloat(span16(), &isValid);
}

inline double StringView::toDouble(bool& isValid) const
{
    if (is8Bit())
        return charactersToDouble(span8(), &isValid);
    return charactersToDouble(span16(), &isValid);
}

inline String StringView::toStringWithoutCopying() const
{
    if (is8Bit())
        return StringImpl::createWithoutCopying(span8());
    return StringImpl::createWithoutCopying(span16());
}

inline size_t StringView::find(UChar character, unsigned start) const
{
    if (is8Bit())
        return WTF::find(span8(), character, start);
    return WTF::find(span16(), character, start);
}

inline size_t StringView::find(LChar character, unsigned start) const
{
    if (is8Bit())
        return WTF::find(span8(), character, start);
    return WTF::find(span16(), character, start);
}

template<typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, UChar>>*>
inline size_t StringView::find(CodeUnitMatchFunction&& matchFunction, unsigned start) const
{
    if (is8Bit())
        return WTF::find(span8(), std::forward<CodeUnitMatchFunction>(matchFunction), start);
    return WTF::find(span16(), std::forward<CodeUnitMatchFunction>(matchFunction), start);
}

inline size_t StringView::reverseFind(UChar character, unsigned start) const
{
    if (is8Bit())
        return WTF::reverseFind(span8(), character, start);
    return WTF::reverseFind(span16(), character, start);
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

    unsigned length() const { return m_string.length(); }
    bool is8Bit() const { return m_string.is8Bit(); }
    template<typename CharacterType> void writeTo(CharacterType* destination) { m_string.getCharacters(destination); }

private:
    StringView m_string;
};

template<typename CharacterType, size_t inlineCapacity> void append(Vector<CharacterType, inlineCapacity>& buffer, StringView string)
{
    size_t oldSize = buffer.size();
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

    auto bSpan = span8(byteCast<char>(b));
    if (a.length() != bSpan.size())
        return false;

    if (a.is8Bit())
        return equal(a.span8().data(), bSpan);
    return equal(a.span16().data(), bSpan);
}

ALWAYS_INLINE bool equal(StringView a, ASCIILiteral b)
{
    return equal(a, b.span8().data());
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

private:
    class Impl;

    std::unique_ptr<Impl> m_impl;
};

class StringView::CodePoints::Iterator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Iterator(StringView, unsigned index);

    char32_t operator*() const;
    Iterator& operator++();

    bool operator==(const Iterator&) const;

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
        const LChar* begin = stringView.span8().data();
        m_current = begin + index;
        m_end = begin + stringView.length();
    } else {
        const UChar* begin = stringView.span16().data();
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

inline char32_t StringView::CodePoints::Iterator::operator*() const
{
#if CHECK_STRINGVIEW_LIFETIME
    ASSERT(m_stringView.underlyingStringIsValid());
#endif
    ASSERT(m_current < m_end);
    if (m_is8Bit)
        return *static_cast<const LChar*>(m_current);
    char32_t codePoint;
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
    return m_stringView.unsafeCharacterAt(m_index);
}

inline bool StringView::CodeUnits::Iterator::operator==(const Iterator& other) const
{
    ASSERT(m_stringView.m_characters == other.m_stringView.m_characters);
    ASSERT(m_stringView.m_length == other.m_stringView.m_length);
    return m_index == other.m_index;
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

template<typename CharacterType, typename MatchedCharacterPredicate>
inline StringView StringView::trim(std::span<const CharacterType> characters, const MatchedCharacterPredicate& predicate) const
{
    if (!m_length)
        return *this;

    unsigned start = 0;
    unsigned end = m_length - 1;

    while (start <= end && predicate(characters[start]))
        ++start;

    if (start > end)
        return emptyStringView();

    while (end && predicate(characters[end]))
        --end;

    if (!start && end == m_length - 1)
        return *this;

    StringView result(characters.subspan(start, end + 1 - start));
    result.setUnderlyingString(*this);
    return result;
}

template<typename MatchedCharacterPredicate>
StringView StringView::trim(const MatchedCharacterPredicate& predicate) const
{
    if (is8Bit())
        return trim<LChar>(span8(), predicate);
    return trim<UChar>(span16(), predicate);
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
        UChar firstCharacter = needle.unsafeCharacterAt(0);
        if (haystack.is8Bit())
            return WTF::find(haystack.span8(), firstCharacter, start);
        return WTF::find(haystack.span16(), firstCharacter, start);
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
            return findInner(haystack.span8().subspan(start), needle.span8(), start);
        return findInner(haystack.span8().subspan(start), needle.span16(), start);
    }

    if (needle.is8Bit())
        return findInner(haystack.span16().subspan(start), needle.span8(), start);

    return findInner(haystack.span16().subspan(start), needle.span16(), start);
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
            return WTF::findIgnoringASCIICase(source.span8(), stringToFind.span8(), static_cast<size_t>(start));
        return WTF::findIgnoringASCIICase(source.span8(), stringToFind.span16(), static_cast<size_t>(start));
    }

    if (stringToFind.is8Bit())
        return WTF::findIgnoringASCIICase(source.span16(), stringToFind.span8(), static_cast<size_t>(start));
    return WTF::findIgnoringASCIICase(source.span16(), stringToFind.span16(), static_cast<size_t>(start));
}

inline bool startsWith(StringView reference, StringView prefix)
{
    if (prefix.length() > reference.length())
        return false;

    if (reference.is8Bit()) {
        if (prefix.is8Bit())
            return equal(reference.span8().data(), prefix.span8());
        return equal(reference.span8().data(), prefix.span16());
    }
    if (prefix.is8Bit())
        return equal(reference.span16().data(), prefix.span8());
    return equal(reference.span16().data(), prefix.span16());
}

inline bool startsWithIgnoringASCIICase(StringView reference, StringView prefix)
{
    if (prefix.length() > reference.length())
        return false;

    if (reference.is8Bit()) {
        if (prefix.is8Bit())
            return equalIgnoringASCIICaseWithLength(reference.span8(), prefix.span8(), prefix.length());
        return equalIgnoringASCIICaseWithLength(reference.span8(), prefix.span16(), prefix.length());
    }
    if (prefix.is8Bit())
        return equalIgnoringASCIICaseWithLength(reference.span16(), prefix.span8(), prefix.length());
    return equalIgnoringASCIICaseWithLength(reference.span16(), prefix.span16(), prefix.length());
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
            return equal(reference.span8().data() + startOffset, suffix.span8());
        return equal(reference.span8().data() + startOffset, suffix.span16());
    }
    if (suffix.is8Bit())
        return equal(reference.span16().data() + startOffset, suffix.span8());
    return equal(reference.span16().data() + startOffset, suffix.span16());
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
            return equalIgnoringASCIICaseWithLength(reference.span8().subspan(startOffset), suffix.span8(), suffix.length());
        return equalIgnoringASCIICaseWithLength(reference.span8().subspan(startOffset), suffix.span16(), suffix.length());
    }
    if (suffix.is8Bit())
        return equalIgnoringASCIICaseWithLength(reference.span16().subspan(startOffset), suffix.span8(), suffix.length());
    return equalIgnoringASCIICaseWithLength(reference.span16().subspan(startOffset), suffix.span16(), suffix.length());
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

inline size_t AtomString::find(StringView string, size_t start) const
{
    return m_string.find(string, start);
}

inline size_t AtomString::findIgnoringASCIICase(StringView string) const
{
    return m_string.findIgnoringASCIICase(string);
}

inline size_t AtomString::findIgnoringASCIICase(StringView string, size_t start) const
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
inline Expected<std::invoke_result_t<Func, std::span<const char8_t>>, UTF8ConversionError> StringView::tryGetUTF8(const Func& function, ConversionMode mode) const
{
    if (is8Bit())
        return StringImpl::tryGetUTF8ForCharacters(function, span8());
    return StringImpl::tryGetUTF8ForCharacters(function, span16(), mode);
}

template<> struct VectorTraits<StringView> : VectorTraitsBase<false, void> {
    static constexpr bool canMoveWithMemcpy = true;
    static constexpr bool canCopyWithMemcpy = true;
};

template<> struct VectorTraits<StringViewWithUnderlyingString> : VectorTraitsBase<false, void> {
    static constexpr bool canMoveWithMemcpy = true;
};

} // namespace WTF

using WTF::append;
using WTF::equal;
using WTF::makeStringByReplacing;
using WTF::makeStringBySimplifyingNewLines;
using WTF::StringView;
using WTF::StringViewWithUnderlyingString;
using WTF::hasUnpairedSurrogate;
using WTF::nullStringView;
using WTF::emptyStringView;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
