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

#ifndef WebHTTPBody_h
#define WebHTTPBody_h

#include "WebData.h"
#include "WebFileInfo.h"
#include "WebNonCopyable.h"
#include "WebString.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class FormData; }
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {

class WebHTTPBodyPrivate;

class WebHTTPBody {
public:
    struct Element {
        enum { TypeData, TypeFile } type;
        WebData data;
        WebString filePath;
        long long fileStart;
        long long fileLength; // -1 means to the end of the file.
        WebFileInfo fileInfo;
    };

    ~WebHTTPBody() { reset(); }

    WebHTTPBody() : m_private(0) { }
    WebHTTPBody(const WebHTTPBody& b) : m_private(0) { assign(b); }
    WebHTTPBody& operator=(const WebHTTPBody& b)
    {
        assign(b);
        return *this;
    }

    WEBKIT_API void initialize();
    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebHTTPBody&);

    bool isNull() const { return !m_private; }

    // Returns the number of elements comprising the http body.
    WEBKIT_API size_t elementCount() const;

    // Sets the values of the element at the given index.  Returns false if
    // index is out of bounds.
    WEBKIT_API bool elementAt(size_t index, Element&) const;

    // Append to the list of elements.
    WEBKIT_API void appendData(const WebData&);
    WEBKIT_API void appendFile(const WebString&);
    // Passing -1 to fileLength means to the end of the file.
    WEBKIT_API void appendFileRange(const WebString&, long long fileStart, long long fileLength, const WebFileInfo&);

    // Identifies a particular form submission instance.  A value of 0 is
    // used to indicate an unspecified identifier.
    WEBKIT_API long long identifier() const;
    WEBKIT_API void setIdentifier(long long);

#if WEBKIT_IMPLEMENTATION
    WebHTTPBody(const WTF::PassRefPtr<WebCore::FormData>&);
    WebHTTPBody& operator=(const WTF::PassRefPtr<WebCore::FormData>&);
    operator WTF::PassRefPtr<WebCore::FormData>() const;
#endif

private:
    void assign(WebHTTPBodyPrivate*);
    void ensureMutable();
    WebHTTPBodyPrivate* m_private;
};

} // namespace WebKit

#endif
