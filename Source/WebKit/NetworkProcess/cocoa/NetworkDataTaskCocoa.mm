/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#import "AuthenticationChallengeDisposition.h"
#import "AuthenticationManager.h"
#import "Download.h"
#import "DownloadProxyMessages.h"
#import "Logging.h"
#import "NetworkProcess.h"
#import "NetworkProximityManager.h"
#import "NetworkSessionCocoa.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/AuthenticationChallenge.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/ResourceRequest.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/FileSystem.h>
#import <wtf/MainThread.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/text/Base64.h>

#if HAVE(NW_ACTIVITY)
#import <CFNetwork/CFNSURLConnection.h>
#endif

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

void NetworkDataTaskCocoa::applySniffingPoliciesAndBindRequestToInferfaceIfNeeded(__strong NSURLRequest *& nsRequest, bool shouldContentSniff, bool shouldContentEncodingSniff)
{
#if !USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
    UNUSED_PARAM(shouldContentEncodingSniff);
#endif

    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(m_session.get());
    if (shouldContentSniff
#if USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
        && shouldContentEncodingSniff
#endif
        && cocoaSession.m_boundInterfaceIdentifier.isNull()
        && !cocoaSession.m_proxyConfiguration)
        return;

    auto mutableRequest = adoptNS([nsRequest mutableCopy]);

#if USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
    if (!shouldContentEncodingSniff)
        [mutableRequest _setProperty:@(YES) forKey:(NSString *)kCFURLRequestContentDecoderSkipURLCheck];
#endif

    if (!shouldContentSniff)
        [mutableRequest _setProperty:@(NO) forKey:(NSString *)_kCFURLConnectionPropertyShouldSniff];

    if (!cocoaSession.m_boundInterfaceIdentifier.isNull())
        [mutableRequest setBoundInterfaceIdentifier:cocoaSession.m_boundInterfaceIdentifier];

    if (cocoaSession.m_proxyConfiguration)
        CFURLRequestSetProxySettings([mutableRequest _CFURLRequest], cocoaSession.m_proxyConfiguration.get());

    nsRequest = mutableRequest.autorelease();
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
NSHTTPCookieStorage *NetworkDataTaskCocoa::statelessCookieStorage()
{
    static NeverDestroyed<RetainPtr<NSHTTPCookieStorage>> statelessCookieStorage;
    if (!statelessCookieStorage.get()) {
#if HAVE(NSHTTPCOOKIESTORAGE__INITWITHIDENTIFIER_WITH_INACCURATE_NULLABILITY)
        IGNORE_NULL_CHECK_WARNINGS_BEGIN
#endif
        statelessCookieStorage.get() = adoptNS([[NSHTTPCookieStorage alloc] _initWithIdentifier:nil private:YES]);
#if HAVE(NSHTTPCOOKIESTORAGE__INITWITHIDENTIFIER_WITH_INACCURATE_NULLABILITY)
        IGNORE_NULL_CHECK_WARNINGS_END
#endif
        statelessCookieStorage.get().get().cookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
    }
    ASSERT(statelessCookieStorage.get().get().cookies.count == 0);
    return statelessCookieStorage.get().get();
}

void NetworkDataTaskCocoa::applyCookieBlockingPolicy(bool shouldBlock)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (shouldBlock == m_hasBeenSetToUseStatelessCookieStorage)
        return;

    NSHTTPCookieStorage *storage = shouldBlock ? statelessCookieStorage() : m_session->networkStorageSession().nsCookieStorage();
    [m_task _setExplicitCookieStorage:storage._cookieStorage];
    m_hasBeenSetToUseStatelessCookieStorage = shouldBlock;
}
#endif

bool NetworkDataTaskCocoa::isThirdPartyRequest(const WebCore::ResourceRequest& request)
{
    return !WebCore::areRegistrableDomainsEqual(request.url(), request.firstPartyForCookies());
}

static void updateTaskWithFirstPartyForSameSiteCookies(NSURLSessionDataTask* task, const WebCore::ResourceRequest& request)
{
    if (request.isSameSiteUnspecified())
        return;
#if HAVE(FOUNDATION_WITH_SAME_SITE_COOKIE_SUPPORT)
    static NSURL *emptyURL = [[NSURL alloc] initWithString:@""];
    task._siteForCookies = request.isSameSite() ? task.currentRequest.URL : emptyURL;
    task._isTopLevelNavigation = request.isTopSite();
#else
    UNUSED_PARAM(task);
#endif
}

NetworkDataTaskCocoa::NetworkDataTaskCocoa(NetworkSession& session, NetworkDataTaskClient& client, const WebCore::ResourceRequest& requestWithCredentials, uint64_t frameID, uint64_t pageID, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, WebCore::ContentSniffingPolicy shouldContentSniff, WebCore::ContentEncodingSniffingPolicy shouldContentEncodingSniff, bool shouldClearReferrerOnHTTPSToHTTPRedirect, PreconnectOnly shouldPreconnectOnly, bool dataTaskIsForMainFrameNavigation, Optional<NetworkActivityTracker> networkActivityTracker)
    : NetworkDataTask(session, client, requestWithCredentials, storedCredentialsPolicy, shouldClearReferrerOnHTTPSToHTTPRedirect, dataTaskIsForMainFrameNavigation)
    , m_frameID(frameID)
    , m_pageID(pageID)
{
    if (m_scheduledFailureType != NoFailure)
        return;

    auto request = requestWithCredentials;
    auto url = request.url();
    if (storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use && url.protocolIsInHTTPFamily()) {
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

    bool shouldBlockCookies = false;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    shouldBlockCookies = session.networkStorageSession().shouldBlockCookies(request, frameID, pageID);
#endif
    restrictRequestReferrerToOriginIfNeeded(request, shouldBlockCookies);

    NSURLRequest *nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
    applySniffingPoliciesAndBindRequestToInferfaceIfNeeded(nsRequest, shouldContentSniff == WebCore::ContentSniffingPolicy::SniffContent && !url.isLocalFile(), shouldContentEncodingSniff == WebCore::ContentEncodingSniffingPolicy::Sniff);
#if ENABLE(PROXIMITY_NETWORKING)
    m_session->networkProcess().proximityManager().applyProperties(request, *this, nsRequest);
#endif

    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(m_session.get());
    if (storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use) {
        m_task = [cocoaSession.m_sessionWithCredentialStorage dataTaskWithRequest:nsRequest];
        ASSERT(!cocoaSession.m_dataTaskMapWithCredentials.contains([m_task taskIdentifier]));
        cocoaSession.m_dataTaskMapWithCredentials.add([m_task taskIdentifier], this);
        LOG(NetworkSession, "%llu Creating stateful NetworkDataTask with URL %s", [m_task taskIdentifier], nsRequest.URL.absoluteString.UTF8String);
    } else {
        m_task = [cocoaSession.m_statelessSession dataTaskWithRequest:nsRequest];
        ASSERT(!cocoaSession.m_dataTaskMapWithoutState.contains([m_task taskIdentifier]));
        cocoaSession.m_dataTaskMapWithoutState.add([m_task taskIdentifier], this);
        LOG(NetworkSession, "%llu Creating stateless NetworkDataTask with URL %s", [m_task taskIdentifier], nsRequest.URL.absoluteString.UTF8String);
    }

    if (shouldPreconnectOnly == PreconnectOnly::Yes) {
#if ENABLE(SERVER_PRECONNECT)
        m_task.get()._preconnect = true;
#else
        ASSERT_NOT_REACHED();
#endif
    }

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (shouldBlockCookies) {
#if !RELEASE_LOG_DISABLED
        if (m_session->shouldLogCookieInformation())
            RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkDataTaskCocoa::logCookieInformation: pageID = %llu, frameID = %llu, taskID = %lu: Blocking cookies for URL %s", this, pageID, frameID, (unsigned long)[m_task taskIdentifier], nsRequest.URL.absoluteString.UTF8String);
#else
        LOG(NetworkSession, "%llu Blocking cookies for URL %s", [m_task taskIdentifier], nsRequest.URL.absoluteString.UTF8String);
#endif
        applyCookieBlockingPolicy(shouldBlockCookies);
    }
#endif

    if (WebCore::ResourceRequest::resourcePrioritiesEnabled())
        m_task.get().priority = toNSURLSessionTaskPriority(request.priority());

    updateTaskWithFirstPartyForSameSiteCookies(m_task.get(), request);

#if HAVE(NW_ACTIVITY)
    if (networkActivityTracker)
        m_task.get()._nw_activity = networkActivityTracker.value().getPlatformObject();
#endif
}

NetworkDataTaskCocoa::~NetworkDataTaskCocoa()
{
    if (!m_task)
        return;

    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(m_session.get());
    if (m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use) {
        ASSERT(cocoaSession.m_dataTaskMapWithCredentials.get([m_task taskIdentifier]) == this);
        cocoaSession.m_dataTaskMapWithCredentials.remove([m_task taskIdentifier]);
    } else {
        ASSERT(cocoaSession.m_dataTaskMapWithoutState.get([m_task taskIdentifier]) == this);
        cocoaSession.m_dataTaskMapWithoutState.remove([m_task taskIdentifier]);
    }
}

void NetworkDataTaskCocoa::restrictRequestReferrerToOriginIfNeeded(WebCore::ResourceRequest& request, bool shouldBlockCookies)
{
    if (shouldBlockCookies || (m_session->sessionID().isEphemeral() && isThirdPartyRequest(request)))
        request.setExistingHTTPReferrerToOriginString();
}

void NetworkDataTaskCocoa::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
    if (m_client)
        m_client->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

void NetworkDataTaskCocoa::didReceiveChallenge(WebCore::AuthenticationChallenge&& challenge, ChallengeCompletionHandler&& completionHandler)
{
    if (tryPasswordBasedAuthentication(challenge, completionHandler))
        return;

    if (m_client)
        m_client->didReceiveChallenge(WTFMove(challenge), WTFMove(completionHandler));
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
    } else if (redirectResponse.httpStatusCode() == 303 && equalLettersIgnoringASCIICase(m_firstRequest.httpMethod(), "head")) // FIXME: (rdar://problem/13706454).
        request.setHTTPMethod("HEAD"_s);
    
    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (m_shouldClearReferrerOnHTTPSToHTTPRedirect && !request.url().protocolIs("https") && WTF::protocolIs(request.httpReferrer(), "https"))
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

    if (isTopLevelNavigation())
        request.setFirstPartyForCookies(request.url());

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    bool shouldBlockCookies = m_session->networkStorageSession().shouldBlockCookies(request, m_frameID, m_pageID);
#if !RELEASE_LOG_DISABLED
    if (m_session->shouldLogCookieInformation())
        RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkDataTaskCocoa::willPerformHTTPRedirection::logCookieInformation: pageID = %llu, frameID = %llu, taskID = %lu: %s cookies for redirect URL %s", this, m_pageID, m_frameID, (unsigned long)[m_task taskIdentifier], (shouldBlockCookies ? "Blocking" : "Not blocking"), request.url().string().utf8().data());
#else
    LOG(NetworkSession, "%llu %s cookies for redirect URL %s", [m_task taskIdentifier], (shouldBlockCookies ? "Blocking" : "Not blocking"), request.url().string().utf8().data());
#endif
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    // Always apply the policy since blocking may need to be turned on or off in a redirect.
    applyCookieBlockingPolicy(shouldBlockCookies);

    if (!shouldBlockCookies) {
#if !RELEASE_LOG_DISABLED
        if (m_session->shouldLogCookieInformation())
            RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkDataTaskCocoa::willPerformHTTPRedirection::logCookieInformation: pageID = %llu, frameID = %llu, taskID = %lu: Not partitioning cookies for redirect URL %s", this, m_pageID, m_frameID, (unsigned long)[m_task taskIdentifier], request.url().string().utf8().data());
#else
        LOG(NetworkSession, "%llu Not partitioning cookies for redirect URL %s", [m_task taskIdentifier], request.url().string().utf8().data());
#endif
    }
#endif

    updateTaskWithFirstPartyForSameSiteCookies(m_task.get(), request);

    if (m_client)
        m_client->willPerformHTTPRedirection(WTFMove(redirectResponse), WTFMove(request), [completionHandler = WTFMove(completionHandler), this, weakThis = makeWeakPtr(*this)] (auto&& request) mutable {
            if (!weakThis)
                return completionHandler({ });
            if (!request.isNull()) {
#if ENABLE(RESOURCE_LOAD_STATISTICS)
                bool shouldBlockCookies = m_session->networkStorageSession().shouldBlockCookies(request, m_frameID, m_pageID);
#else
                bool shouldBlockCookies = false;
#endif
                restrictRequestReferrerToOriginIfNeeded(request, shouldBlockCookies);
            }
            completionHandler(WTFMove(request));
        });
    else {
        ASSERT_NOT_REACHED();
        completionHandler({ });
    }
}

void NetworkDataTaskCocoa::setPendingDownloadLocation(const WTF::String& filename, SandboxExtension::Handle&& sandboxExtensionHandle, bool allowOverwrite)
{
    NetworkDataTask::setPendingDownloadLocation(filename, { }, allowOverwrite);

    ASSERT(!m_sandboxExtension);
    m_sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle));
    if (m_sandboxExtension)
        m_sandboxExtension->consume();

    m_task.get()._pathToDownloadTaskFile = m_pendingDownloadLocation;

    if (allowOverwrite && FileSystem::fileExists(m_pendingDownloadLocation))
        FileSystem::deleteFile(filename);
}

bool NetworkDataTaskCocoa::tryPasswordBasedAuthentication(const WebCore::AuthenticationChallenge& challenge, ChallengeCompletionHandler& completionHandler)
{
    if (!challenge.protectionSpace().isPasswordBased())
        return false;
    
    if (!m_user.isNull() && !m_password.isNull()) {
        auto persistence = m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use ? WebCore::CredentialPersistenceForSession : WebCore::CredentialPersistenceNone;
        completionHandler(AuthenticationChallengeDisposition::UseCredential, WebCore::Credential(m_user, m_password, persistence));
        m_user = String();
        m_password = String();
        return true;
    }

#if USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
    if (m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use) {
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
        m_failureTimer.startOneShot(0_s);
    [m_task resume];
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
    return WebCore::Credential([NSURLCredential credentialForTrust:challenge.nsURLAuthenticationChallenge().protectionSpace.serverTrust]);
}

bool NetworkDataTaskCocoa::isAlwaysOnLoggingAllowed() const
{
    if (m_session->networkProcess().sessionIsControlledByAutomation(m_session->sessionID()))
        return true;

    return m_session->sessionID().isAlwaysOnLoggingAllowed();
}

String NetworkDataTaskCocoa::description() const
{
    return String([m_task description]);
}

}
