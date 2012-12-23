/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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
#include "NetworkingContext.h"
#include <public/Platform.h>
#include <public/WebCookie.h>
#include <public/WebCookieJar.h>
#include <public/WebURL.h>
#include <public/WebVector.h>

namespace WebCore {

void setCookiesFromDOM(const NetworkStorageSession& session, const KURL& firstPartyForCookies, const KURL& url, const String& cookieStr)
{
    if (!session.context())
        return;
    WebKit::WebCookieJar* cookieJar = session.context()->cookieJar();
    if (cookieJar)
        cookieJar->setCookie(url, firstPartyForCookies, cookieStr);
}

String cookiesForDOM(const NetworkStorageSession& session, const KURL& firstPartyForCookies, const KURL& url)
{
    if (!session.context())
        return String();
    String result;
    WebKit::WebCookieJar* cookieJar = session.context()->cookieJar();
    if (cookieJar)
        result = cookieJar->cookies(url, firstPartyForCookies);
    return result;
}

String cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const KURL& firstPartyForCookies, const KURL& url)
{
    if (!session.context())
        return String();
    String result;
    WebKit::WebCookieJar* cookieJar = session.context()->cookieJar();
    if (cookieJar)
        result = cookieJar->cookieRequestHeaderFieldValue(url, firstPartyForCookies);
    return result;
}

bool cookiesEnabled(const NetworkStorageSession& session, const KURL& cookieURL, const KURL& firstPartyForCookies)
{
    bool result = false;
    if (!session.context())
        return result;
    WebKit::WebCookieJar* cookieJar = session.context()->cookieJar();
    if (cookieJar)
        result = cookieJar->cookiesEnabled(cookieURL, firstPartyForCookies);
    return result;
}

bool getRawCookies(const NetworkStorageSession& session, const KURL& firstPartyForCookies, const KURL& url, Vector<Cookie>& rawCookies)
{
    rawCookies.clear();
    if (!session.context())
        return false;
    WebKit::WebVector<WebKit::WebCookie> webCookies;
    WebKit::WebCookieJar* cookieJar = session.context()->cookieJar();
    if (cookieJar)
        cookieJar->rawCookies(url, firstPartyForCookies, webCookies);
    for (unsigned i = 0; i < webCookies.size(); ++i) {
        const WebKit::WebCookie& webCookie = webCookies[i];
        Cookie cookie(webCookie.name, webCookie.value, webCookie.domain, webCookie.path, webCookie.expires, webCookie.httpOnly, webCookie.secure, webCookie.session);
        rawCookies.append(cookie);
    }
    return true;
}

void deleteCookie(const NetworkStorageSession& session, const KURL& url, const String& cookieName)
{
    if (!session.context())
        return;
    WebKit::WebCookieJar* cookieJar = session.context()->cookieJar();
    if (cookieJar)
        cookieJar->deleteCookie(url, cookieName);
}

void getHostnamesWithCookies(const NetworkStorageSession&, HashSet<String>& /*hostnames*/)
{
    // FIXME: Not yet implemented
}

void deleteCookiesForHostname(const NetworkStorageSession&, const String& /*hostname*/)
{
    // FIXME: Not yet implemented
}

void deleteAllCookies(const NetworkStorageSession&)
{
    // FIXME: Not yet implemented
}

} // namespace WebCore
