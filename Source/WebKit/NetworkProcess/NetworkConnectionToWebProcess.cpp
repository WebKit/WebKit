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
#include "NetworkConnectionToWebProcess.h"

#include "BlobDataFileReferenceWithSandboxExtension.h"
#include "CacheStorageEngineConnectionMessages.h"
#include "DataReference.h"
#include "NetworkBlobRegistry.h"
#include "NetworkCache.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkLoad.h"
#include "NetworkMDNSRegisterMessages.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "NetworkProcessMessages.h"
#include "NetworkRTCMonitorMessages.h"
#include "NetworkRTCProviderMessages.h"
#include "NetworkRTCSocketMessages.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoader.h"
#include "NetworkResourceLoaderMessages.h"
#include "NetworkSession.h"
#include "NetworkSocketStream.h"
#include "NetworkSocketStreamMessages.h"
#include "PingLoad.h"
#include "PreconnectTask.h"
#include "SessionTracker.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebIDBConnectionToClient.h"
#include "WebIDBConnectionToClientMessages.h"
#include "WebSWServerConnection.h"
#include "WebSWServerConnectionMessages.h"
#include "WebSWServerToContextConnection.h"
#include "WebSWServerToContextConnectionMessages.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/SecurityPolicy.h>

namespace WebKit {
using namespace WebCore;

Ref<NetworkConnectionToWebProcess> NetworkConnectionToWebProcess::create(IPC::Connection::Identifier connectionIdentifier)
{
    return adoptRef(*new NetworkConnectionToWebProcess(connectionIdentifier));
}

NetworkConnectionToWebProcess::NetworkConnectionToWebProcess(IPC::Connection::Identifier connectionIdentifier)
    : m_connection(IPC::Connection::createServerConnection(connectionIdentifier, *this))
#if ENABLE(WEB_RTC)
    , m_mdnsRegister(*this)
#endif
{
    RELEASE_ASSERT(RunLoop::isMain());

    // Use this flag to force synchronous messages to be treated as asynchronous messages in the WebProcess.
    // Otherwise, the WebProcess would process incoming synchronous IPC while waiting for a synchronous IPC
    // reply from the Network process, which would be unsafe.
    m_connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    m_connection->open();
}

NetworkConnectionToWebProcess::~NetworkConnectionToWebProcess()
{
    RELEASE_ASSERT(RunLoop::isMain());

    m_connection->invalidate();
#if USE(LIBWEBRTC)
    if (m_rtcProvider)
        m_rtcProvider->close();
#endif

#if ENABLE(SERVICE_WORKER)
    unregisterSWConnections();
#endif
}

void NetworkConnectionToWebProcess::didCleanupResourceLoader(NetworkResourceLoader& loader)
{
    RELEASE_ASSERT(loader.identifier());
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(m_networkResourceLoaders.get(loader.identifier()) == &loader);

    m_networkResourceLoaders.remove(loader.identifier());
}

void NetworkConnectionToWebProcess::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::NetworkConnectionToWebProcess::messageReceiverName()) {
        didReceiveNetworkConnectionToWebProcessMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::NetworkResourceLoader::messageReceiverName()) {
        RELEASE_ASSERT(RunLoop::isMain());
        RELEASE_ASSERT(decoder.destinationID());
        if (auto* loader = m_networkResourceLoaders.get(decoder.destinationID()))
            loader->didReceiveNetworkResourceLoaderMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::NetworkSocketStream::messageReceiverName()) {
        if (auto* socketStream = m_networkSocketStreams.get(decoder.destinationID())) {
            socketStream->didReceiveMessage(connection, decoder);
            if (decoder.messageName() == Messages::NetworkSocketStream::Close::name())
                m_networkSocketStreams.remove(decoder.destinationID());
        }
        return;
    }

    if (decoder.messageReceiverName() == Messages::NetworkProcess::messageReceiverName()) {
        NetworkProcess::singleton().didReceiveNetworkProcessMessage(connection, decoder);
        return;
    }


#if USE(LIBWEBRTC)
    if (decoder.messageReceiverName() == Messages::NetworkRTCSocket::messageReceiverName()) {
        rtcProvider().didReceiveNetworkRTCSocketMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::NetworkRTCMonitor::messageReceiverName()) {
        rtcProvider().didReceiveNetworkRTCMonitorMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::NetworkRTCProvider::messageReceiverName()) {
        rtcProvider().didReceiveMessage(connection, decoder);
        return;
    }
#endif
#if ENABLE(WEB_RTC)
    if (decoder.messageReceiverName() == Messages::NetworkMDNSRegister::messageReceiverName()) {
        mdnsRegister().didReceiveMessage(connection, decoder);
        return;
    }
#endif

    if (decoder.messageReceiverName() == Messages::CacheStorageEngineConnection::messageReceiverName()) {
        cacheStorageConnection().didReceiveMessage(connection, decoder);
        return;
    }

#if ENABLE(INDEXED_DATABASE)
    if (decoder.messageReceiverName() == Messages::WebIDBConnectionToClient::messageReceiverName()) {
        auto iterator = m_webIDBConnections.find(decoder.destinationID());
        if (iterator != m_webIDBConnections.end())
            iterator->value->didReceiveMessage(connection, decoder);
        return;
    }
#endif
    
#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWServerConnection::messageReceiverName()) {
        if (auto swConnection = m_swConnections.get(makeObjectIdentifier<SWServerConnectionIdentifierType>(decoder.destinationID())))
            swConnection->didReceiveMessage(connection, decoder);
        return;
    }
    
    if (decoder.messageReceiverName() == Messages::WebSWServerToContextConnection::messageReceiverName()) {
        if (auto* contextConnection = NetworkProcess::singleton().connectionToContextProcessFromIPCConnection(connection)) {
            contextConnection->didReceiveMessage(connection, decoder);
            return;
        }
    }
#endif

    ASSERT_NOT_REACHED();
}

#if USE(LIBWEBRTC)
NetworkRTCProvider& NetworkConnectionToWebProcess::rtcProvider()
{
    if (!m_rtcProvider)
        m_rtcProvider = NetworkRTCProvider::create(*this);
    return *m_rtcProvider;
}
#endif

CacheStorageEngineConnection& NetworkConnectionToWebProcess::cacheStorageConnection()
{
    if (!m_cacheStorageConnection)
        m_cacheStorageConnection = CacheStorageEngineConnection::create(*this);
    return *m_cacheStorageConnection;
}

void NetworkConnectionToWebProcess::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& reply)
{
    if (decoder.messageReceiverName() == Messages::NetworkConnectionToWebProcess::messageReceiverName()) {
        didReceiveSyncNetworkConnectionToWebProcessMessage(connection, decoder, reply);
        return;
    }

#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWServerConnection::messageReceiverName()) {
        if (auto swConnection = m_swConnections.get(makeObjectIdentifier<SWServerConnectionIdentifierType>(decoder.destinationID())))
            swConnection->didReceiveSyncMessage(connection, decoder, reply);
        return;
    }
#endif

    ASSERT_NOT_REACHED();
}

void NetworkConnectionToWebProcess::didClose(IPC::Connection& connection)
{
#if ENABLE(SERVICE_WORKER)
    if (RefPtr<WebSWServerToContextConnection> serverToContextConnection = NetworkProcess::singleton().connectionToContextProcessFromIPCConnection(connection)) {
        // Service Worker process exited.
        NetworkProcess::singleton().connectionToContextProcessWasClosed(serverToContextConnection.releaseNonNull());
        return;
    }
#else
    UNUSED_PARAM(connection);
#endif

    // Protect ourself as we might be otherwise be deleted during this function.
    Ref<NetworkConnectionToWebProcess> protector(*this);

    while (!m_networkResourceLoaders.isEmpty())
        m_networkResourceLoaders.begin()->value->abort();

    // All trackers of resources that were in the middle of being loaded were
    // stopped with the abort() calls above, but we still need to sweep up the
    // root activity trackers.
    stopAllNetworkActivityTracking();

    NetworkBlobRegistry::singleton().connectionToWebProcessDidClose(this);
    NetworkProcess::singleton().removeNetworkConnectionToWebProcess(this);

#if USE(LIBWEBRTC)
    if (m_rtcProvider) {
        m_rtcProvider->close();
        m_rtcProvider = nullptr;
    }
#endif

#if ENABLE(INDEXED_DATABASE)
    auto idbConnections = m_webIDBConnections;
    for (auto& connection : idbConnections.values())
        connection->disconnectedFromWebProcess();
    
    m_webIDBConnections.clear();
#endif
    
#if ENABLE(SERVICE_WORKER)
    unregisterSWConnections();
#endif
}

void NetworkConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference)
{
}

void NetworkConnectionToWebProcess::createSocketStream(URL&& url, PAL::SessionID sessionID, String cachePartition, uint64_t identifier)
{
    ASSERT(!m_networkSocketStreams.contains(identifier));
    WebCore::SourceApplicationAuditToken token = { };
#if PLATFORM(COCOA)
    token = { NetworkProcess::singleton().sourceApplicationAuditData() };
#endif
    m_networkSocketStreams.set(identifier, NetworkSocketStream::create(WTFMove(url), sessionID, cachePartition, identifier, m_connection, WTFMove(token)));
}

void NetworkConnectionToWebProcess::destroySocketStream(uint64_t identifier)
{
    ASSERT(m_networkSocketStreams.get(identifier));
    m_networkSocketStreams.remove(identifier);
}

void NetworkConnectionToWebProcess::cleanupForSuspension(Function<void()>&& completionHandler)
{
#if USE(LIBWEBRTC)
    if (m_rtcProvider) {
        m_rtcProvider->closeListeningSockets(WTFMove(completionHandler));
        return;
    }
#endif
    completionHandler();
}

void NetworkConnectionToWebProcess::endSuspension()
{
#if USE(LIBWEBRTC)
    if (m_rtcProvider)
        m_rtcProvider->authorizeListeningSockets();
#endif
}

void NetworkConnectionToWebProcess::scheduleResourceLoad(NetworkResourceLoadParameters&& loadParameters)
{
    auto identifier = loadParameters.identifier;
    RELEASE_ASSERT(identifier);
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(!m_networkResourceLoaders.contains(identifier));

    auto loader = NetworkResourceLoader::create(WTFMove(loadParameters), *this);
    m_networkResourceLoaders.add(identifier, loader.copyRef());
    loader->start();
}

void NetworkConnectionToWebProcess::performSynchronousLoad(NetworkResourceLoadParameters&& loadParameters, Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply&& reply)
{
    auto identifier = loadParameters.identifier;
    RELEASE_ASSERT(identifier);
    RELEASE_ASSERT(RunLoop::isMain());
    ASSERT(!m_networkResourceLoaders.contains(identifier));

    auto loader = NetworkResourceLoader::create(WTFMove(loadParameters), *this, WTFMove(reply));
    m_networkResourceLoaders.add(identifier, loader.copyRef());
    loader->start();
}

void NetworkConnectionToWebProcess::loadPing(NetworkResourceLoadParameters&& loadParameters)
{
    auto completionHandler = [connection = m_connection.copyRef(), identifier = loadParameters.identifier] (const ResourceError& error, const ResourceResponse& response) {
        connection->send(Messages::NetworkProcessConnection::DidFinishPingLoad(identifier, error, response), 0);
    };

    // PingLoad manages its own lifetime, deleting itself when its purpose has been fulfilled.
    new PingLoad(WTFMove(loadParameters), WTFMove(completionHandler));
}

void NetworkConnectionToWebProcess::setOnLineState(bool isOnLine)
{
    m_connection->send(Messages::NetworkProcessConnection::SetOnLineState(isOnLine), 0);
}

void NetworkConnectionToWebProcess::removeLoadIdentifier(ResourceLoadIdentifier identifier)
{
    RELEASE_ASSERT(identifier);
    RELEASE_ASSERT(RunLoop::isMain());

    RefPtr<NetworkResourceLoader> loader = m_networkResourceLoaders.get(identifier);

    // It's possible we have no loader for this identifier if the NetworkProcess crashed and this was a respawned NetworkProcess.
    if (!loader)
        return;

    // Abort the load now, as the WebProcess won't be able to respond to messages any more which might lead
    // to leaked loader resources (connections, threads, etc).
    loader->abort();
    ASSERT(!m_networkResourceLoaders.contains(identifier));
}

void NetworkConnectionToWebProcess::pageLoadCompleted(uint64_t webPageID)
{
    stopAllNetworkActivityTrackingForPage(webPageID);
}

void NetworkConnectionToWebProcess::setDefersLoading(ResourceLoadIdentifier identifier, bool defers)
{
    RELEASE_ASSERT(identifier);
    RELEASE_ASSERT(RunLoop::isMain());

    RefPtr<NetworkResourceLoader> loader = m_networkResourceLoaders.get(identifier);
    if (!loader)
        return;

    loader->setDefersLoading(defers);
}

void NetworkConnectionToWebProcess::prefetchDNS(const String& hostname)
{
    NetworkProcess::singleton().prefetchDNS(hostname);
}

void NetworkConnectionToWebProcess::preconnectTo(uint64_t preconnectionIdentifier, NetworkResourceLoadParameters&& parameters)
{
#if ENABLE(SERVER_PRECONNECT)
    new PreconnectTask(WTFMove(parameters), [this, protectedThis = makeRef(*this), identifier = preconnectionIdentifier] (const ResourceError& error) {
        didFinishPreconnection(identifier, error);
    });
#else
    UNUSED_PARAM(parameters);
    didFinishPreconnection(preconnectionIdentifier, internalError(parameters.request.url()));
#endif
}

void NetworkConnectionToWebProcess::didFinishPreconnection(uint64_t preconnectionIdentifier, const ResourceError& error)
{
    if (!m_connection->isValid())
        return;

    m_connection->send(Messages::NetworkProcessConnection::DidFinishPreconnection(preconnectionIdentifier, error), 0);
}

static NetworkStorageSession& storageSession(PAL::SessionID sessionID)
{
    ASSERT(sessionID.isValid());
    if (sessionID != PAL::SessionID::defaultSessionID()) {
        if (auto* storageSession = NetworkStorageSession::storageSession(sessionID))
            return *storageSession;

        // Some requests with private browsing mode requested may still be coming shortly after NetworkProcess was told to destroy its session.
        // FIXME: Find a way to track private browsing sessions more rigorously.
        LOG_ERROR("Non-default storage session was requested, but there was no session for it. Please file a bug unless you just disabled private browsing, in which case it's an expected race.");
    }
    return NetworkStorageSession::defaultStorageSession();
}

void NetworkConnectionToWebProcess::startDownload(PAL::SessionID sessionID, DownloadID downloadID, const ResourceRequest& request, const String& suggestedName)
{
    NetworkProcess::singleton().downloadManager().startDownload(this, sessionID, downloadID, request, suggestedName);
}

void NetworkConnectionToWebProcess::convertMainResourceLoadToDownload(PAL::SessionID sessionID, uint64_t mainResourceLoadIdentifier, DownloadID downloadID, const ResourceRequest& request, const ResourceResponse& response)
{
    RELEASE_ASSERT(RunLoop::isMain());

    auto& networkProcess = NetworkProcess::singleton();
    // In case a response is served from service worker, we do not have yet the ability to convert the load.
    if (!mainResourceLoadIdentifier || response.source() == ResourceResponse::Source::ServiceWorker) {
        networkProcess.downloadManager().startDownload(this, sessionID, downloadID, request);
        return;
    }

    NetworkResourceLoader* loader = m_networkResourceLoaders.get(mainResourceLoadIdentifier);
    if (!loader) {
        // If we're trying to download a blob here loader can be null.
        return;
    }

    loader->convertToDownload(downloadID, request, response);
}

void NetworkConnectionToWebProcess::cookiesForDOM(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies, String& cookieString, bool& secureCookiesAccessed)
{
    auto& networkStorageSession = storageSession(sessionID);
    std::tie(cookieString, secureCookiesAccessed) = networkStorageSession.cookiesForDOM(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies);
#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    if (auto session = SessionTracker::networkSession(sessionID)) {
        if (session->shouldLogCookieInformation())
            NetworkResourceLoader::logCookieInformation("NetworkConnectionToWebProcess::cookiesForDOM", reinterpret_cast<const void*>(this), networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, std::nullopt);
    }
#endif
}

void NetworkConnectionToWebProcess::setCookiesFromDOM(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String& cookieString)
{
    auto& networkStorageSession = storageSession(sessionID);
    networkStorageSession.setCookiesFromDOM(firstParty, sameSiteInfo, url, frameID, pageID, cookieString);
#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    if (auto session = SessionTracker::networkSession(sessionID)) {
        if (session->shouldLogCookieInformation())
            NetworkResourceLoader::logCookieInformation("NetworkConnectionToWebProcess::setCookiesFromDOM", reinterpret_cast<const void*>(this), networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, std::nullopt);
    }
#endif
}

void NetworkConnectionToWebProcess::cookiesEnabled(PAL::SessionID sessionID, bool& result)
{
    result = storageSession(sessionID).cookiesEnabled();
}

void NetworkConnectionToWebProcess::cookieRequestHeaderFieldValue(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies, String& cookieString, bool& secureCookiesAccessed)
{
    std::tie(cookieString, secureCookiesAccessed) = storageSession(sessionID).cookieRequestHeaderFieldValue(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies);
}

void NetworkConnectionToWebProcess::getRawCookies(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<Cookie>& result)
{
    storageSession(sessionID).getRawCookies(firstParty, sameSiteInfo, url, frameID, pageID, result);
}

void NetworkConnectionToWebProcess::deleteCookie(PAL::SessionID sessionID, const URL& url, const String& cookieName)
{
    storageSession(sessionID).deleteCookie(url, cookieName);
}

void NetworkConnectionToWebProcess::registerFileBlobURL(const URL& url, const String& path, SandboxExtension::Handle&& extensionHandle, const String& contentType)
{
    RefPtr<SandboxExtension> extension = SandboxExtension::create(WTFMove(extensionHandle));

    NetworkBlobRegistry::singleton().registerFileBlobURL(this, url, path, WTFMove(extension), contentType);
}

void NetworkConnectionToWebProcess::registerBlobURL(const URL& url, Vector<BlobPart>&& blobParts, const String& contentType)
{
    NetworkBlobRegistry::singleton().registerBlobURL(this, url, WTFMove(blobParts), contentType);
}

void NetworkConnectionToWebProcess::registerBlobURLFromURL(const URL& url, const URL& srcURL, bool shouldBypassConnectionCheck)
{
    NetworkBlobRegistry::singleton().registerBlobURL(this, url, srcURL, shouldBypassConnectionCheck);
}

void NetworkConnectionToWebProcess::registerBlobURLOptionallyFileBacked(const URL& url, const URL& srcURL, const String& fileBackedPath, const String& contentType)
{
    NetworkBlobRegistry::singleton().registerBlobURLOptionallyFileBacked(this, url, srcURL, fileBackedPath, contentType);
}

void NetworkConnectionToWebProcess::registerBlobURLForSlice(const URL& url, const URL& srcURL, int64_t start, int64_t end)
{
    NetworkBlobRegistry::singleton().registerBlobURLForSlice(this, url, srcURL, start, end);
}

void NetworkConnectionToWebProcess::unregisterBlobURL(const URL& url)
{
    NetworkBlobRegistry::singleton().unregisterBlobURL(this, url);
}

void NetworkConnectionToWebProcess::blobSize(const URL& url, uint64_t& resultSize)
{
    resultSize = NetworkBlobRegistry::singleton().blobSize(this, url);
}

void NetworkConnectionToWebProcess::writeBlobsToTemporaryFiles(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Vector<RefPtr<BlobDataFileReference>> fileReferences;
    for (auto& url : blobURLs)
        fileReferences.appendVector(NetworkBlobRegistry::singleton().filesInBlob(*this, { { }, url }));

    for (auto& file : fileReferences)
        file->prepareForFileAccess();

    NetworkBlobRegistry::singleton().writeBlobsToTemporaryFiles(blobURLs, [fileReferences = WTFMove(fileReferences), completionHandler = WTFMove(completionHandler)](auto&& fileNames) mutable {
        for (auto& file : fileReferences)
            file->revokeFileAccess();
        completionHandler(WTFMove(fileNames));
    });
}

void NetworkConnectionToWebProcess::setCaptureExtraNetworkLoadMetricsEnabled(bool enabled)
{
    m_captureExtraNetworkLoadMetricsEnabled = enabled;
    if (!m_captureExtraNetworkLoadMetricsEnabled)
        m_networkLoadInformationByID.clear();
}

void NetworkConnectionToWebProcess::ensureLegacyPrivateBrowsingSession()
{
    NetworkProcess::singleton().addWebsiteDataStore(WebsiteDataStoreParameters::legacyPrivateSessionParameters());
}

void NetworkConnectionToWebProcess::removeStorageAccessForFrame(PAL::SessionID sessionID, uint64_t frameID, uint64_t pageID)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (auto* storageSession = NetworkStorageSession::storageSession(sessionID))
        storageSession->removeStorageAccessForFrame(frameID, pageID);
#else
    UNUSED_PARAM(sessionID);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
#endif
}

void NetworkConnectionToWebProcess::removeStorageAccessForAllFramesOnPage(PAL::SessionID sessionID, uint64_t pageID)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (auto* storageSession = NetworkStorageSession::storageSession(sessionID))
        storageSession->removeStorageAccessForAllFramesOnPage(pageID);
#else
    UNUSED_PARAM(sessionID);
    UNUSED_PARAM(pageID);
#endif
}

void NetworkConnectionToWebProcess::addOriginAccessWhitelistEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::addOriginAccessWhitelistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void NetworkConnectionToWebProcess::removeOriginAccessWhitelistEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::removeOriginAccessWhitelistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void NetworkConnectionToWebProcess::resetOriginAccessWhitelists()
{
    SecurityPolicy::resetOriginAccessWhitelists();
}

std::optional<NetworkActivityTracker> NetworkConnectionToWebProcess::startTrackingResourceLoad(uint64_t pageID, ResourceLoadIdentifier resourceID, bool isMainResource, const PAL::SessionID& sessionID)
{
    if (sessionID.isEphemeral())
        return std::nullopt;

    // Either get the existing root activity tracker for this page or create a
    // new one if this is the main resource.

    size_t rootActivityIndex;
    if (isMainResource) {
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

void NetworkConnectionToWebProcess::stopTrackingResourceLoad(ResourceLoadIdentifier resourceID, NetworkActivityTracker::CompletionCode code)
{
    auto itemIndex = findNetworkActivityTracker(resourceID);
    if (itemIndex == notFound)
        return;

    m_networkActivityTrackers[itemIndex].networkActivity.complete(code);
    m_networkActivityTrackers.remove(itemIndex);
}

void NetworkConnectionToWebProcess::stopAllNetworkActivityTracking()
{
    for (auto& activityTracker : m_networkActivityTrackers)
        activityTracker.networkActivity.complete(NetworkActivityTracker::CompletionCode::None);

    m_networkActivityTrackers.clear();
}

void NetworkConnectionToWebProcess::stopAllNetworkActivityTrackingForPage(uint64_t pageID)
{
    for (auto& activityTracker : m_networkActivityTrackers) {
        if (activityTracker.pageID == pageID)
            activityTracker.networkActivity.complete(NetworkActivityTracker::CompletionCode::None);
    }

    m_networkActivityTrackers.removeAllMatching([&](const auto& activityTracker) {
        return activityTracker.pageID == pageID;
    });
}

size_t NetworkConnectionToWebProcess::findRootNetworkActivity(uint64_t pageID)
{
    return m_networkActivityTrackers.findMatching([&](const auto& item) {
        return item.isRootActivity && item.pageID == pageID;
    });
}

size_t NetworkConnectionToWebProcess::findNetworkActivityTracker(ResourceLoadIdentifier resourceID)
{
    return m_networkActivityTrackers.findMatching([&](const auto& item) {
        return item.resourceID == resourceID;
    });
}

#if ENABLE(INDEXED_DATABASE)
static uint64_t generateIDBConnectionToServerIdentifier()
{
    ASSERT(RunLoop::isMain());
    static uint64_t identifier = 0;
    return ++identifier;
}

void NetworkConnectionToWebProcess::establishIDBConnectionToServer(PAL::SessionID sessionID, uint64_t& serverConnectionIdentifier)
{
    serverConnectionIdentifier = generateIDBConnectionToServerIdentifier();
    LOG(IndexedDB, "NetworkConnectionToWebProcess::establishIDBConnectionToServer - %" PRIu64, serverConnectionIdentifier);
    ASSERT(!m_webIDBConnections.contains(serverConnectionIdentifier));
    
    m_webIDBConnections.set(serverConnectionIdentifier, WebIDBConnectionToClient::create(*this, serverConnectionIdentifier, sessionID));
}

void NetworkConnectionToWebProcess::removeIDBConnectionToServer(uint64_t serverConnectionIdentifier)
{
    ASSERT(m_webIDBConnections.contains(serverConnectionIdentifier));
    
    auto connection = m_webIDBConnections.take(serverConnectionIdentifier);
    connection->disconnectedFromWebProcess();
}
#endif
    
#if ENABLE(SERVICE_WORKER)
void NetworkConnectionToWebProcess::unregisterSWConnections()
{
    auto swConnections = WTFMove(m_swConnections);
    for (auto& swConnection : swConnections.values()) {
        if (swConnection)
            swConnection->server().removeConnection(swConnection->identifier());
    }
}

void NetworkConnectionToWebProcess::establishSWServerConnection(PAL::SessionID sessionID, SWServerConnectionIdentifier& serverConnectionIdentifier)
{
    auto& server = NetworkProcess::singleton().swServerForSession(sessionID);
    auto connection = std::make_unique<WebSWServerConnection>(server, m_connection.get(), sessionID);
    
    serverConnectionIdentifier = connection->identifier();
    LOG(ServiceWorker, "NetworkConnectionToWebProcess::establishSWServerConnection - %s", serverConnectionIdentifier.loggingString().utf8().data());

    ASSERT(!m_swConnections.contains(serverConnectionIdentifier));
    m_swConnections.add(serverConnectionIdentifier, makeWeakPtr(*connection));
    server.addConnection(WTFMove(connection));
}
#endif

} // namespace WebKit
