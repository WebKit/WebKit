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

#include "config.h"
#include "NetworkSession.h"

#include "NetworkAdClickAttribution.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsStore.h"
#include <WebCore/AdClickAttribution.h>
#include <WebCore/NetworkStorageSession.h>

#if PLATFORM(COCOA)
#include "NetworkSessionCocoa.h"
#endif
#if USE(SOUP)
#include "NetworkSessionSoup.h"
#endif
#if USE(CURL)
#include "NetworkSessionCurl.h"
#endif

namespace WebKit {
using namespace WebCore;

Ref<NetworkSession> NetworkSession::create(NetworkProcess& networkProcess, NetworkSessionCreationParameters&& parameters)
{
#if PLATFORM(COCOA)
    return NetworkSessionCocoa::create(networkProcess, WTFMove(parameters));
#endif
#if USE(SOUP)
    return NetworkSessionSoup::create(networkProcess, WTFMove(parameters));
#endif
#if USE(CURL)
    return NetworkSessionCurl::create(networkProcess, WTFMove(parameters));
#endif
}

NetworkStorageSession& NetworkSession::networkStorageSession() const
{
    auto* storageSession = m_networkProcess->storageSession(m_sessionID);
    RELEASE_ASSERT(storageSession);
    return *storageSession;
}

NetworkSession::NetworkSession(NetworkProcess& networkProcess, PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    , m_networkProcess(networkProcess)
    , m_adClickAttribution(makeUniqueRef<NetworkAdClickAttribution>())
{
}

NetworkSession::~NetworkSession()
{
}

void NetworkSession::invalidateAndCancel()
{
    for (auto* task : m_dataTaskSet)
        task->invalidateAndCancel();
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void NetworkSession::setResourceLoadStatisticsEnabled(bool enable)
{
    if (!enable) {
        m_resourceLoadStatistics = nullptr;
        return;
    }

    if (m_resourceLoadStatistics)
        return;

    // FIXME(193728): Support ResourceLoadStatistics for ephemeral sessions, too.
    if (m_sessionID.isEphemeral())
        return;
    
    m_resourceLoadStatistics = WebResourceLoadStatisticsStore::create(*this, m_resourceLoadStatisticsDirectory);
}

void NetworkSession::notifyResourceLoadStatisticsProcessed()
{
    m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::NotifyResourceLoadStatisticsProcessed(), 0);
}

void NetworkSession::logDiagnosticMessageWithValue(const String& message, const String& description, unsigned value, unsigned significantFigures, WebCore::ShouldSample shouldSample)
{
    m_networkProcess->parentProcessConnection()->send(Messages::WebPageProxy::LogDiagnosticMessageWithValue(message, description, value, significantFigures, shouldSample), 0);
}

void NetworkSession::notifyPageStatisticsTelemetryFinished(unsigned totalPrevalentResources, unsigned totalPrevalentResourcesWithUserInteraction, unsigned top3SubframeUnderTopFrameOrigins)
{
    m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::NotifyResourceLoadStatisticsTelemetryFinished(totalPrevalentResources, totalPrevalentResourcesWithUserInteraction, top3SubframeUnderTopFrameOrigins), 0);
}

void NetworkSession::deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(OptionSet<WebsiteDataType> dataTypes, Vector<String>&& topPrivatelyControlledDomains, bool shouldNotifyPage, CompletionHandler<void(const HashSet<String>&)>&& completionHandler)
{
    m_networkProcess->deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(m_sessionID, dataTypes, WTFMove(topPrivatelyControlledDomains), shouldNotifyPage, WTFMove(completionHandler));
}

void NetworkSession::topPrivatelyControlledDomainsWithWebsiteData(OptionSet<WebsiteDataType> dataTypes, bool shouldNotifyPage, CompletionHandler<void(HashSet<String>&&)>&& completionHandler)
{
    m_networkProcess->topPrivatelyControlledDomainsWithWebsiteData(m_sessionID, dataTypes, shouldNotifyPage, WTFMove(completionHandler));
}
#endif

void NetworkSession::storeAdClickAttribution(WebCore::AdClickAttribution&& adClickAttribution)
{
    m_adClickAttribution->store(WTFMove(adClickAttribution));
}

void NetworkSession::dumpAdClickAttribution(CompletionHandler<void(String)>&& completionHandler)
{
    m_adClickAttribution->toString(WTFMove(completionHandler));
}

void NetworkSession::clearAdClickAttribution(CompletionHandler<void()>&& completionHandler)
{
    m_adClickAttribution->clear(WTFMove(completionHandler));
}

} // namespace WebKit
