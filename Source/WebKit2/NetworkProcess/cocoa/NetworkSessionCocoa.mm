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
#import "NetworkSession.h"

#if USE(NETWORK_SESSION)

#import "CustomProtocolManager.h"
#import "DataReference.h"
#import "Download.h"
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
#import <WebCore/ResourceLoadTiming.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/URL.h>
#import <WebCore/WebCoreURLResponse.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

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

@interface WKNetworkSessionDelegate : NSObject <NSURLSessionDataDelegate> {
    RefPtr<WebKit::NetworkSession> _session;
    bool _sessionDestroyed;
}

- (id)initWithNetworkSession:(WebKit::NetworkSession&)session;

@end

@implementation WKNetworkSessionDelegate

- (id)initWithNetworkSession:(WebKit::NetworkSession&)session
{
    self = [super init];
    if (!self)
        return nil;

    _session = &session;

    return self;
}

- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(nullable NSError *)error
{
    _session = nullptr;
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didSendBodyData:(int64_t)bytesSent totalBytesSent:(int64_t)totalBytesSent totalBytesExpectedToSend:(int64_t)totalBytesExpectedToSend
{
    auto storedCredentials = session.configuration.URLCredentialStorage ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier, storedCredentials))
        networkDataTask->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    auto storedCredentials = session.configuration.URLCredentialStorage ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier, storedCredentials)) {
        auto completionHandlerCopy = Block_copy(completionHandler);
        networkDataTask->willPerformHTTPRedirection(response, request, [completionHandlerCopy](const WebCore::ResourceRequest& request) {
            completionHandlerCopy(request.nsURLRequest(WebCore::UpdateHTTPBody));
            Block_release(completionHandlerCopy);
        });
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(nil);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    auto storedCredentials = session.configuration.URLCredentialStorage ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier, storedCredentials)) {
        WebCore::AuthenticationChallenge authenticationChallenge(challenge);
        auto completionHandlerCopy = Block_copy(completionHandler);
        auto sessionID = _session->sessionID();
        auto challengeCompletionHandler = [completionHandlerCopy, sessionID, authenticationChallenge](WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential)
        {
#if !USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
            UNUSED_PARAM(sessionID);
            UNUSED_PARAM(authenticationChallenge);
#else
            if (credential.persistence() == WebCore::CredentialPersistenceForSession && authenticationChallenge.protectionSpace().authenticationScheme() != WebCore::ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested) {

                WebCore::Credential nonPersistentCredential(credential.user(), credential.password(), WebCore::CredentialPersistenceNone);
                WebCore::URL urlToStore;
                if (authenticationChallenge.failureResponse().httpStatusCode() == 401)
                    urlToStore = authenticationChallenge.failureResponse().url();
                if (auto storageSession = WebKit::SessionTracker::storageSession(sessionID))
                    storageSession->credentialStorage().set(nonPersistentCredential, authenticationChallenge.protectionSpace(), urlToStore);
                else
                    ASSERT_NOT_REACHED();

                completionHandlerCopy(toNSURLSessionAuthChallengeDisposition(disposition), nonPersistentCredential.nsCredential());
            } else
#endif
                completionHandlerCopy(toNSURLSessionAuthChallengeDisposition(disposition), credential.nsCredential());
            Block_release(completionHandlerCopy);
        };
        networkDataTask->didReceiveChallenge(challenge, challengeCompletionHandler);
    } else {
        ASSERT_NOT_REACHED();
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error
{
    auto storedCredentials = session.configuration.URLCredentialStorage ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier, storedCredentials))
        networkDataTask->didCompleteWithError(error);
    else if (error) {
        auto downloadID = _session->takeDownloadID(task.taskIdentifier);
        if (downloadID.downloadID()) {
            if (auto* download = WebKit::NetworkProcess::singleton().downloadManager().download(downloadID))
                download->didFail(error, { });
        }
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    auto storedCredentials = session.configuration.URLCredentialStorage ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(dataTask.taskIdentifier, storedCredentials)) {
        ASSERT(isMainThread());
        
        // Avoid MIME type sniffing if the response comes back as 304 Not Modified.
        int statusCode = [response respondsToSelector:@selector(statusCode)] ? [(id)response statusCode] : 0;
        if (statusCode != 304)
            WebCore::adjustMIMETypeIfNecessary(response._CFURLResponse);
        
        WebCore::ResourceResponse resourceResponse(response);
        copyTimingData([dataTask _timingData], resourceResponse.resourceLoadTiming());
        auto completionHandlerCopy = Block_copy(completionHandler);
        networkDataTask->didReceiveResponse(resourceResponse, [completionHandlerCopy, resourceResponse](WebCore::PolicyAction policyAction) {
            completionHandlerCopy(toNSURLSessionResponseDisposition(policyAction));
            Block_release(completionHandlerCopy);
        });
    } else
        completionHandler(NSURLSessionResponseCancel);
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
    auto storedCredentials = session.configuration.URLCredentialStorage ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier(dataTask.taskIdentifier, storedCredentials))
        networkDataTask->didReceiveData(WebCore::SharedBuffer::wrapNSData(data));
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didFinishDownloadingToURL:(NSURL *)location
{
    auto downloadID = _session->takeDownloadID([downloadTask taskIdentifier]);
    if (auto* download = WebKit::NetworkProcess::singleton().downloadManager().download(downloadID))
        download->didFinish();
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didWriteData:(int64_t)bytesWritten totalBytesWritten:(int64_t)totalBytesWritten totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite
{
    auto storedCredentials = session.configuration.URLCredentialStorage ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    ASSERT_WITH_MESSAGE_UNUSED(storedCredentials, !_session->dataTaskForIdentifier([downloadTask taskIdentifier], storedCredentials), "The NetworkDataTask should be destroyed immediately after didBecomeDownloadTask returns");

    auto downloadID = _session->downloadID([downloadTask taskIdentifier]);
    if (auto* download = WebKit::NetworkProcess::singleton().downloadManager().download(downloadID))
        download->didReceiveData(bytesWritten);
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didResumeAtOffset:(int64_t)fileOffset expectedTotalBytes:(int64_t)expectedTotalBytes
{
    notImplemented();
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didBecomeDownloadTask:(NSURLSessionDownloadTask *)downloadTask
{
    auto storedCredentials = session.configuration.URLCredentialStorage ? WebCore::StoredCredentials::AllowStoredCredentials : WebCore::StoredCredentials::DoNotAllowStoredCredentials;
    if (auto* networkDataTask = _session->dataTaskForIdentifier([dataTask taskIdentifier], storedCredentials)) {
        auto downloadID = networkDataTask->pendingDownloadID();
        auto& downloadManager = WebKit::NetworkProcess::singleton().downloadManager();
        auto download = std::make_unique<WebKit::Download>(downloadManager, downloadID, downloadTask);
        networkDataTask->transferSandboxExtensionToDownload(*download);
        ASSERT(WebCore::fileExists(networkDataTask->pendingDownloadLocation()));
        download->didCreateDestination(networkDataTask->pendingDownloadLocation());
        download->didReceiveResponse([downloadTask response]);
        auto pendingDownload = downloadManager.dataTaskBecameDownloadTask(downloadID, WTFMove(download));

        networkDataTask->didBecomeDownload();

        _session->addDownloadID([downloadTask taskIdentifier], downloadID);
    }
}

@end

namespace WebKit {
    
static NSURLSessionConfiguration *configurationForType(NetworkSession::Type type)
{
    switch (type) {
    case NetworkSession::Type::Normal:
        return [NSURLSessionConfiguration defaultSessionConfiguration];
    case NetworkSession::Type::Ephemeral:
        return [NSURLSessionConfiguration ephemeralSessionConfiguration];
    }
}

static RefPtr<CustomProtocolManager>& globalCustomProtocolManager()
{
    static NeverDestroyed<RefPtr<CustomProtocolManager>> customProtocolManager;
    return customProtocolManager.get();
}

void NetworkSession::setCustomProtocolManager(CustomProtocolManager* customProtocolManager)
{
    globalCustomProtocolManager() = customProtocolManager;
}

Ref<NetworkSession> NetworkSession::create(Type type, WebCore::SessionID sessionID, CustomProtocolManager* customProtocolManager, std::unique_ptr<WebCore::NetworkStorageSession> networkStorageSession)
{
    return adoptRef(*new NetworkSession(type, sessionID, customProtocolManager, WTFMove(networkStorageSession)));
}

NetworkSession& NetworkSession::defaultSession()
{
    ASSERT(isMainThread());
    static NetworkSession* session = &NetworkSession::create(NetworkSession::Type::Normal, WebCore::SessionID::defaultSessionID(), globalCustomProtocolManager().get(), nullptr).leakRef();
    return *session;
}

NetworkSession::NetworkSession(Type type, WebCore::SessionID sessionID, CustomProtocolManager* customProtocolManager, std::unique_ptr<WebCore::NetworkStorageSession> networkStorageSession)
    : m_sessionID(sessionID)
    , m_networkStorageSession(WTFMove(networkStorageSession))
{
    relaxAdoptionRequirement();

    NSURLSessionConfiguration *configuration = configurationForType(type);

    if (customProtocolManager)
        customProtocolManager->registerProtocolClass(configuration);
    
#if HAVE(TIMINGDATAOPTIONS)
    configuration._timingDataOptions = _TimingDataOptionsEnableW3CNavigationTiming;
#else
    setCollectsTimingData();
#endif

    if (m_networkStorageSession) {
        ASSERT(type == Type::Ephemeral);
        if (CFHTTPCookieStorageRef storage = m_networkStorageSession->cookieStorage().get())
            configuration.HTTPCookieStorage = [[[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:storage] autorelease];
    } else {
        ASSERT(type == Type::Normal);
        if (CFHTTPCookieStorageRef storage = WebCore::NetworkStorageSession::defaultStorageSession().cookieStorage().get())
            configuration.HTTPCookieStorage = [[[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:storage] autorelease];
    }

    m_sessionWithCredentialStorageDelegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:*this]);
    m_sessionWithCredentialStorage = [NSURLSession sessionWithConfiguration:configuration delegate:static_cast<id>(m_sessionWithCredentialStorageDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];

    configuration.URLCredentialStorage = nil;
    m_sessionWithoutCredentialStorageDelegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:*this]);
    m_sessionWithoutCredentialStorage = [NSURLSession sessionWithConfiguration:configuration delegate:static_cast<id>(m_sessionWithoutCredentialStorageDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
}

NetworkSession::~NetworkSession()
{
}

void NetworkSession::invalidateAndCancel()
{
    [m_sessionWithCredentialStorage invalidateAndCancel];
    [m_sessionWithoutCredentialStorage invalidateAndCancel];
}


WebCore::NetworkStorageSession& NetworkSession::networkStorageSession()
{
    if (!m_networkStorageSession)
        return WebCore::NetworkStorageSession::defaultStorageSession();
    return *m_networkStorageSession;
}

#if !USE(CREDENTIAL_STORAGE_WITH_NETWORK_SESSION)
void NetworkSession::clearCredentials()
{
    ASSERT(m_dataTaskMapWithCredentials.isEmpty());
    ASSERT(m_dataTaskMapWithoutCredentials.isEmpty());
    ASSERT(m_downloadMap.isEmpty());
    m_sessionWithCredentialStorage = [NSURLSession sessionWithConfiguration:m_sessionWithCredentialStorage.get().configuration delegate:static_cast<id>(m_sessionDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
}
#endif

NetworkDataTask* NetworkSession::dataTaskForIdentifier(NetworkDataTask::TaskIdentifier taskIdentifier, WebCore::StoredCredentials storedCredentials)
{
    ASSERT(isMainThread());
    if (storedCredentials == WebCore::StoredCredentials::AllowStoredCredentials)
        return m_dataTaskMapWithCredentials.get(taskIdentifier);
    return m_dataTaskMapWithoutCredentials.get(taskIdentifier);
}

void NetworkSession::addDownloadID(NetworkDataTask::TaskIdentifier taskIdentifier, DownloadID downloadID)
{
#ifndef NDEBUG
    ASSERT(!m_downloadMap.contains(taskIdentifier));
    for (auto idInMap : m_downloadMap.values())
        ASSERT(idInMap != downloadID);
#endif
    m_downloadMap.add(taskIdentifier, downloadID);
}

DownloadID NetworkSession::downloadID(NetworkDataTask::TaskIdentifier taskIdentifier)
{
    ASSERT(m_downloadMap.get(taskIdentifier).downloadID());
    return m_downloadMap.get(taskIdentifier);
}

DownloadID NetworkSession::takeDownloadID(NetworkDataTask::TaskIdentifier taskIdentifier)
{
    auto downloadID = m_downloadMap.take(taskIdentifier);
    return downloadID;
}

}

#endif
