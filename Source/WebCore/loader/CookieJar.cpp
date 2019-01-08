/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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

#include "CookieRequestHeaderFieldProxy.h"
#include "CookiesStrategy.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "NetworkStorageSession.h"
#include "NetworkingContext.h"
#include "PlatformStrategies.h"
#include "SameSiteInfo.h"
#include <wtf/SystemTracing.h>

namespace WebCore {

static IncludeSecureCookies shouldIncludeSecureCookies(const Document& document, const URL& url)
{
    return (url.protocolIs("https") && !document.foundMixedContent().contains(SecurityContext::MixedContentType::Active)) ? IncludeSecureCookies::Yes : IncludeSecureCookies::No;
}

static inline SameSiteInfo sameSiteInfo(const Document& document)
{
    if (auto* loader = document.loader())
        return SameSiteInfo::create(loader->request());
    return { };
}

String cookies(Document& document, const URL& url)
{
    TraceScope scope(FetchCookiesStart, FetchCookiesEnd);

    auto includeSecureCookies = shouldIncludeSecureCookies(document, url);

    std::pair<String, bool> result;
    auto frame = document.frame();
    if (frame)
        result = platformStrategies()->cookiesStrategy()->cookiesForDOM(document.sessionID(), document.firstPartyForCookies(), sameSiteInfo(document), url, frame->loader().client().frameID(), frame->loader().client().pageID(), includeSecureCookies);
    else
        result = platformStrategies()->cookiesStrategy()->cookiesForDOM(document.sessionID(), document.firstPartyForCookies(), sameSiteInfo(document), url, WTF::nullopt, WTF::nullopt, includeSecureCookies);

    if (result.second)
        document.setSecureCookiesAccessed();

    return result.first;
}

CookieRequestHeaderFieldProxy cookieRequestHeaderFieldProxy(const Document& document, const URL& url)
{
    TraceScope scope(FetchCookiesStart, FetchCookiesEnd);

    CookieRequestHeaderFieldProxy proxy;
    proxy.sessionID = document.sessionID();
    proxy.firstParty = document.firstPartyForCookies();
    proxy.sameSiteInfo = sameSiteInfo(document);
    proxy.url = url;
    proxy.includeSecureCookies = shouldIncludeSecureCookies(document, url);
    if (auto* frame = document.frame()) {
        proxy.frameID = frame->loader().client().frameID();
        proxy.pageID = frame->loader().client().pageID();
    }
    return proxy;
}

void setCookies(Document& document, const URL& url, const String& cookieString)
{
    auto frame = document.frame();
    if (frame)
        platformStrategies()->cookiesStrategy()->setCookiesFromDOM(document.sessionID(), document.firstPartyForCookies(), sameSiteInfo(document), url, frame->loader().client().frameID(), frame->loader().client().pageID(), cookieString);
    else
        platformStrategies()->cookiesStrategy()->setCookiesFromDOM(document.sessionID(), document.firstPartyForCookies(), sameSiteInfo(document), url, WTF::nullopt, WTF::nullopt, cookieString);
}

bool cookiesEnabled(const Document& document)
{
    return platformStrategies()->cookiesStrategy()->cookiesEnabled(document.sessionID());
}

String cookieRequestHeaderFieldValue(Document& document, const URL& url)
{
    auto includeSecureCookies = shouldIncludeSecureCookies(document, url);

    std::pair<String, bool> result;
    auto frame = document.frame();
    if (frame)
        result = platformStrategies()->cookiesStrategy()->cookieRequestHeaderFieldValue(document.sessionID(), document.firstPartyForCookies(), sameSiteInfo(document), url, frame->loader().client().frameID(), frame->loader().client().pageID(), includeSecureCookies);
    else
        result = platformStrategies()->cookiesStrategy()->cookieRequestHeaderFieldValue(document.sessionID(), document.firstPartyForCookies(), sameSiteInfo(document), url, WTF::nullopt, WTF::nullopt, includeSecureCookies);

    if (result.second)
        document.setSecureCookiesAccessed();

    return result.first;
}

bool getRawCookies(const Document& document, const URL& url, Vector<Cookie>& cookies)
{
    auto frame = document.frame();
    if (frame)
        return platformStrategies()->cookiesStrategy()->getRawCookies(document.sessionID(), document.firstPartyForCookies(), sameSiteInfo(document), url, frame->loader().client().frameID(), frame->loader().client().pageID(), cookies);

    return platformStrategies()->cookiesStrategy()->getRawCookies(document.sessionID(), document.firstPartyForCookies(), sameSiteInfo(document), url, WTF::nullopt, WTF::nullopt, cookies);
}

void deleteCookie(const Document& document, const URL& url, const String& cookieName)
{
    platformStrategies()->cookiesStrategy()->deleteCookie(document.sessionID(), url, cookieName);
}

}
