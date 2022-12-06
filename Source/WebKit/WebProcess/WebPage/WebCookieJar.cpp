/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
#include "WebCookieJar.h"

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/CookieRequestHeaderFieldProxy.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/Settings.h>
#include <WebCore/StorageSessionProvider.h>

namespace WebKit {

using namespace WebCore;

class WebStorageSessionProvider : public WebCore::StorageSessionProvider {
    // NetworkStorageSessions are accessed only in the NetworkProcess.
    WebCore::NetworkStorageSession* storageSession() const final { return nullptr; }
};

WebCookieJar::WebCookieJar()
    : WebCore::CookieJar(adoptRef(*new WebStorageSessionProvider)) { }

#if ENABLE(TRACKING_PREVENTION)
static bool shouldBlockCookies(WebFrame* frame, const URL& firstPartyForCookies, const URL& resourceURL, ApplyTrackingPrevention& applyTrackingPreventionInNetworkProcess)
{
    if (!WebCore::DeprecatedGlobalSettings::trackingPreventionEnabled())
        return false;

    RegistrableDomain firstPartyDomain { firstPartyForCookies };
    if (firstPartyDomain.isEmpty())
        return false;

    RegistrableDomain resourceDomain { resourceURL };
    if (resourceDomain.isEmpty())
        return false;

    if (firstPartyDomain == resourceDomain)
        return false;

    if (frame) {
        if (frame->frameLoaderClient()->hasFrameSpecificStorageAccess())
            return false;
        if (auto* page = frame->page()) {
            if (page->hasPageLevelStorageAccess(firstPartyDomain, resourceDomain))
                return false;
            if (auto* corePage = page->corePage()) {
                if (corePage->shouldRelaxThirdPartyCookieBlocking() == WebCore::ShouldRelaxThirdPartyCookieBlocking::Yes)
                    return false;
            }
        }
    }

    // The WebContent process does not have enough information to deal with other policies than ThirdPartyCookieBlockingMode::All so we have to go to the NetworkProcess for all
    // other policies and the request may end up getting blocked on NetworkProcess side.
    if (WebProcess::singleton().thirdPartyCookieBlockingMode() != ThirdPartyCookieBlockingMode::All) {
        applyTrackingPreventionInNetworkProcess = ApplyTrackingPrevention::Yes;
        return false;
    }

    return true;
}
#endif

bool WebCookieJar::isEligibleForCache(WebFrame& frame, const URL& firstPartyForCookies, const URL& resourceURL) const
{
    auto* page = frame.page() ? frame.page()->corePage() : nullptr;
    if (!page)
        return false;

    if (!m_cache.isSupported())
        return false;

    // For now, we only cache cookies for first-party content. Third-party cookie caching is a bit more complicated due to partitioning and storage access.
    RegistrableDomain resourceDomain { resourceURL };
    if (resourceDomain.isEmpty())
        return false;

    return frame.isMainFrame() || RegistrableDomain { firstPartyForCookies } == resourceDomain;
}

static WebCore::ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking(const WebFrame* frame)
{
    if (!frame)
        return WebCore::ShouldRelaxThirdPartyCookieBlocking::No;
    auto* page = frame->page();
    if (!page)
        return WebCore::ShouldRelaxThirdPartyCookieBlocking::No;
    auto* corePage = page->corePage();
    if (!corePage)
        return WebCore::ShouldRelaxThirdPartyCookieBlocking::No;
    return corePage->shouldRelaxThirdPartyCookieBlocking();
}

String WebCookieJar::cookies(WebCore::Document& document, const URL& url) const
{
    auto* webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.frame()) : nullptr;
    if (!webFrame || !webFrame->page())
        return { };

    ApplyTrackingPrevention applyTrackingPreventionInNetworkProcess = ApplyTrackingPrevention::No;
#if ENABLE(TRACKING_PREVENTION)
    if (shouldBlockCookies(webFrame, document.firstPartyForCookies(), url, applyTrackingPreventionInNetworkProcess))
        return { };
#endif

    auto sameSiteInfo = CookieJar::sameSiteInfo(document, IsForDOMCookieAccess::Yes);
    auto includeSecureCookies = CookieJar::shouldIncludeSecureCookies(document, url);
    auto frameID = webFrame->frameID();
    auto pageID = webFrame->page()->identifier();

    if (isEligibleForCache(*webFrame, document.firstPartyForCookies(), url))
        return m_cache.cookiesForDOM(document.firstPartyForCookies(), sameSiteInfo, url, frameID, pageID, includeSecureCookies);

    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookiesForDOM(document.firstPartyForCookies(), sameSiteInfo, url, frameID, pageID, includeSecureCookies, applyTrackingPreventionInNetworkProcess, shouldRelaxThirdPartyCookieBlocking(webFrame)), 0);
    auto [cookieString, secureCookiesAccessed] = sendResult.takeReplyOr(String { }, false);

    return cookieString;
}

void WebCookieJar::setCookies(WebCore::Document& document, const URL& url, const String& cookieString)
{
    auto* webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.frame()) : nullptr;
    if (!webFrame || !webFrame->page())
        return;

    ApplyTrackingPrevention applyTrackingPreventionInNetworkProcess = ApplyTrackingPrevention::No;
#if ENABLE(TRACKING_PREVENTION)
    if (shouldBlockCookies(webFrame, document.firstPartyForCookies(), url, applyTrackingPreventionInNetworkProcess))
        return;
#endif

    auto sameSiteInfo = CookieJar::sameSiteInfo(document, IsForDOMCookieAccess::Yes);
    auto frameID = webFrame->frameID();
    auto pageID = webFrame->page()->identifier();

    if (isEligibleForCache(*webFrame, document.firstPartyForCookies(), url))
        m_cache.setCookiesFromDOM(document.firstPartyForCookies(), sameSiteInfo, url, frameID, pageID, cookieString, shouldRelaxThirdPartyCookieBlocking(webFrame));

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetCookiesFromDOM(document.firstPartyForCookies(), sameSiteInfo, url, frameID, pageID, applyTrackingPreventionInNetworkProcess, cookieString, shouldRelaxThirdPartyCookieBlocking(webFrame)), 0);
}

void WebCookieJar::cookiesAdded(const String& host, const Vector<WebCore::Cookie>& cookies)
{
    m_cache.cookiesAdded(host, cookies);
}

void WebCookieJar::cookiesDeleted(const String& host, const Vector<WebCore::Cookie>& cookies)
{
    m_cache.cookiesDeleted(host, cookies);
}

void WebCookieJar::allCookiesDeleted()
{
    m_cache.allCookiesDeleted();
}

void WebCookieJar::clearCache()
{
    m_cache.clear();
}

void WebCookieJar::clearCacheForHost(const String& host)
{
    m_cache.clearForHost(host);
}

bool WebCookieJar::cookiesEnabled(const Document& document) const
{
    auto* webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.frame()) : nullptr;
    if (!webFrame || !webFrame->page())
        return false;

#if ENABLE(TRACKING_PREVENTION)
    ApplyTrackingPrevention dummy;
    if (shouldBlockCookies(webFrame, document.firstPartyForCookies(), document.cookieURL(), dummy))
        return false;
#endif

    return WebProcess::singleton().ensureNetworkProcessConnection().cookiesEnabled();
}

std::pair<String, WebCore::SecureCookiesAccessed> WebCookieJar::cookieRequestHeaderFieldValue(const URL& firstParty, const WebCore::SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, WebCore::IncludeSecureCookies includeSecureCookies) const
{
    ApplyTrackingPrevention applyTrackingPreventionInNetworkProcess = ApplyTrackingPrevention::No;
    auto* webFrame = frameID ? WebProcess::singleton().webFrame(*frameID) : nullptr;
#if ENABLE(TRACKING_PREVENTION)
    if (shouldBlockCookies(webFrame, firstParty, url, applyTrackingPreventionInNetworkProcess))
        return { };
#endif

    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, applyTrackingPreventionInNetworkProcess, shouldRelaxThirdPartyCookieBlocking(webFrame)), 0);
    if (!sendResult)
        return { };

    auto [cookieString, secureCookiesAccessed] = sendResult.takeReply();
    return { cookieString, secureCookiesAccessed ? WebCore::SecureCookiesAccessed::Yes : WebCore::SecureCookiesAccessed::No };
}

bool WebCookieJar::getRawCookies(const WebCore::Document& document, const URL& url, Vector<WebCore::Cookie>& rawCookies) const
{
    auto* webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.frame()) : nullptr;
    ApplyTrackingPrevention applyTrackingPreventionInNetworkProcess = ApplyTrackingPrevention::No;
#if ENABLE(TRACKING_PREVENTION)
    if (shouldBlockCookies(webFrame, document.firstPartyForCookies(), url, applyTrackingPreventionInNetworkProcess))
        return { };
#endif

    std::optional<FrameIdentifier> frameID = webFrame ? std::make_optional(webFrame->frameID()) : std::nullopt;
    std::optional<PageIdentifier> pageID = webFrame && webFrame->page() ? std::make_optional(webFrame->page()->identifier()) : std::nullopt;
    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::GetRawCookies(document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, applyTrackingPreventionInNetworkProcess, shouldRelaxThirdPartyCookieBlocking(webFrame)), 0);
    if (!sendResult)
        return false;

    std::tie(rawCookies) = sendResult.takeReply();
    return true;
}

void WebCookieJar::setRawCookie(const WebCore::Document& document, const Cookie& cookie)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetRawCookie(cookie), 0);
}

void WebCookieJar::deleteCookie(const WebCore::Document& document, const URL& url, const String& cookieName, CompletionHandler<void()>&& completionHandler)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::DeleteCookie(url, cookieName), WTFMove(completionHandler));
}

} // namespace WebKit
