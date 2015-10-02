/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#if ENABLE(NETWORK_PROCESS)

#include "AuthenticationManager.h"
#include "DataReference.h"
#include "Logging.h"
#include "NetworkBlobRegistry.h"
#include "NetworkCache.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "NetworkResourceLoadParameters.h"
#include "RemoteNetworkingContext.h"
#include "ShareableResource.h"
#include "SharedMemory.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebResourceLoaderMessages.h"
#include <WebCore/BlobDataFileReference.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/SynchronousLoaderClient.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

struct NetworkResourceLoader::SynchronousLoadData {
    SynchronousLoadData(PassRefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply> reply)
        : delayedReply(reply)
    {
        ASSERT(delayedReply);
    }
    WebCore::ResourceRequest currentRequest;
    RefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply> delayedReply;
    WebCore::ResourceResponse response;
    WebCore::ResourceError error;
};

static void sendReplyToSynchronousRequest(NetworkResourceLoader::SynchronousLoadData& data, const WebCore::SharedBuffer* buffer)
{
    ASSERT(data.delayedReply);
    ASSERT(!data.response.isNull() || !data.error.isNull());

    Vector<char> responseBuffer;
    if (buffer && buffer->size())
        responseBuffer.append(buffer->data(), buffer->size());

    data.delayedReply->send(data.error, data.response, responseBuffer);
    data.delayedReply = nullptr;
}

NetworkResourceLoader::NetworkResourceLoader(const NetworkResourceLoadParameters& parameters, NetworkConnectionToWebProcess* connection, PassRefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply> synchronousReply)
    : m_parameters(parameters)
    , m_connection(connection)
    , m_bytesReceived(0)
    , m_bufferedDataEncodedDataLength(0)
    , m_didConvertHandleToDownload(false)
    , m_didConsumeSandboxExtensions(false)
    , m_defersLoading(parameters.defersLoading)
    , m_bufferingTimer(*this, &NetworkResourceLoader::bufferingTimerFired)
{
    ASSERT(RunLoop::isMain());
    // FIXME: This is necessary because of the existence of EmptyFrameLoaderClient in WebCore.
    //        Once bug 116233 is resolved, this ASSERT can just be "m_webPageID && m_webFrameID"
    ASSERT((m_parameters.webPageID && m_parameters.webFrameID) || m_parameters.clientCredentialPolicy == DoNotAskClientForAnyCredentials);

    if (originalRequest().httpBody()) {
        for (const FormDataElement& element : originalRequest().httpBody()->elements()) {
            if (element.m_type == FormDataElement::Type::EncodedBlob)
                m_fileReferences.appendVector(NetworkBlobRegistry::singleton().filesInBlob(connection, element.m_url));
        }
    }

    if (originalRequest().url().protocolIs("blob")) {
        ASSERT(!m_parameters.resourceSandboxExtension);
        m_fileReferences.appendVector(NetworkBlobRegistry::singleton().filesInBlob(connection, originalRequest().url()));
    }

    if (synchronousReply)
        m_synchronousLoadData = std::make_unique<SynchronousLoadData>(synchronousReply);
}

NetworkResourceLoader::~NetworkResourceLoader()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_handle);
    ASSERT(!isSynchronous() || !m_synchronousLoadData->delayedReply);
}

bool NetworkResourceLoader::isSynchronous() const
{
    return !!m_synchronousLoadData;
}

void NetworkResourceLoader::start()
{
    ASSERT(RunLoop::isMain());

    if (m_defersLoading)
        return;

    m_currentRequest = originalRequest();

#if ENABLE(NETWORK_CACHE)
    if (!NetworkCache::singleton().isEnabled() || sessionID().isEphemeral() || !originalRequest().url().protocolIsInHTTPFamily()) {
        startNetworkLoad();
        return;
    }

    RefPtr<NetworkResourceLoader> loader(this);
    NetworkCache::singleton().retrieve(originalRequest(), m_parameters.webPageID, [loader](std::unique_ptr<NetworkCache::Entry> entry) {
        if (loader->hasOneRef()) {
            // The loader has been aborted and is only held alive by this lambda.
            return;
        }
        if (!entry) {
            loader->startNetworkLoad();
            return;
        }
        if (loader->m_parameters.needsCertificateInfo && !entry->response().containsCertificateInfo()) {
            loader->startNetworkLoad();
            return;
        }
        if (entry->needsValidation()) {
            loader->validateCacheEntry(WTF::move(entry));
            return;
        }
        loader->didRetrieveCacheEntry(WTF::move(entry));
    });
#else
    startNetworkLoad();
#endif
}

void NetworkResourceLoader::startNetworkLoad()
{
    m_networkingContext = RemoteNetworkingContext::create(sessionID(), m_parameters.shouldClearReferrerOnHTTPSToHTTPRedirect);

    consumeSandboxExtensions();

    if (isSynchronous() || m_parameters.maximumBufferingTime > 0_ms)
        m_bufferedData = WebCore::SharedBuffer::create();

#if ENABLE(NETWORK_CACHE)
    if (NetworkCache::singleton().isEnabled())
        m_bufferedDataForCache = WebCore::SharedBuffer::create();
#endif

    bool shouldSniff = m_parameters.contentSniffingPolicy == SniffContent;
    m_handle = ResourceHandle::create(m_networkingContext.get(), m_currentRequest, this, m_defersLoading, shouldSniff);
}

void NetworkResourceLoader::setDefersLoading(bool defers)
{
    if (m_defersLoading == defers)
        return;
    m_defersLoading = defers;
    if (m_handle) {
        m_handle->setDefersLoading(defers);
        return;
    }
    if (!m_defersLoading)
        start();
}

void NetworkResourceLoader::cleanup()
{
    ASSERT(RunLoop::isMain());

    m_bufferingTimer.stop();

    invalidateSandboxExtensions();

    if (m_handle) {
        m_handle->setClient(nullptr);
        m_handle = nullptr;
    }

    // This will cause NetworkResourceLoader to be destroyed and therefore we do it last.
    m_connection->didCleanupResourceLoader(*this);
}

void NetworkResourceLoader::didConvertHandleToDownload()
{
    ASSERT(m_handle);
    m_didConvertHandleToDownload = true;
}

void NetworkResourceLoader::abort()
{
    ASSERT(RunLoop::isMain());

    if (m_handle && !m_didConvertHandleToDownload)
        m_handle->cancel();

    cleanup();
}

void NetworkResourceLoader::didReceiveResponseAsync(ResourceHandle* handle, const ResourceResponse& receivedResponse)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    m_response = receivedResponse;

    m_response.setSource(ResourceResponse::Source::Network);
    if (m_parameters.needsCertificateInfo)
        m_response.includeCertificateInfo();
    // For multipart/x-mixed-replace didReceiveResponseAsync gets called multiple times and buffering would require special handling.
    if (!isSynchronous() && m_response.isMultipart())
        m_bufferedData = nullptr;

    bool shouldSendDidReceiveResponse = true;
#if ENABLE(NETWORK_CACHE)
    if (m_response.isMultipart())
        m_bufferedDataForCache = nullptr;

    if (m_cacheEntryForValidation) {
        bool validationSucceeded = m_response.httpStatusCode() == 304; // 304 Not Modified
        if (validationSucceeded)
            NetworkCache::singleton().update(originalRequest(), m_parameters.webPageID, *m_cacheEntryForValidation, m_response);
        else
            m_cacheEntryForValidation = nullptr;
    }
    shouldSendDidReceiveResponse = !m_cacheEntryForValidation;
#endif

    bool shouldWaitContinueDidReceiveResponse = originalRequest().requester() == ResourceRequest::Requester::Main;
    if (shouldSendDidReceiveResponse) {
        if (isSynchronous())
            m_synchronousLoadData->response = m_response;
        else {
            if (!sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveResponse(m_response, shouldWaitContinueDidReceiveResponse)))
                return;
        }
    }

    // For main resources, the web process is responsible for sending back a NetworkResourceLoader::ContinueDidReceiveResponse message.
    bool shouldContinueDidReceiveResponse = !shouldWaitContinueDidReceiveResponse;
#if ENABLE(NETWORK_CACHE)
    shouldContinueDidReceiveResponse = shouldContinueDidReceiveResponse || m_cacheEntryForValidation;
#endif
    if (!shouldContinueDidReceiveResponse)
        return;

    m_handle->continueDidReceiveResponse();
}

void NetworkResourceLoader::didReceiveData(ResourceHandle*, const char* /* data */, unsigned /* length */, int /* encodedDataLength */)
{
    // The NetworkProcess should never get a didReceiveData callback.
    // We should always be using didReceiveBuffer.
    ASSERT_NOT_REACHED();
}

void NetworkResourceLoader::didReceiveBuffer(ResourceHandle* handle, PassRefPtr<SharedBuffer> buffer, int reportedEncodedDataLength)
{
    ASSERT_UNUSED(handle, handle == m_handle);
#if ENABLE(NETWORK_CACHE)
    ASSERT(!m_cacheEntryForValidation);

    if (m_bufferedDataForCache) {
        // Prevent memory growth in case of streaming data.
        const size_t maximumCacheBufferSize = 10 * 1024 * 1024;
        if (m_bufferedDataForCache->size() + buffer->size() <= maximumCacheBufferSize)
            m_bufferedDataForCache->append(buffer.get());
        else
            m_bufferedDataForCache = nullptr;
    }
#endif
    // FIXME: At least on OS X Yosemite we always get -1 from the resource handle.
    unsigned encodedDataLength = reportedEncodedDataLength >= 0 ? reportedEncodedDataLength : buffer->size();

    m_bytesReceived += buffer->size();
    if (m_bufferedData) {
        m_bufferedData->append(buffer.get());
        m_bufferedDataEncodedDataLength += encodedDataLength;
        startBufferingTimerIfNeeded();
        return;
    }
    sendBufferMaybeAborting(*buffer, encodedDataLength);
}

void NetworkResourceLoader::didFinishLoading(ResourceHandle* handle, double finishTime)
{
    ASSERT_UNUSED(handle, handle == m_handle);

#if ENABLE(NETWORK_CACHE)
    if (NetworkCache::singleton().isEnabled()) {
        if (m_cacheEntryForValidation) {
            // 304 Not Modified
            ASSERT(m_response.httpStatusCode() == 304);
            LOG(NetworkCache, "(NetworkProcess) revalidated");
            didRetrieveCacheEntry(WTF::move(m_cacheEntryForValidation));
            return;
        }
        bool allowStale = originalRequest().cachePolicy() >= ReturnCacheDataElseLoad;
        bool hasCacheableRedirect = m_response.isHTTP() && WebCore::redirectChainAllowsReuse(m_redirectChainCacheStatus, allowStale ? WebCore::ReuseExpiredRedirection : WebCore::DoNotReuseExpiredRedirection);
        if (hasCacheableRedirect && m_redirectChainCacheStatus.status == RedirectChainCacheStatus::CachedRedirection) {
            // Maybe we should cache the actual redirects instead of the end result?
            auto now = std::chrono::system_clock::now();
            auto responseEndOfValidity = now + WebCore::computeFreshnessLifetimeForHTTPFamily(m_response, now) - WebCore::computeCurrentAge(m_response, now);
            hasCacheableRedirect = responseEndOfValidity <= m_redirectChainCacheStatus.endOfValidity;
        }

        bool isPrivate = sessionID().isEphemeral();
        if (m_bufferedDataForCache && hasCacheableRedirect && !isPrivate) {
            // Keep the connection alive.
            RefPtr<NetworkConnectionToWebProcess> connection(connectionToWebProcess());
            RefPtr<NetworkResourceLoader> loader(this);
            NetworkCache::singleton().store(originalRequest(), m_response, WTF::move(m_bufferedDataForCache), [loader, connection](NetworkCache::MappedBody& mappedBody) {
#if ENABLE(SHAREABLE_RESOURCE)
                if (mappedBody.shareableResourceHandle.isNull())
                    return;
                LOG(NetworkCache, "(NetworkProcess) sending DidCacheResource");
                loader->send(Messages::NetworkProcessConnection::DidCacheResource(loader->originalRequest(), mappedBody.shareableResourceHandle, loader->sessionID()));
#endif
            });
        } else if (!hasCacheableRedirect) {
            // Make sure we don't keep a stale entry in the cache.
            NetworkCache::singleton().remove(originalRequest());
        }
    }
#endif

    if (isSynchronous())
        sendReplyToSynchronousRequest(*m_synchronousLoadData, m_bufferedData.get());
    else {
        if (m_bufferedData && m_bufferedData->size()) {
            // FIXME: Pass a real value or remove the encoded data size feature.
            bool shouldContinue = sendBufferMaybeAborting(*m_bufferedData, -1);
            if (!shouldContinue)
                return;
        }
        send(Messages::WebResourceLoader::DidFinishResourceLoad(finishTime));
    }

    cleanup();
}

void NetworkResourceLoader::didFail(ResourceHandle* handle, const ResourceError& error)
{
    ASSERT_UNUSED(handle, !handle || handle == m_handle);

#if ENABLE(NETWORK_CACHE)
    m_cacheEntryForValidation = nullptr;
#endif

    if (isSynchronous()) {
        m_synchronousLoadData->error = error;
        sendReplyToSynchronousRequest(*m_synchronousLoadData, nullptr);
    } else
        send(Messages::WebResourceLoader::DidFailResourceLoad(error));

    cleanup();
}

void NetworkResourceLoader::willSendRequestAsync(ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    // We only expect to get the willSendRequest callback from ResourceHandle as the result of a redirect.
    ASSERT(!redirectResponse.isNull());
    ASSERT(RunLoop::isMain());

    m_currentRequest = request;

#if ENABLE(NETWORK_CACHE)
    WebCore::updateRedirectChainStatus(m_redirectChainCacheStatus, redirectResponse);
#endif

    if (isSynchronous()) {
        // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
        // This includes at least updating host records, and comparing the current request instead of the original request here.
        if (!protocolHostAndPortAreEqual(originalRequest().url(), m_currentRequest.url())) {
            ASSERT(m_synchronousLoadData->error.isNull());
            m_synchronousLoadData->error = SynchronousLoaderClient::platformBadResponseError();
            m_currentRequest = ResourceRequest();
        }
        continueWillSendRequest(m_currentRequest);
        return;
    }
    sendAbortingOnFailure(Messages::WebResourceLoader::WillSendRequest(m_currentRequest, redirectResponse));
}

void NetworkResourceLoader::continueWillSendRequest(const ResourceRequest& newRequest)
{
#if PLATFORM(COCOA)
    m_currentRequest.updateFromDelegatePreservingOldProperties(newRequest.nsURLRequest(DoNotUpdateHTTPBody));
#elif USE(SOUP)
    // FIXME: Implement ResourceRequest::updateFromDelegatePreservingOldProperties. See https://bugs.webkit.org/show_bug.cgi?id=126127.
    m_currentRequest.updateFromDelegatePreservingOldProperties(newRequest);
#endif

    if (m_currentRequest.isNull()) {
        m_handle->cancel();
        didFail(m_handle.get(), cancelledError(m_currentRequest));
        return;
    }

    m_handle->continueWillSendRequest(m_currentRequest);
}

void NetworkResourceLoader::continueDidReceiveResponse()
{
    // FIXME: Remove this check once BlobResourceHandle implements didReceiveResponseAsync correctly.
    // Currently, it does not wait for response, so the load is likely to finish before continueDidReceiveResponse.
    if (!m_handle)
        return;

    m_handle->continueDidReceiveResponse();
}

void NetworkResourceLoader::didSendData(ResourceHandle* handle, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    if (!isSynchronous())
        send(Messages::WebResourceLoader::DidSendData(bytesSent, totalBytesToBeSent));
}

void NetworkResourceLoader::wasBlocked(ResourceHandle* handle)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    didFail(handle, WebKit::blockedError(m_currentRequest));
}

void NetworkResourceLoader::cannotShowURL(ResourceHandle* handle)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    didFail(handle, WebKit::cannotShowURLError(m_currentRequest));
}

bool NetworkResourceLoader::shouldUseCredentialStorage(ResourceHandle* handle)
{
    ASSERT_UNUSED(handle, handle == m_handle || !m_handle); // m_handle will be 0 if called from ResourceHandle::start().

    // When the WebProcess is handling loading a client is consulted each time this shouldUseCredentialStorage question is asked.
    // In NetworkProcess mode we ask the WebProcess client up front once and then reuse the cached answer.

    // We still need this sync version, because ResourceHandle itself uses it internally, even when the delegate uses an async one.

    return m_parameters.allowStoredCredentials == AllowStoredCredentials;
}

void NetworkResourceLoader::didReceiveAuthenticationChallenge(ResourceHandle* handle, const AuthenticationChallenge& challenge)
{
    ASSERT_UNUSED(handle, handle == m_handle);
    // NetworkResourceLoader does not know whether the request is cross origin, so Web process computes an applicable credential policy for it.
    ASSERT(m_parameters.clientCredentialPolicy != DoNotAskClientForCrossOriginCredentials);

    if (m_parameters.clientCredentialPolicy == DoNotAskClientForAnyCredentials) {
        challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    NetworkProcess::singleton().authenticationManager().didReceiveAuthenticationChallenge(m_parameters.webPageID, m_parameters.webFrameID, challenge);
}

void NetworkResourceLoader::didCancelAuthenticationChallenge(ResourceHandle* handle, const AuthenticationChallenge&)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    // This function is probably not needed (see <rdar://problem/8960124>).
    notImplemented();
}

void NetworkResourceLoader::receivedCancellation(ResourceHandle* handle, const AuthenticationChallenge&)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    m_handle->cancel();
    didFail(m_handle.get(), cancelledError(m_currentRequest));
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
    ASSERT(m_handle);
    if (!m_bufferedData->size())
        return;

    IPC::SharedBufferDataReference dataReference(m_bufferedData.get());
    size_t encodedLength = m_bufferedDataEncodedDataLength;

    m_bufferedData = WebCore::SharedBuffer::create();
    m_bufferedDataEncodedDataLength = 0;

    sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveData(dataReference, encodedLength));
}

bool NetworkResourceLoader::sendBufferMaybeAborting(const WebCore::SharedBuffer& buffer, size_t encodedDataLength)
{
    ASSERT(!isSynchronous());

#if PLATFORM(COCOA)
    ShareableResource::Handle shareableResourceHandle;
    NetworkResourceLoader::tryGetShareableHandleFromSharedBuffer(shareableResourceHandle, const_cast<WebCore::SharedBuffer&>(buffer));
    if (!shareableResourceHandle.isNull()) {
        send(Messages::WebResourceLoader::DidReceiveResource(shareableResourceHandle, currentTime()));
        abort();
        return false;
    }
#endif

    IPC::SharedBufferDataReference dataReference(&const_cast<WebCore::SharedBuffer&>(buffer));
    return sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveData(dataReference, encodedDataLength));
}

#if ENABLE(NETWORK_CACHE)
void NetworkResourceLoader::didRetrieveCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    if (isSynchronous()) {
        m_synchronousLoadData->response = entry->response();
        sendReplyToSynchronousRequest(*m_synchronousLoadData, entry->buffer());
    } else {
        if (entry->response().url() != originalRequest().url()) {
            // This is a cached redirect. Synthesize a minimal redirect so we get things like referer header right.
            // FIXME: We should cache the actual redirects.
            ResourceRequest syntheticRedirectRequest(entry->response().url());
            ResourceResponse syntheticRedirectResponse(originalRequest().url(), { }, 0, { });
            sendAbortingOnFailure(Messages::WebResourceLoader::WillSendRequest(syntheticRedirectRequest, syntheticRedirectResponse));
        }

        bool needsContinueDidReceiveResponseMessage = originalRequest().requester() == ResourceRequest::Requester::Main;
        sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveResponse(entry->response(), needsContinueDidReceiveResponseMessage));

#if ENABLE(SHAREABLE_RESOURCE)
        if (!entry->shareableResourceHandle().isNull())
            send(Messages::WebResourceLoader::DidReceiveResource(entry->shareableResourceHandle(), currentTime()));
        else {
#endif
            bool shouldContinue = sendBufferMaybeAborting(*entry->buffer(), entry->buffer()->size());
            if (!shouldContinue)
                return;
            send(Messages::WebResourceLoader::DidFinishResourceLoad(currentTime()));
#if ENABLE(SHAREABLE_RESOURCE)
        }
#endif
    }

    cleanup();
}

void NetworkResourceLoader::validateCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    ASSERT(!m_handle);

    String eTag = entry->response().httpHeaderField(WebCore::HTTPHeaderName::ETag);
    String lastModified = entry->response().httpHeaderField(WebCore::HTTPHeaderName::LastModified);
    if (!eTag.isEmpty())
        m_currentRequest.setHTTPHeaderField(WebCore::HTTPHeaderName::IfNoneMatch, eTag);
    if (!lastModified.isEmpty())
        m_currentRequest.setHTTPHeaderField(WebCore::HTTPHeaderName::IfModifiedSince, lastModified);

    m_cacheEntryForValidation = WTF::move(entry);

    startNetworkLoad();
}
#endif

IPC::Connection* NetworkResourceLoader::messageSenderConnection()
{
    return connectionToWebProcess()->connection();
}

void NetworkResourceLoader::consumeSandboxExtensions()
{
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
    }

    m_fileReferences.clear();

    m_didConsumeSandboxExtensions = false;
}

template<typename T>
bool NetworkResourceLoader::sendAbortingOnFailure(T&& message, unsigned messageSendFlags)
{
    bool result = messageSenderConnection()->send(std::forward<T>(message), messageSenderDestinationID(), messageSendFlags);
    if (!result)
        abort();
    return result;
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void NetworkResourceLoader::canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle* handle, const ProtectionSpace& protectionSpace)
{
    ASSERT(RunLoop::isMain());
    ASSERT_UNUSED(handle, handle == m_handle);

    // Handle server trust evaluation at platform-level if requested, for performance reasons.
    if (protectionSpace.authenticationScheme() == ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested
        && !NetworkProcess::singleton().canHandleHTTPSServerTrustEvaluation()) {
        continueCanAuthenticateAgainstProtectionSpace(false);
        return;
    }

    if (isSynchronous()) {
        // FIXME: We should ask the WebProcess like the asynchronous case below does.
        // This is currently impossible as the WebProcess is blocked waiting on this synchronous load.
        // It's possible that we can jump straight to the UI process to resolve this.
        continueCanAuthenticateAgainstProtectionSpace(true);
        return;
    }
    sendAbortingOnFailure(Messages::WebResourceLoader::CanAuthenticateAgainstProtectionSpace(protectionSpace));
}

void NetworkResourceLoader::continueCanAuthenticateAgainstProtectionSpace(bool result)
{
    m_handle->continueCanAuthenticateAgainstProtectionSpace(result);
}
#endif

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
bool NetworkResourceLoader::supportsDataArray()
{
    notImplemented();
    return false;
}

void NetworkResourceLoader::didReceiveDataArray(ResourceHandle*, CFArrayRef)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}
#endif

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
