/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#include "WebCompiledContentRuleList.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebUserContentControllerProxy.h"
#include "WebsiteData.h"
#include <wtf/CompletionHandler.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShimProxy.h"
#endif

#if PLATFORM(IOS)
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
    m_pendingConnectionReplies.append(std::make_pair(&webProcessProxy, WTFMove(reply)));

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
    for (auto& reply : m_pendingConnectionReplies)
        pendingReplies.append(WTFMove(reply));

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

    while (!m_updateBlockCookiesCallbackMap.isEmpty())
        m_updateBlockCookiesCallbackMap.take(m_updateBlockCookiesCallbackMap.begin()->key)();
    
    while (!m_storageAccessResponseCallbackMap.isEmpty())
        m_storageAccessResponseCallbackMap.take(m_storageAccessResponseCallbackMap.begin()->key)(false);
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

    for (auto& callback : m_writeBlobToFilePathCallbackMap.values())
        callback(false);
    m_writeBlobToFilePathCallbackMap.clear();

    for (auto& callback : m_removeAllStorageAccessCallbackMap.values())
        callback();
    m_removeAllStorageAccessCallbackMap.clear();

    for (auto& callback : m_updateBlockCookiesCallbackMap.values())
        callback();
    m_updateBlockCookiesCallbackMap.clear();
    
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
    
#if PLATFORM(IOS)
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

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void NetworkProcessProxy::updatePrevalentDomainsToBlockCookiesFor(PAL::SessionID sessionID, const Vector<String>& domainsToBlock, ShouldClearFirst shouldClearFirst, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    auto callbackId = generateCallbackID();
    auto addResult = m_updateBlockCookiesCallbackMap.add(callbackId, [protectedProcessPool = makeRef(m_processPool), token = throttler().backgroundActivityToken(), completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    send(Messages::NetworkProcess::UpdatePrevalentDomainsToBlockCookiesFor(sessionID, domainsToBlock, shouldClearFirst == ShouldClearFirst::Yes, callbackId), 0);
}

void NetworkProcessProxy::didUpdateBlockCookies(uint64_t callbackId)
{
    m_updateBlockCookiesCallbackMap.take(callbackId)();
}

void NetworkProcessProxy::hasStorageAccessForFrame(PAL::SessionID sessionID, const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, WTF::CompletionHandler<void(bool)>&& callback)
{
    auto contextId = generateCallbackID();
    auto addResult = m_storageAccessResponseCallbackMap.add(contextId, WTFMove(callback));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    send(Messages::NetworkProcess::HasStorageAccessForFrame(sessionID, resourceDomain, firstPartyDomain, frameID, pageID, contextId), 0);
}

void NetworkProcessProxy::grantStorageAccess(PAL::SessionID sessionID, const String& resourceDomain, const String& firstPartyDomain, std::optional<uint64_t> frameID, uint64_t pageID, WTF::CompletionHandler<void(bool)>&& callback)
{
    auto contextId = generateCallbackID();
    auto addResult = m_storageAccessResponseCallbackMap.add(contextId, WTFMove(callback));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    send(Messages::NetworkProcess::GrantStorageAccess(sessionID, resourceDomain, firstPartyDomain, frameID, pageID, contextId), 0);
}

void NetworkProcessProxy::storageAccessRequestResult(bool wasGranted, uint64_t contextId)
{
    auto callback = m_storageAccessResponseCallbackMap.take(contextId);
    callback(wasGranted);
}

void NetworkProcessProxy::removeAllStorageAccess(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    auto contextId = generateCallbackID();
    auto addResult = m_removeAllStorageAccessCallbackMap.add(contextId, WTFMove(completionHandler));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    send(Messages::NetworkProcess::RemoveAllStorageAccess(sessionID, contextId), 0);
}

void NetworkProcessProxy::didRemoveAllStorageAccess(uint64_t contextId)
{
    auto completionHandler = m_removeAllStorageAccessCallbackMap.take(contextId);
    completionHandler();
}

void NetworkProcessProxy::getAllStorageAccessEntries(PAL::SessionID sessionID, CompletionHandler<void(Vector<String>&& domains)>&& callback)
{
    auto contextId = generateCallbackID();
    auto addResult = m_allStorageAccessEntriesCallbackMap.add(contextId, WTFMove(callback));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    send(Messages::NetworkProcess::GetAllStorageAccessEntries(sessionID, contextId), 0);
}

void NetworkProcessProxy::allStorageAccessEntriesResult(Vector<String>&& domains, uint64_t contextId)
{
    auto callback = m_allStorageAccessEntriesCallbackMap.take(contextId);
    callback(WTFMove(domains));
}

void NetworkProcessProxy::setCacheMaxAgeCapForPrevalentResources(PAL::SessionID sessionID, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    auto contextId = generateCallbackID();
    auto addResult = m_updateRuntimeSettingsCallbackMap.add(contextId, WTFMove(completionHandler));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    send(Messages::NetworkProcess::SetCacheMaxAgeCapForPrevalentResources(sessionID, seconds, contextId), 0);
}

void NetworkProcessProxy::didSetCacheMaxAgeCapForPrevalentResources(uint64_t contextId)
{
    auto completionHandler = m_updateRuntimeSettingsCallbackMap.take(contextId);
    completionHandler();
}

void NetworkProcessProxy::resetCacheMaxAgeCapForPrevalentResources(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    auto contextId = generateCallbackID();
    auto addResult = m_updateRuntimeSettingsCallbackMap.add(contextId, WTFMove(completionHandler));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    send(Messages::NetworkProcess::ResetCacheMaxAgeCapForPrevalentResources(sessionID, contextId), 0);
}

void NetworkProcessProxy::didResetCacheMaxAgeCapForPrevalentResources(uint64_t contextId)
{
    auto completionHandler = m_updateRuntimeSettingsCallbackMap.take(contextId);
    completionHandler();
}
#endif

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

void NetworkProcessProxy::writeBlobToFilePath(const WebCore::URL& url, const String& path, CompletionHandler<void(bool)>&& callback)
{
    if (!canSendMessage()) {
        callback(false);
        return;
    }

    static uint64_t writeBlobToFilePathCallbackIdentifiers = 0;
    uint64_t callbackID = ++writeBlobToFilePathCallbackIdentifiers;
    m_writeBlobToFilePathCallbackMap.add(callbackID, WTFMove(callback));

    SandboxExtension::Handle handleForWriting;
    SandboxExtension::createHandle(path, SandboxExtension::Type::ReadWrite, handleForWriting);
    send(Messages::NetworkProcess::WriteBlobToFilePath(url, path, handleForWriting, callbackID), 0);
}

void NetworkProcessProxy::didWriteBlobToFilePath(bool success, uint64_t callbackID)
{
    if (auto handler = m_writeBlobToFilePathCallbackMap.take(callbackID))
        handler(success);
    else
        ASSERT_NOT_REACHED();
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
        auto quota = m_processPool.websiteDataStore() ? m_processPool.websiteDataStore()->websiteDataStore().cacheStoragePerOriginQuota() : WebsiteDataStore::defaultCacheStoragePerOriginQuota;
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
void NetworkProcessProxy::getSandboxExtensionsForBlobFiles(uint64_t requestID, const Vector<String>& paths)
{
    SandboxExtension::HandleArray extensions;
    extensions.allocate(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) {
        // ReadWrite is required for creating hard links, which is something that might be done with these extensions.
        SandboxExtension::createHandle(paths[i], SandboxExtension::Type::ReadWrite, extensions[i]);
    }
    
    send(Messages::NetworkProcess::DidGetSandboxExtensionsForBlobFiles(requestID, extensions), 0);
}
#endif

#if ENABLE(SERVICE_WORKER)
void NetworkProcessProxy::establishWorkerContextConnectionToNetworkProcess(SecurityOriginData&& origin)
{
    m_processPool.establishWorkerContextConnectionToNetworkProcess(*this, WTFMove(origin), std::nullopt);
}

void NetworkProcessProxy::establishWorkerContextConnectionToNetworkProcessForExplicitSession(SecurityOriginData&& origin, PAL::SessionID sessionID)
{
    m_processPool.establishWorkerContextConnectionToNetworkProcess(*this, WTFMove(origin), sessionID);
}
#endif

} // namespace WebKit

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_URL
