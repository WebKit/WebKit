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
#import "NetworkSession.h"

#if USE(NETWORK_SESSION)

#import "Download.h"
#import "DownloadProxyMessages.h"
#import "NetworkProcess.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/AuthenticationChallenge.h>
#import <WebCore/CFNetworkSPI.h>
#import <WebCore/ResourceRequest.h>
#import <wtf/MainThread.h>

@interface NSURLSessionTask ()
@property (readwrite, copy) NSString *_pathToDownloadTaskFile;
@end

namespace WebKit {

NetworkDataTask::NetworkDataTask(NetworkSession& session, NetworkDataTaskClient& client, const WebCore::ResourceRequest& requestWithCredentials, WebCore::StoredCredentials storedCredentials, WebCore::ContentSniffingPolicy shouldContentSniff, bool shouldClearReferrerOnHTTPSToHTTPRedirect)
    : m_failureTimer(*this, &NetworkDataTask::failureTimerFired)
    , m_session(session)
    , m_client(&client)
    , m_storedCredentials(storedCredentials)
    , m_lastHTTPMethod(requestWithCredentials.httpMethod())
    , m_firstRequest(requestWithCredentials)
    , m_shouldClearReferrerOnHTTPSToHTTPRedirect(shouldClearReferrerOnHTTPSToHTTPRedirect)
{
    ASSERT(isMainThread());
    
    if (!requestWithCredentials.url().isValid()) {
        scheduleFailure(InvalidURLFailure);
        return;
    }
    
    if (!portAllowed(requestWithCredentials.url())) {
        scheduleFailure(BlockedFailure);
        return;
    }
    
    auto request = requestWithCredentials;
    m_user = request.url().user();
    m_password = request.url().pass();
    request.removeCredentials();
    
    NSURLRequest *nsRequest = request.nsURLRequest(WebCore::UpdateHTTPBody);
    if (shouldContentSniff == WebCore::DoNotSniffContent) {
        NSMutableURLRequest *mutableRequest = [[nsRequest mutableCopy] autorelease];
        [mutableRequest _setProperty:@(NO) forKey:(NSString *)_kCFURLConnectionPropertyShouldSniff];
        nsRequest = mutableRequest;
    }

    if (storedCredentials == WebCore::AllowStoredCredentials)
        m_task = [m_session.m_sessionWithCredentialStorage dataTaskWithRequest:nsRequest];
    else
        m_task = [m_session.m_sessionWithoutCredentialStorage dataTaskWithRequest:nsRequest];
    
    ASSERT(!m_session.m_dataTaskMap.contains(taskIdentifier()));
    m_session.m_dataTaskMap.add(taskIdentifier(), this);
}

NetworkDataTask::~NetworkDataTask()
{
    if (m_task) {
        ASSERT(m_session.m_dataTaskMap.contains(taskIdentifier()));
        ASSERT(m_session.m_dataTaskMap.get(taskIdentifier()) == this);
        ASSERT(isMainThread());
        m_session.m_dataTaskMap.remove(taskIdentifier());
    }
}

void NetworkDataTask::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
    if (m_client)
        m_client->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

void NetworkDataTask::didReceiveChallenge(const WebCore::AuthenticationChallenge& challenge, ChallengeCompletionHandler completionHandler)
{
    if (m_client)
        m_client->didReceiveChallenge(challenge, completionHandler);
}

void NetworkDataTask::didCompleteWithError(const WebCore::ResourceError& error)
{
    if (m_client)
        m_client->didCompleteWithError(error);
}

void NetworkDataTask::didReceiveResponse(const WebCore::ResourceResponse& response, ResponseCompletionHandler completionHandler)
{
    if (m_client)
        m_client->didReceiveResponseNetworkSession(response, completionHandler);
}

void NetworkDataTask::didReceiveData(RefPtr<WebCore::SharedBuffer>&& data)
{
    if (m_client)
        m_client->didReceiveData(WTFMove(data));
}

void NetworkDataTask::didBecomeDownload()
{
    if (m_client)
        m_client->didBecomeDownload();
}

void NetworkDataTask::willPerformHTTPRedirection(const WebCore::ResourceResponse& redirectResponse, WebCore::ResourceRequest&& request, RedirectCompletionHandler completionHandler)
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
    }
    
    if (m_client)
        m_client->willPerformHTTPRedirection(redirectResponse, request, completionHandler);
}
    
void NetworkDataTask::scheduleFailure(FailureType type)
{
    ASSERT(type != NoFailure);
    m_scheduledFailureType = type;
    m_failureTimer.startOneShot(0);
}

void NetworkDataTask::failureTimerFired()
{
    RefPtr<NetworkDataTask> protect(this);
    
    switch (m_scheduledFailureType) {
    case BlockedFailure:
        m_scheduledFailureType = NoFailure;
        if (m_client)
            m_client->wasBlocked();
        return;
    case InvalidURLFailure:
        m_scheduledFailureType = NoFailure;
        if (m_client)
            m_client->cannotShowURL();
        return;
    case NoFailure:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
}

void NetworkDataTask::setPendingDownloadLocation(const WTF::String& filename, const SandboxExtension::Handle& sandboxExtensionHandle)
{
    ASSERT(!m_sandboxExtension);
    m_sandboxExtension = SandboxExtension::create(sandboxExtensionHandle);
    if (m_sandboxExtension)
        m_sandboxExtension->consume();

    m_pendingDownloadLocation = filename;
    m_task.get()._pathToDownloadTaskFile = filename;
}

bool NetworkDataTask::tryPasswordBasedAuthentication(const WebCore::AuthenticationChallenge& challenge, ChallengeCompletionHandler completionHandler)
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
    
    if (!challenge.proposedCredential().isEmpty() && !challenge.previousFailureCount()) {
        completionHandler(AuthenticationChallengeDisposition::UseCredential, challenge.proposedCredential());
        return true;
    }
    
    return false;
}

void NetworkDataTask::transferSandboxExtensionToDownload(Download& download)
{
    download.setSandboxExtension(WTFMove(m_sandboxExtension));
}

String NetworkDataTask::suggestedFilename()
{
    return m_task.get().response.suggestedFilename;
}

WebCore::ResourceRequest NetworkDataTask::currentRequest()
{
    return [m_task currentRequest];
}

void NetworkDataTask::cancel()
{
    [m_task cancel];
}

void NetworkDataTask::resume()
{
    if (m_scheduledFailureType != NoFailure)
        m_failureTimer.startOneShot(0);
    [m_task resume];
}

void NetworkDataTask::suspend()
{
    if (m_failureTimer.isActive())
        m_failureTimer.stop();
    [m_task suspend];
}

auto NetworkDataTask::taskIdentifier() -> TaskIdentifier
{
    return [m_task taskIdentifier];
}

WebCore::Credential serverTrustCredential(const WebCore::AuthenticationChallenge& challenge)
{
    return WebCore::Credential([NSURLCredential credentialForTrust:challenge.nsURLAuthenticationChallenge().protectionSpace.serverTrust]);
}

}

#endif
