/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "WebCookieCache.h"

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "WebProcess.h"

namespace WebKit {

using namespace WebCore;

bool WebCookieCache::isSupported()
{
#if HAVE(COOKIE_CHANGE_LISTENER_API)
    // FIXME: This can eventually be removed, this is merely to ensure a smooth transition to the new API.
    return inMemoryStorageSession().supportsCookieChangeListenerAPI();
#else
    return false;
#endif
}

String WebCookieCache::cookiesForDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, FrameIdentifier frameID, PageIdentifier pageID, IncludeSecureCookies includeSecureCookies)
{
    if (!m_hostsWithInMemoryStorage.contains<StringViewHashTranslator>(url.host())) {
        auto host = url.host().toString();
        bool subscribeToCookieChangeNotifications = true;
        auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::DomCookiesForHost(url, subscribeToCookieChangeNotifications), 0);
        if (!sendResult)
            return { };

        auto& [cookies] = sendResult.reply();
        pruneCacheIfNecessary();
        m_hostsWithInMemoryStorage.add(WTFMove(host));
        for (auto& cookie : cookies)
            inMemoryStorageSession().setCookie(cookie);
    }
    return inMemoryStorageSession().cookiesForDOM(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, ApplyTrackingPrevention::No, ShouldRelaxThirdPartyCookieBlocking::No).first;
}

void WebCookieCache::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, FrameIdentifier frameID, PageIdentifier pageID, const String& cookieString, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking)
{
    if (m_hostsWithInMemoryStorage.contains<StringViewHashTranslator>(url.host()))
        inMemoryStorageSession().setCookiesFromDOM(firstParty, sameSiteInfo, url, frameID, pageID, ApplyTrackingPrevention::No, cookieString, shouldRelaxThirdPartyCookieBlocking);
}

void WebCookieCache::cookiesAdded(const String& host, const Vector<Cookie>& cookies)
{
    if (!m_hostsWithInMemoryStorage.contains(host))
        return;

    for (auto& cookie : cookies)
        inMemoryStorageSession().setCookie(cookie);
}

void WebCookieCache::cookiesDeleted(const String& host, const Vector<WebCore::Cookie>& cookies)
{
    if (!m_hostsWithInMemoryStorage.contains(host))
        return;

    for (auto& cookie : cookies)
        inMemoryStorageSession().deleteCookie(cookie, [] { });
}

void WebCookieCache::allCookiesDeleted()
{
    clear();
}

void WebCookieCache::clear()
{
#if HAVE(COOKIE_CHANGE_LISTENER_API)
    if (!m_hostsWithInMemoryStorage.isEmpty())
        WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::UnsubscribeFromCookieChangeNotifications(m_hostsWithInMemoryStorage), 0);
#endif
    m_hostsWithInMemoryStorage.clear();
    m_inMemoryStorageSession = nullptr;
}

void WebCookieCache::clearForHost(const String& host)
{
    String removedHost = m_hostsWithInMemoryStorage.take(host);
    if (removedHost.isNull())
        return;

    inMemoryStorageSession().deleteCookiesForHostnames(Vector<String> { removedHost }, [] { });
#if HAVE(COOKIE_CHANGE_LISTENER_API)
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::UnsubscribeFromCookieChangeNotifications(HashSet<String> { removedHost }), 0);
#endif
}

void WebCookieCache::pruneCacheIfNecessary()
{
    // We may want to raise this limit if we start using the cache for third-party iframes.
    static const unsigned maxCachedHosts = 5;

    while (m_hostsWithInMemoryStorage.size() >= maxCachedHosts)
        clearForHost(*m_hostsWithInMemoryStorage.random());
}

#if !PLATFORM(COCOA)
NetworkStorageSession& WebCookieCache::inMemoryStorageSession()
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return *m_inMemoryStorageSession;
}
#endif

} // namespace WebKit
