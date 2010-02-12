/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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

#include "config.h"
#include "CookieJar.h"

#include "Cookie.h"
#include "ChromiumBridge.h"
#include "Document.h"

namespace WebCore {

void setCookies(Document* document, const KURL& url, const String& value)
{
    ChromiumBridge::setCookies(url, document->firstPartyForCookies(), value);
}

String cookies(const Document* document, const KURL& url)
{
    return ChromiumBridge::cookies(url, document->firstPartyForCookies());
}

String cookieRequestHeaderFieldValue(const Document* document, const KURL& url)
{
    // FIXME: move in ChromiumBridge?
    Vector<Cookie> cookies;
    getRawCookies(document, url, cookies);
    String cookieLine;
    // FIXME: Set $Version=v;
    for (size_t i = 0; i < cookies.size(); i++) {
        Cookie cookie = cookies[i];
        if (i > 0)
            cookieLine += "; ";
        if (!cookie.name.isEmpty())
            cookieLine += cookie.name + "=";
        cookieLine += cookie.value;
        // FIXME: set $Path, $Domain, ...
    }
    return cookieLine;
}

bool cookiesEnabled(const Document* document)
{
    return ChromiumBridge::cookiesEnabled(document->cookieURL(), document->firstPartyForCookies());
}

bool getRawCookies(const Document* document, const KURL& url, Vector<Cookie>& rawCookies)
{
    return ChromiumBridge::rawCookies(url, document->firstPartyForCookies(), &rawCookies);
}

void deleteCookie(const Document*, const KURL& url, const String& cookieName)
{
    return ChromiumBridge::deleteCookie(url, cookieName);
}

} // namespace WebCore
