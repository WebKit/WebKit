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
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SwiftBridging.h>
#include <wtf/text/Latin1Character.h>

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
// Latin-1, and char is ASCII, as in ASCIILiteral.
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

    // This is the escape hatch for external C functions and printf-style formatting, matching ASCIILiteral::characters().
    // Interactions with other strings should go through the span.
    // FIXME: Should go away once callers that only need bytes have moved to span().
    const char* characters() const LIFETIME_BOUND { return CString::data(); }

private:
    explicit CStringWithEncoding(CStringBuffer* buffer)
        : CString(buffer)
    {
    }
};

using UTF8CString = CStringWithEncoding<char8_t>;
using Latin1CString = CStringWithEncoding<Latin1Character>;
using ASCIICString = CStringWithEncoding<char>;

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

// Comparing Latin-1 bytes against UTF-8 bytes is meaningless. Convert explicitly instead. ASCII is a subset of
// both, so those combinations are left to the CString comparison above, which compares bytes.
template<typename A, typename B> concept AreIncompatibleCStringEncodings =
    (std::same_as<A, char8_t> && std::same_as<B, Latin1Character>) || (std::same_as<A, Latin1Character> && std::same_as<B, char8_t>);

template<typename A, typename B> requires AreIncompatibleCStringEncodings<A, B>
bool operator==(const CStringWithEncoding<A>&, const CStringWithEncoding<B>&) = delete;

template<typename A, typename B> requires AreIncompatibleCStringEncodings<A, B>
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
