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
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTTPCookieAcceptPolicy.h"
#include "NetworkStorageSession.h"
#include "NetworkingContext.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "SameSiteInfo.h"
#include "StorageSessionProvider.h"
#include <wtf/SystemTracing.h>

namespace WebCore {

static ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking(const Document& document)
{
    if (auto* page = document.page())
        return page->shouldRelaxThirdPartyCookieBlocking();
    return ShouldRelaxThirdPartyCookieBlocking::No;
}

Ref<CookieJar> CookieJar::create(Ref<StorageSessionProvider>&& storageSessionProvider)
{
    return adoptRef(*new CookieJar(WTFMove(storageSessionProvider)));
}

IncludeSecureCookies CookieJar::shouldIncludeSecureCookies(const Document& document, const URL& url)
{
    return (url.protocolIs("https") && !document.foundMixedContent().contains(SecurityContext::MixedContentType::Active)) ? IncludeSecureCookies::Yes : IncludeSecureCookies::No;
}

SameSiteInfo CookieJar::sameSiteInfo(const Document& document)
{
    if (auto* loader = document.loader())
        return SameSiteInfo::create(loader->request());
    return { };
}

CookieJar::CookieJar(Ref<StorageSessionProvider>&& storageSessionProvider)
    : m_storageSessionProvider(WTFMove(storageSessionProvider))
{
}

CookieJar::~CookieJar() = default;

String CookieJar::cookies(Document& document, const URL& url) const
{
    TraceScope scope(FetchCookiesStart, FetchCookiesEnd);

    auto includeSecureCookies = shouldIncludeSecureCookies(document, url);

    Optional<FrameIdentifier> frameID;
    Optional<PageIdentifier> pageID;
    if (auto* frame = document.frame()) {
        frameID = frame->loader().frameID();
        pageID = frame->loader().pageID();
    }

    std::pair<String, bool> result;
    if (auto* session = m_storageSessionProvider->storageSession())
        result = session->cookiesForDOM(document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, includeSecureCookies, ShouldAskITP::Yes, shouldRelaxThirdPartyCookieBlocking(document));
    else
        ASSERT_NOT_REACHED();

    if (result.second)
        document.setSecureCookiesAccessed();

    return result.first;
}

CookieRequestHeaderFieldProxy CookieJar::cookieRequestHeaderFieldProxy(const Document& document, const URL& url)
{
    TraceScope scope(FetchCookiesStart, FetchCookiesEnd);

    Optional<FrameIdentifier> frameID;
    Optional<PageIdentifier> pageID;
    if (auto* frame = document.frame()) {
        frameID = frame->loader().frameID();
        pageID = frame->loader().pageID();
    }

    return { document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, shouldIncludeSecureCookies(document, url) };
}

void CookieJar::setCookies(Document& document, const URL& url, const String& cookieString)
{
    Optional<FrameIdentifier> frameID;
    Optional<PageIdentifier> pageID;
    if (auto* frame = document.frame()) {
        frameID = frame->loader().frameID();
        pageID = frame->loader().pageID();
    }

    if (auto* session = m_storageSessionProvider->storageSession())
        session->setCookiesFromDOM(document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, ShouldAskITP::Yes, cookieString, shouldRelaxThirdPartyCookieBlocking(document));
    else
        ASSERT_NOT_REACHED();
}

bool CookieJar::cookiesEnabled(const Document&) const
{
    if (auto* session = m_storageSessionProvider->storageSession())
        return session->cookieAcceptPolicy() != HTTPCookieAcceptPolicy::Never;

    ASSERT_NOT_REACHED();
    return false;
}

std::pair<String, SecureCookiesAccessed> CookieJar::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies) const
{
    if (auto* session = m_storageSessionProvider->storageSession()) {
        std::pair<String, bool> result = session->cookieRequestHeaderFieldValue(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, ShouldAskITP::Yes, ShouldRelaxThirdPartyCookieBlocking::No);
        return { result.first, result.second ? SecureCookiesAccessed::Yes : SecureCookiesAccessed::No };
    }

    ASSERT_NOT_REACHED();
    return { };
}

String CookieJar::cookieRequestHeaderFieldValue(Document& document, const URL& url) const
{
    Optional<FrameIdentifier> frameID;
    Optional<PageIdentifier> pageID;
    if (auto* frame = document.frame()) {
        frameID = frame->loader().frameID();
        pageID = frame->loader().pageID();
    }

    auto result = cookieRequestHeaderFieldValue(document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, shouldIncludeSecureCookies(document, url));
    if (result.second == SecureCookiesAccessed::Yes)
        document.setSecureCookiesAccessed();
    return result.first;
}

bool CookieJar::getRawCookies(const Document& document, const URL& url, Vector<Cookie>& cookies) const
{
    Optional<FrameIdentifier> frameID;
    Optional<PageIdentifier> pageID;
    if (auto* frame = document.frame()) {
        frameID = frame->loader().frameID();
        pageID = frame->loader().pageID();
    }

    if (auto* session = m_storageSessionProvider->storageSession())
        return session->getRawCookies(document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, ShouldAskITP::Yes, shouldRelaxThirdPartyCookieBlocking(document), cookies);

    ASSERT_NOT_REACHED();
    return false;
}

void CookieJar::setRawCookie(const Document&, const Cookie& cookie)
{
    if (auto* session = m_storageSessionProvider->storageSession())
        session->setCookie(cookie);
    else
        ASSERT_NOT_REACHED();
}

void CookieJar::deleteCookie(const Document&, const URL& url, const String& cookieName)
{
    if (auto* session = m_storageSessionProvider->storageSession())
        session->deleteCookie(url, cookieName);
    else
        ASSERT_NOT_REACHED();
}

}
