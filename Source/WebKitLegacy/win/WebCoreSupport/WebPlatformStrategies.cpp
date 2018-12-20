/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#include "WebPlatformStrategies.h"

#include "WebFrameNetworkingContext.h"
#include "WebResourceLoadScheduler.h"
#include <WebCore/BlobRegistryImpl.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>

#if USE(CFURLCONNECTION)
#include <pal/spi/cf/CFNetworkSPI.h>
#endif

using namespace WebCore;

void WebPlatformStrategies::initialize()
{
    static NeverDestroyed<WebPlatformStrategies> platformStrategies;
}

WebPlatformStrategies::WebPlatformStrategies()
{
    setPlatformStrategies(this);
}

CookiesStrategy* WebPlatformStrategies::createCookiesStrategy()
{
    return this;
}

LoaderStrategy* WebPlatformStrategies::createLoaderStrategy()
{
    return new WebResourceLoadScheduler;
}

PasteboardStrategy* WebPlatformStrategies::createPasteboardStrategy()
{
    return nullptr;
}

BlobRegistry* WebPlatformStrategies::createBlobRegistry()
{
    return new BlobRegistryImpl;
}

std::pair<String, bool> WebPlatformStrategies::cookiesForDOM(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    return session.cookiesForDOM(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies);
}

void WebPlatformStrategies::setCookiesFromDOM(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, const String& cookieString)
{
    session.setCookiesFromDOM(firstParty, sameSiteInfo, url, frameID, pageID, cookieString);
}

bool WebPlatformStrategies::cookiesEnabled(const NetworkStorageSession& session)
{
    return session.cookiesEnabled();
}

std::pair<String, bool> WebPlatformStrategies::cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    return session.cookieRequestHeaderFieldValue(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies);
}

std::pair<String, bool> WebPlatformStrategies::cookieRequestHeaderFieldValue(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    auto& session = sessionID.isEphemeral() ? WebFrameNetworkingContext::ensurePrivateBrowsingSession() : NetworkStorageSession::defaultStorageSession();
    return session.cookieRequestHeaderFieldValue(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies);
}

bool WebPlatformStrategies::getRawCookies(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, Vector<Cookie>& rawCookies)
{
    return session.getRawCookies(firstParty, sameSiteInfo, url, frameID, pageID, rawCookies);
}

void WebPlatformStrategies::deleteCookie(const NetworkStorageSession& session, const URL& url, const String& cookieName)
{
    session.deleteCookie(url, cookieName);
}
