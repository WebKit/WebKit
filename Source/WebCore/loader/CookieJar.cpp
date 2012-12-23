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
#include "NetworkingContext.h"
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
    // FIXME: Returning 0 means falling back to default context. That's not a choice that is appropriate to do at runtime
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

#if PLATFORM(MAC) || USE(CFNETWORK)
inline NetworkStorageSession& storageSession(const Document* document)
{
    NetworkingContext* context = networkingContext(document);
    return context ? context->storageSession() : NetworkStorageSession::defaultStorageSession();
}
#define LOCAL_SESSION(document) NetworkStorageSession& session = storageSession(document);
#else
#define LOCAL_SESSION(document) NetworkStorageSession session(networkingContext(document));
#endif

String cookies(const Document* document, const KURL& url)
{
    LOCAL_SESSION(document)
#if USE(PLATFORM_STRATEGIES)
    return platformStrategies()->cookiesStrategy()->cookiesForDOM(session, document->firstPartyForCookies(), url);
#else
    return cookiesForDOM(session, document->firstPartyForCookies(), url);
#endif
}

void setCookies(Document* document, const KURL& url, const String& cookieString)
{
    LOCAL_SESSION(document)
#if USE(PLATFORM_STRATEGIES)
    platformStrategies()->cookiesStrategy()->setCookiesFromDOM(session, document->firstPartyForCookies(), url, cookieString);
#else
    setCookiesFromDOM(session, document->firstPartyForCookies(), url, cookieString);
#endif
}

bool cookiesEnabled(const Document* document)
{
    LOCAL_SESSION(document)
#if USE(PLATFORM_STRATEGIES)
    return platformStrategies()->cookiesStrategy()->cookiesEnabled(session, document->firstPartyForCookies(), document->cookieURL());
#else
    return cookiesEnabled(session, document->firstPartyForCookies(), document->cookieURL());
#endif
}

String cookieRequestHeaderFieldValue(const Document* document, const KURL& url)
{
    LOCAL_SESSION(document)
#if USE(PLATFORM_STRATEGIES)
    return platformStrategies()->cookiesStrategy()->cookieRequestHeaderFieldValue(session, document->firstPartyForCookies(), url);
#else
    return cookieRequestHeaderFieldValue(session, document->firstPartyForCookies(), url);
#endif
}

bool getRawCookies(const Document* document, const KURL& url, Vector<Cookie>& cookies)
{
    LOCAL_SESSION(document)
#if USE(PLATFORM_STRATEGIES)
    return platformStrategies()->cookiesStrategy()->getRawCookies(session, document->firstPartyForCookies(), url, cookies);
#else
    return getRawCookies(session, document->firstPartyForCookies(), url, cookies);
#endif
}

void deleteCookie(const Document* document, const KURL& url, const String& cookieName)
{
    LOCAL_SESSION(document)
#if USE(PLATFORM_STRATEGIES)
    platformStrategies()->cookiesStrategy()->deleteCookie(session, url, cookieName);
#else
    deleteCookie(session, url, cookieName);
#endif
}

}
