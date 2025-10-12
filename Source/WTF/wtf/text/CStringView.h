/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2024 Apple Inc. All Rights Reserved.
 * Copyright (C) 2025 Comcast Inc.
 * Copyright (C) 2025 Igalia S.L.
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
#include <wtf/Compiler.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Forward.h>
#include <wtf/HashFunctions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/SuperFastHash.h>

namespace WTF {

class PrintStream;
class String;

// This is a class designed to contain a UTF8 string, untouched. Interactions with other string classes in WebKit should
// be handled with care or perform a string conversion through the String class, with the exception of ASCIILiteral
// because ASCII characters are also UTF8.
class CStringView final {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    static CStringView unsafeFromUTF8(const char* string)
    {
        if (!string)
            return CStringView();
        return CStringView(unsafeMakeSpan(byteCast<char8_t>(string), std::char_traits<char>::length(string) + 1));
    }

    WTF_EXPORT_PRIVATE void dump(PrintStream& out) const;

    CStringView() = default;
    constexpr CStringView(std::nullptr_t)
        : CStringView()
    { }
    CStringView(ASCIILiteral literal LIFETIME_BOUND)
    {
        if (!literal.length())
            return;
        m_spanWithNullTerminator = byteCast<char8_t>(literal.spanIncludingNullTerminator());
    }

    unsigned hash() const;
    bool isNull() const { return m_spanWithNullTerminator.empty(); }

    // This method is designed to interface with external C functions handling UTF8 strings. Interactions with other
    // strings should be done through String with the exception of ASCIILiteral because ASCII is also UTF8.
    const char* utf8() const LIFETIME_BOUND { return reinterpret_cast<const char*>(m_spanWithNullTerminator.data()); }
    size_t length() const { return m_spanWithNullTerminator.size() > 0 ? m_spanWithNullTerminator.size() - 1 : 0; }
    std::span<const char8_t> span8() const LIFETIME_BOUND { return m_spanWithNullTerminator.first(length()); }
    std::span<const char8_t> spanIncludingNullTerminator() const LIFETIME_BOUND { return m_spanWithNullTerminator; }
    size_t isEmpty() const { return m_spanWithNullTerminator.size() <= 1; }
    WTF_EXPORT_PRIVATE String toString() const;

    explicit operator bool() const { return !isEmpty(); }
    bool operator!() const { return isEmpty(); }

private:
    explicit CStringView(std::span<const char8_t> spanWithNullTerminator LIFETIME_BOUND)
        : m_spanWithNullTerminator(spanWithNullTerminator)
    {
    }

    std::span<const char8_t> m_spanWithNullTerminator;
};

inline bool operator==(CStringView a, CStringView b)
{
    if (!a || !b)
        return a.utf8() == b.utf8();
    return equalSpans(a.span8(), b.span8());
}

inline bool operator==(CStringView a, ASCIILiteral b)
{
    if (a.isEmpty() || b.isEmpty())
        return a.utf8() == b.characters();
    return equalSpans(a.span8(), byteCast<char8_t>(b.span()));
}

inline bool operator==(ASCIILiteral a, CStringView b)
{
    return b == a;
}

// CStringView is null terminated
inline const char* safePrintfType(const CStringView& string) { return string.utf8(); }

} // namespace WTF

using WTF::CStringView;
