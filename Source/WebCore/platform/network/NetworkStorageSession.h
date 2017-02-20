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

#pragma once

#include "CredentialStorage.h"
#include "SessionID.h"
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
#include "CFNetworkSPI.h"
#include <wtf/RetainPtr.h>
#endif

#if USE(SOUP)
#include <wtf/Function.h>
#include <wtf/glib/GRefPtr.h>
typedef struct _SoupCookieJar SoupCookieJar;
#endif

namespace WebCore {

class NetworkingContext;
class ResourceRequest;
class SoupNetworkSession;

class NetworkStorageSession {
    WTF_MAKE_NONCOPYABLE(NetworkStorageSession); WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT static NetworkStorageSession& defaultStorageSession();
    WEBCORE_EXPORT static NetworkStorageSession* storageSession(SessionID);
    WEBCORE_EXPORT static void ensurePrivateBrowsingSession(SessionID, const String& identifierBase = String());
    WEBCORE_EXPORT static void destroySession(SessionID);
    WEBCORE_EXPORT static void forEach(std::function<void(const WebCore::NetworkStorageSession&)>);

    WEBCORE_EXPORT static void switchToNewTestingSession();

    SessionID sessionID() const { return m_sessionID; }
    CredentialStorage& credentialStorage() { return m_credentialStorage; }

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    NetworkStorageSession(SessionID, RetainPtr<CFURLStorageSessionRef>);

    // May be null, in which case a Foundation default should be used.
    CFURLStorageSessionRef platformSession() { return m_platformSession.get(); }
    WEBCORE_EXPORT RetainPtr<CFHTTPCookieStorageRef> cookieStorage() const;
    WEBCORE_EXPORT static void setCookieStoragePartitioningEnabled(bool);
#elif USE(SOUP)
    NetworkStorageSession(SessionID, std::unique_ptr<SoupNetworkSession>&&);
    ~NetworkStorageSession();

    SoupNetworkSession* soupNetworkSession() const { return m_session.get(); };
    SoupNetworkSession& getOrCreateSoupNetworkSession() const;
    SoupCookieJar* cookieStorage() const;
    void setCookieStorage(SoupCookieJar*);
    void setCookieObserverHandler(Function<void ()>&&);
    void getCredentialFromPersistentStorage(const ProtectionSpace&, Function<void (Credential&&)> completionHandler);
    void saveCredentialToPersistentStorage(const ProtectionSpace&, const Credential&);
#else
    NetworkStorageSession(SessionID, NetworkingContext*);
    ~NetworkStorageSession();

    NetworkingContext* context() const;
#endif

private:
    static HashMap<SessionID, std::unique_ptr<NetworkStorageSession>>& globalSessionMap();
    SessionID m_sessionID;

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    RetainPtr<CFURLStorageSessionRef> m_platformSession;
#elif USE(SOUP)
    static void cookiesDidChange(NetworkStorageSession*);

    mutable std::unique_ptr<SoupNetworkSession> m_session;
    GRefPtr<SoupCookieJar> m_cookieStorage;
    Function<void ()> m_cookieObserverHandler;
#if USE(LIBSECRET)
    Function<void (Credential&&)> m_persisentStorageCompletionHandler;
    GRefPtr<GCancellable> m_persisentStorageCancellable;
#endif
#else
    RefPtr<NetworkingContext> m_context;
#endif

    CredentialStorage m_credentialStorage;
};

WEBCORE_EXPORT String cookieStoragePartition(const ResourceRequest&);
String cookieStoragePartition(const URL& firstPartyForCookies, const URL& resource);

}
