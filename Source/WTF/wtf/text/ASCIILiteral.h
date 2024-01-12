/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>
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
    constexpr operator const char*() const { return m_characters; }

    static constexpr ASCIILiteral fromLiteralUnsafe(const char* string)
    {
        return ASCIILiteral { string };
    }

    WTF_EXPORT_PRIVATE void dump(PrintStream& out) const;

    ASCIILiteral() = default;

    unsigned hash() const;
    constexpr bool isNull() const { return !m_characters; }

    constexpr const char* characters() const { return m_characters; }
    const LChar* characters8() const { return bitwise_cast<const LChar*>(m_characters); }
    constexpr size_t length() const;
    std::span<const LChar> span8() const { return { characters8(), length() }; }
    size_t isEmpty() const { return !m_characters || !*m_characters; }

    constexpr char characterAt(unsigned index) const { return m_characters[index]; }

#ifdef __OBJC__
    // This function convert null strings to empty strings.
    WTF_EXPORT_PRIVATE RetainPtr<NSString> createNSString() const;
#endif

private:
    constexpr explicit ASCIILiteral(const char* characters) : m_characters(characters) { }

    const char* m_characters { nullptr };
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

inline constexpr size_t ASCIILiteral::length() const
{
    if (std::is_constant_evaluated()) {
        if (!m_characters)
            return 0;

        size_t length = 0;
        while (true) {
            if (!m_characters[length])
                return length;
            ++length;
        }
        return length;
    }
    return strlen(m_characters);
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

struct ASCIILiteralPtrHash {
    static unsigned hash(const ASCIILiteral& key) { return IntHash<uintptr_t>::hash(reinterpret_cast<uintptr_t>(key.characters())); }
    static bool equal(const ASCIILiteral& a, const ASCIILiteral& b) { return a.characters() == b.characters(); }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

inline namespace StringLiterals {

constexpr ASCIILiteral operator"" _s(const char* characters, size_t n)
{
#if ASSERT_ENABLED
    for (size_t i = 0; i < n; ++i)
        ASSERT_UNDER_CONSTEXPR_CONTEXT(isASCII(characters[i]));
#else
    UNUSED_PARAM(n);
#endif
    return ASCIILiteral::fromLiteralUnsafe(characters);
}

} // inline StringLiterals

} // namespace WTF

using WTF::ASCIILiteralPtrHash;
using namespace WTF::StringLiterals;
