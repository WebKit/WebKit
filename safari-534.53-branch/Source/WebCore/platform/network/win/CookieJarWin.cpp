/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CookieJar.h"

#include "Cookie.h"
#include "KURL.h"
#include "PlatformString.h"
#include "Document.h"
#include "ResourceHandle.h"
#include <windows.h>
#include <Wininet.h>

namespace WebCore {

void setCookies(Document* /*document*/, const KURL& url, const String& value)
{
    // FIXME: Deal with document->firstPartyForCookies().
    String str = url.string();
    String val = value;
    InternetSetCookie(str.charactersWithNullTermination(), 0, val.charactersWithNullTermination());
}

String cookies(const Document* /*document*/, const KURL& url)
{
    String str = url.string();

    DWORD count = 0;
    if (!InternetGetCookie(str.charactersWithNullTermination(), 0, 0, &count))
        return String();

    if (count <= 1) // Null terminator counts as 1.
        return String();

    Vector<UChar> buffer(count);
    if (!InternetGetCookie(str.charactersWithNullTermination(), 0, buffer.data(), &count))
        return String();

    buffer.shrink(count - 1); // Ignore the null terminator.
    return String::adopt(buffer);
}

String cookieRequestHeaderFieldValue(const Document* document, const KURL& url)
{
    // FIXME: include HttpOnly cookie
    return cookies(document, url);
}

bool cookiesEnabled(const Document* /*document*/)
{
    return true;
}

bool getRawCookies(const Document*, const KURL&, Vector<Cookie>& rawCookies)
{
    // FIXME: Not yet implemented
    rawCookies.clear();
    return false; // return true when implemented
}

void deleteCookie(const Document*, const KURL&, const String&)
{
    // FIXME: Not yet implemented
}

void getHostnamesWithCookies(HashSet<String>& hostnames)
{
    // FIXME: Not yet implemented
}

void deleteCookiesForHostname(const String& hostname)
{
    // FIXME: Not yet implemented
}

void deleteAllCookies()
{
    // FIXME: Not yet implemented
}

}
