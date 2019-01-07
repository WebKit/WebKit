/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

OBJC_CLASS NSData;
OBJC_CLASS NSURLSession;
OBJC_CLASS NSURLSessionDownloadTask;
OBJC_CLASS NSOperationQueue;
OBJC_CLASS WKNetworkSessionDelegate;

#include "DownloadID.h"
#include "NetworkDataTaskCocoa.h"
#include "NetworkSession.h"
#include <WebCore/NetworkLoadMetrics.h>
#include <wtf/HashMap.h>

namespace WebKit {

class LegacyCustomProtocolManager;

class NetworkSessionCocoa final : public NetworkSession {
    friend class NetworkDataTaskCocoa;
public:
    static Ref<NetworkSession> create(NetworkProcess&, NetworkSessionCreationParameters&&);
    ~NetworkSessionCocoa();

    // Must be called before any NetworkSession has been created.
    // FIXME: Move this to NetworkSessionCreationParameters.
#if PLATFORM(IOS_FAMILY)
    static void setCTDataConnectionServiceType(const String&);
#endif

    NetworkDataTaskCocoa* dataTaskForIdentifier(NetworkDataTaskCocoa::TaskIdentifier, WebCore::StoredCredentialsPolicy);
    NSURLSessionDownloadTask* downloadTaskWithResumeData(NSData*);

    void addDownloadID(NetworkDataTaskCocoa::TaskIdentifier, DownloadID);
    DownloadID downloadID(NetworkDataTaskCocoa::TaskIdentifier);
    DownloadID takeDownloadID(NetworkDataTaskCocoa::TaskIdentifier);

    static bool allowsSpecificHTTPSCertificateForHost(const WebCore::AuthenticationChallenge&);

private:
    NetworkSessionCocoa(NetworkProcess&, NetworkSessionCreationParameters&&);

    void invalidateAndCancel() override;
    void clearCredentials() override;
    bool shouldLogCookieInformation() const override { return m_shouldLogCookieInformation; }
    Seconds loadThrottleLatency() const override { return m_loadThrottleLatency; }

    HashMap<NetworkDataTaskCocoa::TaskIdentifier, NetworkDataTaskCocoa*> m_dataTaskMapWithCredentials;
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, NetworkDataTaskCocoa*> m_dataTaskMapWithoutState;
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, DownloadID> m_downloadMap;

    RetainPtr<NSURLSession> m_sessionWithCredentialStorage;
    RetainPtr<WKNetworkSessionDelegate> m_sessionWithCredentialStorageDelegate;
    RetainPtr<NSURLSession> m_statelessSession;
    RetainPtr<WKNetworkSessionDelegate> m_statelessSessionDelegate;

    String m_boundInterfaceIdentifier;
    RetainPtr<CFDictionaryRef> m_proxyConfiguration;
    bool m_shouldLogCookieInformation { false };
    Seconds m_loadThrottleLatency;
};

} // namespace WebKit
