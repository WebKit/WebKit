/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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

#include <span>

#include <wtf/Compiler.h>
#include <wtf/SwiftBridging.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/StringImpl.h>

#ifdef __OBJC__
#include <objc/objc.h>
#include <wtf/RetainPtr.h>
#endif

#if OS(WINDOWS)
#include <wtf/text/win/WCharStringExtras.h>
#endif

namespace WTF {

// Declarations of string operations

WTF_EXPORT_PRIVATE double charactersToDouble(std::span<const Latin1Character>, bool* ok = nullptr);
WTF_EXPORT_PRIVATE double charactersToDouble(std::span<const char16_t>, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(std::span<const Latin1Character>, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(std::span<const char16_t>, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(std::span<const Latin1Character>, size_t& parsedLength);
WTF_EXPORT_PRIVATE float charactersToFloat(std::span<const char16_t>, size_t& parsedLength);

template<bool isSpecialCharacter(char16_t), typename CharacterType, std::size_t Extent> bool containsOnly(std::span<const CharacterType, Extent>);

enum class TrailingZerosPolicy : bool { Keep, Truncate };

class String final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(String);
public:
    // Construct a null string, distinguishable from an empty string.
    String() = default;

    // Construct a string with UTF-16 data.
    WTF_EXPORT_PRIVATE String(std::span<const char16_t> characters);

    // Construct a string with Latin-1 data.
    WTF_EXPORT_PRIVATE String(std::span<const Latin1Character> characters);
    WTF_EXPORT_PRIVATE String(std::span<const char> characters);
    ALWAYS_INLINE static String fromLatin1(const char* characters) { return String { characters }; }

    // Construct a string with UTF-8 data, null string if it contains invalid UTF-8 sequences.
    WTF_EXPORT_PRIVATE String(std::span<const char8_t>);

    // Construct a string referencing an existing StringImpl.
    String(StringImpl&);
    String(StringImpl*);
    String(Ref<StringImpl>&&);
    String(RefPtr<StringImpl>&&);

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

    static String adopt(StringBuffer<Latin1Character>&& buffer) { return StringImpl::adopt(WTFMove(buffer)); }
    static String adopt(StringBuffer<char16_t>&& buffer) { return StringImpl::adopt(WTFMove(buffer)); }
    template<typename CharacterType, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
    static String adopt(Vector<CharacterType, inlineCapacity, OverflowHandler, minCapacity, Malloc>&& vector) { return StringImpl::adopt(WTFMove(vector)); }

    bool isNull() const { return !m_impl; }
    bool isEmpty() const { return !m_impl || m_impl->isEmpty(); }

    StringImpl* impl() const LIFETIME_BOUND { return m_impl.get(); }
    RefPtr<StringImpl> releaseImpl() { return WTFMove(m_impl); }

    unsigned length() const { return m_impl ? m_impl->length() : 0; }
    std::span<const Latin1Character> span8() const LIFETIME_BOUND { return m_impl ? m_impl->span8() : std::span<const Latin1Character>(); }
    std::span<const char16_t> span16() const LIFETIME_BOUND { return m_impl ? m_impl->span16() : std::span<const char16_t>(); }

    // Return span8() or span16() depending on CharacterType.
    template<typename CharacterType> std::span<const CharacterType> span() const LIFETIME_BOUND;

    bool is8Bit() const { return !m_impl || m_impl->is8Bit(); }

    unsigned sizeInBytes() const { return m_impl ? m_impl->length() * (is8Bit() ? sizeof(Latin1Character) : sizeof(char16_t)) : 0; }

    WTF_EXPORT_PRIVATE CString ascii() const;
    WTF_EXPORT_PRIVATE CString latin1() const;

    WTF_EXPORT_PRIVATE CString utf8(ConversionMode = LenientConversion) const;

    template<typename Func>
    Expected<std::invoke_result_t<Func, std::span<const char8_t>>, UTF8ConversionError> tryGetUTF8(NOESCAPE const Func&, ConversionMode = LenientConversion) const;
    WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> tryGetUTF8(ConversionMode) const;
    WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> tryGetUTF8() const;

    char16_t characterAt(unsigned index) const;
    char16_t operator[](unsigned index) const { return characterAt(index); }

    WTF_EXPORT_PRIVATE static String number(int);
    WTF_EXPORT_PRIVATE static String number(unsigned);
    WTF_EXPORT_PRIVATE static String number(long);
    WTF_EXPORT_PRIVATE static String number(unsigned long);
    WTF_EXPORT_PRIVATE static String number(long long);
    WTF_EXPORT_PRIVATE static String number(unsigned long long);
    WTF_EXPORT_PRIVATE static String number(float);
    WTF_EXPORT_PRIVATE static String number(double);

    WTF_EXPORT_PRIVATE static String numberToStringFixedPrecision(float, unsigned precision = 6, TrailingZerosPolicy = TrailingZerosPolicy::Truncate);
    WTF_EXPORT_PRIVATE static String numberToStringFixedPrecision(double, unsigned precision = 6, TrailingZerosPolicy = TrailingZerosPolicy::Truncate);
    WTF_EXPORT_PRIVATE static String numberToStringFixedWidth(double, unsigned decimalPlaces);

    AtomString toExistingAtomString() const;

    // Find a single character or string, also with match function & latin1 forms.
    size_t find(char16_t character, unsigned start = 0) const { return m_impl ? m_impl->find(character, start) : notFound; }
    size_t find(Latin1Character character, unsigned start = 0) const { return m_impl ? m_impl->find(character, start) : notFound; }
    size_t find(char character, unsigned start = 0) const { return m_impl ? m_impl->find(character, start) : notFound; }

    size_t find(StringView) const;
    size_t find(StringView, unsigned start) const;
    size_t findIgnoringASCIICase(StringView) const;
    size_t findIgnoringASCIICase(StringView, unsigned start) const;

    template<typename CodeUnitMatchFunction>
        requires (std::is_invocable_r_v<bool, CodeUnitMatchFunction, char16_t>)
    size_t find(CodeUnitMatchFunction matchFunction, unsigned start = 0) const { return m_impl ? m_impl->find(matchFunction, start) : notFound; }
    size_t find(ASCIILiteral literal, unsigned start = 0) const { return m_impl ? m_impl->find(literal, start) : notFound; }

    // Find the last instance of a single character or string.
    size_t reverseFind(char16_t character, unsigned start = MaxLength) const { return m_impl ? m_impl->reverseFind(character, start) : notFound; }
    size_t reverseFind(ASCIILiteral literal, unsigned start = MaxLength) const { return m_impl ? m_impl->reverseFind(literal, start) : notFound; }
    size_t reverseFind(StringView, unsigned start = MaxLength) const;

    WTF_EXPORT_PRIVATE Expected<Vector<char16_t>, UTF8ConversionError> charactersWithNullTermination() const;
    WTF_EXPORT_PRIVATE Expected<Vector<char16_t>, UTF8ConversionError> charactersWithoutNullTermination() const;

    WTF_EXPORT_PRIVATE char32_t characterStartingAt(unsigned) const;

    bool contains(char16_t character) const { return find(character) != notFound; }
    bool contains(ASCIILiteral literal) const { return find(literal) != notFound; }
    bool contains(StringView) const;
    template<typename CodeUnitMatchFunction>
        requires (std::is_invocable_r_v<bool, CodeUnitMatchFunction, char16_t>)
    bool contains(CodeUnitMatchFunction matchFunction) const { return find(matchFunction, 0) != notFound; }
    bool containsIgnoringASCIICase(StringView) const;
    bool containsIgnoringASCIICase(StringView, unsigned start) const;

    bool startsWith(StringView) const;
    bool startsWithIgnoringASCIICase(StringView) const;
    bool startsWith(char16_t character) const { return m_impl && m_impl->startsWith(character); }
    bool hasInfixStartingAt(StringView prefix, unsigned start) const;

    bool endsWith(StringView) const;
    bool endsWithIgnoringASCIICase(StringView) const;
    bool endsWith(char16_t character) const { return m_impl && m_impl->endsWith(character); }
    bool endsWith(char character) const { return endsWith(static_cast<char16_t>(character)); }
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

    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN simplifyWhiteSpace(CodeUnitMatchFunction) const;

    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN trim(CodeUnitMatchFunction) const;
    template<typename Predicate> String WARN_UNUSED_RETURN removeCharacters(const Predicate&) const;

    // Returns the string with case folded for case insensitive comparison.
    // Use convertToASCIILowercase instead if ASCII case insensitive comparison is desired.
    WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN foldCase() const;

    // Returns an uninitialized string. The characters needs to be written
    // into the buffer returned in data before the returned string is used.
    static String createUninitialized(unsigned length, std::span<char16_t>& data) { return StringImpl::createUninitialized(length, data); }
    static String createUninitialized(unsigned length, std::span<Latin1Character>& data) { return StringImpl::createUninitialized(length, data); }

    using SplitFunctor = WTF::Function<void(StringView)>;

    WTF_EXPORT_PRIVATE void split(char16_t separator, NOESCAPE const SplitFunctor&) const;
    WTF_EXPORT_PRIVATE Vector<String> WARN_UNUSED_RETURN split(char16_t separator) const;
    WTF_EXPORT_PRIVATE Vector<String> WARN_UNUSED_RETURN split(StringView separator) const;

    WTF_EXPORT_PRIVATE void splitAllowingEmptyEntries(char16_t separator, NOESCAPE const SplitFunctor&) const;
    WTF_EXPORT_PRIVATE Vector<String> WARN_UNUSED_RETURN splitAllowingEmptyEntries(char16_t separator) const;
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

#if USE(FOUNDATION) && defined(__OBJC__)
    String(NSString *string)
        : String((__bridge CFStringRef)string) { }

    // This conversion converts the null string to an empty NSString rather than to nil.
    // Given Cocoa idioms, this is a more useful default. Clients that need to preserve the
    // null string can check isNull explicitly.
    RetainPtr<NSString> createNSString() const;
#endif

#if OS(WINDOWS)
    String(const wchar_t* characters, unsigned length)
        : String({ ucharFrom(characters), length }) { }

    String(const wchar_t* characters)
        : String({ ucharFrom(characters), static_cast<size_t>(characters ? wcslen(characters) : 0) }) { }

    WTF_EXPORT_PRIVATE Vector<wchar_t> wideCharacters() const;
#endif

    WTF_EXPORT_PRIVATE static String make8Bit(std::span<const char16_t>);
    WTF_EXPORT_PRIVATE void convertTo16Bit();

    // String::fromUTF8 will return a null string if the input data contains invalid UTF-8 sequences.
    // FIXME: Deprecated: Use constructor that takes span<const char8_t>.
    WTF_EXPORT_PRIVATE static String fromUTF8(std::span<const char8_t>);
    static String fromUTF8(std::span<const Latin1Character> characters) { return byteCast<char8_t>(characters); }
    static String fromUTF8(std::span<const char> characters) { return byteCast<char8_t>(characters); }
    static String fromUTF8(const char* string) { return byteCast<char8_t>(unsafeSpan(string)); }

    // Convert each invalid UTF-8 sequence into a replacement character.
    static String fromUTF8ReplacingInvalidSequences(std::span<const char8_t>);
    static String fromUTF8ReplacingInvalidSequences(std::span<const Latin1Character> characters) { return fromUTF8ReplacingInvalidSequences(byteCast<char8_t>(characters)); }

    // Tries to convert the passed in string to UTF-8, but will fall back to Latin-1 if the string is not valid UTF-8.
    // FIXME: Deprecated: Use fromUTF8ReplacingInvalidSequences.
    WTF_EXPORT_PRIVATE static String fromUTF8WithLatin1Fallback(std::span<const char8_t>);
    static String fromUTF8WithLatin1Fallback(std::span<const Latin1Character> characters) { return fromUTF8WithLatin1Fallback(byteCast<char8_t>(characters)); }
    static String fromUTF8WithLatin1Fallback(std::span<const char> characters) { return fromUTF8WithLatin1Fallback(byteCast<char8_t>(characters)); }

    WTF_EXPORT_PRIVATE static String fromCodePoint(char32_t codePoint);

    // Determines the writing direction using the Unicode Bidi Algorithm rules P2 and P3.
    std::optional<UCharDirection> defaultWritingDirection() const;

    bool containsOnlyASCII() const { SUPPRESS_UNCOUNTED_ARG return !m_impl || m_impl->containsOnlyASCII(); }
    bool containsOnlyLatin1() const { SUPPRESS_UNCOUNTED_ARG return !m_impl || m_impl->containsOnlyLatin1(); }
    template<bool isSpecialCharacter(char16_t)> bool containsOnly() const { SUPPRESS_UNCOUNTED_ARG return !m_impl || m_impl->containsOnly<isSpecialCharacter>(); }

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
    template<bool allowEmptyEntries> void splitInternal(char16_t separator, NOESCAPE const SplitFunctor&) const;
    template<bool allowEmptyEntries> Vector<String> splitInternal(char16_t separator) const;
    template<bool allowEmptyEntries> Vector<String> splitInternal(StringView separator) const;

    // This is intentionally private. Use fromLatin1() / fromUTF8() / String(ASCIILiteral) instead.
    WTF_EXPORT_PRIVATE explicit String(const char* characters);

    RefPtr<StringImpl> m_impl;
} SWIFT_ESCAPABLE;

static_assert(sizeof(String) == sizeof(void*), "String should effectively be a pointer to a StringImpl, and efficient to pass by value");

inline bool operator==(const String& a, const String& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const String& a, ASCIILiteral b) { return equal(a.impl(), b); }
template<size_t inlineCapacity> inline bool operator==(const String& a, const Vector<char, inlineCapacity>& b) { return b == a; }

bool equalIgnoringASCIICase(const String&, const String&);
bool equalIgnoringASCIICase(const String&, ASCIILiteral);

bool equalLettersIgnoringASCIICase(const String&, ASCIILiteral);
bool startsWithLettersIgnoringASCIICase(const String&, ASCIILiteral);

inline bool equalIgnoringNullity(const String& a, const String& b) { return equalIgnoringNullity(a.impl(), b.impl()); }
template<size_t inlineCapacity> inline bool equalIgnoringNullity(const Vector<char16_t, inlineCapacity>& a, const String& b) { return equalIgnoringNullity(a, b.impl()); }

inline bool operator!(const String& string) { return string.isNull(); }

inline void swap(String& a, String& b) { a.swap(b); }

#ifdef __OBJC__

// Used in a small number of places where the long standing behavior has been "nil if empty".
RetainPtr<NSString> nsStringNilIfEmpty(const String&);

// Used in a small number of places where null strings should be converted to nil but empty strings should be maintained.
RetainPtr<NSString> nsStringNilIfNull(const String&);

#endif

WTF_EXPORT_PRIVATE std::strong_ordering codePointCompare(const String&, const String&);
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

inline const String& nullString() { SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<const String*>(&nullStringData); }
inline const String& emptyString() { SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<const String*>(&emptyStringData); }

template<typename> struct DefaultHash;
template<> struct DefaultHash<String>;
template<> struct VectorTraits<String> : VectorTraitsBase<false, void> {
    static constexpr bool canInitializeWithMemset = true;
    static constexpr bool canMoveWithMemcpy = true;
};

template<> struct IntegerToStringConversionTrait<String> {
    using ReturnType = String;
    using AdditionalArgumentType = void;
    static String flush(std::span<const Latin1Character> characters, void*) { return characters; }
};

template<> struct MarkableTraits<String> {
    static bool isEmptyValue(const String& string) { return string.isNull(); }
    static String emptyValue() { return nullString(); }
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
    : m_impl(characters.isNull() ? nullptr : RefPtr { StringImpl::create(characters) })
{
}

template<> inline std::span<const Latin1Character> String::span<Latin1Character>() const
{
    return span8();
}

template<> inline std::span<const char16_t> String::span<char16_t>() const
{
    return span16();
}

inline char16_t String::characterAt(unsigned index) const
{
    if (!m_impl || index >= m_impl->length())
        return 0;
    return m_impl->at(index);
}

inline String WARN_UNUSED_RETURN makeStringByReplacingAll(const String& string, char16_t target, char16_t replacement)
{
    if (auto impl = string.impl())
        return String { impl->replace(target, replacement) };
    return string;
}

ALWAYS_INLINE String WARN_UNUSED_RETURN makeStringByReplacingAll(const String& string, char16_t target, ASCIILiteral literal)
{
    if (auto impl = string.impl())
        return String { impl->replace(target, literal.span8()) };
    return string;
}

WTF_EXPORT_PRIVATE String WARN_UNUSED_RETURN makeStringByRemoving(const String&, unsigned position, unsigned lengthToRemove);

WTF_EXPORT_PRIVATE String makeStringByJoining(std::span<const String> strings, const String& separator);

inline std::optional<UCharDirection> String::defaultWritingDirection() const
{
    if (m_impl)
        SUPPRESS_UNCOUNTED_ARG return m_impl->defaultWritingDirection();
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

    SUPPRESS_UNCOUNTED_ARG return m_impl->substring(position, length);
}

template<typename Func>
inline Expected<std::invoke_result_t<Func, std::span<const char8_t>>, UTF8ConversionError> String::tryGetUTF8(NOESCAPE const Func& function, ConversionMode mode) const
{
    if (!m_impl)
        return function(nonNullEmptyUTF8Span());
    SUPPRESS_UNCOUNTED_ARG return m_impl->tryGetUTF8(function, mode);
}

#if USE(FOUNDATION) && defined(__OBJC__)

inline RetainPtr<NSString> String::createNSString() const
{
    if (RefPtr impl = m_impl)
        return impl->createNSString();
    return @"";
}

inline RetainPtr<NSString> nsStringNilIfEmpty(const String& string)
{
    if (string.isEmpty())
        return nil;
    return string.impl()->createNSString();
}

inline RetainPtr<NSString> nsStringNilIfNull(const String& string)
{
    if (string.isNull())
        return nil;
    return string.impl()->createNSString();
}

#endif

inline bool codePointCompareLessThan(const String& a, const String& b)
{
    return codePointCompare(a.impl(), b.impl()) < 0;
}

template<typename Predicate>
String String::removeCharacters(const Predicate& findMatch) const
{
    SUPPRESS_UNCOUNTED_ARG return m_impl ? m_impl->removeCharacters(findMatch) : String { };
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

inline namespace StringLiterals {

inline String operator""_str(const char* characters, size_t)
{
    return ASCIILiteral::fromLiteralUnsafe(characters);
}

inline String operator""_str(const char16_t* characters, size_t length)
{
    return String(unsafeMakeSpan(characters, length));
}

} // inline StringLiterals

} // namespace WTF

using WTF::TrailingZerosPolicy;
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
using WTF::containsOnly;
using WTF::reverseFind;
using WTF::codePointCompareLessThan;

#include <wtf/text/AtomString.h>
