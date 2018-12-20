/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "NetworkDataTaskCurl.h"

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationManager.h"
#include "NetworkSessionCurl.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/CookiesStrategy.h>
#include <WebCore/CurlRequest.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <WebCore/SameSiteInfo.h>

using namespace WebCore;

namespace WebKit {

NetworkDataTaskCurl::NetworkDataTaskCurl(NetworkSession& session, NetworkDataTaskClient& client, const ResourceRequest& requestWithCredentials, StoredCredentialsPolicy storedCredentialsPolicy, ContentSniffingPolicy shouldContentSniff, ContentEncodingSniffingPolicy, bool shouldClearReferrerOnHTTPSToHTTPRedirect, bool dataTaskIsForMainFrameNavigation)
    : NetworkDataTask(session, client, requestWithCredentials, storedCredentialsPolicy, shouldClearReferrerOnHTTPSToHTTPRedirect, dataTaskIsForMainFrameNavigation)
{
    if (m_scheduledFailureType != NoFailure)
        return;

    m_startTime = MonotonicTime::now();

    auto request = requestWithCredentials;
    if (request.url().protocolIsInHTTPFamily()) {
        if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
            auto url = request.url();
            m_user = url.user();
            m_password = url.pass();
            request.removeCredentials();

            if (m_user.isEmpty() && m_password.isEmpty())
                m_initialCredential = m_session->networkStorageSession().credentialStorage().get(m_partition, request.url());
            else
                m_session->networkStorageSession().credentialStorage().set(m_partition, Credential(m_user, m_password, CredentialPersistenceNone), request.url());
        }
    }

    m_curlRequest = createCurlRequest(WTFMove(request));
    if (!m_initialCredential.isEmpty()) {
        m_curlRequest->setUserPass(m_initialCredential.user(), m_initialCredential.password());
        m_curlRequest->setAuthenticationScheme(ProtectionSpaceAuthenticationSchemeHTTPBasic);
    }
    m_curlRequest->setStartTime(m_startTime);
    m_curlRequest->start();
}

NetworkDataTaskCurl::~NetworkDataTaskCurl()
{
    if (m_curlRequest)
        m_curlRequest->invalidateClient();
}

void NetworkDataTaskCurl::resume()
{
    ASSERT(m_state != State::Running);
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Running;

    if (m_scheduledFailureType != NoFailure) {
        ASSERT(m_failureTimer.isActive());
        return;
    }

    if (m_curlRequest)
        m_curlRequest->resume();
}

void NetworkDataTaskCurl::suspend()
{
    ASSERT(m_state != State::Suspended);
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Suspended;

    if (m_curlRequest)
        m_curlRequest->suspend();
}

void NetworkDataTaskCurl::cancel()
{
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Canceling;

    if (m_curlRequest)
        m_curlRequest->cancel();
}

void NetworkDataTaskCurl::invalidateAndCancel()
{
    cancel();

    if (m_curlRequest)
        m_curlRequest->invalidateClient();
}

NetworkDataTask::State NetworkDataTaskCurl::state() const
{
    return m_state;
}

Ref<CurlRequest> NetworkDataTaskCurl::createCurlRequest(ResourceRequest&& request, RequestStatus status)
{
    if (status == RequestStatus::NewRequest)
        appendCookieHeader(request);

    // Creates a CurlRequest in suspended state.
    // Then, NetworkDataTaskCurl::resume() will be called and communication resumes.
    const auto captureMetrics = shouldCaptureExtraNetworkLoadMetrics() ? CurlRequest::CaptureNetworkLoadMetrics::Extended : CurlRequest::CaptureNetworkLoadMetrics::Basic;
    return CurlRequest::create(request, *this, CurlRequest::ShouldSuspend::Yes, CurlRequest::EnableMultipart::No, captureMetrics);
}

void NetworkDataTaskCurl::curlDidSendData(CurlRequest&, unsigned long long totalBytesSent, unsigned long long totalBytesExpectedToSend)
{
    auto protectedThis = makeRef(*this);
    if (state() == State::Canceling || state() == State::Completed || !m_client)
        return;

    m_client->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

void NetworkDataTaskCurl::curlDidReceiveResponse(CurlRequest& request, const CurlResponse& receivedResponse)
{
    auto protectedThis = makeRef(*this);
    if (state() == State::Canceling || state() == State::Completed || !m_client)
        return;

    m_response = ResourceResponse(receivedResponse);
    m_response.setDeprecatedNetworkLoadMetrics(request.networkLoadMetrics().isolatedCopy());

    handleCookieHeaders(receivedResponse);

    if (m_response.shouldRedirect()) {
        willPerformHTTPRedirection();
        return;
    }

    if (m_response.isUnauthorized() && receivedResponse.availableHttpAuth) {
        tryHttpAuthentication(AuthenticationChallenge(receivedResponse, m_authFailureCount, m_response));
        m_authFailureCount++;
        return;
    }

    if (m_response.isProxyAuthenticationRequired() && receivedResponse.availableProxyAuth) {
        tryProxyAuthentication(AuthenticationChallenge(receivedResponse, 0, m_response));
        return;
    }

    invokeDidReceiveResponse();
}

void NetworkDataTaskCurl::curlDidReceiveBuffer(CurlRequest&, Ref<SharedBuffer>&& buffer)
{
    auto protectedThis = makeRef(*this);
    if (state() == State::Canceling || state() == State::Completed || (!m_client && !isDownload()))
        return;

    m_client->didReceiveData(WTFMove(buffer));
}

void NetworkDataTaskCurl::curlDidComplete(CurlRequest& request)
{
    if (state() == State::Canceling || state() == State::Completed || (!m_client && !isDownload()))
        return;

    m_response.setDeprecatedNetworkLoadMetrics(request.networkLoadMetrics().isolatedCopy());

    m_client->didCompleteWithError({ }, m_response.deprecatedNetworkLoadMetrics());
}

void NetworkDataTaskCurl::curlDidFailWithError(CurlRequest&, const ResourceError& resourceError)
{
    if (state() == State::Canceling || state() == State::Completed || (!m_client && !isDownload()))
        return;

    m_client->didCompleteWithError(resourceError);
}

bool NetworkDataTaskCurl::shouldRedirectAsGET(const ResourceRequest& request, bool crossOrigin)
{
    if (request.httpMethod() == "GET" || request.httpMethod() == "HEAD")
        return false;

    if (!request.url().protocolIsInHTTPFamily())
        return true;

    if (m_response.isSeeOther())
        return true;

    if ((m_response.isMovedPermanently() || m_response.isFound()) && (request.httpMethod() == "POST"))
        return true;

    if (crossOrigin && (request.httpMethod() == "DELETE"))
        return true;

    return false;
}

void NetworkDataTaskCurl::invokeDidReceiveResponse()
{
    didReceiveResponse(ResourceResponse(m_response), [this, protectedThis = makeRef(*this)](PolicyAction policyAction) {
        if (m_state == State::Canceling || m_state == State::Completed)
            return;

        switch (policyAction) {
        case PolicyAction::Use:
            if (m_curlRequest)
                m_curlRequest->completeDidReceiveResponse();
            break;
        case PolicyAction::Ignore:
            break;
        case PolicyAction::Download:
            notImplemented();
            break;
        }
    });
}

void NetworkDataTaskCurl::willPerformHTTPRedirection()
{
    static const int maxRedirects = 20;

    if (m_redirectCount++ > maxRedirects) {
        m_client->didCompleteWithError(ResourceError::httpError(CURLE_TOO_MANY_REDIRECTS, m_response.url()));
        return;
    }

    ResourceRequest request = m_firstRequest;
    URL redirectedURL = URL(m_response.url(), m_response.httpHeaderField(HTTPHeaderName::Location));
    if (!redirectedURL.hasFragmentIdentifier() && request.url().hasFragmentIdentifier())
        redirectedURL.setFragmentIdentifier(request.url().fragmentIdentifier());
    request.setURL(redirectedURL);

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (m_shouldClearReferrerOnHTTPSToHTTPRedirect && !request.url().protocolIs("https") && protocolIs(request.httpReferrer(), "https"))
        request.clearHTTPReferrer();

    bool isCrossOrigin = !protocolHostAndPortAreEqual(m_firstRequest.url(), request.url());
    if (!equalLettersIgnoringASCIICase(request.httpMethod(), "get")) {
        // Change request method to GET if change was made during a previous redirection or if current redirection says so.
        if (!request.url().protocolIsInHTTPFamily() || shouldRedirectAsGET(request, isCrossOrigin)) {
            request.setHTTPMethod("GET");
            request.setHTTPBody(nullptr);
            request.clearHTTPContentType();
        }
    }

    bool didChangeCredential = false;
    const auto& url = request.url();
    m_user = url.user();
    m_password = url.pass();
    m_lastHTTPMethod = request.httpMethod();
    request.removeCredentials();

    if (isCrossOrigin) {
        // The network layer might carry over some headers from the original request that
        // we want to strip here because the redirect is cross-origin.
        request.clearHTTPAuthorization();
        request.clearHTTPOrigin();
    } else if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
        // Only consider applying authentication credentials if this is actually a redirect and the redirect
        // URL didn't include credentials of its own.
        if (m_user.isEmpty() && m_password.isEmpty()) {
            auto credential = m_session->networkStorageSession().credentialStorage().get(m_partition, request.url());
            if (!credential.isEmpty()) {
                m_initialCredential = credential;
                didChangeCredential = true;
            }
        }
    }

    auto response = ResourceResponse(m_response);
    m_client->willPerformHTTPRedirection(WTFMove(response), WTFMove(request), [this, protectedThis = makeRef(*this), didChangeCredential, isCrossOrigin](const ResourceRequest& newRequest) {
        if (newRequest.isNull() || m_state == State::Canceling)
            return;

        if (m_curlRequest)
            m_curlRequest->cancel();

        if (newRequest.url().protocolIsInHTTPFamily() && isCrossOrigin)
            m_startTime = MonotonicTime::now();

        auto requestCopy = newRequest;
        m_curlRequest = createCurlRequest(WTFMove(requestCopy));
        if (didChangeCredential && !m_initialCredential.isEmpty()) {
            m_curlRequest->setUserPass(m_initialCredential.user(), m_initialCredential.password());
            m_curlRequest->setAuthenticationScheme(ProtectionSpaceAuthenticationSchemeHTTPBasic);
        }
        m_curlRequest->setStartTime(m_startTime);
        m_curlRequest->start();

        if (m_state != State::Suspended) {
            m_state = State::Suspended;
            resume();
        }
    });
}

void NetworkDataTaskCurl::tryHttpAuthentication(AuthenticationChallenge&& challenge)
{
    if (!m_user.isNull() && !m_password.isNull()) {
        auto persistence = m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use ? WebCore::CredentialPersistenceForSession : WebCore::CredentialPersistenceNone;
        restartWithCredential(challenge.protectionSpace(), Credential(m_user, m_password, persistence));
        m_user = String();
        m_password = String();
        return;
    }

    if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
        if (!m_initialCredential.isEmpty() || challenge.previousFailureCount()) {
            // The stored credential wasn't accepted, stop using it. There is a race condition
            // here, since a different credential might have already been stored by another
            // NetworkDataTask, but the observable effect should be very minor, if any.
            m_session->networkStorageSession().credentialStorage().remove(m_partition, challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            auto credential = m_session->networkStorageSession().credentialStorage().get(m_partition, challenge.protectionSpace());
            if (!credential.isEmpty() && credential != m_initialCredential) {
                ASSERT(credential.persistence() == CredentialPersistenceNone);
                if (challenge.failureResponse().isUnauthorized()) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    m_session->networkStorageSession().credentialStorage().set(m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }
                restartWithCredential(challenge.protectionSpace(), credential);
                return;
            }
        }
    }

    m_client->didReceiveChallenge(AuthenticationChallenge(challenge), [this, protectedThis = makeRef(*this), challenge](AuthenticationChallengeDisposition disposition, const Credential& credential) {
        if (m_state == State::Canceling || m_state == State::Completed)
            return;

        if (disposition == AuthenticationChallengeDisposition::Cancel) {
            cancel();
            m_client->didCompleteWithError(ResourceError::httpError(CURLE_COULDNT_RESOLVE_HOST, m_response.url()));
            return;
        }

        if (disposition == AuthenticationChallengeDisposition::UseCredential && (!credential.isEmpty() || !m_didChallengeEmptyCredentialForAuth)) {
            // When "isAllowedToAskUserForCredentials" is false, an empty credential, which might cause
            // an infinite authentication loop. To avoid such infinite loop, a HTTP authentication with empty
            // user and password is processed only once.
            if (credential.isEmpty())
                m_didChallengeEmptyCredentialForAuth = true;

            if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
                if (credential.persistence() == CredentialPersistenceForSession || credential.persistence() == CredentialPersistencePermanent)
                    m_session->networkStorageSession().credentialStorage().set(m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
            }

            restartWithCredential(challenge.protectionSpace(), credential);
            return;
        }

        invokeDidReceiveResponse();
    });
}

void NetworkDataTaskCurl::tryProxyAuthentication(WebCore::AuthenticationChallenge&& challenge)
{
    m_client->didReceiveChallenge(AuthenticationChallenge(challenge), [this, protectedThis = makeRef(*this), challenge](AuthenticationChallengeDisposition disposition, const Credential& credential) {
        if (m_state == State::Canceling || m_state == State::Completed)
            return;

        if (disposition == AuthenticationChallengeDisposition::Cancel) {
            cancel();
            m_client->didCompleteWithError(ResourceError::httpError(CURLE_COULDNT_RESOLVE_PROXY, m_response.url()));
            return;
        }

        if (disposition == AuthenticationChallengeDisposition::UseCredential && (!credential.isEmpty() || !m_didChallengeEmptyCredentialForProxyAuth)) {
            if (credential.isEmpty())
                m_didChallengeEmptyCredentialForProxyAuth = true;

            CurlContext::singleton().setProxyUserPass(credential.user(), credential.password());
            CurlContext::singleton().setDefaultProxyAuthMethod();

            auto requestCredential = m_curlRequest ? Credential(m_curlRequest->user(), m_curlRequest->password(), CredentialPersistenceNone) : Credential();
            restartWithCredential(challenge.protectionSpace(), requestCredential);
            return;
        }

        invokeDidReceiveResponse();
    });
}

void NetworkDataTaskCurl::restartWithCredential(const ProtectionSpace& protectionSpace, const Credential& credential)
{
    ASSERT(m_curlRequest);

    auto previousRequest = m_curlRequest->resourceRequest();
    m_curlRequest->cancel();

    m_curlRequest = createCurlRequest(WTFMove(previousRequest), RequestStatus::ReusedRequest);
    m_curlRequest->setAuthenticationScheme(protectionSpace.authenticationScheme());
    m_curlRequest->setUserPass(credential.user(), credential.password());
    m_curlRequest->setStartTime(m_startTime);
    m_curlRequest->start();

    if (m_state != State::Suspended) {
        m_state = State::Suspended;
        resume();
    }
}

void NetworkDataTaskCurl::appendCookieHeader(WebCore::ResourceRequest& request)
{
    const auto& storageSession = m_session->networkStorageSession();
    const auto& cookieJar = storageSession.cookieStorage();
    auto includeSecureCookies = request.url().protocolIs("https") ? IncludeSecureCookies::Yes : IncludeSecureCookies::No;
    auto cookieHeaderField = cookieJar.cookieRequestHeaderFieldValue(storageSession, request.firstPartyForCookies(), WebCore::SameSiteInfo::create(request), request.url(), WTF::nullopt, WTF::nullopt, includeSecureCookies).first;
    if (!cookieHeaderField.isEmpty())
        request.addHTTPHeaderField(HTTPHeaderName::Cookie, cookieHeaderField);
}

void NetworkDataTaskCurl::handleCookieHeaders(const CurlResponse& response)
{
    static const auto setCookieHeader = "set-cookie: ";

    const auto& storageSession = m_session->networkStorageSession();
    const auto& cookieJar = storageSession.cookieStorage();
    for (auto header : response.headers) {
        if (header.startsWithIgnoringASCIICase(setCookieHeader)) {
            String setCookieString = header.right(header.length() - strlen(setCookieHeader));
            cookieJar.setCookiesFromHTTPResponse(storageSession, response.url, setCookieString);
        }
    }
}

} // namespace WebKit
