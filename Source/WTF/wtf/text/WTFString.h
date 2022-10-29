/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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

// This file would be called String.h, but that conflicts with <string.h>
// on systems without case-sensitive file systems.

#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/ExternalStringImpl.h>

#ifdef __OBJC__
#include <objc/objc.h>
#endif

#if OS(WINDOWS)
#include <wtf/text/win/WCharStringExtras.h>
#endif

namespace WTF {

// Declarations of string operations

WTF_EXPORT_PRIVATE double charactersToDouble(const LChar*, size_t, bool* ok = nullptr);
WTF_EXPORT_PRIVATE double charactersToDouble(const UChar*, size_t, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(const LChar*, size_t, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(const UChar*, size_t, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(const LChar*, size_t, size_t& parsedLength);
WTF_EXPORT_PRIVATE float charactersToFloat(const UChar*, size_t, size_t& parsedLength);

template<bool isSpecialCharacter(UChar), typename CharacterType> bool isAllSpecialCharacters(const CharacterType*, size_t);

enum TrailingZerosTruncatingPolicy { KeepTrailingZeros, TruncateTrailingZeros };

class String final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // Construct a null string, distinguishable from an empty string.
    String() = default;

    // Construct a string with UTF-16 data.
    WTF_EXPORT_PRIVATE String(const UChar* characters, unsigned length);
    ALWAYS_INLINE String(Span<const UChar> characters) : String(characters.data(), characters.size()) { }

    // Construct a string with Latin-1 data.
    WTF_EXPORT_PRIVATE String(const LChar* characters, unsigned length);
    WTF_EXPORT_PRIVATE String(const char* characters, unsigned length);
    ALWAYS_INLINE String(Span<const LChar> characters) : String(characters.data(), characters.size()) { }
    ALWAYS_INLINE static String fromLatin1(const char* characters) { return String { characters }; }

    // Construct a string referencing an existing StringImpl.
    String(StringImpl&);
    String(StringImpl*);
    String(Ref<StringImpl>&&);
    String(RefPtr<StringImpl>&&);

    String(ExternalStringImpl&);
    String(ExternalStringImpl*);
    String(Ref<ExternalStringImpl>&&);
    String(RefPtr<ExternalStringImpl>&&);

    String(Ref<AtomStringImpl>&&);
    String(RefPtr<AtomStringImpl>&&);

    String(StaticStringImpl&);
    String(StaticStringImpl*);

    // Construct a string from a constant string literal.
    String(ASCIILiteral);

    String(const String&) = default;
    String(String&&) = default;
    String& operator=(const String&) = default;
    String& operator=(String&&) = default;

    ALWAYS_INLINE ~String() = default;

    void swap(String& o) { m_impl.swap(o.m_impl); }

    static String adopt(StringBuffer<LChar>&& buffer) { return StringImpl::adopt(WTFMove(buffer)); }
    static String adopt(StringBuffer<UChar>&& buffer) { return StringImpl::adopt(WTFMove(buffer)); }
    template<typename CharacterType, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity>
    static String adopt(Vector<CharacterType, inlineCapacity, OverflowHandler, minCapacity>&& vector) { return StringImpl::adopt(WTFMove(vector)); }

    bool isNull() const { return !m_impl; }
    bool isEmpty() const { return !m_impl || m_impl->isEmpty(); }

    StringImpl* impl() const { return m_impl.get(); }
    RefPtr<StringImpl> releaseImpl() { return WTFMove(m_impl); }

    unsigned length() const { return m_impl ? m_impl->length() : 0; }
    const LChar* characters8() const { return m_impl ? m_impl->characters8() : nullptr; }
    const UChar* characters16() const { return m_impl ? m_impl->characters16() : nullptr; }

    // Return characters8() or characters16() depending on CharacterType.
    template<typename CharacterType> const CharacterType* characters() const;

    bool is8Bit() const { return !m_impl || m_impl->is8Bit(); }

    unsigned sizeInBytes() const { return m_impl ? m_impl->length() * (is8Bit() ? sizeof(LChar) : sizeof(UChar)) : 0; }

    WTF_EXPORT_PRIVATE CString ascii() const;
    WTF_EXPORT_PRIVATE CString latin1() const;

    WTF_EXPORT_PRIVATE CString utf8(ConversionMode = LenientConversion) const;

    template<typename Func>
    Expected<std::invoke_result_t<Func, Span<const char>>, UTF8ConversionError> tryGetUTF8(const Func&, ConversionMode = LenientConversion) const;
    WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> tryGetUTF8(ConversionMode) const;
    WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> tryGetUTF8() const;

    UChar characterAt(unsigned index) const;
    UChar operator[](unsigned index) const { return characterAt(index); }

    WTF_EXPORT_PRIVATE static String number(int);
    WTF_EXPORT_PRIVATE static String number(unsigned);
    WTF_EXPORT_PRIVATE static String number(long);
    WTF_EXPORT_PRIVATE static String number(unsigned long);
    WTF_EXPORT_PRIVATE static String number(long long);
    WTF_EXPORT_PRIVATE static String number(unsigned long long);
    WTF_EXPORT_PRIVATE static String number(float);
    WTF_EXPORT_PRIVATE static String number(double);

    WTF_EXPORT_PRIVATE static String numberToStringFixedPrecision(float, unsigned precision = 6, TrailingZerosTruncatingPolicy = TruncateTrailingZeros);
    WTF_EXPORT_PRIVATE static String numberToStringFixedPrecision(double, unsigned precision = 6, TrailingZerosTruncatingPolicy = TruncateTrailingZeros);
    WTF_EXPORT_PRIVATE static String numberToStringFixedWidth(float, unsigned decimalPlaces);
    WTF_EXPORT_PRIVATE static String numberToStringFixedWidth(double, unsigned decimalPlaces);

    AtomString toExistingAtomString() const;

    // Find a single character or string, also with match function & latin1 forms.
    size_t find(UChar character, unsigned start = 0) const { return m_impl ? m_impl->find(character, start) : notFound; }

    size_t find(StringView) const;
    size_t find(StringView, unsigned start) const;
    size_t findIgnoringASCIICase(StringView) const;
    size_t findIgnoringASCIICase(StringView, unsigned start) const;

    template<typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, UChar>>* = nullptr>
    size_t find(CodeUnitMatchFunction matchFunction, unsigned start = 0) const { return m_impl ? m_impl->find(matchFunction, start) : notFound; }
    size_t find(ASCIILiteral literal, unsigned start = 0) const { return m_impl ? m_impl->find(literal, start) : notFound; }

    // Find the last instance of a single character or string.
    size_t reverseFind(UChar character, unsigned start = MaxLength) const { return m_impl ? m_impl->reverseFind(character, start) : notFound; }
    size_t reverseFind(ASCIILiteral literal, unsigned start = MaxLength) const { return m_impl ? m_impl->reverseFind(literal, start) : notFound; }
    size_t reverseFind(StringView, unsigned start = MaxLength) const;

    WTF_EXPORT_PRIVATE Vector<UChar> charactersWithNullTermination() const;
    WTF_EXPORT_PRIVATE Vector<UChar> charactersWithoutNullTermination() const;

    WTF_EXPORT_PRIVATE UChar32 characterStartingAt(unsigned) const;

    bool contains(UChar character) const { return find(character) != notFound; }
    bool contains(ASCIILiteral literal) const { return find(literal) != notFound; }
    bool contains(StringView) const;
    template<typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, UChar>>* = nullptr>
    bool contains(CodeUnitMatchFunction matchFunction) const { return find(matchFunction, 0) != notFound; }
    bool containsIgnoringASCIICase(StringView) const;
    bool containsIgnoringASCIICase(StringView, unsigned start) const;

    bool startsWith(StringView) const;
    bool startsWithIgnoringASCIICase(StringView) const;
    bool startsWith(UChar character) const { return m_impl && m_impl->startsWith(character); }
    bool hasInfixStartingAt(StringView prefix, unsigned start) const;

    bool endsWith(StringView) const;
    bool endsWithIgnoringASCIICase(StringView) const;
    bool endsWith(UChar character) const { return m_impl && m_impl->endsWith(character); }
    bool endsWith(char character) const { return endsWith(static_cast<UChar>(character)); }
    bool hasInfixEndingAt(StringView suffix, unsigned end) const;

    String WARN_UNUSED_RETURN substring(unsigned position, unsigned length = MaxLength) const;
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN substringSharingImpl(unsigned position, unsigned length = MaxLength) const;
    String WARN_UNUSED_RETURN left(unsigned length) const { return substring(0, length); }
    String WARN_UNUSED_RETURN right(unsigned length) const { return substring(this->length() - length, length); }

    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN convertToASCIILowercase() const;
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN convertToASCIIUppercase() const;
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN convertToLowercaseWithoutLocale() const;
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(unsigned) const;
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN convertToUppercaseWithoutLocale() const;
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN convertToLowercaseWithLocale(const AtomString& localeIdentifier) const;
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN convertToUppercaseWithLocale(const AtomString& localeIdentifier) const;

    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN stripWhiteSpace() const;
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN simplifyWhiteSpace() const;
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN simplifyWhiteSpace(CodeUnitMatchFunction) const;

    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN stripLeadingAndTrailingCharacters(CodeUnitMatchFunction) const;
    template<typename Predicate> String WARN_UNUSED_RETURN removeCharacters(const Predicate&) const;

    // Returns the string with case folded for case insensitive comparison.
    // Use convertToASCIILowercase instead if ASCII case insensitive comparison is desired.
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN foldCase() const;

    // Returns an uninitialized string. The characters needs to be written
    // into the buffer returned in data before the returned string is used.
    static String createUninitialized(unsigned length, UChar*& data) { return StringImpl::createUninitialized(length, data); }
    static String createUninitialized(unsigned length, LChar*& data) { return StringImpl::createUninitialized(length, data); }

    using SplitFunctor = WTF::Function<void(StringView)>;

    WTF_EXPORT_PRIVATE void split(UChar separator, const SplitFunctor&) const;
    WTF_EXPORT_PRIVATE Vector<String> WARN_UNUSED_RETURN split(UChar separator) const;
    WTF_EXPORT_PRIVATE Vector<String> WARN_UNUSED_RETURN split(StringView separator) const;

    WTF_EXPORT_PRIVATE void splitAllowingEmptyEntries(UChar separator, const SplitFunctor&) const;
    WTF_EXPORT_PRIVATE Vector<String> WARN_UNUSED_RETURN splitAllowingEmptyEntries(UChar separator) const;
    WTF_EXPORT_PRIVATE Vector<String> WARN_UNUSED_RETURN splitAllowingEmptyEntries(StringView separator) const;

    WTF_EXPORT_PRIVATE double toDouble(bool* ok = nullptr) const;
    WTF_EXPORT_PRIVATE float toFloat(bool* ok = nullptr) const;

    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN isolatedCopy() const &;
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN isolatedCopy() &&;

    WTF_EXPORT_PRIVATE bool isSafeToSendToAnotherThread() const;

    // Prevent Strings from being implicitly convertable to bool as it will be ambiguous on any platform that
    // allows implicit conversion to another pointer type (e.g., Mac allows implicit conversion to NSString *).
    typedef struct ImplicitConversionFromWTFStringToBoolDisallowedA* (String::*UnspecifiedBoolTypeA);
    typedef struct ImplicitConversionFromWTFStringToBoolDisallowedB* (String::*UnspecifiedBoolTypeB);
    operator UnspecifiedBoolTypeA() const;
    operator UnspecifiedBoolTypeB() const;

#if USE(CF)
    WTF_EXPORT_PRIVATE String(CFStringRef);
    WTF_EXPORT_PRIVATE RetainPtr<CFStringRef> createCFString() const;
#endif

#ifdef __OBJC__
#if HAVE(SAFARI_FOR_WEBKIT_DEVELOPMENT_REQUIRING_EXTRA_SYMBOLS)
    WTF_EXPORT_PRIVATE String(NSString *);
#else
    String(NSString *string)
        : String((__bridge CFStringRef)string) { }
#endif

    // This conversion converts the null string to an empty NSString rather than to nil.
    // Given Cocoa idioms, this is a more useful default. Clients that need to preserve the
    // null string can check isNull explicitly.
    operator NSString *() const;
#endif

#if OS(WINDOWS)
    String(const wchar_t* characters, unsigned length)
        : String(ucharFrom(characters), length) { }

    String(const wchar_t* characters)
        : String(characters, characters ? wcslen(characters) : 0) { }

    WTF_EXPORT_PRIVATE Vector<wchar_t> wideCharacters() const;
#endif

    WTF_EXPORT_PRIVATE static String make8Bit(const UChar*, unsigned);
    WTF_EXPORT_PRIVATE void convertTo16Bit();

    // String::fromUTF8 will return a null string if the input data contains invalid UTF-8 sequences.
    WTF_EXPORT_PRIVATE static String fromUTF8(const LChar*, size_t);
    WTF_EXPORT_PRIVATE static String fromUTF8(const LChar*);
    static String fromUTF8(const char* characters, size_t length) { return fromUTF8(reinterpret_cast<const LChar*>(characters), length); }
    static String fromUTF8(const char* string) { return fromUTF8(reinterpret_cast<const LChar*>(string)); }
    WTF_EXPORT_PRIVATE static String fromUTF8(const CString&);
    static String fromUTF8(const Vector<LChar>& characters);
    static String fromUTF8ReplacingInvalidSequences(const LChar*, size_t);

    // Tries to convert the passed in string to UTF-8, but will fall back to Latin-1 if the string is not valid UTF-8.
    WTF_EXPORT_PRIVATE static String fromUTF8WithLatin1Fallback(const LChar*, size_t);
    static String fromUTF8WithLatin1Fallback(const char* characters, size_t length) { return fromUTF8WithLatin1Fallback(reinterpret_cast<const LChar*>(characters), length); }

    WTF_EXPORT_PRIVATE static String fromCodePoint(UChar32 codePoint);

    // Determines the writing direction using the Unicode Bidi Algorithm rules P2 and P3.
    std::optional<UCharDirection> defaultWritingDirection() const;

    bool isAllASCII() const { return !m_impl || m_impl->isAllASCII(); }
    bool isAllLatin1() const { return !m_impl || m_impl->isAllLatin1(); }
    template<bool isSpecialCharacter(UChar)> bool isAllSpecialCharacters() const { return !m_impl || m_impl->isAllSpecialCharacters<isSpecialCharacter>(); }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    String(WTF::HashTableDeletedValueType) : m_impl(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_impl.isHashTableDeletedValue(); }

    unsigned hash() const { return isNull() ? 0 : impl()->hash(); }
    unsigned existingHash() const { return isNull() ? 0 : impl()->existingHash(); }

#ifndef NDEBUG
    WTF_EXPORT_PRIVATE void show() const;
#endif

    // Turns this String empty if the StringImpl is not referenced by anyone else.
    // This is useful for clearing String-based caches.
    void clearImplIfNotShared();

    static constexpr unsigned MaxLength = StringImpl::MaxLength;

private:
    template<bool allowEmptyEntries> void splitInternal(UChar separator, const SplitFunctor&) const;
    template<bool allowEmptyEntries> Vector<String> splitInternal(UChar separator) const;
    template<bool allowEmptyEntries> Vector<String> splitInternal(StringView separator) const;

    // This is intentionally private. Use fromLatin1() / fromUTF8() / String(ASCIILiteral) instead.
    WTF_EXPORT_PRIVATE explicit String(const char* characters);

    RefPtr<StringImpl> m_impl;
};

static_assert(sizeof(String) == sizeof(void*), "String should effectively be a pointer to a StringImpl, and efficient to pass by value");

inline bool operator==(const String& a, const String& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const String& a, ASCIILiteral b) { return equal(a.impl(), b); }
inline bool operator==(ASCIILiteral a, const String& b) { return equal(b.impl(), a); }
template<size_t inlineCapacity> inline bool operator==(const Vector<char, inlineCapacity>& a, const String& b) { return equal(b.impl(), a.data(), a.size()); }
template<size_t inlineCapacity> inline bool operator==(const String& a, const Vector<char, inlineCapacity>& b) { return b == a; }

inline bool operator!=(const String& a, const String& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const String& a, ASCIILiteral b) { return !equal(a.impl(), b); }
inline bool operator!=(ASCIILiteral a, const String& b) { return !equal(b.impl(), a); }
template<size_t inlineCapacity> inline bool operator!=(const Vector<char, inlineCapacity>& a, const String& b) { return !(a == b); }
template<size_t inlineCapacity> inline bool operator!=(const String& a, const Vector<char, inlineCapacity>& b) { return b != a; }

bool equalIgnoringASCIICase(const String&, const String&);
bool equalIgnoringASCIICase(const String&, ASCIILiteral);

bool equalLettersIgnoringASCIICase(const String&, ASCIILiteral);
bool startsWithLettersIgnoringASCIICase(const String&, ASCIILiteral);

inline bool equalIgnoringNullity(const String& a, const String& b) { return equalIgnoringNullity(a.impl(), b.impl()); }
template<size_t inlineCapacity> inline bool equalIgnoringNullity(const Vector<UChar, inlineCapacity>& a, const String& b) { return equalIgnoringNullity(a, b.impl()); }

inline bool operator!(const String& string) { return string.isNull(); }

inline void swap(String& a, String& b) { a.swap(b); }

#ifdef __OBJC__

// Used in a small number of places where the long standing behavior has been "nil if empty".
NSString * nsStringNilIfEmpty(const String&);

#endif

WTF_EXPORT_PRIVATE int codePointCompare(const String&, const String&);
bool codePointCompareLessThan(const String&, const String&);

// Shared global empty and null string.
struct StaticString {
    constexpr StaticString(StringImpl::StaticStringImpl* pointer)
        : m_pointer(pointer)
    {
    }

    StringImpl::StaticStringImpl* m_pointer;
};
static_assert(sizeof(String) == sizeof(StaticString), "String and StaticString must be the same size!");
extern WTF_EXPORT_PRIVATE const StaticString nullStringData;
extern WTF_EXPORT_PRIVATE const StaticString emptyStringData;

inline const String& nullString() { return *reinterpret_cast<const String*>(&nullStringData); }
inline const String& emptyString() { return *reinterpret_cast<const String*>(&emptyStringData); }

template<typename> struct DefaultHash;
template<> struct DefaultHash<String>;
template<> struct VectorTraits<String> : VectorTraitsBase<false, void> {
    static constexpr bool canInitializeWithMemset = true;
    static constexpr bool canMoveWithMemcpy = true;
};

template<> struct IntegerToStringConversionTrait<String> {
    using ReturnType = String;
    using AdditionalArgumentType = void;
    static String flush(LChar* characters, unsigned length, void*) { return { characters, length }; }
};

#ifdef __OBJC__
WTF_EXPORT_PRIVATE RetainPtr<id> makeNSArrayElement(const String&);
WTF_EXPORT_PRIVATE std::optional<String> makeVectorElement(const String*, id);
#endif

#if USE(CF)
WTF_EXPORT_PRIVATE RetainPtr<CFStringRef> makeCFArrayElement(const String&);
WTF_EXPORT_PRIVATE std::optional<String> makeVectorElement(const String*, CFStringRef);
#endif

// Definitions of string operations

inline String::String(StringImpl& string)
    : m_impl(&string)
{
}

inline String::String(StringImpl* string)
    : m_impl(string)
{
}

inline String::String(Ref<StringImpl>&& string)
    : m_impl(WTFMove(string))
{
}

inline String::String(RefPtr<StringImpl>&& string)
    : m_impl(WTFMove(string))
{
}

inline String::String(Ref<AtomStringImpl>&& string)
    : m_impl(WTFMove(string))
{
}

inline String::String(RefPtr<AtomStringImpl>&& string)
    : m_impl(WTFMove(string))
{
}

inline String::String(StaticStringImpl& string)
    : m_impl(reinterpret_cast<StringImpl*>(&string))
{
}

inline String::String(StaticStringImpl* string)
    : m_impl(reinterpret_cast<StringImpl*>(string))
{
}

inline String::String(ASCIILiteral characters)
    : m_impl(characters.isNull() ? nullptr : RefPtr<StringImpl> { StringImpl::create(characters) })
{
}

inline String::String(ExternalStringImpl& string)
    : m_impl(&string)
{
}

inline String::String(ExternalStringImpl* string)
    : m_impl(string)
{
}

inline String::String(Ref<ExternalStringImpl>&& string)
    : m_impl(WTFMove(string))
{
}

inline String::String(RefPtr<ExternalStringImpl>&& string)
    : m_impl(WTFMove(string))
{
}

template<> inline const LChar* String::characters<LChar>() const
{
    return characters8();
}

template<> inline const UChar* String::characters<UChar>() const
{
    return characters16();
}

inline UChar String::characterAt(unsigned index) const
{
    if (!m_impl || index >= m_impl->length())
        return 0;
    return (*m_impl)[index];
}

inline String WARN_UNUSED_RETURN makeStringByReplacingAll(const String& string, UChar target, UChar replacement)
{
    if (auto impl = string.impl())
        return String { impl->replace(target, replacement) };
    return string;
}

ALWAYS_INLINE String WARN_UNUSED_RETURN makeStringByReplacingAll(const String& string, UChar target, ASCIILiteral literal)
{
    if (auto impl = string.impl())
        return String { impl->replace(target, literal.characters(), literal.length()) };
    return string;
}

WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN makeStringByRemoving(const String&, unsigned position, unsigned lengthToRemove);

WTF_EXPORT_PRIVATE String makeStringByJoining(Span<const String> strings, const String& separator);

inline std::optional<UCharDirection> String::defaultWritingDirection() const
{
    if (m_impl)
        return m_impl->defaultWritingDirection();
    return std::nullopt;
}

inline void String::clearImplIfNotShared()
{
    if (m_impl && m_impl->hasOneRef())
        m_impl = nullptr;
}

inline String String::substring(unsigned position, unsigned length) const
{
    if (!m_impl)
        return { };

    if (!position && length >= m_impl->length())
        return *this;

    return m_impl->substring(position, length);
}

template<typename Func>
inline Expected<std::invoke_result_t<Func, Span<const char>>, UTF8ConversionError> String::tryGetUTF8(const Func& function, ConversionMode mode) const
{
    if (!m_impl) {
        constexpr const char* emptyString = "";
        return function(Span { emptyString, emptyString });
    }
    return m_impl->tryGetUTF8(function, mode);
}

#ifdef __OBJC__

inline String::operator NSString *() const
{
    if (!m_impl)
        return @"";
    return *m_impl;
}

inline NSString * nsStringNilIfEmpty(const String& string)
{
    if (string.isEmpty())
        return nil;
    return *string.impl();
}

#endif

inline bool codePointCompareLessThan(const String& a, const String& b)
{
    return codePointCompare(a.impl(), b.impl()) < 0;
}

inline String String::fromUTF8(const Vector<LChar>& characters)
{
    if (characters.isEmpty())
        return emptyString();
    return fromUTF8(characters.data(), characters.size());
}

template<typename Predicate>
String String::removeCharacters(const Predicate& findMatch) const
{
    return m_impl ? m_impl->removeCharacters(findMatch) : String { };
}

inline bool equalLettersIgnoringASCIICase(const String& string, ASCIILiteral literal)
{
    return equalLettersIgnoringASCIICase(string.impl(), literal);
}

inline bool equalIgnoringASCIICase(const String& a, const String& b)
{
    return equalIgnoringASCIICase(a.impl(), b.impl());
}

inline bool equalIgnoringASCIICase(const String& a, ASCIILiteral b)
{
    return equalIgnoringASCIICase(a.impl(), b);
}

inline bool startsWithLettersIgnoringASCIICase(const String& string, ASCIILiteral literal)
{
    return startsWithLettersIgnoringASCIICase(string.impl(), literal);
}

struct HashTranslatorASCIILiteral {
    static unsigned hash(ASCIILiteral literal)
    {
        return StringHasher::computeHashAndMaskTop8Bits(literal.characters(), literal.length());
    }

    static bool equal(const String& a, ASCIILiteral b)
    {
        return a == b;
    }

    static void translate(String& location, ASCIILiteral literal, unsigned hash)
    {
        location = literal;
        location.impl()->setHash(hash);
    }
};

inline namespace StringLiterals {

inline String operator"" _str(const char* characters, size_t)
{
    return ASCIILiteral::fromLiteralUnsafe(characters);
}

} // inline StringLiterals

} // namespace WTF

using WTF::HashTranslatorASCIILiteral;
using WTF::KeepTrailingZeros;
using WTF::String;
using WTF::charactersToDouble;
using WTF::charactersToFloat;
using WTF::emptyString;
using WTF::makeStringByJoining;
using WTF::makeStringByRemoving;
using WTF::makeStringByReplacingAll;
using WTF::nullString;
using WTF::equal;
using WTF::find;
using WTF::isAllSpecialCharacters;
using WTF::reverseFind;

#include <wtf/text/AtomString.h>
