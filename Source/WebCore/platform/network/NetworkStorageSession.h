/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef NetworkStorageSession_h
#define NetworkStorageSession_h

#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA) || USE(CFNETWORK)
typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
typedef struct OpaqueCFHTTPCookieStorage*  CFHTTPCookieStorageRef;
#endif

namespace WebCore {

class NetworkingContext;
class SoupNetworkSession;

class NetworkStorageSession {
    WTF_MAKE_NONCOPYABLE(NetworkStorageSession); WTF_MAKE_FAST_ALLOCATED;
public:
    static NetworkStorageSession& defaultStorageSession();
    static std::unique_ptr<NetworkStorageSession> createPrivateBrowsingSession(const String& identifierBase = String());

    static void switchToNewTestingSession();

#if PLATFORM(COCOA) || USE(CFNETWORK) || USE(SOUP)
    bool isPrivateBrowsingSession() const { return m_isPrivate; }
#endif

#if PLATFORM(COCOA) || USE(CFNETWORK)
    NetworkStorageSession(RetainPtr<CFURLStorageSessionRef>);
    // May be null, in which case a Foundation default should be used.
    CFURLStorageSessionRef platformSession() { return m_platformSession.get(); }
    RetainPtr<CFHTTPCookieStorageRef> cookieStorage() const;
#elif USE(SOUP)
    NetworkStorageSession(std::unique_ptr<SoupNetworkSession>);
    ~NetworkStorageSession();
    SoupNetworkSession& soupNetworkSession() const;
    void setSoupNetworkSession(std::unique_ptr<SoupNetworkSession>);
#else
    NetworkStorageSession(NetworkingContext*);
    ~NetworkStorageSession();
    NetworkingContext* context() const;
#endif

private:
#if PLATFORM(COCOA) || USE(CFNETWORK)
    NetworkStorageSession();
    RetainPtr<CFURLStorageSessionRef> m_platformSession;
#elif USE(SOUP)
    std::unique_ptr<SoupNetworkSession> m_session;
#else
    RefPtr<NetworkingContext> m_context;
#endif

#if PLATFORM(COCOA) || USE(CFNETWORK) || USE(SOUP)
    bool m_isPrivate;
#endif
};

#if PLATFORM(WIN) && USE(CFNETWORK)
// Needed for WebKit1 API only.
void overrideCookieStorage(CFHTTPCookieStorageRef);
CFHTTPCookieStorageRef overridenCookieStorage();
#endif

}

#endif // NetworkStorageSession_h
