/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
#include "APICustomProtocolManagerClient.h"
#include "APIDataTask.h"
#include "APIDataTaskClient.h"
#include "APIHTTPCookieStore.h"
#include "APINavigation.h"
#include "AuthenticationChallengeProxy.h"
#include "AuthenticationManager.h"
#include "DownloadProxyMap.h"
#include "DownloadProxyMessages.h"
#include "FrameInfoData.h"
#include "LegacyGlobalSettings.h"
#include "Logging.h"
#include "NetworkContentRuleListManagerMessages.h"
#include "NetworkProcessConnectionInfo.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxyMessages.h"
#include "PageClient.h"
#include "RemoteWorkerType.h"
#include "SandboxExtension.h"
#include "ShouldGrandfatherStatistics.h"
#include "StorageAccessStatus.h"
#include "WebCompiledContentRuleList.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebProcessProxyMessages.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebUserContentControllerProxy.h"
#include "WebsiteData.h"
#include "WebsiteDataStoreClient.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/PushPermissionState.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceError.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CompletionHandler.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShimProxy.h"
#endif

#if HAVE(SEC_KEY_PROXY)
#include "SecKeyProxyStore.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManagerMessages.h"
#include "LegacyCustomProtocolManagerProxyMessages.h"
#endif

#if PLATFORM(COCOA)
#include "LegacyCustomProtocolManagerClient.h"
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())

namespace WebKit {
using namespace WebCore;

static constexpr Seconds networkProcessResponsivenessTimeout = 6_s;
static constexpr int unresponsivenessCountLimit = 3;
static constexpr Seconds unresponsivenessCheckPeriod = 15_s;

static HashSet<NetworkProcessProxy*>& networkProcessesSet()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashSet<NetworkProcessProxy*>> set;
    return set;
}

static bool shouldTerminateNetworkProcessBySendingMessage()
{
    static WallTime unresponsivenessPeriodStartTime = WallTime::now();
    static int unresponsivenessCountDuringThisPeriod = 0;
    auto now = WallTime::now();

    if (now - unresponsivenessPeriodStartTime > unresponsivenessCheckPeriod) {
        unresponsivenessCountDuringThisPeriod = 1;
        unresponsivenessPeriodStartTime = now;
        return false;
    }

    ++unresponsivenessCountDuringThisPeriod;
    if (unresponsivenessCountDuringThisPeriod >= unresponsivenessCountLimit)
        return true;

    return false;
}

Vector<Ref<NetworkProcessProxy>> NetworkProcessProxy::allNetworkProcesses()
{
    return WTF::map(networkProcessesSet(), [](auto* networkProcess) {
        return Ref { *networkProcess };
    });
}

WeakPtr<NetworkProcessProxy>& NetworkProcessProxy::defaultNetworkProcess()
{
    static NeverDestroyed<WeakPtr<NetworkProcessProxy>> networkProcess;
    return networkProcess.get();
}

Ref<NetworkProcessProxy> NetworkProcessProxy::ensureDefaultNetworkProcess()
{
    auto& networkProcess = defaultNetworkProcess();
    if (networkProcess)
        return Ref { *networkProcess };

    auto newNetworkProcess = NetworkProcessProxy::create();
    networkProcess = newNetworkProcess.get();
    return newNetworkProcess;
}

void NetworkProcessProxy::terminate()
{
    AuxiliaryProcessProxy::terminate();
    if (auto* connection = this->connection())
        connection->invalidate();
}

void NetworkProcessProxy::requestTermination()
{
    terminate();
    networkProcessDidTerminate(ProcessTerminationReason::RequestedByClient);
}

void NetworkProcessProxy::didBecomeUnresponsive()
{
    RELEASE_LOG_ERROR(Process, "NetworkProcessProxy::didBecomeUnresponsive: NetworkProcess with PID %d became unresponsive, terminating it", processIdentifier());

    // Let network process terminates itself and generate crash report for investigation of hangs.
    // We currently only do this when network process becomes unresponsive multiple times in a short
    // time period to avoid generating too many crash reports with same back trace on user's device.
    if (shouldTerminateNetworkProcessBySendingMessage()) {
        sendMessage(makeUniqueRef<IPC::Encoder>(IPC::MessageName::Terminate, 0), { });
        RunLoop::main().dispatchAfter(1_s, [weakThis = WeakPtr { *this }] () mutable {
            if (weakThis) {
                weakThis->terminate();
                weakThis->networkProcessDidTerminate(ProcessTerminationReason::Unresponsive);
            }

        });
        return;
    }

    terminate();
    networkProcessDidTerminate(ProcessTerminationReason::Unresponsive);
}

void NetworkProcessProxy::sendCreationParametersToNewProcess()
{
    ASSERT(RunLoop::isMain());

    // FIXME: This is a temporary workaround for apps using WebKit API on non-main threads.
    // We should remove this once we enforce threading violation check on our APIs.
    // https://bugs.webkit.org/show_bug.cgi?id=200246.
    if (!RunLoop::isMain()) {
        callOnMainRunLoopAndWait([this] {
            sendCreationParametersToNewProcess();
        });
    }

    NetworkProcessCreationParameters parameters;
    parameters.auxiliaryProcessParameters = auxiliaryProcessParameters();
    parameters.urlSchemesRegisteredAsSecure = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsSecure());
    parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsBypassingContentSecurityPolicy());
    parameters.urlSchemesRegisteredAsLocal = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsLocal());
    parameters.urlSchemesRegisteredAsNoAccess = copyToVector(LegacyGlobalSettings::singleton().schemesToRegisterAsNoAccess());
    parameters.cacheModel = LegacyGlobalSettings::singleton().cacheModel();
    parameters.urlSchemesRegisteredForCustomProtocols = WebProcessPool::urlSchemesWithCustomProtocolHandlers();
    parameters.localhostAliasesForTesting = LegacyGlobalSettings::singleton().hostnamesToRegisterAsLocal();

#if !PLATFORM(GTK) && !PLATFORM(WPE) // GTK and WPE don't use defaultNetworkProcess
    parameters.websiteDataStoreParameters = WebsiteDataStore::parametersFromEachWebsiteDataStore();
    WebsiteDataStore::forEachWebsiteDataStore([&](auto& websiteDataStore) {
        addSession(websiteDataStore, SendParametersToNetworkProcess::No);
    });
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    parameters.enablePrivateClickMeasurement = false;
#endif

    parameters.allowedFirstPartiesForCookies = WebProcessProxy::allowedFirstPartiesForCookies();

    WebProcessPool::platformInitializeNetworkProcess(parameters);
    sendWithAsyncReply(Messages::NetworkProcess::InitializeNetworkProcess(parameters), [weakThis = WeakPtr { *this }] {
        if (weakThis)
            weakThis->beginResponsivenessChecks();
    });
}

static bool anyProcessPoolAlwaysRunsAtBackgroundPriority()
{
    for (auto& processPool : WebProcessPool::allProcessPools()) {
        if (processPool->alwaysRunsAtBackgroundPriority())
            return true;
    }
    return false;
}

NetworkProcessProxy::NetworkProcessProxy()
    : AuxiliaryProcessProxy(anyProcessPoolAlwaysRunsAtBackgroundPriority(), networkProcessResponsivenessTimeout)
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    , m_customProtocolManagerClient(makeUniqueRef<LegacyCustomProtocolManagerClient>())
    , m_customProtocolManagerProxy(*this)
#else
    , m_customProtocolManagerClient(makeUniqueRef<API::CustomProtocolManagerClient>())
#endif
    , m_throttler(*this, WebProcessPool::anyProcessPoolNeedsUIBackgroundAssertion())
{
    RELEASE_LOG(Process, "%p - NetworkProcessProxy::NetworkProcessProxy", this);

    connect();
    sendCreationParametersToNewProcess();
    updateProcessAssertion();
    networkProcessesSet().add(this);
#if PLATFORM(IOS_FAMILY)
    addBackgroundStateObservers();
#endif
}

NetworkProcessProxy::~NetworkProcessProxy()
{
#if ENABLE(CONTENT_EXTENSIONS)
    for (auto* proxy : m_webUserContentControllerProxies)
        proxy->removeNetworkProcess(*this);
#endif

    if (m_downloadProxyMap)
        m_downloadProxyMap->invalidate();
    networkProcessesSet().remove(this);
#if PLATFORM(IOS_FAMILY)
    removeBackgroundStateObservers();
#endif
}

void NetworkProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::Network;
    AuxiliaryProcessProxy::getLaunchOptions(launchOptions);

    if (WebsiteDataStore::shouldMakeNextNetworkProcessLaunchFailForTesting())
        launchOptions.shouldMakeProcessLaunchFailForTesting = true;
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

void NetworkProcessProxy::getNetworkProcessConnection(WebProcessProxy& webProcessProxy, CompletionHandler<void(NetworkProcessConnectionInfo&&)>&& reply)
{
    RELEASE_LOG(ProcessSuspension, "%p - NetworkProcessProxy is taking a background assertion because a web process is requesting a connection", this);
    startResponsivenessTimer(UseLazyStop::No);
    sendWithAsyncReply(Messages::NetworkProcess::CreateNetworkConnectionToWebProcess { webProcessProxy.coreProcessIdentifier(), webProcessProxy.sessionID() }, [this, weakThis = WeakPtr { *this }, reply = WTFMove(reply)](auto&& identifier, auto cookieAcceptPolicy) mutable {
        if (!weakThis) {
            RELEASE_LOG_ERROR(Process, "NetworkProcessProxy::getNetworkProcessConnection: NetworkProcessProxy deallocated during connection establishment");
            return reply({ });
        }

        stopResponsivenessTimer();
        if (!identifier) {
            RELEASE_LOG_ERROR(Process, "NetworkProcessProxy::getNetworkProcessConnection: connection identifier is empty");
            return reply({ });
        }

#if USE(UNIX_DOMAIN_SOCKETS) || OS(WINDOWS)
        reply(NetworkProcessConnectionInfo { WTFMove(*identifier), cookieAcceptPolicy });
        UNUSED_VARIABLE(this);
#elif OS(DARWIN)
        MESSAGE_CHECK(MACH_PORT_VALID(identifier->sendRight()));
        reply(NetworkProcessConnectionInfo { WTFMove(*identifier) , cookieAcceptPolicy, connection()->getAuditToken() });
#else
        notImplemented();
#endif
    }, 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void NetworkProcessProxy::synthesizeAppIsBackground(bool background)
{
    if (background)
        applicationDidEnterBackground();
    else
        applicationWillEnterForeground();
}

DownloadProxy& NetworkProcessProxy::createDownloadProxy(WebsiteDataStore& dataStore, WebProcessPool& processPool, const ResourceRequest& resourceRequest, const FrameInfoData& frameInfo, WebPageProxy* originatingPage)
{
    if (!m_downloadProxyMap)
        m_downloadProxyMap = makeUnique<DownloadProxyMap>(*this);

    return m_downloadProxyMap->createDownloadProxy(dataStore, processPool, resourceRequest, frameInfo, originatingPage);
}

void NetworkProcessProxy::dataTaskWithRequest(WebPageProxy& page, PAL::SessionID sessionID, WebCore::ResourceRequest&& request, CompletionHandler<void(API::DataTask&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::DataTaskWithRequest(page.identifier(), sessionID, request, IPC::FormDataReference(request.httpBody())), [this, protectedThis = Ref { *this }, weakPage = WeakPtr { page }, completionHandler = WTFMove(completionHandler), originalURL = request.url()] (DataTaskIdentifier identifier) mutable {
        MESSAGE_CHECK(decltype(m_dataTasks)::isValidKey(identifier));
        auto dataTask = API::DataTask::create(identifier, WTFMove(weakPage), WTFMove(originalURL));
        completionHandler(dataTask);
        m_dataTasks.add(identifier, WTFMove(dataTask));
    });
}

void NetworkProcessProxy::dataTaskReceivedChallenge(DataTaskIdentifier identifier, WebCore::AuthenticationChallenge&& challenge, CompletionHandler<void(AuthenticationChallengeDisposition, WebCore::Credential&&)>&& completionHandler)
{
    MESSAGE_CHECK(decltype(m_dataTasks)::isValidKey(identifier));
    if (auto task = m_dataTasks.get(identifier))
        task->client().didReceiveChallenge(*task, WTFMove(challenge), WTFMove(completionHandler));
    else
        completionHandler(AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue, { });
}

void NetworkProcessProxy::dataTaskWillPerformHTTPRedirection(DataTaskIdentifier identifier, WebCore::ResourceResponse&& response, WebCore::ResourceRequest&& request, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK(decltype(m_dataTasks)::isValidKey(identifier));
    if (auto task = m_dataTasks.get(identifier))
        task->client().willPerformHTTPRedirection(*task, WTFMove(response), WTFMove(request), WTFMove(completionHandler));
}

void NetworkProcessProxy::dataTaskDidReceiveResponse(DataTaskIdentifier identifier, WebCore::ResourceResponse&& response, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK(decltype(m_dataTasks)::isValidKey(identifier));
    if (auto task = m_dataTasks.get(identifier))
        task->client().didReceiveResponse(*task, WTFMove(response), WTFMove(completionHandler));
    else
        completionHandler(false);
}

void NetworkProcessProxy::dataTaskDidReceiveData(DataTaskIdentifier identifier, const IPC::DataReference& data)
{
    MESSAGE_CHECK(decltype(m_dataTasks)::isValidKey(identifier));
    if (auto task = m_dataTasks.get(identifier))
        task->client().didReceiveData(*task, data);
}

void NetworkProcessProxy::dataTaskDidCompleteWithError(DataTaskIdentifier identifier, WebCore::ResourceError&& error)
{
    MESSAGE_CHECK(decltype(m_dataTasks)::isValidKey(identifier));
    if (auto task = m_dataTasks.take(identifier))
        task->client().didCompleteWithError(*task, WTFMove(error));
}

void NetworkProcessProxy::cancelDataTask(DataTaskIdentifier identifier, PAL::SessionID sessionID)
{
    m_dataTasks.remove(identifier);
    send(Messages::NetworkProcess::CancelDataTask(identifier, sessionID), 0);
}

void NetworkProcessProxy::fetchWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, CompletionHandler<void(WebsiteData)>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::FetchWebsiteData(sessionID, dataTypes, fetchOptions), WTFMove(completionHandler));
}

void NetworkProcessProxy::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::DeleteWebsiteData(sessionID, dataTypes, modifiedSince), WTFMove(completionHandler));
}

void NetworkProcessProxy::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, const Vector<WebCore::SecurityOriginData>& origins, const Vector<String>& cookieHostNames, const Vector<String>& HSTSCacheHostNames, const Vector<RegistrableDomain>& registrableDomains, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::DeleteWebsiteDataForOrigins(sessionID, dataTypes, origins, cookieHostNames, HSTSCacheHostNames, registrableDomains), WTFMove(completionHandler));
}

void NetworkProcessProxy::renameOriginInWebsiteData(PAL::SessionID sessionID, const URL& oldName, const URL& newName, OptionSet<WebsiteDataType> dataTypes, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::RenameOriginInWebsiteData(sessionID, oldName, newName, dataTypes), WTFMove(completionHandler));
}

void NetworkProcessProxy::websiteDataOriginDirectoryForTesting(PAL::SessionID sessionID, URL&& origin, URL&& topOrigin, WebsiteDataType type, CompletionHandler<void(const String&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::WebsiteDataOriginDirectoryForTesting(sessionID, WTFMove(origin), WTFMove(topOrigin), type), WTFMove(completionHandler));
}

void NetworkProcessProxy::networkProcessDidTerminate(ProcessTerminationReason reason)
{
    Ref protectedThis { *this };

    if (m_downloadProxyMap)
        m_downloadProxyMap->invalidate();
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    m_customProtocolManagerProxy.invalidate();
#endif

    m_uploadActivity = std::nullopt;

    if (defaultNetworkProcess() == this)
        defaultNetworkProcess() = nullptr;

    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->networkProcessDidTerminate(*this, reason);
    for (auto& websiteDataStore : copyToVectorOf<Ref<WebsiteDataStore>>(m_websiteDataStores))
        websiteDataStore->networkProcessDidTerminate(*this);
    for (auto& task : m_dataTasks.values())
        task->client().didCompleteWithError(task, WebCore::internalError(task->originalURL()));
    m_dataTasks.clear();
}

void NetworkProcessProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (dispatchMessage(connection, decoder))
        return;

    didReceiveNetworkProcessProxyMessage(connection, decoder);
}

bool NetworkProcessProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    if (dispatchSyncMessage(connection, decoder, replyEncoder))
        return true;

    return didReceiveSyncNetworkProcessProxyMessage(connection, decoder, replyEncoder);
}

void NetworkProcessProxy::didClose(IPC::Connection& connection)
{
#if OS(DARWIN)
    RELEASE_LOG_ERROR(Process, "%p - NetworkProcessProxy::didClose (Network Process %d crash)", this, connection.remoteProcessID());
#else
    RELEASE_LOG_ERROR(Process, "%p - NetworkProcessProxy::didClose (Network Process crash)", this);
#endif

    // This will cause us to be deleted.
    networkProcessDidTerminate(ProcessTerminationReason::Crash);
}

void NetworkProcessProxy::didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName)
{
    logInvalidMessage(connection, messageName);
    terminate();
    networkProcessDidTerminate(ProcessTerminationReason::Crash);
}

void NetworkProcessProxy::processAuthenticationChallenge(PAL::SessionID sessionID, Ref<AuthenticationChallengeProxy>&& authenticationChallenge)
{
    auto* store = websiteDataStoreFromSessionID(sessionID);
    if (!store || authenticationChallenge->core().protectionSpace().authenticationScheme() != ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested) {
        authenticationChallenge->listener().completeChallenge(AuthenticationChallengeDisposition::PerformDefaultHandling);
        return;
    }
    store->client().didReceiveAuthenticationChallenge(WTFMove(authenticationChallenge));
}

void NetworkProcessProxy::didReceiveAuthenticationChallenge(PAL::SessionID sessionID, WebPageProxyIdentifier pageID, const std::optional<SecurityOriginData>& topOrigin, WebCore::AuthenticationChallenge&& coreChallenge, bool negotiatedLegacyTLS, AuthenticationChallengeIdentifier challengeID)
{
#if HAVE(SEC_KEY_PROXY)
    WeakPtr<SecKeyProxyStore> secKeyProxyStore;
    if (coreChallenge.protectionSpace().authenticationScheme() == ProtectionSpace::AuthenticationScheme::ClientCertificateRequested) {
        if (auto* store = websiteDataStoreFromSessionID(sessionID)) {
            auto newSecKeyProxyStore = SecKeyProxyStore::create();
            secKeyProxyStore = newSecKeyProxyStore;
            store->addSecKeyProxyStore(WTFMove(newSecKeyProxyStore));
        }
    }
    auto authenticationChallenge = AuthenticationChallengeProxy::create(WTFMove(coreChallenge), challengeID, *connection(), WTFMove(secKeyProxyStore));
#else
    auto authenticationChallenge = AuthenticationChallengeProxy::create(WTFMove(coreChallenge), challengeID, *connection(), nullptr);
#endif

    RefPtr<WebPageProxy> page;
    if (pageID)
        page = WebProcessProxy::webPage(pageID);

    if (page) {
        page->didReceiveAuthenticationChallengeProxy(WTFMove(authenticationChallenge), negotiatedLegacyTLS ? NegotiatedLegacyTLS::Yes : NegotiatedLegacyTLS::No);
        return;
    }

    if (!topOrigin) {
        authenticationChallenge->listener().completeChallenge(AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue);
        return;
    }

    WebPageProxy::forMostVisibleWebPageIfAny(sessionID, *topOrigin, [this, weakThis = WeakPtr { *this }, sessionID, authenticationChallenge = WTFMove(authenticationChallenge), negotiatedLegacyTLS](WebPageProxy* page) mutable {
        if (!weakThis)
            return;

        if (page) {
            page->didReceiveAuthenticationChallengeProxy(WTFMove(authenticationChallenge), negotiatedLegacyTLS ? NegotiatedLegacyTLS::Yes : NegotiatedLegacyTLS::No);
            return;
        }
        processAuthenticationChallenge(sessionID, WTFMove(authenticationChallenge));
    });
}

void NetworkProcessProxy::negotiatedLegacyTLS(WebPageProxyIdentifier pageID)
{
    RefPtr<WebPageProxy> page;
    if (pageID)
        page = WebProcessProxy::webPage(pageID);
    if (page)
        page->negotiatedLegacyTLS();
}

void NetworkProcessProxy::didNegotiateModernTLS(WebPageProxyIdentifier pageID, const URL& url)
{
    if (auto page = pageID ? WebProcessProxy::webPage(pageID) : nullptr)
        page->didNegotiateModernTLS(url);
}

void NetworkProcessProxy::triggerBrowsingContextGroupSwitchForNavigation(WebPageProxyIdentifier pageID, uint64_t navigationID, BrowsingContextGroupSwitchDecision browsingContextGroupSwitchDecision, const WebCore::RegistrableDomain& responseDomain, NetworkResourceLoadIdentifier existingNetworkResourceLoadIdentifierToResume, CompletionHandler<void(bool success)>&& completionHandler)
{
    RELEASE_LOG(ProcessSwapping, "%p - NetworkProcessProxy::triggerBrowsingContextGroupSwitchForNavigation: pageID=%" PRIu64 ", navigationID=%" PRIu64 ", browsingContextGroupSwitchDecision=%u, existingNetworkResourceLoadIdentifierToResume=%" PRIu64, this, pageID.toUInt64(), navigationID, (unsigned)browsingContextGroupSwitchDecision, existingNetworkResourceLoadIdentifierToResume.toUInt64());
    if (auto page = pageID ? WebProcessProxy::webPage(pageID) : nullptr)
        page->triggerBrowsingContextGroupSwitchForNavigation(navigationID, browsingContextGroupSwitchDecision, responseDomain, existingNetworkResourceLoadIdentifierToResume, WTFMove(completionHandler));
    else
        completionHandler(false);
}

void NetworkProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    RELEASE_LOG(Process, "%p - NetworkProcessProxy::didFinishLaunching", this);

    AuxiliaryProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (!connectionIdentifier) {
        networkProcessDidTerminate(ProcessTerminationReason::Crash);
        return;
    }
    
#if USE(RUNNINGBOARD)
    if (xpc_connection_t connection = this->connection()->xpcConnection())
        m_throttler.didConnectToProcess(xpc_connection_get_pid(connection));
#endif
}

void NetworkProcessProxy::logDiagnosticMessage(WebPageProxyIdentifier pageID, const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    auto page = WebProcessProxy::webPage(pageID);
    // FIXME: We do this null-check because by the time the decision to log is made, the page may be gone. We should refactor to avoid this,
    // but for now we simply drop the message in the rare case this happens.
    if (!page)
        return;

    page->logDiagnosticMessage(message, description, shouldSample);
}

void NetworkProcessProxy::terminateWebProcess(WebCore::ProcessIdentifier webProcessIdentifier)
{
    if (auto* process = WebProcessProxy::processForIdentifier(webProcessIdentifier))
        process->requestTermination(ProcessTerminationReason::RequestedByNetworkProcess);
}

void NetworkProcessProxy::terminateUnresponsiveServiceWorkerProcesses(WebCore::ProcessIdentifier processIdentifier)
{
    if (RefPtr process = WebProcessProxy::processForIdentifier(processIdentifier)) {
        process->disableRemoteWorkers(RemoteWorkerType::ServiceWorker);
        process->requestTermination(ProcessTerminationReason::ExceededCPULimit);
    }
}

void NetworkProcessProxy::logDiagnosticMessageWithResult(WebPageProxyIdentifier pageID, const String& message, const String& description, uint32_t result, WebCore::ShouldSample shouldSample)
{
    auto page = WebProcessProxy::webPage(pageID);
    // FIXME: We do this null-check because by the time the decision to log is made, the page may be gone. We should refactor to avoid this,
    // but for now we simply drop the message in the rare case this happens.
    if (!page)
        return;

    page->logDiagnosticMessageWithResult(message, description, result, shouldSample);
}

void NetworkProcessProxy::logDiagnosticMessageWithValue(WebPageProxyIdentifier pageID, const String& message, const String& description, double value, unsigned significantFigures, WebCore::ShouldSample shouldSample)
{
    auto page = WebProcessProxy::webPage(pageID);
    // FIXME: We do this null-check because by the time the decision to log is made, the page may be gone. We should refactor to avoid this,
    // but for now we simply drop the message in the rare case this happens.
    if (!page)
        return;

    page->logDiagnosticMessageWithValue(message, description, value, significantFigures, shouldSample);
}

void NetworkProcessProxy::resourceLoadDidSendRequest(WebPageProxyIdentifier pageID, ResourceLoadInfo&& loadInfo, WebCore::ResourceRequest&& request, std::optional<IPC::FormDataReference>&& httpBody)
{
    auto page = WebProcessProxy::webPage(pageID);
    if (!page)
        return;

    if (httpBody) {
        auto data = httpBody->takeData();
        request.setHTTPBody(WTFMove(data));
    }

    page->resourceLoadDidSendRequest(WTFMove(loadInfo), WTFMove(request));
}

void NetworkProcessProxy::resourceLoadDidPerformHTTPRedirection(WebPageProxyIdentifier pageID, ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response, WebCore::ResourceRequest&& request)
{
    auto page = WebProcessProxy::webPage(pageID);
    if (!page)
        return;

    page->resourceLoadDidPerformHTTPRedirection(WTFMove(loadInfo), WTFMove(response), WTFMove(request));
}

void NetworkProcessProxy::resourceLoadDidReceiveChallenge(WebPageProxyIdentifier pageID, ResourceLoadInfo&& loadInfo, WebCore::AuthenticationChallenge&& challenge)
{
    auto page = WebProcessProxy::webPage(pageID);
    if (!page)
        return;

    page->resourceLoadDidReceiveChallenge(WTFMove(loadInfo), WTFMove(challenge));
}

void NetworkProcessProxy::resourceLoadDidReceiveResponse(WebPageProxyIdentifier pageID, ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response)
{
    auto page = WebProcessProxy::webPage(pageID);
    if (!page)
        return;

    page->resourceLoadDidReceiveResponse(WTFMove(loadInfo), WTFMove(response));
}

void NetworkProcessProxy::resourceLoadDidCompleteWithError(WebPageProxyIdentifier pageID, ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response, WebCore::ResourceError&& error)
{
    auto page = WebProcessProxy::webPage(pageID);
    if (!page)
        return;

    page->resourceLoadDidCompleteWithError(WTFMove(loadInfo), WTFMove(response), WTFMove(error));
}

#if ENABLE(TRACKING_PREVENTION)
void NetworkProcessProxy::dumpResourceLoadStatistics(PAL::SessionID sessionID, CompletionHandler<void(String)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler({ });
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::DumpResourceLoadStatistics(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::updatePrevalentDomainsToBlockCookiesFor(PAL::SessionID sessionID, const Vector<RegistrableDomain>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    sendWithAsyncReply(Messages::NetworkProcess::UpdatePrevalentDomainsToBlockCookiesFor(sessionID, domainsToBlock), WTFMove(completionHandler));
}

void NetworkProcessProxy::isPrevalentResource(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::IsPrevalentResource(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::isVeryPrevalentResource(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::IsVeryPrevalentResource(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setPrevalentResource(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetPrevalentResource(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setPrevalentResourceForDebugMode(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetPrevalentResourceForDebugMode(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setVeryPrevalentResource(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetVeryPrevalentResource(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setLastSeen(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, Seconds lastSeen, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetLastSeen(sessionID, resourceDomain, lastSeen), WTFMove(completionHandler));
}

void NetworkProcessProxy::domainIDExistsInDatabase(PAL::SessionID sessionID, int domainID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::DomainIDExistsInDatabase(sessionID, domainID), WTFMove(completionHandler));
}

void NetworkProcessProxy::mergeStatisticForTesting(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, const RegistrableDomain& topFrameDomain1, const RegistrableDomain& topFrameDomain2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::MergeStatisticForTesting(sessionID, resourceDomain, topFrameDomain1, topFrameDomain2, lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved), WTFMove(completionHandler));
}

void NetworkProcessProxy::insertExpiredStatisticForTesting(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::InsertExpiredStatisticForTesting(sessionID, resourceDomain, numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent), WTFMove(completionHandler));
}

void NetworkProcessProxy::clearPrevalentResource(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, CompletionHandler<void()>&& completionHandler)
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

void NetworkProcessProxy::scheduleClearInMemoryAndPersistent(PAL::SessionID sessionID, std::optional<WallTime> modifiedSince, ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
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

void NetworkProcessProxy::statisticsDatabaseHasAllTables(PAL::SessionID sessionID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::StatisticsDatabaseHasAllTables(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::logUserInteraction(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::LogUserInteraction(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::hasHadUserInteraction(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::HadUserInteraction(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::isRelationshipOnlyInDatabaseOnce(PAL::SessionID sessionID, const RegistrableDomain& subDomain, const RegistrableDomain& topDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::IsRelationshipOnlyInDatabaseOnce(sessionID, subDomain, topDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::clearUserInteraction(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::ClearUserInteraction(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::hasLocalStorage(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::HasLocalStorage(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setTimeToLiveUserInteraction(PAL::SessionID sessionID, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetTimeToLiveUserInteraction(sessionID, seconds), WTFMove(completionHandler));
}

void NetworkProcessProxy::setNotifyPagesWhenDataRecordsWereScanned(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetNotifyPagesWhenDataRecordsWereScanned(sessionID, value), WTFMove(completionHandler));
}

void NetworkProcessProxy::setResourceLoadStatisticsTimeAdvanceForTesting(PAL::SessionID sessionID, Seconds time, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetResourceLoadStatisticsTimeAdvanceForTesting(sessionID, time), WTFMove(completionHandler));
}

void NetworkProcessProxy::setIsRunningResourceLoadStatisticsTest(PAL::SessionID sessionID, bool value, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetIsRunningResourceLoadStatisticsTest(sessionID, value), WTFMove(completionHandler));
}

void NetworkProcessProxy::setSubframeUnderTopFrameDomain(PAL::SessionID sessionID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetSubframeUnderTopFrameDomain(sessionID, subFrameDomain, topFrameDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::isRegisteredAsRedirectingTo(PAL::SessionID sessionID, const RegistrableDomain& domainRedirectedFrom, const RegistrableDomain& domainRedirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::IsRegisteredAsRedirectingTo(sessionID, domainRedirectedFrom, domainRedirectedTo), WTFMove(completionHandler));
}

void NetworkProcessProxy::isRegisteredAsSubFrameUnder(PAL::SessionID sessionID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::IsRegisteredAsSubFrameUnder(sessionID, subFrameDomain, topFrameDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setSubresourceUnderTopFrameDomain(PAL::SessionID sessionID, const RegistrableDomain& subresourceDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetSubresourceUnderTopFrameDomain(sessionID, subresourceDomain, topFrameDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::isRegisteredAsSubresourceUnder(PAL::SessionID sessionID, const RegistrableDomain& subresourceDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::IsRegisteredAsSubresourceUnder(sessionID, subresourceDomain, topFrameDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setSubresourceUniqueRedirectTo(PAL::SessionID sessionID, const RegistrableDomain& subresourceDomain, const RegistrableDomain& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetSubresourceUniqueRedirectTo(sessionID, subresourceDomain, domainRedirectedTo), WTFMove(completionHandler));
}

void NetworkProcessProxy::setSubresourceUniqueRedirectFrom(PAL::SessionID sessionID, const RegistrableDomain& subresourceDomain, const RegistrableDomain& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetSubresourceUniqueRedirectFrom(sessionID, subresourceDomain, domainRedirectedFrom), WTFMove(completionHandler));
}

void NetworkProcessProxy::setTopFrameUniqueRedirectTo(PAL::SessionID sessionID, const RegistrableDomain& topFrameDomain, const RegistrableDomain& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetTopFrameUniqueRedirectTo(sessionID, topFrameDomain, domainRedirectedTo), WTFMove(completionHandler));
}

void NetworkProcessProxy::setTopFrameUniqueRedirectFrom(PAL::SessionID sessionID, const RegistrableDomain& topFrameDomain, const RegistrableDomain& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetTopFrameUniqueRedirectFrom(sessionID, topFrameDomain, domainRedirectedFrom), WTFMove(completionHandler));
}

void NetworkProcessProxy::isGrandfathered(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::IsGrandfathered(sessionID, resourceDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setGrandfathered(PAL::SessionID sessionID, const RegistrableDomain& resourceDomain, bool isGrandfathered, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetGrandfathered(sessionID, resourceDomain, isGrandfathered), WTFMove(completionHandler));
}

void NetworkProcessProxy::requestStorageAccessConfirm(WebPageProxyIdentifier pageID, FrameIdentifier frameID, const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    auto page = WebProcessProxy::webPage(pageID);
    if (!page) {
        completionHandler(false);
        return;
    }
    
    page->requestStorageAccessConfirm(subFrameDomain, topFrameDomain, frameID, WTFMove(completionHandler));
}

void NetworkProcessProxy::getAllStorageAccessEntries(PAL::SessionID sessionID, CompletionHandler<void(Vector<String> domains)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler({ });
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::GetAllStorageAccessEntries(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::getResourceLoadStatisticsDataSummary(PAL::SessionID sessionID, CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler({ });
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::GetResourceLoadStatisticsDataSummary(sessionID), WTFMove(completionHandler));
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

void NetworkProcessProxy::isResourceLoadStatisticsEphemeral(PAL::SessionID sessionID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::IsResourceLoadStatisticsEphemeral(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::resetCacheMaxAgeCapForPrevalentResources(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::ResetCacheMaxAgeCapForPrevalentResources(sessionID), [completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });
}

void NetworkProcessProxy::resetParametersToDefaultValues(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::ResetParametersToDefaultValues(sessionID), [completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });
}

void NetworkProcessProxy::scheduleClearInMemoryAndPersistent(PAL::SessionID sessionID, ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::ScheduleClearInMemoryAndPersistent(sessionID, { }, shouldGrandfather), [completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });
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

void NetworkProcessProxy::notifyWebsiteDataDeletionForRegistrableDomainsFinished()
{
    WebProcessProxy::notifyWebsiteDataDeletionForRegistrableDomainsFinished();
}

void NetworkProcessProxy::notifyWebsiteDataScanForRegistrableDomainsFinished()
{
    WebProcessProxy::notifyWebsiteDataScanForRegistrableDomainsFinished();
}

void NetworkProcessProxy::didCommitCrossSiteLoadWithDataTransfer(PAL::SessionID sessionID, const RegistrableDomain& fromDomain, const RegistrableDomain& toDomain, OptionSet<WebCore::CrossSiteNavigationDataTransfer::Flag> navigationDataTransfer, WebPageProxyIdentifier webPageProxyID, PageIdentifier webPageID)
{
    if (!canSendMessage())
        return;

    send(Messages::NetworkProcess::DidCommitCrossSiteLoadWithDataTransfer(sessionID, fromDomain, toDomain, navigationDataTransfer, webPageProxyID, webPageID), 0);
}

void NetworkProcessProxy::didCommitCrossSiteLoadWithDataTransferFromPrevalentResource(WebPageProxyIdentifier pageID)
{
    if (!canSendMessage())
        return;

    auto page = WebProcessProxy::webPage(pageID);
    if (!page)
        return;

    page->didCommitCrossSiteLoadWithDataTransferFromPrevalentResource();
}

void NetworkProcessProxy::setCrossSiteLoadWithLinkDecorationForTesting(PAL::SessionID sessionID, const RegistrableDomain& fromDomain, const RegistrableDomain& toDomain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetCrossSiteLoadWithLinkDecorationForTesting(sessionID, fromDomain, toDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::resetCrossSiteLoadsWithLinkDecorationForTesting(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::ResetCrossSiteLoadsWithLinkDecorationForTesting(sessionID), [completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });
}

void NetworkProcessProxy::deleteCookiesForTesting(PAL::SessionID sessionID, const RegistrableDomain& domain, bool includeHttpOnlyCookies, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::DeleteCookiesForTesting(sessionID, domain, includeHttpOnlyCookies), WTFMove(completionHandler));
}

void NetworkProcessProxy::deleteWebsiteDataInUIProcessForRegistrableDomains(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, Vector<RegistrableDomain>&& domains, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    auto* websiteDataStore = websiteDataStoreFromSessionID(sessionID);
    if (!websiteDataStore || dataTypes.isEmpty() || domains.isEmpty()) {
        completionHandler({ });
        return;
    }

    websiteDataStore->fetchDataForRegistrableDomains(dataTypes, fetchOptions, WTFMove(domains), [dataTypes, websiteDataStore = Ref { *websiteDataStore }, completionHandler = WTFMove(completionHandler)] (Vector<WebsiteDataRecord>&& matchingDataRecords, HashSet<WebCore::RegistrableDomain>&& domainsWithMatchingDataRecords) mutable {
        websiteDataStore->removeData(dataTypes, WTFMove(matchingDataRecords), [domainsWithMatchingDataRecords = WTFMove(domainsWithMatchingDataRecords), completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(WTFMove(domainsWithMatchingDataRecords));
        });
    });
}

void NetworkProcessProxy::hasIsolatedSession(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::HasIsolatedSession(sessionID, domain), WTFMove(completionHandler));
}

#if ENABLE(APP_BOUND_DOMAINS)
void NetworkProcessProxy::setAppBoundDomainsForResourceLoadStatistics(PAL::SessionID sessionID, const HashSet<RegistrableDomain>& appBoundDomains, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::SetAppBoundDomainsForResourceLoadStatistics(sessionID, appBoundDomains), [completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });
}
#endif

#if ENABLE(MANAGED_DOMAINS)
void NetworkProcessProxy::setManagedDomainsForResourceLoadStatistics(PAL::SessionID sessionID, const HashSet<RegistrableDomain>& appBoundDomains, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::SetManagedDomainsForResourceLoadStatistics(sessionID, appBoundDomains), [completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });
}
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI_USES_SCENE)
void NetworkProcessProxy::getWindowSceneIdentifierForPaymentPresentation(WebPageProxyIdentifier webPageProxyIdentifier, CompletionHandler<void(const String&)>&& completionHandler)
{
    auto* page = WebProcessProxy::webPage(webPageProxyIdentifier);
    if (!page) {
        completionHandler(nullString());
        return;
    }
    completionHandler(page->pageClient().sceneID());
}
#endif

void NetworkProcessProxy::setShouldDowngradeReferrerForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::SetShouldDowngradeReferrerForTesting(enabled), [completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });
}

void NetworkProcessProxy::setThirdPartyCookieBlockingMode(PAL::SessionID sessionID, ThirdPartyCookieBlockingMode blockingMode, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::SetThirdPartyCookieBlockingMode(sessionID, blockingMode), [completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });
}

void NetworkProcessProxy::setShouldEnbleSameSiteStrictEnforcementForTesting(PAL::SessionID sessionID, WebCore::SameSiteStrictEnforcementEnabled enabled, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::SetShouldEnbleSameSiteStrictEnforcementForTesting(sessionID, enabled), WTFMove(completionHandler));
}

void NetworkProcessProxy::setFirstPartyWebsiteDataRemovalModeForTesting(PAL::SessionID sessionID, FirstPartyWebsiteDataRemovalMode mode, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetFirstPartyWebsiteDataRemovalModeForTesting(sessionID, mode), WTFMove(completionHandler));
}

void NetworkProcessProxy::setToSameSiteStrictCookiesForTesting(PAL::SessionID sessionID, const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::NetworkProcess::SetToSameSiteStrictCookiesForTesting(sessionID, domain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setFirstPartyHostCNAMEDomainForTesting(PAL::SessionID sessionID, const String& firstPartyHost, const RegistrableDomain& cnameDomain, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::SetFirstPartyHostCNAMEDomainForTesting(sessionID, firstPartyHost, cnameDomain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setThirdPartyCNAMEDomainForTesting(PAL::SessionID sessionID, const WebCore::RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::SetThirdPartyCNAMEDomainForTesting(sessionID, domain), WTFMove(completionHandler));
}

void NetworkProcessProxy::setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&& domains)
{
    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain> { domains });
}

void NetworkProcessProxy::setDomainsWithCrossPageStorageAccess(HashMap<TopFrameDomain, SubResourceDomain>&& domains, CompletionHandler<void()>&& completionHandler)
{    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->setDomainsWithCrossPageStorageAccess(HashMap<TopFrameDomain, SubResourceDomain> { domains }, [callbackAggregator] { });
}

#endif // ENABLE(TRACKING_PREVENTION)

void NetworkProcessProxy::setPrivateClickMeasurementDebugMode(PAL::SessionID sessionID, bool debugMode)
{
    if (!canSendMessage())
        return;

    send(Messages::NetworkProcess::SetPrivateClickMeasurementDebugMode(sessionID, debugMode), 0);
}

void NetworkProcessProxy::sendProcessWillSuspendImminentlyForTesting()
{
    if (canSendMessage())
        sendSync(Messages::NetworkProcess::ProcessWillSuspendImminentlyForTestingSync(), 0);
}

static bool s_suspensionAllowedForTesting { true };
void NetworkProcessProxy::setSuspensionAllowedForTesting(bool allowed)
{
    s_suspensionAllowedForTesting = allowed;
}

void NetworkProcessProxy::sendPrepareToSuspend(IsSuspensionImminent isSuspensionImminent, double remainingRunTime, CompletionHandler<void()>&& completionHandler)
{
    if (!s_suspensionAllowedForTesting)
        return completionHandler();
    
    auto estimatedSuspendTime = MonotonicTime::now() + Seconds(remainingRunTime);
    sendWithAsyncReply(Messages::NetworkProcess::PrepareToSuspend(isSuspensionImminent == IsSuspensionImminent::Yes, estimatedSuspendTime), WTFMove(completionHandler), 0, { }, ShouldStartProcessThrottlerActivity::No);
}

void NetworkProcessProxy::sendProcessDidResume(ResumeReason reason)
{
    if (canSendMessage())
        send(Messages::NetworkProcess::ProcessDidResume(reason == ResumeReason::ForegroundActivity), 0);
}

void NetworkProcessProxy::addSession(WebsiteDataStore& store, SendParametersToNetworkProcess sendParametersToNetworkProcess)
{
    auto addResult = m_websiteDataStores.add(store);
    if (!addResult.isNewEntry)
        return;

    // DispatchMessageEvenWhenWaitingForSyncReply is needed because session needs to be
    // added in network process before CreateNetworkConnectionToWebProcess, which has
    // DispatchMessageEvenWhenWaitingForSyncReply flag.
    if (canSendMessage() && sendParametersToNetworkProcess == SendParametersToNetworkProcess::Yes)
        send(Messages::NetworkProcess::AddWebsiteDataStore { store.parameters() }, 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    auto sessionID = store.sessionID();
    if (!sessionID.isEphemeral())
        createSymLinkForFileUpgrade(store.resolvedIndexedDBDatabaseDirectory());
}

void NetworkProcessProxy::removeSession(WebsiteDataStore& websiteDataStore)
{
    m_websiteDataStores.remove(websiteDataStore);

    if (canSendMessage())
        send(Messages::NetworkProcess::DestroySession { websiteDataStore.sessionID() }, 0);

    if (m_websiteDataStores.computesEmpty())
        defaultNetworkProcess() = nullptr;
}

WebsiteDataStore* NetworkProcessProxy::websiteDataStoreFromSessionID(PAL::SessionID sessionID)
{
    return WebsiteDataStore::existingDataStoreForSessionID(sessionID);
}

#if ENABLE(CONTENT_EXTENSIONS)
void NetworkProcessProxy::contentExtensionRules(UserContentControllerIdentifier identifier)
{
    if (auto* webUserContentControllerProxy = WebUserContentControllerProxy::get(identifier)) {
        m_webUserContentControllerProxies.add(webUserContentControllerProxy);
        webUserContentControllerProxy->addNetworkProcess(*this);

        auto rules = WTF::map(webUserContentControllerProxy->contentExtensionRules(), [](auto&& keyValue) -> std::pair<WebCompiledContentRuleListData, URL> {
            return { keyValue.value.first->compiledRuleList().data(), keyValue.value.second };
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

void NetworkProcessProxy::registerRemoteWorkerClientProcess(RemoteWorkerType workerType, WebCore::ProcessIdentifier clientProcessIdentifier, WebCore::ProcessIdentifier remoteWorkerProcessIdentifier)
{
    RELEASE_LOG(Worker, "NetworkProcessProxy::registerRemoteWorkerClientProcess: workerType=%" PUBLIC_LOG_STRING ", clientProcessIdentifier=%" PRIu64 ", remoteWorkerProcessIdentifier=%" PRIu64, workerType == RemoteWorkerType::ServiceWorker ? "service" : "shared", clientProcessIdentifier.toUInt64(), remoteWorkerProcessIdentifier.toUInt64());
    auto* clientWebProcess = WebProcessProxy::processForIdentifier(clientProcessIdentifier);
    auto* remoteWorkerProcess = WebProcessProxy::processForIdentifier(remoteWorkerProcessIdentifier);
    if (!clientWebProcess || !remoteWorkerProcess) {
        RELEASE_LOG_ERROR(Worker, "NetworkProcessProxy::registerRemoteWorkerClientProcess: Could not look up one of the processes (clientWebProcess=%p, remoteWorkerProcess=%p)", clientWebProcess, remoteWorkerProcess);
        return;
    }

    remoteWorkerProcess->registerRemoteWorkerClientProcess(workerType, *clientWebProcess);
}

void NetworkProcessProxy::unregisterRemoteWorkerClientProcess(RemoteWorkerType workerType, WebCore::ProcessIdentifier clientProcessIdentifier, WebCore::ProcessIdentifier remoteWorkerProcessIdentifier)
{
    RELEASE_LOG(Worker, "NetworkProcessProxy::unregisterRemoteWorkerClientProcess: workerType=%" PUBLIC_LOG_STRING ", clientProcessIdentifier=%" PRIu64 ", remoteWorkerProcessIdentifier=%" PRIu64, workerType == RemoteWorkerType::ServiceWorker ? "service" : "shared", clientProcessIdentifier.toUInt64(), remoteWorkerProcessIdentifier.toUInt64());
    auto* clientWebProcess = WebProcessProxy::processForIdentifier(clientProcessIdentifier);
    auto* remoteWorkerProcess = WebProcessProxy::processForIdentifier(remoteWorkerProcessIdentifier);
    if (!clientWebProcess || !remoteWorkerProcess) {
        RELEASE_LOG_ERROR(Worker, "NetworkProcessProxy::unregisterRemoteWorkerClientProcess: Could not look up one of the processes (clientWebProcess=%p, remoteWorkerProcess=%p)", clientWebProcess, remoteWorkerProcess);
        return;
    }

    remoteWorkerProcess->unregisterRemoteWorkerClientProcess(workerType, *clientWebProcess);
}

void NetworkProcessProxy::remoteWorkerContextConnectionNoLongerNeeded(RemoteWorkerType workerType, WebCore::ProcessIdentifier identifier)
{
    if (auto* process = WebProcessProxy::processForIdentifier(identifier))
        process->disableRemoteWorkers(workerType);
}

void NetworkProcessProxy::establishRemoteWorkerContextConnectionToNetworkProcess(RemoteWorkerType workerType, RegistrableDomain&& registrableDomain, std::optional<WebCore::ProcessIdentifier> requestingProcessIdentifier, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, PAL::SessionID sessionID, CompletionHandler<void(WebCore::ProcessIdentifier)>&& completionHandler)
{
    WebProcessPool::establishRemoteWorkerContextConnectionToNetworkProcess(workerType, WTFMove(registrableDomain), requestingProcessIdentifier, serviceWorkerPageIdentifier, sessionID, WTFMove(completionHandler));
}

#if ENABLE(SERVICE_WORKER)

void NetworkProcessProxy::startServiceWorkerBackgroundProcessing(WebCore::ProcessIdentifier serviceWorkerProcessIdentifier)
{
    if (auto* serviceWorkerProcess = WebProcessProxy::processForIdentifier(serviceWorkerProcessIdentifier))
        serviceWorkerProcess->startServiceWorkerBackgroundProcessing();
}

void NetworkProcessProxy::endServiceWorkerBackgroundProcessing(WebCore::ProcessIdentifier serviceWorkerProcessIdentifier)
{
    if (auto* serviceWorkerProcess = WebProcessProxy::processForIdentifier(serviceWorkerProcessIdentifier))
        serviceWorkerProcess->endServiceWorkerBackgroundProcessing();
}
#endif

void NetworkProcessProxy::requestStorageSpace(PAL::SessionID sessionID, const WebCore::ClientOrigin& origin, uint64_t currentQuota, uint64_t currentSize, uint64_t spaceRequired, CompletionHandler<void(std::optional<uint64_t> quota)>&& completionHandler)
{
    RELEASE_LOG(Storage, "%p - NetworkProcessProxy::requestStorageSpace", this);
    auto* store = websiteDataStoreFromSessionID(sessionID);

    if (!store) {
        completionHandler({ });
        return;
    }

    store->client().requestStorageSpace(origin.topOrigin, origin.clientOrigin, currentQuota, currentSize, spaceRequired, [sessionID, origin, currentQuota, currentSize, spaceRequired, completionHandler = WTFMove(completionHandler)](auto quota) mutable {
        if (quota) {
            completionHandler(quota);
            return;
        }

        if (origin.topOrigin != origin.clientOrigin) {
            completionHandler({ });
            return;
        }

        WebPageProxy::forMostVisibleWebPageIfAny(sessionID, origin.topOrigin, [completionHandler = WTFMove(completionHandler), origin, currentQuota, currentSize, spaceRequired](auto* page) mutable {
            RELEASE_LOG(Storage, "NetworkProcessProxy::requestStorageSpace trying to get a visible page: %d", !!page);
            if (!page) {
                completionHandler({ });
                return;
            }
            auto name = makeString(FileSystem::encodeForFileName(origin.topOrigin.host), " content");
            page->requestStorageSpace(page->mainFrame()->frameID(), origin.topOrigin.databaseIdentifier(), name, name, currentQuota, currentSize, currentSize, spaceRequired, [completionHandler = WTFMove(completionHandler)](auto quota) mutable {
                completionHandler(quota);
            });
        });
    });
}

void NetworkProcessProxy::increaseQuota(PAL::SessionID sessionID, const WebCore::ClientOrigin& origin, QuotaIncreaseRequestIdentifier identifier, uint64_t currentQuota, uint64_t currentUsage, uint64_t spaceRequested)
{
    requestStorageSpace(sessionID, origin, currentQuota, currentUsage, spaceRequested, [this, weakThis = WeakPtr { *this }, sessionID, origin, identifier](auto result) mutable {
        if (!weakThis)
            return;

        send(Messages::NetworkProcess::DidIncreaseQuota(sessionID, origin, identifier, result), 0);
    });
}

void NetworkProcessProxy::registerSchemeForLegacyCustomProtocol(const String& scheme)
{
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    send(Messages::LegacyCustomProtocolManager::RegisterScheme(scheme), 0);
#else
    UNUSED_PARAM(scheme);
#endif
}

void NetworkProcessProxy::unregisterSchemeForLegacyCustomProtocol(const String& scheme)
{
#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    send(Messages::LegacyCustomProtocolManager::UnregisterScheme(scheme), 0);
#else
    UNUSED_PARAM(scheme);
#endif
}

void NetworkProcessProxy::setWebProcessHasUploads(WebCore::ProcessIdentifier processID, bool hasUpload)
{
    if (!hasUpload) {
        if (!m_uploadActivity)
            return;

        auto assertion = m_uploadActivity->webProcessAssertions.take(processID);
        if (!assertion)
            return;

        RELEASE_LOG(ProcessSuspension, "NetworkProcessProxy::setWebProcessHasUploads: Releasing upload assertion on behalf of WebProcess with PID %d", assertion->pid());

        if (m_uploadActivity->webProcessAssertions.isEmpty()) {
            RELEASE_LOG(ProcessSuspension, "NetworkProcessProxy::setWebProcessHasUploads: The number of uploads in progress is now zero. Releasing Networking and UI process assertions.");
            m_uploadActivity = std::nullopt;
        }
        return;
    }

    auto* process = WebProcessProxy::processForIdentifier(processID);
    if (!process)
        return;

    if (!m_uploadActivity) {
        RELEASE_LOG(ProcessSuspension, "NetworkProcessProxy::setWebProcessHasUploads: The number of uploads in progress is now greater than 0. Taking Networking and UI process assertions.");
        m_uploadActivity = UploadActivity {
            ProcessAssertion::create(getCurrentProcessID(), "WebKit uploads"_s, ProcessAssertionType::UnboundedNetworking),
            ProcessAssertion::create(processIdentifier(), "WebKit uploads"_s, ProcessAssertionType::UnboundedNetworking),
            HashMap<WebCore::ProcessIdentifier, RefPtr<ProcessAssertion>>()
        };
    }

    m_uploadActivity->webProcessAssertions.ensure(processID, [&] {
        RELEASE_LOG(ProcessSuspension, "NetworkProcessProxy::setWebProcessHasUploads: Taking upload assertion on behalf of WebProcess with PID %d", process->processIdentifier());
        return ProcessAssertion::create(process->processIdentifier(), "WebKit uploads"_s, ProcessAssertionType::UnboundedNetworking);
    });
}

void NetworkProcessProxy::testProcessIncomingSyncMessagesWhenWaitingForSyncReply(WebPageProxyIdentifier pageID, CompletionHandler<void(bool)>&& reply)
{
    auto page = WebProcessProxy::webPage(pageID);
    if (!page)
        return reply(false);

    auto syncResult = page->sendSync(Messages::WebPage::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply(), Seconds::infinity(), IPC::SendSyncOption::ForceDispatchWhenDestinationIsWaitingForUnboundedSyncReply);
    auto [handled] = syncResult.takeReplyOr(false);
    reply(handled);
}

void NetworkProcessProxy::createSymLinkForFileUpgrade(const String& indexedDatabaseDirectory)
{
    if (indexedDatabaseDirectory.isEmpty())
        return;

    String oldVersionDirectory = FileSystem::pathByAppendingComponent(indexedDatabaseDirectory, "v0"_s);
    FileSystem::deleteEmptyDirectory(oldVersionDirectory);
    if (!FileSystem::fileExists(oldVersionDirectory))
        FileSystem::createSymbolicLink(indexedDatabaseDirectory, oldVersionDirectory);
}

void NetworkProcessProxy::preconnectTo(PAL::SessionID sessionID, WebPageProxyIdentifier webPageProxyID, WebCore::PageIdentifier webPageID, const URL& url, const String& userAgent, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, LastNavigationWasAppInitiated lastNavigationWasAppInitiated)
{
    if (!url.isValid() || !url.protocolIsInHTTPFamily())
        return;
    send(Messages::NetworkProcess::PreconnectTo(sessionID, webPageProxyID, webPageID, url, userAgent, storedCredentialsPolicy, isNavigatingToAppBoundDomain, lastNavigationWasAppInitiated), 0);
}

static bool anyProcessPoolHasForegroundWebProcesses()
{
    for (auto& processPool : WebProcessPool::allProcessPools()) {
        if (processPool->hasForegroundWebProcesses())
            return true;
    }
    return false;
}

static bool anyProcessPoolHasBackgroundWebProcesses()
{
    for (auto& processPool : WebProcessPool::allProcessPools()) {
        if (processPool->hasBackgroundWebProcesses())
            return true;
    }
    return false;
}

void NetworkProcessProxy::updateProcessAssertion()
{
    if (anyProcessPoolHasForegroundWebProcesses()) {
        if (!ProcessThrottler::isValidForegroundActivity(m_activityFromWebProcesses)) {
            m_activityFromWebProcesses = throttler().foregroundActivity("Networking for foreground view(s)"_s);
        }
        return;
    }
    if (anyProcessPoolHasBackgroundWebProcesses()) {
        if (!ProcessThrottler::isValidBackgroundActivity(m_activityFromWebProcesses)) {
            m_activityFromWebProcesses = throttler().backgroundActivity("Networking for background view(s)"_s);
        }
        return;
    }
    // Use std::exchange() instead of a simple nullptr assignment to avoid re-entering this
    // function during the destructor of the ProcessThrottler activity, before setting
    // m_activityFromWebProcesses.
    std::exchange(m_activityFromWebProcesses, nullptr);
}

void NetworkProcessProxy::resetQuota(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::ResetQuota(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::clearStorage(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::ClearStorage(sessionID), WTFMove(completionHandler));
}

#if ENABLE(APP_BOUND_DOMAINS)
void NetworkProcessProxy::hasAppBoundSession(PAL::SessionID sessionID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler(false);
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::HasAppBoundSession(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::clearAppBoundSession(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    if (!canSendMessage()) {
        completionHandler();
        return;
    }
    
    sendWithAsyncReply(Messages::NetworkProcess::ClearAppBoundSession(sessionID), WTFMove(completionHandler));
}

void NetworkProcessProxy::getAppBoundDomains(PAL::SessionID sessionID, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    if (auto* store = websiteDataStoreFromSessionID(sessionID)) {
        store->getAppBoundDomains([completionHandler = WTFMove(completionHandler)] (auto& appBoundDomains) mutable {
            auto appBoundDomainsCopy = appBoundDomains;
            completionHandler(WTFMove(appBoundDomainsCopy));
        });
        return;
    }
    completionHandler({ });
}
#endif

void NetworkProcessProxy::updateBundleIdentifier(const String& bundleIdentifier, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::UpdateBundleIdentifier(bundleIdentifier), WTFMove(completionHandler));
}

void NetworkProcessProxy::clearBundleIdentifier(CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::ClearBundleIdentifier(), WTFMove(completionHandler));
}

#if USE(SOUP)
void NetworkProcessProxy::didExceedMemoryLimit()
{
    AuxiliaryProcessProxy::terminate();
    if (auto* connection = this->connection())
        connection->invalidate();
    networkProcessDidTerminate(ProcessTerminationReason::ExceededMemoryLimit);
}
#endif

#if ENABLE(SERVICE_WORKER)
void NetworkProcessProxy::getPendingPushMessages(PAL::SessionID sessionID, CompletionHandler<void(const Vector<WebPushMessage>&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::GetPendingPushMessages { sessionID }, WTFMove(completionHandler));
}

void NetworkProcessProxy::processPushMessage(PAL::SessionID sessionID, const WebPushMessage& pushMessage, CompletionHandler<void(bool wasProcessed)>&& callback)
{
    auto permission = PushPermissionState::Prompt;
    auto permissions = WebNotificationManagerProxy::sharedServiceWorkerManager().notificationPermissions();
    auto origin = SecurityOriginData::fromURL(pushMessage.registrationURL).toString();

    if (auto it = permissions.find(origin); it != permissions.end())
        permission = it->value ? PushPermissionState::Granted : PushPermissionState::Denied;

    sendWithAsyncReply(Messages::NetworkProcess::ProcessPushMessage { sessionID, pushMessage, permission }, WTFMove(callback));
}

void NetworkProcessProxy::processNotificationEvent(const NotificationData& data, NotificationEventType eventType, CompletionHandler<void(bool wasProcessed)>&& callback)
{
    RELEASE_ASSERT(!!callback);
    sendWithAsyncReply(Messages::NetworkProcess::ProcessNotificationEvent { data, eventType }, WTFMove(callback));
}
#endif // ENABLE(SERVICE_WORKER)

void NetworkProcessProxy::setPushAndNotificationsEnabledForOrigin(PAL::SessionID sessionID, const SecurityOriginData& origin, bool enabled, CompletionHandler<void()>&& callback)
{
    sendWithAsyncReply(Messages::NetworkProcess::SetPushAndNotificationsEnabledForOrigin { sessionID, origin, enabled }, WTFMove(callback));
}

void NetworkProcessProxy::deletePushAndNotificationRegistration(PAL::SessionID sessionID, const SecurityOriginData& origin, CompletionHandler<void(const String&)>&& callback)
{
    sendWithAsyncReply(Messages::NetworkProcess::DeletePushAndNotificationRegistration { sessionID, origin }, WTFMove(callback));
}

void NetworkProcessProxy::getOriginsWithPushAndNotificationPermissions(PAL::SessionID sessionID, CompletionHandler<void(const Vector<SecurityOriginData>&)>&& callback)
{
    sendWithAsyncReply(Messages::NetworkProcess::GetOriginsWithPushAndNotificationPermissions { sessionID }, WTFMove(callback));
}

void NetworkProcessProxy::hasPushSubscriptionForTesting(PAL::SessionID sessionID, const URL& scopeURL, CompletionHandler<void(bool)>&& callback)
{
    sendWithAsyncReply(Messages::NetworkProcess::HasPushSubscriptionForTesting { sessionID, scopeURL }, WTFMove(callback));
}

void NetworkProcessProxy::terminateRemoteWorkerContextConnectionWhenPossible(RemoteWorkerType workerType, PAL::SessionID sessionID, const WebCore::RegistrableDomain& registrableDomain, WebCore::ProcessIdentifier processIdentifier)
{
    send(Messages::NetworkProcess::TerminateRemoteWorkerContextConnectionWhenPossible(workerType, sessionID, registrableDomain, processIdentifier), 0);
}

void NetworkProcessProxy::openWindowFromServiceWorker(PAL::SessionID sessionID, const String& urlString, const WebCore::SecurityOriginData& serviceWorkerOrigin, CompletionHandler<void(std::optional<WebCore::PageIdentifier>&&)>&& callback)
{
    if (auto* store = websiteDataStoreFromSessionID(sessionID)) {
        store->openWindowFromServiceWorker(urlString, serviceWorkerOrigin, WTFMove(callback));
        return;
    }

    callback(std::nullopt);
}

void NetworkProcessProxy::navigateServiceWorkerClient(WebCore::FrameIdentifier frameIdentifier, WebCore::ScriptExecutionContextIdentifier documentIdentifier, const URL& url, CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebCore::FrameIdentifier>)>&& callback)
{
    auto* process = WebProcessProxy::processForIdentifier(documentIdentifier.processIdentifier());
    if (!process)
        return callback({ }, { });
    if (auto* frame = WebFrameProxy::webFrame(frameIdentifier))
        return frame->navigateServiceWorkerClient(documentIdentifier, url, WTFMove(callback));
    callback({ }, { });
}

void NetworkProcessProxy::applicationDidEnterBackground()
{
    send(Messages::NetworkProcess::ApplicationDidEnterBackground(), 0);
}

void NetworkProcessProxy::applicationWillEnterForeground()
{
    send(Messages::NetworkProcess::ApplicationWillEnterForeground(), 0);
}

void NetworkProcessProxy::cookiesDidChange(PAL::SessionID sessionID)
{
    if (auto* websiteDataStore = websiteDataStoreFromSessionID(sessionID))
        websiteDataStore->cookieStore().cookiesDidChange();
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

void NetworkProcessProxy::setEmulatedConditions(PAL::SessionID sessionID, std::optional<int64_t>&& bytesPerSecondLimit)
{
    if (!canSendMessage())
        return;

    send(Messages::NetworkProcess::SetEmulatedConditions(sessionID, WTFMove(bytesPerSecondLimit)), 0);
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

} // namespace WebKit

#undef MESSAGE_CHECK
