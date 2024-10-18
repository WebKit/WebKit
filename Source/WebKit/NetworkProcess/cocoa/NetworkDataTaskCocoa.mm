/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#import "NetworkIssueReporter.h"
#import "NetworkProcess.h"
#import "NetworkSessionCocoa.h"
#import "WebCoreArgumentCoders.h"
#import "WebPrivacyHelpers.h"
#import <WebCore/AdvancedPrivacyProtections.h>
#import <WebCore/AuthenticationChallenge.h>
#import <WebCore/HTTPStatusCodes.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/OriginAccessPatterns.h>
#import <WebCore/RegistrableDomain.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/TimingAllowOrigin.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/FileSystem.h>
#import <wtf/MainThread.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/SystemTracing.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/text/Base64.h>

#if HAVE(NW_ACTIVITY)
#import <pal/spi/cocoa/NSURLConnectionSPI.h>
#endif

namespace WebKit {

#if HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)

inline static bool shouldBlockTrackersForThirdPartyCloaking(NSURLRequest *request)
{
    auto requestURL = request.URL;
    auto mainDocumentURL = request.mainDocumentURL;
    if (!requestURL || !mainDocumentURL)
        return false;

    if (!WebCore::areRegistrableDomainsEqual(requestURL, mainDocumentURL))
        return false;

    if ([requestURL.host isEqualToString:mainDocumentURL.host])
        return false;

    return true;
}

#endif // HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)

void enableAdvancedPrivacyProtections(NSMutableURLRequest *request, OptionSet<WebCore::AdvancedPrivacyProtections> policy)
{
#if HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)
    if (policy.contains(WebCore::AdvancedPrivacyProtections::EnhancedNetworkPrivacy))
        request._useEnhancedPrivacyMode = YES;

    if (policy.contains(WebCore::AdvancedPrivacyProtections::BaselineProtections) && shouldBlockTrackersForThirdPartyCloaking(request))
        request._blockTrackers = YES;
#else
    UNUSED_PARAM(request);
    UNUSED_PARAM(policy);
#endif
}

void setPCMDataCarriedOnRequest(WebCore::PrivateClickMeasurement::PcmDataCarried pcmDataCarried, NSMutableURLRequest *request)
{
#if ENABLE(TRACKER_DISPOSITION)
    if (request._needsNetworkTrackingPrevention || pcmDataCarried == WebCore::PrivateClickMeasurement::PcmDataCarried::PersonallyIdentifiable)
        return;

    request._needsNetworkTrackingPrevention = YES;
#else
    UNUSED_PARAM(pcmDataCarried);
    UNUSED_PARAM(request);
#endif
}

static void applyBasicAuthorizationHeader(WebCore::ResourceRequest& request, const WebCore::Credential& credential)
{
    request.setHTTPHeaderField(WebCore::HTTPHeaderName::Authorization, credential.serializationForBasicAuthorizationHeader());
}

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

void NetworkDataTaskCocoa::applySniffingPoliciesAndBindRequestToInferfaceIfNeeded(RetainPtr<NSURLRequest>& nsRequest, bool shouldContentSniff, WebCore::ContentEncodingSniffingPolicy contentEncodingSniffingPolicy)
{
#if !USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
    UNUSED_PARAM(contentEncodingSniffingPolicy);
#endif

    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(*networkSession());
    auto& boundInterfaceIdentifier = cocoaSession.boundInterfaceIdentifier();
    if (shouldContentSniff
#if USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
        && contentEncodingSniffingPolicy == WebCore::ContentEncodingSniffingPolicy::Default 
#endif
        && boundInterfaceIdentifier.isNull())
        return;

    auto mutableRequest = adoptNS([nsRequest mutableCopy]);

#if USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
    if (contentEncodingSniffingPolicy == WebCore::ContentEncodingSniffingPolicy::Disable)
        [mutableRequest _setProperty:@YES forKey:(NSString *)kCFURLRequestContentDecoderSkipURLCheck];
#endif

    if (!shouldContentSniff)
        [mutableRequest _setProperty:@NO forKey:(NSString *)_kCFURLConnectionPropertyShouldSniff];

    if (!boundInterfaceIdentifier.isNull())
        [mutableRequest setBoundInterfaceIdentifier:boundInterfaceIdentifier];

    nsRequest = WTFMove(mutableRequest);
}

void NetworkDataTaskCocoa::updateFirstPartyInfoForSession(const URL& requestURL)
{
    if (!shouldApplyCookiePolicyForThirdPartyCloaking() || requestURL.host().isEmpty())
        return;

    auto* session = networkSession();
    auto cnameDomain = [this]() {
        if (auto* lastResolvedCNAMEInChain = [[m_task _resolvedCNAMEChain] lastObject])
            return lastCNAMEDomain(lastResolvedCNAMEInChain);
        return WebCore::RegistrableDomain { };
    }();
    if (!cnameDomain.isEmpty())
        session->setFirstPartyHostCNAMEDomain(requestURL.host().toString(), WTFMove(cnameDomain));

    if (NSString *ipAddress = lastRemoteIPAddress(m_task.get()); ipAddress.length)
        session->setFirstPartyHostIPAddress(requestURL.host().toString(), ipAddress);
}

NetworkDataTaskCocoa::NetworkDataTaskCocoa(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters)
    : NetworkDataTask(session, client, parameters.request, parameters.storedCredentialsPolicy, parameters.shouldClearReferrerOnHTTPSToHTTPRedirect, parameters.isMainFrameNavigation)
    , NetworkTaskCocoa(session, parameters.shouldRelaxThirdPartyCookieBlocking)
    , m_sessionWrapper(static_cast<NetworkSessionCocoa&>(session).sessionWrapperForTask(parameters.webPageProxyID, parameters.request, parameters.storedCredentialsPolicy, parameters.isNavigatingToAppBoundDomain))
    , m_frameID(parameters.webFrameID)
    , m_pageID(parameters.webPageID)
    , m_webPageProxyID(parameters.webPageProxyID)
    , m_isForMainResourceNavigationForAnyFrame(!!parameters.mainResourceNavigationDataForAnyFrame)
    , m_sourceOrigin(parameters.sourceOrigin)
{
    auto request = parameters.request;
    auto url = request.url();
    if (m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use && url.protocolIsInHTTPFamily()) {
        m_user = url.user();
        m_password = url.password();
        request.removeCredentials();
        url = request.url();
    
        if (auto* storageSession = m_session->networkStorageSession()) {
            if (m_user.isEmpty() && m_password.isEmpty())
                m_initialCredential = storageSession->credentialStorage().get(m_partition, url);
            else
                storageSession->credentialStorage().set(m_partition, WebCore::Credential(m_user, m_password, WebCore::CredentialPersistence::None), url);
        }
    }

    if (!m_initialCredential.isEmpty() && !request.hasHTTPHeaderField(WebCore::HTTPHeaderName::Authorization)) {
        // FIXME: Support Digest authentication, and Proxy-Authorization.
        applyBasicAuthorizationHeader(request, m_initialCredential);
    }

    const auto shouldBlockCookies = [](WebCore::ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecision) {
        return thirdPartyCookieBlockingDecision == WebCore::ThirdPartyCookieBlockingDecision::All;
    };

    auto thirdPartyCookieBlockingDecision = m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::EphemeralStateless ? WebCore::ThirdPartyCookieBlockingDecision::All : WebCore::ThirdPartyCookieBlockingDecision::None;
    if (auto* networkStorageSession = session.networkStorageSession()) {
        if (!shouldBlockCookies(thirdPartyCookieBlockingDecision))
            thirdPartyCookieBlockingDecision = networkStorageSession->thirdPartyCookieBlockingDecisionForRequest(request, frameID(), pageID(), shouldRelaxThirdPartyCookieBlocking());
    }
    restrictRequestReferrerToOriginIfNeeded(request);

    RetainPtr<NSURLRequest> nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
    ASSERT(nsRequest);
    RetainPtr<NSMutableURLRequest> mutableRequest = adoptNS([nsRequest.get() mutableCopy]);

    if (parameters.isMainFrameNavigation
        || parameters.hadMainFrameMainResourcePrivateRelayed
        || request.url().host() == request.firstPartyForCookies().host()) {
        if ([mutableRequest respondsToSelector:@selector(_setPrivacyProxyFailClosedForUnreachableNonMainHosts:)])
            [mutableRequest _setPrivacyProxyFailClosedForUnreachableNonMainHosts:YES];
    }

#if HAVE(PROHIBIT_PRIVACY_PROXY)
    if (!parameters.allowPrivacyProxy)
        [mutableRequest _setProhibitPrivacyProxy:YES];
#endif

    auto advancedPrivacyProtections = parameters.advancedPrivacyProtections;
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    if (advancedPrivacyProtections.contains(WebCore::AdvancedPrivacyProtections::BaselineProtections) && parameters.isMainFrameNavigation)
        configureForAdvancedPrivacyProtections(m_sessionWrapper->session.get());

    enableAdvancedPrivacyProtections(mutableRequest.get(), advancedPrivacyProtections);
#endif

#if HAVE(PRIVACY_PROXY_FAIL_CLOSED_FOR_UNREACHABLE_HOSTS)
    if ([mutableRequest respondsToSelector:@selector(_setPrivacyProxyFailClosedForUnreachableHosts:)] && advancedPrivacyProtections.contains(WebCore::AdvancedPrivacyProtections::FailClosed))
        [mutableRequest _setPrivacyProxyFailClosedForUnreachableHosts:YES];
#endif

    if ([mutableRequest respondsToSelector:@selector(_setWebSearchContent:)] && advancedPrivacyProtections.contains(WebCore::AdvancedPrivacyProtections::WebSearchContent))
        [mutableRequest _setWebSearchContent:YES];

#if HAVE(ALLOW_PRIVATE_ACCESS_TOKENS_FOR_THIRD_PARTY)
    if ([mutableRequest respondsToSelector:@selector(_setAllowPrivateAccessTokensForThirdParty:)] && parameters.request.isPrivateTokenUsageByThirdPartyAllowed())
        [mutableRequest _setAllowPrivateAccessTokensForThirdParty:YES];
#endif

#if ENABLE(APP_PRIVACY_REPORT)
    mutableRequest.get().attribution = request.isAppInitiated() ? NSURLRequestAttributionDeveloper : NSURLRequestAttributionUser;
#endif

    // FIXME: Remove hadMainFrameMainResourcePrivateRelayed, PrivateRelayed, and all the associated piping.
    
    nsRequest = mutableRequest;

#if ENABLE(APP_PRIVACY_REPORT)
    m_session->appPrivacyReportTestingData().didLoadAppInitiatedRequest(nsRequest.get().attribution == NSURLRequestAttributionDeveloper);
#endif

    applySniffingPoliciesAndBindRequestToInferfaceIfNeeded(nsRequest, parameters.contentSniffingPolicy == WebCore::ContentSniffingPolicy::SniffContent && !url.protocolIsFile(), parameters.contentEncodingSniffingPolicy);

    if (url.protocolIs("ws"_s) || url.protocolIs("wss"_s)) {
        // FIXME: Remove this once configuration._usesNWLoader is always effectively YES.
        // It will be no longer needed, as verified by the WebSocket.LoadRequestWSS API test.
        scheduleFailure(FailureType::RestrictedURL);
        return;
    }

    m_task = [m_sessionWrapper->session dataTaskWithRequest:nsRequest.get()];

#if HAVE(CFNETWORK_HOSTOVERRIDE)
    if (session.networkProcess().localhostAliasesForTesting().contains<StringViewHashTranslator>(url.host()))
        m_task.get()._hostOverride = adoptNS(nw_endpoint_create_host_with_numeric_port("localhost", url.port().value_or(0))).get();
#endif

    WTFBeginSignpost(m_task.get(), DataTask, "%" PUBLIC_LOG_STRING " %" PRIVATE_LOG_STRING " pri: %.2f preconnect: %d", request.httpMethod().utf8().data(), url.string().utf8().data(), toNSURLSessionTaskPriority(request.priority()), parameters.shouldPreconnectOnly == PreconnectOnly::Yes);

    switch (parameters.storedCredentialsPolicy) {
    case WebCore::StoredCredentialsPolicy::Use:
        ASSERT(m_sessionWrapper->session.get().configuration.URLCredentialStorage);
        break;
    case WebCore::StoredCredentialsPolicy::EphemeralStateless:
        ASSERT(!m_sessionWrapper->session.get().configuration.URLCredentialStorage);
        break;
    case WebCore::StoredCredentialsPolicy::DoNotUse:
        RetainPtr<NSURLSessionConfiguration> effectiveConfiguration = m_sessionWrapper->session.get().configuration;
        effectiveConfiguration.get().URLCredentialStorage = nil;
        [m_task _adoptEffectiveConfiguration:effectiveConfiguration.get()];
        break;
    };

    RELEASE_ASSERT(!m_sessionWrapper->dataTaskMap.contains([m_task taskIdentifier]));
    m_sessionWrapper->dataTaskMap.add([m_task taskIdentifier], this);
    LOG(NetworkSession, "%lu Creating NetworkDataTask with URL %s", (unsigned long)[m_task taskIdentifier], [nsRequest URL].absoluteString.UTF8String);

    if (parameters.shouldPreconnectOnly == PreconnectOnly::Yes) {
#if ENABLE(SERVER_PRECONNECT)
        m_task.get()._preconnect = true;
#else
        ASSERT_NOT_REACHED();
#endif
    }

    if (!isTopLevelNavigation())
        applyCookiePolicyForThirdPartyCloaking(request);
    if (shouldBlockCookies(thirdPartyCookieBlockingDecision)) {
#if !RELEASE_LOG_DISABLED
        if (m_session->shouldLogCookieInformation())
            RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkDataTaskCocoa::logCookieInformation: pageID=%" PRIu64 ", frameID=%" PRIu64 ", taskID=%lu: Blocking cookies for URL %s", this, pageID() ? pageID()->toUInt64() : 0, frameID() ? frameID()->object().toUInt64() : 0, (unsigned long)[m_task taskIdentifier], [nsRequest URL].absoluteString.UTF8String);
#else
        LOG(NetworkSession, "%lu Blocking cookies for URL %s", (unsigned long)[m_task taskIdentifier], [nsRequest URL].absoluteString.UTF8String);
#endif
        blockCookies();
    }

    if (WebCore::ResourceRequest::resourcePrioritiesEnabled())
        m_task.get().priority = toNSURLSessionTaskPriority(request.priority());

    updateTaskWithFirstPartyForSameSiteCookies(m_task.get(), request);

#if HAVE(NW_ACTIVITY)
    if (parameters.networkActivityTracker)
        m_task.get()._nw_activity = parameters.networkActivityTracker->getPlatformObject();
#endif
}

NetworkDataTaskCocoa::~NetworkDataTaskCocoa()
{
    if (m_task)
        WTFEndSignpost(m_task.get(), DataTask);

    if (m_task && m_sessionWrapper) {
        auto& map = m_sessionWrapper->dataTaskMap;
        auto iterator = map.find([m_task taskIdentifier]);
        RELEASE_ASSERT(iterator != map.end());
        ASSERT(!iterator->value.get());
        map.remove(iterator);
    }
}

void NetworkDataTaskCocoa::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
    WTFEmitSignpost(m_task.get(), DataTask, "sent %llu bytes (expected %llu bytes)", totalBytesSent, totalBytesExpectedToSend);

    if (m_client)
        m_client->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

void NetworkDataTaskCocoa::didReceiveChallenge(WebCore::AuthenticationChallenge&& challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, ChallengeCompletionHandler&& completionHandler)
{
    WTFEmitSignpost(m_task.get(), DataTask, "received challenge");

    if (tryPasswordBasedAuthentication(challenge, completionHandler))
        return;

    if (m_client)
        m_client->didReceiveChallenge(WTFMove(challenge), negotiatedLegacyTLS, WTFMove(completionHandler));
    else {
        ASSERT_NOT_REACHED();
        completionHandler(AuthenticationChallengeDisposition::PerformDefaultHandling, { });
    }
}

void NetworkDataTaskCocoa::didNegotiateModernTLS(const URL& url)
{
    if (m_client)
        m_client->didNegotiateModernTLS(url);
}

void NetworkDataTaskCocoa::didCompleteWithError(const WebCore::ResourceError& error, const WebCore::NetworkLoadMetrics& networkLoadMetrics)
{
    WTFEmitSignpost(m_task.get(), DataTask, "completed with error: %d", !error.isNull());

    if (m_client)
        m_client->didCompleteWithError(error, networkLoadMetrics);
}

void NetworkDataTaskCocoa::didReceiveData(const WebCore::SharedBuffer& data)
{
    WTFEmitSignpost(m_task.get(), DataTask, "received %zd bytes", data.size());

    if (m_client)
        m_client->didReceiveData(data);
}

void NetworkDataTaskCocoa::didReceiveResponse(WebCore::ResourceResponse&& response, NegotiatedLegacyTLS negotiatedLegacyTLS, PrivateRelayed privateRelayed, WebKit::ResponseCompletionHandler&& completionHandler)
{
    WTFEmitSignpost(m_task.get(), DataTask, "received response headers");
    if (isTopLevelNavigation())
        updateFirstPartyInfoForSession(response.url());
#if ENABLE(NETWORK_ISSUE_REPORTING)
    else if (NetworkIssueReporter::shouldReport([m_task _incompleteTaskMetrics])) {
        if (auto session = networkSession())
            session->reportNetworkIssue(*m_webPageProxyID, firstRequest().url());
    }
#endif
    NetworkDataTask::didReceiveResponse(WTFMove(response), negotiatedLegacyTLS, privateRelayed, WebCore::IPAddress::fromString(lastRemoteIPAddress(m_task.get())), WTFMove(completionHandler));
}

void NetworkDataTaskCocoa::willPerformHTTPRedirection(WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    WTFEmitSignpost(m_task.get(), DataTask, "redirect");

    networkLoadMetrics().hasCrossOriginRedirect = networkLoadMetrics().hasCrossOriginRedirect || !WebCore::SecurityOrigin::create(request.url())->canRequest(redirectResponse.url(), WebCore::EmptyOriginAccessPatterns::singleton());

    const auto& previousRequest = m_previousRequest.isNull() ? m_firstRequest : m_previousRequest;
    if (redirectResponse.httpStatusCode() == httpStatus307TemporaryRedirect || redirectResponse.httpStatusCode() == httpStatus308PermanentRedirect) {
        ASSERT(m_lastHTTPMethod == request.httpMethod());
        auto body = previousRequest.httpBody();
        if (body && !body->isEmpty() && !equalLettersIgnoringASCIICase(m_lastHTTPMethod, "get"_s))
            request.setHTTPBody(WTFMove(body));
        
        String originalContentType = previousRequest.httpContentType();
        if (!originalContentType.isEmpty())
            request.setHTTPHeaderField(WebCore::HTTPHeaderName::ContentType, originalContentType);
    } else if (redirectResponse.httpStatusCode() == httpStatus303SeeOther) { // FIXME: (rdar://problem/13706454).
        if (equalLettersIgnoringASCIICase(previousRequest.httpMethod(), "head"_s))
            request.setHTTPMethod("HEAD"_s);

        String originalContentType = previousRequest.httpContentType();
        if (!originalContentType.isEmpty())
            request.setHTTPHeaderField(WebCore::HTTPHeaderName::ContentType, originalContentType);
    }
    
    // Should not set Referer after a redirect from a secure resource to non-secure one.
    if (m_shouldClearReferrerOnHTTPSToHTTPRedirect && !request.url().protocolIs("https"_s) && WTF::protocolIs(request.httpReferrer(), "https"_s))
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
    }

    if (isTopLevelNavigation())
        request.setFirstPartyForCookies(request.url());
    else {
        WebCore::RegistrableDomain firstPartyDomain { request.firstPartyForCookies() };
        if (auto* storageSession = m_session->networkStorageSession()) {
            bool didPreviousRequestHaveStorageAccess = storageSession->hasStorageAccess(WebCore::RegistrableDomain { redirectResponse.url() }, firstPartyDomain, m_frameID, m_pageID);
            bool doesRequestHaveStorageAccess = storageSession->hasStorageAccess(WebCore::RegistrableDomain { request.url() }, firstPartyDomain, m_frameID, m_pageID);
            if (didPreviousRequestHaveStorageAccess && doesRequestHaveStorageAccess)
                request.setFirstPartyForCookies(request.url());
        }
    }

    NetworkTaskCocoa::willPerformHTTPRedirection(WTFMove(redirectResponse), WTFMove(request), [completionHandler = WTFMove(completionHandler), this, weakThis = ThreadSafeWeakPtr { *this }, redirectResponse] (WebCore::ResourceRequest&& request) mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler({ });
        if (!m_client)
            return completionHandler({ });
        m_client->willPerformHTTPRedirection(WTFMove(redirectResponse), WTFMove(request), [completionHandler = WTFMove(completionHandler), this, weakThis] (WebCore::ResourceRequest&& request) mutable {
            auto protectedThis = weakThis.get();
            if (!protectedThis || !m_session)
                return completionHandler({ });
            if (!request.isNull())
                restrictRequestReferrerToOriginIfNeeded(request);
            m_previousRequest = request;
            completionHandler(WTFMove(request));
        });
    });
}

void NetworkDataTaskCocoa::setPendingDownloadLocation(const WTF::String& filename, SandboxExtension::Handle&& sandboxExtensionHandle, bool allowOverwrite)
{
    NetworkDataTask::setPendingDownloadLocation(filename, { }, allowOverwrite);

    ASSERT(!m_sandboxExtension);
    m_sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle));
    if (RefPtr extention = m_sandboxExtension)
        extention->consume();

    m_task.get()._pathToDownloadTaskFile = m_pendingDownloadLocation;

    if (allowOverwrite && FileSystem::fileExists(m_pendingDownloadLocation))
        FileSystem::deleteFile(filename);
}

bool NetworkDataTaskCocoa::tryPasswordBasedAuthentication(const WebCore::AuthenticationChallenge& challenge, ChallengeCompletionHandler& completionHandler)
{
    if (!challenge.protectionSpace().isPasswordBased())
        return false;
    
    if (!m_user.isEmpty() || !m_password.isEmpty()) {
        auto persistence = m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use ? WebCore::CredentialPersistence::ForSession : WebCore::CredentialPersistence::None;
        completionHandler(AuthenticationChallengeDisposition::UseCredential, WebCore::Credential(m_user, m_password, persistence));
        m_user = String();
        m_password = String();
        return true;
    }

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
                ASSERT(credential.persistence() == WebCore::CredentialPersistence::None);
                if (challenge.failureResponse().httpStatusCode() == httpStatus401Unauthorized) {
                    // Store the credential back, possibly adding it as a default for this directory.
                    if (auto* storageSession = m_session->networkStorageSession())
                        storageSession->credentialStorage().set(m_partition, credential, challenge.protectionSpace(), challenge.failureResponse().url());
                }
                completionHandler(AuthenticationChallengeDisposition::UseCredential, credential);
                return true;
            }
        }
    }

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
    WTFEmitSignpost(m_task.get(), DataTask, "cancel");
    [m_task cancel];
}

void NetworkDataTaskCocoa::resume()
{
    WTFEmitSignpost(m_task.get(), DataTask, "resume");

    if (m_failureScheduled)
        return;

    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(*m_session);
    if (cocoaSession.deviceManagementRestrictionsEnabled() && m_isForMainResourceNavigationForAnyFrame) {
        auto didDetermineDeviceRestrictionPolicyForURL = makeBlockPtr([this, protectedThis = Ref { *this }](BOOL isBlocked) mutable {
            callOnMainRunLoop([this, protectedThis = WTFMove(protectedThis), isBlocked] {
                if (isBlocked) {
                    scheduleFailure(FailureType::RestrictedURL);
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

void NetworkDataTaskCocoa::setPriority(WebCore::ResourceLoadPriority priority)
{
    if (!WebCore::ResourceRequest::resourcePrioritiesEnabled())
        return;
    m_task.get().priority = toNSURLSessionTaskPriority(priority);
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

void NetworkDataTaskCocoa::setEmulatedConditions(const std::optional<int64_t>& bytesPerSecondLimit)
{
    m_task.get()._bytesPerSecondLimit = bytesPerSecondLimit.value_or(0);
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

void NetworkDataTaskCocoa::setTimingAllowFailedFlag()
{
    networkLoadMetrics().failsTAOCheck = true;
}

NSURLSessionTask* NetworkDataTaskCocoa::task() const
{
    return m_task.get();
}

}
