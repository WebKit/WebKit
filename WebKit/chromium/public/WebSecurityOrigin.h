/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebSecurityOrigin_h
#define WebSecurityOrigin_h

#include "WebCommon.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class SecurityOrigin; }
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {

class WebSecurityOriginPrivate;
class WebString;
class WebURL;

class WebSecurityOrigin {
public:
    ~WebSecurityOrigin() { reset(); }

    WebSecurityOrigin() : m_private(0) { }
    WebSecurityOrigin(const WebSecurityOrigin& s) : m_private(0) { assign(s); }
    WebSecurityOrigin& operator=(const WebSecurityOrigin& s)
    {
        assign(s);
        return *this;
    }

    WEBKIT_API static WebSecurityOrigin createFromDatabaseIdentifier(const WebString& databaseIdentifier);
    WEBKIT_API static WebSecurityOrigin createFromString(const WebString&);
    WEBKIT_API static WebSecurityOrigin create(const WebURL&);

    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebSecurityOrigin&);

    bool isNull() const { return !m_private; }

    WEBKIT_API WebString protocol() const;
    WEBKIT_API WebString host() const;
    WEBKIT_API unsigned short port() const;

    // The empty WebSecurityOrigin is the least privileged WebSecurityOrigin.
    WEBKIT_API bool isEmpty() const;

    // Returns true if this WebSecurityOrigin can script objects in the given
    // SecurityOrigin. For example, call this function before allowing
    // script from one security origin to read or write objects from
    // another SecurityOrigin.
    WEBKIT_API bool canAccess(const WebSecurityOrigin&) const;

    // Returns true if this WebSecurityOrigin can read content retrieved from
    // the given URL. For example, call this function before allowing script
    // from a given security origin to receive contents from a given URL.
    WEBKIT_API bool canRequest(const WebURL&) const;

    // Returns a string representation of the WebSecurityOrigin.  The empty
    // WebSecurityOrigin is represented by "null".  The representation of a
    // non-empty WebSecurityOrigin resembles a standard URL.
    WEBKIT_API WebString toString() const;

    // Returns a string representation of this WebSecurityOrigin that can
    // be used as a file.  Should be used in storage APIs only.
    WEBKIT_API WebString databaseIdentifier();

#if WEBKIT_IMPLEMENTATION
    WebSecurityOrigin(const WTF::PassRefPtr<WebCore::SecurityOrigin>&);
    WebSecurityOrigin& operator=(const WTF::PassRefPtr<WebCore::SecurityOrigin>&);
    operator WTF::PassRefPtr<WebCore::SecurityOrigin>() const;
#endif

private:
    void assign(WebSecurityOriginPrivate*);
    WebSecurityOriginPrivate* m_private;
};

} // namespace WebKit

#endif
