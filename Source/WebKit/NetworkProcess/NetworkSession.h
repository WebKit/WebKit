/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#include <pal/SessionID.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Seconds.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class AdClickAttribution;
class NetworkStorageSession;
enum class ShouldSample : bool;
}

namespace WebKit {

class NetworkAdClickAttribution;
class NetworkDataTask;
class NetworkProcess;
class WebResourceLoadStatisticsStore;
struct NetworkSessionCreationParameters;

enum class WebsiteDataType;
    
class NetworkSession : public RefCounted<NetworkSession>, public CanMakeWeakPtr<NetworkSession> {
public:
    static Ref<NetworkSession> create(NetworkProcess&, NetworkSessionCreationParameters&&);
    virtual ~NetworkSession();

    virtual void invalidateAndCancel();
    virtual void clearCredentials() { };
    virtual bool shouldLogCookieInformation() const { return false; }
    virtual Seconds loadThrottleLatency() const { return { }; }

    PAL::SessionID sessionID() const { return m_sessionID; }
    NetworkProcess& networkProcess() { return m_networkProcess; }
    WebCore::NetworkStorageSession& networkStorageSession() const;

    void registerNetworkDataTask(NetworkDataTask& task) { m_dataTaskSet.add(&task); }
    void unregisterNetworkDataTask(NetworkDataTask& task) { m_dataTaskSet.remove(&task); }

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebResourceLoadStatisticsStore* resourceLoadStatistics() const { return m_resourceLoadStatistics.get(); }
    void setResourceLoadStatisticsEnabled(bool);
    void notifyResourceLoadStatisticsProcessed();
    void deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(OptionSet<WebsiteDataType>, Vector<String>&& topPrivatelyControlledDomains, bool shouldNotifyPage, CompletionHandler<void(const HashSet<String>&)>&&);
    void topPrivatelyControlledDomainsWithWebsiteData(OptionSet<WebsiteDataType>, bool shouldNotifyPage, CompletionHandler<void(HashSet<String>&&)>&&);
    void logDiagnosticMessageWithValue(const String& message, const String& description, unsigned value, unsigned significantFigures, WebCore::ShouldSample);
    void notifyPageStatisticsTelemetryFinished(unsigned totalPrevalentResources, unsigned totalPrevalentResourcesWithUserInteraction, unsigned top3SubframeUnderTopFrameOrigins);
#endif
    void storeAdClickAttribution(WebCore::AdClickAttribution&&);
    void dumpAdClickAttribution(CompletionHandler<void(String)>&&);
    void clearAdClickAttribution(CompletionHandler<void()>&&);

protected:
    NetworkSession(NetworkProcess&, PAL::SessionID);

    PAL::SessionID m_sessionID;
    Ref<NetworkProcess> m_networkProcess;
    HashSet<NetworkDataTask*> m_dataTaskSet;
    String m_resourceLoadStatisticsDirectory;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    RefPtr<WebResourceLoadStatisticsStore> m_resourceLoadStatistics;
#endif
    UniqueRef<NetworkAdClickAttribution> m_adClickAttribution;
};

} // namespace WebKit
