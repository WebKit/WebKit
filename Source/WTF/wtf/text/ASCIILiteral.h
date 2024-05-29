/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2024 Apple Inc. All Rights Reserved.
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

#include <span>
#include <string>
#include <type_traits>
#include <wtf/ASCIICType.h>
#include <wtf/Forward.h>
#include <wtf/HashFunctions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/SuperFastHash.h>

OBJC_CLASS NSString;

namespace WTF {

class PrintStream;

class ASCIILiteral final {
public:
    constexpr operator const char*() const { return m_charactersWithNullTerminator.data(); }

    static constexpr ASCIILiteral fromLiteralUnsafe(const char* string)
    {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(string);
        return ASCIILiteral { string };
    }

    WTF_EXPORT_PRIVATE void dump(PrintStream& out) const;

    ASCIILiteral() = default;
    constexpr ASCIILiteral(std::nullptr_t)
        : ASCIILiteral()
    { }

    unsigned hash() const;
    constexpr bool isNull() const { return m_charactersWithNullTerminator.empty(); }

    constexpr const char* characters() const { return m_charactersWithNullTerminator.data(); }
    constexpr size_t length() const { return !m_charactersWithNullTerminator.empty() ? m_charactersWithNullTerminator.size() - 1 : 0; }
    std::span<const LChar> span8() const { return { bitwise_cast<const LChar*>(characters()), length() }; }
    std::span<const char> spanIncludingNullTerminator() const { return m_charactersWithNullTerminator; }
    size_t isEmpty() const { return m_charactersWithNullTerminator.size() <= 1; }

    constexpr char operator[](size_t index) const { return m_charactersWithNullTerminator[index]; }
    constexpr char characterAt(size_t index) const { return m_charactersWithNullTerminator[index]; }

#ifdef __OBJC__
    // This function convert null strings to empty strings.
    WTF_EXPORT_PRIVATE RetainPtr<NSString> createNSString() const;
#endif

    static ASCIILiteral deletedValue();
    bool isDeletedValue() const { return characters() == reinterpret_cast<char*>(-1); }

private:
    constexpr explicit ASCIILiteral(const char* characters)
        : m_charactersWithNullTerminator(characters, std::char_traits<char>::length(characters) + 1)
    {
#if ASSERT_ENABLED
    for (size_t i = 0; i < length(); ++i)
        ASSERT_UNDER_CONSTEXPR_CONTEXT(isASCII(m_charactersWithNullTerminator[i]));
#endif
    }

    std::span<const char> m_charactersWithNullTerminator;
};

inline bool operator==(ASCIILiteral a, const char* b)
{
    if (!a || !b)
        return a.characters() == b;
    return !strcmp(a.characters(), b);
}

inline bool operator==(ASCIILiteral a, ASCIILiteral b)
{
    if (!a || !b)
        return a.characters() == b.characters();
    return !strcmp(a.characters(), b.characters());
}

inline unsigned ASCIILiteral::hash() const
{
    if (isNull())
        return 0;
    SuperFastHash hasher;
    hasher.addCharacters(characters(), length());
    return hasher.hash();
}

struct ASCIILiteralHash {
    static unsigned hash(const ASCIILiteral& literal) { return literal.hash(); }
    static bool equal(const ASCIILiteral& a, const ASCIILiteral& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<ASCIILiteral> : ASCIILiteralHash { };

inline ASCIILiteral ASCIILiteral::deletedValue()
{
    ASCIILiteral result;
    result.m_charactersWithNullTerminator = { reinterpret_cast<char*>(-1), static_cast<size_t>(0) };
    return result;
}

inline namespace StringLiterals {

constexpr ASCIILiteral operator"" _s(const char* characters, size_t)
{
    auto result = ASCIILiteral::fromLiteralUnsafe(characters);
    ASSERT_UNDER_CONSTEXPR_CONTEXT(result.characters() == characters);
    return result;
}

constexpr std::span<const LChar> operator"" _span(const char* characters, size_t n)
{
#if ASSERT_ENABLED
    for (size_t i = 0; i < n; ++i)
        ASSERT_UNDER_CONSTEXPR_CONTEXT(isASCII(characters[i]));
#endif
    return std::span { bitwise_cast<const LChar*>(characters), n };
}

} // inline StringLiterals

} // namespace WTF

using namespace WTF::StringLiterals;
