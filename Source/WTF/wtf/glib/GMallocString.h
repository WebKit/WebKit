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

#include <wtf/Compiler.h>
#include <wtf/HashFunctions.h>
#include <wtf/glib/GSpanExtras.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/CStringView.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/SuperFastHash.h>

namespace WTF {

class PrintStream;

class GMallocString final {
    WTF_FORBID_HEAP_ALLOCATION;
    WTF_MAKE_NONCOPYABLE(GMallocString);
public:

    static GMallocString unsafeAdoptFromUTF8(char* string)
    {
        if (!string)
            return GMallocString();
        return GMallocString { adoptGMallocSpan<char8_t>(unsafeMakeSpan(byteCast<char8_t>(string), string ? std::char_traits<char>::length(string) + 1 : 0)) };
    }

    static GMallocString unsafeAdoptFromUTF8(GUniquePtr<char>&& pointer)
    {
        return unsafeAdoptFromUTF8(pointer.release());
    }

    static GMallocString unsafeAdoptFromUTF8(GUniqueOutPtr<char>&& pointer)
    {
        return unsafeAdoptFromUTF8(pointer.release());
    }

    static GMallocString adoptFromUTF8(std::span<char> string)
    {
        if (string.size() < 1)
            return GMallocString();
        RELEASE_ASSERT(string[string.size() - 1] == '\0');
        return GMallocString { adoptGMallocSpan<char8_t>(byteCast<char8_t>(string)) };
    }

    explicit GMallocString(const CStringView& view)
    {
        m_spanWithNullTerminator = dupGMallocSpan(view.spanIncludingNullTerminator());
    }

    WTF_EXPORT_PRIVATE void dump(PrintStream& out) const;

    GMallocString() = default;
    constexpr GMallocString(std::nullptr_t)
        : GMallocString()
    { }

    GMallocString(GMallocString&& other)
        : m_spanWithNullTerminator(WTF::move(other.m_spanWithNullTerminator))
    {
    }
    GMallocString& operator=(GMallocString&& other)
    {
        GMallocSpan<char8_t> otherSpan = WTF::move(other.m_spanWithNullTerminator);
        std::swap(m_spanWithNullTerminator, otherSpan);
        return *this;
    }

    bool isNull() const { return m_spanWithNullTerminator.span().empty(); }
    const char* utf8() const LIFETIME_BOUND { return byteCast<char>(m_spanWithNullTerminator.span().data()); }
    [[nodiscard]] char* leakUTF8() { return byteCast<char>(m_spanWithNullTerminator.leakSpan().data()); }
    size_t lengthInBytes() const { return !m_spanWithNullTerminator.span().empty() ? m_spanWithNullTerminator.span().size() - 1 : 0; }
    std::span<const char8_t> span() const LIFETIME_BOUND { return m_spanWithNullTerminator.span().first(lengthInBytes()); }
    std::span<const char8_t> spanIncludingNullTerminator() const LIFETIME_BOUND { return m_spanWithNullTerminator.span(); }
    size_t isEmpty() const { return m_spanWithNullTerminator.span().size() <= 1; }

    explicit operator bool() const { return !isEmpty(); }
    bool operator!() const { return isEmpty(); }

private:
    explicit GMallocString(GMallocSpan<char8_t>&& string)
        : m_spanWithNullTerminator(WTF::move(string))
    {
    }

    GMallocSpan<char8_t> m_spanWithNullTerminator;
};

inline bool operator==(const GMallocString& a, const GMallocString& b)
{
    return equal(a.span(), b.span());
}

inline bool operator==(const GMallocString& a, ASCIILiteral b)
{
    return equal(a.span(), byteCast<char8_t>(b.span()));
}

inline bool operator==(const GMallocString& a, CStringView b)
{
    return equal(a.span(), b.span());
}

// GMallocString is null terminated
inline const char* safePrintfType(const GMallocString& string) { return string.utf8(); }

inline CStringView toCStringView(const GMallocString& string LIFETIME_BOUND) { return CStringView::fromUTF8(string.spanIncludingNullTerminator()); }

} // namespace WTF

using WTF::GMallocString;
using WTF::toCStringView;
