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

#include "CookieStorageWin.h"
#include "Document.h"
#include "KURL.h"
#include "PlatformString.h"
#include "ResourceHandle.h"
#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <CoreFoundation/CoreFoundation.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#include <windows.h>

namespace WebCore {

static const CFStringRef s_setCookieKeyCF = CFSTR("Set-Cookie");
static const CFStringRef s_cookieCF = CFSTR("Cookie");

static RetainPtr<CFArrayRef> filterCookies(CFArrayRef unfilteredCookies)
{
    CFIndex count = CFArrayGetCount(unfilteredCookies);
    RetainPtr<CFMutableArrayRef> filteredCookies(AdoptCF, CFArrayCreateMutable(0, count, &kCFTypeArrayCallBacks));
    for (CFIndex i = 0; i < count; ++i) {
        CFHTTPCookieRef cookie = (CFHTTPCookieRef)CFArrayGetValueAtIndex(unfilteredCookies, i);

        // <rdar://problem/5632883> CFHTTPCookieStorage would store an empty cookie,
        // which would be sent as "Cookie: =". We have a workaround in setCookies() to prevent
        // that, but we also need to avoid sending cookies that were previously stored, and
        // there's no harm to doing this check because such a cookie is never valid.
        if (!CFStringGetLength(CFHTTPCookieGetName(cookie)))
            continue;

        if (CFHTTPCookieIsHTTPOnly(cookie))
            continue;

        CFArrayAppendValue(filteredCookies.get(), cookie);
    }
    return filteredCookies;
}

void setCookies(Document* document, const KURL& url, const String& value)
{
    // <rdar://problem/5632883> CFHTTPCookieStorage stores an empty cookie, which would be sent as "Cookie: =".
    if (value.isEmpty())
        return;

    CFHTTPCookieStorageRef cookieStorage = currentCookieStorage();
    if (!cookieStorage)
        return;

    RetainPtr<CFURLRef> urlCF(AdoptCF, url.createCFURL());
    RetainPtr<CFURLRef> firstPartyForCookiesCF(AdoptCF, document->firstPartyForCookies().createCFURL());

    // <http://bugs.webkit.org/show_bug.cgi?id=6531>, <rdar://4409034>
    // cookiesWithResponseHeaderFields doesn't parse cookies without a value
    String cookieString = value.contains('=') ? value : value + "=";

    RetainPtr<CFStringRef> cookieStringCF(AdoptCF, cookieString.createCFString());
    RetainPtr<CFDictionaryRef> headerFieldsCF(AdoptCF, CFDictionaryCreate(kCFAllocatorDefault,
        (const void**)&s_setCookieKeyCF, (const void**)&cookieStringCF, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr<CFArrayRef> cookiesCF(AdoptCF, CFHTTPCookieCreateWithResponseHeaderFields(kCFAllocatorDefault,
        headerFieldsCF.get(), urlCF.get()));

    CFHTTPCookieStorageSetCookies(cookieStorage, filterCookies(cookiesCF.get()).get(), urlCF.get(), firstPartyForCookiesCF.get());
}

String cookies(const Document* /*document*/, const KURL& url)
{
    CFHTTPCookieStorageRef cookieStorage = currentCookieStorage();
    if (!cookieStorage)
        return String();

    RetainPtr<CFURLRef> urlCF(AdoptCF, url.createCFURL());

    bool secure = url.protocolIs("https");
    RetainPtr<CFArrayRef> cookiesCF(AdoptCF, CFHTTPCookieStorageCopyCookiesForURL(cookieStorage, urlCF.get(), secure));
    RetainPtr<CFDictionaryRef> headerCF(AdoptCF, CFHTTPCookieCopyRequestHeaderFields(kCFAllocatorDefault, filterCookies(cookiesCF.get()).get()));
    return (CFStringRef)CFDictionaryGetValue(headerCF.get(), s_cookieCF);
}

bool cookiesEnabled(const Document* /*document*/)
{
    CFHTTPCookieStorageAcceptPolicy policy = CFHTTPCookieStorageAcceptPolicyOnlyFromMainDocumentDomain;
    if (CFHTTPCookieStorageRef cookieStorage = currentCookieStorage())
        policy = CFHTTPCookieStorageGetCookieAcceptPolicy(cookieStorage);
    return policy == CFHTTPCookieStorageAcceptPolicyOnlyFromMainDocumentDomain || policy == CFHTTPCookieStorageAcceptPolicyAlways;
}

}
