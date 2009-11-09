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

#ifndef WebData_h
#define WebData_h

#include "WebCommon.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class SharedBuffer; }
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {

class WebDataPrivate;

// A container for raw bytes.  It is inexpensive to copy a WebData object.
//
// WARNING: It is not safe to pass a WebData across threads!!!
//
class WebData {
public:
    ~WebData() { reset(); }

    WebData() : m_private(0) { }

    WebData(const char* data, size_t size) : m_private(0)
    {
        assign(data, size);
    }

    template <int N>
    WebData(const char (&data)[N]) : m_private(0)
    {
        assign(data, N - 1);
    }

    WebData(const WebData& d) : m_private(0) { assign(d); }

    WebData& operator=(const WebData& d)
    {
        assign(d);
        return *this;
    }

    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebData&);
    WEBKIT_API void assign(const char* data, size_t size);

    WEBKIT_API size_t size() const;
    WEBKIT_API const char* data() const;

    bool isEmpty() const { return !size(); }
    bool isNull() const { return !m_private; }

#if WEBKIT_IMPLEMENTATION
    WebData(const WTF::PassRefPtr<WebCore::SharedBuffer>&);
    WebData& operator=(const WTF::PassRefPtr<WebCore::SharedBuffer>&);
    operator WTF::PassRefPtr<WebCore::SharedBuffer>() const;
#else
    template <class C>
    WebData(const C& c) : m_private(0)
    {
        assign(c.data(), c.size());
    }

    template <class C>
    WebData& operator=(const C& c)
    {
        assign(c.data(), c.size());
        return *this;
    }
#endif

private:
    void assign(WebDataPrivate*);
    WebDataPrivate* m_private;
};

} // namespace WebKit

#endif
