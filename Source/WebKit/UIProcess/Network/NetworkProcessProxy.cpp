/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#include "NetworkProcessProxy.h"

#include "APIContentRuleList.h"
#include "AuthenticationChallengeProxy.h"
#include "DownloadProxyMessages.h"
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManagerProxyMessages.h"
#endif
#include "Logging.h"
#include "NetworkContentRuleListManagerMessages.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "SandboxExtension.h"
#include "ShouldGrandfatherStatistics.h"
#include "StorageAccessStatus.h"
#include "WebCompiledContentRuleList.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebUserContentControllerProxy.h"
#include "WebsiteData.h"
#include "WebsiteDataStoreClient.h"
#include <WebCore/ClientOrigin.h>
#include <wtf/CompletionHandler.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShimProxy.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())

namespace WebKit {
using namespace WebCore;

static uint64_t generateCallbackID()
{
    static uint64_t callbackID;

    return ++callbackID;
}

NetworkProcessProxy::NetworkProcessProxy(WebProcessPool& processPool)
    : ChildProcessProxy(processPool.alwaysRunsAtBackgroundPriority())
    , m_processPool(processPool)
    , m_numPendingConnectionRequests(0)
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    , m_customProtocolManagerProxy(*this)
#endif
    , m_throttler(*this, processPool.shouldTakeUIBackgroundAssertion())
{
    connect();

    if (auto* websiteDataStore = m_processPool.websiteDataStore())
        m_websiteDataStores.set(websiteDataStore->websiteDataStore().sessionID(), makeRef(websiteDataStore->websiteDataStore()));
}

NetworkProcessProxy::~NetworkProcessProxy()
{
    ASSERT(m_pendingFetchWebsiteDataCallbacks.isEmpty());
    ASSERT(m_pendingDeleteWebsiteDataCallbacks.isEmpty());
    ASSERT(m_pendingDeleteWebsiteDataForOriginsCallbacks.isEmpty());
#if ENABLE(CONTENT_EXTENSIONS)
    for (auto* proxy : m_webUserContentControllerProxies)
        proxy->removeNetworkProcess(*this);
#endif

    for (auto& reply : m_pendingConnectionReplies)
        reply.second({ });
}

void NetworkProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::Network;
    ChildProcessProxy::getLaunchOptions(launchOptions);

    if (processPool().shouldMakeNextNetworkProcessLaunchFailForTesting()) {
        processPool().setShouldMakeNextNetworkProcessLaunchFailForTesting(false);
        launchOptions.shouldMakeProcessLaunchFailForTesting = true;
    }
}

void NetworkProcessProxy::connectionWillOpen(IPC::Connection& connection)
{
#if ENABLE(SEC_ITEM_SHIM)
    SecItemShimProxy::singleton().initializeConnection(connection);
#else
    UNUSED_PARAM(connection);
#endif
}

void NetworkProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    ASSERT_UNUSED(connection, this->connection() == &connection);
}

void NetworkProcessProxy::getNetworkProcessConnection(WebProcessProxy& webProcessProxy, Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply&& reply)
{
    m_pendingConnectionReplies.append(std::make_pair(makeWeakPtr(webProcessProxy), WTFMove(reply)));

    if (state() == State::Launching) {
        m_numPendingConnectionRequests++;
        return;
    }

    bool isServiceWorkerProcess = false;
    SecurityOriginData securityOrigin;
#if ENABLE(SERVICE_WORKER)
    if (is<ServiceWorkerProcessProxy>(webProcessProxy)) {
        isServiceWorkerProcess = true;
        securityOrigin = downcast<ServiceWorkerProcessProxy>(webProcessProxy).securityOrigin();
    }
#endif

    connection()->send(Messages::NetworkProcess::CreateNetworkConnectionToWebProcess(isServiceWorkerProcess, securityOrigin), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

DownloadProxy* NetworkProcessProxy::createDownloadProxy(const ResourceRequest& resourceRequest)
{
    if (!m_downloadProxyMap)
        m_downloadProxyMap = std::make_unique<DownloadProxyMap>(this);

    return m_downloadProxyMap->createDownloadProxy(m_processPool, resourceRequest);
}

void NetworkProcessProxy::fetchWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, CompletionHandler<void (WebsiteData)>&& completionHandler)
{
    ASSERT(canSendMessage());

    uint64_t callbackID = generateCallbackID();
    RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - NetworkProcessProxy is taking a background assertion because the Network process is fetching Website data", this);

    m_pendingFetchWebsiteDataCallbacks.add(callbackID, [this, token = throttler().backgroundActivityToken(), completionHandler = WTFMove(completionHandler), sessionID] (WebsiteData websiteData) mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
        UNUSED_PARAM(sessionID);
#endif
        completionHandler(WTFMove(websiteData));
        RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - NetworkProcessProxy is releasing a background assertion because the Network process is done fetching Website data", this);
    });

    send(Messages::NetworkProcess::FetchWebsiteData(sessionID, dataTypes, fetchOptions, callbackID), 0);
}

void NetworkProcessProxy::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, WallTime modifiedSince, CompletionHandler<void ()>&& completionHandler)
{
    auto callbackID = generateCallbackID();
    RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - NetworkProcessProxy is taking a background assertion because the Network process is deleting Website data", this);

    m_pendingDeleteWebsiteDataCallbacks.add(callbackID, [this, token = throttler().backgroundActivityToken(), completionHandler = WTFMove(completionHandler), sessionID] () mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
        UNUSED_PARAM(sessionID);
#endif
        completionHandler();
        RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - NetworkProcessProxy is releasing a background assertion because the Network process is done deleting Website data", this);
    });
    send(Messages::NetworkProcess::DeleteWebsiteData(sessionID, dataTypes, modifiedSince, callbackID), 0);
}

void NetworkProcessProxy::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, const Vector<WebCore::SecurityOriginData>& origins, const Vector<String>& cookieHostNames, const Vector<String>& HSTSCacheHostNames, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(canSendMessage());

    uint64_t callbackID = generateCallbackID();
    RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - NetworkProcessProxy is taking a background assertion because the Network process is deleting Website data for several origins", this);

    m_pendingDeleteWebsiteDataForOriginsCallbacks.add(callbackID, [this, token = throttler().backgroundActivityToken(), completionHandler = WTFMove(completionHandler), sessionID] () mutable {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
        UNUSED_PARAM(sessionID);
#endif
        completionHandler();
        RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - NetworkProcessProxy is releasing a background assertion because the Network process is done deleting Website data for several origins", this);
    });

    send(Messages::NetworkProcess::DeleteWebsiteDataForOrigins(sessionID, dataTypes, origins, cookieHostNames, HSTSCacheHostNames, callbackID), 0);
}

void NetworkProcessProxy::networkProcessCrashed()
{
    clearCallbackStates();

    Vector<std::pair<RefPtr<WebProcessProxy>, Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>> pendingReplies;
    pendingReplies.reserveInitialCapacity(m_pendingConnectionReplies.size());
    for (auto& reply : m_pendingConnectionReplies) {
        if (reply.first)
            pendingReplies.append(std::make_pair(makeRefPtr(reply.first.get()), WTFMove(reply.second)));
        else
            reply.second({ });
    }
    m_pendingConnectionReplies.clear();

    // Tell the network process manager to forget about this network process proxy. This will cause us to be deleted.
    m_processPool.networkProcessCrashed(*this, WTFMove(pendingReplies));
}

void NetworkProcessProxy::clearCallbackStates()
{
    while (!m_pendingFetchWebsiteDataCallbacks.isEmpty())
        m_pendingFetchWebsiteDataCallbacks.take(m_pendingFetchWebsiteDataCallbacks.begin()->key)(WebsiteData { });

    while (!m_pendingDeleteWebsiteDataCallbacks.isEmpty())
        m_pendingDeleteWebsiteDataCallbacks.take(m_pendingDeleteWebsiteDataCallbacks.begin()->key)();

    while (!m_pendingDeleteWebsiteDataForOriginsCallbacks.isEmpty())
        m_pendingDeleteWebsiteDataForOriginsCallbacks.take(m_pendingDeleteWebsiteDataForOriginsCallbacks.begin()->key)();
}

void NetworkProcessProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (dispatchMessage(connection, decoder))
        return;

    if (m_processPool.dispatchMessage(connection, decoder))
        return;

    didReceiveNetworkProcessProxyMessage(connection, decoder);
}

void NetworkProcessProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    if (dispatchSyncMessage(connection, decoder, replyEncoder))
        return;

    ASSERT_NOT_REACHED();
}

void NetworkProcessProxy::didClose(IPC::Connection&)
{
    auto protectedProcessPool = makeRef(m_processPool);

    if (m_downloadProxyMap)
        m_downloadProxyMap->processDidClose();
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    m_customProtocolManagerProxy.invalidate();
#endif

    m_tokenForHoldingLockedFiles = nullptr;
    
    m_syncAllCookiesToken = nullptr;
    m_syncAllCookiesCounter = 0;

    // This will cause us to be deleted.
    networkProcessCrashed();
}

void NetworkProcessProxy::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference)
{
}

void NetworkProcessProxy::didCreateNetworkConnectionToWebProcess(const IPC::Attachment& connectionIdentifier)
{
    ASSERT(!m_pendingConnectionReplies.isEmpty());

    // Grab the first pending connection reply.
    auto reply = m_pendingConnectionReplies.takeFirst().second;

#if USE(UNIX_DOMAIN_SOCKETS) || OS(WINDOWS)
    reply(connectionIdentifier);
#elif OS(DARWIN)
    MESSAGE_CHECK(MACH_PORT_VALID(connectionIdentifier.port()));
    reply(IPC::Attachment(connectionIdentifier.port(), MACH_MSG_TYPE_MOVE_SEND));
#else
    notImplemented();
#endif
}

void NetworkProcessProxy::didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, WebCore::AuthenticationChallenge&& coreChallenge, uint64_t challengeID)
{
#if ENABLE(SERVICE_WORKER)
    if (auto* serviceWorkerProcessProxy = m_processPool.serviceWorkerProcessProxyFromPageID(pageID)) {
        auto authenticationChallenge = AuthenticationChallengeProxy::create(WTFMove(coreChallenge), challengeID, makeRef(*connection()), nullptr);
        serviceWorkerProcessProxy->didReceiveAuthenticationChallenge(pageID, frameID, WTFMove(authenticationChallenge));
        return;
    }
#endif

    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    MESSAGE_CHECK(page);

    auto authenticationChallenge = AuthenticationChallengeProxy::create(WTFMove(coreChallenge), challengeID, makeRef(*connection()), page->secKeyProxyStore(coreChallenge));
    page->didReceiveAuthenticationChallengeProxy(frameID, WTFMove(authenticationChallenge));
}

void NetworkProcessProxy::didFetchWebsiteData(uint64_t callbackID, const WebsiteData& websiteData)
{
    auto callback = m_pendingFetchWebsiteDataCallbacks.take(callbackID);
    callback(websiteData);
}

void NetworkProcessProxy::didDeleteWebsiteData(uint64_t callbackID)
{
    auto callback = m_pendingDeleteWebsiteDataCallbacks.take(callbackID);
    callback();
}

void NetworkProcessProxy::didDeleteWebsiteDataForOrigins(uint64_t callbackID)
{
    auto callback = m_pendingDeleteWebsiteDataForOriginsCallbacks.take(callbackID);
    callback();
}

void NetworkProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    ChildProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (!IPC::Connection::identifierIsValid(connectionIdentifier)) {
        networkProcessCrashed();
        return;
    }

    for (unsigned i = 0; i < m_numPendingConnectionRequests; ++i)
        connection()->send(Messages::NetworkProcess::CreateNetworkConnectionToWebProcess(false, { }), 0);
    
    m_numPendingConnectionRequests = 0;

#if PLATFORM(COCOA)
    if (m_processPool.processSuppressionEnabled())
        setProcessSuppressionEnabled(true);
#endif
    
#if PLATFORM(IOS_FAMILY)
    if (xpc_connection_t connection = this->connection()->xpcConnection())
        m_throttler.didConnectToProcess(xpc_connection_get_pid(connection));
#endif
}

void NetworkProcessProxy::logDiagnosticMessage(uint64_t pageID, const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    // FIXME: We do this null-check because by the time the decision to log is made, the page may be gone. We should refactor to avoid this,
    // but for now we simply drop the message in the rare case this happens.
    if (!page)
        return;

    page->logDiagnosticMessage(message, description, shouldSample);
}

void NetworkProcessProxy::logDiagnosticMessageWithResult(uint64_t pageID, const String& message, const String& description, uint32_t result, WebCore::ShouldSample shouldSample)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    // FIXME: We do this null-check because by the time the decision to log is made, the page may be gone. We should refactor to avoid this,
    // but for now we simply drop the message in the rare case this happens.
    if (!page)
        return;

    page->logDiagnosticMessageWithResult(message, description, result, shouldSample);
}

void NetworkProcessProxy::logDiagnosticMessageWithValue(uint64_t pageID, const String& message, const String& description, double value, unsigned significantFigures, WebCore::ShouldSample shouldSample)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    // FIXME: We do this null-check because by the time the decision to log is made, the page may be gone. We should refactor to avoid this,
    // but for now we simply drop the message in the rare case this happens.
    if (!page)
        return;

    page->logDiagnosticMessageWithValue(message, description, value, significantFigures, shouldSample);
}

void NetworkProcessProxy::logGlobalDiagnosticMessageWithValue(const String& message, const String& description, double value, unsigned significantFigures, WebCore::ShouldSample shouldSample)
{
    if (auto* page = WebPageProxy::nonEphemeralWebPageProxy())
        page->logDiagnosticMessageWithValue(message, description, value, significantFigures, shouldSample);
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void NetworkProcessProxy::dumpResourceLoadStatistics(PAL::SessionID sessionID, CompletionHandler<void(String)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler({ });
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::DumpResourceLoadStatistics(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::updatePrevalentDomainsToBlockCookiesFor(PAL::SessionID sessionID, const Vector<String>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::UpdatePrevalentDomainsToBlockCookiesFor(sessionID, domainsToBlock), WTFMove(completionHandler));
}

void NetworkProcessProxy::isPrevalentResource(PAL::SessionID sessionID, const String& resourceDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::IsPrevalentResource(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::isVeryPrevalentResource(PAL::SessionID sessionID, const String& resourceDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::IsVeryPrevalentResource(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setPrevalentResource(PAL::SessionID sessionID, const String& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetPrevalentResource(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setPrevalentResourceForDebugMode(PAL::SessionID sessionID, const String& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetPrevalentResourceForDebugMode(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setVeryPrevalentResource(PAL::SessionID sessionID, const String& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetVeryPrevalentResource(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setLastSeen(PAL::SessionID sessionID, const String& resourceDomain, Seconds lastSeen, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetLastSeen(sessionID, resourceDomain, lastSeen), WTFMove(completionHandler));
}

void NetworkProcessProxy::clearPrevalentResource(PAL::SessionID sessionID, const String& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::ClearPrevalentResource(sessionID, resourceDomain), WTFMove(completionHandler));
}
    
void NetworkProcessProxy::scheduleCookieBlockingUpdate(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::ScheduleCookieBlockingUpdate(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::scheduleClearInMemoryAndPersistent(PAL::SessionID sessionID, Optional<WallTime> modifiedSince, ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::ScheduleClearInMemoryAndPersistent(sessionID, modifiedSince, shouldGrandfather), WTFMove(completionHandler));
}

void NetworkProcessProxy::scheduleStatisticsAndDataRecordsProcessing(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::ScheduleStatisticsAndDataRecordsProcessing(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::logUserInteraction(PAL::SessionID sessionID, const String& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::LogUserInteraction(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::hasHadUserInteraction(PAL::SessionID sessionID, const String& resourceDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::HadUserInteraction(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::clearUserInteraction(PAL::SessionID sessionID, const String& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::ClearUserInteraction(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setAgeCapForClientSideCookies(PAL::SessionID sessionID, Optional<Seconds> seconds, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetAgeCapForClientSideCookies(sessionID, seconds), WTFMove(completionHandler));
}

void NetworkProcessProxy::setTimeToLiveUserInteraction(PAL::SessionID sessionID, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetTimeToLiveUserInteraction(sessionID, seconds), WTFMove(completionHandler));
}

void NetworkProcessProxy::setNotifyPagesWhenTelemetryWasCaptured(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetNotifyPagesWhenTelemetryWasCaptured(sessionID, value), WTFMove(completionHandler));
}

void NetworkProcessProxy::setNotifyPagesWhenDataRecordsWereScanned(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetNotifyPagesWhenDataRecordsWereScanned(sessionID, value), WTFMove(completionHandler));
}

void NetworkProcessProxy::setSubframeUnderTopFrameOrigin(PAL::SessionID sessionID, const String& subframe, const String& topFrame, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetSubframeUnderTopFrameOrigin(sessionID, subframe, topFrame), WTFMove(completionHandler));
}

void NetworkProcessProxy::isRegisteredAsRedirectingTo(PAL::SessionID sessionID, const String& redirectedFrom, const String& redirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::IsRegisteredAsRedirectingTo(sessionID, redirectedFrom, redirectedTo), WTFMove(completionHandler));
}

void NetworkProcessProxy::isRegisteredAsSubFrameUnder(PAL::SessionID sessionID, const String& subFrame, const String& topFrame, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::IsRegisteredAsSubFrameUnder(sessionID, subFrame, topFrame), WTFMove(completionHandler));
}

void NetworkProcessProxy::setSubresourceUnderTopFrameOrigin(PAL::SessionID sessionID, const String& subresource, const String& topFrame, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetSubresourceUnderTopFrameOrigin(sessionID, subresource, topFrame), WTFMove(completionHandler));
}

void NetworkProcessProxy::isRegisteredAsSubresourceUnder(PAL::SessionID sessionID, const String& subresource, const String& topFrame, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::IsRegisteredAsSubresourceUnder(sessionID, subresource, topFrame), WTFMove(completionHandler));
}

void NetworkProcessProxy::setSubresourceUniqueRedirectTo(PAL::SessionID sessionID, const String& subresource, const String& hostNameRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetSubresourceUniqueRedirectTo(sessionID, subresource, hostNameRedirectedTo), WTFMove(completionHandler));
}

void NetworkProcessProxy::setSubresourceUniqueRedirectFrom(PAL::SessionID sessionID, const String& subresource, const String& hostNameRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetSubresourceUniqueRedirectFrom(sessionID, subresource, hostNameRedirectedFrom), WTFMove(completionHandler));
}

void NetworkProcessProxy::setTopFrameUniqueRedirectTo(PAL::SessionID sessionID, const String& topFrameHostName, const String& hostNameRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetTopFrameUniqueRedirectTo(sessionID, topFrameHostName, hostNameRedirectedTo), WTFMove(completionHandler));
}

void NetworkProcessProxy::setTopFrameUniqueRedirectFrom(PAL::SessionID sessionID, const String& topFrameHostName, const String& hostNameRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetTopFrameUniqueRedirectFrom(sessionID, topFrameHostName, hostNameRedirectedFrom), WTFMove(completionHandler));
}

void NetworkProcessProxy::isGrandfathered(PAL::SessionID sessionID, const String& resourceDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::IsGrandfathered(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setGrandfathered(PAL::SessionID sessionID, const String& resourceDomain, bool isGrandfathered, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetGrandfathered(sessionID, resourceDomain, isGrandfathered), WTFMove(completionHandler));
}

void NetworkProcessProxy::hasStorageAccessForFrame(PAL::SessionID sessionID, const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::HasStorageAccessForFrame(sessionID, resourceDomain, firstPartyDomain, frameID, pageID), WTFMove(completionHandler));
}

void NetworkProcessProxy::hasStorageAccess(PAL::SessionID sessionID, const String& resourceDomain, const String& firstPartyDomain, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::HasStorageAccess(sessionID, resourceDomain, firstPartyDomain, frameID, pageID), WTFMove(completionHandler));
}

void NetworkProcessProxy::requestStorageAccess(PAL::SessionID sessionID, const String& resourceDomain, const String& firstPartyDomain, Optional<uint64_t> frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::RequestStorageAccess(sessionID, resourceDomain, firstPartyDomain, frameID, pageID, promptEnabled), WTFMove(completionHandler));
}

void NetworkProcessProxy::requestStorageAccessConfirm(uint64_t pageID, uint64_t frameID, const String& subFrameHost, const String& topFrameHost, CompletionHandler<void(bool)>&& completionHandler)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    if (!page) {
        completionHandler(false);
        return;
    }
    
    page->requestStorageAccessConfirm(subFrameHost, topFrameHost, frameID, WTFMove(completionHandler));
}

void NetworkProcessProxy::grantStorageAccess(PAL::SessionID sessionID, const String& resourceDomain, const String& firstPartyDomain, Optional<uint64_t> frameID, uint64_t pageID, bool userWasPrompted, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::GrantStorageAccess(sessionID, resourceDomain, firstPartyDomain, frameID.value(), pageID, userWasPrompted), WTFMove(completionHandler));
}

void NetworkProcessProxy::removeAllStorageAccess(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::RemoveAllStorageAccess(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::getAllStorageAccessEntries(PAL::SessionID sessionID, CompletionHandler<void(Vector<String> domains)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler({ });
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::GetAllStorageAccessEntries(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::setCacheMaxAgeCapForPrevalentResources(PAL::SessionID sessionID, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetCacheMaxAgeCapForPrevalentResources(sessionID, seconds), WTFMove(completionHandler));
}

void NetworkProcessProxy::setCacheMaxAgeCap(PAL::SessionID sessionID, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetCacheMaxAgeCapForPrevalentResources(sessionID, seconds), WTFMove(completionHandler));
}

void NetworkProcessProxy::setGrandfatheringTime(PAL::SessionID sessionID, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetGrandfatheringTime(sessionID, seconds), WTFMove(completionHandler));
}

void NetworkProcessProxy::setMaxStatisticsEntries(PAL::SessionID sessionID, size_t maximumEntryCount, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetMaxStatisticsEntries(sessionID, maximumEntryCount), WTFMove(completionHandler));
}

void NetworkProcessProxy::setMinimumTimeBetweenDataRecordsRemoval(PAL::SessionID sessionID, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetMinimumTimeBetweenDataRecordsRemoval(sessionID, seconds), WTFMove(completionHandler));
}

void NetworkProcessProxy::setPruneEntriesDownTo(PAL::SessionID sessionID, size_t pruneTargetCount, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetPruneEntriesDownTo(sessionID, pruneTargetCount), WTFMove(completionHandler));
}

void NetworkProcessProxy::setShouldClassifyResourcesBeforeDataRecordsRemoval(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetShouldClassifyResourcesBeforeDataRecordsRemoval(sessionID, value), WTFMove(completionHandler));
}

void NetworkProcessProxy::setResourceLoadStatisticsDebugMode(PAL::SessionID sessionID, bool debugMode, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetResourceLoadStatisticsDebugMode(sessionID, debugMode), WTFMove(completionHandler));
}

void NetworkProcessProxy::resetCacheMaxAgeCapForPrevalentResources(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::ResetCacheMaxAgeCapForPrevalentResources(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::resetParametersToDefaultValues(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::ResetParametersToDefaultValues(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::submitTelemetry(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SubmitTelemetry(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::scheduleClearInMemoryAndPersistent(PAL::SessionID sessionID, ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::ScheduleClearInMemoryAndPersistent(sessionID, { }, shouldGrandfather), WTFMove(completionHandler));
}

void NetworkProcessProxy::logTestingEvent(PAL::SessionID sessionID, const String& event)
{
    if (auto* websiteDataStore = websiteDataStoreFromSessionID(sessionID))
        websiteDataStore->logTestingEvent(event);
}

void NetworkProcessProxy::notifyResourceLoadStatisticsProcessed()
{
    WebProcessProxy::notifyPageStatisticsAndDataRecordsProcessed();
}

void NetworkProcessProxy::notifyWebsiteDataDeletionForTopPrivatelyOwnedDomainsFinished()
{
    WebProcessProxy::notifyWebsiteDataDeletionForTopPrivatelyOwnedDomainsFinished();
}

void NetworkProcessProxy::notifyWebsiteDataScanForTopPrivatelyControlledDomainsFinished()
{
    WebProcessProxy::notifyWebsiteDataScanForTopPrivatelyControlledDomainsFinished();
}

void NetworkProcessProxy::notifyResourceLoadStatisticsTelemetryFinished(unsigned totalPrevalentResources, unsigned totalPrevalentResourcesWithUserInteraction, unsigned top3SubframeUnderTopFrameOrigins)
{
    API::Dictionary::MapType messageBody;
    messageBody.set("TotalPrevalentResources"_s, API::UInt64::create(totalPrevalentResources));
    messageBody.set("TotalPrevalentResourcesWithUserInteraction"_s, API::UInt64::create(totalPrevalentResourcesWithUserInteraction));
    messageBody.set("Top3SubframeUnderTopFrameOrigins"_s, API::UInt64::create(top3SubframeUnderTopFrameOrigins));

    WebProcessProxy::notifyPageStatisticsTelemetryFinished(API::Dictionary::create(messageBody).ptr());
}
#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

void NetworkProcessProxy::sendProcessWillSuspendImminently()
{
    if (!canSendMessage())
        return;

    bool handled = false;
    sendSync(Messages::NetworkProcess::ProcessWillSuspendImminently(), Messages::NetworkProcess::ProcessWillSuspendImminently::Reply(handled), 0, 1_s);
}
    
void NetworkProcessProxy::sendPrepareToSuspend()
{
    if (canSendMessage())
        send(Messages::NetworkProcess::PrepareToSuspend(), 0);
}

void NetworkProcessProxy::sendCancelPrepareToSuspend()
{
    if (canSendMessage())
        send(Messages::NetworkProcess::CancelPrepareToSuspend(), 0);
}

void NetworkProcessProxy::sendProcessDidResume()
{
    if (canSendMessage())
        send(Messages::NetworkProcess::ProcessDidResume(), 0);
}

void NetworkProcessProxy::writeBlobToFilePath(const URL& url, const String& path, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }

    SandboxExtension::Handle handleForWriting;
    SandboxExtension::createHandle(path, SandboxExtension::Type::ReadWrite, handleForWriting);
    sendWithAsyncReply(Messages::NetworkProcess::WriteBlobToFilePath(url, path, handleForWriting), WTFMove(completionHandler));
}

void NetworkProcessProxy::processReadyToSuspend()
{
    m_throttler.processReadyToSuspend();
}

void NetworkProcessProxy::didSetAssertionState(AssertionState)
{
}
    
void NetworkProcessProxy::setIsHoldingLockedFiles(bool isHoldingLockedFiles)
{
    if (!isHoldingLockedFiles) {
        RELEASE_LOG(ProcessSuspension, "UIProcess is releasing a background assertion because the Network process is no longer holding locked files");
        m_tokenForHoldingLockedFiles = nullptr;
        return;
    }
    if (!m_tokenForHoldingLockedFiles) {
        RELEASE_LOG(ProcessSuspension, "UIProcess is taking a background assertion because the Network process is holding locked files");
        m_tokenForHoldingLockedFiles = m_throttler.backgroundActivityToken();
    }
}

void NetworkProcessProxy::syncAllCookies()
{
    send(Messages::NetworkProcess::SyncAllCookies(), 0);
    
    ++m_syncAllCookiesCounter;
    if (m_syncAllCookiesToken)
        return;
    
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcessProxy is taking a background assertion because the Network process is syncing cookies", this);
    m_syncAllCookiesToken = throttler().backgroundActivityToken();
}
    
void NetworkProcessProxy::didSyncAllCookies()
{
    ASSERT(m_syncAllCookiesCounter);

    --m_syncAllCookiesCounter;
    if (!m_syncAllCookiesCounter) {
        RELEASE_LOG(ProcessSuspension, "%p - NetworkProcessProxy is releasing a background assertion because the Network process is done syncing cookies", this);
        m_syncAllCookiesToken = nullptr;
    }
}

void NetworkProcessProxy::addSession(Ref<WebsiteDataStore>&& store)
{
    if (canSendMessage())
        send(Messages::NetworkProcess::AddWebsiteDataStore { store->parameters() }, 0);
    auto sessionID = store->sessionID();
    if (!sessionID.isEphemeral())
        m_websiteDataStores.set(sessionID, WTFMove(store));
}

void NetworkProcessProxy::removeSession(PAL::SessionID sessionID)
{
    if (canSendMessage())
        send(Messages::NetworkProcess::DestroySession { sessionID }, 0);
    if (!sessionID.isEphemeral())
        m_websiteDataStores.remove(sessionID);
}

WebsiteDataStore* NetworkProcessProxy::websiteDataStoreFromSessionID(PAL::SessionID sessionID)
{
    auto iterator = m_websiteDataStores.find(sessionID);
    if (iterator != m_websiteDataStores.end())
        return iterator->value.get();

    if (auto* websiteDataStore = m_processPool.websiteDataStore()) {
        if (sessionID == websiteDataStore->websiteDataStore().sessionID())
            return &websiteDataStore->websiteDataStore();
    }

    if (sessionID != PAL::SessionID::defaultSessionID())
        return nullptr;

    return &API::WebsiteDataStore::defaultDataStore()->websiteDataStore();
}

void NetworkProcessProxy::retrieveCacheStorageParameters(PAL::SessionID sessionID)
{
    auto* store = websiteDataStoreFromSessionID(sessionID);

    if (!store) {
        RELEASE_LOG_ERROR(CacheStorage, "%p - NetworkProcessProxy is unable to retrieve CacheStorage parameters from the given session ID %" PRIu64, this, sessionID.sessionID());
        auto quota = m_processPool.websiteDataStore() ? m_processPool.websiteDataStore()->websiteDataStore().cacheStoragePerOriginQuota() : WebsiteDataStoreConfiguration::defaultCacheStoragePerOriginQuota;
        send(Messages::NetworkProcess::SetCacheStorageParameters { sessionID, quota, { }, { } }, 0);
        return;
    }

    auto& cacheStorageDirectory = store->cacheStorageDirectory();
    SandboxExtension::Handle cacheStorageDirectoryExtensionHandle;
    if (!cacheStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(cacheStorageDirectory, cacheStorageDirectoryExtensionHandle);

    send(Messages::NetworkProcess::SetCacheStorageParameters { sessionID, store->cacheStoragePerOriginQuota(), cacheStorageDirectory, cacheStorageDirectoryExtensionHandle }, 0);
}

#if ENABLE(CONTENT_EXTENSIONS)
void NetworkProcessProxy::contentExtensionRules(UserContentControllerIdentifier identifier)
{
    if (auto* webUserContentControllerProxy = WebUserContentControllerProxy::get(identifier)) {
        m_webUserContentControllerProxies.add(webUserContentControllerProxy);
        webUserContentControllerProxy->addNetworkProcess(*this);

        auto rules = WTF::map(webUserContentControllerProxy->contentExtensionRules(), [](auto&& keyValue) -> std::pair<String, WebCompiledContentRuleListData> {
            return std::make_pair(keyValue.value->name(), keyValue.value->compiledRuleList().data());
        });
        send(Messages::NetworkContentRuleListManager::AddContentRuleLists { identifier, rules }, 0);
        return;
    }
    send(Messages::NetworkContentRuleListManager::AddContentRuleLists { identifier, { } }, 0);
}

void NetworkProcessProxy::didDestroyWebUserContentControllerProxy(WebUserContentControllerProxy& proxy)
{
    send(Messages::NetworkContentRuleListManager::Remove { proxy.identifier() }, 0);
    m_webUserContentControllerProxies.remove(&proxy);
}
#endif
    
void NetworkProcessProxy::sendProcessDidTransitionToForeground()
{
    send(Messages::NetworkProcess::ProcessDidTransitionToForeground(), 0);
}

void NetworkProcessProxy::sendProcessDidTransitionToBackground()
{
    send(Messages::NetworkProcess::ProcessDidTransitionToBackground(), 0);
}

#if ENABLE(SANDBOX_EXTENSIONS)
void NetworkProcessProxy::getSandboxExtensionsForBlobFiles(const Vector<String>& paths, Messages::NetworkProcessProxy::GetSandboxExtensionsForBlobFiles::AsyncReply&& reply)
{
    SandboxExtension::HandleArray extensions;
    extensions.allocate(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) {
        // ReadWrite is required for creating hard links, which is something that might be done with these extensions.
        SandboxExtension::createHandle(paths[i], SandboxExtension::Type::ReadWrite, extensions[i]);
    }
    reply(WTFMove(extensions));
}
#endif

#if ENABLE(SERVICE_WORKER)
void NetworkProcessProxy::establishWorkerContextConnectionToNetworkProcess(SecurityOriginData&& origin)
{
    m_processPool.establishWorkerContextConnectionToNetworkProcess(*this, WTFMove(origin), WTF::nullopt);
}

void NetworkProcessProxy::establishWorkerContextConnectionToNetworkProcessForExplicitSession(SecurityOriginData&& origin, PAL::SessionID sessionID)
{
    m_processPool.establishWorkerContextConnectionToNetworkProcess(*this, WTFMove(origin), sessionID);
}
#endif

void NetworkProcessProxy::requestCacheStorageSpace(PAL::SessionID sessionID, const WebCore::ClientOrigin& origin, uint64_t quota, uint64_t currentSize, uint64_t spaceRequired, CompletionHandler<void(Optional<uint64_t> quota)>&& completionHandler)
{
    auto* store = websiteDataStoreFromSessionID(sessionID);

    if (!store) {
        completionHandler({ });
        return;
    }

    store->client().requestCacheStorageSpace(origin.topOrigin, origin.clientOrigin, quota, currentSize, spaceRequired, WTFMove(completionHandler));
}

} // namespace WebKit

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_URL
