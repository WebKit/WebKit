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
#include <WebCore/SessionID.h>
#include <wtf/MainThread.h>

#if !USE(NETWORK_SESSION)
#include <WebCore/ResourceHandle.h>
#endif

namespace WebKit {

using namespace WebCore;

NetworkLoad::NetworkLoad(NetworkLoadClient& client, const NetworkLoadParameters& parameters)
    : m_client(client)
    , m_parameters(parameters)
    , m_networkingContext(RemoteNetworkingContext::create(parameters.sessionID, parameters.shouldClearReferrerOnHTTPSToHTTPRedirect))
#if USE(NETWORK_SESSION)
    , m_task(SessionTracker::networkSession(parameters.sessionID)->createDataTaskWithRequest(parameters.request, *this))
#endif
    , m_currentRequest(parameters.request)
{
#if USE(NETWORK_SESSION)
    m_task->resume();
#else
    m_handle = ResourceHandle::create(m_networkingContext.get(), parameters.request, this, parameters.defersLoading, parameters.contentSniffingPolicy == SniffContent);
#endif
}

NetworkLoad::~NetworkLoad()
{
    ASSERT(RunLoop::isMain());
#if USE(NETWORK_SESSION)
    if (m_responseCompletionHandler)
        m_responseCompletionHandler(PolicyIgnore);
    m_task->clearClient();
#else
    if (m_handle)
        m_handle->clearClient();
#endif
}

void NetworkLoad::setDefersLoading(bool defers)
{
#if USE(NETWORK_SESSION)
    // FIXME: Do something here.
    notImplemented();
#else
    if (m_handle)
        m_handle->setDefersLoading(defers);
#endif
}

void NetworkLoad::cancel()
{
#if USE(NETWORK_SESSION)
    m_task->cancel();
#else
    if (m_handle)
        m_handle->cancel();
#endif
}

void NetworkLoad::continueWillSendRequest(const WebCore::ResourceRequest& newRequest)
{
#if PLATFORM(COCOA)
    m_currentRequest.updateFromDelegatePreservingOldProperties(newRequest.nsURLRequest(DoNotUpdateHTTPBody));
#elif USE(SOUP)
    // FIXME: Implement ResourceRequest::updateFromDelegatePreservingOldProperties. See https://bugs.webkit.org/show_bug.cgi?id=126127.
    m_currentRequest.updateFromDelegatePreservingOldProperties(newRequest);
#endif

    if (m_currentRequest.isNull()) {
#if USE(NETWORK_SESSION)
        // FIXME: Do something here.
        notImplemented();
#else
        m_handle->cancel();
        didFail(m_handle.get(), cancelledError(m_currentRequest));
#endif
        return;
    }

#if USE(NETWORK_SESSION)
    // FIXME: Do something here.
    notImplemented();
#else
    m_handle->continueWillSendRequest(m_currentRequest);
#endif
}

void NetworkLoad::continueDidReceiveResponse()
{
#if USE(NETWORK_SESSION)
    ASSERT(m_responseCompletionHandler);
    m_responseCompletionHandler(PolicyUse);
    m_responseCompletionHandler = nullptr;
#else
    m_handle->continueDidReceiveResponse();
#endif
}

NetworkLoadClient::ShouldContinueDidReceiveResponse NetworkLoad::sharedDidReceiveResponse(const ResourceResponse& receivedResponse)
{
    ResourceResponse response = receivedResponse;
    response.setSource(ResourceResponse::Source::Network);
    if (m_parameters.needsCertificateInfo)
        response.includeCertificateInfo();

    return m_client.didReceiveResponse(response);
}

void NetworkLoad::sharedWillSendRedirectedRequest(const ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    // We only expect to get the willSendRequest callback from ResourceHandle as the result of a redirect.
    ASSERT(!redirectResponse.isNull());
    ASSERT(RunLoop::isMain());

    m_currentRequest = request;
    m_client.willSendRedirectedRequest(request, redirectResponse);
}

#if USE(NETWORK_SESSION)

void NetworkLoad::convertTaskToDownload()
{
    ASSERT(m_responseCompletionHandler);
    m_responseCompletionHandler(PolicyDownload);
    m_responseCompletionHandler = nullptr;
}

void NetworkLoad::willPerformHTTPRedirection(const ResourceResponse& response, const ResourceRequest& request, std::function<void(const ResourceRequest&)> completionHandler)
{
    sharedWillSendRedirectedRequest(request, response);
    completionHandler(request);
}

void NetworkLoad::didReceiveChallenge(const AuthenticationChallenge& challenge, std::function<void(AuthenticationChallengeDisposition, const Credential&)> completionHandler)
{
    // NetworkResourceLoader does not know whether the request is cross origin, so Web process computes an applicable credential policy for it.
    ASSERT(m_parameters.clientCredentialPolicy != DoNotAskClientForCrossOriginCredentials);

    // Handle server trust evaluation at platform-level if requested, for performance reasons.
    if (challenge.protectionSpace().authenticationScheme() == ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested
        && !NetworkProcess::singleton().canHandleHTTPSServerTrustEvaluation()) {
        completionHandler(AuthenticationChallengeDisposition::PerformDefaultHandling, Credential());
        return;
    }

    if (m_client.isSynchronous()) {
        // FIXME: We should ask the WebProcess like the asynchronous case below does.
        // This is currently impossible as the WebProcess is blocked waiting on this synchronous load.
        // It's possible that we can jump straight to the UI process to resolve this.
        completionHandler(AuthenticationChallengeDisposition::PerformDefaultHandling, Credential());
        return;
    }

    m_challengeCompletionHandler = completionHandler;
    m_challenge = challenge;
    m_client.canAuthenticateAgainstProtectionSpaceAsync(challenge.protectionSpace());
}

void NetworkLoad::didReceiveResponse(const ResourceResponse& response, ResponseCompletionHandler completionHandler)
{
    ASSERT(isMainThread());
    if (sharedDidReceiveResponse(response) == NetworkLoadClient::ShouldContinueDidReceiveResponse::Yes)
        completionHandler(PolicyUse);
    else
        m_responseCompletionHandler = completionHandler;
}

void NetworkLoad::didReceiveData(RefPtr<SharedBuffer>&& buffer)
{
    ASSERT(buffer);
    auto size = buffer->size();
    m_client.didReceiveBuffer(WTF::move(buffer), size);
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
    m_client.didConvertToDownload();
}

#else

void NetworkLoad::didReceiveResponseAsync(ResourceHandle* handle, const ResourceResponse& receivedResponse)
{
    ASSERT_UNUSED(handle, handle == m_handle);
    if (sharedDidReceiveResponse(receivedResponse) == NetworkLoadClient::ShouldContinueDidReceiveResponse::Yes)
        m_handle->continueDidReceiveResponse();
}

void NetworkLoad::didReceiveData(ResourceHandle*, const char* /* data */, unsigned /* length */, int /* encodedDataLength */)
{
    // The NetworkProcess should never get a didReceiveData callback.
    // We should always be using didReceiveBuffer.
    ASSERT_NOT_REACHED();
}

void NetworkLoad::didReceiveBuffer(ResourceHandle* handle, PassRefPtr<SharedBuffer> buffer, int reportedEncodedDataLength)
{
    ASSERT_UNUSED(handle, handle == m_handle);
    m_client.didReceiveBuffer(WTF::move(buffer), reportedEncodedDataLength);
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

void NetworkLoad::willSendRequestAsync(ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT_UNUSED(handle, handle == m_handle);
    sharedWillSendRedirectedRequest(request, redirectResponse);
}

#endif // USE(NETWORK_SESSION)

#if USE(PROTECTION_SPACE_AUTH_CALLBACK) && !USE(NETWORK_SESSION)
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

    if (m_client.isSynchronous()) {
        // FIXME: We should ask the WebProcess like the asynchronous case below does.
        // This is currently impossible as the WebProcess is blocked waiting on this synchronous load.
        // It's possible that we can jump straight to the UI process to resolve this.
        continueCanAuthenticateAgainstProtectionSpace(true);
        return;
    }

    m_client.canAuthenticateAgainstProtectionSpaceAsync(protectionSpace);
}
#endif

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void NetworkLoad::continueCanAuthenticateAgainstProtectionSpace(bool result)
{
#if USE(NETWORK_SESSION)
    ASSERT(m_challengeCompletionHandler);
    auto completionHandler = WTF::move(m_challengeCompletionHandler);
    if (!result) {
        completionHandler(AuthenticationChallengeDisposition::PerformDefaultHandling, Credential());
        return;
    }
    
    if (m_parameters.clientCredentialPolicy == DoNotAskClientForAnyCredentials) {
        completionHandler(AuthenticationChallengeDisposition::UseCredential, Credential());
        return;
    }
    
    NetworkProcess::singleton().authenticationManager().didReceiveAuthenticationChallenge(m_parameters.webPageID, m_parameters.webFrameID, m_challenge, completionHandler);
#else
    m_handle->continueCanAuthenticateAgainstProtectionSpace(result);
#endif
}
#endif

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK) && !USE(NETWORK_SESSION)
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

#if !USE(NETWORK_SESSION)

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
    // NetworkResourceLoader does not know whether the request is cross origin, so Web process computes an applicable credential policy for it.
    ASSERT(m_parameters.clientCredentialPolicy != DoNotAskClientForCrossOriginCredentials);

    if (m_parameters.clientCredentialPolicy == DoNotAskClientForAnyCredentials) {
        challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(challenge);
        return;
    }

    NetworkProcess::singleton().authenticationManager().didReceiveAuthenticationChallenge(m_parameters.webPageID, m_parameters.webFrameID, challenge);
}

void NetworkLoad::didCancelAuthenticationChallenge(ResourceHandle* handle, const AuthenticationChallenge&)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    // This function is probably not needed (see <rdar://problem/8960124>).
    notImplemented();
}

void NetworkLoad::receivedCancellation(ResourceHandle* handle, const AuthenticationChallenge&)
{
    ASSERT_UNUSED(handle, handle == m_handle);

    m_handle->cancel();
    didFail(m_handle.get(), cancelledError(m_currentRequest));
}

#endif // !USE(NETWORK_SESSION)

} // namespace WebKit
