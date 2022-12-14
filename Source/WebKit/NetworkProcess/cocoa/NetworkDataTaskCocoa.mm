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
#import <WebCore/NetworkConnectionIntegrity.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/RegistrableDomain.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/TimingAllowOrigin.h>
#import <pal/spi/cf/CFNetworkSPI.h>
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

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/NetworkDataTaskCocoaAdditions.h>
#else
namespace WebKit {
void enableNetworkConnectionIntegrity(NSMutableURLRequest *, bool) { }
}
#endif

namespace WebKit {

static NSString *lastRemoteIPAddress(NSURLSessionDataTask *task)
{
    // FIXME (246428): In a future patch, this should adopt CFNetwork API that retrieves the original
    // IP address of the proxied response, rather than the proxy itself.
    return task._incompleteTaskMetrics.transactionMetrics.lastObject.remoteAddress;
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

#if USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
static void applyBasicAuthorizationHeader(WebCore::ResourceRequest& request, const WebCore::Credential& credential)
{
    request.setHTTPHeaderField(WebCore::HTTPHeaderName::Authorization, credential.serializationForBasicAuthorizationHeader());
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

void NetworkDataTaskCocoa::applySniffingPoliciesAndBindRequestToInferfaceIfNeeded(RetainPtr<NSURLRequest>& nsRequest, bool shouldContentSniff, WebCore::ContentEncodingSniffingPolicy contentEncodingSniffingPolicy)
{
#if !USE(CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE)
    UNUSED_PARAM(contentEncodingSniffingPolicy);
#endif

    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(*m_session);
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

#if ENABLE(TRACKING_PREVENTION)
NSHTTPCookieStorage *NetworkDataTaskCocoa::statelessCookieStorage()
{
    static NeverDestroyed<RetainPtr<NSHTTPCookieStorage>> statelessCookieStorage;
    if (!statelessCookieStorage.get()) {
        statelessCookieStorage.get() = adoptNS([[NSHTTPCookieStorage alloc] _initWithIdentifier:nil private:YES]);
        statelessCookieStorage.get().get().cookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
    }
    ASSERT(statelessCookieStorage.get().get().cookies.count == 0);
    return statelessCookieStorage.get().get();
}

static WebCore::RegistrableDomain lastCNAMEDomain(NSArray<NSString *> *cnames)
{
    if (auto* lastResolvedCNAMEInChain = [cnames lastObject]) {
        auto cname = String(lastResolvedCNAMEInChain);
        if (cname.endsWith('.'))
            cname = cname.left(cname.length() - 1);
        return WebCore::RegistrableDomain::uncheckedCreateFromHost(cname);
    }

    return { };
}

bool NetworkDataTaskCocoa::shouldApplyCookiePolicyForThirdPartyCloaking() const
{
    auto* session = networkSession();
    return session && session->networkStorageSession() && session->networkStorageSession()->trackingPreventionEnabled();
}

void NetworkDataTaskCocoa::updateFirstPartyInfoForSession(const URL& requestURL)
{
    if (!shouldApplyCookiePolicyForThirdPartyCloaking() || requestURL.host().isEmpty())
        return;

    auto* session = networkSession();
    auto cnameDomain = lastCNAMEDomain([m_task _resolvedCNAMEChain]);
    if (!cnameDomain.isEmpty())
        session->setFirstPartyHostCNAMEDomain(requestURL.host().toString(), WTFMove(cnameDomain));

    if (NSString *ipAddress = lastRemoteIPAddress(m_task.get()); ipAddress.length)
        session->setFirstPartyHostIPAddress(requestURL.host().toString(), ipAddress);
}

static NSArray<NSHTTPCookie *> *cookiesByCappingExpiry(NSArray<NSHTTPCookie *> *cookies, Seconds ageCap)
{
    auto *cappedCookies = [NSMutableArray arrayWithCapacity:cookies.count];
    for (NSHTTPCookie *cookie in cookies)
        [cappedCookies addObject:WebCore::NetworkStorageSession::capExpiryOfPersistentCookie(cookie, ageCap)];
    return cappedCookies;
}

static bool shouldCapCookieExpiryForThirdPartyIPAddress(const WebCore::IPAddress& remote, const WebCore::IPAddress& firstParty)
{
    auto matchingLength = remote.matchingNetMaskLength(firstParty);
    if (remote.isIPv4())
        return matchingLength < 4 * sizeof(struct in_addr);
    return matchingLength < 4 * sizeof(struct in6_addr);
}

void NetworkDataTaskCocoa::applyCookiePolicyForThirdPartyCloaking(const WebCore::ResourceRequest& request)
{
    if (isTopLevelNavigation() || !shouldApplyCookiePolicyForThirdPartyCloaking())
        return;

    if (request.isThirdParty()) {
        m_task.get()._cookieTransformCallback = nil;
        return;
    }

    // Cap expiry of incoming cookies in response if it is a same-site
    // subresource but it resolves to a different CNAME than the top
    // site request, a.k.a. third-party CNAME cloaking.
    auto firstPartyURL = request.firstPartyForCookies();
    auto firstPartyHostName = firstPartyURL.host().toString();
    auto firstPartyHostCNAME = networkSession()->firstPartyHostCNAMEDomain(firstPartyHostName);
    auto firstPartyAddress = networkSession()->firstPartyHostIPAddress(firstPartyHostName);

    m_task.get()._cookieTransformCallback = makeBlockPtr([requestURL = crossThreadCopy(request.url()), firstPartyURL = crossThreadCopy(firstPartyURL), firstPartyHostCNAME = crossThreadCopy(firstPartyHostCNAME), firstPartyAddress = crossThreadCopy(firstPartyAddress), thirdPartyCNAMEDomainForTesting = crossThreadCopy(networkSession()->thirdPartyCNAMEDomainForTesting()), ageCapForCNAMECloakedCookies = crossThreadCopy(m_ageCapForCNAMECloakedCookies), weakTask = WeakObjCPtr<NSURLSessionDataTask>(m_task.get()), debugLoggingEnabled = networkSession()->networkStorageSession()->trackingPreventionDebugLoggingEnabled()] (NSArray<NSHTTPCookie*> *cookiesSetInResponse) -> NSArray<NSHTTPCookie*> * {
        auto task = weakTask.get();
        if (!task || ![cookiesSetInResponse count])
            return cookiesSetInResponse;

        auto cnameDomain = lastCNAMEDomain([task _resolvedCNAMEChain]);
        if (cnameDomain.isEmpty() && thirdPartyCNAMEDomainForTesting)
            cnameDomain = *thirdPartyCNAMEDomainForTesting;

        if (cnameDomain.isEmpty()) {
            if (!firstPartyAddress)
                return cookiesSetInResponse;

            auto remoteAddress = WebCore::IPAddress::fromString(lastRemoteIPAddress(task.get()));
            if (!remoteAddress)
                return cookiesSetInResponse;

            if (shouldCapCookieExpiryForThirdPartyIPAddress(*remoteAddress, *firstPartyAddress)) {
                cookiesSetInResponse = cookiesByCappingExpiry(cookiesSetInResponse, ageCapForCNAMECloakedCookies);
                if (debugLoggingEnabled) {
                    for (NSHTTPCookie *cookie in cookiesSetInResponse)
                        RELEASE_LOG_INFO(ITPDebug, "Capped the expiry of third-party IP address cookie named %{public}@.", cookie.name);
                }
            }

            return cookiesSetInResponse;
        }

        // CNAME cloaking is a first-party sub resource that resolves
        // through a CNAME that differs from the first-party domain and
        // also differs from the top frame host's CNAME, if one exists.
        if (!cnameDomain.matches(firstPartyURL) && (!firstPartyHostCNAME || cnameDomain != *firstPartyHostCNAME)) {
            // Don't use RetainPtr here. This array has to be retained and
            // auto released to not be released before returned to the code
            // executing the block.
            cookiesSetInResponse = cookiesByCappingExpiry(cookiesSetInResponse, ageCapForCNAMECloakedCookies);
            if (debugLoggingEnabled) {
                for (NSHTTPCookie *cookie in cookiesSetInResponse)
                    RELEASE_LOG_INFO(ITPDebug, "Capped the expiry of third-party CNAME cloaked cookie named %{public}@.", cookie.name);
            }
        }

        return cookiesSetInResponse;
    }).get();
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

// FIXME: Temporary fix for <rdar://60089022> and <rdar://100500464> until content can be updated.
bool NetworkDataTaskCocoa::needsFirstPartyCookieBlockingLatchModeQuirk(const URL& firstPartyURL, const URL& requestURL, const URL& redirectingURL) const
{
    using RegistrableDomain = WebCore::RegistrableDomain;
    static NeverDestroyed<HashMap<RegistrableDomain, RegistrableDomain>> quirkPairs = [] {
        HashMap<RegistrableDomain, RegistrableDomain> map;
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("ymail.com"_s), RegistrableDomain::uncheckedCreateFromRegistrableDomainString("yahoo.com"_s));
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("aolmail.com"_s), RegistrableDomain::uncheckedCreateFromRegistrableDomainString("aol.com"_s));
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("googleusercontent.com"_s), RegistrableDomain::uncheckedCreateFromRegistrableDomainString("google.com"_s));
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

NetworkDataTaskCocoa::NetworkDataTaskCocoa(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters)
    : NetworkDataTask(session, client, parameters.request, parameters.storedCredentialsPolicy, parameters.shouldClearReferrerOnHTTPSToHTTPRedirect, parameters.isMainFrameNavigation)
    , m_sessionWrapper(static_cast<NetworkSessionCocoa&>(session).sessionWrapperForTask(parameters.webPageProxyID, parameters.request, parameters.storedCredentialsPolicy, parameters.isNavigatingToAppBoundDomain))
    , m_frameID(parameters.webFrameID)
    , m_pageID(parameters.webPageID)
    , m_webPageProxyID(parameters.webPageProxyID)
    , m_isForMainResourceNavigationForAnyFrame(parameters.isMainResourceNavigationForAnyFrame)
    , m_isAlwaysOnLoggingAllowed(computeIsAlwaysOnLoggingAllowed(session))
    , m_shouldRelaxThirdPartyCookieBlocking(parameters.shouldRelaxThirdPartyCookieBlocking)
    , m_sourceOrigin(parameters.sourceOrigin)
{
    auto request = parameters.request;
    auto url = request.url();
    if (m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use && url.protocolIsInHTTPFamily()) {
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
    if (!m_initialCredential.isEmpty() && !request.hasHTTPHeaderField(WebCore::HTTPHeaderName::Authorization)) {
        // FIXME: Support Digest authentication, and Proxy-Authorization.
        applyBasicAuthorizationHeader(request, m_initialCredential);
    }
#endif

    bool shouldBlockCookies = false;
#if ENABLE(TRACKING_PREVENTION)
    shouldBlockCookies = m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::EphemeralStateless;
    if (auto* networkStorageSession = session.networkStorageSession()) {
        if (!shouldBlockCookies)
            shouldBlockCookies = networkStorageSession->shouldBlockCookies(request, frameID(), pageID(), m_shouldRelaxThirdPartyCookieBlocking);
    }
#endif
    restrictRequestReferrerToOriginIfNeeded(request);

    RetainPtr<NSURLRequest> nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
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

    if (parameters.networkConnectionIntegrityPolicy.contains(WebCore::NetworkConnectionIntegrity::Enabled))
        enableNetworkConnectionIntegrity(mutableRequest.get(), NetworkSession::needsAdditionalNetworkConnectionIntegritySettings(request));

#if ENABLE(APP_PRIVACY_REPORT)
    mutableRequest.get().attribution = request.isAppInitiated() ? NSURLRequestAttributionDeveloper : NSURLRequestAttributionUser;
#endif

    // FIXME: Remove hadMainFrameMainResourcePrivateRelayed, PrivateRelayed, and all the associated piping.
    
    nsRequest = mutableRequest;

#if ENABLE(APP_PRIVACY_REPORT)
    m_session->appPrivacyReportTestingData().didLoadAppInitiatedRequest(nsRequest.get().attribution == NSURLRequestAttributionDeveloper);
#endif

    applySniffingPoliciesAndBindRequestToInferfaceIfNeeded(nsRequest, parameters.contentSniffingPolicy == WebCore::ContentSniffingPolicy::SniffContent && !url.isLocalFile(), parameters.contentEncodingSniffingPolicy);

    m_task = [m_sessionWrapper->session dataTaskWithRequest:nsRequest.get()];

    switch (parameters.storedCredentialsPolicy) {
    case WebCore::StoredCredentialsPolicy::Use:
        ASSERT(m_sessionWrapper->session.get().configuration.URLCredentialStorage);
        break;
    case WebCore::StoredCredentialsPolicy::EphemeralStateless:
        ASSERT(!m_sessionWrapper->session.get().configuration.URLCredentialStorage);
        break;
    case WebCore::StoredCredentialsPolicy::DoNotUse:
#if HAVE(NSURLSESSION_EFFECTIVE_CONFIGURATION_OBJECT)
        RetainPtr<NSURLSessionConfiguration> copiedConfiguration = m_sessionWrapper->session.get().configuration;
        copiedConfiguration.get().URLCredentialStorage = nil;
        auto effectiveConfiguration = adoptNS([[NSURLSessionEffectiveConfiguration alloc] _initWithConfiguration:copiedConfiguration.get()]);
        [m_task _adoptEffectiveConfiguration:effectiveConfiguration.get()];
#else
        RetainPtr<NSURLSessionConfiguration> effectiveConfiguration = m_sessionWrapper->session.get().configuration;
        effectiveConfiguration.get().URLCredentialStorage = nil;
        [m_task _adoptEffectiveConfiguration:effectiveConfiguration.get()];
#endif
        break;
    };

    WTFBeginSignpost(m_task.get(), "DataTask", "%" PRIVATE_LOG_STRING " pri: %.2f preconnect: %d", url.string().ascii().data(), toNSURLSessionTaskPriority(request.priority()), parameters.shouldPreconnectOnly == PreconnectOnly::Yes);

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

#if ENABLE(TRACKING_PREVENTION)
    applyCookiePolicyForThirdPartyCloaking(request);
    if (shouldBlockCookies) {
#if !RELEASE_LOG_DISABLED
        if (m_session->shouldLogCookieInformation())
            RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkDataTaskCocoa::logCookieInformation: pageID=%" PRIu64 ", frameID=%" PRIu64 ", taskID=%lu: Blocking cookies for URL %s", this, pageID().toUInt64(), frameID().object().toUInt64(), (unsigned long)[m_task taskIdentifier], [nsRequest URL].absoluteString.UTF8String);
#else
        LOG(NetworkSession, "%lu Blocking cookies for URL %s", (unsigned long)[m_task taskIdentifier], [nsRequest URL].absoluteString.UTF8String);
#endif
        blockCookies();
    }
#endif

    if (WebCore::ResourceRequest::resourcePrioritiesEnabled())
        m_task.get().priority = toNSURLSessionTaskPriority(request.priority());

    updateTaskWithFirstPartyForSameSiteCookies(m_task.get(), request);

#if HAVE(NW_ACTIVITY)
    if (parameters.networkActivityTracker)
        m_task.get()._nw_activity = parameters.networkActivityTracker->getPlatformObject();
#endif

    m_session->registerNetworkDataTask(*this);
}

NetworkDataTaskCocoa::~NetworkDataTaskCocoa()
{
    if (m_task && m_sessionWrapper) {
        auto dataTask = m_sessionWrapper->dataTaskMap.take([m_task taskIdentifier]);
        RELEASE_ASSERT(dataTask == this);
    }
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

void NetworkDataTaskCocoa::didNegotiateModernTLS(const URL& url)
{
    if (m_client)
        m_client->didNegotiateModernTLS(url);
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

void NetworkDataTaskCocoa::didReceiveData(const WebCore::SharedBuffer& data)
{
    WTFEmitSignpost(m_task.get(), "DataTask", "received %zd bytes", data.size());

    if (m_client)
        m_client->didReceiveData(data);
}

void NetworkDataTaskCocoa::didReceiveResponse(WebCore::ResourceResponse&& response, NegotiatedLegacyTLS negotiatedLegacyTLS, PrivateRelayed privateRelayed, WebKit::ResponseCompletionHandler&& completionHandler)
{
    WTFEmitSignpost(m_task.get(), "DataTask", "received response headers");
    if (isTopLevelNavigation())
        updateFirstPartyInfoForSession(response.url());
    NetworkDataTask::didReceiveResponse(WTFMove(response), negotiatedLegacyTLS, privateRelayed, WTFMove(completionHandler));
}

void NetworkDataTaskCocoa::willPerformHTTPRedirection(WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    WTFEmitSignpost(m_task.get(), "DataTask", "redirect");

    networkLoadMetrics().hasCrossOriginRedirect = networkLoadMetrics().hasCrossOriginRedirect || !WebCore::SecurityOrigin::create(request.url())->canRequest(redirectResponse.url());

    if (redirectResponse.httpStatusCode() == 307 || redirectResponse.httpStatusCode() == 308) {
        ASSERT(m_lastHTTPMethod == request.httpMethod());
        WebCore::FormData* body = m_firstRequest.httpBody();
        if (body && !body->isEmpty() && !equalLettersIgnoringASCIICase(m_lastHTTPMethod, "get"_s))
            request.setHTTPBody(body);
        
        String originalContentType = m_firstRequest.httpContentType();
        if (!originalContentType.isEmpty())
            request.setHTTPHeaderField(WebCore::HTTPHeaderName::ContentType, originalContentType);
    } else if (redirectResponse.httpStatusCode() == 303) { // FIXME: (rdar://problem/13706454).
        if (equalLettersIgnoringASCIICase(m_firstRequest.httpMethod(), "head"_s))
            request.setHTTPMethod("HEAD"_s);

        String originalContentType = m_firstRequest.httpContentType();
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
#if USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
        // Only consider applying authentication credentials if this is actually a redirect and the redirect
        // URL didn't include credentials of its own.
        if (m_user.isEmpty() && m_password.isEmpty() && !redirectResponse.isNull()) {
            auto credential = m_session->networkStorageSession() ? m_session->networkStorageSession()->credentialStorage().get(m_partition, request.url()) : WebCore::Credential();
            if (!credential.isEmpty()) {
                m_initialCredential = credential;

                // FIXME: Support Digest authentication, and Proxy-Authorization.
                applyBasicAuthorizationHeader(request, m_initialCredential);
            }
#endif
        }
    }

    if (isTopLevelNavigation())
        request.setFirstPartyForCookies(request.url());

#if ENABLE(APP_PRIVACY_REPORT)
    request.setIsAppInitiated(request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody).attribution == NSURLRequestAttributionDeveloper);
#endif

#if ENABLE(TRACKING_PREVENTION)
    applyCookiePolicyForThirdPartyCloaking(request);
    if (!m_hasBeenSetToUseStatelessCookieStorage) {
        if (m_storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::EphemeralStateless
            || (m_session->networkStorageSession() && m_session->networkStorageSession()->shouldBlockCookies(request, m_frameID, m_pageID, m_shouldRelaxThirdPartyCookieBlocking)))
            blockCookies();
    } else if (m_storedCredentialsPolicy != WebCore::StoredCredentialsPolicy::EphemeralStateless && needsFirstPartyCookieBlockingLatchModeQuirk(request.firstPartyForCookies(), request.url(), redirectResponse.url()))
        unblockCookies();
#if !RELEASE_LOG_DISABLED
    if (m_session->shouldLogCookieInformation())
        RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkDataTaskCocoa::willPerformHTTPRedirection::logCookieInformation: pageID=%" PRIu64 ", frameID=%" PRIu64 ", taskID=%lu: %s cookies for redirect URL %s", this, m_pageID.toUInt64(), m_frameID.object().toUInt64(), (unsigned long)[m_task taskIdentifier], (m_hasBeenSetToUseStatelessCookieStorage ? "Blocking" : "Not blocking"), request.url().string().utf8().data());
#else
    LOG(NetworkSession, "%lu %s cookies for redirect URL %s", (unsigned long)[m_task taskIdentifier], (m_hasBeenSetToUseStatelessCookieStorage ? "Blocking" : "Not blocking"), request.url().string().utf8().data());
#endif
#endif

    updateTaskWithFirstPartyForSameSiteCookies(m_task.get(), request);

    if (m_client)
        m_client->willPerformHTTPRedirection(WTFMove(redirectResponse), WTFMove(request), [completionHandler = WTFMove(completionHandler), this, weakThis = ThreadSafeWeakPtr { *this }] (auto&& request) mutable {
            auto strongThis = weakThis.get();
            if (!strongThis || !m_session)
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
    
    if (!m_user.isEmpty() || !m_password.isEmpty()) {
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

void NetworkDataTaskCocoa::checkTAO(const WebCore::ResourceResponse& response)
{
    if (networkLoadMetrics().failsTAOCheck)
        return;

    RefPtr<WebCore::SecurityOrigin> origin;
    if (isTopLevelNavigation())
        origin = WebCore::SecurityOrigin::create(firstRequest().url());
    else
        origin = m_sourceOrigin;

    if (origin)
        networkLoadMetrics().failsTAOCheck = !passesTimingAllowOriginCheck(response, *origin);
}

}
