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

#pragma once

#include "WebPageProxyIdentifier.h"
#include <WebCore/CookieChangeListener.h>
#include <WebCore/CookieJar.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/SameSiteInfo.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefCounter.h>

namespace WebCore {
struct Cookie;
enum class ShouldRelaxThirdPartyCookieBlocking : bool;
}

namespace WebKit {

enum PendingCookieUpdateCounterType { };
using PendingCookieUpdateCounter = RefCounter<PendingCookieUpdateCounterType>;

class WebCookieCache final : public RefCounted<WebCookieCache>, public WebCore::CookieChangeListener {
public:
    static Ref<WebCookieCache> create()
    {
        return adoptRef(*new WebCookieCache);
    }

    virtual ~WebCookieCache();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    bool isSupported();

    String cookiesForDOM(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebPageProxyIdentifier, WebCore::IncludeSecureCookies);
    void setCookiesFromDOM(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, WebCore::FrameIdentifier, WebCore::PageIdentifier, const String& cookieString, WebCore::ShouldRelaxThirdPartyCookieBlocking);

    PendingCookieUpdateCounter::Token WARN_UNUSED_RETURN willSetCookieFromDOM();
    void didSetCookieFromDOM(PendingCookieUpdateCounter::Token, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, WebCore::FrameIdentifier, WebCore::PageIdentifier, const WebCore::Cookie&, WebCore::ShouldRelaxThirdPartyCookieBlocking);

    void allCookiesDeleted();

    void clear();
    void clearForHost(const String&);

    void setOptInCookiePartitioningEnabled(bool);

private:
    WebCookieCache() = default;

    WebCore::NetworkStorageSession& inMemoryStorageSession();
    CheckedRef<WebCore::NetworkStorageSession> checkedInMemoryStorageSession() { return inMemoryStorageSession(); }

    void pruneCacheIfNecessary();
    bool cacheMayBeOutOfSync() const;

    // CookieChangeListener
    void cookiesAdded(const String& host, const Vector<WebCore::Cookie>&) final;
    void cookiesDeleted(const String& host, const Vector<WebCore::Cookie>&) final;

    HashSet<String> m_hostsWithInMemoryStorage;
    std::unique_ptr<WebCore::NetworkStorageSession> m_inMemoryStorageSession;
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    bool m_optInCookiePartitioningEnabled { false };
#endif

    PendingCookieUpdateCounter m_pendingCookieUpdateCounter;
};

} // namespace WebKit
