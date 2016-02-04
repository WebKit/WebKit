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
#import "Download.h"
#import "NetworkLoad.h"
#import "NetworkProcess.h"
#import "SessionTracker.h"
#import <Foundation/NSURLSession.h>
#import <WebCore/AuthenticationChallenge.h>
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
    WebKit::NetworkSession* _session;
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

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didSendBodyData:(int64_t)bytesSent totalBytesSent:(int64_t)totalBytesSent totalBytesExpectedToSend:(int64_t)totalBytesExpectedToSend
{
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier))
        networkDataTask->client().didSendData(totalBytesSent, totalBytesExpectedToSend);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier)) {
        auto completionHandlerCopy = Block_copy(completionHandler);
        networkDataTask->client().willPerformHTTPRedirection(response, request, [completionHandlerCopy](const WebCore::ResourceRequest& request) {
            completionHandlerCopy(request.nsURLRequest(WebCore::UpdateHTTPBody));
            Block_release(completionHandlerCopy);
        });
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier)) {
        auto completionHandlerCopy = Block_copy(completionHandler);
        auto challengeCompletionHandler = [completionHandlerCopy](WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential)
        {
            completionHandlerCopy(toNSURLSessionAuthChallengeDisposition(disposition), credential.nsCredential());
            Block_release(completionHandlerCopy);
        };
        
        if (networkDataTask->tryPasswordBasedAuthentication(challenge, challengeCompletionHandler))
            return;
        
        networkDataTask->client().didReceiveChallenge(challenge, challengeCompletionHandler);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error
{
    if (auto* networkDataTask = _session->dataTaskForIdentifier(task.taskIdentifier))
        networkDataTask->client().didCompleteWithError(error);
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    if (auto* networkDataTask = _session->dataTaskForIdentifier(dataTask.taskIdentifier)) {
        ASSERT(isMainThread());
        WebCore::ResourceResponse resourceResponse(response);
        copyTimingData([dataTask _timingData], resourceResponse.resourceLoadTiming());
        auto completionHandlerCopy = Block_copy(completionHandler);
        networkDataTask->client().didReceiveResponse(resourceResponse, [completionHandlerCopy](WebCore::PolicyAction policyAction) {
            completionHandlerCopy(toNSURLSessionResponseDisposition(policyAction));
            Block_release(completionHandlerCopy);
        });
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
    if (auto* networkDataTask = _session->dataTaskForIdentifier(dataTask.taskIdentifier))
        networkDataTask->client().didReceiveData(WebCore::SharedBuffer::wrapNSData(data));
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didFinishDownloadingToURL:(NSURL *)location
{
    auto downloadID = _session->takeDownloadID([downloadTask taskIdentifier]);
    notImplemented();
    if (auto* download = WebKit::NetworkProcess::singleton().downloadManager().download(downloadID))
        download->didFinish();
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didWriteData:(int64_t)bytesWritten totalBytesWritten:(int64_t)totalBytesWritten totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite
{
    ASSERT_WITH_MESSAGE(!_session->dataTaskForIdentifier([downloadTask taskIdentifier]), "The NetworkDataTask should be destroyed immediately after didBecomeDownloadTask returns");

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
    if (auto* networkDataTask = _session->dataTaskForIdentifier([dataTask taskIdentifier])) {
        auto downloadID = networkDataTask->pendingDownloadID();
        auto& downloadManager = WebKit::NetworkProcess::singleton().downloadManager();
        auto download = std::make_unique<WebKit::Download>(downloadManager, downloadID);
        download->didStart([downloadTask currentRequest]);
        download->didReceiveResponse([downloadTask response]);
        auto pendingDownload = downloadManager.dataTaskBecameDownloadTask(downloadID, WTFMove(download));

        networkDataTask->client().didBecomeDownload();

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

NetworkSession& NetworkSession::defaultSession()
{
    ASSERT(isMainThread());
    static NeverDestroyed<NetworkSession> session(NetworkSession::Type::Normal, WebCore::SessionID::defaultSessionID());
    return session;
}

NetworkSession::NetworkSession(Type type, WebCore::SessionID sessionID)
{
    m_sessionDelegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:*this]);

    NSURLSessionConfiguration *configuration = configurationForType(type);

    if (auto* customProtocolManager = NetworkProcess::singleton().supplement<CustomProtocolManager>())
        customProtocolManager->registerProtocolClass(configuration);
    
#if HAVE(TIMINGDATAOPTIONS)
    configuration._timingDataOptions = _TimingDataOptionsEnableW3CNavigationTiming;
#else
    setCollectsTimingData();
#endif

    if (auto* storageSession = SessionTracker::storageSession(sessionID)) {
        if (CFHTTPCookieStorageRef storage = storageSession->cookieStorage().get())
            configuration.HTTPCookieStorage = [[[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:storage] autorelease];
    }
    m_sessionWithCredentialStorage = [NSURLSession sessionWithConfiguration:configuration delegate:static_cast<id>(m_sessionDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
    configuration.URLCredentialStorage = nil;
    m_sessionWithoutCredentialStorage = [NSURLSession sessionWithConfiguration:configuration delegate:static_cast<id>(m_sessionDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
}

NetworkSession::~NetworkSession()
{
    [m_sessionWithCredentialStorage invalidateAndCancel];
    [m_sessionWithoutCredentialStorage invalidateAndCancel];
}

NetworkDataTask* NetworkSession::dataTaskForIdentifier(NetworkDataTask::TaskIdentifier taskIdentifier)
{
    ASSERT(isMainThread());
    return m_dataTaskMap.get(taskIdentifier);
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
    ASSERT(downloadID.downloadID());
    return downloadID;
}

NetworkDataTask::NetworkDataTask(NetworkSession& session, NetworkSessionTaskClient& client, const WebCore::ResourceRequest& requestWithCredentials, WebCore::StoredCredentials storedCredentials)
    : m_failureTimer(*this, &NetworkDataTask::failureTimerFired)
    , m_session(session)
    , m_client(client)
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

    if (storedCredentials == WebCore::AllowStoredCredentials)
        m_task = [m_session.m_sessionWithCredentialStorage dataTaskWithRequest:request.nsURLRequest(WebCore::UpdateHTTPBody)];
    else
        m_task = [m_session.m_sessionWithoutCredentialStorage dataTaskWithRequest:request.nsURLRequest(WebCore::UpdateHTTPBody)];
    
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
        client().wasBlocked();
        return;
    case InvalidURLFailure:
        m_scheduledFailureType = NoFailure;
        client().cannotShowURL();
        return;
    case NoFailure:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
}

bool NetworkDataTask::tryPasswordBasedAuthentication(const WebCore::AuthenticationChallenge& challenge, ChallengeCompletionHandler completionHandler)
{
    if (!challenge.protectionSpace().isPasswordBased())
        return false;

    if (!m_user.isNull() && !m_password.isNull()) {
        completionHandler(AuthenticationChallengeDisposition::UseCredential, WebCore::Credential(m_user, m_password, WebCore::CredentialPersistenceForSession));
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

}

#endif
