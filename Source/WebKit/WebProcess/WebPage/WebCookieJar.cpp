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
#include "WebProcess.h"
#include <WebCore/CookieRequestHeaderFieldProxy.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/StorageSessionProvider.h>

namespace WebKit {

class WebStorageSessionProvider : public WebCore::StorageSessionProvider {
    // NetworkStorageSessions are accessed only in the NetworkProcess.
    WebCore::NetworkStorageSession* storageSession() const final { return nullptr; }
};

WebCookieJar::WebCookieJar()
    : WebCore::CookieJar(adoptRef(*new WebStorageSessionProvider)) { }

String WebCookieJar::cookies(WebCore::Document& document, const URL& url) const
{
    Optional<uint64_t> frameID;
    Optional<uint64_t> pageID;
    if (auto* frame = document.frame()) {
        frameID = frame->loader().client().frameID();
        pageID = frame->loader().client().pageID();
    }

    String cookieString;
    bool secureCookiesAccessed = false;
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookiesForDOM(document.sessionID(), document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, shouldIncludeSecureCookies(document, url)), Messages::NetworkConnectionToWebProcess::CookiesForDOM::Reply(cookieString, secureCookiesAccessed), 0))
        return { };

    return cookieString;
}

void WebCookieJar::setCookies(WebCore::Document& document, const URL& url, const String& cookieString)
{
    Optional<uint64_t> frameID;
    Optional<uint64_t> pageID;
    if (auto* frame = document.frame()) {
        frameID = frame->loader().client().frameID();
        pageID = frame->loader().client().pageID();
    }

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetCookiesFromDOM(document.sessionID(), document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, cookieString), 0);
}

bool WebCookieJar::cookiesEnabled(const WebCore::Document& document) const
{
    bool result = false;
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookiesEnabled(document.sessionID()), Messages::NetworkConnectionToWebProcess::CookiesEnabled::Reply(result), 0))
        return false;
    return result;
}

std::pair<String, WebCore::SecureCookiesAccessed> WebCookieJar::cookieRequestHeaderFieldValue(const PAL::SessionID& sessionID, const URL& firstParty, const WebCore::SameSiteInfo& sameSiteInfo, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, WebCore::IncludeSecureCookies includeSecureCookies) const
{
    String cookieString;
    bool secureCookiesAccessed = false;
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue(sessionID, firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies), Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue::Reply(cookieString, secureCookiesAccessed), 0))
        return { };
    return { cookieString, secureCookiesAccessed ? WebCore::SecureCookiesAccessed::Yes : WebCore::SecureCookiesAccessed::No };
}

bool WebCookieJar::getRawCookies(const WebCore::Document& document, const URL& url, Vector<WebCore::Cookie>& rawCookies) const
{
    Optional<uint64_t> frameID;
    Optional<uint64_t> pageID;
    if (auto* frame = document.frame()) {
        frameID = frame->loader().client().frameID();
        pageID = frame->loader().client().pageID();
    }

    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::GetRawCookies(document.sessionID(), document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID), Messages::NetworkConnectionToWebProcess::GetRawCookies::Reply(rawCookies), 0))
        return false;
    return true;
}

void WebCookieJar::deleteCookie(const WebCore::Document& document, const URL& url, const String& cookieName)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::DeleteCookie(document.sessionID(), url, cookieName), 0);
}

} // namespace WebKit
