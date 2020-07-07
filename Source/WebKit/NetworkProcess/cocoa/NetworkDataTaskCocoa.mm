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
#import "DeviceManagementSPI.h"
#import "Download.h"
#import "DownloadProxyMessages.h"
#import "Logging.h"
#import "NetworkProcess.h"
#import "NetworkSessionCocoa.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/AuthenticationChallenge.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/ResourceRequest.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/FileSystem.h>
#import <wtf/MainThread.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SystemTracing.h>
#import <wtf/text/Base64.h>

#if HAVE(NW_ACTIVITY)
#import <pal/spi/cocoa/NSURLConnectionSPI.h>
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

    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(*m_session);
    auto& boundInterfaceIdentifier = cocoaSession.boundInterfaceIdentifier();
    if (shouldContentSniff
#if USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
        && shouldContentEncodingSniff
#endif
        && boundInterfaceIdentifier.isNull())
        return;

    auto mutableRequest = adoptNS([nsRequest mutableCopy]);

#if USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
    if (!shouldContentEncodingSniff)
        [mutableRequest _setProperty:@YES forKey:(NSString *)kCFURLRequestContentDecoderSkipURLCheck];
#endif

    if (!shouldContentSniff)
        [mutableRequest _setProperty:@NO forKey:(NSString *)_kCFURLConnectionPropertyShouldSniff];

    if (!boundInterfaceIdentifier.isNull())
        [mutableRequest setBoundInterfaceIdentifier:boundInterfaceIdentifier];

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

void NetworkDataTaskCocoa::blockCookies()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (m_hasBeenSetToUseStatelessCookieStorage)
        return;

    [m_task _setExplicitCookieStorage:statelessCookieStorage()._cookieStorage];
    m_hasBeenSetToUseStatelessCookieStorage = true;
}

void NetworkDataTaskCocoa::unblockCookies()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (!m_hasBeenSetToUseStatelessCookieStorage)
        return;

    if (auto* storageSession = m_session->networkStorageSession()) {
        [m_task _setExplicitCookieStorage:storageSession->nsCookieStorage()._cookieStorage];
        m_hasBeenSetToUseStatelessCookieStorage = false;
    }
}

// FIXME: Temporary fix for <rdar://problem/60089022> until content can be updated.
bool NetworkDataTaskCocoa::needsFirstPartyCookieBlockingLatchModeQuirk(const URL& firstPartyURL, const URL& requestURL, const URL& redirectingURL) const
{
    using RegistrableDomain = WebCore::RegistrableDomain;
    static NeverDestroyed<HashMap<RegistrableDomain, RegistrableDomain>> quirkPairs = [] {
        HashMap<RegistrableDomain, RegistrableDomain> map;
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("ymail.com"_s), RegistrableDomain::uncheckedCreateFromRegistrableDomainString("yahoo.com"_s));
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("aolmail.com"_s), RegistrableDomain::uncheckedCreateFromRegistrableDomainString("aol.com"_s));
        return map;
    }();

    RegistrableDomain firstPartyDomain { firstPartyURL };
    RegistrableDomain requestDomain { requestURL };
    if (firstPartyDomain != requestDomain)
        return false;

    RegistrableDomain redirectingDomain { redirectingURL };
    auto quirk = quirkPairs.get().find(redirectingDomain);
    if (quirk == quirkPairs.get().end())
        return false;

    return quirk->value == requestDomain;
}
#endif

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

static inline bool computeIsAlwaysOnLoggingAllowed(NetworkSession& session)
{
    if (session.networkProcess().sessionIsControlledByAutomation(session.sessionID()))
        return true;

    return session.sessionID().isAlwaysOnLoggingAllowed();
}

NetworkDataTaskCocoa::NetworkDataTaskCocoa(NetworkSession& session, NetworkDataTaskClient& client, const WebCore::ResourceRequest& requestWithCredentials, WebCore::FrameIdentifier frameID, WebCore::PageIdentifier pageID, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, WebCore::ContentSniffingPolicy shouldContentSniff, WebCore::ContentEncodingSniffingPolicy shouldContentEncodingSniff, bool shouldClearReferrerOnHTTPSToHTTPRedirect, PreconnectOnly shouldPreconnectOnly, bool dataTaskIsForMainFrameNavigation, bool dataTaskIsForMainResourceNavigationForAnyFrame, Optional<NetworkActivityTracker> networkActivityTracker, Optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, WebCore::ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking)
    : NetworkDataTask(session, client, requestWithCredentials, storedCredentialsPolicy, shouldClearReferrerOnHTTPSToHTTPRedirect, dataTaskIsForMainFrameNavigation)
    , m_sessionWrapper(makeWeakPtr(static_cast<NetworkSessionCocoa&>(session).sessionWrapperForTask(requestWithCredentials, storedCredentialsPolicy, isNavigatingToAppBoundDomain)))
    , m_frameID(frameID)
    , m_pageID(pageID)
    , m_isForMainResourceNavigationForAnyFrame(dataTaskIsForMainResourceNavigationForAnyFrame)
    , m_isAlwaysOnLoggingAllowed(computeIsAlwaysOnLoggingAllowed(session))
    , m_shouldRelaxThirdPartyCookieBlocking(shouldRelaxThirdPartyCookieBlocking)
{
    auto request = requestWithCredentials;
    auto url = request.url();
    if (storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use && url.protocolIsInHTTPFamily()) {
        m_user = url.user();
        m_password = url.password();
        request.removeCredentials();
        url = request.url();
    
#if USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
        if (auto* storageSession = m_session->networkStorageSession()) {
            if (m_user.isEmpty() && m_password.isEmpty())
                m_initialCredential = storageSession->credentialStorage().get(m_partition, url);
            else
                storageSession->credentialStorage().set(m_partition, WebCore::Credential(m_user, m_password, WebCore::CredentialPersistenceNone), url);
        }
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
    shouldBlockCookies = storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::EphemeralStateless;
    if (auto* networkStorageSession = session.networkStorageSession()) {
        if (!shouldBlockCookies)
            shouldBlockCookies = networkStorageSession->shouldBlockCookies(request, frameID, pageID, m_shouldRelaxThirdPartyCookieBlocking);
    }
#endif
    restrictRequestReferrerToOriginIfNeeded(request);

    NSURLRequest *nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
    applySniffingPoliciesAndBindRequestToInferfaceIfNeeded(nsRequest, shouldContentSniff == WebCore::ContentSniffingPolicy::SniffContent && !url.isLocalFile(), shouldContentEncodingSniff == WebCore::ContentEncodingSniffingPolicy::Sniff);

    m_task = [m_sessionWrapper->session dataTaskWithRequest:nsRequest];

    WTFBeginSignpost(m_task.get(), "DataTask", "%{public}s pri: %.2f preconnect: %d", url.string().ascii().data(), toNSURLSessionTaskPriority(request.priority()), shouldPreconnectOnly == PreconnectOnly::Yes);

    RELEASE_ASSERT(!m_sessionWrapper->dataTaskMap.contains([m_task taskIdentifier]));
    m_sessionWrapper->dataTaskMap.add([m_task taskIdentifier], this);
    LOG(NetworkSession, "%llu Creating NetworkDataTask with URL %s", [m_task taskIdentifier], nsRequest.URL.absoluteString.UTF8String);

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
            RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkDataTaskCocoa::logCookieInformation: pageID = %llu, frameID = %llu, taskID = %lu: Blocking cookies for URL %s", this, pageID.toUInt64(), frameID.toUInt64(), (unsigned long)[m_task taskIdentifier], nsRequest.URL.absoluteString.UTF8String);
#else
        LOG(NetworkSession, "%llu Blocking cookies for URL %s", [m_task taskIdentifier], nsRequest.URL.absoluteString.UTF8String);
#endif
        blockCookies();
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
    if (!m_task || !m_sessionWrapper)
        return;

    RELEASE_ASSERT(m_sessionWrapper->dataTaskMap.get([m_task taskIdentifier]) == this);
    m_sessionWrapper->dataTaskMap.remove([m_task taskIdentifier]);
}

void NetworkDataTaskCocoa::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
    WTFEmitSignpost(m_task.get(), "DataTask", "sent %llu bytes (expected %llu bytes)", totalBytesSent, totalBytesExpectedToSend);

    if (m_client)
        m_client->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

void NetworkDataTaskCocoa::didReceiveChallenge(WebCore::AuthenticationChallenge&& challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, ChallengeCompletionHandler&& completionHandler)
{
    WTFEmitSignpost(m_task.get(), "DataTask", "received challenge");

    if (tryPasswordBasedAuthentication(challenge, completionHandler))
        return;

    if (m_client)
        m_client->didReceiveChallenge(WTFMove(challenge), negotiatedLegacyTLS, WTFMove(completionHandler));
    else {
        ASSERT_NOT_REACHED();
        completionHandler(AuthenticationChallengeDisposition::PerformDefaultHandling, { });
    }
}

void NetworkDataTaskCocoa::didNegotiateModernTLS(const WebCore::AuthenticationChallenge& challenge)
{
    if (m_client)
        m_client->didNegotiateModernTLS(challenge);
}

void NetworkDataTaskCocoa::didCompleteWithError(const WebCore::ResourceError& error, const WebCore::NetworkLoadMetrics& networkLoadMetrics)
{
    if (error.isNull())
        WTFEndSignpost(m_task.get(), "DataTask", "completed");
    else
        WTFEndSignpost(m_task.get(), "DataTask", "failed");

    if (m_client)
        m_client->didCompleteWithError(error, networkLoadMetrics);
}

void NetworkDataTaskCocoa::didReceiveData(Ref<WebCore::SharedBuffer>&& data)
{
    WTFEmitSignpost(m_task.get(), "DataTask", "received %zd bytes", data->size());

    if (m_client)
        m_client->didReceiveData(WTFMove(data));
}

void NetworkDataTaskCocoa::didReceiveResponse(WebCore::ResourceResponse&& response, NegotiatedLegacyTLS negotiatedLegacyTLS, WebKit::ResponseCompletionHandler&& completionHandler)
{
    WTFEmitSignpost(m_task.get(), "DataTask", "received response headers");
    NetworkDataTask::didReceiveResponse(WTFMove(response), negotiatedLegacyTLS, WTFMove(completionHandler));
}

void NetworkDataTaskCocoa::willPerformHTTPRedirection(WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    WTFEmitSignpost(m_task.get(), "DataTask", "redirect");

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
    m_password = url.password();
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
            auto credential = m_session->networkStorageSession() ? m_session->networkStorageSession()->credentialStorage().get(m_partition, request.url()) : WebCore::Credential();
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
    if (!m_hasBeenSetToUseStatelessCookieStorage) {
        if (m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::EphemeralStateless
            || (m_session->networkStorageSession() && m_session->networkStorageSession()->shouldBlockCookies(request, m_frameID, m_pageID, m_shouldRelaxThirdPartyCookieBlocking)))
            blockCookies();
    } else if (m_storedCredentialsPolicy != WebCore::StoredCredentialsPolicy::EphemeralStateless && needsFirstPartyCookieBlockingLatchModeQuirk(request.firstPartyForCookies(), request.url(), redirectResponse.url()))
        unblockCookies();
#if !RELEASE_LOG_DISABLED
    if (m_session->shouldLogCookieInformation())
        RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkDataTaskCocoa::willPerformHTTPRedirection::logCookieInformation: pageID = %llu, frameID = %llu, taskID = %lu: %s cookies for redirect URL %s", this, m_pageID.toUInt64(), m_frameID.toUInt64(), (unsigned long)[m_task taskIdentifier], (m_hasBeenSetToUseStatelessCookieStorage ? "Blocking" : "Not blocking"), request.url().string().utf8().data());
#else
    LOG(NetworkSession, "%llu %s cookies for redirect URL %s", [m_task taskIdentifier], (m_hasBeenSetToUseStatelessCookieStorage ? "Blocking" : "Not blocking"), request.url().string().utf8().data());
#endif
#endif

    updateTaskWithFirstPartyForSameSiteCookies(m_task.get(), request);

    if (m_client)
        m_client->willPerformHTTPRedirection(WTFMove(redirectResponse), WTFMove(request), [completionHandler = WTFMove(completionHandler), this, weakThis = makeWeakPtr(*this)] (auto&& request) mutable {
            if (!weakThis || !m_session)
                return completionHandler({ });
            if (!request.isNull())
                restrictRequestReferrerToOriginIfNeeded(request);
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
            if (auto* storageSession = m_session->networkStorageSession())
                storageSession->credentialStorage().remove(m_partition, challenge.protectionSpace());
        }

        if (!challenge.previousFailureCount()) {
            auto credential = m_session->networkStorageSession() ? m_session->networkStorageSession()->credentialStorage().get(m_partition, challenge.protectionSpace()) : WebCore::Credential();
            if (!credential.isEmpty() && credential != m_initialCredential) {
                ASSERT(credential.persistence() == WebCore::CredentialPersistenceNone);
                if (challenge.failureResponse().httpStatusCode() == 401) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    if (auto* storageSession = m_session->networkStorageSession())
                        storageSession->credentialStorage().set(m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
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
    WTFEmitSignpost(m_task.get(), "DataTask", "cancel");
    [m_task cancel];
}

void NetworkDataTaskCocoa::resume()
{
    WTFEmitSignpost(m_task.get(), "DataTask", "resume");

    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(*m_session);
    if (cocoaSession.deviceManagementRestrictionsEnabled() && m_isForMainResourceNavigationForAnyFrame) {
        auto didDetermineDeviceRestrictionPolicyForURL = makeBlockPtr([this, protectedThis = makeRef(*this)](BOOL isBlocked) mutable {
            callOnMainThread([this, protectedThis = WTFMove(protectedThis), isBlocked] {
                if (isBlocked) {
                    scheduleFailure(RestrictedURLFailure);
                    return;
                }

                [m_task resume];
            });
        });

#if HAVE(DEVICE_MANAGEMENT)
        if (cocoaSession.allLoadsBlockedByDeviceManagementRestrictionsForTesting())
            didDetermineDeviceRestrictionPolicyForURL(true);
        else {
            RetainPtr<NSURL> urlToCheck = [m_task currentRequest].URL;
            [cocoaSession.deviceManagementPolicyMonitor() requestPoliciesForWebsites:@[ urlToCheck.get() ] completionHandler:makeBlockPtr([didDetermineDeviceRestrictionPolicyForURL, urlToCheck] (NSDictionary<NSURL *, NSNumber *> *policies, NSError *error) {
                bool isBlocked = error || policies[urlToCheck.get()].integerValue != DMFPolicyOK;
                didDetermineDeviceRestrictionPolicyForURL(isBlocked);
            }).get()];
        }
#else
        didDetermineDeviceRestrictionPolicyForURL(cocoaSession.allLoadsBlockedByDeviceManagementRestrictionsForTesting());
#endif
        return;
    }

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
    return m_isAlwaysOnLoggingAllowed;
}

String NetworkDataTaskCocoa::description() const
{
    return String([m_task description]);
}

void NetworkDataTaskCocoa::setH2PingCallback(const URL& url, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
#if HAVE(PRECONNECT_PING)
    ASSERT(m_task.get()._preconnect);
    auto handler = CompletionHandlerWithFinalizer<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>(WTFMove(completionHandler), [url] (Function<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>& completionHandler) {
        completionHandler(makeUnexpected(WebCore::internalError(url)));
    });
    [m_task getUnderlyingHTTPConnectionInfoWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(handler), url] (_NSHTTPConnectionInfo *connectionInfo) mutable {
        if (!connectionInfo.isValid)
            return completionHandler(makeUnexpected(WebCore::internalError(url)));
        [connectionInfo sendPingWithReceiveHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](NSError *error, NSTimeInterval interval) mutable {
            completionHandler(Seconds(interval));
        }).get()];
    }).get()];
#else
    ASSERT_NOT_REACHED();
    return completionHandler(makeUnexpected(WebCore::internalError(url)));
#endif
}

}
