/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebCString_h
#define WebCString_h

#include "WebCommon.h"

#if WEBKIT_IMPLEMENTATION
namespace WTF { class CString; }
#else
#include <string>
#endif

namespace WebKit {

class WebCStringPrivate;
class WebString;

// A single-byte string container with unspecified encoding.  It is
// inexpensive to copy a WebCString object.
//
// WARNING: It is not safe to pass a WebCString across threads!!!
//
class WebCString {
public:
    ~WebCString() { reset(); }

    WebCString() : m_private(0) { }

    WebCString(const char* data, size_t len) : m_private(0)
    {
        assign(data, len);
    }

    WebCString(const WebCString& s) : m_private(0) { assign(s); }

    WebCString& operator=(const WebCString& s)
    {
        assign(s);
        return *this;
    }

    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebCString&);
    WEBKIT_API void assign(const char* data, size_t len);

    WEBKIT_API size_t length() const;
    WEBKIT_API const char* data() const;

    bool isEmpty() const { return !length(); }
    bool isNull() const { return !m_private; }

    WEBKIT_API WebString utf16() const;

    WEBKIT_API static WebCString fromUTF16(const WebUChar* data, size_t length);
    WEBKIT_API static WebCString fromUTF16(const WebUChar* data);

#if WEBKIT_IMPLEMENTATION
    WebCString(const WTF::CString&);
    WebCString& operator=(const WTF::CString&);
    operator WTF::CString() const;
#else
    WebCString(const std::string& s) : m_private(0)
    {
        assign(s.data(), s.length());
    }

    WebCString& operator=(const std::string& s)
    {
        assign(s.data(), s.length());
        return *this;
    }

    operator std::string() const
    {
        size_t len = length();
        return len ? std::string(data(), len) : std::string();
    }

    template <class UTF16String>
    static WebCString fromUTF16(const UTF16String& s)
    {
        return fromUTF16(s.data(), s.length());
    }
#endif

private:
    void assign(WebCStringPrivate*);
    WebCStringPrivate* m_private;
};

} // namespace WebKit

#endif
