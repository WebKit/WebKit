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

#import "config.h"
#import "NetworkSessionCocoa.h"

#if USE(NETWORK_SESSION)

#import "AuthenticationManager.h"
#import "DataReference.h"
#import "Download.h"
#import "LegacyCustomProtocolManager.h"
#import "Logging.h"
#import "NetworkCache.h"
#import "NetworkLoad.h"
#import "NetworkProcess.h"
#import "SessionTracker.h"
#import <Foundation/NSURLSession.h>
#import <WebCore/CFNetworkSPI.h>
#import <WebCore/Credential.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/URL.h>
#import <WebCore/WebCoreURLResponse.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

using namespace WebKit;

static NSURLSessionResponseDisposition toNSURLSessionResponseDisposition(WebCore::PolicyAction disposition)
{
    switch (disposition) {
    case WebCore::PolicyAction::PolicyIgnore:
        return NSURLSessionResponseCancel;
    case WebCore::PolicyAction::PolicyUse:
        return NSURLSessionResponseAllow;
    case WebCore::PolicyAction::PolicyDownload:
        return NSURLSessionResponseBecomeDownload;
    }
}

static NSURLSessionAuthChallengeDisposition toNSURLSessionAuthChallengeDisposition(WebKit::AuthenticationChallengeDisposition disposition)
{
    switch (disposition) {
    case WebKit::AuthenticationChallengeDisposition::UseCredential:
        return NSURLSessionAuthChallengeUseCredential;
    case WebKit::AuthenticationChallengeDisposition::PerformDefaultHandling:
        return NSURLSessionAuthChallengePerformDefaultHandling;
    case WebKit::AuthenticationChallengeDisposition::Cancel:
        return NSURLSessionAuthChallengeCancelAuthenticationChallenge;
    case WebKit::AuthenticationChallengeDisposition::RejectProtectionSpace:
        return NSURLSessionAuthChallengeRejectProtectionSpace;
    }
}

static WebCore::NetworkLoadPriority toNetworkLoadPriority(float priority)
{
    if (priority <= NSURLSessionTaskPriorityLow)
        return WebCore::NetworkLoadPriority::Low;
    if (priority >= NSURLSessionTaskPriorityHigh)
        return WebCore::NetworkLoadPriority::High;
    return WebCore::NetworkLoadPriority::Medium;
}

@interface WKNetworkSessionDelegate : NSObject <NSURLSessionDataDelegate> {
    RefPtr<WebKit::NetworkSessionCocoa> _session;
    bool _withCredentials;
}

- (id)initWithNetworkSession:(WebKit::NetworkSessionCocoa&)session withCredentials:(bool)withCredentials;
- (void)sessionInvalidated;

@end

@implementation WKNetworkSessionDelegate

- (id)initWithNetworkSession:(WebKit::NetworkSessionCocoa&)session withCredentials:(bool)withCredentials
{
    self = [super init];
    if (!self)
        return nil;

    _session = &session;
    _withCredentials = withCredentials;

    return self;
}

- (void)sessionInvalidated
{
    _session = nullptr;
}

- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(nullable NSError *)error
{
    ASSERT(!_session);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didSendBodyData:(int64_t)bytesSent totalBytesSent:(int64_t)totalBytesSent totalBytesExpectedToSend:(int64_t)totalBytesExpectedToSend
{
    if (!_session)
        return;

    auto storedCredentials = _withCredentials ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier, storedCredentials))
        networkDataTask->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    if (!_session) {
        completionHandler(nil);
        return;
    }

    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%llu willPerformHTTPRedirection from %s to %s", taskIdentifier, response.URL.absoluteString.UTF8String, request.URL.absoluteString.UTF8String);
    
    auto storedCredentials = _withCredentials ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier, storedCredentials)) {
        auto completionHandlerCopy = Block_copy(completionHandler);
        networkDataTask->willPerformHTTPRedirection(response, request, [completionHandlerCopy, taskIdentifier](auto& request) {
            LOG(NetworkSession, "%llu willPerformHTTPRedirection completionHandler (%s)", taskIdentifier, request.url().string().utf8().data());
            completionHandlerCopy(request.nsURLRequest(WebCore::UpdateHTTPBody));
            Block_release(completionHandlerCopy);
        });
    } else {
        LOG(NetworkSession, "%llu willPerformHTTPRedirection completionHandler (nil)", taskIdentifier);
        completionHandler(nil);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask*)task _schemeUpgraded:(NSURLRequest*)request completionHandler:(void (^)(NSURLRequest*))completionHandler
{
    if (!_session) {
        completionHandler(nil);
        return;
    }

    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%llu _schemeUpgraded %s", taskIdentifier, request.URL.absoluteString.UTF8String);
    
    auto storedCredentials = _withCredentials ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(taskIdentifier, storedCredentials)) {
        auto completionHandlerCopy = Block_copy(completionHandler);
        networkDataTask->willPerformHTTPRedirection(WebCore::synthesizeRedirectResponseIfNecessary([task currentRequest], request, nil), request, [completionHandlerCopy, taskIdentifier](auto& request) {
            LOG(NetworkSession, "%llu _schemeUpgraded completionHandler (%s)", taskIdentifier, request.url().string().utf8().data());
            completionHandlerCopy(request.nsURLRequest(WebCore::UpdateHTTPBody));
            Block_release(completionHandlerCopy);
        });
    } else {
        LOG(NetworkSession, "%llu _schemeUpgraded completionHandler (nil)", taskIdentifier);
        completionHandler(nil);
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask willCacheResponse:(NSCachedURLResponse *)proposedResponse completionHandler:(void (^)(NSCachedURLResponse * _Nullable cachedResponse))completionHandler
{
    if (!_session) {
        completionHandler(nil);
        return;
    }

    // FIXME: remove if <rdar://problem/20001985> is ever resolved.
    if ([proposedResponse.response respondsToSelector:@selector(allHeaderFields)]
        && [[(id)proposedResponse.response allHeaderFields] objectForKey:@"Content-Range"])
        completionHandler(nil);
    else
        completionHandler(proposedResponse);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    if (!_session) {
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        return;
    }

    auto taskIdentifier = task.taskIdentifier;
    LOG(NetworkSession, "%llu didReceiveChallenge", taskIdentifier);
    
    auto storedCredentials = _withCredentials ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier, storedCredentials)) {
        WebCore::AuthenticationChallenge authenticationChallenge(challenge);
        auto completionHandlerCopy = Block_copy(completionHandler);
        auto sessionID = _session->sessionID();
        auto challengeCompletionHandler = [completionHandlerCopy, sessionID, authenticationChallenge, taskIdentifier, partition = networkDataTask->partition()](WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential)
        {
            LOG(NetworkSession, "%llu didReceiveChallenge completionHandler %d", taskIdentifier, disposition);
#if !USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
            UNUSED_PARAM(sessionID);
            UNUSED_PARAM(authenticationChallenge);
#else
            if (credential.persistence() == WebCore::CredentialPersistenceForSession && authenticationChallenge.protectionSpace().isPasswordBased()) {

                WebCore::Credential nonPersistentCredential(credential.user(), credential.password(), WebCore::CredentialPersistenceNone);
                WebCore::URL urlToStore;
                if (authenticationChallenge.failureResponse().httpStatusCode() == 401)
                    urlToStore = authenticationChallenge.failureResponse().url();
                if (auto storageSession = WebCore::NetworkStorageSession::storageSession(sessionID))
                    storageSession->credentialStorage().set(partition, nonPersistentCredential, authenticationChallenge.protectionSpace(), urlToStore);
                else
                    ASSERT_NOT_REACHED();

                completionHandlerCopy(toNSURLSessionAuthChallengeDisposition(disposition), nonPersistentCredential.nsCredential());
            } else
#endif
                completionHandlerCopy(toNSURLSessionAuthChallengeDisposition(disposition), credential.nsCredential());
            Block_release(completionHandlerCopy);
        };
        networkDataTask->didReceiveChallenge(challenge, WTFMove(challengeCompletionHandler));
    } else {
        LOG(NetworkSession, "%llu didReceiveChallenge completionHandler (cancel)", taskIdentifier);
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error
{
    if (!_session)
        return;

    LOG(NetworkSession, "%llu didCompleteWithError %@", task.taskIdentifier, error);
    auto storedCredentials = _withCredentials ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier, storedCredentials))
        networkDataTask->didCompleteWithError(error, networkDataTask->networkLoadMetrics());
    else if (error) {
        auto downloadID = _session->takeDownloadID(task.taskIdentifier);
        if (downloadID.downloadID()) {
            if (auto* download = WebKit::NetworkProcess::singleton().downloadManager().download(downloadID)) {
                NSData *resumeData = nil;
                if (id userInfo = error.userInfo) {
                    if ([userInfo isKindOfClass:[NSDictionary class]])
                        resumeData = userInfo[@"NSURLSessionDownloadTaskResumeData"];
                }
                
                if (resumeData && [resumeData isKindOfClass:[NSData class]])
                    download->didFail(error, { static_cast<const uint8_t*>(resumeData.bytes), resumeData.length });
                else
                    download->didFail(error, { });
            }
        }
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didFinishCollectingMetrics:(NSURLSessionTaskMetrics *)metrics
{
    if (!_session)
        return;

    LOG(NetworkSession, "%llu didFinishCollectingMetrics", task.taskIdentifier);
    auto storedCredentials = _withCredentials ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier, storedCredentials)) {
        NSURLSessionTaskTransactionMetrics *m = metrics.transactionMetrics.lastObject;
        NSDate *fetchStartDate = m.fetchStartDate;
        NSTimeInterval domainLookupStartInterval = m.domainLookupStartDate ? [m.domainLookupStartDate timeIntervalSinceDate:fetchStartDate] : -1;
        NSTimeInterval domainLookupEndInterval = m.domainLookupEndDate ? [m.domainLookupEndDate timeIntervalSinceDate:fetchStartDate] : -1;
        NSTimeInterval connectStartInterval = m.connectStartDate ? [m.connectStartDate timeIntervalSinceDate:fetchStartDate] : -1;
        NSTimeInterval secureConnectionStartInterval = m.secureConnectionStartDate ? [m.secureConnectionStartDate timeIntervalSinceDate:fetchStartDate] : -1;
        NSTimeInterval connectEndInterval = m.connectEndDate ? [m.connectEndDate timeIntervalSinceDate:fetchStartDate] : -1;
        NSTimeInterval requestStartInterval = [m.requestStartDate timeIntervalSinceDate:fetchStartDate];
        NSTimeInterval responseStartInterval = [m.responseStartDate timeIntervalSinceDate:fetchStartDate];
        NSTimeInterval responseEndInterval = [m.responseEndDate timeIntervalSinceDate:fetchStartDate];

        auto& networkLoadMetrics = networkDataTask->networkLoadMetrics();
        networkLoadMetrics.domainLookupStart = Seconds(domainLookupStartInterval);
        networkLoadMetrics.domainLookupEnd = Seconds(domainLookupEndInterval);
        networkLoadMetrics.connectStart = Seconds(connectStartInterval);
        networkLoadMetrics.secureConnectionStart = Seconds(secureConnectionStartInterval);
        networkLoadMetrics.connectEnd = Seconds(connectEndInterval);
        networkLoadMetrics.requestStart = Seconds(requestStartInterval);
        networkLoadMetrics.responseStart = Seconds(responseStartInterval);
        networkLoadMetrics.responseEnd = Seconds(responseEndInterval);
        networkLoadMetrics.markComplete();

        networkLoadMetrics.protocol = String(m.networkProtocolName);
        networkLoadMetrics.priority = toNetworkLoadPriority(task.priority);
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
        networkLoadMetrics.remoteAddress = String(m._remoteAddressAndPort);
        networkLoadMetrics.connectionIdentifier = String([m._connectionIdentifier UUIDString]);
#endif
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    if (!_session) {
        completionHandler(NSURLSessionResponseCancel);
        return;
    }

    auto taskIdentifier = dataTask.taskIdentifier;
    LOG(NetworkSession, "%llu didReceiveResponse", taskIdentifier);
    auto storedCredentials = _withCredentials ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(taskIdentifier, storedCredentials)) {
        ASSERT(isMainThread());
        
        // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
        int statusCode = [response respondsToSelector:@selector(statusCode)] ? [(id)response statusCode] : 0;
        if (statusCode != 304) {
            bool isMainResourceLoad = networkDataTask->firstRequest().requester() == WebCore::ResourceRequest::Requester::Main;
            WebCore::adjustMIMETypeIfNecessary(response._CFURLResponse, isMainResourceLoad);
        }

        WebCore::ResourceResponse resourceResponse(response);
        // Lazy initialization is not helpful in the WebKit2 case because we always end up initializing
        // all the fields when sending the response to the WebContent process over IPC.
        resourceResponse.disableLazyInitialization();

        // FIXME: This cannot be eliminated until other code no longer relies on ResourceResponse's
        // NetworkLoadMetrics. For example, PerformanceTiming.
        copyTimingData([dataTask _timingData], resourceResponse.deprecatedNetworkLoadMetrics());

        auto completionHandlerCopy = Block_copy(completionHandler);
        networkDataTask->didReceiveResponse(WTFMove(resourceResponse), [completionHandlerCopy, taskIdentifier](WebCore::PolicyAction policyAction) {
            LOG(NetworkSession, "%llu didReceiveResponse completionHandler (%d)", taskIdentifier, policyAction);
            completionHandlerCopy(toNSURLSessionResponseDisposition(policyAction));
            Block_release(completionHandlerCopy);
        });
    } else {
        LOG(NetworkSession, "%llu didReceiveResponse completionHandler (cancel)", taskIdentifier);
        completionHandler(NSURLSessionResponseCancel);
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
    if (!_session)
        return;

    auto storedCredentials = _withCredentials ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(dataTask.taskIdentifier, storedCredentials))
        networkDataTask->didReceiveData(WebCore::SharedBuffer::wrapNSData(data));
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didFinishDownloadingToURL:(NSURL *)location
{
    if (!_session)
        return;

    auto downloadID = _session->takeDownloadID([downloadTask taskIdentifier]);
    if (auto* download = WebKit::NetworkProcess::singleton().downloadManager().download(downloadID))
        download->didFinish();
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didWriteData:(int64_t)bytesWritten totalBytesWritten:(int64_t)totalBytesWritten totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite
{
    if (!_session)
        return;

    auto storedCredentials = _withCredentials ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    ASSERT_WITH_MESSAGE_UNUSED(storedCredentials, !_session->dataTaskForIdentifier([downloadTask taskIdentifier], storedCredentials), "The NetworkDataTask should be destroyed immediately after didBecomeDownloadTask returns");

    auto downloadID = _session->downloadID([downloadTask taskIdentifier]);
    if (auto* download = WebKit::NetworkProcess::singleton().downloadManager().download(downloadID))
        download->didReceiveData(bytesWritten);
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didResumeAtOffset:(int64_t)fileOffset expectedTotalBytes:(int64_t)expectedTotalBytes
{
    if (!_session)
        return;

    notImplemented();
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didBecomeDownloadTask:(NSURLSessionDownloadTask *)downloadTask
{
    if (!_session)
        return;

    auto storedCredentials = _withCredentials ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier([dataTask taskIdentifier], storedCredentials)) {
        Ref<NetworkDataTaskCocoa> protectedNetworkDataTask(*networkDataTask);
        auto downloadID = networkDataTask->pendingDownloadID();
        auto& downloadManager = WebKit::NetworkProcess::singleton().downloadManager();
        auto download = std::make_unique<WebKit::Download>(downloadManager, downloadID, downloadTask, _session->sessionID(), networkDataTask->suggestedFilename());
        networkDataTask->transferSandboxExtensionToDownload(*download);
        ASSERT(WebCore::fileExists(networkDataTask->pendingDownloadLocation()));
        download->didCreateDestination(networkDataTask->pendingDownloadLocation());
        downloadManager.dataTaskBecameDownloadTask(downloadID, WTFMove(download));

        _session->addDownloadID([downloadTask taskIdentifier], downloadID);
    }
}

@end

namespace WebKit {
    
static NSURLSessionConfiguration *configurationForSessionID(const WebCore::SessionID& session)
{
    if (session.isEphemeral())
        return [NSURLSessionConfiguration ephemeralSessionConfiguration];
    return [NSURLSessionConfiguration defaultSessionConfiguration];
}

static LegacyCustomProtocolManager*& globalLegacyCustomProtocolManager()
{
    static LegacyCustomProtocolManager* customProtocolManager { nullptr };
    return customProtocolManager;
}

static RetainPtr<CFDataRef>& globalSourceApplicationAuditTokenData()
{
    static NeverDestroyed<RetainPtr<CFDataRef>> sourceApplicationAuditTokenData;
    return sourceApplicationAuditTokenData.get();
}

static String& globalSourceApplicationBundleIdentifier()
{
    static NeverDestroyed<String> sourceApplicationBundleIdentifier;
    return sourceApplicationBundleIdentifier.get();
}

static String& globalSourceApplicationSecondaryIdentifier()
{
    static NeverDestroyed<String> sourceApplicationSecondaryIdentifier;
    return sourceApplicationSecondaryIdentifier.get();
}

#if PLATFORM(IOS)
static String& globalCTDataConnectionServiceType()
{
    static NeverDestroyed<String> ctDataConnectionServiceType;
    return ctDataConnectionServiceType.get();
}
#endif

#if !ASSERT_DISABLED
static bool sessionsCreated = false;
#endif

void NetworkSessionCocoa::setLegacyCustomProtocolManager(LegacyCustomProtocolManager* customProtocolManager)
{
    ASSERT(!sessionsCreated);
    globalLegacyCustomProtocolManager() = customProtocolManager;
}
    
void NetworkSessionCocoa::setSourceApplicationAuditTokenData(RetainPtr<CFDataRef>&& data)
{
    ASSERT(!sessionsCreated);
    globalSourceApplicationAuditTokenData() = data;
}

void NetworkSessionCocoa::setSourceApplicationBundleIdentifier(const String& identifier)
{
    ASSERT(!sessionsCreated);
    globalSourceApplicationBundleIdentifier() = identifier;
}

void NetworkSessionCocoa::setSourceApplicationSecondaryIdentifier(const String& identifier)
{
    ASSERT(!sessionsCreated);
    globalSourceApplicationSecondaryIdentifier() = identifier;
}

#if PLATFORM(IOS)
void NetworkSessionCocoa::setCTDataConnectionServiceType(const String& type)
{
    ASSERT(!sessionsCreated);
    globalCTDataConnectionServiceType() = type;
}
#endif

Ref<NetworkSession> NetworkSessionCocoa::create(WebCore::SessionID sessionID, LegacyCustomProtocolManager* customProtocolManager)
{
    return adoptRef(*new NetworkSessionCocoa(sessionID, customProtocolManager));
}

NetworkSession& NetworkSessionCocoa::defaultSession()
{
    ASSERT(isMainThread());
    static NetworkSession* session = &NetworkSessionCocoa::create(WebCore::SessionID::defaultSessionID(), globalLegacyCustomProtocolManager()).leakRef();
    return *session;
}

NetworkSessionCocoa::NetworkSessionCocoa(WebCore::SessionID sessionID, LegacyCustomProtocolManager* customProtocolManager)
    : NetworkSession(sessionID)
{
    relaxAdoptionRequirement();

#if !ASSERT_DISABLED
    sessionsCreated = true;
#endif

    NSURLSessionConfiguration *configuration = configurationForSessionID(m_sessionID);

    if (NetworkCache::singleton().isEnabled())
        configuration.URLCache = nil;
    
    if (auto& data = globalSourceApplicationAuditTokenData())
        configuration._sourceApplicationAuditTokenData = (NSData *)data.get();

    auto& sourceApplicationBundleIdentifier = globalSourceApplicationBundleIdentifier();
    if (!sourceApplicationBundleIdentifier.isEmpty()) {
        configuration._sourceApplicationBundleIdentifier = sourceApplicationBundleIdentifier;
        configuration._sourceApplicationAuditTokenData = nil;
    }

    auto& sourceApplicationSecondaryIdentifier = globalSourceApplicationSecondaryIdentifier();
    if (!sourceApplicationSecondaryIdentifier.isEmpty())
        configuration._sourceApplicationSecondaryIdentifier = sourceApplicationSecondaryIdentifier;

#if PLATFORM(IOS)
    auto& ctDataConnectionServiceType = globalCTDataConnectionServiceType();
    if (!ctDataConnectionServiceType.isEmpty())
        configuration._CTDataConnectionServiceType = ctDataConnectionServiceType;
#endif

    if (customProtocolManager)
        customProtocolManager->registerProtocolClass(configuration);
    
#if HAVE(TIMINGDATAOPTIONS)
    configuration._timingDataOptions = _TimingDataOptionsEnableW3CNavigationTiming;
#else
    setCollectsTimingData();
#endif

    if (sessionID == WebCore::SessionID::defaultSessionID()) {
        if (CFHTTPCookieStorageRef storage = WebCore::NetworkStorageSession::defaultStorageSession().cookieStorage().get())
            configuration.HTTPCookieStorage = [[[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:storage] autorelease];
    } else {
        auto* storageSession = WebCore::NetworkStorageSession::storageSession(sessionID);
        RELEASE_ASSERT(storageSession);
        if (CFHTTPCookieStorageRef storage = storageSession->cookieStorage().get())
            configuration.HTTPCookieStorage = [[[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:storage] autorelease];
    }

    m_sessionWithCredentialStorageDelegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:*this withCredentials:true]);
    m_sessionWithCredentialStorage = [NSURLSession sessionWithConfiguration:configuration delegate:static_cast<id>(m_sessionWithCredentialStorageDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];

    configuration.URLCredentialStorage = nil;
    m_sessionWithoutCredentialStorageDelegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:*this withCredentials:false]);
    m_sessionWithoutCredentialStorage = [NSURLSession sessionWithConfiguration:configuration delegate:static_cast<id>(m_sessionWithoutCredentialStorageDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
    LOG(NetworkSession, "Created NetworkSession with cookieAcceptPolicy %lu", configuration.HTTPCookieStorage.cookieAcceptPolicy);
}

NetworkSessionCocoa::~NetworkSessionCocoa()
{
}

void NetworkSessionCocoa::invalidateAndCancel()
{
    NetworkSession::invalidateAndCancel();

    [m_sessionWithCredentialStorage invalidateAndCancel];
    [m_sessionWithoutCredentialStorage invalidateAndCancel];
    [m_sessionWithCredentialStorageDelegate sessionInvalidated];
    [m_sessionWithoutCredentialStorageDelegate sessionInvalidated];
}

void NetworkSessionCocoa::clearCredentials()
{
#if !USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
    ASSERT(m_dataTaskMapWithCredentials.isEmpty());
    ASSERT(m_dataTaskMapWithoutCredentials.isEmpty());
    ASSERT(m_downloadMap.isEmpty());
    // FIXME: Use resetWithCompletionHandler instead.
    m_sessionWithCredentialStorage = [NSURLSession sessionWithConfiguration:m_sessionWithCredentialStorage.get().configuration delegate:static_cast<id>(m_sessionWithCredentialStorageDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
    m_sessionWithoutCredentialStorage = [NSURLSession sessionWithConfiguration:m_sessionWithoutCredentialStorage.get().configuration delegate:static_cast<id>(m_sessionWithoutCredentialStorageDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
#endif
}

NetworkDataTaskCocoa* NetworkSessionCocoa::dataTaskForIdentifier(NetworkDataTaskCocoa::TaskIdentifier taskIdentifier, WebCore::StoredCredentials storedCredentials)
{
    ASSERT(isMainThread());
    if (storedCredentials == WebCore::StoredCredentials::AllowStoredCredentials)
        return m_dataTaskMapWithCredentials.get(taskIdentifier);
    return m_dataTaskMapWithoutCredentials.get(taskIdentifier);
}

void NetworkSessionCocoa::addDownloadID(NetworkDataTaskCocoa::TaskIdentifier taskIdentifier, DownloadID downloadID)
{
#ifndef NDEBUG
    ASSERT(!m_downloadMap.contains(taskIdentifier));
    for (auto idInMap : m_downloadMap.values())
        ASSERT(idInMap != downloadID);
#endif
    m_downloadMap.add(taskIdentifier, downloadID);
}

DownloadID NetworkSessionCocoa::downloadID(NetworkDataTaskCocoa::TaskIdentifier taskIdentifier)
{
    ASSERT(m_downloadMap.get(taskIdentifier).downloadID());
    return m_downloadMap.get(taskIdentifier);
}

DownloadID NetworkSessionCocoa::takeDownloadID(NetworkDataTaskCocoa::TaskIdentifier taskIdentifier)
{
    auto downloadID = m_downloadMap.take(taskIdentifier);
    return downloadID;
}

}

#endif
