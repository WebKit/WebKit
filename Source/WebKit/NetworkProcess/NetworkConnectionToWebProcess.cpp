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
#include "Logging.h"
#include "NetworkBlobRegistry.h"
#include "NetworkCache.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkLoad.h"
#include "NetworkMDNSRegisterMessages.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "NetworkRTCMonitorMessages.h"
#include "NetworkRTCProviderMessages.h"
#include "NetworkRTCSocketMessages.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoader.h"
#include "NetworkResourceLoaderMessages.h"
#include "NetworkSocketStream.h"
#include "NetworkSocketStreamMessages.h"
#include "PingLoad.h"
#include "PreconnectTask.h"
#include "SessionTracker.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PlatformCookieJar.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/SecurityPolicy.h>
#include <pal/SessionID.h>

using namespace WebCore;

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(true, Network, "%p - NetworkConnectionToWebProcess::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(true, Network, "%p - NetworkConnectionToWebProcess::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

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
    m_connection->open();
}

NetworkConnectionToWebProcess::~NetworkConnectionToWebProcess()
{
    m_connection->invalidate();
#if USE(LIBWEBRTC)
    if (m_rtcProvider)
        m_rtcProvider->close();
#endif
}

void NetworkConnectionToWebProcess::didCleanupResourceLoader(NetworkResourceLoader& loader)
{
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
        auto loaderIterator = m_networkResourceLoaders.find(decoder.destinationID());
        if (loaderIterator != m_networkResourceLoaders.end())
            loaderIterator->value->didReceiveNetworkResourceLoaderMessage(connection, decoder);
        return;
    }

    if (decoder.messageReceiverName() == Messages::NetworkSocketStream::messageReceiverName()) {
        auto socketIterator = m_networkSocketStreams.find(decoder.destinationID());
        if (socketIterator != m_networkSocketStreams.end()) {
            socketIterator->value->didReceiveMessage(connection, decoder);
            if (decoder.messageName() == Messages::NetworkSocketStream::Close::name())
                m_networkSocketStreams.remove(socketIterator);
        }
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
    ASSERT_NOT_REACHED();
}

void NetworkConnectionToWebProcess::didClose(IPC::Connection&)
{
    // Protect ourself as we might be otherwise be deleted during this function.
    Ref<NetworkConnectionToWebProcess> protector(*this);

    for (auto& loader : copyToVector(m_networkResourceLoaders.values()))
        loader->abort();
    ASSERT(m_networkResourceLoaders.isEmpty());

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
    ASSERT(!m_networkResourceLoaders.contains(identifier));

    auto loader = NetworkResourceLoader::create(WTFMove(loadParameters), *this);
    m_networkResourceLoaders.add(identifier, loader.ptr());
    loader->start();
}

void NetworkConnectionToWebProcess::performSynchronousLoad(NetworkResourceLoadParameters&& loadParameters, Ref<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply>&& reply)
{
    auto identifier = loadParameters.identifier;
    ASSERT(!m_networkResourceLoaders.contains(identifier));

    auto loader = NetworkResourceLoader::create(WTFMove(loadParameters), *this, WTFMove(reply));
    m_networkResourceLoaders.add(identifier, loader.ptr());
    loader->start();
}

void NetworkConnectionToWebProcess::loadPing(NetworkResourceLoadParameters&& loadParameters)
{
    auto completionHandler = [this, protectedThis = makeRef(*this), identifier = loadParameters.identifier] (const ResourceError& error, const ResourceResponse& response) {
        didFinishPingLoad(identifier, error, response);
    };

    // PingLoad manages its own lifetime, deleting itself when its purpose has been fulfilled.
    new PingLoad(WTFMove(loadParameters), WTFMove(completionHandler));
}

void NetworkConnectionToWebProcess::didFinishPingLoad(uint64_t pingLoadIdentifier, const ResourceError& error, const ResourceResponse& response)
{
    if (!m_connection->isValid())
        return;

    m_connection->send(Messages::NetworkProcessConnection::DidFinishPingLoad(pingLoadIdentifier, error, response), 0);
}

void NetworkConnectionToWebProcess::setOnLineState(bool isOnLine)
{
    m_connection->send(Messages::NetworkProcessConnection::SetOnLineState(isOnLine), 0);
}

void NetworkConnectionToWebProcess::removeLoadIdentifier(ResourceLoadIdentifier identifier)
{
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
    std::tie(cookieString, secureCookiesAccessed) = WebCore::cookiesForDOM(networkStorageSession, firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies);
#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
    if (NetworkProcess::singleton().shouldLogCookieInformation())
        NetworkResourceLoader::logCookieInformation("NetworkConnectionToWebProcess::cookiesForDOM", reinterpret_cast<const void*>(this), networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, std::nullopt);
#endif
}

void NetworkConnectionToWebProcess::setCookiesFromDOM(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String& cookieString)
{
    auto& networkStorageSession = storageSession(sessionID);
    WebCore::setCookiesFromDOM(networkStorageSession, firstParty, sameSiteInfo, url, frameID, pageID, cookieString);
#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
    if (NetworkProcess::singleton().shouldLogCookieInformation())
        NetworkResourceLoader::logCookieInformation("NetworkConnectionToWebProcess::setCookiesFromDOM", reinterpret_cast<const void*>(this), networkStorageSession, firstParty, sameSiteInfo, url, emptyString(), frameID, pageID, std::nullopt);
#endif
}

void NetworkConnectionToWebProcess::cookiesEnabled(PAL::SessionID sessionID, bool& result)
{
    result = WebCore::cookiesEnabled(storageSession(sessionID));
}

void NetworkConnectionToWebProcess::cookieRequestHeaderFieldValue(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies, String& cookieString, bool& secureCookiesAccessed)
{
    std::tie(cookieString, secureCookiesAccessed) = WebCore::cookieRequestHeaderFieldValue(storageSession(sessionID), firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies);
}

void NetworkConnectionToWebProcess::getRawCookies(PAL::SessionID sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<Cookie>& result)
{
    WebCore::getRawCookies(storageSession(sessionID), firstParty, sameSiteInfo, url, frameID, pageID, result);
}

void NetworkConnectionToWebProcess::deleteCookie(PAL::SessionID sessionID, const URL& url, const String& cookieName)
{
    WebCore::deleteCookie(storageSession(sessionID), url, cookieName);
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

void NetworkConnectionToWebProcess::preregisterSandboxExtensionsForOptionallyFileBackedBlob(const Vector<String>& filePaths, SandboxExtension::HandleArray&& handles)
{
#if ENABLE(SANDBOX_EXTENSIONS)
    ASSERT(filePaths.size() == handles.size());

    for (size_t i = 0; i < filePaths.size(); ++i)
        m_blobDataFileReferences.add(filePaths[i], BlobDataFileReferenceWithSandboxExtension::create(filePaths[i], SandboxExtension::create(WTFMove(handles[i]))));
#else
    for (size_t i = 0; i < filePaths.size(); ++i)
        m_blobDataFileReferences.add(filePaths[i], BlobDataFileReferenceWithSandboxExtension::create(filePaths[i], nullptr));
#endif
}

RefPtr<WebCore::BlobDataFileReference> NetworkConnectionToWebProcess::getBlobDataFileReferenceForPath(const String& path)
{
    ASSERT(m_blobDataFileReferences.contains(path));
    return m_blobDataFileReferences.get(path);
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

void NetworkConnectionToWebProcess::writeBlobsToTemporaryFiles(const Vector<String>& blobURLs, uint64_t requestIdentifier)
{
    Vector<RefPtr<BlobDataFileReference>> fileReferences;
    for (auto& url : blobURLs)
        fileReferences.appendVector(NetworkBlobRegistry::singleton().filesInBlob(*this, { ParsedURLString, url }));

    for (auto& file : fileReferences)
        file->prepareForFileAccess();

    NetworkBlobRegistry::singleton().writeBlobsToTemporaryFiles(blobURLs, [this, protectedThis = makeRef(*this), requestIdentifier, fileReferences = WTFMove(fileReferences)](auto& fileNames) mutable {
        for (auto& file : fileReferences)
            file->revokeFileAccess();

        NetworkProcess::singleton().grantSandboxExtensionsToStorageProcessForBlobs(fileNames, [this, protectedThis = WTFMove(protectedThis), requestIdentifier, fileNames]() {
            if (!m_connection->isValid())
                return;

            m_connection->send(Messages::NetworkProcessConnection::DidWriteBlobsToTemporaryFiles(requestIdentifier, fileNames), 0);
        });
    });
}

void NetworkConnectionToWebProcess::storeDerivedDataToCache(const WebKit::NetworkCache::DataKey& dataKey, const IPC::DataReference& data)
{
    if (auto* cache = NetworkProcess::singleton().cache())
        cache->storeData(dataKey, data.data(), data.size());
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
#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
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
#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
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

static bool networkActivityTrackingEnabled()
{
    return NetworkProcess::singleton().trackNetworkActivity();
}

std::optional<NetworkActivityTracker> NetworkConnectionToWebProcess::startTrackingResourceLoad(uint64_t pageID, ResourceLoadIdentifier resourceID, bool isMainResource)
{
    if (!networkActivityTrackingEnabled())
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
    if (!networkActivityTrackingEnabled())
        return;

    auto itemIndex = findNetworkActivityTracker(resourceID);
    if (itemIndex == notFound) {
        RELEASE_LOG_ERROR(Network, "stopTrackingResourceLoad: Unable to find network activity for resource: %d", static_cast<int>(resourceID));
        return;
    }

    m_networkActivityTrackers[itemIndex].networkActivity.complete(code);
    m_networkActivityTrackers.remove(itemIndex);
}

void NetworkConnectionToWebProcess::stopAllNetworkActivityTracking()
{
    if (!networkActivityTrackingEnabled())
        return;

    for (auto& activityTracker : m_networkActivityTrackers)
        activityTracker.networkActivity.complete(NetworkActivityTracker::CompletionCode::None);

    m_networkActivityTrackers.clear();
}

void NetworkConnectionToWebProcess::stopAllNetworkActivityTrackingForPage(uint64_t pageID)
{
    if (!networkActivityTrackingEnabled())
        return;

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

} // namespace WebKit
