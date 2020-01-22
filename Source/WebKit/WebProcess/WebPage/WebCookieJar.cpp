/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/StorageSessionProvider.h>

namespace WebKit {

using namespace WebCore;

class WebStorageSessionProvider : public WebCore::StorageSessionProvider {
    // NetworkStorageSessions are accessed only in the NetworkProcess.
    WebCore::NetworkStorageSession* storageSession() const final { return nullptr; }
};

WebCookieJar::WebCookieJar()
    : WebCore::CookieJar(adoptRef(*new WebStorageSessionProvider)) { }

#if ENABLE(RESOURCE_LOAD_STATISTICS)
static bool shouldBlockCookies(WebFrame* frame, const URL& firstPartyForCookies, const URL& resourceURL, ShouldAskITP& shouldAskITPInNetworkProcess)
{
    if (!WebCore::DeprecatedGlobalSettings::resourceLoadStatisticsEnabled())
        return false;

    if (frame && frame->isMainFrame())
        return false;

    RegistrableDomain firstPartyDomain { firstPartyForCookies };
    if (firstPartyDomain.isEmpty())
        return false;

    RegistrableDomain resourceDomain { resourceURL };
    if (resourceDomain.isEmpty())
        return false;

    if (firstPartyDomain == resourceDomain)
        return false;

    if (frame && frame->frameLoaderClient()->hasFrameSpecificStorageAccess())
        return false;

    if (frame && frame->page() && frame->page()->hasPageLevelStorageAccess(firstPartyDomain, resourceDomain))
        return false;

    // The WebContent process does not have enough information to deal with other policies than ThirdPartyCookieBlockingMode::All so we have to go to the NetworkProcess for all
    // other policies and the request may end up getting blocked on NetworkProcess side.
    if (WebProcess::singleton().thirdPartyCookieBlockingMode() != ThirdPartyCookieBlockingMode::All) {
        shouldAskITPInNetworkProcess = ShouldAskITP::Yes;
        return false;
    }

    return true;
}
#endif

String WebCookieJar::cookies(WebCore::Document& document, const URL& url) const
{
    auto* webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.frame()) : nullptr;
    if (!webFrame || !webFrame->page())
        return { };

    ShouldAskITP shouldAskITPInNetworkProcess = ShouldAskITP::No;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (shouldBlockCookies(webFrame, document.firstPartyForCookies(), url, shouldAskITPInNetworkProcess))
        return { };
#endif

    auto frameID = webFrame->frameID();
    auto pageID = webFrame->page()->identifier();

    String cookieString;
    bool secureCookiesAccessed = false;
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookiesForDOM(document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, shouldIncludeSecureCookies(document, url), shouldAskITPInNetworkProcess), Messages::NetworkConnectionToWebProcess::CookiesForDOM::Reply(cookieString, secureCookiesAccessed), 0))
        return { };

    return cookieString;
}

void WebCookieJar::setCookies(WebCore::Document& document, const URL& url, const String& cookieString)
{
    auto* webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.frame()) : nullptr;
    if (!webFrame || !webFrame->page())
        return;

    ShouldAskITP shouldAskITPInNetworkProcess = ShouldAskITP::No;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (shouldBlockCookies(webFrame, document.firstPartyForCookies(), url, shouldAskITPInNetworkProcess))
        return;
#endif

    auto frameID = webFrame->frameID();
    auto pageID = webFrame->page()->identifier();

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetCookiesFromDOM(document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, shouldAskITPInNetworkProcess, cookieString), 0);
}

bool WebCookieJar::cookiesEnabled(const Document& document) const
{
    auto* webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.frame()) : nullptr;
    if (!webFrame || !webFrame->page())
        return false;

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    ShouldAskITP dummy;
    if (shouldBlockCookies(webFrame, document.firstPartyForCookies(), document.cookieURL(), dummy))
        return false;
#endif

    return WebProcess::singleton().ensureNetworkProcessConnection().cookiesEnabled();
}

std::pair<String, WebCore::SecureCookiesAccessed> WebCookieJar::cookieRequestHeaderFieldValue(const URL& firstParty, const WebCore::SameSiteInfo& sameSiteInfo, const URL& url, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, WebCore::IncludeSecureCookies includeSecureCookies) const
{
    ShouldAskITP shouldAskITPInNetworkProcess = ShouldAskITP::No;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto* webFrame = frameID ? WebProcess::singleton().webFrame(*frameID) : nullptr;
    if (shouldBlockCookies(webFrame, firstParty, url, shouldAskITPInNetworkProcess))
        return { };
#endif

    String cookieString;
    bool secureCookiesAccessed = false;
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, shouldAskITPInNetworkProcess), Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue::Reply(cookieString, secureCookiesAccessed), 0))
        return { };
    return { cookieString, secureCookiesAccessed ? WebCore::SecureCookiesAccessed::Yes : WebCore::SecureCookiesAccessed::No };
}

bool WebCookieJar::getRawCookies(const WebCore::Document& document, const URL& url, Vector<WebCore::Cookie>& rawCookies) const
{
    auto* webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.frame()) : nullptr;
    ShouldAskITP shouldAskITPInNetworkProcess = ShouldAskITP::No;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (shouldBlockCookies(webFrame, document.firstPartyForCookies(), url, shouldAskITPInNetworkProcess))
        return { };
#endif

    Optional<FrameIdentifier> frameID = webFrame ? makeOptional(webFrame->frameID()) : WTF::nullopt;
    Optional<PageIdentifier> pageID = webFrame && webFrame->page() ? makeOptional(webFrame->page()->identifier()) : WTF::nullopt;
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::GetRawCookies(document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, shouldAskITPInNetworkProcess), Messages::NetworkConnectionToWebProcess::GetRawCookies::Reply(rawCookies), 0))
        return false;
    return true;
}

void WebCookieJar::deleteCookie(const WebCore::Document& document, const URL& url, const String& cookieName)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::DeleteCookie(url, cookieName), 0);
}

} // namespace WebKit
