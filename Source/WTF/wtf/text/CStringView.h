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
#include <wtf/StdLibExtras.h>
#include <wtf/text/ASCIILiteral.h>

namespace WTF {

class PrintStream;

// This is a class designed to contain a null terminated UTF8 string, untouched. It contains char8_t to avoid mixing
// incompatible encodings at compile time.
class CStringView final {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    static CStringView unsafeFromUTF8(const char* string)
    {
        if (!string)
            return CStringView();
        return CStringView(unsafeMakeSpan(byteCast<char8_t>(string), std::char_traits<char>::length(string) + 1));
    }

    static CStringView fromUTF8(std::span<const char8_t> spanWithNullTerminator LIFETIME_BOUND)
    {
        if (spanWithNullTerminator.size() < 1)
            return CStringView();
        RELEASE_ASSERT(spanWithNullTerminator[spanWithNullTerminator.size() - 1] == '\0');
        return CStringView(spanWithNullTerminator);
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

    bool isNull() const { return m_spanWithNullTerminator.empty(); }

    // This member function is designed to interface with external C functions handling UTF8 strings. Interactions with
    // other strings should be handled by using the span.
    const char* utf8() const LIFETIME_BOUND { return reinterpret_cast<const char*>(m_spanWithNullTerminator.data()); }
    size_t lengthInBytes() const { return m_spanWithNullTerminator.size() > 0 ? m_spanWithNullTerminator.size() - 1 : 0; }
    std::span<const char8_t> span() const LIFETIME_BOUND { return m_spanWithNullTerminator.first(lengthInBytes()); }
    std::span<const char8_t> spanIncludingNullTerminator() const LIFETIME_BOUND { return m_spanWithNullTerminator; }
    bool isEmpty() const { return m_spanWithNullTerminator.size() <= 1; }

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
    return equalSpans(a.span(), b.span());
}

inline bool operator==(CStringView a, ASCIILiteral b)
{
    return equalSpans(a.span(), byteCast<char8_t>(b.span()));
}

inline bool operator==(ASCIILiteral a, CStringView b)
{
    return b == a;
}

// CStringView is null terminated
inline const char* safePrintfType(const CStringView& string) { return string.utf8(); }

} // namespace WTF

using WTF::CStringView;
