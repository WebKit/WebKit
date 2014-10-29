/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "AsynchronousNetworkLoaderClient.h"
#include "AuthenticationManager.h"
#include "DataReference.h"
#include "Logging.h"
#include "NetworkBlobRegistry.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "NetworkResourceLoadParameters.h"
#include "RemoteNetworkingContext.h"
#include "ShareableResource.h"
#include "SharedMemory.h"
#include "SynchronousNetworkLoaderClient.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebResourceLoaderMessages.h"
#include <WebCore/BlobDataFileReference.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceBuffer.h>
#include <WebCore/ResourceHandle.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

NetworkResourceLoader::NetworkResourceLoader(const NetworkResourceLoadParameters& parameters, NetworkConnectionToWebProcess* connection, PassRefPtr<Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply> reply)
    : m_bytesReceived(0)
    , m_handleConvertedToDownload(false)
    , m_identifier(parameters.identifier)
    , m_webPageID(parameters.webPageID)
    , m_webFrameID(parameters.webFrameID)
    , m_sessionID(parameters.sessionID)
    , m_request(parameters.request)
    , m_priority(parameters.priority)
    , m_contentSniffingPolicy(parameters.contentSniffingPolicy)
    , m_allowStoredCredentials(parameters.allowStoredCredentials)
    , m_clientCredentialPolicy(parameters.clientCredentialPolicy)
    , m_shouldClearReferrerOnHTTPSToHTTPRedirect(parameters.shouldClearReferrerOnHTTPSToHTTPRedirect)
    , m_isLoadingMainResource(parameters.isMainResource)
    , m_defersLoading(parameters.defersLoading)
    , m_sandboxExtensionsAreConsumed(false)
    , m_connection(connection)
{
    // Either this loader has both a webPageID and webFrameID, or it is not allowed to ask the client for authentication credentials.
    // FIXME: This is necessary because of the existence of EmptyFrameLoaderClient in WebCore.
    //        Once bug 116233 is resolved, this ASSERT can just be "m_webPageID && m_webFrameID"
    ASSERT((m_webPageID && m_webFrameID) || m_clientCredentialPolicy == DoNotAskClientForAnyCredentials);

    for (size_t i = 0, count = parameters.requestBodySandboxExtensions.size(); i < count; ++i) {
        if (RefPtr<SandboxExtension> extension = SandboxExtension::create(parameters.requestBodySandboxExtensions[i]))
            m_requestBodySandboxExtensions.append(extension);
    }

    if (m_request.httpBody()) {
        for (const FormDataElement& element : m_request.httpBody()->elements()) {
            if (element.m_type == FormDataElement::Type::EncodedBlob)
                m_fileReferences.appendVector(NetworkBlobRegistry::shared().filesInBlob(connection, element.m_url));
        }
    }

    if (m_request.url().protocolIs("blob")) {
        ASSERT(!SandboxExtension::create(parameters.resourceSandboxExtension));
        m_fileReferences.appendVector(NetworkBlobRegistry::shared().filesInBlob(connection, m_request.url()));
    } else

    if (RefPtr<SandboxExtension> resourceSandboxExtension = SandboxExtension::create(parameters.resourceSandboxExtension))
        m_resourceSandboxExtensions.append(resourceSandboxExtension);

    ASSERT(RunLoop::isMain());
    
    if (reply || parameters.shouldBufferResource)
        m_bufferedData = WebCore::SharedBuffer::create();

    if (reply)
        m_networkLoaderClient = std::make_unique<SynchronousNetworkLoaderClient>(m_request, reply);
    else
        m_networkLoaderClient = std::make_unique<AsynchronousNetworkLoaderClient>();
}

NetworkResourceLoader::~NetworkResourceLoader()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_handle);
    ASSERT(!m_hostRecord);
}

bool NetworkResourceLoader::isSynchronous() const
{
    return m_networkLoaderClient->isSynchronous();
}

void NetworkResourceLoader::start()
{
    ASSERT(RunLoop::isMain());

    if (m_defersLoading) {
        m_deferredRequest = m_request;
        return;
    }

    // Explicit ref() balanced by a deref() in NetworkResourceLoader::cleanup()
    ref();

    // FIXME (NetworkProcess): Set platform specific settings.
    m_networkingContext = RemoteNetworkingContext::create(m_sessionID, m_shouldClearReferrerOnHTTPSToHTTPRedirect);

    consumeSandboxExtensions();

    // FIXME (NetworkProcess): Pass an actual value for defersLoading
    m_handle = ResourceHandle::create(m_networkingContext.get(), m_request, this, false /* defersLoading */, m_contentSniffingPolicy == SniffContent);
}

void NetworkResourceLoader::setDefersLoading(bool defers)
{
    m_defersLoading = defers;
    if (m_handle)
        m_handle->setDefersLoading(defers);
    if (!defers && !m_deferredRequest.isNull()) {
        m_request = m_deferredRequest;
        m_deferredRequest = ResourceRequest();
        start();
    }
}

void NetworkResourceLoader::cleanup()
{
    ASSERT(RunLoop::isMain());

    invalidateSandboxExtensions();

    // Tell the scheduler about this finished loader soon so it can start more network requests.
    NetworkProcess::shared().networkResourceLoadScheduler().scheduleRemoveLoader(this);

    if (m_handle) {
        // Explicit deref() balanced by a ref() in NetworkResourceLoader::start()
        // This might cause the NetworkResourceLoader to be destroyed and therefore we do it last.
        m_handle = 0;
        deref();
    }
}

void NetworkResourceLoader::didConvertHandleToDownload()
{
    ASSERT(m_handle);
    m_handleConvertedToDownload = true;
}

void NetworkResourceLoader::abort()
{
    ASSERT(RunLoop::isMain());

    if (m_handle && !m_handleConvertedToDownload)
        m_handle->cancel();

    cleanup();
}

void NetworkResourceLoader::didReceiveResponseAsync(ResourceHandle* handle, const ResourceResponse& response)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    m_networkLoaderClient->didReceiveResponse(this, response);

    // m_handle will be 0 if the request got aborted above.
    if (!m_handle)
        return;

    if (!m_isLoadingMainResource) {
        // For main resources, the web process is responsible for sending back a NetworkResourceLoader::ContinueDidReceiveResponse message.
        m_handle->continueDidReceiveResponse();
    }
}

void NetworkResourceLoader::didReceiveData(ResourceHandle*, const char* /* data */, unsigned /* length */, int /* encodedDataLength */)
{
    // The NetworkProcess should never get a didReceiveData callback.
    // We should always be using didReceiveBuffer.
    ASSERT_NOT_REACHED();
}

void NetworkResourceLoader::didReceiveBuffer(ResourceHandle* handle, PassRefPtr<SharedBuffer> buffer, int encodedDataLength)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    // FIXME (NetworkProcess): For the memory cache we'll also need to cache the response data here.
    // Such buffering will need to be thread safe, as this callback is happening on a background thread.
    
    m_bytesReceived += buffer->size();
    if (m_bufferedData)
        m_bufferedData->append(buffer.get());
    else
        m_networkLoaderClient->didReceiveBuffer(this, buffer.get(), encodedDataLength);
}

void NetworkResourceLoader::didFinishLoading(ResourceHandle* handle, double finishTime)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    // Send the full resource data if we were buffering it.
    if (m_bufferedData && m_bufferedData->size())
        m_networkLoaderClient->didReceiveBuffer(this, m_bufferedData.get(), m_bufferedData->size());

    m_networkLoaderClient->didFinishLoading(this, finishTime);

    cleanup();
}

void NetworkResourceLoader::didFail(ResourceHandle* handle, const ResourceError& error)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    m_networkLoaderClient->didFail(this, error);

    cleanup();
}

void NetworkResourceLoader::willSendRequestAsync(ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    // We only expect to get the willSendRequest callback from ResourceHandle as the result of a redirect.
    ASSERT(!redirectResponse.isNull());
    ASSERT(RunLoop::isMain());

    ResourceRequest proposedRequest = request;
    m_suggestedRequestForWillSendRequest = request;

    m_networkLoaderClient->willSendRequest(this, proposedRequest, redirectResponse);
}

void NetworkResourceLoader::continueWillSendRequest(const ResourceRequest& newRequest)
{
#if PLATFORM(COCOA)
    m_suggestedRequestForWillSendRequest.updateFromDelegatePreservingOldProperties(newRequest.nsURLRequest(DoNotUpdateHTTPBody));
#elif USE(SOUP)
    // FIXME: Implement ResourceRequest::updateFromDelegatePreservingOldProperties. See https://bugs.webkit.org/show_bug.cgi?id=126127.
    m_suggestedRequestForWillSendRequest.updateFromDelegatePreservingOldProperties(newRequest);
#endif

    RunLoop::main().dispatch(bind(&NetworkResourceLoadScheduler::receivedRedirect, &NetworkProcess::shared().networkResourceLoadScheduler(), this, m_suggestedRequestForWillSendRequest.url()));

    m_request = m_suggestedRequestForWillSendRequest;
    m_suggestedRequestForWillSendRequest = ResourceRequest();

    m_handle->continueWillSendRequest(m_request);

    if (m_request.isNull()) {
        m_handle->cancel();
        didFail(m_handle.get(), cancelledError(m_request));
    }
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

    m_networkLoaderClient->didSendData(this, bytesSent, totalBytesToBeSent);
}

void NetworkResourceLoader::wasBlocked(ResourceHandle* handle)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    didFail(handle, WebKit::blockedError(request()));
}

void NetworkResourceLoader::cannotShowURL(ResourceHandle* handle)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    didFail(handle, WebKit::cannotShowURLError(request()));
}

bool NetworkResourceLoader::shouldUseCredentialStorage(ResourceHandle* handle)
{
    ASSERT_UNUSED(handle, handle == m_handle || !m_handle); // m_handle will be 0 if called from ResourceHandle::start().

    // When the WebProcess is handling loading a client is consulted each time this shouldUseCredentialStorage question is asked.
    // In NetworkProcess mode we ask the WebProcess client up front once and then reuse the cached answer.

    // We still need this sync version, because ResourceHandle itself uses it internally, even when the delegate uses an async one.

    return m_allowStoredCredentials == AllowStoredCredentials;
}

void NetworkResourceLoader::didReceiveAuthenticationChallenge(ResourceHandle* handle, const AuthenticationChallenge& challenge)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    // FIXME (http://webkit.org/b/115291): Since we go straight to the UI process for authentication we don't get WebCore's
    // cross-origin check before asking the client for credentials.
    // Therefore we are too permissive in the case where the ClientCredentialPolicy is DoNotAskClientForCrossOriginCredentials.
    if (m_clientCredentialPolicy == DoNotAskClientForAnyCredentials) {
        challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    NetworkProcess::shared().authenticationManager().didReceiveAuthenticationChallenge(m_webPageID, m_webFrameID, challenge);
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
    didFail(m_handle.get(), cancelledError(m_request));
}

IPC::Connection* NetworkResourceLoader::messageSenderConnection()
{
    return connectionToWebProcess()->connection();
}

void NetworkResourceLoader::consumeSandboxExtensions()
{
    for (RefPtr<SandboxExtension>& extension : m_requestBodySandboxExtensions)
        extension->consume();

    for (RefPtr<SandboxExtension>& extension : m_resourceSandboxExtensions)
        extension->consume();

    for (RefPtr<BlobDataFileReference>& fileReference : m_fileReferences)
        fileReference->prepareForFileAccess();

    m_sandboxExtensionsAreConsumed = true;
}

void NetworkResourceLoader::invalidateSandboxExtensions()
{
    if (m_sandboxExtensionsAreConsumed) {
        for (RefPtr<SandboxExtension>& extension : m_requestBodySandboxExtensions)
            extension->revoke();
        for (RefPtr<SandboxExtension>& extension : m_resourceSandboxExtensions)
            extension->revoke();
        for (RefPtr<BlobDataFileReference>& fileReference : m_fileReferences)
            fileReference->revokeFileAccess();
    }

    m_requestBodySandboxExtensions.clear();
    m_resourceSandboxExtensions.clear();
    m_fileReferences.clear();

    m_sandboxExtensionsAreConsumed = false;
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void NetworkResourceLoader::canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle* handle, const ProtectionSpace& protectionSpace)
{
    ASSERT(RunLoop::isMain());
    ASSERT_UNUSED(handle, handle == m_handle);

    m_networkLoaderClient->canAuthenticateAgainstProtectionSpace(this, protectionSpace);

    // Handle server trust evaluation at platform-level if requested, for performance reasons.
    if (protectionSpace.authenticationScheme() == ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested
        && !NetworkProcess::shared().canHandleHTTPSServerTrustEvaluation()) {
        continueCanAuthenticateAgainstProtectionSpace(false);
        return;
    }
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
