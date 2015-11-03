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

#import "SessionTracker.h"
#import <Foundation/NSURLSession.h>
#import <WebCore/AuthenticationChallenge.h>
#import <WebCore/CFNetworkSPI.h>
#import <WebCore/Credential.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/SharedBuffer.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

static NSURLSessionResponseDisposition toNSURLSessionResponseDisposition(WebKit::ResponseDisposition disposition)
{
    switch (disposition) {
    case WebKit::ResponseDisposition::Cancel:
        return NSURLSessionResponseCancel;
    case WebKit::ResponseDisposition::Allow:
        return NSURLSessionResponseAllow;
    case WebKit::ResponseDisposition::BecomeDownload:
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

@interface NetworkSessionDelegate : NSObject <NSURLSessionDataDelegate> {
    WebKit::NetworkSession* _session;
}

- (id)initWithNetworkSession:(WebKit::NetworkSession&)session;

@end

@implementation NetworkSessionDelegate

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
            client->willPerformHTTPRedirection(response, request, ^(const WebCore::ResourceRequest& request)
                {
                    completionHandler(request.nsURLRequest(WebCore::UpdateHTTPBody));
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
            client->didReceiveChallenge(challenge, ^(WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential)
                {
                    completionHandler(toNSURLSessionAuthChallengeDisposition(disposition), credential.nsCredential());
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
            client->didReceiveResponse(resourceResponse, ^(WebKit::ResponseDisposition responseDisposition)
                {
                    completionHandler(toNSURLSessionResponseDisposition(responseDisposition));
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

@end

namespace WebKit {
    
Ref<NetworkSession> NetworkSession::create(Type type)
{
    return adoptRef(*new NetworkSession(type));
}

Ref<NetworkSession> NetworkSession::singleton()
{
    static NeverDestroyed<Ref<NetworkSession>> sharedInstance(NetworkSession::create(Type::Normal));
    return sharedInstance.get().copyRef();
}
    
static NSURLSessionConfiguration *configurationForType(NetworkSession::Type type)
{
    switch (type) {
    case NetworkSession::Type::Normal:
        return [NSURLSessionConfiguration defaultSessionConfiguration];
    case NetworkSession::Type::Ephemeral:
        return [NSURLSessionConfiguration ephemeralSessionConfiguration];
    }
}

NetworkSession::NetworkSession(Type type)
{
    m_sessionDelegate = adoptNS([[NetworkSessionDelegate alloc] initWithNetworkSession:*this]);

    NSURLSessionConfiguration *configuration = configurationForType(type);
    // FIXME: Use SessionTracker to make sure the correct cookie storage is used once there is more than one NetworkSession.
    if (auto* session = SessionTracker::session(WebCore::SessionID::defaultSessionID())) {
        if (CFHTTPCookieStorageRef storage = session->cookieStorage().get())
            configuration.HTTPCookieStorage = [[[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:storage] autorelease];
    }
    m_session = [NSURLSession sessionWithConfiguration:configuration delegate:static_cast<id>(m_sessionDelegate.get()) delegateQueue:[NSOperationQueue mainQueue]];
}

Ref<NetworkingDataTask> NetworkSession::createDataTaskWithRequest(const WebCore::ResourceRequest& request, NetworkSessionTaskClient& client)
{
    return adoptRef(*new NetworkingDataTask(*this, client, [m_session dataTaskWithRequest:request.nsURLRequest(WebCore::UpdateHTTPBody)]));
}

NetworkingDataTask* NetworkSession::dataTaskForIdentifier(uint64_t taskIdentifier)
{
    ASSERT(isMainThread());
    return m_dataTaskMap.get(taskIdentifier);
}

NetworkingDataTask::NetworkingDataTask(NetworkSession& session, NetworkSessionTaskClient& client, RetainPtr<NSURLSessionDataTask> task)
    : m_session(session)
    , m_task(task)
    , m_client(&client)
{
    ASSERT(!m_session.m_dataTaskMap.contains(taskIdentifier()));
    ASSERT(isMainThread());
    m_session.m_dataTaskMap.add(taskIdentifier(), this);
}

NetworkingDataTask::~NetworkingDataTask()
{
    ASSERT(m_session.m_dataTaskMap.contains(taskIdentifier()));
    ASSERT(m_session.m_dataTaskMap.get(taskIdentifier()) == this);
    ASSERT(isMainThread());
    m_session.m_dataTaskMap.remove(taskIdentifier());
}

void NetworkingDataTask::suspend()
{
    [m_task suspend];
}

void NetworkingDataTask::resume()
{
    [m_task resume];
}

uint64_t NetworkingDataTask::taskIdentifier()
{
    return [m_task taskIdentifier];
}

}

#endif
