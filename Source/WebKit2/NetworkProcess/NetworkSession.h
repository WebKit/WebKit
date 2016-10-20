/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if USE(NETWORK_SESSION)

#if PLATFORM(COCOA)
OBJC_CLASS NSURLSession;
OBJC_CLASS NSOperationQueue;
OBJC_CLASS WKNetworkSessionDelegate;
#endif

#include "DownloadID.h"
#include "NetworkDataTask.h"
#include <WebCore/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

#if USE(SOUP)
typedef struct _SoupSession SoupSession;
#endif

namespace WebCore {
class NetworkStorageSession;
}

namespace WebKit {

class CustomProtocolManager;

class NetworkSession : public RefCounted<NetworkSession> {
    friend class NetworkDataTask;
public:
    enum class Type {
        Normal,
        Ephemeral
    };

    static Ref<NetworkSession> create(Type, WebCore::SessionID, CustomProtocolManager*);
    static NetworkSession& defaultSession();
    ~NetworkSession();

    void invalidateAndCancel();

    WebCore::SessionID sessionID() const { return m_sessionID; }

#if USE(SOUP)
    SoupSession* soupSession() const;
#endif

#if PLATFORM(COCOA)
    // Must be called before any NetworkSession has been created.
    static void setCustomProtocolManager(CustomProtocolManager*);
    static void setSourceApplicationAuditTokenData(RetainPtr<CFDataRef>&&);
    static void setSourceApplicationBundleIdentifier(const String&);
    static void setSourceApplicationSecondaryIdentifier(const String&);
#if PLATFORM(IOS)
    static void setCTDataConnectionServiceType(const String&);
#endif
#endif

    void clearCredentials();
#if PLATFORM(COCOA)
    NetworkDataTask* dataTaskForIdentifier(NetworkDataTask::TaskIdentifier, WebCore::StoredCredentials);

    void addDownloadID(NetworkDataTask::TaskIdentifier, DownloadID);
    DownloadID downloadID(NetworkDataTask::TaskIdentifier);
    DownloadID takeDownloadID(NetworkDataTask::TaskIdentifier);
#endif

private:
    NetworkSession(Type, WebCore::SessionID, CustomProtocolManager*);
    WebCore::NetworkStorageSession& networkStorageSession() const;

    WebCore::SessionID m_sessionID;

#if PLATFORM(COCOA)
    HashMap<NetworkDataTask::TaskIdentifier, NetworkDataTask*> m_dataTaskMapWithCredentials;
    HashMap<NetworkDataTask::TaskIdentifier, NetworkDataTask*> m_dataTaskMapWithoutCredentials;
    HashMap<NetworkDataTask::TaskIdentifier, DownloadID> m_downloadMap;

    RetainPtr<NSURLSession> m_sessionWithCredentialStorage;
    RetainPtr<WKNetworkSessionDelegate> m_sessionWithCredentialStorageDelegate;
    RetainPtr<NSURLSession> m_sessionWithoutCredentialStorage;
    RetainPtr<WKNetworkSessionDelegate> m_sessionWithoutCredentialStorageDelegate;
#elif USE(SOUP)
    HashSet<NetworkDataTask*> m_dataTaskSet;
#endif
};

} // namespace WebKit

#endif // USE(NETWORK_SESSION)
