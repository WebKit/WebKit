/*
 * Copyright (C) 2012, 2016 Apple Inc. All rights reserved.
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

#include "CookiesStrategy.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "NetworkStorageSession.h"
#include "NetworkingContext.h"
#include "PlatformCookieJar.h"
#include "PlatformStrategies.h"

namespace WebCore {

static NetworkingContext* networkingContext(const Document& document)
{
    // FIXME: Returning 0 means falling back to default context. That's not a choice that is appropriate to do at runtime
    Frame* frame = document.frame();
    if (!frame)
        return nullptr;

    return frame->loader().networkingContext();
}

inline NetworkStorageSession& storageSession(const Document& document)
{
    NetworkingContext* context = networkingContext(document);
    return context ? context->storageSession() : NetworkStorageSession::defaultStorageSession();
}

String cookies(const Document& document, const URL& url)
{
    return platformStrategies()->cookiesStrategy()->cookiesForDOM(storageSession(document), document.firstPartyForCookies(), url);
}

void setCookies(Document& document, const URL& url, const String& cookieString)
{
    platformStrategies()->cookiesStrategy()->setCookiesFromDOM(storageSession(document), document.firstPartyForCookies(), url, cookieString);
}

bool cookiesEnabled(const Document& document)
{
    return platformStrategies()->cookiesStrategy()->cookiesEnabled(storageSession(document), document.firstPartyForCookies(), document.cookieURL());
}

String cookieRequestHeaderFieldValue(const Document& document, const URL& url)
{
    return platformStrategies()->cookiesStrategy()->cookieRequestHeaderFieldValue(storageSession(document), document.firstPartyForCookies(), url);
}

bool getRawCookies(const Document& document, const URL& url, Vector<Cookie>& cookies)
{
    return platformStrategies()->cookiesStrategy()->getRawCookies(storageSession(document), document.firstPartyForCookies(), url, cookies);
}

void deleteCookie(const Document& document, const URL& url, const String& cookieName)
{
    platformStrategies()->cookiesStrategy()->deleteCookie(storageSession(document), url, cookieName);
}

void addCookie(const Document& document, const URL& url, const Cookie& cookie)
{
    platformStrategies()->cookiesStrategy()->addCookie(storageSession(document), url, cookie);
}

}
