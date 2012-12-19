/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CookieJar.h"

#include "Document.h"
#include "Frame.h"
#include "PlatformCookieJar.h"

#if USE(PLATFORM_STRATEGIES)
#include "CookiesStrategy.h"
#include "PlatformStrategies.h"
#endif

#if PLATFORM(BLACKBERRY)
#error Blackberry currently uses a fork of this file because of layering violations
#endif

namespace WebCore {

static NetworkingContext* networkingContext(const Document* document)
{
    if (!document)
        return 0;
    Frame* frame = document->frame();
    if (!frame)
        return 0;
    FrameLoader* loader = frame->loader();
    if (!loader)
        return 0;
    return loader->networkingContext();
}

String cookies(const Document* document, const KURL& url)
{
#if USE(PLATFORM_STRATEGIES)
    return platformStrategies()->cookiesStrategy()->cookiesForDOM(networkingContext(document), document->firstPartyForCookies(), url);
#else
    return cookiesForDOM(networkingContext(document), document->firstPartyForCookies(), url);
#endif
}

void setCookies(Document* document, const KURL& url, const String& cookieString)
{
#if USE(PLATFORM_STRATEGIES)
    platformStrategies()->cookiesStrategy()->setCookiesFromDOM(networkingContext(document), document->firstPartyForCookies(), url, cookieString);
#else
    setCookiesFromDOM(networkingContext(document), document->firstPartyForCookies(), url, cookieString);
#endif
}

bool cookiesEnabled(const Document* document)
{
#if USE(PLATFORM_STRATEGIES)
    return platformStrategies()->cookiesStrategy()->cookiesEnabled(networkingContext(document), document->firstPartyForCookies(), document->cookieURL());
#else
    return cookiesEnabled(networkingContext(document), document->firstPartyForCookies(), document->cookieURL());
#endif
}

String cookieRequestHeaderFieldValue(const Document* document, const KURL& url)
{
#if USE(PLATFORM_STRATEGIES)
    return platformStrategies()->cookiesStrategy()->cookieRequestHeaderFieldValue(networkingContext(document), document->firstPartyForCookies(), url);
#else
    return cookieRequestHeaderFieldValue(networkingContext(document), document->firstPartyForCookies(), url);
#endif
}

bool getRawCookies(const Document* document, const KURL& url, Vector<Cookie>& cookies)
{
#if USE(PLATFORM_STRATEGIES)
    return platformStrategies()->cookiesStrategy()->getRawCookies(networkingContext(document), document->firstPartyForCookies(), url, cookies);
#else
    return getRawCookies(networkingContext(document), document->firstPartyForCookies(), url, cookies);
#endif
}

void deleteCookie(const Document* document, const KURL& url, const String& cookieName)
{
#if USE(PLATFORM_STRATEGIES)
    platformStrategies()->cookiesStrategy()->deleteCookie(networkingContext(document), url, cookieName);
#else
    deleteCookie(networkingContext(document), url, cookieName);
#endif
}

}
