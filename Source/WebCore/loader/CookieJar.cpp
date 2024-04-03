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

#include "Cookie.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "CookieStoreGetOptions.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTTPCookieAcceptPolicy.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "NetworkStorageSession.h"
#include "NetworkingContext.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "StorageSessionProvider.h"
#include <optional>
#include <wtf/CompletionHandler.h>
#include <wtf/SystemTracing.h>

namespace WebCore {

static ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking(const Document& document)
{
    if (RefPtr page = document.page())
        return page->shouldRelaxThirdPartyCookieBlocking();
    return ShouldRelaxThirdPartyCookieBlocking::No;
}

Ref<CookieJar> CookieJar::create(Ref<StorageSessionProvider>&& storageSessionProvider)
{
    return adoptRef(*new CookieJar(WTFMove(storageSessionProvider)));
}

IncludeSecureCookies CookieJar::shouldIncludeSecureCookies(const Document& document, const URL& url)
{
    return (url.protocolIs("https"_s) && !document.foundMixedContent().contains(SecurityContext::MixedContentType::Active)) ? IncludeSecureCookies::Yes : IncludeSecureCookies::No;
}

SameSiteInfo CookieJar::sameSiteInfo(const Document& document, IsForDOMCookieAccess isAccessForDOM)
{
    RefPtr frame = document.frame();
    if (frame && frame->loader().client().isRemoteWorkerFrameLoaderClient()) {
        RegistrableDomain domain(document.securityOrigin().data());
        return { domain.matches(document.firstPartyForCookies()), false, true };
    }

    if (RefPtr loader = document.loader())
        return SameSiteInfo::create(loader->request(), isAccessForDOM);
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

    auto pageID = document.pageID();
    std::optional<FrameIdentifier> frameID;
    if (auto* frame = document.frame())
        frameID = frame->loader().frameID();

    std::pair<String, bool> result;
    if (auto* session = m_storageSessionProvider->storageSession())
        result = session->cookiesForDOM(document.firstPartyForCookies(), sameSiteInfo(document, IsForDOMCookieAccess::Yes), url, frameID, pageID, includeSecureCookies, ApplyTrackingPrevention::Yes, shouldRelaxThirdPartyCookieBlocking(document));
    else
        ASSERT_NOT_REACHED();

    if (result.second)
        document.setSecureCookiesAccessed();

    return result.first;
}

CookieRequestHeaderFieldProxy CookieJar::cookieRequestHeaderFieldProxy(const Document& document, const URL& url)
{
    TraceScope scope(FetchCookiesStart, FetchCookiesEnd);

    auto pageID = document.pageID();
    std::optional<FrameIdentifier> frameID;
    if (auto* frame = document.frame())
        frameID = frame->loader().frameID();

    return { document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, shouldIncludeSecureCookies(document, url) };
}

void CookieJar::setCookies(Document& document, const URL& url, const String& cookieString)
{
    auto pageID = document.pageID();
    std::optional<FrameIdentifier> frameID;
    if (auto* frame = document.frame())
        frameID = frame->loader().frameID();

    if (auto* session = m_storageSessionProvider->storageSession())
        session->setCookiesFromDOM(document.firstPartyForCookies(), sameSiteInfo(document, IsForDOMCookieAccess::Yes), url, frameID, pageID, ApplyTrackingPrevention::Yes, cookieString, shouldRelaxThirdPartyCookieBlocking(document));
    else
        ASSERT_NOT_REACHED();
}

bool CookieJar::cookiesEnabled(Document& document)
{
    auto cookieURL = document.cookieURL();
    if (cookieURL.isEmpty())
        return false;

    auto pageID = document.pageID();
    std::optional<FrameIdentifier> frameID;
    if (auto* frame = document.frame())
        frameID = frame->loader().frameID();

    if (auto* session = m_storageSessionProvider->storageSession())
        return session->cookiesEnabled(document.firstPartyForCookies(), cookieURL, frameID, pageID, shouldRelaxThirdPartyCookieBlocking(document));

    ASSERT_NOT_REACHED();
    return false;
}

void CookieJar::remoteCookiesEnabled(const Document&, CompletionHandler<void(bool)>&& completionHandler) const
{
    completionHandler(false);
}

std::pair<String, SecureCookiesAccessed> CookieJar::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies) const
{
    if (auto* session = m_storageSessionProvider->storageSession()) {
        std::pair<String, bool> result = session->cookieRequestHeaderFieldValue(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, ApplyTrackingPrevention::Yes, ShouldRelaxThirdPartyCookieBlocking::No);
        return { result.first, result.second ? SecureCookiesAccessed::Yes : SecureCookiesAccessed::No };
    }

    ASSERT_NOT_REACHED();
    return { };
}

String CookieJar::cookieRequestHeaderFieldValue(Document& document, const URL& url) const
{
    auto pageID = document.pageID();
    std::optional<FrameIdentifier> frameID;
    if (auto* frame = document.frame())
        frameID = frame->loader().frameID();

    auto result = cookieRequestHeaderFieldValue(document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, shouldIncludeSecureCookies(document, url));
    if (result.second == SecureCookiesAccessed::Yes)
        document.setSecureCookiesAccessed();
    return result.first;
}

bool CookieJar::getRawCookies(const Document& document, const URL& url, Vector<Cookie>& cookies) const
{
    auto pageID = document.pageID();
    std::optional<FrameIdentifier> frameID;
    if (auto* frame = document.frame())
        frameID = frame->loader().frameID();

    if (auto* session = m_storageSessionProvider->storageSession())
        return session->getRawCookies(document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, ApplyTrackingPrevention::Yes, shouldRelaxThirdPartyCookieBlocking(document), cookies);

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

void CookieJar::deleteCookie(const Document&, const URL& url, const String& cookieName, CompletionHandler<void()>&& completionHandler)
{
    if (auto* session = m_storageSessionProvider->storageSession())
        session->deleteCookie(url, cookieName, WTFMove(completionHandler));
    else {
        ASSERT_NOT_REACHED();
        completionHandler();
    }
}

void CookieJar::getCookiesAsync(Document&, const URL&, const CookieStoreGetOptions&, CompletionHandler<void(std::optional<Vector<Cookie>>&&)>&& completionHandler) const
{
    completionHandler(std::nullopt);
}

void CookieJar::setCookieAsync(Document&, const URL&, const Cookie&, CompletionHandler<void(bool)>&& completionHandler) const
{
    completionHandler(false);
}

#if HAVE(COOKIE_CHANGE_LISTENER_API)
void CookieJar::addChangeListener(const String&, const CookieChangeListener&)
{
}

void CookieJar::removeChangeListener(const String&, const CookieChangeListener&)
{
}
#endif

} // namespace WebCore
