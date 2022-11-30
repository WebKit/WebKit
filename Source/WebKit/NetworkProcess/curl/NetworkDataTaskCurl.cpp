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

#include "APIError.h"
#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationManager.h"
#include "DataReference.h"
#include "Download.h"
#include "NetworkProcess.h"
#include "NetworkSessionCurl.h"
#include "PrivateRelayed.h"
#include "WebErrors.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/CookieJar.h>
#include <WebCore/CurlRequest.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/ShouldRelaxThirdPartyCookieBlocking.h>
#include <WebCore/SynchronousLoaderClient.h>
#include <WebCore/TimingAllowOrigin.h>
#include <pal/text/TextEncoding.h>
#include <wtf/FileSystem.h>

namespace WebKit {

using namespace WebCore;

NetworkDataTaskCurl::NetworkDataTaskCurl(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters)
    : NetworkDataTask(session, client, parameters.request, parameters.storedCredentialsPolicy, parameters.shouldClearReferrerOnHTTPSToHTTPRedirect, parameters.isMainFrameNavigation)
    , m_frameID(parameters.webFrameID)
    , m_pageID(parameters.webPageID)
    , m_shouldRelaxThirdPartyCookieBlocking(parameters.shouldRelaxThirdPartyCookieBlocking)
    , m_sourceOrigin(parameters.sourceOrigin)
{
    m_session->registerNetworkDataTask(*this);

    auto request = parameters.request;
    if (request.url().protocolIsInHTTPFamily()) {
        if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
            auto url = request.url();
            m_user = url.user();
            m_password = url.password();
            request.removeCredentials();

            if (m_user.isEmpty() && m_password.isEmpty())
                m_initialCredential = m_session->networkStorageSession()->credentialStorage().get(m_partition, request.url());
            else
                m_session->networkStorageSession()->credentialStorage().set(m_partition, Credential(m_user, m_password, CredentialPersistenceNone), request.url());
        }
    }

#if ENABLE(TRACKING_PREVENTION)
    if (shouldBlockCookies(request))
        blockCookies();
#endif
    restrictRequestReferrerToOriginIfNeeded(request);

    m_curlRequest = createCurlRequest(WTFMove(request));
    if (!m_initialCredential.isEmpty()) {
        m_curlRequest->setUserPass(m_initialCredential.user(), m_initialCredential.password());
        m_curlRequest->setAuthenticationScheme(ProtectionSpace::AuthenticationScheme::HTTPBasic);
    }
    m_curlRequest->start();
}

NetworkDataTaskCurl::~NetworkDataTaskCurl()
{
    invalidateAndCancel();

    if (m_session)
        m_session->unregisterNetworkDataTask(*this);
}

void NetworkDataTaskCurl::resume()
{
    ASSERT(m_state != State::Running);
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Running;

    if (m_curlRequest)
        m_curlRequest->resume();
}

void NetworkDataTaskCurl::cancel()
{
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Canceling;

    if (m_curlRequest)
        m_curlRequest->cancel();

    if (isDownload())
        deleteDownloadFile();
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
    if (status == RequestStatus::NewRequest && !m_blockingCookies)
        appendCookieHeader(request);

    // Creates a CurlRequest in suspended state.
    // Then, NetworkDataTaskCurl::resume() will be called and communication resumes.
    const auto captureMetrics = shouldCaptureExtraNetworkLoadMetrics() ? CurlRequest::CaptureNetworkLoadMetrics::Extended : CurlRequest::CaptureNetworkLoadMetrics::Basic;
    return CurlRequest::create(request, *this, CurlRequest::ShouldSuspend::Yes, CurlRequest::EnableMultipart::No, captureMetrics);
}

void NetworkDataTaskCurl::curlDidSendData(CurlRequest&, unsigned long long totalBytesSent, unsigned long long totalBytesExpectedToSend)
{
    Ref protectedThis { *this };
    if (state() == State::Canceling || state() == State::Completed || !m_client)
        return;

    m_client->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

void NetworkDataTaskCurl::curlDidReceiveResponse(CurlRequest& request, CurlResponse&& receivedResponse)
{
    Ref protectedThis { *this };
    if (state() == State::Canceling || state() == State::Completed || !m_client)
        return;

    m_response = ResourceResponse(receivedResponse);
    m_response.setCertificateInfo(WTFMove(receivedResponse.certificateInfo));

    updateNetworkLoadMetrics(receivedResponse.networkLoadMetrics);
    m_response.setDeprecatedNetworkLoadMetrics(Box<NetworkLoadMetrics>::create(WTFMove(receivedResponse.networkLoadMetrics)));

    handleCookieHeaders(request.resourceRequest(), receivedResponse);

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

void NetworkDataTaskCurl::curlDidReceiveData(CurlRequest&, const SharedBuffer& buffer)
{
    Ref protectedThis { *this };
    if (state() == State::Canceling || state() == State::Completed || (!m_client && !isDownload()))
        return;

    if (isDownload()) {
        auto* download = m_session->networkProcess().downloadManager().download(m_pendingDownloadID);
        RELEASE_ASSERT(download);
        uint64_t bytesWritten = 0;
        for (auto& segment : buffer) {
            if (-1 == FileSystem::writeToFile(m_downloadDestinationFile, segment.segment->data(), segment.segment->size())) {
                download->didFail(ResourceError::httpError(CURLE_WRITE_ERROR, m_response.url()), IPC::DataReference());
                invalidateAndCancel();
                return;
            }

            bytesWritten += segment.segment->size();
        }
        download->didReceiveData(bytesWritten, 0, 0);
        return;
    }

    m_client->didReceiveData(buffer);
}

void NetworkDataTaskCurl::curlDidComplete(CurlRequest&, NetworkLoadMetrics&& networkLoadMetrics)
{
    if (state() == State::Canceling || state() == State::Completed || (!m_client && !isDownload()))
        return;

    if (isDownload()) {
        auto* download = m_session->networkProcess().downloadManager().download(m_pendingDownloadID);
        RELEASE_ASSERT(download);
        FileSystem::closeFile(m_downloadDestinationFile);
        m_downloadDestinationFile = FileSystem::invalidPlatformFileHandle;
        download->didFinish();
        return;
    }

    updateNetworkLoadMetrics(networkLoadMetrics);

    m_client->didCompleteWithError({ }, WTFMove(networkLoadMetrics));
}

void NetworkDataTaskCurl::deleteDownloadFile()
{
    if (FileSystem::isHandleValid(m_downloadDestinationFile)) {
        FileSystem::closeFile(m_downloadDestinationFile);
        FileSystem::deleteFile(m_pendingDownloadLocation);
        m_downloadDestinationFile = FileSystem::invalidPlatformFileHandle;
    }
}

void NetworkDataTaskCurl::curlDidFailWithError(CurlRequest& request, ResourceError&& resourceError, CertificateInfo&& certificateInfo)
{
    if (state() == State::Canceling || state() == State::Completed || (!m_client && !isDownload()))
        return;

    if (resourceError.isSSLCertVerificationError()) {
        tryServerTrustEvaluation(AuthenticationChallenge(request.resourceRequest().url(), certificateInfo, resourceError));
        return;
    }

    if (isDownload()) {
        deleteDownloadFile();
        auto* download = m_session->networkProcess().downloadManager().download(m_pendingDownloadID);
        RELEASE_ASSERT(download);
        download->didFail(resourceError, IPC::DataReference());
        return;
    }

    m_client->didCompleteWithError(resourceError);
}

bool NetworkDataTaskCurl::shouldRedirectAsGET(const ResourceRequest& request, bool crossOrigin)
{
    if (request.httpMethod() == "GET"_s || request.httpMethod() == "HEAD"_s)
        return false;

    if (!request.url().protocolIsInHTTPFamily())
        return true;

    if (m_response.isSeeOther())
        return true;

    if ((m_response.isMovedPermanently() || m_response.isFound()) && (request.httpMethod() == "POST"_s))
        return true;

    if (crossOrigin && (request.httpMethod() == "DELETE"_s))
        return true;

    return false;
}

void NetworkDataTaskCurl::invokeDidReceiveResponse()
{
    didReceiveResponse(ResourceResponse(m_response), NegotiatedLegacyTLS::No, PrivateRelayed::No, [this, protectedThis = Ref { *this }](PolicyAction policyAction) {
        if (m_state == State::Canceling || m_state == State::Completed)
            return;

        switch (policyAction) {
        case PolicyAction::Use:
            if (m_curlRequest)
                m_curlRequest->completeDidReceiveResponse();
            break;
        case PolicyAction::Ignore:
            invalidateAndCancel();
            break;
        case PolicyAction::Download: {
            m_downloadDestinationFile = FileSystem::openFile(m_pendingDownloadLocation, FileSystem::FileOpenMode::Write);
            if (!FileSystem::isHandleValid(m_downloadDestinationFile)) {
                if (m_client)
                    m_client->didCompleteWithError(ResourceError::httpError(CURLE_WRITE_ERROR, m_response.url()));
                invalidateAndCancel();
                return;
            }

            auto& downloadManager = m_session->networkProcess().downloadManager();
            auto download = makeUnique<Download>(downloadManager, m_pendingDownloadID, *this, *m_session, suggestedFilename());
            auto* downloadPtr = download.get();
            downloadManager.dataTaskBecameDownloadTask(m_pendingDownloadID, WTFMove(download));
            downloadPtr->didCreateDestination(m_pendingDownloadLocation);
            if (m_curlRequest)
                m_curlRequest->completeDidReceiveResponse();
            break;
        }
        default:
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

    m_hasCrossOriginRedirect = m_hasCrossOriginRedirect || !SecurityOrigin::create(m_response.url())->canRequest(request.url());

    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (m_shouldClearReferrerOnHTTPSToHTTPRedirect && !request.url().protocolIs("https"_s) && protocolIs(request.httpReferrer(), "https"_s))
        request.clearHTTPReferrer();

    bool isCrossOrigin = !protocolHostAndPortAreEqual(m_firstRequest.url(), request.url());
    if (!equalLettersIgnoringASCIICase(request.httpMethod(), "get"_s)) {
        // Change request method to GET if change was made during a previous redirection or if current redirection says so.
        if (!request.url().protocolIsInHTTPFamily() || shouldRedirectAsGET(request, isCrossOrigin)) {
            request.setHTTPMethod("GET"_s);
            request.setHTTPBody(nullptr);
            request.clearHTTPContentType();
        }
    }

    bool didChangeCredential = false;
    const auto& url = request.url();
    m_user = url.user();
    m_password = url.password();
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
            auto credential = m_session->networkStorageSession()->credentialStorage().get(m_partition, request.url());
            if (!credential.isEmpty()) {
                m_initialCredential = credential;
                didChangeCredential = true;
            }
        }
    }

#if ENABLE(TRACKING_PREVENTION)
    if (!m_blockingCookies && shouldBlockCookies(request))
        blockCookies();
#endif
    auto response = ResourceResponse(m_response);
    m_client->willPerformHTTPRedirection(WTFMove(response), WTFMove(request), [this, protectedThis = Ref { *this }, didChangeCredential](const ResourceRequest& newRequest) {
        if (newRequest.isNull() || m_state == State::Canceling)
            return;

        if (m_curlRequest)
            m_curlRequest->cancel();

        auto requestCopy = newRequest;
        restrictRequestReferrerToOriginIfNeeded(requestCopy);
        m_curlRequest = createCurlRequest(WTFMove(requestCopy));
        if (didChangeCredential && !m_initialCredential.isEmpty()) {
            m_curlRequest->setUserPass(m_initialCredential.user(), m_initialCredential.password());
            m_curlRequest->setAuthenticationScheme(ProtectionSpace::AuthenticationScheme::HTTPBasic);
        }
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
            m_session->networkStorageSession()->credentialStorage().remove(m_partition, challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            auto credential = m_session->networkStorageSession()->credentialStorage().get(m_partition, challenge.protectionSpace());
            if (!credential.isEmpty() && credential != m_initialCredential) {
                ASSERT(credential.persistence() == CredentialPersistenceNone);
                if (challenge.failureResponse().isUnauthorized()) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    m_session->networkStorageSession()->credentialStorage().set(m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }
                restartWithCredential(challenge.protectionSpace(), credential);
                return;
            }
        }
    }

    m_client->didReceiveChallenge(AuthenticationChallenge(challenge), NegotiatedLegacyTLS::No, [this, protectedThis = Ref { *this }, challenge](AuthenticationChallengeDisposition disposition, const Credential& credential) {
        if (m_state == State::Canceling || m_state == State::Completed)
            return;

        if (disposition == AuthenticationChallengeDisposition::Cancel) {
            cancel();
            m_client->didCompleteWithError(cancelledError(m_curlRequest->resourceRequest()));
            return;
        }

        if (disposition == AuthenticationChallengeDisposition::UseCredential && !credential.isEmpty()) {
            if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
                if (credential.persistence() == CredentialPersistenceForSession || credential.persistence() == CredentialPersistencePermanent)
                    m_session->networkStorageSession()->credentialStorage().set(m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
            }

            restartWithCredential(challenge.protectionSpace(), credential);
            return;
        }

        invokeDidReceiveResponse();
    });
}

void NetworkDataTaskCurl::tryProxyAuthentication(WebCore::AuthenticationChallenge&& challenge)
{
    m_client->didReceiveChallenge(AuthenticationChallenge(challenge), NegotiatedLegacyTLS::No, [this, protectedThis = Ref { *this }, challenge](AuthenticationChallengeDisposition disposition, const Credential& credential) {
        if (m_state == State::Canceling || m_state == State::Completed)
            return;

        if (disposition == AuthenticationChallengeDisposition::Cancel) {
            cancel();
            m_client->didCompleteWithError(cancelledError(m_curlRequest->resourceRequest()));
            return;
        }

        if (disposition == AuthenticationChallengeDisposition::UseCredential && !credential.isEmpty()) {
            CurlContext::singleton().setProxyUserPass(credential.user(), credential.password());
            CurlContext::singleton().setDefaultProxyAuthMethod();

            auto requestCredential = m_curlRequest ? Credential(m_curlRequest->user(), m_curlRequest->password(), CredentialPersistenceNone) : Credential();
            restartWithCredential(challenge.protectionSpace(), requestCredential);
            return;
        }

        invokeDidReceiveResponse();
    });
}

void NetworkDataTaskCurl::tryServerTrustEvaluation(AuthenticationChallenge&& challenge)
{
    m_client->didReceiveChallenge(AuthenticationChallenge(challenge), NegotiatedLegacyTLS::No, [this, protectedThis = Ref { *this }, challenge](AuthenticationChallengeDisposition disposition, const Credential& credential) {
        if (m_state == State::Canceling || m_state == State::Completed)
            return;

        if (disposition == AuthenticationChallengeDisposition::UseCredential && !credential.isEmpty()) {
            auto requestCredential = m_curlRequest ? Credential(m_curlRequest->user(), m_curlRequest->password(), CredentialPersistenceNone) : Credential();
            restartWithCredential(challenge.protectionSpace(), requestCredential);
            return;
        }

        cancel();
        m_client->didCompleteWithError(challenge.error());
    });
}

void NetworkDataTaskCurl::restartWithCredential(const ProtectionSpace& protectionSpace, const Credential& credential)
{
    ASSERT(m_curlRequest);

    auto previousRequest = m_curlRequest->resourceRequest();
    auto shouldDisableServerTrustEvaluation = protectionSpace.authenticationScheme() == ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested || m_curlRequest->isServerTrustEvaluationDisabled();
    m_curlRequest->cancel();

    m_curlRequest = createCurlRequest(WTFMove(previousRequest), RequestStatus::ReusedRequest);
    m_curlRequest->setAuthenticationScheme(protectionSpace.authenticationScheme());
    m_curlRequest->setUserPass(credential.user(), credential.password());
    if (shouldDisableServerTrustEvaluation)
        m_curlRequest->disableServerTrustEvaluation();
    m_curlRequest->start();

    if (m_state != State::Suspended) {
        m_state = State::Suspended;
        resume();
    }
}

void NetworkDataTaskCurl::appendCookieHeader(WebCore::ResourceRequest& request)
{
    auto includeSecureCookies = request.url().protocolIs("https"_s) ? IncludeSecureCookies::Yes : IncludeSecureCookies::No;
    auto cookieHeaderField = m_session->networkStorageSession()->cookieRequestHeaderFieldValue(request.firstPartyForCookies(), WebCore::SameSiteInfo::create(request), request.url(), std::nullopt, std::nullopt, includeSecureCookies, ApplyTrackingPrevention::Yes, WebCore::ShouldRelaxThirdPartyCookieBlocking::No).first;
    if (!cookieHeaderField.isEmpty())
        request.addHTTPHeaderField(HTTPHeaderName::Cookie, cookieHeaderField);
}

void NetworkDataTaskCurl::handleCookieHeaders(const WebCore::ResourceRequest& request, const CurlResponse& response)
{
    static constexpr auto setCookieHeader = "set-cookie: "_s;

    for (auto header : response.headers) {
        if (header.startsWithIgnoringASCIICase(setCookieHeader)) {
            String setCookieString = header.right(header.length() - setCookieHeader.length());
            m_session->networkStorageSession()->setCookiesFromHTTPResponse(request.firstPartyForCookies(), response.url, setCookieString);
        }
    }
}

String NetworkDataTaskCurl::suggestedFilename() const
{
    if (!m_suggestedFilename.isEmpty())
        return m_suggestedFilename;

    String suggestedFilename = m_response.suggestedFilename();
    if (!suggestedFilename.isEmpty())
        return suggestedFilename;

    return PAL::decodeURLEscapeSequences(m_response.url().lastPathComponent());
}

void NetworkDataTaskCurl::blockCookies()
{
#if ENABLE(TRACKING_PREVENTION)
    m_blockingCookies = true;
#endif
}

void NetworkDataTaskCurl::unblockCookies()
{
#if ENABLE(TRACKING_PREVENTION)
    m_blockingCookies = false;
#endif
}

bool NetworkDataTaskCurl::shouldBlockCookies(const WebCore::ResourceRequest& request)
{
#if ENABLE(TRACKING_PREVENTION)
    bool shouldBlockCookies = m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::EphemeralStateless;

    if (!shouldBlockCookies && m_session->networkStorageSession())
        shouldBlockCookies = m_session->networkStorageSession()->shouldBlockCookies(request, m_frameID, m_pageID, m_shouldRelaxThirdPartyCookieBlocking);

    if (shouldBlockCookies)
        return true;
#endif
    return false;
}

bool NetworkDataTaskCurl::isThirdPartyRequest(const WebCore::ResourceRequest& request)
{
    return !WebCore::areRegistrableDomainsEqual(request.url(), request.firstPartyForCookies());
}

void NetworkDataTaskCurl::updateNetworkLoadMetrics(WebCore::NetworkLoadMetrics& networkLoadMetrics)
{
    if (!m_startTime)
        m_startTime = networkLoadMetrics.fetchStart;

    if (!m_failsTAOCheck) {
        RefPtr<SecurityOrigin> origin = isTopLevelNavigation() ? SecurityOrigin::create(firstRequest().url()) : m_sourceOrigin;
        if (origin)
            m_failsTAOCheck = !passesTimingAllowOriginCheck(m_response, *origin);
    }

    networkLoadMetrics.redirectStart = m_startTime;
    networkLoadMetrics.redirectCount = m_redirectCount;
    networkLoadMetrics.failsTAOCheck = m_failsTAOCheck;
    networkLoadMetrics.hasCrossOriginRedirect = m_hasCrossOriginRedirect;
}

} // namespace WebKit
