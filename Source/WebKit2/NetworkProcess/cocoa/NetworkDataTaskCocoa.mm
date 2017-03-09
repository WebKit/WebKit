/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetworkDataTaskCocoa.h"

#if USE(NETWORK_SESSION)

#import "AuthenticationManager.h"
#import "Download.h"
#import "DownloadProxyMessages.h"
#import "Logging.h"
#import "NetworkProcess.h"
#import "NetworkSessionCocoa.h"
#import "SessionTracker.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/AuthenticationChallenge.h>
#import <WebCore/CFNetworkSPI.h>
#import <WebCore/FileSystem.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/ResourceRequest.h>
#import <wtf/MainThread.h>
#import <wtf/text/Base64.h>

namespace WebKit {

#if USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
static void applyBasicAuthorizationHeader(WebCore::ResourceRequest& request, const WebCore::Credential& credential)
{
    String authenticationHeader = "Basic " + base64Encode(String(credential.user() + ":" + credential.password()).utf8());
    request.setHTTPHeaderField(WebCore::HTTPHeaderName::Authorization, authenticationHeader);
}
#endif

static float toNSURLSessionTaskPriority(WebCore::ResourceLoadPriority priority)
{
    switch (priority) {
    case WebCore::ResourceLoadPriority::VeryLow:
        return 0;
    case WebCore::ResourceLoadPriority::Low:
        return 0.25;
    case WebCore::ResourceLoadPriority::Medium:
        return 0.5;
    case WebCore::ResourceLoadPriority::High:
        return 0.75;
    case WebCore::ResourceLoadPriority::VeryHigh:
        return 1;
    }

    ASSERT_NOT_REACHED();
    return NSURLSessionTaskPriorityDefault;
}

NetworkDataTaskCocoa::NetworkDataTaskCocoa(NetworkSession& session, NetworkDataTaskClient& client, const WebCore::ResourceRequest& requestWithCredentials, WebCore::StoredCredentials storedCredentials, WebCore::ContentSniffingPolicy shouldContentSniff, bool shouldClearReferrerOnHTTPSToHTTPRedirect)
    : NetworkDataTask(session, client, requestWithCredentials, storedCredentials, shouldClearReferrerOnHTTPSToHTTPRedirect)
{
    if (m_scheduledFailureType != NoFailure)
        return;

    auto request = requestWithCredentials;
    auto url = request.url();
    if (storedCredentials == WebCore::AllowStoredCredentials && url.protocolIsInHTTPFamily()) {
        m_user = url.user();
        m_password = url.pass();
        request.removeCredentials();
        url = request.url();
    
#if USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
        if (m_user.isEmpty() && m_password.isEmpty())
            m_initialCredential = m_session->networkStorageSession().credentialStorage().get(m_partition, url);
        else
            m_session->networkStorageSession().credentialStorage().set(m_partition, WebCore::Credential(m_user, m_password, WebCore::CredentialPersistenceNone), url);
#endif
    }

#if USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
    if (!m_initialCredential.isEmpty()) {
        // FIXME: Support Digest authentication, and Proxy-Authorization.
        applyBasicAuthorizationHeader(request, m_initialCredential);
    }
#endif
    
    NSURLRequest *nsRequest = request.nsURLRequest(WebCore::UpdateHTTPBody);
    if (shouldContentSniff == WebCore::DoNotSniffContent || url.protocolIs("file")) {
        NSMutableURLRequest *mutableRequest = [[nsRequest mutableCopy] autorelease];
        [mutableRequest _setProperty:@(NO) forKey:(NSString *)_kCFURLConnectionPropertyShouldSniff];
        nsRequest = mutableRequest;
    }

    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(m_session.get());
    if (storedCredentials == WebCore::AllowStoredCredentials) {
        m_task = [cocoaSession.m_sessionWithCredentialStorage dataTaskWithRequest:nsRequest];
        ASSERT(!cocoaSession.m_dataTaskMapWithCredentials.contains([m_task taskIdentifier]));
        cocoaSession.m_dataTaskMapWithCredentials.add([m_task taskIdentifier], this);
    } else {
        m_task = [cocoaSession.m_sessionWithoutCredentialStorage dataTaskWithRequest:nsRequest];
        ASSERT(!cocoaSession.m_dataTaskMapWithoutCredentials.contains([m_task taskIdentifier]));
        cocoaSession.m_dataTaskMapWithoutCredentials.add([m_task taskIdentifier], this);
    }
    LOG(NetworkSession, "%llu Creating NetworkDataTask with URL %s", [m_task taskIdentifier], nsRequest.URL.absoluteString.UTF8String);

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
    if (session.networkStorageSession().shouldPartitionCookiesForHost(url.host())) {
        LOG(NetworkSession, "%llu Partitioning cookies for URL %s", [m_task taskIdentifier], nsRequest.URL.absoluteString.UTF8String);
        String storagePartition = WebCore::cookieStoragePartition(request);
        if (!storagePartition.isEmpty())
            m_task.get()._storagePartitionIdentifier = storagePartition;
    }
#endif

    if (WebCore::ResourceRequest::resourcePrioritiesEnabled())
        m_task.get().priority = toNSURLSessionTaskPriority(request.priority());
}

NetworkDataTaskCocoa::~NetworkDataTaskCocoa()
{
    if (!m_task)
        return;

    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(m_session.get());
    if (m_storedCredentials == WebCore::StoredCredentials::AllowStoredCredentials) {
        ASSERT(cocoaSession.m_dataTaskMapWithCredentials.get([m_task taskIdentifier]) == this);
        cocoaSession.m_dataTaskMapWithCredentials.remove([m_task taskIdentifier]);
    } else {
        ASSERT(cocoaSession.m_dataTaskMapWithoutCredentials.get([m_task taskIdentifier]) == this);
        cocoaSession.m_dataTaskMapWithoutCredentials.remove([m_task taskIdentifier]);
    }
}

void NetworkDataTaskCocoa::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
    if (m_client)
        m_client->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

void NetworkDataTaskCocoa::didReceiveChallenge(const WebCore::AuthenticationChallenge& challenge, ChallengeCompletionHandler&& completionHandler)
{
    // Proxy authentication is handled by CFNetwork internally. We can get here if the user cancels
    // CFNetwork authentication dialog, and we shouldn't ask the client to display another one in that case.
    if (challenge.protectionSpace().isProxy()) {
        completionHandler(AuthenticationChallengeDisposition::UseCredential, { });
        return;
    }

    if (tryPasswordBasedAuthentication(challenge, completionHandler))
        return;

    if (m_client)
        m_client->didReceiveChallenge(challenge, WTFMove(completionHandler));
    else {
        ASSERT_NOT_REACHED();
        completionHandler(AuthenticationChallengeDisposition::PerformDefaultHandling, { });
    }
}

void NetworkDataTaskCocoa::didCompleteWithError(const WebCore::ResourceError& error, const WebCore::NetworkLoadMetrics& networkLoadMetrics)
{
    if (m_client)
        m_client->didCompleteWithError(error, networkLoadMetrics);
}

void NetworkDataTaskCocoa::didReceiveData(Ref<WebCore::SharedBuffer>&& data)
{
    if (m_client)
        m_client->didReceiveData(WTFMove(data));
}

void NetworkDataTaskCocoa::willPerformHTTPRedirection(WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    if (redirectResponse.httpStatusCode() == 307 || redirectResponse.httpStatusCode() == 308) {
        ASSERT(m_lastHTTPMethod == request.httpMethod());
        WebCore::FormData* body = m_firstRequest.httpBody();
        if (body && !body->isEmpty() && !equalLettersIgnoringASCIICase(m_lastHTTPMethod, "get"))
            request.setHTTPBody(body);
        
        String originalContentType = m_firstRequest.httpContentType();
        if (!originalContentType.isEmpty())
            request.setHTTPHeaderField(WebCore::HTTPHeaderName::ContentType, originalContentType);
    }
    
    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (m_shouldClearReferrerOnHTTPSToHTTPRedirect && !request.url().protocolIs("https") && WebCore::protocolIs(request.httpReferrer(), "https"))
        request.clearHTTPReferrer();
    
    const auto& url = request.url();
    m_user = url.user();
    m_password = url.pass();
    m_lastHTTPMethod = request.httpMethod();
    request.removeCredentials();
    
    if (!protocolHostAndPortAreEqual(request.url(), redirectResponse.url())) {
        // The network layer might carry over some headers from the original request that
        // we want to strip here because the redirect is cross-origin.
        request.clearHTTPAuthorization();
        request.clearHTTPOrigin();
#if USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
    } else {
        // Only consider applying authentication credentials if this is actually a redirect and the redirect
        // URL didn't include credentials of its own.
        if (m_user.isEmpty() && m_password.isEmpty() && !redirectResponse.isNull()) {
            auto credential = m_session->networkStorageSession().credentialStorage().get(m_partition, request.url());
            if (!credential.isEmpty()) {
                m_initialCredential = credential;

                // FIXME: Support Digest authentication, and Proxy-Authorization.
                applyBasicAuthorizationHeader(request, m_initialCredential);
            }
        }
#endif
    }
    
    if (m_client)
        m_client->willPerformHTTPRedirection(WTFMove(redirectResponse), WTFMove(request), WTFMove(completionHandler));
    else {
        ASSERT_NOT_REACHED();
        completionHandler({ });
    }
}

void NetworkDataTaskCocoa::setPendingDownloadLocation(const WTF::String& filename, const SandboxExtension::Handle& sandboxExtensionHandle, bool allowOverwrite)
{
    NetworkDataTask::setPendingDownloadLocation(filename, sandboxExtensionHandle, allowOverwrite);

    ASSERT(!m_sandboxExtension);
    m_sandboxExtension = SandboxExtension::create(sandboxExtensionHandle);
    if (m_sandboxExtension)
        m_sandboxExtension->consume();

    m_task.get()._pathToDownloadTaskFile = m_pendingDownloadLocation;

    if (allowOverwrite && WebCore::fileExists(m_pendingDownloadLocation))
        WebCore::deleteFile(filename);
}

bool NetworkDataTaskCocoa::tryPasswordBasedAuthentication(const WebCore::AuthenticationChallenge& challenge, const ChallengeCompletionHandler& completionHandler)
{
    if (!challenge.protectionSpace().isPasswordBased())
        return false;
    
    if (!m_user.isNull() && !m_password.isNull()) {
        auto persistence = m_storedCredentials == WebCore::StoredCredentials::AllowStoredCredentials ? WebCore::CredentialPersistenceForSession : WebCore::CredentialPersistenceNone;
        completionHandler(AuthenticationChallengeDisposition::UseCredential, WebCore::Credential(m_user, m_password, persistence));
        m_user = String();
        m_password = String();
        return true;
    }

#if USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
    if (m_storedCredentials == WebCore::AllowStoredCredentials) {
        if (!m_initialCredential.isEmpty() || challenge.previousFailureCount()) {
            // The stored credential wasn't accepted, stop using it.
            // There is a race condition here, since a different credential might have already been stored by another ResourceHandle,
            // but the observable effect should be very minor, if any.
            m_session->networkStorageSession().credentialStorage().remove(m_partition, challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            auto credential = m_session->networkStorageSession().credentialStorage().get(m_partition, challenge.protectionSpace());
            if (!credential.isEmpty() && credential != m_initialCredential) {
                ASSERT(credential.persistence() == WebCore::CredentialPersistenceNone);
                if (challenge.failureResponse().httpStatusCode() == 401) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    m_session->networkStorageSession().credentialStorage().set(m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }
                completionHandler(AuthenticationChallengeDisposition::UseCredential, credential);
                return true;
            }
        }
    }
#endif

    if (!challenge.proposedCredential().isEmpty() && !challenge.previousFailureCount()) {
        completionHandler(AuthenticationChallengeDisposition::UseCredential, challenge.proposedCredential());
        return true;
    }
    
    return false;
}

void NetworkDataTaskCocoa::transferSandboxExtensionToDownload(Download& download)
{
    download.setSandboxExtension(WTFMove(m_sandboxExtension));
}

#if !USE(CFURLCONNECTION)
static bool certificatesMatch(SecTrustRef trust1, SecTrustRef trust2)
{
    if (!trust1 || !trust2)
        return false;

    CFIndex count1 = SecTrustGetCertificateCount(trust1);
    CFIndex count2 = SecTrustGetCertificateCount(trust2);
    if (count1 != count2)
        return false;

    for (CFIndex i = 0; i < count1; i++) {
        auto cert1 = SecTrustGetCertificateAtIndex(trust1, i);
        auto cert2 = SecTrustGetCertificateAtIndex(trust2, i);
        RELEASE_ASSERT(cert1);
        RELEASE_ASSERT(cert2);
        if (!CFEqual(cert1, cert2))
            return false;
    }

    return true;
}
#endif

bool NetworkDataTaskCocoa::allowsSpecificHTTPSCertificateForHost(const WebCore::AuthenticationChallenge& challenge)
{
    const String& host = challenge.protectionSpace().host();
    NSArray *certificates = [NSURLRequest allowsSpecificHTTPSCertificateForHost:host];
    if (!certificates)
        return false;
    
    bool requireServerCertificates = challenge.protectionSpace().authenticationScheme() == WebCore::ProtectionSpaceAuthenticationScheme::ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested;
    RetainPtr<SecPolicyRef> policy = adoptCF(SecPolicyCreateSSL(requireServerCertificates, host.createCFString().get()));
    
    SecTrustRef trustRef = nullptr;
    if (SecTrustCreateWithCertificates((CFArrayRef)certificates, policy.get(), &trustRef) != noErr)
        return false;
    RetainPtr<SecTrustRef> trust = adoptCF(trustRef);

#if USE(CFURLCONNECTION)
    notImplemented();
    return false;
#else
    return certificatesMatch(trust.get(), challenge.nsURLAuthenticationChallenge().protectionSpace.serverTrust);
#endif
}

String NetworkDataTaskCocoa::suggestedFilename() const
{
    if (!m_suggestedFilename.isEmpty())
        return m_suggestedFilename;
    return m_task.get().response.suggestedFilename;
}

void NetworkDataTaskCocoa::cancel()
{
    [m_task cancel];
}

void NetworkDataTaskCocoa::resume()
{
    if (m_scheduledFailureType != NoFailure)
        m_failureTimer.startOneShot(0);
    [m_task resume];
}

void NetworkDataTaskCocoa::suspend()
{
    if (m_failureTimer.isActive())
        m_failureTimer.stop();
    [m_task suspend];
}

NetworkDataTask::State NetworkDataTaskCocoa::state() const
{
    switch ([m_task state]) {
    case NSURLSessionTaskStateRunning:
        return State::Running;
    case NSURLSessionTaskStateSuspended:
        return State::Suspended;
    case NSURLSessionTaskStateCanceling:
        return State::Canceling;
    case NSURLSessionTaskStateCompleted:
        return State::Completed;
    }

    ASSERT_NOT_REACHED();
    return State::Completed;
}

WebCore::Credential serverTrustCredential(const WebCore::AuthenticationChallenge& challenge)
{
#if USE(CFURLCONNECTION)
    notImplemented();
    return { };
#else
    return WebCore::Credential([NSURLCredential credentialForTrust:challenge.nsURLAuthenticationChallenge().protectionSpace.serverTrust]);
#endif
}

}

#endif
