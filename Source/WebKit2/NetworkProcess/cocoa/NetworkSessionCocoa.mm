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

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    UNUSED_PARAM(session);

    if (auto* networkingTask = _session->dataTaskForIdentifier(task.taskIdentifier)) {
        if (auto* client = networkingTask->client()) {
            auto completionHandlerCopy = Block_copy(completionHandler);
            client->willPerformHTTPRedirection(response, request, [completionHandlerCopy](const WebCore::ResourceRequest& request)
                {
                    completionHandlerCopy(request.nsURLRequest(WebCore::UpdateHTTPBody));
                    Block_release(completionHandlerCopy);
                }
            );
        }
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    UNUSED_PARAM(session);

    if (auto* networkingTask = _session->dataTaskForIdentifier(task.taskIdentifier)) {
        if (auto* client = networkingTask->client()) {
            auto completionHandlerCopy = Block_copy(completionHandler);
            client->didReceiveChallenge(challenge, [completionHandlerCopy](WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential)
                {
                    completionHandlerCopy(toNSURLSessionAuthChallengeDisposition(disposition), credential.nsCredential());
                    Block_release(completionHandlerCopy);
                }
            );
        }
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error
{
    UNUSED_PARAM(session);

    if (auto* networkingTask = _session->dataTaskForIdentifier(task.taskIdentifier)) {
        if (auto* client = networkingTask->client())
            client->didCompleteWithError(error);
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    UNUSED_PARAM(session);

    if (auto* networkingTask = _session->dataTaskForIdentifier(dataTask.taskIdentifier)) {
        if (auto* client = networkingTask->client()) {
            ASSERT(isMainThread());
            WebCore::ResourceResponse resourceResponse(response);
            copyTimingData([dataTask _timingData], resourceResponse.resourceLoadTiming());
            auto completionHandlerCopy = Block_copy(completionHandler);
            client->didReceiveResponse(resourceResponse, [completionHandlerCopy](WebCore::PolicyAction policyAction)
                {
                    completionHandlerCopy(toNSURLSessionResponseDisposition(policyAction));
                    Block_release(completionHandlerCopy);
                }
            );
        }
    }
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
    UNUSED_PARAM(session);

    if (auto* networkingTask = _session->dataTaskForIdentifier(dataTask.taskIdentifier)) {
        if (auto* client = networkingTask->client())
            client->didReceiveData(WebCore::SharedBuffer::wrapNSData(data));
    }
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didFinishDownloadingToURL:(NSURL *)location
{
    notImplemented();
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didWriteData:(int64_t)bytesWritten totalBytesWritten:(int64_t)totalBytesWritten totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite
{
    ASSERT_WITH_MESSAGE(!_session->dataTaskForIdentifier([downloadTask taskIdentifier]), "The NetworkDataTask should be destroyed immediately after didBecomeDownloadTask returns");
    notImplemented();
}

- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)downloadTask didResumeAtOffset:(int64_t)fileOffset expectedTotalBytes:(int64_t)expectedTotalBytes
{
    notImplemented();
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didBecomeDownloadTask:(NSURLSessionDownloadTask *)downloadTask
{
    if (auto* networkDataTask = _session->dataTaskForIdentifier([dataTask taskIdentifier])) {
        auto downloadID = networkDataTask->downloadID();
        auto& downloadManager = WebKit::NetworkProcess::singleton().downloadManager();
        auto download = std::make_unique<WebKit::Download>(downloadManager, *_session, downloadID);
        download->didStart([downloadTask currentRequest]);
        downloadManager.dataTaskBecameDownloadTask(downloadID, WTFMove(download));

        if (auto* client = networkDataTask->client())
            client->didBecomeDownload();
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
    : m_sessionID(sessionID)
{
    m_sessionDelegate = adoptNS([[WKNetworkSessionDelegate alloc] initWithNetworkSession:*this]);

    NSURLSessionConfiguration *configuration = configurationForType(type);

#if HAVE(TIMINGDATAOPTIONS)
    configuration._timingDataOptions = _TimingDataOptionsEnableW3CNavigationTiming;
#else
    setCollectsTimingData();
#endif

    if (auto* storageSession = SessionTracker::storageSession(sessionID)) {
        if (CFHTTPCookieStorageRef storage = storageSession->cookieStorage().get())
            configuration.HTTPCookieStorage = [[[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:storage] autorelease];
    }
    m_session = [NSURLSession sessionWithConfiguration:configuration delegate:static_cast<id>(m_sessionDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
}

NetworkSession::~NetworkSession()
{
    [m_session invalidateAndCancel];
}

Ref<NetworkDataTask> NetworkSession::createDataTaskWithRequest(const WebCore::ResourceRequest& request, NetworkSessionTaskClient& client)
{
    return adoptRef(*new NetworkDataTask(*this, client, [m_session dataTaskWithRequest:request.nsURLRequest(WebCore::UpdateHTTPBody)]));
}

NetworkDataTask* NetworkSession::dataTaskForIdentifier(NetworkDataTask::TaskIdentifier taskIdentifier)
{
    ASSERT(isMainThread());
    return m_dataTaskMap.get(taskIdentifier);
}

NetworkDataTask::NetworkDataTask(NetworkSession& session, NetworkSessionTaskClient& client, RetainPtr<NSURLSessionDataTask>&& task)
    : m_session(session)
    , m_client(&client)
    , m_task(WTFMove(task))
{
    ASSERT(!m_session.m_dataTaskMap.contains(taskIdentifier()));
    ASSERT(isMainThread());
    m_session.m_dataTaskMap.add(taskIdentifier(), this);
}

NetworkDataTask::~NetworkDataTask()
{
    ASSERT(m_session.m_dataTaskMap.contains(taskIdentifier()));
    ASSERT(m_session.m_dataTaskMap.get(taskIdentifier()) == this);
    ASSERT(isMainThread());
    m_session.m_dataTaskMap.remove(taskIdentifier());
}

void NetworkDataTask::cancel()
{
    [m_task cancel];
}

void NetworkDataTask::resume()
{
    [m_task resume];
}

auto NetworkDataTask::taskIdentifier() -> TaskIdentifier
{
    return [m_task taskIdentifier];
}

}

#endif
