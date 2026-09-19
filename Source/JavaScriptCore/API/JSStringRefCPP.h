/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// The C++ counterpart to JSStringRef.h, bridging JSStringRef to the WTF string types the
// way JSStringRefCF.h bridges it to CFString. Unlike its siblings this header is C++ only,
// so it is not wrapped in extern "C".

#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

// The counterpart to JSStringGetUTF8CString(), for callers that want the converted string
// rather than a buffer to convert into. The result is sized to the string, not to the maximum
// size the conversion could have needed.
inline UTF8CString utf8CString(JSStringRef string)
{
    Vector<char> buffer(JSStringGetMaximumUTF8CStringSize(string));
    // The returned length counts the null terminator, which UTF8CString adds itself.
    size_t length = JSStringGetUTF8CString(string, buffer.mutableSpan().data(), buffer.size());
    return UTF8CString { byteCast<char8_t>(buffer.span().first(length ? length - 1 : 0)) };
}

// Counterparts to JSStringCreateWithUTF8CString(), taking the encoding in the type and
// returning an owning JSRetainPtr so that the reference cannot be leaked. Like that function
// these never return null: a string that cannot be represented becomes an empty JSString.
//
// A JSString stores a String, so the characters are handed straight over rather than transcoded,
// and an rvalue is moved into place. Reaching one from UTF-8 has to decode, which is the same
// cost JSStringCreateWithUTF8CString() pays.
inline JSRetainPtr<JSStringRef> createJSString(String&& string)
{
    // adopt() consumes the +1 that leakRef() hands over, but the checker does not know it the way
    // it knows adoptRef(), so JSRetainPtr.h is itself listed in UncountedCallArgsCheckerExpectations.
    if (RefPtr result = OpaqueJSString::tryCreate(WTF::move(string)))
        SUPPRESS_UNCOUNTED_ARG return adopt(result.leakRef());
    SUPPRESS_UNCOUNTED_ARG return adopt(&OpaqueJSString::create().leakRef());
}

inline JSRetainPtr<JSStringRef> createJSString(const String& string)
{
    return createJSString(String { string });
}

// Required rather than merely convenient: ASCIILiteral converts implicitly to both String and
// UTF8CString, so without this overload the call would be ambiguous.
inline JSRetainPtr<JSStringRef> createJSString(ASCIILiteral literal)
{
    return createJSString(String { literal });
}

inline JSRetainPtr<JSStringRef> createJSString(const UTF8CString& string)
{
    return createJSString(String { string });
}
