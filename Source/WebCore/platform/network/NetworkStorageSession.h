/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
#include <pal/spi/cf/CFNetworkSPI.h>
#include <wtf/RetainPtr.h>
#endif

#if USE(SOUP)
#include <wtf/Function.h>
#include <wtf/glib/GRefPtr.h>
typedef struct _SoupCookieJar SoupCookieJar;
#endif

#ifdef __OBJC__
#include <objc/objc.h>
#endif

#if PLATFORM(COCOA)
#include "CookieStorageObserver.h"
#endif

namespace WebCore {

class NetworkingContext;
class ResourceRequest;
class SoupNetworkSession;

struct Cookie;

class NetworkStorageSession {
    WTF_MAKE_NONCOPYABLE(NetworkStorageSession); WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT static NetworkStorageSession& defaultStorageSession();
    WEBCORE_EXPORT static NetworkStorageSession* storageSession(SessionID);
    WEBCORE_EXPORT static void ensurePrivateBrowsingSession(SessionID, const String& identifierBase = String());
    WEBCORE_EXPORT static void ensureSession(SessionID, const String& identifierBase = String());
    WEBCORE_EXPORT static void destroySession(SessionID);
    WEBCORE_EXPORT static void forEach(const WTF::Function<void(const WebCore::NetworkStorageSession&)>&);

    WEBCORE_EXPORT static void switchToNewTestingSession();

    SessionID sessionID() const { return m_sessionID; }
    CredentialStorage& credentialStorage() { return m_credentialStorage; }

#ifdef __OBJC__
    NSHTTPCookieStorage *nsCookieStorage() const;
#endif

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    WEBCORE_EXPORT static void ensureSession(SessionID, const String& identifierBase, RetainPtr<CFHTTPCookieStorageRef>&&);
    NetworkStorageSession(SessionID, RetainPtr<CFURLStorageSessionRef>&&, RetainPtr<CFHTTPCookieStorageRef>&&);

    // May be null, in which case a Foundation default should be used.
    CFURLStorageSessionRef platformSession() { return m_platformSession.get(); }
    WEBCORE_EXPORT RetainPtr<CFHTTPCookieStorageRef> cookieStorage() const;
    WEBCORE_EXPORT static void setCookieStoragePartitioningEnabled(bool);
#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
    WEBCORE_EXPORT String cookieStoragePartition(const ResourceRequest&) const;
    String cookieStoragePartition(const URL& firstPartyForCookies, const URL& resource) const;
    WEBCORE_EXPORT void setShouldPartitionCookiesForHosts(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst);
#endif
#elif USE(SOUP)
    NetworkStorageSession(SessionID, std::unique_ptr<SoupNetworkSession>&&);
    ~NetworkStorageSession();

    SoupNetworkSession* soupNetworkSession() const { return m_session.get(); };
    SoupNetworkSession& getOrCreateSoupNetworkSession() const;
    void clearSoupNetworkSessionAndCookieStorage();
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

    WEBCORE_EXPORT void setCookie(const Cookie&);
    WEBCORE_EXPORT void setCookies(const Vector<Cookie>&, const URL&, const URL& mainDocumentURL);
    WEBCORE_EXPORT void deleteCookie(const Cookie&);
    WEBCORE_EXPORT Vector<Cookie> getAllCookies();
    WEBCORE_EXPORT Vector<Cookie> getCookies(const URL&);
    WEBCORE_EXPORT void flushCookieStore();

private:
    static HashMap<SessionID, std::unique_ptr<NetworkStorageSession>>& globalSessionMap();
    SessionID m_sessionID;

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    RetainPtr<CFURLStorageSessionRef> m_platformSession;
    RetainPtr<CFHTTPCookieStorageRef> m_platformCookieStorage;
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

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
    bool shouldPartitionCookies(const String& topPrivatelyControlledDomain) const;
    HashSet<String> m_topPrivatelyControlledDomainsForCookiePartitioning;
#endif

#if PLATFORM(COCOA)
public:
    CookieStorageObserver& cookieStorageObserver() const;

private:
    mutable RefPtr<CookieStorageObserver> m_cookieStorageObserver;
#endif
};

}
