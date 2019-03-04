/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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

#include <stdarg.h>
#include <wtf/Function.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/StringImpl.h>

#ifdef __OBJC__
#include <objc/objc.h>
#endif

namespace WTF {

// Declarations of string operations

WTF_EXPORT_PRIVATE int charactersToIntStrict(const LChar*, size_t, bool* ok = nullptr, int base = 10);
WTF_EXPORT_PRIVATE int charactersToIntStrict(const UChar*, size_t, bool* ok = nullptr, int base = 10);
WTF_EXPORT_PRIVATE unsigned charactersToUIntStrict(const LChar*, size_t, bool* ok = nullptr, int base = 10);
WTF_EXPORT_PRIVATE unsigned charactersToUIntStrict(const UChar*, size_t, bool* ok = nullptr, int base = 10);
int64_t charactersToInt64Strict(const LChar*, size_t, bool* ok = nullptr, int base = 10);
int64_t charactersToInt64Strict(const UChar*, size_t, bool* ok = nullptr, int base = 10);
WTF_EXPORT_PRIVATE uint64_t charactersToUInt64Strict(const LChar*, size_t, bool* ok = nullptr, int base = 10);
WTF_EXPORT_PRIVATE uint64_t charactersToUInt64Strict(const UChar*, size_t, bool* ok = nullptr, int base = 10);
intptr_t charactersToIntPtrStrict(const LChar*, size_t, bool* ok = nullptr, int base = 10);
intptr_t charactersToIntPtrStrict(const UChar*, size_t, bool* ok = nullptr, int base = 10);

WTF_EXPORT_PRIVATE int charactersToInt(const LChar*, size_t, bool* ok = nullptr); // ignores trailing garbage
WTF_EXPORT_PRIVATE int charactersToInt(const UChar*, size_t, bool* ok = nullptr); // ignores trailing garbage
unsigned charactersToUInt(const LChar*, size_t, bool* ok = nullptr); // ignores trailing garbage
unsigned charactersToUInt(const UChar*, size_t, bool* ok = nullptr); // ignores trailing garbage
int64_t charactersToInt64(const LChar*, size_t, bool* ok = nullptr); // ignores trailing garbage
int64_t charactersToInt64(const UChar*, size_t, bool* ok = nullptr); // ignores trailing garbage
uint64_t charactersToUInt64(const LChar*, size_t, bool* ok = nullptr); // ignores trailing garbage
WTF_EXPORT_PRIVATE uint64_t charactersToUInt64(const UChar*, size_t, bool* ok = nullptr); // ignores trailing garbage
intptr_t charactersToIntPtr(const LChar*, size_t, bool* ok = nullptr); // ignores trailing garbage
intptr_t charactersToIntPtr(const UChar*, size_t, bool* ok = nullptr); // ignores trailing garbage

// FIXME: Like the strict functions above, these give false for "ok" when there is trailing garbage.
// Like the non-strict functions above, these return the value when there is trailing garbage.
// It would be better if these were more consistent with the above functions instead.
WTF_EXPORT_PRIVATE double charactersToDouble(const LChar*, size_t, bool* ok = nullptr);
WTF_EXPORT_PRIVATE double charactersToDouble(const UChar*, size_t, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(const LChar*, size_t, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(const UChar*, size_t, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(const LChar*, size_t, size_t& parsedLength);
WTF_EXPORT_PRIVATE float charactersToFloat(const UChar*, size_t, size_t& parsedLength);

template<bool isSpecialCharacter(UChar), typename CharacterType> bool isAllSpecialCharacters(const CharacterType*, size_t);

enum TrailingZerosTruncatingPolicy { KeepTrailingZeros, TruncateTrailingZeros };

class String {
public:
    // Construct a null string, distinguishable from an empty string.
    String() = default;

    // Construct a string with UTF-16 data.
    WTF_EXPORT_PRIVATE String(const UChar* characters, unsigned length);

    // Construct a string by copying the contents of a vector.  To avoid
    // copying, consider using String::adopt instead.
    // This method will never create a null string. Vectors with size() == 0
    // will return the empty string.
    // NOTE: This is different from String(vector.data(), vector.size())
    // which will sometimes return a null string when vector.data() is null
    // which can only occur for vectors without inline capacity.
    // See: https://bugs.webkit.org/show_bug.cgi?id=109792
    template<size_t inlineCapacity, typename OverflowHandler>
    explicit String(const Vector<UChar, inlineCapacity, OverflowHandler>&);

    // Construct a string with UTF-16 data, from a null-terminated source.
    WTF_EXPORT_PRIVATE String(const UChar*);

    // Construct a string with latin1 data.
    WTF_EXPORT_PRIVATE String(const LChar* characters, unsigned length);
    WTF_EXPORT_PRIVATE String(const char* characters, unsigned length);

    // Construct a string with latin1 data, from a null-terminated source.
    WTF_EXPORT_PRIVATE String(const LChar* characters);
    WTF_EXPORT_PRIVATE String(const char* characters);

    // Construct a string referencing an existing StringImpl.
    String(StringImpl&);
    String(StringImpl*);
    String(Ref<StringImpl>&&);
    String(RefPtr<StringImpl>&&);

    String(Ref<AtomicStringImpl>&&);
    String(RefPtr<AtomicStringImpl>&&);

    String(StaticStringImpl&);
    String(StaticStringImpl*);

    // Construct a string from a constant string literal.
    WTF_EXPORT_PRIVATE String(ASCIILiteral);

    // Construct a string from a constant string literal.
    // This constructor is the "big" version, as it put the length in the function call and generate bigger code.
    enum ConstructFromLiteralTag { ConstructFromLiteral };
    template<unsigned characterCount> String(const char (&characters)[characterCount], ConstructFromLiteralTag) : m_impl(StringImpl::createFromLiteral<characterCount>(characters)) { }

    // FIXME: Why do we have to define these explicitly given that we just want the default versions?
    // We have verified empirically that we do.
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

    WTF_EXPORT_PRIVATE CString utf8(ConversionMode) const;
    WTF_EXPORT_PRIVATE CString utf8() const;

    WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> tryGetUtf8(ConversionMode) const;
    WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> tryGetUtf8() const;

    UChar characterAt(unsigned index) const;
    UChar operator[](unsigned index) const { return characterAt(index); }

    WTF_EXPORT_PRIVATE static String number(int);
    WTF_EXPORT_PRIVATE static String number(unsigned);
    WTF_EXPORT_PRIVATE static String number(long);
    WTF_EXPORT_PRIVATE static String number(unsigned long);
    WTF_EXPORT_PRIVATE static String number(long long);
    WTF_EXPORT_PRIVATE static String number(unsigned long long);
    // FIXME: Change number to be numberToStringShortest instead of numberToStringFixedPrecision.
    static String number(float);
    static String number(double, unsigned precision = 6, TrailingZerosTruncatingPolicy = TruncateTrailingZeros);

    WTF_EXPORT_PRIVATE static String numberToStringShortest(float);
    WTF_EXPORT_PRIVATE static String numberToStringShortest(double);
    WTF_EXPORT_PRIVATE static String numberToStringFixedPrecision(float, unsigned precision = 6, TrailingZerosTruncatingPolicy = TruncateTrailingZeros);
    WTF_EXPORT_PRIVATE static String numberToStringFixedPrecision(double, unsigned precision = 6, TrailingZerosTruncatingPolicy = TruncateTrailingZeros);
    WTF_EXPORT_PRIVATE static String numberToStringFixedWidth(float, unsigned decimalPlaces);
    WTF_EXPORT_PRIVATE static String numberToStringFixedWidth(double, unsigned decimalPlaces);

    // FIXME: Delete in favor of the name numberToStringShortest or just number.
    static String numberToStringECMAScript(float);
    static String numberToStringECMAScript(double);

    // Find a single character or string, also with match function & latin1 forms.
    size_t find(UChar character, unsigned start = 0) const { return m_impl ? m_impl->find(character, start) : notFound; }

    size_t find(const String& string) const { return m_impl ? m_impl->find(string.impl()) : notFound; }
    size_t find(const String& string, unsigned start) const { return m_impl ? m_impl->find(string.impl(), start) : notFound; }
    size_t findIgnoringASCIICase(const String& string) const { return m_impl ? m_impl->findIgnoringASCIICase(string.impl()) : notFound; }
    size_t findIgnoringASCIICase(const String& string, unsigned startOffset) const { return m_impl ? m_impl->findIgnoringASCIICase(string.impl(), startOffset) : notFound; }

    size_t find(CodeUnitMatchFunction matchFunction, unsigned start = 0) const { return m_impl ? m_impl->find(matchFunction, start) : notFound; }
    size_t find(const LChar* string, unsigned start = 0) const { return m_impl ? m_impl->find(string, start) : notFound; }

    // Find the last instance of a single character or string.
    size_t reverseFind(UChar character, unsigned start = MaxLength) const { return m_impl ? m_impl->reverseFind(character, start) : notFound; }
    size_t reverseFind(const String& string, unsigned start = MaxLength) const { return m_impl ? m_impl->reverseFind(string.impl(), start) : notFound; }

    WTF_EXPORT_PRIVATE Vector<UChar> charactersWithNullTermination() const;

    WTF_EXPORT_PRIVATE UChar32 characterStartingAt(unsigned) const;

    bool contains(UChar character) const { return find(character) != notFound; }
    bool contains(const LChar* string) const { return find(string) != notFound; }
    bool contains(const String& string) const { return find(string) != notFound; }
    bool containsIgnoringASCIICase(const String& string) const { return findIgnoringASCIICase(string) != notFound; }
    bool containsIgnoringASCIICase(const String& string, unsigned startOffset) const { return findIgnoringASCIICase(string, startOffset) != notFound; }

    bool startsWith(const String& string) const { return m_impl ? m_impl->startsWith(string.impl()) : string.isEmpty(); }
    bool startsWithIgnoringASCIICase(const String& string) const { return m_impl ? m_impl->startsWithIgnoringASCIICase(string.impl()) : string.isEmpty(); }
    bool startsWith(UChar character) const { return m_impl && m_impl->startsWith(character); }
    template<unsigned matchLength> bool startsWith(const char (&prefix)[matchLength]) const { return m_impl ? m_impl->startsWith<matchLength>(prefix) : !matchLength; }
    bool hasInfixStartingAt(const String& prefix, unsigned startOffset) const { return m_impl && prefix.impl() && m_impl->hasInfixStartingAt(*prefix.impl(), startOffset); }

    bool endsWith(const String& string) const { return m_impl ? m_impl->endsWith(string.impl()) : string.isEmpty(); }
    bool endsWithIgnoringASCIICase(const String& string) const { return m_impl ? m_impl->endsWithIgnoringASCIICase(string.impl()) : string.isEmpty(); }
    bool endsWith(UChar character) const { return m_impl && m_impl->endsWith(character); }
    bool endsWith(char character) const { return endsWith(static_cast<UChar>(character)); }
    template<unsigned matchLength> bool endsWith(const char (&prefix)[matchLength]) const { return m_impl ? m_impl->endsWith<matchLength>(prefix) : !matchLength; }
    bool hasInfixEndingAt(const String& suffix, unsigned endOffset) const { return m_impl && suffix.impl() && m_impl->hasInfixEndingAt(*suffix.impl(), endOffset); }

    WTF_EXPORT_PRIVATE void append(const String&);
    WTF_EXPORT_PRIVATE void append(LChar);
    void append(char character) { append(static_cast<LChar>(character)); };
    WTF_EXPORT_PRIVATE void append(UChar);
    WTF_EXPORT_PRIVATE void append(const LChar*, unsigned length);
    WTF_EXPORT_PRIVATE void append(const UChar*, unsigned length);
    WTF_EXPORT_PRIVATE void insert(const String&, unsigned position);

    String& replace(UChar target, UChar replacement);
    String& replace(UChar target, const String& replacement);
    String& replace(const String& target, const String& replacement);
    String& replace(unsigned start, unsigned length, const String& replacement);
    template<unsigned characterCount> String& replaceWithLiteral(UChar target, const char (&replacement)[characterCount]);

    WTF_EXPORT_PRIVATE void truncate(unsigned length);
    WTF_EXPORT_PRIVATE void remove(unsigned position, unsigned length = 1);

    WTF_EXPORT_PRIVATE String substring(unsigned position, unsigned length = MaxLength) const;
    WTF_EXPORT_PRIVATE String substringSharingImpl(unsigned position, unsigned length = MaxLength) const;
    String left(unsigned length) const { return substring(0, length); }
    String right(unsigned length) const { return substring(this->length() - length, length); }

    WTF_EXPORT_PRIVATE String convertToASCIILowercase() const;
    WTF_EXPORT_PRIVATE String convertToASCIIUppercase() const;
    WTF_EXPORT_PRIVATE String convertToLowercaseWithoutLocale() const;
    WTF_EXPORT_PRIVATE String convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(unsigned) const;
    WTF_EXPORT_PRIVATE String convertToUppercaseWithoutLocale() const;
    WTF_EXPORT_PRIVATE String convertToLowercaseWithLocale(const AtomicString& localeIdentifier) const;
    WTF_EXPORT_PRIVATE String convertToUppercaseWithLocale(const AtomicString& localeIdentifier) const;

    WTF_EXPORT_PRIVATE String stripWhiteSpace() const;
    WTF_EXPORT_PRIVATE String simplifyWhiteSpace() const;
    WTF_EXPORT_PRIVATE String simplifyWhiteSpace(CodeUnitMatchFunction) const;

    WTF_EXPORT_PRIVATE String stripLeadingAndTrailingCharacters(CodeUnitMatchFunction) const;
    WTF_EXPORT_PRIVATE String removeCharacters(CodeUnitMatchFunction) const;

    // Returns the string with case folded for case insensitive comparison.
    // Use convertToASCIILowercase instead if ASCII case insensitive comparison is desired.
    WTF_EXPORT_PRIVATE String foldCase() const;

    // Returns an uninitialized string. The characters needs to be written
    // into the buffer returned in data before the returned string is used.
    static String createUninitialized(unsigned length, UChar*& data) { return StringImpl::createUninitialized(length, data); }
    static String createUninitialized(unsigned length, LChar*& data) { return StringImpl::createUninitialized(length, data); }

    using SplitFunctor = WTF::Function<void(const StringView&)>;

    WTF_EXPORT_PRIVATE void split(UChar separator, const SplitFunctor&) const;
    WTF_EXPORT_PRIVATE Vector<String> split(UChar separator) const;
    WTF_EXPORT_PRIVATE Vector<String> split(const String& separator) const;

    WTF_EXPORT_PRIVATE void splitAllowingEmptyEntries(UChar separator, const SplitFunctor&) const;
    WTF_EXPORT_PRIVATE Vector<String> splitAllowingEmptyEntries(UChar separator) const;
    WTF_EXPORT_PRIVATE Vector<String> splitAllowingEmptyEntries(const String& separator) const;

    WTF_EXPORT_PRIVATE int toIntStrict(bool* ok = nullptr, int base = 10) const;
    WTF_EXPORT_PRIVATE unsigned toUIntStrict(bool* ok = nullptr, int base = 10) const;
    WTF_EXPORT_PRIVATE int64_t toInt64Strict(bool* ok = nullptr, int base = 10) const;
    WTF_EXPORT_PRIVATE uint64_t toUInt64Strict(bool* ok = nullptr, int base = 10) const;
    WTF_EXPORT_PRIVATE intptr_t toIntPtrStrict(bool* ok = nullptr, int base = 10) const;

    WTF_EXPORT_PRIVATE int toInt(bool* ok = nullptr) const;
    WTF_EXPORT_PRIVATE unsigned toUInt(bool* ok = nullptr) const;
    WTF_EXPORT_PRIVATE int64_t toInt64(bool* ok = nullptr) const;
    WTF_EXPORT_PRIVATE uint64_t toUInt64(bool* ok = nullptr) const;
    WTF_EXPORT_PRIVATE intptr_t toIntPtr(bool* ok = nullptr) const;

    // FIXME: Like the strict functions above, these give false for "ok" when there is trailing garbage.
    // Like the non-strict functions above, these return the value when there is trailing garbage.
    // It would be better if these were more consistent with the above functions instead.
    WTF_EXPORT_PRIVATE double toDouble(bool* ok = nullptr) const;
    WTF_EXPORT_PRIVATE float toFloat(bool* ok = nullptr) const;

    bool percentage(int& percentage) const;

    WTF_EXPORT_PRIVATE String isolatedCopy() const &;
    WTF_EXPORT_PRIVATE String isolatedCopy() &&;

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
    WTF_EXPORT_PRIVATE String(NSString *);

    // This conversion converts the null string to an empty NSString rather than to nil.
    // Given Cocoa idioms, this is a more useful default. Clients that need to preserve the
    // null string can check isNull explicitly.
    operator NSString *() const;
#endif

    WTF_EXPORT_PRIVATE static String make8BitFrom16BitSource(const UChar*, size_t);
    template<size_t inlineCapacity> static String make8BitFrom16BitSource(const Vector<UChar, inlineCapacity>&);

    WTF_EXPORT_PRIVATE static String make16BitFrom8BitSource(const LChar*, size_t);

    // String::fromUTF8 will return a null string if
    // the input data contains invalid UTF-8 sequences.
    WTF_EXPORT_PRIVATE static String fromUTF8(const LChar*, size_t);
    WTF_EXPORT_PRIVATE static String fromUTF8(const LChar*);
    static String fromUTF8(const char* characters, size_t length) { return fromUTF8(reinterpret_cast<const LChar*>(characters), length); };
    static String fromUTF8(const char* string) { return fromUTF8(reinterpret_cast<const LChar*>(string)); };
    WTF_EXPORT_PRIVATE static String fromUTF8(const CString&);
    static String fromUTF8(const Vector<LChar>& characters);

    // Tries to convert the passed in string to UTF-8, but will fall back to Latin-1 if the string is not valid UTF-8.
    WTF_EXPORT_PRIVATE static String fromUTF8WithLatin1Fallback(const LChar*, size_t);
    static String fromUTF8WithLatin1Fallback(const char* characters, size_t length) { return fromUTF8WithLatin1Fallback(reinterpret_cast<const LChar*>(characters), length); };

    // Determines the writing direction using the Unicode Bidi Algorithm rules P2 and P3.
    UCharDirection defaultWritingDirection(bool* hasStrongDirectionality = nullptr) const;

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
    template<typename CharacterType> void removeInternal(const CharacterType*, unsigned, unsigned);

    template<bool allowEmptyEntries> void splitInternal(UChar separator, const SplitFunctor&) const;
    template<bool allowEmptyEntries> Vector<String> splitInternal(UChar separator) const;
    template<bool allowEmptyEntries> Vector<String> splitInternal(const String& separator) const;

    RefPtr<StringImpl> m_impl;
};

static_assert(sizeof(String) == sizeof(void*), "String should effectively be a pointer to a StringImpl, and efficient to pass by value");

inline bool operator==(const String& a, const String& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const String& a, const LChar* b) { return equal(a.impl(), b); }
inline bool operator==(const String& a, const char* b) { return equal(a.impl(), reinterpret_cast<const LChar*>(b)); }
inline bool operator==(const String& a, ASCIILiteral b) { return equal(a.impl(), reinterpret_cast<const LChar*>(b.characters())); }
inline bool operator==(const LChar* a, const String& b) { return equal(a, b.impl()); }
inline bool operator==(const char* a, const String& b) { return equal(reinterpret_cast<const LChar*>(a), b.impl()); }
inline bool operator==(ASCIILiteral a, const String& b) { return equal(reinterpret_cast<const LChar*>(a.characters()), b.impl()); }
template<size_t inlineCapacity> inline bool operator==(const Vector<char, inlineCapacity>& a, const String& b) { return equal(b.impl(), a.data(), a.size()); }
template<size_t inlineCapacity> inline bool operator==(const String& a, const Vector<char, inlineCapacity>& b) { return b == a; }

inline bool operator!=(const String& a, const String& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const String& a, const LChar* b) { return !equal(a.impl(), b); }
inline bool operator!=(const String& a, const char* b) { return !equal(a.impl(), reinterpret_cast<const LChar*>(b)); }
inline bool operator!=(const String& a, ASCIILiteral b) { return !equal(a.impl(), reinterpret_cast<const LChar*>(b.characters())); }
inline bool operator!=(const LChar* a, const String& b) { return !equal(a, b.impl()); }
inline bool operator!=(const char* a, const String& b) { return !equal(reinterpret_cast<const LChar*>(a), b.impl()); }
inline bool operator!=(ASCIILiteral a, const String& b) { return !equal(reinterpret_cast<const LChar*>(a.characters()), b.impl()); }
template<size_t inlineCapacity> inline bool operator!=(const Vector<char, inlineCapacity>& a, const String& b) { return !(a == b); }
template<size_t inlineCapacity> inline bool operator!=(const String& a, const Vector<char, inlineCapacity>& b) { return b != a; }

bool equalIgnoringASCIICase(const String&, const String&);
bool equalIgnoringASCIICase(const String&, const char*);

template<unsigned length> bool equalLettersIgnoringASCIICase(const String&, const char (&lowercaseLetters)[length]);
template<unsigned length> bool startsWithLettersIgnoringASCIICase(const String&, const char (&lowercaseLetters)[length]);

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

template<typename CharacterType> void appendNumber(Vector<CharacterType>&, unsigned char number);

// Shared global empty and null string.
WTF_EXPORT_PRIVATE const String& emptyString();
WTF_EXPORT_PRIVATE const String& nullString();

template<typename> struct DefaultHash;
template<> struct DefaultHash<String> { using Hash = StringHash; };
template<> struct VectorTraits<String> : VectorTraitsBase<false, void> {
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template<> struct IntegerToStringConversionTrait<String> {
    using ReturnType = String;
    using AdditionalArgumentType = void;
    static String flush(LChar* characters, unsigned length, void*) { return { characters, length }; }
};

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

inline String::String(Ref<AtomicStringImpl>&& string)
    : m_impl(WTFMove(string))
{
}

inline String::String(RefPtr<AtomicStringImpl>&& string)
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

template<size_t inlineCapacity, typename OverflowHandler> String::String(const Vector<UChar, inlineCapacity, OverflowHandler>& vector)
    : m_impl(vector.size() ? StringImpl::create(vector.data(), vector.size()) : Ref<StringImpl> { *StringImpl::empty() })
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

inline String& String::replace(UChar target, UChar replacement)
{
    if (m_impl)
        m_impl = m_impl->replace(target, replacement);
    return *this;
}

inline String& String::replace(UChar target, const String& replacement)
{
    if (m_impl)
        m_impl = m_impl->replace(target, replacement.impl());
    return *this;
}

inline String& String::replace(const String& target, const String& replacement)
{
    if (m_impl)
        m_impl = m_impl->replace(target.impl(), replacement.impl());
    return *this;
}

inline String& String::replace(unsigned start, unsigned length, const String& replacement)
{
    if (m_impl)
        m_impl = m_impl->replace(start, length, replacement.impl());
    return *this;
}

template<unsigned characterCount> ALWAYS_INLINE String& String::replaceWithLiteral(UChar target, const char (&characters)[characterCount])
{
    if (m_impl)
        m_impl = m_impl->replace(target, characters, characterCount - 1);
    return *this;
}

template<size_t inlineCapacity> inline String String::make8BitFrom16BitSource(const Vector<UChar, inlineCapacity>& buffer)
{
    return make8BitFrom16BitSource(buffer.data(), buffer.size());
}

inline UCharDirection String::defaultWritingDirection(bool* hasStrongDirectionality) const
{
    if (m_impl)
        return m_impl->defaultWritingDirection(hasStrongDirectionality);
    if (hasStrongDirectionality)
        *hasStrongDirectionality = false;
    return U_LEFT_TO_RIGHT;
}

inline void String::clearImplIfNotShared()
{
    if (m_impl && m_impl->hasOneRef())
        m_impl = nullptr;
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

template<typename CharacterType>
inline void appendNumber(Vector<CharacterType>& vector, unsigned char number)
{
    int numberLength = number > 99 ? 3 : (number > 9 ? 2 : 1);
    size_t vectorSize = vector.size();
    vector.grow(vectorSize + numberLength);

    switch (numberLength) {
    case 3:
        vector[vectorSize + 2] = number % 10 + '0';
        number /= 10;
        FALLTHROUGH;

    case 2:
        vector[vectorSize + 1] = number % 10 + '0';
        number /= 10;
        FALLTHROUGH;

    case 1:
        vector[vectorSize] = number % 10 + '0';
    }
}

inline String String::fromUTF8(const Vector<LChar>& characters)
{
    if (characters.isEmpty())
        return emptyString();
    return fromUTF8(characters.data(), characters.size());
}

template<unsigned length> inline bool equalLettersIgnoringASCIICase(const String& string, const char (&lowercaseLetters)[length])
{
    return equalLettersIgnoringASCIICase(string.impl(), lowercaseLetters);
}

inline bool equalIgnoringASCIICase(const String& a, const String& b)
{
    return equalIgnoringASCIICase(a.impl(), b.impl());
}

inline bool equalIgnoringASCIICase(const String& a, const char* b)
{
    return equalIgnoringASCIICase(a.impl(), b);
}

template<unsigned length> inline bool startsWithLettersIgnoringASCIICase(const String& string, const char (&lowercaseLetters)[length])
{
    return startsWithLettersIgnoringASCIICase(string.impl(), lowercaseLetters);
}

inline String String::number(float number)
{
    return numberToStringFixedPrecision(number);
}

inline String String::number(double number, unsigned precision, TrailingZerosTruncatingPolicy policy)
{
    return numberToStringFixedPrecision(number, precision, policy);
}

inline String String::numberToStringECMAScript(float number)
{
    // FIXME: This preserves existing behavior but is not what we want.
    // In the future, this should either be a compilation error or call numberToStringShortest without converting to double.
    return numberToStringShortest(static_cast<double>(number));
}

inline String String::numberToStringECMAScript(double number)
{
    return numberToStringShortest(number);
}

inline namespace StringLiterals {

inline String operator"" _str(const char* characters, size_t)
{
    return ASCIILiteral::fromLiteralUnsafe(characters);
}

} // inline StringLiterals

} // namespace WTF

using WTF::KeepTrailingZeros;
using WTF::String;
using WTF::appendNumber;
using WTF::charactersToDouble;
using WTF::charactersToFloat;
using WTF::charactersToInt64;
using WTF::charactersToInt64Strict;
using WTF::charactersToInt;
using WTF::charactersToIntPtr;
using WTF::charactersToIntPtrStrict;
using WTF::charactersToIntStrict;
using WTF::charactersToUInt64;
using WTF::charactersToUInt64Strict;
using WTF::charactersToUInt;
using WTF::charactersToUIntStrict;
using WTF::emptyString;
using WTF::nullString;
using WTF::equal;
using WTF::find;
using WTF::isAllSpecialCharacters;
using WTF::isSpaceOrNewline;
using WTF::reverseFind;

#include <wtf/text/AtomicString.h>
