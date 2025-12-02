/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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
#include "NetworkConnectionToWebProcess.h"

#include "BlobDataFileReferenceWithSandboxExtension.h"
#include "LogInitialization.h"
#include "Logging.h"
#include "NetworkBroadcastChannelRegistry.h"
#include "NetworkBroadcastChannelRegistryMessages.h"
#include "NetworkCache.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkLoad.h"
#include "NetworkLoadScheduler.h"
#include "NetworkMDNSRegisterMessages.h"
#include "NetworkOriginAccessPatterns.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "NetworkProcessConnectionParameters.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkRTCMonitorMessages.h"
#include "NetworkRTCProviderMessages.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoader.h"
#include "NetworkResourceLoaderMessages.h"
#include "NetworkSchemeRegistry.h"
#include "NetworkSession.h"
#include "NetworkSocketChannel.h"
#include "NetworkSocketChannelMessages.h"
#include "NetworkStorageManager.h"
#include "NetworkTransportSession.h"
#include "NetworkTransportSessionMessages.h"
#include "NotificationManagerMessageHandlerMessages.h"
#include "PingLoad.h"
#include "PreconnectTask.h"
#include "RTCDataChannelRemoteManagerProxy.h"
#include "RemoteWorkerType.h"
#include "ServiceWorkerFetchTaskMessages.h"
#include "WebErrors.h"
#include "WebProcessMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebSWServerConnection.h"
#include "WebSWServerConnectionMessages.h"
#include "WebSWServerToContextConnection.h"
#include "WebSWServerToContextConnectionMessages.h"
#include "WebSharedWorkerServer.h"
#include "WebSharedWorkerServerConnection.h"
#include "WebSharedWorkerServerConnectionMessages.h"
#include "WebSharedWorkerServerToContextConnection.h"
#include "WebSharedWorkerServerToContextConnectionMessages.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/Cookie.h>
#include <WebCore/CookieJar.h>
#include <WebCore/CookieStoreGetOptions.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/LoginStatus.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Quirks.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoadObserver.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityPolicy.h>
#include <optional>
#include <wtf/HashSet.h>
#include <wtf/LogInitialization.h>

#if PLATFORM(COCOA)
#include <wtf/OSObjectPtr.h>
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
#include "WebPaymentCoordinatorProxyMessages.h"
#endif

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
#include "LegacyCustomProtocolManager.h"
#endif

#if ENABLE(IPC_TESTING_API)
#include "IPCTesterMessages.h"
#endif

#if ENABLE(CONTENT_FILTERING)
#include <WebCore/MockContentFilterSettings.h>
#endif

#if ENABLE(CONTENT_EXTENSIONS)
#include <WebCore/ResourceMonitorThrottlerHolder.h>
#endif

#if USE(LIBRICE)
#include "RiceBackendMessages.h"
#endif

#define CONNECTION_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [webProcessIdentifier=%" PRIu64 "] NetworkConnectionToWebProcess::" fmt, this, this->webProcessIdentifier().toUInt64(), ##__VA_ARGS__)
#define CONNECTION_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [webProcessIdentifier=%" PRIu64 "] NetworkConnectionToWebProcess::" fmt, this, this->webProcessIdentifier().toUInt64(), ##__VA_ARGS__)

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, this->connection())
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, this->connection(), completion)
#define MESSAGE_CHECK_WITH_RETURN_VALUE(assertion, returnValue) MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(assertion, this->connection(), returnValue)

namespace WebKit {
using namespace WebCore;

Ref<NetworkConnectionToWebProcess> NetworkConnectionToWebProcess::create(NetworkProcess& networkProcess, WebCore::ProcessIdentifier webProcessIdentifier, PAL::SessionID sessionID, NetworkProcessConnectionParameters&& parameters, IPC::Connection::Identifier&& connectionIdentifier)
{
    return adoptRef(*new NetworkConnectionToWebProcess(networkProcess, webProcessIdentifier, sessionID, WTFMove(parameters), WTFMove(connectionIdentifier)));
}

NetworkConnectionToWebProcess::NetworkConnectionToWebProcess(NetworkProcess& networkProcess, WebCore::ProcessIdentifier webProcessIdentifier, PAL::SessionID sessionID, NetworkProcessConnectionParameters&& parameters, IPC::Connection::Identifier&& connectionIdentifier)
    : m_connection(IPC::Connection::createServerConnection(WTFMove(connectionIdentifier)))
    , m_networkProcess(networkProcess)
    , m_sessionID(sessionID)
    , m_networkResourceLoaders([this](bool hasUpload) { hasUploadStateChanged(hasUpload); })
#if ENABLE(WEB_RTC)
    , m_mdnsRegister(*this)
#endif
    , m_webProcessIdentifier(webProcessIdentifier)
    , m_schemeRegistry(NetworkSchemeRegistry::create())
    , m_originAccessPatterns(makeUniqueRef<NetworkOriginAccessPatterns>())
    , m_sharedPreferencesForWebProcess(parameters.sharedPreferencesForWebProcess)
#if ENABLE(IPC_TESTING_API)
    , m_ipcTester(IPCTester::create())
#endif
{
    RELEASE_ASSERT(RunLoop::isMain());

    // Use this flag to force synchronous messages to be treated as asynchronous messages in the WebProcess.
    // Otherwise, the WebProcess would process incoming synchronous IPC while waiting for a synchronous IPC
    // reply from the Network process, which would be unsafe.
    Ref protectedConnection = m_connection;
    protectedConnection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    protectedConnection->open(*this);
#if USE(RUNNINGBOARD)
    protectedConnection->setOutgoingMessageQueueIsGrowingLargeCallback([weakThis = WeakPtr { *this }] {
        ensureOnMainRunLoop([weakThis] {
            if (weakThis)
                weakThis->m_networkProcess->protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::WakeUpWebProcessForIPC(weakThis->m_webProcessIdentifier), 0);
        });
    });
#endif

    establishSWServerConnection();
    establishSharedWorkerServerConnection();

#if !PLATFORM(WATCHOS)
    if (!m_sharedPreferencesForWebProcess.webSocketEnabled)
        CONNECTION_RELEASE_LOG(IPC, "NetworkConnectionToWebProcess: webSocketEnabled is false (version=%" PRIu64 ")", m_sharedPreferencesForWebProcess.version);
#endif
}

NetworkConnectionToWebProcess::~NetworkConnectionToWebProcess()
{
    RELEASE_ASSERT(RunLoop::isMain());

    m_connection->invalidate();

    // This may call hasUploadStateChanged().
    m_networkResourceLoaders.clear();

    for (auto& port : m_processEntangledPorts)
        m_networkProcess->checkedMessagePortChannelRegistry()->didCloseMessagePort(port);

    auto completionHandlers = std::exchange(m_messageBatchDeliveryCompletionHandlers, { });
    for (auto& completionHandler : completionHandlers.values())
        completionHandler();

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    if (CheckedPtr networkStorageSession = storageSession())
        networkStorageSession->stopListeningForCookieChangeNotifications(*this, m_hostsWithCookieListeners);
#endif
    if (CheckedPtr networkStorageSession = storageSession())
        networkStorageSession->removeCookiesEnabledStateObserver(*this);

    if (CheckedPtr networkSession = this->networkSession()) {
        if (RefPtr resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->stopListeningForStorageAccessPermissionChanges(*this);
    }

#if USE(LIBWEBRTC)
    if (RefPtr rtcProvider = m_rtcProvider)
        rtcProvider->close();
#endif
#if ENABLE(WEB_RTC)
    unregisterToRTCDataChannelProxy();
#endif

    unregisterSWConnection();
    unregisterSharedWorkerConnection();
}

void NetworkConnectionToWebProcess::hasUploadStateChanged(bool hasUpload)
{
    CONNECTION_RELEASE_LOG(Loading, "hasUploadStateChanged: (hasUpload=%d)", hasUpload);
    m_networkProcess->protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::SetWebProcessHasUploads(m_webProcessIdentifier, hasUpload), 0);
}

void NetworkConnectionToWebProcess::loadImageForDecoding(WebCore::ResourceRequest&& request, WebPageProxyIdentifier pageID, uint64_t maximumBytesFromNetwork, CompletionHandler<void(Expected<Ref<WebCore::FragmentedSharedBuffer>, WebCore::ResourceError>&&)>&& completionHandler)
{
    auto url = request.url();
    MESSAGE_CHECK_COMPLETION(url.isValid(), completionHandler(makeUnexpected<WebCore::ResourceError>({ })));
    CheckedPtr networkSession = this->networkSession();
    if (!networkSession)
        return completionHandler(makeUnexpected<WebCore::ResourceError>({ }));
    networkSession->loadImageForDecoding(WTFMove(request), pageID, maximumBytesFromNetwork, WTFMove(completionHandler));
}

void NetworkConnectionToWebProcess::didCleanupResourceLoader(NetworkResourceLoader& loader)
{
    RELEASE_ASSERT(RunLoop::isMain());

    if (loader.isKeptAlive()) {
        m_networkProcess->removeKeptAliveLoad(loader);
        return;
    }

    m_networkResourceLoaders.remove(loader.coreIdentifier());
}

void NetworkConnectionToWebProcess::transferKeptAliveLoad(NetworkResourceLoader& loader)
{
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(loader.isKeptAlive());
    ASSERT(m_networkResourceLoaders.get(loader.coreIdentifier()) == &loader);
    if (auto takenLoader = m_networkResourceLoaders.take(loader.coreIdentifier()))
        m_networkProcess->addKeptAliveLoad(takenLoader.releaseNonNull());
}

bool NetworkConnectionToWebProcess::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    ASSERT_WITH_SECURITY_IMPLICATION(RunLoop::isMain());

    // For security reasons, Messages::NetworkProcess IPC is only supposed to come from the UIProcess.
    MESSAGE_CHECK_WITH_RETURN_VALUE(decoder.messageReceiverName() != Messages::NetworkProcess::messageReceiverName(), false);

    if (decoder.messageReceiverName() == Messages::NetworkBroadcastChannelRegistry::messageReceiverName()) {
        if (CheckedPtr networkSession = this->networkSession())
            networkSession->broadcastChannelRegistry().didReceiveMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::NetworkResourceLoader::messageReceiverName()) {
        MESSAGE_CHECK_WITH_RETURN_VALUE(AtomicObjectIdentifier<WebCore::ResourceLoaderIdentifierType>::isValidIdentifier(decoder.destinationID()), false);
        if (RefPtr loader = m_networkResourceLoaders.get(AtomicObjectIdentifier<WebCore::ResourceLoaderIdentifierType>(decoder.destinationID())))
            loader->didReceiveMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::NetworkSocketChannel::messageReceiverName()) {
        // FIXME: Replace this with message endpoint enablement check after root cause of rdar://problem/142320806 is fixed.
        if (!m_sharedPreferencesForWebProcess.webSocketEnabled) {
            CONNECTION_RELEASE_LOG_ERROR(IPC, "dispatchMessage: ignoring message '%s' as webSocketEnabled is false (version=%" PRIu64 ")", IPC::description(decoder.messageName()).characters(), m_sharedPreferencesForWebProcess.version);
            ASSERT_NOT_REACHED();
            return true;
        }

        MESSAGE_CHECK_WITH_RETURN_VALUE(AtomicObjectIdentifier<WebSocketIdentifierType>::isValidIdentifier(decoder.destinationID()), false);
        if (RefPtr channel = m_networkSocketChannels.get(AtomicObjectIdentifier<WebSocketIdentifierType>(decoder.destinationID())))
            channel->didReceiveMessage(connection, decoder);
        return true;
    }

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    if (decoder.messageReceiverName() == Messages::NotificationManagerMessageHandler::messageReceiverName()) {
        MESSAGE_CHECK_WITH_RETURN_VALUE(builtInNotificationsEnabled(), false);
        if (CheckedPtr networkSession = this->networkSession())
            networkSession->notificationManager().didReceiveMessage(connection, decoder);
        return true;
    }
#endif
#if USE(LIBWEBRTC)
    if (decoder.messageReceiverName() == Messages::NetworkRTCMonitor::messageReceiverName()) {
        protectedRTCProvider()->didReceiveNetworkRTCMonitorMessage(connection, decoder);
        return true;
    }
#endif
#if ENABLE(WEB_RTC)
    if (decoder.messageReceiverName() == Messages::NetworkMDNSRegister::messageReceiverName()) {
        protectedMDNSRegister()->didReceiveMessage(connection, decoder);
        return true;
    }
#endif

    if (decoder.messageReceiverName() == Messages::NetworkTransportSession::messageReceiverName()) {
        MESSAGE_CHECK_WITH_RETURN_VALUE(WebTransportSessionIdentifier::isValidIdentifier(decoder.destinationID()), false);
        if (RefPtr networkTransportSession = m_networkTransportSessions.get(WebTransportSessionIdentifier(decoder.destinationID())))
            networkTransportSession->didReceiveMessage(connection, decoder);
        return true;
    }

#if USE(LIBRICE)
    if (decoder.messageReceiverName() == Messages::RiceBackend::messageReceiverName()) {
        MESSAGE_CHECK_WITH_RETURN_VALUE(RiceBackendIdentifier::isValidIdentifier(decoder.destinationID()), false);
        if (RefPtr iceBackend = m_gstreamerIceBackends.get(RiceBackendIdentifier(decoder.destinationID())))
            iceBackend->didReceiveMessage(connection, decoder);
        return true;
    }
#endif

    if (decoder.messageReceiverName() == Messages::WebSWServerConnection::messageReceiverName()) {
        if (RefPtr swConnection = m_swConnection.get())
            swConnection->didReceiveMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebSWServerToContextConnection::messageReceiverName()) {
        if (RefPtr swContextConnection = m_swContextConnection)
            swContextConnection->didReceiveMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::ServiceWorkerFetchTask::messageReceiverName()) {
        if (RefPtr swContextConnection = m_swContextConnection)
            swContextConnection->didReceiveFetchTaskMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::WebSharedWorkerServerConnection::messageReceiverName()) {
        if (RefPtr sharedWorkerConnection = m_sharedWorkerConnection.get())
            sharedWorkerConnection->didReceiveMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebSharedWorkerServerToContextConnection::messageReceiverName()) {
        if (RefPtr sharedWorkerContextConnection = m_sharedWorkerContextConnection)
            sharedWorkerContextConnection->didReceiveMessage(connection, decoder);
        return true;
    }


#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinatorProxy::messageReceiverName()) {
        paymentCoordinator().didReceiveMessage(connection, decoder);
        return true;
    }
#endif

#if ENABLE(IPC_TESTING_API)
    if (decoder.messageReceiverName() == Messages::IPCTester::messageReceiverName()) {
        m_ipcTester->didReceiveMessage(connection, decoder);
        return true;
    }
#endif
    return false;
}

#if USE(LIBWEBRTC)
NetworkRTCProvider& NetworkConnectionToWebProcess::rtcProvider()
{
    if (!m_rtcProvider)
        m_rtcProvider = NetworkRTCProvider::create(*this);
    return *m_rtcProvider;
}
#endif

void NetworkConnectionToWebProcess::createRTCProvider(CompletionHandler<void()>&& callback)
{
#if USE(LIBWEBRTC)
    rtcProvider();
#endif
    callback();
}

#if ENABLE(WEB_RTC)
void NetworkConnectionToWebProcess::connectToRTCDataChannelRemoteSource(WebCore::RTCDataChannelIdentifier localIdentifier, WebCore::RTCDataChannelIdentifier remoteIdentifier, CompletionHandler<void(std::optional<bool>)>&& callback)
{
    RefPtr connectionToWebProcess = m_networkProcess->webProcessConnection(remoteIdentifier.processIdentifier());
    if (!connectionToWebProcess) {
        callback(false);
        return;
    }
    registerToRTCDataChannelProxy();
    connectionToWebProcess->registerToRTCDataChannelProxy();
    connectionToWebProcess->m_connection->sendWithAsyncReply(Messages::NetworkProcessConnection::ConnectToRTCDataChannelRemoteSource { remoteIdentifier, localIdentifier }, WTFMove(callback), 0);
}

void NetworkConnectionToWebProcess::registerToRTCDataChannelProxy()
{
    if (m_isRegisteredToRTCDataChannelProxy)
        return;
    m_isRegisteredToRTCDataChannelProxy = true;
    m_networkProcess->protectedRTCDataChannelProxy()->registerConnectionToWebProcess(*this);
}

void NetworkConnectionToWebProcess::unregisterToRTCDataChannelProxy()
{
    if (m_isRegisteredToRTCDataChannelProxy)
        m_networkProcess->protectedRTCDataChannelProxy()->unregisterConnectionToWebProcess(*this);
}
#endif

bool NetworkConnectionToWebProcess::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& reply)
{
    // For security reasons, Messages::NetworkProcess IPC is only supposed to come from the UIProcess.
    MESSAGE_CHECK_WITH_RETURN_VALUE(decoder.messageReceiverName() != Messages::NetworkProcess::messageReceiverName(), false);

    if (decoder.messageReceiverName() == Messages::WebSWServerConnection::messageReceiverName()) {
        if (RefPtr swConnection = m_swConnection.get())
            swConnection->didReceiveSyncMessage(connection, decoder, reply);
        return true;
    }

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    if (decoder.messageReceiverName() == Messages::NotificationManagerMessageHandler::messageReceiverName()) {
        MESSAGE_CHECK_WITH_RETURN_VALUE(builtInNotificationsEnabled(), false);
        if (CheckedPtr networkSession = this->networkSession())
            networkSession->notificationManager().didReceiveSyncMessage(connection, decoder, reply);
        return true;
    }
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinatorProxy::messageReceiverName()) {
        paymentCoordinator().didReceiveSyncMessage(connection, decoder, reply);
        return true;
    }
#endif

#if ENABLE(IPC_TESTING_API)
    if (decoder.messageReceiverName() == Messages::IPCTester::messageReceiverName()) {
        m_ipcTester->didReceiveSyncMessage(connection, decoder, reply);
        return true;
    }
#endif

#if USE(LIBRICE)
    if (decoder.messageReceiverName() == Messages::RiceBackend::messageReceiverName()) {
        if (RefPtr iceBackend = m_gstreamerIceBackends.get(RiceBackendIdentifier(decoder.destinationID()))) {
            iceBackend->didReceiveSyncMessage(connection, decoder, reply);
            return true;
        }
    }
#endif

    return false;
}

void NetworkConnectionToWebProcess::didClose(IPC::Connection& connection)
{
    if (RefPtr connection = std::exchange(m_swContextConnection, nullptr))
        connection->stop();

    m_sharedWorkerContextConnection = nullptr;

    // Protect ourself as we might be otherwise be deleted during this function.
    Ref<NetworkConnectionToWebProcess> protector(*this);

#if OS(DARWIN)
    CONNECTION_RELEASE_LOG(Loading, "didClose: WebProcess (%d) closed its connection. Aborting related loaders.", connection.remoteProcessID());
#else
    CONNECTION_RELEASE_LOG(Loading, "didClose: WebProcess closed its connection. Aborting related loaders.");
#endif

    while (!m_networkResourceLoaders.isEmpty())
        Ref { m_networkResourceLoaders.begin()->value }->abort();

    if (CheckedPtr networkSession = this->networkSession()) {
        networkSession->broadcastChannelRegistry().removeConnection(connection);
        for (auto& [url, topOrigin] : m_blobURLs)
            networkSession->blobRegistry().unregisterBlobURL(url, topOrigin);
        for (auto& [urlAndOrigin, count] : m_blobURLHandles) {
            auto& [url, topOrigin] = urlAndOrigin;
            for (unsigned i = 0; i < count; ++i)
                networkSession->blobRegistry().unregisterBlobURLHandle(url, topOrigin);
        }
    }

    // All trackers of resources that were in the middle of being loaded were
    // stopped with the abort() calls above, but we still need to sweep up the
    // root activity trackers.
    stopAllNetworkActivityTracking();

    Ref networkProcess = m_networkProcess.get();
    networkProcess->connectionToWebProcessClosed(connection, m_sessionID);
    networkProcess->removeNetworkConnectionToWebProcess(*this);

#if USE(LIBWEBRTC)
    if (RefPtr rtcProvider = m_rtcProvider) {
        rtcProvider->close();
        m_rtcProvider = nullptr;
    }
#endif

    unregisterSWConnection();
    unregisterSharedWorkerConnection();

#if ENABLE(APPLE_PAY_REMOTE_UI)
    m_paymentCoordinator = nullptr;
#endif
}

void NetworkConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName messageName, const Vector<uint32_t>&)
{
    RELEASE_LOG_FAULT(IPC, "Received an invalid message '%" PUBLIC_LOG_STRING "' from WebContent process %" PRIu64 ", requesting for it to be terminated.", description(messageName).characters(), m_webProcessIdentifier.toUInt64());
    m_networkProcess->protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::TerminateWebProcess(m_webProcessIdentifier), 0);
}

void NetworkConnectionToWebProcess::createSocketChannel(const ResourceRequest& request, const String& protocol, WebSocketIdentifier identifier, WebPageProxyIdentifier webPageProxyID, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, const ClientOrigin& clientOrigin, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, OptionSet<AdvancedPrivacyProtections> advancedPrivacyProtections, WebCore::StoredCredentialsPolicy storedCredentialsPolicy)
{
    MESSAGE_CHECK(m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, request.firstPartyForCookies()) != NetworkProcess::AllowCookieAccess::Terminate);

    ASSERT(!m_networkSocketChannels.contains(identifier));
    if (auto channel = NetworkSocketChannel::create(*this, m_sessionID, request, protocol, identifier, webPageProxyID, frameID, pageID, clientOrigin, hadMainFrameMainResourcePrivateRelayed, allowPrivacyProxy, advancedPrivacyProtections, storedCredentialsPolicy))
        m_networkSocketChannels.add(identifier, WTFMove(channel));
}

void NetworkConnectionToWebProcess::removeSocketChannel(WebSocketIdentifier identifier)
{
    ASSERT(m_networkSocketChannels.contains(identifier));
    m_networkSocketChannels.remove(identifier);
}

NetworkSession* NetworkConnectionToWebProcess::networkSession()
{
    return m_networkProcess->networkSession(m_sessionID);
}

Vector<RefPtr<WebCore::BlobDataFileReference>> NetworkConnectionToWebProcess::resolveBlobReferences(const NetworkResourceLoadParameters& loadParameters)
{
    CONNECTION_RELEASE_LOG(Loading, "resolveBlobReferences: (parentPID=%d, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ")", loadParameters.parentPID, loadParameters.webPageProxyID.toUInt64(), loadParameters.webPageID.toUInt64(), loadParameters.webFrameID.toUInt64(), loadParameters.identifier ? loadParameters.identifier->toUInt64() : 0);

    CheckedPtr session = networkSession();
    if (!session)
        return { };

    auto& blobRegistry = session->blobRegistry();

    Vector<RefPtr<WebCore::BlobDataFileReference>> files;
    if (auto body = loadParameters.request.httpBody()) {
        for (auto& element : body->elements()) {
            if (auto* blobData = std::get_if<FormDataElement::EncodedBlobData>(&element.data))
                files.appendVector(blobRegistry.filesInBlob(blobData->url));
        }
        const_cast<WebCore::ResourceRequest&>(loadParameters.request).setHTTPBody(body->resolveBlobReferences(&blobRegistry));
    }

    return files;
}

RefPtr<ServiceWorkerFetchTask> NetworkConnectionToWebProcess::createFetchTask(NetworkResourceLoader& loader, const ResourceRequest& request)
{
    RefPtr swConnection = this->swConnection();
    return swConnection ? swConnection->createFetchTask(loader, request) : nullptr;
}

void NetworkConnectionToWebProcess::scheduleResourceLoad(NetworkResourceLoadParameters&& loadParameters, std::optional<NetworkResourceLoadIdentifier> existingLoaderToResume)
{
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, loadParameters.request.firstPartyForCookies());
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow) [[unlikely]]
        RELEASE_LOG_ERROR(Loading, "scheduleResourceLoad: Web process does not have cookie access to url %" SENSITIVE_LOG_STRING " for request %" SENSITIVE_LOG_STRING, loadParameters.request.firstPartyForCookies().string().utf8().data(), loadParameters.request.url().string().utf8().data());

    MESSAGE_CHECK(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate);

    CONNECTION_RELEASE_LOG(Loading, "scheduleResourceLoad: (parentPID=%d, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ", existingLoaderToResume=%" PRIu64 ")", loadParameters.parentPID, loadParameters.webPageProxyID.toUInt64(), loadParameters.webPageID.toUInt64(), loadParameters.webFrameID.toUInt64(), loadParameters.identifier ? loadParameters.identifier->toUInt64() : 0, existingLoaderToResume ? existingLoaderToResume->toUInt64() : 0);

    if (CheckedPtr session = networkSession()) {
        if (Ref server = session->ensureSWServer(); !server->isImportCompleted()) {
            server->whenImportIsCompleted([this, protectedThis = Ref { *this }, loadParameters = WTFMove(loadParameters), existingLoaderToResume]() mutable {
                if (!m_networkProcess->webProcessConnection(webProcessIdentifier()))
                    return;

                ASSERT(networkSession());
                ASSERT(networkSession()->swServer());
                ASSERT(networkSession()->swServer()->isImportCompleted());
                scheduleResourceLoad(WTFMove(loadParameters), existingLoaderToResume);
            });
            return;
        }
    }

    auto identifier = loadParameters.identifier;
    RELEASE_ASSERT(identifier);
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(!m_networkResourceLoaders.contains(*identifier));

    if (existingLoaderToResume) {
        if (CheckedPtr session = networkSession()) {
            if (auto existingLoader = session->takeLoaderAwaitingWebProcessTransfer(*existingLoaderToResume)) {
                CONNECTION_RELEASE_LOG(Loading, "scheduleResourceLoad: Resuming existing NetworkResourceLoader");
                m_networkResourceLoaders.add(*identifier, *existingLoader);
                existingLoader->transferToNewWebProcess(*this, loadParameters);
                return;
            }
            CONNECTION_RELEASE_LOG_ERROR(Loading, "scheduleResourceLoad: Could not find existing NetworkResourceLoader to resume, will do a fresh load");
        } else
            CONNECTION_RELEASE_LOG_ERROR(Loading, "scheduleResourceLoad: Could not find network session of existing NetworkResourceLoader to resume, will do a fresh load");
    }

    if (loadParameters.shouldRecordFrameLoadForStorageAccess && loadParameters.mainResourceNavigationDataForAnyFrame) {
        if (CheckedPtr session = networkSession()) {
            if (RefPtr resourceLoadStatistics = session->resourceLoadStatistics())
                resourceLoadStatistics->recordFrameLoadForStorageAccess(loadParameters.webPageProxyID, loadParameters.webFrameID, RegistrableDomain { loadParameters.request.url() });
        }
    }

    auto& loader = m_networkResourceLoaders.add(*identifier, NetworkResourceLoader::create(WTFMove(loadParameters), *this)).iterator->value;

    loader->startWithServiceWorker();
}

void NetworkConnectionToWebProcess::performSynchronousLoad(NetworkResourceLoadParameters&& loadParameters, CompletionHandler<void(const ResourceError&, const ResourceResponse, Vector<uint8_t>&&)>&& reply)
{
    MESSAGE_CHECK(m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, loadParameters.request.firstPartyForCookies()) == NetworkProcess::AllowCookieAccess::Allow);
    CONNECTION_RELEASE_LOG(Loading, "performSynchronousLoad: (parentPID=%d, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ")", loadParameters.parentPID, loadParameters.webPageProxyID.toUInt64(), loadParameters.webPageID.toUInt64(), loadParameters.webFrameID.toUInt64(), loadParameters.identifier ? loadParameters.identifier->toUInt64() : 0);

    auto identifier = loadParameters.identifier;
    RELEASE_ASSERT(identifier);
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(!m_networkResourceLoaders.contains(*identifier));

    auto loader = NetworkResourceLoader::create(WTFMove(loadParameters), *this, WTFMove(reply));
    m_networkResourceLoaders.add(*identifier, loader.copyRef());
    loader->start();
}

void NetworkConnectionToWebProcess::testProcessIncomingSyncMessagesWhenWaitingForSyncReply(WebPageProxyIdentifier pageID, CompletionHandler<void(bool)>&& reply)
{
    auto syncResult = m_networkProcess->protectedParentProcessConnection()->sendSync(Messages::NetworkProcessProxy::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply(pageID), 0);
    auto [handled] = syncResult.takeReplyOr(false);
    reply(handled);
}

void NetworkConnectionToWebProcess::loadPing(NetworkResourceLoadParameters&& loadParameters)
{
    CONNECTION_RELEASE_LOG(Loading, "loadPing: (parentPID=%d, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ")", loadParameters.parentPID, loadParameters.webPageProxyID.toUInt64(), loadParameters.webPageID.toUInt64(), loadParameters.webFrameID.toUInt64(), loadParameters.identifier ? loadParameters.identifier->toUInt64() : 0);

    auto completionHandler = [connection = m_connection, identifier = *loadParameters.identifier] (const ResourceError& error, const ResourceResponse& response) {
        connection->send(Messages::NetworkProcessConnection::DidFinishPingLoad(identifier, error, response), 0);
    };

    // PingLoad manages its own lifetime, derefing itself when its purpose has been fulfilled.
    PingLoad::create(*this, WTFMove(loadParameters), WTFMove(completionHandler));
}

void NetworkConnectionToWebProcess::setOnLineState(bool isOnLine)
{
    m_connection->send(Messages::NetworkProcessConnection::SetOnLineState(isOnLine), 0);
}

void NetworkConnectionToWebProcess::cookieAcceptPolicyChanged(HTTPCookieAcceptPolicy newPolicy)
{
    m_connection->send(Messages::NetworkProcessConnection::CookieAcceptPolicyChanged(newPolicy), 0);
}

void NetworkConnectionToWebProcess::removeLoadIdentifier(WebCore::ResourceLoaderIdentifier identifier)
{
    RELEASE_ASSERT(RunLoop::isMain());

    RefPtr<NetworkResourceLoader> loader = m_networkResourceLoaders.get(identifier);

    // It's possible we have no loader for this identifier if the NetworkProcess crashed and this was a respawned NetworkProcess.
    if (!loader)
        return;

    // Abort the load now, as the WebProcess won't be able to respond to messages any more which might lead
    // to leaked loader resources (connections, threads, etc).
    CONNECTION_RELEASE_LOG(Loading, "removeLoadIdentifier: Removing identifier %" PRIu64 " and aborting corresponding loader", identifier.toUInt64());
    loader->abort();
    ASSERT(!m_networkResourceLoaders.contains(identifier));
}

void NetworkConnectionToWebProcess::pageLoadCompleted(PageIdentifier webPageID)
{
    stopAllNetworkActivityTrackingForPage(webPageID);
}

void NetworkConnectionToWebProcess::browsingContextRemoved(WebPageProxyIdentifier webPageProxyID, PageIdentifier webPageID, FrameIdentifier webFrameID)
{
    if (CheckedPtr session = m_networkProcess->networkSession(sessionID())) {
        if (RefPtr cache = session->cache())
            cache->browsingContextRemoved(webPageProxyID, webPageID, webFrameID);
    }
}

void NetworkConnectionToWebProcess::prefetchDNS(const String& hostname)
{
    m_networkProcess->prefetchDNS(hostname);
}

void NetworkConnectionToWebProcess::sendH2Ping(NetworkResourceLoadParameters&& parameters, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler)
{
#if ENABLE(SERVER_PRECONNECT)
    CheckedPtr networkSession = this->networkSession();
    if (!networkSession)
        return completionHandler(makeUnexpected(internalError(parameters.request.url())));

    URL url = parameters.request.url();
    Ref task = PreconnectTask::create(*networkSession, parameters.networkLoadParameters());
    task->setH2PingCallback(url, WTFMove(completionHandler));
    task->start();
#else
    ASSERT_NOT_REACHED();
    completionHandler(makeUnexpected(internalError(parameters.request.url())));
#endif
}

void NetworkConnectionToWebProcess::preconnectTo(std::optional<WebCore::ResourceLoaderIdentifier> preconnectionIdentifier, NetworkResourceLoadParameters&& loadParameters)
{
    CONNECTION_RELEASE_LOG(Loading, "preconnectTo: (parentPID=%d, pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ")", loadParameters.parentPID, loadParameters.webPageProxyID.toUInt64(), loadParameters.webPageID.toUInt64(), loadParameters.webFrameID.toUInt64(), loadParameters.identifier ? loadParameters.identifier->toUInt64() : 0);

    ASSERT(!loadParameters.request.httpBody());

    auto completionHandler = [this, protectedThis = Ref { *this }, preconnectionIdentifier = WTFMove(preconnectionIdentifier)](const ResourceError& error) {
        if (preconnectionIdentifier)
            didFinishPreconnection(*preconnectionIdentifier, error);
    };

#if ENABLE(LEGACY_CUSTOM_PROTOCOL_MANAGER)
    if (RefPtr { m_networkProcess->supplement<LegacyCustomProtocolManager>() }->supportsScheme(loadParameters.request.url().protocol().toString())) {
        completionHandler(internalError(loadParameters.request.url()));
        return;
    }
#endif

#if ENABLE(SERVER_PRECONNECT)
    CheckedPtr session = networkSession();
    if (session && session->allowsServerPreconnect()) {
        Ref preconnectTask = PreconnectTask::create(*session, loadParameters.networkLoadParameters());
        preconnectTask->start([completionHandler = WTFMove(completionHandler)] (const ResourceError& error, const WebCore::NetworkLoadMetrics&) {
            completionHandler(error);
        });
        return;
    }
#endif
    completionHandler(internalError(loadParameters.request.url()));
}

void NetworkConnectionToWebProcess::isResourceLoadFinished(WebCore::ResourceLoaderIdentifier loadIdentifier, CompletionHandler<void(bool)>&& callback)
{
    callback(!m_networkResourceLoaders.contains(loadIdentifier));
}

void NetworkConnectionToWebProcess::didFinishPreconnection(WebCore::ResourceLoaderIdentifier preconnectionIdentifier, const ResourceError& error)
{
    if (!m_connection->isValid())
        return;

    m_connection->send(Messages::NetworkProcessConnection::DidFinishPreconnection(preconnectionIdentifier, error), 0);
}

NetworkStorageSession* NetworkConnectionToWebProcess::storageSession()
{
    return m_networkProcess->storageSession(m_sessionID);
}

void NetworkConnectionToWebProcess::startDownload(DownloadID downloadID, const ResourceRequest& request, const std::optional<WebCore::SecurityOriginData>& topOrigin, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, const String& suggestedName, FromDownloadAttribute fromDownloadAttribute, std::optional<WebCore::FrameIdentifier> frameID, std::optional<WebCore::PageIdentifier> pageID)
{
    m_networkProcess->checkedDownloadManager()->startDownload(m_sessionID, downloadID, request, topOrigin, isNavigatingToAppBoundDomain, suggestedName, fromDownloadAttribute, frameID, pageID, webProcessIdentifier());
}

void NetworkConnectionToWebProcess::loadCancelledDownloadRedirectRequestInFrame(const WebCore::ResourceRequest& request, const WebCore::FrameIdentifier& frameID, const WebCore::PageIdentifier& pageID)
{
    m_connection->send(Messages::NetworkProcessConnection::LoadCancelledDownloadRedirectRequestInFrame(request, frameID, pageID), 0);
}

void NetworkConnectionToWebProcess::convertMainResourceLoadToDownload(std::optional<WebCore::ResourceLoaderIdentifier> mainResourceLoadIdentifier, DownloadID downloadID, const ResourceRequest& request, const std::optional<WebCore::SecurityOriginData>& topOrigin, const ResourceResponse& response, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain)
{
    RELEASE_ASSERT(RunLoop::isMain());

    if (!mainResourceLoadIdentifier) {
        m_networkProcess->checkedDownloadManager()->startDownload(m_sessionID, downloadID, request, topOrigin, isNavigatingToAppBoundDomain);
        return;
    }

    RefPtr loader = m_networkResourceLoaders.get(*mainResourceLoadIdentifier);
    if (!loader) {
        // If we're trying to download a blob here loader can be null.
        return;
    }

    loader->convertToDownload(downloadID, request, response);
}

void NetworkConnectionToWebProcess::registerURLSchemesAsCORSEnabled(Vector<String>&& schemes)
{
    Ref registry = m_schemeRegistry;
    for (auto&& scheme : WTFMove(schemes))
        registry->registerURLSchemeAsCORSEnabled(WTFMove(scheme));
}

static bool shouldTreatAsSameSite(const URL& firstParty, const URL& url)
{
#if PLATFORM(COCOA) || USE(SOUP)
    if (SecurityPolicy::shouldInheritSecurityOriginFromOwner(url))
        return true;

    return RegistrableDomain(firstParty) == RegistrableDomain(url);
#else
    return true;
#endif
}

void NetworkConnectionToWebProcess::cookiesForDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, FrameIdentifier frameID, PageIdentifier pageID, IncludeSecureCookies includeSecureCookies, WebPageProxyIdentifier webPageProxyID, CompletionHandler<void(String cookieString, bool secureCookiesAccessed)>&& completionHandler)
{
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, firstParty);
    MESSAGE_CHECK_COMPLETION(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate, completionHandler({ }, false));
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow)
        return completionHandler({ }, false);
    if (sameSiteInfo.isSameSite && !shouldTreatAsSameSite(firstParty, url)) {
        CONNECTION_RELEASE_LOG_ERROR(IPC, "cookiesForDOM: Rejecting cookie access due to invalid sameSiteInfo");
        return completionHandler({ }, false);
    }

    CheckedPtr networkStorageSession = storageSession();
    if (!networkStorageSession)
        return completionHandler({ }, false);
    auto result = networkStorageSession->cookiesForDOM(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, ApplyTrackingPrevention::Yes, m_networkProcess->shouldRelaxThirdPartyCookieBlockingForPage(webPageProxyID), NetworkSession::isResourceFromKnownCrossSiteTracker(firstParty, url));
#if !RELEASE_LOG_DISABLED
    if (CheckedPtr session = networkSession()) {
        if (session->shouldLogCookieInformation())
            NetworkResourceLoader::logCookieInformation(*this, "NetworkConnectionToWebProcess::cookiesForDOM"_s, reinterpret_cast<const void*>(this), *networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, std::nullopt);
    }
#endif
    completionHandler(WTFMove(result.first), result.second);
}

void NetworkConnectionToWebProcess::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, FrameIdentifier frameID, PageIdentifier pageID, const String& cookieString, RequiresScriptTrackingPrivacy requiresScriptTrackingPrivacy, WebPageProxyIdentifier webPageProxyID)
{
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, firstParty);
    MESSAGE_CHECK(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate);
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow)
        return;
    if (sameSiteInfo.isSameSite && !shouldTreatAsSameSite(firstParty, url)) {
        CONNECTION_RELEASE_LOG_ERROR(IPC, "setCookiesFromDOM: Rejecting cookie access due to invalid sameSiteInfo");
        return;
    }

    CheckedPtr networkStorageSession = storageSession();
    if (!networkStorageSession)
        return;

    networkStorageSession->setCookiesFromDOM(firstParty, sameSiteInfo, url, frameID, pageID, ApplyTrackingPrevention::Yes, requiresScriptTrackingPrivacy, cookieString, m_networkProcess->shouldRelaxThirdPartyCookieBlockingForPage(webPageProxyID), NetworkSession::isResourceFromKnownCrossSiteTracker(firstParty, url));
#if !RELEASE_LOG_DISABLED
    if (CheckedPtr session = networkSession()) {
        if (session->shouldLogCookieInformation())
            NetworkResourceLoader::logCookieInformation(*this, "NetworkConnectionToWebProcess::setCookiesFromDOM"_s, reinterpret_cast<const void*>(this), *networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, std::nullopt);
    }
#endif
}

void NetworkConnectionToWebProcess::cookiesEnabledSync(const URL& firstParty, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, WebPageProxyIdentifier webPageProxyID, CompletionHandler<void(bool)>&& completionHandler)
{
    cookiesEnabled(firstParty, url, frameID, pageID, webPageProxyID, WTFMove(completionHandler));
}

void NetworkConnectionToWebProcess::cookiesEnabled(const URL& firstParty, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, WebPageProxyIdentifier webPageProxyID, CompletionHandler<void(bool)>&& completionHandler)
{
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, firstParty);
    MESSAGE_CHECK_COMPLETION(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate, completionHandler(false));
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow)
        return completionHandler(false);

    CheckedPtr networkStorageSession = storageSession();
    if (!networkStorageSession) {
        completionHandler(false);
        return;
    }

    networkStorageSession->addCookiesEnabledStateObserver(*this);
    completionHandler(networkStorageSession->cookiesEnabled(firstParty, url, frameID, pageID, m_networkProcess->shouldRelaxThirdPartyCookieBlockingForPage(webPageProxyID), NetworkSession::isResourceFromKnownCrossSiteTracker(firstParty, url)));
}

void NetworkConnectionToWebProcess::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, std::optional<WebPageProxyIdentifier> webPageProxyID, CompletionHandler<void(String, bool)>&& completionHandler)
{
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, firstParty);
    MESSAGE_CHECK_COMPLETION(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate, completionHandler({ }, false));
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow)
        return completionHandler({ }, false);
    if (sameSiteInfo.isSameSite && !shouldTreatAsSameSite(firstParty, url)) {
        CONNECTION_RELEASE_LOG_ERROR(IPC, "cookieRequestHeaderFieldValue: Rejecting cookie access due to invalid sameSiteInfo");
        return completionHandler({ }, false);
    }

    CheckedPtr networkStorageSession = storageSession();
    if (!networkStorageSession)
        return completionHandler({ }, false);
    auto result = networkStorageSession->cookieRequestHeaderFieldValue(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, ApplyTrackingPrevention::Yes, m_networkProcess->shouldRelaxThirdPartyCookieBlockingForPage(webPageProxyID), NetworkSession::isResourceFromKnownCrossSiteTracker(firstParty, url));
    completionHandler(WTFMove(result.first), result.second);
}

void NetworkConnectionToWebProcess::getRawCookies(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, std::optional<WebPageProxyIdentifier> webPageProxyID, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&& completionHandler)
{
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, firstParty);
    MESSAGE_CHECK_COMPLETION(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate, completionHandler({ }));
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow)
        return completionHandler({ });
    if (sameSiteInfo.isSameSite && !shouldTreatAsSameSite(firstParty, url)) {
        CONNECTION_RELEASE_LOG_ERROR(IPC, "getRawCookies: Rejecting cookie access due to invalid sameSiteInfo");
        return completionHandler({ });
    }

    CheckedPtr networkStorageSession = storageSession();
    if (!networkStorageSession)
        return completionHandler({ });
    Vector<WebCore::Cookie> result;
    networkStorageSession->getRawCookies(firstParty, sameSiteInfo, url, frameID, pageID, ApplyTrackingPrevention::Yes, m_networkProcess->shouldRelaxThirdPartyCookieBlockingForPage(webPageProxyID), result);
    completionHandler(WTFMove(result));
}

void NetworkConnectionToWebProcess::setRawCookie(const URL& firstParty, const URL& url, const WebCore::Cookie& cookie, ShouldPartitionCookie shouldPartitionCookie)
{
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, firstParty);
    MESSAGE_CHECK(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate);
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow)
        return;

    CheckedPtr networkStorageSession = storageSession();
    if (!networkStorageSession)
        return;

#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    networkStorageSession->setCookie(firstParty, cookie, shouldPartitionCookie);
#else
    UNUSED_PARAM(firstParty);
    UNUSED_PARAM(shouldPartitionCookie);
    networkStorageSession->setCookie(cookie, url, firstParty);
#endif
}

void NetworkConnectionToWebProcess::deleteCookie(const URL& firstParty, const URL& url, const String& cookieName, CompletionHandler<void()>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!firstParty.isEmpty() && firstParty.isValid(), completionHandler());
    MESSAGE_CHECK_COMPLETION(!url.isEmpty() && url.isValid(), completionHandler());
    MESSAGE_CHECK_COMPLETION(!cookieName.isEmpty(), completionHandler());
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, firstParty);
    MESSAGE_CHECK_COMPLETION(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate, completionHandler());
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow)
        return completionHandler();

    CheckedPtr networkStorageSession = storageSession();
    if (!networkStorageSession)
        return completionHandler();
    networkStorageSession->deleteCookie(firstParty, url, cookieName, WTFMove(completionHandler));
}

void NetworkConnectionToWebProcess::cookiesForDOMAsync(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<WebCore::FrameIdentifier> frameID, std::optional<WebCore::PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, WebCore::CookieStoreGetOptions&& options, std::optional<WebPageProxyIdentifier> webPageProxyID, CompletionHandler<void(std::optional<Vector<WebCore::Cookie>>&&)>&& completionHandler)
{
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, firstParty);
    MESSAGE_CHECK_COMPLETION(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate, completionHandler(std::nullopt));
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow)
        return completionHandler(std::nullopt);
    if (sameSiteInfo.isSameSite && !shouldTreatAsSameSite(firstParty, url)) {
        CONNECTION_RELEASE_LOG_ERROR(IPC, "cookiesForDOMAsync: Rejecting cookie access due to invalid sameSiteInfo");
        return completionHandler(std::nullopt);
    }

    CheckedPtr networkStorageSession = storageSession();
    if (!networkStorageSession)
        return completionHandler(std::nullopt);
    auto result = networkStorageSession->cookiesForDOMAsVector(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, ApplyTrackingPrevention::Yes, m_networkProcess->shouldRelaxThirdPartyCookieBlockingForPage(webPageProxyID), NetworkSession::isResourceFromKnownCrossSiteTracker(firstParty, url), WTFMove(options));
#if !RELEASE_LOG_DISABLED
    if (CheckedPtr session = networkSession()) {
        if (session->shouldLogCookieInformation())
            NetworkResourceLoader::logCookieInformation(*this, "NetworkConnectionToWebProcess::cookiesForDOMAsync"_s, reinterpret_cast<const void*>(this), *networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, std::nullopt);
    }
#endif
    completionHandler(WTFMove(result));
}

void NetworkConnectionToWebProcess::setCookieFromDOMAsync(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, WebCore::Cookie&& cookie, RequiresScriptTrackingPrivacy requiresScriptTrackingPrivacy, std::optional<WebPageProxyIdentifier> webPageProxyID, CompletionHandler<void(bool)>&& completionHandler)
{
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, firstParty);
    MESSAGE_CHECK_COMPLETION(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate, completionHandler(false));
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow)
        return completionHandler(false);
    if (sameSiteInfo.isSameSite && !shouldTreatAsSameSite(firstParty, url)) {
        CONNECTION_RELEASE_LOG_ERROR(IPC, "setCookieFromDOMAsync: Rejecting cookie access due to invalid sameSiteInfo");
        return completionHandler(false);
    }

    CheckedPtr networkStorageSession = storageSession();
    if (!networkStorageSession)
        return completionHandler(false);

    auto result = networkStorageSession->setCookieFromDOM(firstParty, sameSiteInfo, url, frameID, pageID, ApplyTrackingPrevention::Yes, requiresScriptTrackingPrivacy, cookie, m_networkProcess->shouldRelaxThirdPartyCookieBlockingForPage(webPageProxyID), NetworkSession::isResourceFromKnownCrossSiteTracker(firstParty, url));
#if !RELEASE_LOG_DISABLED
    if (CheckedPtr session = networkSession()) {
        if (session->shouldLogCookieInformation())
            NetworkResourceLoader::logCookieInformation(*this, "NetworkConnectionToWebProcess::setCookiesFromDOMAsync"_s, reinterpret_cast<const void*>(this), *networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, std::nullopt);
    }
#endif
    completionHandler(result);
}

void NetworkConnectionToWebProcess::domCookiesForHost(const URL& url, CompletionHandler<void(const Vector<WebCore::Cookie>&)>&& completionHandler)
{
    auto host = url.host().toString();
    MESSAGE_CHECK_COMPLETION(HashSet<String>::isValidValue(url.host().toString()), completionHandler({ }));
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, url);
    MESSAGE_CHECK_COMPLETION(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate, completionHandler({ }));
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow)
        return completionHandler({ });

    CheckedPtr networkStorageSession = storageSession();
    if (!networkStorageSession)
        return completionHandler({ });

    completionHandler(networkStorageSession->domCookiesForHost(url));
}

#if HAVE(COOKIE_CHANGE_LISTENER_API)

void NetworkConnectionToWebProcess::subscribeToCookieChangeNotifications(const URL& url, const URL& firstParty, WebCore::FrameIdentifier frameID, WebCore::PageIdentifier pageID, WebPageProxyIdentifier webPageProxyID, CompletionHandler<void(bool)>&& completionHandler)
{
    auto allowCookieAccess = m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, firstParty);
    MESSAGE_CHECK_COMPLETION(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate, completionHandler(false));
    if (allowCookieAccess != NetworkProcess::AllowCookieAccess::Allow)
        return completionHandler({ });

    auto host = url.host().toString();
    MESSAGE_CHECK_COMPLETION(m_hostsWithCookieListeners.isValidValue(host), completionHandler(false));

    bool startedListening = false;
    if (CheckedPtr networkStorageSession = storageSession())
        startedListening = networkStorageSession->startListeningForCookieChangeNotifications(*this, url, firstParty, frameID, pageID, m_networkProcess->shouldRelaxThirdPartyCookieBlockingForPage(webPageProxyID), NetworkSession::isResourceFromKnownCrossSiteTracker(firstParty, url));

    if (startedListening)
        m_hostsWithCookieListeners.add(host);
    completionHandler(startedListening);
}

void NetworkConnectionToWebProcess::unsubscribeFromCookieChangeNotifications(const String& host)
{
    MESSAGE_CHECK(m_hostsWithCookieListeners.isValidValue(host));

    bool removed = m_hostsWithCookieListeners.remove(host);
    ASSERT_UNUSED(removed, removed);

    if (CheckedPtr networkStorageSession = storageSession())
        networkStorageSession->stopListeningForCookieChangeNotifications(*this, HashSet<String> { host });
}

void NetworkConnectionToWebProcess::cookiesAdded(const String& host, const Vector<WebCore::Cookie>& cookies)
{
    m_connection->send(Messages::NetworkProcessConnection::CookiesAdded(host, cookies), 0);
}

void NetworkConnectionToWebProcess::cookiesDeleted(const String& host, const Vector<WebCore::Cookie>& cookies)
{
    m_connection->send(Messages::NetworkProcessConnection::CookiesDeleted(host, cookies), 0);
}

void NetworkConnectionToWebProcess::allCookiesDeleted()
{
    m_connection->send(Messages::NetworkProcessConnection::AllCookiesDeleted(), 0);
}

#endif

void NetworkConnectionToWebProcess::subscribeToStorageAccessPermissionChanges(RegistrableDomain&& topFrameDomain, RegistrableDomain&& subFrameDomain)
{
    if (CheckedPtr networkSession = this->networkSession()) {
        if (RefPtr resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->startListeningForStorageAccessPermissionChanges(*this, WTFMove(topFrameDomain), WTFMove(subFrameDomain));
    }
}

void NetworkConnectionToWebProcess::unsubscribeFromStorageAccessPermissionChanges(RegistrableDomain&& topFrameDomain, RegistrableDomain&& subFrameDomain)
{
    if (CheckedPtr networkSession = this->networkSession()) {
        if (RefPtr resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->stopListeningForStorageAccessPermissionChanges(*this, WTFMove(topFrameDomain), WTFMove(subFrameDomain));
    }
}

void NetworkConnectionToWebProcess::storageAccessPermissionChanged(const RegistrableDomain& topFrameDomain, const RegistrableDomain& subFrameDomain)
{
    m_connection->send(Messages::NetworkProcessConnection::StorageAccessPermissionChanged(topFrameDomain, subFrameDomain), 0);
}

void NetworkConnectionToWebProcess::cookieEnabledStateMayHaveChanged()
{
    m_connection->send(Messages::NetworkProcessConnection::UpdateCachedCookiesEnabled(), 0);
}

bool NetworkConnectionToWebProcess::isFilePathAllowed(NetworkSession& session, String path)
{
    path = FileSystem::lexicallyNormal(path);
    auto parentPath = FileSystem::parentPath(path);
    while (parentPath != path) {
        if (m_allowedFilePaths.contains(path) || parentPath == session.storageManager().path() || parentPath == session.storageManager().customIDBStoragePath())
            return true;
        path = parentPath;
        parentPath = FileSystem::parentPath(path);
    }
    return false;
}

static bool shouldCheckBlobFileAccess()
{
#if PLATFORM(COCOA)
    return WTF::linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::BlobFileAccessEnforcement) && !Quirks::shouldDisableBlobFileAccessEnforcement();
#else
    return true;
#endif
}

void NetworkConnectionToWebProcess::registerInternalFileBlobURL(const URL& url, const String& path, const String& replacementPath, SandboxExtension::Handle&& extensionHandle, const String& contentType)
{
    MESSAGE_CHECK(!url.isEmpty());

    CheckedPtr session = networkSession();
    if (!session)
        return;
    if (blobFileAccessEnforcementEnabled() && shouldCheckBlobFileAccess())
        MESSAGE_CHECK(isFilePathAllowed(*session, path));

    m_blobURLs.add({ url, std::nullopt });
    session->blobRegistry().registerInternalFileBlobURL(url, BlobDataFileReferenceWithSandboxExtension::create(path, replacementPath, SandboxExtension::create(WTFMove(extensionHandle))), contentType);
}

void NetworkConnectionToWebProcess::registerInternalBlobURL(const URL& url, Vector<BlobPart>&& blobParts, const String& contentType)
{
    CheckedPtr session = networkSession();
    if (!session)
        return;

    m_blobURLs.add({ url, std::nullopt });
    session->blobRegistry().registerInternalBlobURL(url, WTFMove(blobParts), contentType);
}

void NetworkConnectionToWebProcess::registerBlobURL(const URL& url, const URL& srcURL, PolicyContainer&& policyContainer, const std::optional<SecurityOriginData>& topOrigin)
{
    CheckedPtr session = networkSession();
    if (!session)
        return;

    m_blobURLs.add({ url, topOrigin });
    session->blobRegistry().registerBlobURL(url, srcURL, WTFMove(policyContainer), topOrigin);
}

void NetworkConnectionToWebProcess::registerInternalBlobURLOptionallyFileBacked(URL&& url, URL&& srcURL, const String& fileBackedPath, String&& contentType)
{
    MESSAGE_CHECK(!url.isEmpty() && !srcURL.isEmpty() && !fileBackedPath.isEmpty());
    CheckedPtr session = networkSession();
    if (!session)
        return;
    if (blobFileAccessEnforcementEnabled() && shouldCheckBlobFileAccess())
        MESSAGE_CHECK(isFilePathAllowed(*session, fileBackedPath));

    m_blobURLs.add({ url, std::nullopt });
    session->blobRegistry().registerInternalBlobURLOptionallyFileBacked(url, srcURL, BlobDataFileReferenceWithSandboxExtension::create(fileBackedPath), contentType, { });
}

void NetworkConnectionToWebProcess::registerInternalBlobURLForSlice(const URL& url, const URL& srcURL, int64_t start, int64_t end, const String& contentType)
{
    CheckedPtr session = networkSession();
    if (!session)
        return;

    m_blobURLs.add({ url, std::nullopt });
    session->blobRegistry().registerInternalBlobURLForSlice(url, srcURL, start, end, contentType);
}

void NetworkConnectionToWebProcess::unregisterBlobURL(const URL& url, const std::optional<WebCore::SecurityOriginData>& topOrigin)
{
    CheckedPtr session = networkSession();
    if (!session)
        return;

    m_blobURLs.remove({ url, topOrigin });
    session->blobRegistry().unregisterBlobURL(url, topOrigin);
}

void NetworkConnectionToWebProcess::registerBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& topOrigin)
{
    CheckedPtr session = networkSession();
    if (!session)
        return;

    m_blobURLHandles.add({ url, topOrigin });
    session->blobRegistry().registerBlobURLHandle(url, topOrigin);
}

void NetworkConnectionToWebProcess::unregisterBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& topOrigin)
{
    CheckedPtr session = networkSession();
    if (!session)
        return;

    m_blobURLHandles.remove({ url, topOrigin });
    session->blobRegistry().unregisterBlobURLHandle(url, topOrigin);
}

void NetworkConnectionToWebProcess::blobType(const URL& url, CompletionHandler<void(String)>&& completionHandler)
{
    CheckedPtr session = networkSession();
    completionHandler(session ? session->blobRegistry().blobType(url) : emptyString());
}

void NetworkConnectionToWebProcess::blobSize(const URL& url, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    CheckedPtr session = networkSession();
    completionHandler(session ? session->blobRegistry().blobSize(url) : 0);
}

void NetworkConnectionToWebProcess::writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    CheckedPtr session = networkSession();
    if (!session)
        return completionHandler({ });

    MESSAGE_CHECK_COMPLETION(!session->sessionID().isEphemeral(), completionHandler({ }));

    Vector<RefPtr<BlobDataFileReference>> fileReferences;
    for (auto& url : blobURLs)
        fileReferences.appendVector(session->blobRegistry().filesInBlob({ { }, url }));

    for (auto& file : fileReferences)
        file->prepareForFileAccess();

    session->blobRegistry().writeBlobsToTemporaryFilesForIndexedDB(blobURLs, [this, protectedThis = Ref { *this }, fileReferences = WTFMove(fileReferences), completionHandler = WTFMove(completionHandler)](Vector<String>&& filePaths) mutable {
        for (auto& file : fileReferences)
            file->revokeFileAccess();

        if (CheckedPtr session = networkSession())
            session->storageManager().registerTemporaryBlobFilePaths(Ref { m_connection }, filePaths);

        // Web process might create Blob with these files during index key generation.
        for (auto& path : filePaths)
            allowAccessToFile(path);

        completionHandler(WTFMove(filePaths));
    });
}

void NetworkConnectionToWebProcess::registerBlobPathForTesting(const String& path, CompletionHandler<void()>&& completion)
{
    if (!allowTestOnlyIPC())
        return completion();
    allowAccessToFile(path);
    completion();
}

void NetworkConnectionToWebProcess::allowAccessToFile(const String& path)
{
    m_allowedFilePaths.add(FileSystem::lexicallyNormal(path));
}

void NetworkConnectionToWebProcess::setCaptureExtraNetworkLoadMetricsEnabled(bool enabled)
{
    m_captureExtraNetworkLoadMetricsEnabled = enabled;
    if (m_captureExtraNetworkLoadMetricsEnabled)
        return;

    m_networkLoadInformationByID.clear();
    for (auto& loader : m_networkResourceLoaders.values())
        loader->disableExtraNetworkLoadMetricsCapture();
}

void NetworkConnectionToWebProcess::clearPageSpecificData(PageIdentifier pageID)
{
    if (CheckedPtr session = networkSession())
        session->protectedNetworkLoadScheduler()->clearPageData(pageID);

    if (CheckedPtr storageSession = m_networkProcess->storageSession(m_sessionID))
        storageSession->clearPageSpecificDataForResourceLoadStatistics(pageID);
}

void NetworkConnectionToWebProcess::removeStorageAccessForFrame(FrameIdentifier frameID, PageIdentifier pageID)
{
    if (CheckedPtr storageSession = m_networkProcess->storageSession(m_sessionID))
        storageSession->removeStorageAccessForFrame(frameID, pageID);
}

void NetworkConnectionToWebProcess::logUserInteraction(RegistrableDomain&& domain)
{
    if (CheckedPtr networkSession = this->networkSession()) {
        if (RefPtr resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->logUserInteraction(WTFMove(domain), [] { });
    }
}

void NetworkConnectionToWebProcess::resourceLoadStatisticsUpdated(Vector<ResourceLoadStatistics>&& statistics, CompletionHandler<void()>&& completionHandler)
{
    if (CheckedPtr networkSession = this->networkSession()) {
        if (networkSession->sessionID().isEphemeral()) {
            completionHandler();
            return;
        }
        if (RefPtr resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->resourceLoadStatisticsUpdated(WTFMove(statistics), WTFMove(completionHandler));
            return;
        }
    }
    completionHandler();
}

void NetworkConnectionToWebProcess::hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (CheckedPtr networkSession = this->networkSession()) {
        if (RefPtr resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->hasStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, WTFMove(completionHandler));
            return;
        }
        CheckedRef { *storageSession() }->hasCookies(subFrameDomain, WTFMove(completionHandler));
        return;
    }

    completionHandler(false);
}

void NetworkConnectionToWebProcess::requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier webPageID, WebPageProxyIdentifier webPageProxyID, StorageAccessScope scope, HasOrShouldIgnoreUserGesture hasOrShouldIgnoreUserGesture, CompletionHandler<void(WebCore::RequestStorageAccessResult result)>&& completionHandler)
{
    if (CheckedPtr networkSession = this->networkSession()) {
        if (RefPtr resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->requestStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, webPageID, webPageProxyID, scope, hasOrShouldIgnoreUserGesture, WTFMove(completionHandler));
            return;
        }
    }

    completionHandler({ WebCore::StorageAccessWasGranted::Yes, WebCore::StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });
}

void NetworkConnectionToWebProcess::queryStorageAccessPermission(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, std::optional<WebPageProxyIdentifier> webPageProxyID, CompletionHandler<void(WebCore::PermissionState)>&& completionHandler)
{
    if (CheckedPtr networkSession = this->networkSession()) {
        if (RefPtr resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->queryStorageAccessPermission(WTFMove(subFrameDomain), WTFMove(topFrameDomain), webPageProxyID, WTFMove(completionHandler));
            return;
        }
    }
}

void NetworkConnectionToWebProcess::setLoginStatus(RegistrableDomain&& domain, IsLoggedIn loggedInStatus, std::optional<WebCore::LoginStatus>&& lastAuthentication, CompletionHandler<void()>&& completionHandler)
{
    if (isLoginStatusAPIRequiresWebAuthnEnabled() && !lastAuthentication) {
        auto umanagedAuthentication = LoginStatus::create(domain, emptyString(), WebCore::LoginStatus::CredentialTokenType::HTTPStateToken, WebCore::LoginStatus::AuthenticationType::Unmanaged, WebCore::LoginStatus::TimeToLiveAuthentication);
        if (umanagedAuthentication.hasException())
            return completionHandler();
        lastAuthentication = umanagedAuthentication.releaseReturnValue();
    }

    if (CheckedPtr networkSession = this->networkSession()) {
        if (RefPtr resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->setLoginStatus(WTFMove(domain), loggedInStatus, WTFMove(lastAuthentication), WTFMove(completionHandler));
            return;
        }
    }
    completionHandler();
}

void NetworkConnectionToWebProcess::isLoggedIn(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (CheckedPtr networkSession = this->networkSession()) {
        if (RefPtr resourceLoadStatistics = networkSession->resourceLoadStatistics()) {
            resourceLoadStatistics->isLoggedIn(WTFMove(domain), WTFMove(completionHandler));
            return;
        }
    }
    completionHandler(false);
}

void NetworkConnectionToWebProcess::storageAccessQuirkForTopFrameDomain(URL&& topFrameURL, CompletionHandler<void(Vector<RegistrableDomain>)>&& completionHandler)
{
    completionHandler(NetworkStorageSession::storageAccessQuirkForTopFrameDomain(topFrameURL));
}

void NetworkConnectionToWebProcess::requestStorageAccessUnderOpener(WebCore::RegistrableDomain&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, WebCore::RegistrableDomain&& openerDomain)
{
    if (CheckedPtr networkSession = this->networkSession()) {
        if (RefPtr resourceLoadStatistics = networkSession->resourceLoadStatistics())
            resourceLoadStatistics->requestStorageAccessUnderOpener(WTFMove(domainInNeedOfStorageAccess), openerPageID, WTFMove(openerDomain));
    }
}

void NetworkConnectionToWebProcess::addOriginAccessAllowListEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::addOriginAccessAllowlistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void NetworkConnectionToWebProcess::removeOriginAccessAllowListEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::removeOriginAccessAllowlistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void NetworkConnectionToWebProcess::resetOriginAccessAllowLists()
{
    SecurityPolicy::resetOriginAccessAllowlists();
}

std::optional<NetworkActivityTracker> NetworkConnectionToWebProcess::startTrackingResourceLoad(PageIdentifier pageID, WebCore::ResourceLoaderIdentifier resourceID, bool isTopResource)
{
    if (m_sessionID.isEphemeral())
        return std::nullopt;

    // Either get the existing root activity tracker for this page or create a
    // new one if this is the main resource.

    size_t rootActivityIndex;
    if (isTopResource) {
        // If we're loading a page from the top, make sure any tracking of
        // previous activity for this page is stopped.

        stopAllNetworkActivityTrackingForPage(pageID);

        rootActivityIndex = m_networkActivityTrackers.size();
        m_networkActivityTrackers.constructAndAppend(pageID);
        m_networkActivityTrackers[rootActivityIndex].networkActivity.start();

#if HAVE(NW_ACTIVITY)
        ASSERT(m_networkActivityTrackers[rootActivityIndex].networkActivity.getPlatformObject());
#endif
    } else {
        rootActivityIndex = findRootNetworkActivity(pageID);

        // This could happen if the Networking process crashes, taking its
        // previous state with it.
        if (rootActivityIndex == notFound)
            return std::nullopt;

#if HAVE(NW_ACTIVITY)
        ASSERT(m_networkActivityTrackers[rootActivityIndex].networkActivity.getPlatformObject());
#endif
    }

    // Create a tracker for the loading of the new resource, setting the root
    // activity tracker as its parent.

    size_t newActivityIndex = m_networkActivityTrackers.size();
    m_networkActivityTrackers.constructAndAppend(pageID, resourceID);
#if HAVE(NW_ACTIVITY)
    ASSERT(m_networkActivityTrackers[newActivityIndex].networkActivity.getPlatformObject());
#endif

    auto& newActivityTracker = m_networkActivityTrackers[newActivityIndex];
    newActivityTracker.networkActivity.setParent(m_networkActivityTrackers[rootActivityIndex].networkActivity);
    newActivityTracker.networkActivity.start();

    return newActivityTracker.networkActivity;
}

void NetworkConnectionToWebProcess::stopTrackingResourceLoad(WebCore::ResourceLoaderIdentifier resourceID, NetworkActivityTracker::CompletionCode code)
{
    auto itemIndex = findNetworkActivityTracker(resourceID);
    if (itemIndex == notFound)
        return;

    m_networkActivityTrackers[itemIndex].networkActivity.complete(code);
    m_networkActivityTrackers.removeAt(itemIndex);
}

void NetworkConnectionToWebProcess::stopAllNetworkActivityTracking()
{
    for (auto& activityTracker : m_networkActivityTrackers)
        activityTracker.networkActivity.complete(NetworkActivityTracker::CompletionCode::Cancel);

    m_networkActivityTrackers.clear();
}

void NetworkConnectionToWebProcess::stopAllNetworkActivityTrackingForPage(PageIdentifier pageID)
{
    for (auto& activityTracker : m_networkActivityTrackers) {
        if (activityTracker.pageID == pageID)
            activityTracker.networkActivity.complete(NetworkActivityTracker::CompletionCode::Cancel);
    }

    m_networkActivityTrackers.removeAllMatching([&](const auto& activityTracker) {
        return activityTracker.pageID == pageID;
    });
}

size_t NetworkConnectionToWebProcess::findRootNetworkActivity(PageIdentifier pageID)
{
    return m_networkActivityTrackers.findIf([&](const auto& item) {
        return item.isRootActivity && item.pageID == pageID;
    });
}

size_t NetworkConnectionToWebProcess::findNetworkActivityTracker(WebCore::ResourceLoaderIdentifier resourceID)
{
    return m_networkActivityTrackers.findIf([&](const auto& item) {
        return item.resourceID == resourceID;
    });
}

void NetworkConnectionToWebProcess::establishSharedWorkerContextConnection(WebPageProxyIdentifier, WebCore::Site&& site, CompletionHandler<void()>&& completionHandler)
{
    CONNECTION_RELEASE_LOG(SharedWorker, "establishSharedWorkerContextConnection:");
    CheckedPtr session = networkSession();
    if (CheckedPtr swServer = session ? session->sharedWorkerServer() : nullptr)
        m_sharedWorkerContextConnection = WebSharedWorkerServerToContextConnection::create(*this, WTFMove(site), *swServer);
    completionHandler();
}

void NetworkConnectionToWebProcess::establishSharedWorkerServerConnection()
{
    if (m_sharedWorkerConnection)
        return;

    CheckedPtr session = networkSession();
    if (!session)
        return;

    CONNECTION_RELEASE_LOG(SharedWorker, "establishSharedWorkerServerConnection:");

    CheckedRef server = session->ensureSharedWorkerServer();
    auto connection = WebSharedWorkerServerConnection::create(m_networkProcess, server.get(), m_connection.get(), m_webProcessIdentifier);

    m_sharedWorkerConnection = connection;
    server->addConnection(WTFMove(connection));
}

void NetworkConnectionToWebProcess::closeSharedWorkerContextConnection()
{
    CONNECTION_RELEASE_LOG(SharedWorker, "closeSharedWorkerContextConnection:");
    m_sharedWorkerContextConnection = nullptr;
}

void NetworkConnectionToWebProcess::unregisterSharedWorkerConnection()
{
    CONNECTION_RELEASE_LOG(SharedWorker, "unregisterSharedWorkerConnection:");
    if (RefPtr connection = m_sharedWorkerConnection.get()) {
        if (CheckedPtr server = connection->server())
            server->removeConnection(m_sharedWorkerConnection->webProcessIdentifier());
    }
}

void NetworkConnectionToWebProcess::sharedWorkerServerToContextConnectionIsNoLongerNeeded()
{
    CONNECTION_RELEASE_LOG(SharedWorker, "sharedWorkerServerToContextConnectionIsNoLongerNeeded:");
    m_networkProcess->protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::RemoteWorkerContextConnectionNoLongerNeeded { RemoteWorkerType::SharedWorker, webProcessIdentifier() }, 0);

    m_sharedWorkerContextConnection = nullptr;
}

WebSharedWorkerServerConnection* NetworkConnectionToWebProcess::sharedWorkerConnection()
{
    if (!m_sharedWorkerConnection)
        establishSharedWorkerServerConnection();
    return m_sharedWorkerConnection.get();
}

void NetworkConnectionToWebProcess::unregisterSWConnection()
{
    if (RefPtr swServer = m_swConnection ? m_swConnection->server() : nullptr)
        swServer->removeConnection(m_swConnection->identifier());
}

void NetworkConnectionToWebProcess::establishSWServerConnection()
{
    if (m_swConnection)
        return;

    CheckedPtr session = networkSession();
    if (!session)
        return;

    Ref server = session->ensureSWServer();
    Ref connection = WebSWServerConnection::create(*this, server, m_connection.get(), m_webProcessIdentifier);

    m_swConnection = connection.get();
    server->addConnection(WTFMove(connection));
}

void NetworkConnectionToWebProcess::establishSWContextConnection(WebPageProxyIdentifier webPageProxyID, Site&& site, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, CompletionHandler<void()>&& completionHandler)
{
    CheckedPtr session = networkSession();

    if (session) {
        Ref swServer = session->ensureSWServer();
        auto allowCookieAccess = session->networkProcess().allowsFirstPartyForCookies(webProcessIdentifier(), site.domain());
        MESSAGE_CHECK_COMPLETION(allowCookieAccess != NetworkProcess::AllowCookieAccess::Terminate, completionHandler());
        m_swContextConnection = WebSWServerToContextConnection::create(*this, webPageProxyID, WTFMove(site), serviceWorkerPageIdentifier, swServer);
    }
    completionHandler();
}

void NetworkConnectionToWebProcess::closeSWContextConnection()
{
    if (RefPtr connection = std::exchange(m_swContextConnection, nullptr))
        connection->stop();
}

void NetworkConnectionToWebProcess::terminateIdleServiceWorkers()
{
    if (RefPtr connection = m_swContextConnection)
        connection->terminateIdleServiceWorkers();
}

void NetworkConnectionToWebProcess::serviceWorkerServerToContextConnectionNoLongerNeeded()
{
    CONNECTION_RELEASE_LOG(ServiceWorker, "serviceWorkerServerToContextConnectionNoLongerNeeded: WebProcess no longer useful for running service workers");
    m_networkProcess->protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::RemoteWorkerContextConnectionNoLongerNeeded { RemoteWorkerType::ServiceWorker, webProcessIdentifier() }, 0);

    if (RefPtr connection = std::exchange(m_swContextConnection, nullptr))
        connection->stop();
}

void NetworkConnectionToWebProcess::terminateSWContextConnectionDueToUnresponsiveness()
{
    m_networkProcess->protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::ProcessHasUnresponseServiceWorker { webProcessIdentifier() }, 0);
    closeSWContextConnection();
}

WebSWServerConnection* NetworkConnectionToWebProcess::swConnection()
{
    if (!m_swConnection)
        establishSWServerConnection();
    return m_swConnection.get();
}

void NetworkConnectionToWebProcess::createNewMessagePortChannel(const MessagePortIdentifier& port1, const MessagePortIdentifier& port2)
{
    m_processEntangledPorts.add(port1);
    m_processEntangledPorts.add(port2);
    m_networkProcess->checkedMessagePortChannelRegistry()->didCreateMessagePortChannel(port1, port2);
}

void NetworkConnectionToWebProcess::entangleLocalPortInThisProcessToRemote(const MessagePortIdentifier& local, const MessagePortIdentifier& remote)
{
    m_processEntangledPorts.add(local);
    m_networkProcess->checkedMessagePortChannelRegistry()->didEntangleLocalToRemote(local, remote, m_webProcessIdentifier);

    RefPtr channel = m_networkProcess->checkedMessagePortChannelRegistry()->existingChannelContainingPort(local);
    if (channel && channel->hasAnyMessagesPendingOrInFlight())
        m_connection->send(Messages::NetworkProcessConnection::MessagesAvailableForPort(local), 0);
}

void NetworkConnectionToWebProcess::messagePortDisentangled(const MessagePortIdentifier& port)
{
    m_processEntangledPorts.remove(port);

    m_networkProcess->checkedMessagePortChannelRegistry()->didDisentangleMessagePort(port);
}

void NetworkConnectionToWebProcess::messagePortClosed(const MessagePortIdentifier& port)
{
    m_networkProcess->checkedMessagePortChannelRegistry()->didCloseMessagePort(port);
}

MessageBatchIdentifier NetworkConnectionToWebProcess::nextMessageBatchIdentifier(CompletionHandler<void()>&& deliveryCallback)
{
    auto identifier = MessageBatchIdentifier::generate();
    ASSERT(!m_messageBatchDeliveryCompletionHandlers.contains(identifier));
    m_messageBatchDeliveryCompletionHandlers.add(identifier, WTFMove(deliveryCallback));
    return identifier;
}

void NetworkConnectionToWebProcess::takeAllMessagesForPort(const MessagePortIdentifier& port, CompletionHandler<void(Vector<MessageWithMessagePorts>&&, std::optional<MessageBatchIdentifier>)>&& callback)
{
    m_networkProcess->checkedMessagePortChannelRegistry()->takeAllMessagesForPort(port, [this, protectedThis = Ref { *this }, callback = WTFMove(callback)](Vector<MessageWithMessagePorts>&& messages, CompletionHandler<void()>&& deliveryCallback) mutable {
        callback(WTFMove(messages), nextMessageBatchIdentifier(WTFMove(deliveryCallback)));
    });
}

void NetworkConnectionToWebProcess::didDeliverMessagePortMessages(MessageBatchIdentifier messageBatchIdentifier)
{
    // Null check only necessary for rare condition where network process crashes during message port connection establishment.
    if (auto callback = m_messageBatchDeliveryCompletionHandlers.take(messageBatchIdentifier))
        callback();
}

void NetworkConnectionToWebProcess::postMessageToRemote(MessageWithMessagePorts&& message, const MessagePortIdentifier& port)
{
    if (m_networkProcess->checkedMessagePortChannelRegistry()->didPostMessageToRemote(WTFMove(message), port)) {
        // Look up the process for that port
        RefPtr channel = m_networkProcess->checkedMessagePortChannelRegistry()->existingChannelContainingPort(port);
        ASSERT(channel);
        auto processIdentifier = channel->processForPort(port);
        if (processIdentifier) {
            if (RefPtr connectionToWebProcess = m_networkProcess->webProcessConnection(*processIdentifier))
                connectionToWebProcess->m_connection->send(Messages::NetworkProcessConnection::MessagesAvailableForPort(port), 0);
        }
    }
}

void NetworkConnectionToWebProcess::broadcastConsoleMessage(JSC::MessageSource source, JSC::MessageLevel level, const String& message)
{
    m_connection->send(Messages::NetworkProcessConnection::BroadcastConsoleMessage(source, level, message), 0);
}

void NetworkConnectionToWebProcess::setCORSDisablingPatterns(WebCore::PageIdentifier pageIdentifier, Vector<String>&& patterns)
{
    m_networkProcess->setCORSDisablingPatterns(*this, pageIdentifier, WTFMove(patterns));
}

void NetworkConnectionToWebProcess::setResourceLoadSchedulingMode(WebCore::PageIdentifier pageIdentifier, WebCore::LoadSchedulingMode mode)
{
    CheckedPtr session = networkSession();
    if (!session)
        return;

    session->protectedNetworkLoadScheduler()->setResourceLoadSchedulingMode(pageIdentifier, mode);
}

void NetworkConnectionToWebProcess::prioritizeResourceLoads(const Vector<WebCore::ResourceLoaderIdentifier>& loadIdentifiers)
{
    CheckedPtr session = networkSession();
    if (!session)
        return;

    Vector<RefPtr<NetworkLoad>> loads;
    for (auto identifier : loadIdentifiers) {
        RefPtr loader = m_networkResourceLoaders.get(identifier);
        if (!loader || !loader->networkLoad())
            continue;
        loads.append(loader->networkLoad());
    }

    session->protectedNetworkLoadScheduler()->prioritizeLoads(loads);
}

RefPtr<NetworkResourceLoader> NetworkConnectionToWebProcess::takeNetworkResourceLoader(WebCore::ResourceLoaderIdentifier resourceLoadIdentifier)
{
    if (!NetworkResourceLoadMap::MapType::isValidKey(resourceLoadIdentifier))
        return nullptr;
    return m_networkResourceLoaders.take(resourceLoadIdentifier);
}

#if ENABLE(CONTENT_FILTERING)
void NetworkConnectionToWebProcess::installMockContentFilter(WebCore::MockContentFilterSettings&& settings)
{
    MockContentFilterSettings::singleton() = WTFMove(settings);
}
#endif

void NetworkConnectionToWebProcess::useRedirectionForCurrentNavigation(WebCore::ResourceLoaderIdentifier identifier, WebCore::ResourceResponse&& response)
{
    if (RefPtr loader = m_networkResourceLoaders.get(identifier))
        loader->useRedirectionForCurrentNavigation(WTFMove(response));
}

#if ENABLE(DECLARATIVE_WEB_PUSH)
void NetworkConnectionToWebProcess::navigatorSubscribeToPushService(URL&& scopeURL, Vector<uint8_t>&& applicationServerKey, CompletionHandler<void(Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&&)>&& completionHandler)
{
    CheckedPtr session = networkSession();
    if (!session) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "No active network session"_s }));
        return;
    }

    auto registrableDomain = RegistrableDomain(scopeURL);
    session->notificationManager().subscribeToPushService(WTFMove(scopeURL), WTFMove(applicationServerKey), [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler), registrableDomain = WTFMove(registrableDomain)](auto&& result) mutable {
        if (RefPtr resourceLoadStatistics = weakThis && weakThis->networkSession() ? weakThis->networkSession()->resourceLoadStatistics() : nullptr; result && resourceLoadStatistics) {
            return resourceLoadStatistics->setMostRecentWebPushInteractionTime(WTFMove(registrableDomain), [result = WTFMove(result), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(WTFMove(result));
            });
        }
        completionHandler(WTFMove(result));
    });
}

void NetworkConnectionToWebProcess::navigatorUnsubscribeFromPushService(URL&& scopeURL, const PushSubscriptionIdentifier& subscriptionIdentifier, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&& completionHandler)
{
    CheckedPtr session = networkSession();
    if (!session) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "No active network session"_s }));
        return;
    }

    session->notificationManager().unsubscribeFromPushService(WTFMove(scopeURL), subscriptionIdentifier, WTFMove(completionHandler));

}

void NetworkConnectionToWebProcess::navigatorGetPushSubscription(URL&& scopeURL, CompletionHandler<void(Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&&)>&& completionHandler)
{
    CheckedPtr session = networkSession();
    if (!session) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "No active network session"_s }));
        return;
    }

    session->notificationManager().getPushSubscription(WTFMove(scopeURL), WTFMove(completionHandler));
}

void NetworkConnectionToWebProcess::navigatorGetPushPermissionState(URL&& scopeURL, CompletionHandler<void(Expected<uint8_t, WebCore::ExceptionData>&&)>&& completionHandler)
{
    CheckedPtr session = networkSession();
    if (!session) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "No active network session"_s }));
        return;
    }

    session->notificationManager().getPermissionState(SecurityOriginData::fromURL(scopeURL), [completionHandler = WTFMove(completionHandler)](WebCore::PushPermissionState state) mutable {
        completionHandler(static_cast<uint8_t>(state));
    });
}
#endif // ENABLE(DECLARATIVE_WEB_PUSH)

void NetworkConnectionToWebProcess::initializeWebTransportSession(WebTransportSessionIdentifier identifier, URL&& url, WebCore::WebTransportOptions&& options, WebPageProxyIdentifier&& pageID, WebCore::ClientOrigin&& clientOrigin, CompletionHandler<void(std::optional<WebCore::WebTransportConnectionInfo>&&)>&& completionHandler)
{
    if (!url.isValid()
        || !portAllowed(url)
        || isIPAddressDisallowed(url)
        || m_networkTransportSessions.contains(identifier))
        return completionHandler(std::nullopt);

    RefPtr session = NetworkTransportSession::create(*this, identifier, WTFMove(url), WTFMove(options), WTFMove(pageID), WTFMove(clientOrigin));
    if (!session)
        return completionHandler(std::nullopt);
    session->initialize(WTFMove(completionHandler));
    m_networkTransportSessions.set(identifier, session.releaseNonNull());
}

void NetworkConnectionToWebProcess::destroyWebTransportSession(WebTransportSessionIdentifier identifier)
{
    m_networkTransportSessions.remove(identifier);
}

#if USE(LIBRICE)
void NetworkConnectionToWebProcess::initializeRiceBackend(WebPageProxyIdentifier&& pageID, CompletionHandler<void(std::optional<RiceBackendIdentifier>)>&& completionHandler)
{
    RiceBackend::initialize(*this, WTFMove(pageID), [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)](RefPtr<RiceBackend>&& backend) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!backend || !protectedThis)
            return completionHandler(std::nullopt);

        auto identifier = backend->identifier();
        ASSERT(!protectedThis->m_gstreamerIceBackends.contains(identifier));
        protectedThis->m_gstreamerIceBackends.set(identifier, backend.releaseNonNull());
        completionHandler(identifier);
    });
}

void NetworkConnectionToWebProcess::destroyRiceBackend(RiceBackendIdentifier identifier)
{
    ASSERT(m_gstreamerIceBackends.contains(identifier));
    m_gstreamerIceBackends.remove(identifier);
}
#endif

void NetworkConnectionToWebProcess::clearFrameLoadRecordsForStorageAccess(WebCore::FrameIdentifier frameID)
{
    if (CheckedPtr session = networkSession()) {
        if (RefPtr resourceLoadStatistics = session->resourceLoadStatistics())
            resourceLoadStatistics->clearFrameLoadRecordsForStorageAccess(frameID);
    }
}

bool NetworkConnectionToWebProcess::isAlwaysOnLoggingAllowed() const
{
    return m_sessionID.isAlwaysOnLoggingAllowed() || m_sharedPreferencesForWebProcess.allowPrivacySensitiveOperationsInNonPersistentDataStores;
}

void NetworkConnectionToWebProcess::updateSharedPreferencesForWebProcess(SharedPreferencesForWebProcess&& sharedPreferencesForWebProcess)
{
    m_sharedPreferencesForWebProcess = WTFMove(sharedPreferencesForWebProcess);
    if (CheckedPtr session = networkSession())
        session->storageManager().updateSharedPreferencesForConnection(m_connection, m_sharedPreferencesForWebProcess);
}

#if ENABLE(CONTENT_EXTENSIONS)
void NetworkConnectionToWebProcess::shouldOffloadIFrameForHost(const String& host, CompletionHandler<void(bool)>&& completionHandler)
{
    CONNECTION_RELEASE_LOG(ResourceMonitoring, "shouldOffloadIFrameForHost: (host=%" SENSITIVE_LOG_STRING ")", host.utf8().data());
    if (CheckedPtr session = networkSession())
        session->protectedResourceMonitorThrottler()->tryAccess(host, ContinuousApproximateTime::now(), WTFMove(completionHandler));
    else
        completionHandler(false);
}
#endif

#if ENABLE(IPC_TESTING_API)
void NetworkConnectionToWebProcess::takeInvalidMessageStringForTesting(CompletionHandler<void(String&&)>&& callback)
{
    ASCIILiteral error = connection().takeErrorString();
    String errorString = !error.isNull() ? String::fromUTF8(error) : emptyString();
    callback(WTFMove(errorString));
}
#endif

} // namespace WebKit

#undef CONNECTION_RELEASE_LOG
#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_WITH_RETURN_VALUE
