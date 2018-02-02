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
#include "NetworkResourceLoader.h"

#include "DataReference.h"
#include "Logging.h"
#include "NetworkBlobRegistry.h"
#include "NetworkCache.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkLoad.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "SessionTracker.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebResourceLoaderMessages.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/BlobDataFileReference.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SynchronousLoaderClient.h>
#include <wtf/CurrentTime.h>
#include <wtf/RunLoop.h>

#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PlatformCookieJar.h>
#endif

using namespace WebCore;

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkResourceLoader::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkResourceLoader::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

struct NetworkResourceLoader::SynchronousLoadData {
    SynchronousLoadData(RefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply>&& reply)
        : delayedReply(WTFMove(reply))
    {
        ASSERT(delayedReply);
    }
    ResourceRequest currentRequest;
    RefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply> delayedReply;
    ResourceResponse response;
    ResourceError error;
};

static void sendReplyToSynchronousRequest(NetworkResourceLoader::SynchronousLoadData& data, const SharedBuffer* buffer)
{
    ASSERT(data.delayedReply);
    ASSERT(!data.response.isNull() || !data.error.isNull());

    Vector<char> responseBuffer;
    if (buffer && buffer->size())
        responseBuffer.append(buffer->data(), buffer->size());

    data.delayedReply->send(data.error, data.response, responseBuffer);
    data.delayedReply = nullptr;
}

NetworkResourceLoader::NetworkResourceLoader(const NetworkResourceLoadParameters& parameters, NetworkConnectionToWebProcess& connection, RefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply>&& synchronousReply)
    : m_parameters { parameters }
    , m_connection { connection }
    , m_defersLoading { parameters.defersLoading }
    , m_isAllowedToAskUserForCredentials { parameters.clientCredentialPolicy == ClientCredentialPolicy::MayAskClientForCredentials }
    , m_bufferingTimer { *this, &NetworkResourceLoader::bufferingTimerFired }
    , m_cache { sessionID().isEphemeral() ? nullptr : NetworkProcess::singleton().cache() }
{
    ASSERT(RunLoop::isMain());
    // FIXME: This is necessary because of the existence of EmptyFrameLoaderClient in WebCore.
    //        Once bug 116233 is resolved, this ASSERT can just be "m_webPageID && m_webFrameID"
    ASSERT((m_parameters.webPageID && m_parameters.webFrameID) || m_parameters.clientCredentialPolicy == ClientCredentialPolicy::CannotAskClientForCredentials);

    if (originalRequest().httpBody()) {
        for (const auto& element : originalRequest().httpBody()->elements()) {
            if (element.m_type == FormDataElement::Type::EncodedBlob)
                m_fileReferences.appendVector(NetworkBlobRegistry::singleton().filesInBlob(connection, element.m_url));
        }
    }

    if (synchronousReply)
        m_synchronousLoadData = std::make_unique<SynchronousLoadData>(WTFMove(synchronousReply));
}

NetworkResourceLoader::~NetworkResourceLoader()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_networkLoad);
    ASSERT(!isSynchronous() || !m_synchronousLoadData->delayedReply);
}

bool NetworkResourceLoader::canUseCache(const ResourceRequest& request) const
{
    if (!m_cache)
        return false;
    ASSERT(!sessionID().isEphemeral());

    if (!request.url().protocolIsInHTTPFamily())
        return false;
    if (originalRequest().cachePolicy() == WebCore::DoNotUseAnyCache)
        return false;

    return true;
}

bool NetworkResourceLoader::canUseCachedRedirect(const ResourceRequest& request) const
{
    if (!canUseCache(request))
        return false;
    // Limit cached redirects to avoid cycles and other trouble.
    // Networking layer follows over 30 redirects but caching that many seems unnecessary.
    static const unsigned maximumCachedRedirectCount { 5 };
    if (m_redirectCount > maximumCachedRedirectCount)
        return false;

    return true;
}

bool NetworkResourceLoader::isSynchronous() const
{
    return !!m_synchronousLoadData;
}

void NetworkResourceLoader::start()
{
    ASSERT(RunLoop::isMain());

    if (m_defersLoading) {
        RELEASE_LOG_IF_ALLOWED("start: Loading is deferred (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
        return;
    }

    ASSERT(!m_wasStarted);
    m_wasStarted = true;

    if (canUseCache(originalRequest())) {
        RELEASE_LOG_IF_ALLOWED("start: Checking cache for resource (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
        retrieveCacheEntry(originalRequest());
        return;
    }

    startNetworkLoad(originalRequest());
}

void NetworkResourceLoader::retrieveCacheEntry(const ResourceRequest& request)
{
    ASSERT(canUseCache(request));

    RefPtr<NetworkResourceLoader> loader(this);
    m_cache->retrieve(request, { m_parameters.webPageID, m_parameters.webFrameID }, [this, loader = WTFMove(loader), request](auto entry) {
        if (loader->hasOneRef()) {
            // The loader has been aborted and is only held alive by this lambda.
            return;
        }
        if (!entry) {
            RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Resource not in cache (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
            loader->startNetworkLoad(request);
            return;
        }
        if (entry->redirectRequest()) {
            RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Handling redirect (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
            loader->dispatchWillSendRequestForCacheEntry(WTFMove(entry));
            return;
        }
        if (loader->m_parameters.needsCertificateInfo && !entry->response().certificateInfo()) {
            RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Resource does not have required certificate (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
            loader->startNetworkLoad(request);
            return;
        }
        if (entry->needsValidation() || request.cachePolicy() == WebCore::RefreshAnyCacheData) {
            RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Validating cache entry (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
            loader->validateCacheEntry(WTFMove(entry));
            return;
        }
        RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Retrieved resource from cache (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
        loader->didRetrieveCacheEntry(WTFMove(entry));
    });
}

void NetworkResourceLoader::startNetworkLoad(const ResourceRequest& request)
{
    RELEASE_LOG_IF_ALLOWED("startNetworkLoad: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());

    consumeSandboxExtensions();

    if (isSynchronous() || m_parameters.maximumBufferingTime > 0_s)
        m_bufferedData = SharedBuffer::create();

    if (canUseCache(request))
        m_bufferedDataForCache = SharedBuffer::create();

    NetworkLoadParameters parameters = m_parameters;
    parameters.defersLoading = m_defersLoading;
    parameters.request = request;

    if (request.url().protocolIsBlob())
        parameters.blobFileReferences = NetworkBlobRegistry::singleton().filesInBlob(m_connection, originalRequest().url());

    auto* networkSession = SessionTracker::networkSession(parameters.sessionID);
    if (!networkSession && parameters.sessionID.isEphemeral()) {
        NetworkProcess::singleton().addWebsiteDataStore(WebsiteDataStoreParameters::privateSessionParameters(parameters.sessionID));
        networkSession = SessionTracker::networkSession(parameters.sessionID);
    }
    if (!networkSession) {
        WTFLogAlways("Attempted to create a NetworkLoad with a session (id=%" PRIu64 ") that does not exist.", parameters.sessionID.sessionID());
        RELEASE_LOG_ERROR_IF_ALLOWED("startNetworkLoad: Attempted to create a NetworkLoad with a session that does not exist (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", sessionID=%" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, parameters.sessionID.sessionID());
        NetworkProcess::singleton().logDiagnosticMessage(m_parameters.webPageID, WebCore::DiagnosticLoggingKeys::internalErrorKey(), WebCore::DiagnosticLoggingKeys::invalidSessionIDKey(), WebCore::ShouldSample::No);
        didFailLoading(internalError(request.url()));
        return;
    }
    m_networkLoad = std::make_unique<NetworkLoad>(*this, WTFMove(parameters), *networkSession);

    if (m_defersLoading) {
        RELEASE_LOG_IF_ALLOWED("startNetworkLoad: Created, but deferred (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")",
            m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);
    }
}

void NetworkResourceLoader::setDefersLoading(bool defers)
{
    if (m_defersLoading == defers)
        return;
    m_defersLoading = defers;

    if (defers)
        RELEASE_LOG_IF_ALLOWED("setDefersLoading: Deferring resource load (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);
    else
        RELEASE_LOG_IF_ALLOWED("setDefersLoading: Resuming deferred resource load (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);

    if (m_networkLoad) {
        m_networkLoad->setDefersLoading(defers);
        return;
    }

    if (!m_defersLoading && !m_wasStarted)
        start();
    else
        RELEASE_LOG_IF_ALLOWED("setDefersLoading: defers = %d, but nothing to do (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_defersLoading, m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);
}

void NetworkResourceLoader::cleanup()
{
    ASSERT(RunLoop::isMain());

    m_bufferingTimer.stop();

    invalidateSandboxExtensions();

    m_networkLoad = nullptr;

    // This will cause NetworkResourceLoader to be destroyed and therefore we do it last.
    m_connection->didCleanupResourceLoader(*this);
}

void NetworkResourceLoader::convertToDownload(DownloadID downloadID, const ResourceRequest& request, const ResourceResponse& response)
{
    ASSERT(m_networkLoad);
    NetworkProcess::singleton().downloadManager().convertNetworkLoadToDownload(downloadID, std::exchange(m_networkLoad, nullptr), WTFMove(m_fileReferences), request, response);
}

void NetworkResourceLoader::abort()
{
    ASSERT(RunLoop::isMain());

    RELEASE_LOG_IF_ALLOWED("abort: Canceling resource load (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")",
        m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);

    if (m_networkLoad) {
        if (canUseCache(m_networkLoad->currentRequest())) {
            // We might already have used data from this incomplete load. Ensure older versions don't remain in the cache after cancel.
            if (!m_response.isNull())
                m_cache->remove(m_networkLoad->currentRequest());
        }
        m_networkLoad->cancel();
    }

    cleanup();
}

auto NetworkResourceLoader::didReceiveResponse(ResourceResponse&& receivedResponse) -> ShouldContinueDidReceiveResponse
{
    RELEASE_LOG_IF_ALLOWED("didReceiveResponse: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", httpStatusCode = %d, length = %" PRId64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, receivedResponse.httpStatusCode(), receivedResponse.expectedContentLength());

    m_response = WTFMove(receivedResponse);

    // For multipart/x-mixed-replace didReceiveResponseAsync gets called multiple times and buffering would require special handling.
    if (!isSynchronous() && m_response.isMultipart())
        m_bufferedData = nullptr;

    if (m_response.isMultipart())
        m_bufferedDataForCache = nullptr;

    if (m_cacheEntryForValidation) {
        bool validationSucceeded = m_response.httpStatusCode() == 304; // 304 Not Modified
        if (validationSucceeded) {
            m_cacheEntryForValidation = m_cache->update(originalRequest(), { m_parameters.webPageID, m_parameters.webFrameID }, *m_cacheEntryForValidation, m_response);
            // If the request was conditional then this revalidation was not triggered by the network cache and we pass the 304 response to WebCore.
            if (originalRequest().isConditional())
                m_cacheEntryForValidation = nullptr;
        } else
            m_cacheEntryForValidation = nullptr;
    }
    bool shouldSendDidReceiveResponse = !m_cacheEntryForValidation;

    bool shouldWaitContinueDidReceiveResponse = isMainResource();
    if (shouldSendDidReceiveResponse) {
        if (isSynchronous())
            m_synchronousLoadData->response = m_response;
        else
            send(Messages::WebResourceLoader::DidReceiveResponse(m_response, shouldWaitContinueDidReceiveResponse));
    }

    // For main resources, the web process is responsible for sending back a NetworkResourceLoader::ContinueDidReceiveResponse message.
    bool shouldContinueDidReceiveResponse = !shouldWaitContinueDidReceiveResponse || m_cacheEntryForValidation;

    if (shouldContinueDidReceiveResponse) {
        RELEASE_LOG_IF_ALLOWED("didReceiveResponse: Should not wait for message from WebContent process before continuing resource load (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);
        return ShouldContinueDidReceiveResponse::Yes;
    }

    RELEASE_LOG_IF_ALLOWED("didReceiveResponse: Should wait for message from WebContent process before continuing resource load (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);
    return ShouldContinueDidReceiveResponse::No;
}

void NetworkResourceLoader::didReceiveBuffer(Ref<SharedBuffer>&& buffer, int reportedEncodedDataLength)
{
    if (!m_numBytesReceived) {
        RELEASE_LOG_IF_ALLOWED("didReceiveBuffer: Started receiving data (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);
    }
    m_numBytesReceived += buffer->size();

    ASSERT(!m_cacheEntryForValidation);

    if (m_bufferedDataForCache) {
        // Prevent memory growth in case of streaming data.
        const size_t maximumCacheBufferSize = 10 * 1024 * 1024;
        if (m_bufferedDataForCache->size() + buffer->size() <= maximumCacheBufferSize)
            m_bufferedDataForCache->append(buffer.get());
        else
            m_bufferedDataForCache = nullptr;
    }
    // FIXME: At least on OS X Yosemite we always get -1 from the resource handle.
    unsigned encodedDataLength = reportedEncodedDataLength >= 0 ? reportedEncodedDataLength : buffer->size();

    m_bytesReceived += buffer->size();
    if (m_bufferedData) {
        m_bufferedData->append(buffer.get());
        m_bufferedDataEncodedDataLength += encodedDataLength;
        startBufferingTimerIfNeeded();
        return;
    }
    sendBuffer(buffer, encodedDataLength);
}

void NetworkResourceLoader::didFinishLoading(const NetworkLoadMetrics& networkLoadMetrics)
{
    RELEASE_LOG_IF_ALLOWED("didFinishLoading: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", length = %zd)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, m_numBytesReceived);

    if (m_cacheEntryForValidation) {
        // 304 Not Modified
        ASSERT(m_response.httpStatusCode() == 304);
        LOG(NetworkCache, "(NetworkProcess) revalidated");
        didRetrieveCacheEntry(WTFMove(m_cacheEntryForValidation));
        return;
    }

#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
    if (shouldLogCookieInformation())
        logCookieInformation();
#endif

    if (isSynchronous())
        sendReplyToSynchronousRequest(*m_synchronousLoadData, m_bufferedData.get());
    else {
        if (m_bufferedData && !m_bufferedData->isEmpty()) {
            // FIXME: Pass a real value or remove the encoded data size feature.
            sendBuffer(*m_bufferedData, -1);
        }
        send(Messages::WebResourceLoader::DidFinishResourceLoad(networkLoadMetrics));
    }

    tryStoreAsCacheEntry();

    cleanup();
}

void NetworkResourceLoader::didFailLoading(const ResourceError& error)
{
    RELEASE_LOG_IF_ALLOWED("didFailLoading: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isTimeout = %d, isCancellation = %d, errCode = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, error.isTimeout(), error.isCancellation(), error.errorCode());

    ASSERT(!error.isNull());

    m_cacheEntryForValidation = nullptr;

    if (isSynchronous()) {
        m_synchronousLoadData->error = error;
        sendReplyToSynchronousRequest(*m_synchronousLoadData, nullptr);
    } else if (auto* connection = messageSenderConnection())
        connection->send(Messages::WebResourceLoader::DidFailResourceLoad(error), messageSenderDestinationID());

    cleanup();
}

void NetworkResourceLoader::willSendRedirectedRequest(ResourceRequest&& request, WebCore::ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse)
{
    ++m_redirectCount;

    if (isSynchronous()) {
        ResourceRequest overridenRequest = redirectRequest;
        // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
        // This includes at least updating host records, and comparing the current request instead of the original request here.
        if (!protocolHostAndPortAreEqual(originalRequest().url(), redirectRequest.url())) {
            ASSERT(m_synchronousLoadData->error.isNull());
            m_synchronousLoadData->error = SynchronousLoaderClient::platformBadResponseError();
            m_networkLoad->clearCurrentRequest();
            overridenRequest = ResourceRequest();
        }
        // We do not support prompting for credentials for synchronous loads. If we ever change this policy then
        // we need to take care to prompt if and only if request and redirectRequest are not mixed content.
        continueWillSendRequest(WTFMove(overridenRequest), false);
        return;
    }
    send(Messages::WebResourceLoader::WillSendRequest(redirectRequest, redirectResponse));

    if (canUseCachedRedirect(request))
        m_cache->storeRedirect(request, redirectResponse, redirectRequest);
}

void NetworkResourceLoader::continueWillSendRequest(ResourceRequest&& newRequest, bool isAllowedToAskUserForCredentials)
{
    RELEASE_LOG_IF_ALLOWED("continueWillSendRequest: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);

    m_isAllowedToAskUserForCredentials = isAllowedToAskUserForCredentials;

    // If there is a match in the network cache, we need to reuse the original cache policy and partition.
    newRequest.setCachePolicy(originalRequest().cachePolicy());
    newRequest.setCachePartition(originalRequest().cachePartition());

    if (m_isWaitingContinueWillSendRequestForCachedRedirect) {
        m_isWaitingContinueWillSendRequestForCachedRedirect = false;

        LOG(NetworkCache, "(NetworkProcess) Retrieving cached redirect");

        if (canUseCachedRedirect(newRequest))
            retrieveCacheEntry(newRequest);
        else
            startNetworkLoad(newRequest);

        return;
    }

    if (m_networkLoad)
        m_networkLoad->continueWillSendRequest(WTFMove(newRequest));
}

void NetworkResourceLoader::continueDidReceiveResponse()
{
    // FIXME: Remove this check once BlobResourceHandle implements didReceiveResponseAsync correctly.
    // Currently, it does not wait for response, so the load is likely to finish before continueDidReceiveResponse.
    if (m_networkLoad)
        m_networkLoad->continueDidReceiveResponse();
}

void NetworkResourceLoader::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    if (!isSynchronous())
        send(Messages::WebResourceLoader::DidSendData(bytesSent, totalBytesToBeSent));
}

void NetworkResourceLoader::startBufferingTimerIfNeeded()
{
    if (isSynchronous())
        return;
    if (m_bufferingTimer.isActive())
        return;
    m_bufferingTimer.startOneShot(m_parameters.maximumBufferingTime);
}

void NetworkResourceLoader::bufferingTimerFired()
{
    ASSERT(m_bufferedData);
    ASSERT(m_networkLoad);

    if (m_bufferedData->isEmpty())
        return;

    IPC::SharedBufferDataReference dataReference(m_bufferedData.get());
    size_t encodedLength = m_bufferedDataEncodedDataLength;

    m_bufferedData = SharedBuffer::create();
    m_bufferedDataEncodedDataLength = 0;

    send(Messages::WebResourceLoader::DidReceiveData(dataReference, encodedLength));
}

void NetworkResourceLoader::sendBuffer(SharedBuffer& buffer, size_t encodedDataLength)
{
    ASSERT(!isSynchronous());

    IPC::SharedBufferDataReference dataReference(&buffer);
    send(Messages::WebResourceLoader::DidReceiveData(dataReference, encodedDataLength));
}

void NetworkResourceLoader::tryStoreAsCacheEntry()
{
    if (!canUseCache(m_networkLoad->currentRequest()))
        return;
    if (!m_bufferedDataForCache)
        return;

    m_cache->store(m_networkLoad->currentRequest(), m_response, WTFMove(m_bufferedDataForCache), [loader = makeRef(*this)](auto& mappedBody) mutable {
#if ENABLE(SHAREABLE_RESOURCE)
        if (mappedBody.shareableResourceHandle.isNull())
            return;
        LOG(NetworkCache, "(NetworkProcess) sending DidCacheResource");
        loader->send(Messages::NetworkProcessConnection::DidCacheResource(loader->originalRequest(), mappedBody.shareableResourceHandle, loader->sessionID()));
#endif
    });
}

void NetworkResourceLoader::didRetrieveCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    if (isSynchronous()) {
        m_synchronousLoadData->response = entry->response();
        sendReplyToSynchronousRequest(*m_synchronousLoadData, entry->buffer());
        cleanup();
        return;
    }

    bool needsContinueDidReceiveResponseMessage = isMainResource();
    send(Messages::WebResourceLoader::DidReceiveResponse(entry->response(), needsContinueDidReceiveResponseMessage));

    if (entry->sourceStorageRecord().bodyHash && !m_parameters.derivedCachedDataTypesToRetrieve.isEmpty()) {
        auto bodyHash = *entry->sourceStorageRecord().bodyHash;
        auto* entryPtr = entry.release();
        auto retrieveCount = m_parameters.derivedCachedDataTypesToRetrieve.size();

        for (auto& type : m_parameters.derivedCachedDataTypesToRetrieve) {
            NetworkCache::DataKey key { originalRequest().cachePartition(), type, bodyHash };
            m_cache->retrieveData(key, [loader = makeRef(*this), entryPtr, type, retrieveCount] (const uint8_t* data, size_t size) mutable {
                loader->m_retrievedDerivedDataCount++;
                bool retrievedAll = loader->m_retrievedDerivedDataCount == retrieveCount;
                std::unique_ptr<NetworkCache::Entry> entry(retrievedAll ? entryPtr : nullptr);
                if (loader->hasOneRef())
                    return;
                if (data) {
                    IPC::DataReference dataReference(data, size);
                    loader->send(Messages::WebResourceLoader::DidRetrieveDerivedData(type, dataReference));
                }
                if (retrievedAll) {
                    loader->sendResultForCacheEntry(WTFMove(entry));
                    loader->cleanup();
                }
            });
        }
        return;
    }

    sendResultForCacheEntry(WTFMove(entry));

    cleanup();
}

void NetworkResourceLoader::sendResultForCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
#if ENABLE(SHAREABLE_RESOURCE)
    if (!entry->shareableResourceHandle().isNull()) {
        send(Messages::WebResourceLoader::DidReceiveResource(entry->shareableResourceHandle()));
        return;
    }
#endif

#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
    if (shouldLogCookieInformation())
        logCookieInformation();
#endif

    WebCore::NetworkLoadMetrics networkLoadMetrics;
    networkLoadMetrics.markComplete();
    networkLoadMetrics.requestHeaderBytesSent = 0;
    networkLoadMetrics.requestBodyBytesSent = 0;
    networkLoadMetrics.responseHeaderBytesReceived = 0;
    networkLoadMetrics.responseBodyBytesReceived = 0;
    networkLoadMetrics.responseBodyDecodedSize = 0;

    sendBuffer(*entry->buffer(), entry->buffer()->size());
    send(Messages::WebResourceLoader::DidFinishResourceLoad(networkLoadMetrics));
}

void NetworkResourceLoader::validateCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    ASSERT(!m_networkLoad);

    // If the request is already conditional then the revalidation was not triggered by the disk cache
    // and we should not overwrite the existing conditional headers.
    ResourceRequest revalidationRequest = originalRequest();
    if (!revalidationRequest.isConditional()) {
        String eTag = entry->response().httpHeaderField(HTTPHeaderName::ETag);
        String lastModified = entry->response().httpHeaderField(HTTPHeaderName::LastModified);
        if (!eTag.isEmpty())
            revalidationRequest.setHTTPHeaderField(HTTPHeaderName::IfNoneMatch, eTag);
        if (!lastModified.isEmpty())
            revalidationRequest.setHTTPHeaderField(HTTPHeaderName::IfModifiedSince, lastModified);
    }

    m_cacheEntryForValidation = WTFMove(entry);

    startNetworkLoad(revalidationRequest);
}

void NetworkResourceLoader::dispatchWillSendRequestForCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    ASSERT(entry->redirectRequest());
    ASSERT(!m_isWaitingContinueWillSendRequestForCachedRedirect);

    LOG(NetworkCache, "(NetworkProcess) Executing cached redirect");

    ++m_redirectCount;
    send(Messages::WebResourceLoader::WillSendRequest(*entry->redirectRequest(), entry->response()));
    m_isWaitingContinueWillSendRequestForCachedRedirect = true;
}

IPC::Connection* NetworkResourceLoader::messageSenderConnection()
{
    return &connectionToWebProcess().connection();
}

void NetworkResourceLoader::consumeSandboxExtensions()
{
    ASSERT(!m_didConsumeSandboxExtensions);

    for (auto& extension : m_parameters.requestBodySandboxExtensions)
        extension->consume();

    if (auto& extension = m_parameters.resourceSandboxExtension)
        extension->consume();

    for (auto& fileReference : m_fileReferences)
        fileReference->prepareForFileAccess();

    m_didConsumeSandboxExtensions = true;
}

void NetworkResourceLoader::invalidateSandboxExtensions()
{
    if (m_didConsumeSandboxExtensions) {
        for (auto& extension : m_parameters.requestBodySandboxExtensions)
            extension->revoke();
        if (auto& extension = m_parameters.resourceSandboxExtension)
            extension->revoke();
        for (auto& fileReference : m_fileReferences)
            fileReference->revokeFileAccess();

        m_didConsumeSandboxExtensions = false;
    }

    m_fileReferences.clear();
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void NetworkResourceLoader::canAuthenticateAgainstProtectionSpaceAsync(const ProtectionSpace& protectionSpace)
{
    NetworkProcess::singleton().canAuthenticateAgainstProtectionSpace(*this, protectionSpace);
}

void NetworkResourceLoader::continueCanAuthenticateAgainstProtectionSpace(bool result)
{
    if (m_networkLoad)
        m_networkLoad->continueCanAuthenticateAgainstProtectionSpace(result);
}
#endif

bool NetworkResourceLoader::isAlwaysOnLoggingAllowed() const
{
    return sessionID().isAlwaysOnLoggingAllowed();
}

bool NetworkResourceLoader::shouldCaptureExtraNetworkLoadMetrics() const
{
    return m_connection->captureExtraNetworkLoadMetricsEnabled();
}

#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
bool NetworkResourceLoader::shouldLogCookieInformation() const
{
    return NetworkProcess::singleton().shouldLogCookieInformation();
}

void NetworkResourceLoader::logCookieInformation() const
{
    ASSERT(shouldLogCookieInformation());

    auto networkStorageSession = WebCore::NetworkStorageSession::storageSession(sessionID());
    ASSERT(networkStorageSession);

#define LOCAL_LOG(str, ...) \
    RELEASE_LOG_IF_ALLOWED("logCookieInformation: pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ": " str, pageID(), frameID(), identifier(), ##__VA_ARGS__)

    auto escapeForJSON = [](String s) {
        return s.replace('\\', "\\\\").replace('"', "\\\"");
    };

    auto url = originalRequest().url();
    if (networkStorageSession->shouldBlockCookies(originalRequest())) {
        auto escapedURL = escapeForJSON(url.string());
        auto escapedReferrer = escapeForJSON(originalRequest().httpReferrer());

        LOCAL_LOG(R"({ "url": "%{public}s",)", escapedURL.utf8().data());
        LOCAL_LOG(R"(  "partition": "%{public}s",)", "BLOCKED");
        LOCAL_LOG(R"(  "hasStorageAccess": %{public}s,)", "false");
        LOCAL_LOG(R"(  "referer": "%{public}s",)", escapedReferrer.utf8().data());
        LOCAL_LOG(R"(  "cookies": []})");
        return;
    }

    auto partition = WebCore::URL(ParsedURLString, networkStorageSession->cookieStoragePartition(originalRequest(), frameID(), pageID()));
    bool hasStorageAccessForFrame = networkStorageSession->hasStorageAccessForFrame(originalRequest(), frameID(), pageID());

    Vector<WebCore::Cookie> cookies;
    bool result = WebCore::getRawCookies(*networkStorageSession, partition, url, frameID(), pageID(), cookies);

    if (result) {
        auto escapedURL = escapeForJSON(url.string());
        auto escapedPartition = escapeForJSON(partition.string());
        auto escapedReferrer = escapeForJSON(originalRequest().httpReferrer());

        LOCAL_LOG(R"({ "url": "%{public}s",)", escapedURL.utf8().data());
        LOCAL_LOG(R"(  "partition": "%{public}s",)", escapedPartition.utf8().data());
        LOCAL_LOG(R"(  "hasStorageAccess": %{public}s,)", hasStorageAccessForFrame ? "true" : "false");
        LOCAL_LOG(R"(  "referer": "%{public}s",)", escapedReferrer.utf8().data());
        LOCAL_LOG(R"(  "cookies": [)");

        auto size = cookies.size();
        decltype(size) count = 0;
        for (const auto& cookie : cookies) {
            const char* trailingComma = ",";
            if (++count == size)
                trailingComma = "";

            auto escapedName = escapeForJSON(cookie.name);
            auto escapedValue = escapeForJSON(cookie.value);
            auto escapedDomain = escapeForJSON(cookie.domain);
            auto escapedPath = escapeForJSON(cookie.path);
            auto escapedComment = escapeForJSON(cookie.comment);
            auto escapedCommentURL = escapeForJSON(cookie.commentURL.string());

            LOCAL_LOG(R"(  { "name": "%{public}s",)", escapedName.utf8().data());
            LOCAL_LOG(R"(    "value": "%{public}s",)", escapedValue.utf8().data());
            LOCAL_LOG(R"(    "domain": "%{public}s",)", escapedDomain.utf8().data());
            LOCAL_LOG(R"(    "path": "%{public}s",)", escapedPath.utf8().data());
            LOCAL_LOG(R"(    "created": %f,)", cookie.created);
            LOCAL_LOG(R"(    "expires": %f,)", cookie.expires);
            LOCAL_LOG(R"(    "httpOnly": %{public}s,)", cookie.httpOnly ? "true" : "false");
            LOCAL_LOG(R"(    "secure": %{public}s,)", cookie.secure ? "true" : "false");
            LOCAL_LOG(R"(    "session": %{public}s,)", cookie.session ? "true" : "false");
            LOCAL_LOG(R"(    "comment": "%{public}s",)", escapedComment.utf8().data());
            LOCAL_LOG(R"(    "commentURL": "%{public}s")", escapedCommentURL.utf8().data());
            LOCAL_LOG(R"(  }%{public}s)", trailingComma);
        }
        LOCAL_LOG(R"(]})");
#undef LOCAL_LOG
    }
}
#endif

} // namespace WebKit
