/*
 * Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#pragma once

#include <concepts>
#include <span>
#include <wtf/DebugHeap.h>
#include <wtf/Forward.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SwiftBridging.h>
#include <wtf/text/ASCIIFastPath.h>
#include <wtf/text/Latin1Character.h>
#include <wtf/unicode/UTF8Conversion.h>

#ifdef __OBJC__
#include <objc/objc.h>
#include <wtf/RetainPtr.h>
#endif

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CStringBuffer);

// CStringBuffer is the ref-counted storage class for the characters in a CString.
// The data is implicitly allocated 1 character longer than length(), as it is zero-terminated.
class CStringBuffer final : public RefCounted<CStringBuffer> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CStringBuffer, CStringBuffer);
public:
    size_t length() const { return m_length; }

    std::span<const char> span() const LIFETIME_BOUND { return unsafeMakeSpan(m_data, m_length); }
    std::span<const char> spanIncludingNullTerminator() const LIFETIME_BOUND { return unsafeMakeSpan(m_data, m_length + 1); }

private:
    friend class CString;

    static Ref<CStringBuffer> createUninitialized(size_t length);

    CStringBuffer(size_t length) : m_length(length) { }
    std::span<char> mutableSpan() LIFETIME_BOUND { return unsafeMakeSpan(m_data, m_length); }
    std::span<char> mutableSpanIncludingNullTerminator() LIFETIME_BOUND { return unsafeMakeSpan(m_data, m_length + 1); }

    const size_t m_length;
    char m_data[0];
};

// NOTE: Prefer using String.

// A null-terminated, nullable, copy-on-write char array. Useful for interacting with C-style APIs.

// Like const char*, CString does not know its encoding. The caller must apply the right encoding when extracting characters.
// Prefer CStringWithEncoding (UTF8CString / Latin1CString / ASCIICString) below, which carries the encoding in the type.
class CString {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CString);
public:
    CString() { }
    WTF_EXPORT_PRIVATE CString(ASCIILiteral);
    WTF_EXPORT_PRIVATE CString(const char*); // Any encoding
    WTF_EXPORT_PRIVATE CString(std::span<const char>); // Any encoding
    // FIXME: These two should be removed. They are the exact ambiguity CStringWithEncoding exists to
    // eliminate: both encodings land in the same char buffer and become indistinguishable.
    CString(std::span<const Latin1Character>); // Latin1
    CString(std::span<const char8_t> characters) : CString(byteCast<Latin1Character>(characters)) { } // UTF-8
    CString(CStringBuffer* buffer) : m_buffer(buffer) { }
    CString(const std::string&); // Any encoding.
    WTF_EXPORT_PRIVATE static CString newUninitialized(size_t length, std::span<char>& characterBuffer);
    CString(HashTableDeletedValueType) : m_buffer(HashTableDeletedValue) { }

    const char* data() const LIFETIME_BOUND; // Any encoding

    // Escape hatch for external C functions and printf-style formatting, matching
    // CStringWithEncoding::legacyCStringPointer() below. Unlike data(), this keeps returning const char*
    // as producers are migrated to the encoding-aware types. Named for the destination rather than the
    // contents: const char* is what C string interfaces take, which is why it is char and not char8_t.
    const char* legacyCStringPointer() const LIFETIME_BOUND { return data(); } // Any encoding

    std::string toStdString() const;

    std::span<const char> span() const LIFETIME_BOUND; // Any encoding
    std::span<const char> spanIncludingNullTerminator() const LIFETIME_BOUND; // Any encoding

    // Copy-on-write
    WTF_EXPORT_PRIVATE std::span<char> mutableSpan() LIFETIME_BOUND;
    WTF_EXPORT_PRIVATE std::span<char> mutableSpanIncludingNullTerminator() LIFETIME_BOUND;
    WTF_EXPORT_PRIVATE void grow(size_t newLength);

    size_t length() const;

    bool isNull() const { return !m_buffer; }
    bool isEmpty() const { return isNull() || !m_buffer->length(); }
    bool NODELETE isSafeToSendToAnotherThread() const;

    CStringBuffer* buffer() const LIFETIME_BOUND { return m_buffer.get(); }

    bool isHashTableDeletedValue() const { return m_buffer.isHashTableDeletedValue(); }

    WTF_EXPORT_PRIVATE unsigned NODELETE hash() const;

private:
    void copyBufferIfNeeded();
    void init(std::span<const char>);
    RefPtr<CStringBuffer> m_buffer;
} SWIFT_ESCAPABLE;

WTF_EXPORT_PRIVATE bool NODELETE operator==(const CString&, const CString&);
WTF_EXPORT_PRIVATE bool NODELETE operator==(const CString&, ASCIILiteral);
WTF_EXPORT_PRIVATE bool operator<(const CString&, const CString&);

WTF_EXPORT_PRIVATE UTF8CString convertToASCIILowercase(std::span<const char8_t>);
WTF_EXPORT_PRIVATE UTF8CString convertToASCIIUppercase(std::span<const char8_t>);

struct CStringHash {
    static unsigned hash(const CString& string) { return string.hash(); }
    WTF_EXPORT_PRIVATE static bool NODELETE equal(const CString& a, const CString& b);
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<typename> struct DefaultHash;
template<> struct DefaultHash<CString> : CStringHash { };

template<typename> struct HashTraits;
template<> struct HashTraits<CString> : SimpleClassHashTraits<CString> { };

inline CString::CString(std::span<const Latin1Character> bytes)
    : CString(byteCast<char>(bytes))
{
}

inline CString::CString(const std::string& value)
    : CString(unsafeMakeSpan(value.data(), value.size()))
{
}

inline const char* CString::data() const LIFETIME_BOUND
{
    return m_buffer ? m_buffer->spanIncludingNullTerminator().data() : nullptr;
}

inline std::span<const char> CString::span() const LIFETIME_BOUND
{
    if (m_buffer)
        return m_buffer->span();
    return { };
}

inline std::span<const char> CString::spanIncludingNullTerminator() const LIFETIME_BOUND
{
    if (m_buffer)
        return m_buffer->spanIncludingNullTerminator();
    return { };
}

inline size_t CString::length() const
{
    return m_buffer ? m_buffer->length() : 0;
}

inline std::string CString::toStdString() const
{
    auto span = this->span();
    return std::string { span.data(), span.size() };
}

// CString is null terminated
inline const char* safePrintfType(const CString& cstring) { return cstring.data(); }

// A CString that remembers the encoding of its bytes. Converts implicitly to CString, which erases the encoding.
// The character type carries the encoding, following WTF convention: char8_t is UTF-8, Latin1Character is
// Latin-1, and char is ASCII, as in ASCIILiteral. For char that is a representation, not a meaning: a char in
// CString itself means "any encoding", and ASCII is spelled with char here only because const char* is what C
// string interfaces take, which is what an ASCII string is for. ASCIICString::data() is therefore directly
// usable by those interfaces and needs no escape hatch.
//
// Unlike ASCIILiteral, ASCIICString cannot enforce that its bytes really are ASCII: newUninitialized hands out
// a mutable buffer, so there is no point at which the contents can be checked. It is therefore defined as
// Latin-1 restricted to 0..127, and every operation treats it that way. A stray non-ASCII byte is then merely
// a mislabeled Latin-1 byte, with no ill-defined behavior; the constructor asserts against it in debug builds.
template<typename CharacterType> class CStringWithEncoding final : public CString {
    // Heap allocation policy is inherited from CString.
    static_assert(std::same_as<CharacterType, char8_t> || std::same_as<CharacterType, Latin1Character> || std::same_as<CharacterType, char>);
public:
    CStringWithEncoding() = default;

    CStringWithEncoding(HashTableDeletedValueType)
        : CString(HashTableDeletedValue)
    {
    }

    // An ASCII literal is valid in every supported encoding.
    CStringWithEncoding(ASCIILiteral literal)
        : CString(literal)
    {
    }

    explicit CStringWithEncoding(std::span<const CharacterType> characters)
        : CString(byteCast<char>(characters))
    {
        if constexpr (std::same_as<CharacterType, char>)
            ASSERT(charactersAreAllASCII(byteCast<Latin1Character>(characters)));
    }

    // std::string does not know its encoding, so this asserts that its bytes are in CharacterType's.
    explicit CStringWithEncoding(const std::string& string)
        : CStringWithEncoding(byteCast<CharacterType>(std::span { string }))
    {
    }

    // Null-terminated, like CString(const char*), and likewise asserts the encoding of its bytes.
    explicit CStringWithEncoding(const CharacterType* string)
        : CString(byteCast<char>(string))
    {
        if constexpr (std::same_as<CharacterType, char>)
            ASSERT(charactersAreAllASCII(byteCast<Latin1Character>(CString::span())));
    }

    static CStringWithEncoding newUninitialized(size_t length, std::span<CharacterType>& characterBuffer)
    {
        std::span<char> bytes;
        auto result = CString::newUninitialized(length, bytes);
        characterBuffer = byteCast<CharacterType>(bytes);
        return CStringWithEncoding { result.buffer() };
    }

    // These hide the CString versions so that the encoding survives into the pointer and span types.
    const CharacterType* data() const LIFETIME_BOUND { return byteCast<CharacterType>(CString::data()); }
    std::span<const CharacterType> span() const LIFETIME_BOUND { return byteCast<CharacterType>(CString::span()); }
    std::span<const CharacterType> spanIncludingNullTerminator() const LIFETIME_BOUND { return byteCast<CharacterType>(CString::spanIncludingNullTerminator()); }
    std::span<CharacterType> mutableSpan() LIFETIME_BOUND { return byteCast<CharacterType>(CString::mutableSpan()); }
    std::span<CharacterType> mutableSpanIncludingNullTerminator() LIFETIME_BOUND { return byteCast<CharacterType>(CString::mutableSpanIncludingNullTerminator()); }

    // This is the escape hatch for external C functions and printf-style formatting. It is named for the
    // destination rather than the contents: const char* is what C string interfaces take, which is why this
    // is char and not the char8_t that would otherwise be correct for UTF-8.
    // Interactions with other strings should go through the span. Only offered for UTF-8: Latin-1 bytes are
    // meaningful to a const char* API only when they happen to be ASCII, and such a string should have come from
    // String::ascii(), whose data() is already a const char*. Callers that really want the raw bytes can use
    // byteCast<char>(span()) or slice to CString.
    // FIXME: Should go away once callers that only need bytes have moved to span().
    const char* legacyCStringPointer() const LIFETIME_BOUND requires std::same_as<CharacterType, char8_t> { return CString::data(); }

#if USE(FOUNDATION) && defined(__OBJC__)
    // Converts a null string to an empty string, like String::createNSString(). ASCII decodes as
    // Latin-1, matching how the comparison operators below treat it, so that an ASCIICString holding
    // a mislabeled non-ASCII byte round-trips instead of failing.
    RetainPtr<NSString> createNSString() const
    {
        auto characters = span();
        if (characters.empty())
            return @"";
        constexpr auto encoding = std::same_as<CharacterType, char8_t> ? NSUTF8StringEncoding : NSISOLatin1StringEncoding;
        return adoptNS([[NSString alloc] initWithBytes:characters.data() length:characters.size() encoding:encoding]);
    }
#endif

private:
    explicit CStringWithEncoding(CStringBuffer* buffer)
        : CString(buffer)
    {
    }
};

// These are exact matches, so they win over the CString overloads above, which would require a derived-to-base conversion.
template<typename CharacterType>
bool NODELETE operator==(const CStringWithEncoding<CharacterType>& a, const CStringWithEncoding<CharacterType>& b)
{
    if (a.isNull() != b.isNull())
        return false;
    return equalSpans(a.span(), b.span());
}

template<typename CharacterType>
bool operator<(const CStringWithEncoding<CharacterType>& a, const CStringWithEncoding<CharacterType>& b)
{
    if (a.isNull())
        return !b.isNull();
    if (b.isNull())
        return false;
    return is_lt(compareSpans(a.span(), b.span()));
}

// ASCII is Latin-1 restricted to 0..127, so ASCII against Latin-1 is left to the CString comparison above,
// which compares bytes; decoding both sides would give the same answer. UTF-8 bytes cannot be compared
// against either of the single-byte encodings, so those combinations decode to code points instead.
// Treating ASCII as Latin-1 rather than as raw bytes is what keeps this consistent for an ASCIICString that
// holds a non-ASCII byte: it then compares exactly like the equivalent Latin1CString, instead of matching
// both a Latin1CString and a UTF8CString that do not match each other.
template<typename A, typename B> concept NeedsCodePointComparison = std::same_as<A, char8_t> != std::same_as<B, char8_t>;

template<typename A, typename B> requires NeedsCodePointComparison<A, B>
bool operator==(const CStringWithEncoding<A>& a, const CStringWithEncoding<B>& b)
{
    if (a.isNull() != b.isNull())
        return false;
    if constexpr (std::same_as<A, char8_t>)
        return Unicode::equal(byteCast<Latin1Character>(b.span()), a.span());
    else
        return Unicode::equal(byteCast<Latin1Character>(a.span()), b.span());
}

// Ordering has no equivalent helper, and nothing needs to order strings of different encodings. Convert explicitly.
template<typename A, typename B> requires NeedsCodePointComparison<A, B>
bool operator<(const CStringWithEncoding<A>&, const CStringWithEncoding<B>&) = delete;

template<typename CharacterType> struct CStringWithEncodingHash {
    static unsigned hash(const CStringWithEncoding<CharacterType>& string) { return string.hash(); }
    static bool NODELETE equal(const CStringWithEncoding<CharacterType>& a, const CStringWithEncoding<CharacterType>& b)
    {
        if (a.isHashTableDeletedValue())
            return b.isHashTableDeletedValue();
        if (b.isHashTableDeletedValue())
            return false;
        return a == b;
    }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<typename CharacterType> struct DefaultHash<CStringWithEncoding<CharacterType>> : CStringWithEncodingHash<CharacterType> { };
template<typename CharacterType> struct HashTraits<CStringWithEncoding<CharacterType>> : SimpleClassHashTraits<CStringWithEncoding<CharacterType>> { };

} // namespace WTF

using WTF::ASCIICString;
using WTF::CString;
using WTF::CStringWithEncoding;
using WTF::Latin1CString;
using WTF::UTF8CString;
using WTF::convertToASCIILowercase;
using WTF::convertToASCIIUppercase;
