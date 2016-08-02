/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "NetworkLoad.h"

#include "AuthenticationManager.h"
#include "NetworkProcess.h"
#include "SessionTracker.h"
#include "WebErrors.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/SessionID.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/MainThread.h>

namespace WebKit {

using namespace WebCore;

#if USE(NETWORK_SESSION)

NetworkLoad::NetworkLoad(NetworkLoadClient& client, NetworkLoadParameters&& parameters, NetworkSession& networkSession)
    : m_client(client)
    , m_parameters(WTFMove(parameters))
    , m_currentRequest(m_parameters.request)
{
    if (m_parameters.request.url().protocolIsBlob()) {
        m_handle = ResourceHandle::create(nullptr, m_parameters.request, this, m_parameters.defersLoading, m_parameters.contentSniffingPolicy == SniffContent);
        return;
    }
    m_task = NetworkDataTask::create(networkSession, *this, m_parameters.request, m_parameters.allowStoredCredentials, m_parameters.contentSniffingPolicy, m_parameters.shouldClearReferrerOnHTTPSToHTTPRedirect);
    if (!m_parameters.defersLoading)
        m_task->resume();
}

#else

NetworkLoad::NetworkLoad(NetworkLoadClient& client, NetworkLoadParameters&& parameters)
    : m_client(client)
    , m_parameters(WTFMove(parameters))
    , m_networkingContext(RemoteNetworkingContext::create(m_parameters.sessionID, m_parameters.shouldClearReferrerOnHTTPSToHTTPRedirect))
    , m_currentRequest(m_parameters.request)
{
    m_handle = ResourceHandle::create(m_networkingContext.get(), m_parameters.request, this, m_parameters.defersLoading, m_parameters.contentSniffingPolicy == SniffContent);
}

#endif

NetworkLoad::~NetworkLoad()
{
    ASSERT(RunLoop::isMain());
#if USE(NETWORK_SESSION)
    if (m_responseCompletionHandler)
        m_responseCompletionHandler(PolicyIgnore);
    if (m_challengeCompletionHandler)
        m_challengeCompletionHandler(AuthenticationChallengeDisposition::Cancel, { });
    if (m_task)
        m_task->clearClient();
#elif USE(PROTECTION_SPACE_AUTH_CALLBACK)
    if (m_handle && m_waitingForContinueCanAuthenticateAgainstProtectionSpace)
        m_handle->continueCanAuthenticateAgainstProtectionSpace(false);
#endif
    if (m_handle)
        m_handle->clearClient();
}

void NetworkLoad::setDefersLoading(bool defers)
{
#if USE(NETWORK_SESSION)
    if (m_task) {
        if (defers)
            m_task->suspend();
        else
            m_task->resume();
    }
#endif
    if (m_handle)
        m_handle->setDefersLoading(defers);
}

void NetworkLoad::cancel()
{
#if USE(NETWORK_SESSION)
    if (m_task)
        m_task->cancel();
#endif
    if (m_handle)
        m_handle->cancel();
}

void NetworkLoad::continueWillSendRequest(WebCore::ResourceRequest&& newRequest)
{
#if PLATFORM(COCOA)
    m_currentRequest.updateFromDelegatePreservingOldProperties(newRequest.nsURLRequest(DoNotUpdateHTTPBody));
#elif USE(SOUP)
    // FIXME: Implement ResourceRequest::updateFromDelegatePreservingOldProperties. See https://bugs.webkit.org/show_bug.cgi?id=126127.
    m_currentRequest.updateFromDelegatePreservingOldProperties(newRequest);
#endif

#if USE(NETWORK_SESSION)
    auto redirectCompletionHandler = std::exchange(m_redirectCompletionHandler, nullptr);    
    ASSERT(redirectCompletionHandler);
#endif
    
    if (m_currentRequest.isNull()) {
        if (m_handle)
            m_handle->cancel();
        didFail(m_handle.get(), cancelledError(m_currentRequest));
#if USE(NETWORK_SESSION)
        if (redirectCompletionHandler)
            redirectCompletionHandler({ });
#endif
        return;
    } else if (m_handle) {
        auto currentRequestCopy = m_currentRequest;
        m_handle->continueWillSendRequest(WTFMove(currentRequestCopy));
    }

#if USE(NETWORK_SESSION)
    if (redirectCompletionHandler)
        redirectCompletionHandler(m_currentRequest);
#endif
}

void NetworkLoad::continueDidReceiveResponse()
{
#if USE(NETWORK_SESSION)
    ASSERT(m_responseCompletionHandler);
    if (m_responseCompletionHandler) {
        m_responseCompletionHandler(PolicyUse);
        m_responseCompletionHandler = nullptr;
    }
#endif
    if (m_handle)
        m_handle->continueDidReceiveResponse();
}

NetworkLoadClient::ShouldContinueDidReceiveResponse NetworkLoad::sharedDidReceiveResponse(ResourceResponse&& response)
{
    response.setSource(ResourceResponse::Source::Network);
    if (m_parameters.needsCertificateInfo)
        response.includeCertificateInfo();

    return m_client.didReceiveResponse(WTFMove(response));
}

void NetworkLoad::sharedWillSendRedirectedRequest(ResourceRequest&& request, ResourceResponse&& redirectResponse)
{
    // We only expect to get the willSendRequest callback from ResourceHandle as the result of a redirect.
    ASSERT(!redirectResponse.isNull());
    ASSERT(RunLoop::isMain());

    auto oldRequest = WTFMove(m_currentRequest);
    m_currentRequest = request;
    m_client.willSendRedirectedRequest(WTFMove(oldRequest), WTFMove(request), WTFMove(redirectResponse));
}

#if USE(NETWORK_SESSION)

void NetworkLoad::convertTaskToDownload(DownloadID downloadID, const ResourceRequest& updatedRequest, const ResourceResponse& response)
{
    if (!m_task)
        return;

    m_task->setPendingDownloadID(downloadID);
    
    ASSERT(m_responseCompletionHandler);
    if (m_responseCompletionHandler)
        NetworkProcess::singleton().findPendingDownloadLocation(*m_task.get(), std::exchange(m_responseCompletionHandler, nullptr), updatedRequest, response);
}

void NetworkLoad::setPendingDownloadID(DownloadID downloadID)
{
    if (!m_task)
        return;

    m_task->setPendingDownloadID(downloadID);
}

void NetworkLoad::setPendingDownload(PendingDownload& pendingDownload)
{
    if (!m_task)
        return;

    m_task->setPendingDownload(pendingDownload);
}

void NetworkLoad::willPerformHTTPRedirection(ResourceResponse&& response, ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    ASSERT(!m_redirectCompletionHandler);
    m_redirectCompletionHandler = WTFMove(completionHandler);
    sharedWillSendRedirectedRequest(WTFMove(request), WTFMove(response));
}

void NetworkLoad::didReceiveChallenge(const AuthenticationChallenge& challenge, ChallengeCompletionHandler&& completionHandler)
{
    // Handle server trust evaluation at platform-level if requested, for performance reasons.
    if (challenge.protectionSpace().authenticationScheme() == ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested
        && !NetworkProcess::singleton().canHandleHTTPSServerTrustEvaluation()) {
        if (m_task && m_task->allowsSpecificHTTPSCertificateForHost(challenge))
            completionHandler(AuthenticationChallengeDisposition::UseCredential, serverTrustCredential(challenge));
        else
            completionHandler(AuthenticationChallengeDisposition::RejectProtectionSpace, { });
        return;
    }

    m_challengeCompletionHandler = WTFMove(completionHandler);
    m_challenge = challenge;

    m_client.canAuthenticateAgainstProtectionSpaceAsync(challenge.protectionSpace());
}

void NetworkLoad::didReceiveResponseNetworkSession(ResourceResponse&& response, ResponseCompletionHandler&& completionHandler)
{
    ASSERT(isMainThread());
    if (m_task && m_task->pendingDownloadID().downloadID())
        NetworkProcess::singleton().findPendingDownloadLocation(*m_task.get(), WTFMove(completionHandler), m_task->currentRequest(), response);
    else if (sharedDidReceiveResponse(WTFMove(response)) == NetworkLoadClient::ShouldContinueDidReceiveResponse::Yes)
        completionHandler(PolicyUse);
    else
        m_responseCompletionHandler = WTFMove(completionHandler);
}

void NetworkLoad::didReceiveData(Ref<SharedBuffer>&& buffer)
{
    // FIXME: This should be the encoded data length, not the decoded data length.
    auto size = buffer->size();
    m_client.didReceiveBuffer(WTFMove(buffer), size);
}

void NetworkLoad::didCompleteWithError(const ResourceError& error)
{
    if (error.isNull())
        m_client.didFinishLoading(WTF::monotonicallyIncreasingTime());
    else
        m_client.didFailLoading(error);
}

void NetworkLoad::didBecomeDownload()
{
    m_client.didBecomeDownload();
}

void NetworkLoad::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
    m_client.didSendData(totalBytesSent, totalBytesExpectedToSend);
}

void NetworkLoad::wasBlocked()
{
    m_client.didFailLoading(blockedError(m_currentRequest));
}

void NetworkLoad::cannotShowURL()
{
    m_client.didFailLoading(cannotShowURLError(m_currentRequest));
}
    
#endif

void NetworkLoad::didReceiveResponseAsync(ResourceHandle* handle, ResourceResponse&& receivedResponse)
{
    ASSERT_UNUSED(handle, handle == m_handle);
    if (sharedDidReceiveResponse(WTFMove(receivedResponse)) == NetworkLoadClient::ShouldContinueDidReceiveResponse::Yes)
        m_handle->continueDidReceiveResponse();
}

void NetworkLoad::didReceiveData(ResourceHandle*, const char* /* data */, unsigned /* length */, int /* encodedDataLength */)
{
    // The NetworkProcess should never get a didReceiveData callback.
    // We should always be using didReceiveBuffer.
    ASSERT_NOT_REACHED();
}

void NetworkLoad::didReceiveBuffer(ResourceHandle* handle, Ref<SharedBuffer>&& buffer, int reportedEncodedDataLength)
{
    ASSERT_UNUSED(handle, handle == m_handle);
    m_client.didReceiveBuffer(WTFMove(buffer), reportedEncodedDataLength);
}

void NetworkLoad::didFinishLoading(ResourceHandle* handle, double finishTime)
{
    ASSERT_UNUSED(handle, handle == m_handle);
    m_client.didFinishLoading(finishTime);
}

void NetworkLoad::didFail(ResourceHandle* handle, const ResourceError& error)
{
    ASSERT_UNUSED(handle, !handle || handle == m_handle);
    ASSERT(!error.isNull());

    m_client.didFailLoading(error);
}

void NetworkLoad::willSendRequestAsync(ResourceHandle* handle, ResourceRequest&& request, ResourceResponse&& redirectResponse)
{
    ASSERT_UNUSED(handle, handle == m_handle);
    sharedWillSendRedirectedRequest(WTFMove(request), WTFMove(redirectResponse));
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void NetworkLoad::canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle* handle, const ProtectionSpace& protectionSpace)
{
    ASSERT(RunLoop::isMain());
    ASSERT_UNUSED(handle, handle == m_handle);

    // Handle server trust evaluation at platform-level if requested, for performance reasons.
    if (protectionSpace.authenticationScheme() == ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested
        && !NetworkProcess::singleton().canHandleHTTPSServerTrustEvaluation()) {
        continueCanAuthenticateAgainstProtectionSpace(false);
        return;
    }

#if !USE(NETWORK_SESSION)
    m_waitingForContinueCanAuthenticateAgainstProtectionSpace = true;
#endif
    m_client.canAuthenticateAgainstProtectionSpaceAsync(protectionSpace);
}
#endif

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void NetworkLoad::continueCanAuthenticateAgainstProtectionSpace(bool result)
{
#if USE(NETWORK_SESSION)
    ASSERT_WITH_MESSAGE(!m_handle, "Blobs should never give authentication challenges");
    ASSERT(m_challengeCompletionHandler);
    auto completionHandler = std::exchange(m_challengeCompletionHandler, nullptr);
    if (!result) {
        if (m_task && m_task->allowsSpecificHTTPSCertificateForHost(*m_challenge))
            completionHandler(AuthenticationChallengeDisposition::UseCredential, serverTrustCredential(*m_challenge));
        else
            completionHandler(AuthenticationChallengeDisposition::RejectProtectionSpace, { });
        return;
    }

    if (m_parameters.clientCredentialPolicy == ClientCredentialPolicy::CannotAskClientForCredentials) {
        completionHandler(AuthenticationChallengeDisposition::UseCredential, { });
        return;
    }
    
    if (m_task) {
        if (auto* pendingDownload = m_task->pendingDownload())
            NetworkProcess::singleton().authenticationManager().didReceiveAuthenticationChallenge(*pendingDownload, *m_challenge, WTFMove(completionHandler));
        else
            NetworkProcess::singleton().authenticationManager().didReceiveAuthenticationChallenge(m_parameters.webPageID, m_parameters.webFrameID, *m_challenge, WTFMove(completionHandler));
    }
#else
    m_waitingForContinueCanAuthenticateAgainstProtectionSpace = false;
#endif
    if (m_handle)
        m_handle->continueCanAuthenticateAgainstProtectionSpace(result);
}
#endif

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
bool NetworkLoad::supportsDataArray()
{
    notImplemented();
    return false;
}

void NetworkLoad::didReceiveDataArray(ResourceHandle*, CFArrayRef)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}
#endif

void NetworkLoad::didSendData(ResourceHandle* handle, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    m_client.didSendData(bytesSent, totalBytesToBeSent);
}

void NetworkLoad::wasBlocked(ResourceHandle* handle)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    didFail(handle, WebKit::blockedError(m_currentRequest));
}

void NetworkLoad::cannotShowURL(ResourceHandle* handle)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    didFail(handle, WebKit::cannotShowURLError(m_currentRequest));
}

bool NetworkLoad::shouldUseCredentialStorage(ResourceHandle* handle)
{
    ASSERT_UNUSED(handle, handle == m_handle || !m_handle); // m_handle will be 0 if called from ResourceHandle::start().

    // When the WebProcess is handling loading a client is consulted each time this shouldUseCredentialStorage question is asked.
    // In NetworkProcess mode we ask the WebProcess client up front once and then reuse the cached answer.

    // We still need this sync version, because ResourceHandle itself uses it internally, even when the delegate uses an async one.

    return m_parameters.allowStoredCredentials == AllowStoredCredentials;
}

void NetworkLoad::didReceiveAuthenticationChallenge(ResourceHandle* handle, const AuthenticationChallenge& challenge)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    if (m_parameters.clientCredentialPolicy == ClientCredentialPolicy::CannotAskClientForCredentials) {
        challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    NetworkProcess::singleton().authenticationManager().didReceiveAuthenticationChallenge(m_parameters.webPageID, m_parameters.webFrameID, challenge);
}

void NetworkLoad::receivedCancellation(ResourceHandle* handle, const AuthenticationChallenge&)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    m_handle->cancel();
    didFail(m_handle.get(), cancelledError(m_currentRequest));
}

} // namespace WebKit
