/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "WKURLSessionTaskDelegate.h"

#import "AuthenticationChallengeDispositionCocoa.h"
#import "Connection.h"
#import "NetworkProcess.h"
#import "NetworkProcessProxyMessages.h"
#import "NetworkSessionCocoa.h"
#import <Foundation/NSURLSession.h>
#import <WebCore/AuthenticationChallenge.h>
#import <WebCore/Credential.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceResponse.h>
#import <wtf/BlockPtr.h>
#import <wtf/SystemTracing.h>
#import <wtf/cocoa/SpanCocoa.h>

@implementation WKURLSessionTaskDelegate {
    Markable<WebKit::DataTaskIdentifier> _identifier;
    WeakPtr<WebKit::NetworkSessionCocoa> _session;
}

- (instancetype)initWithTask:(NSURLSessionTask *)task identifier:(WebKit::DataTaskIdentifier)identifier session:(WebKit::NetworkSessionCocoa&)session
{
    if (!(self = [super init]))
        return nil;

    WTFBeginSignpost(self, DataTask, "%{public}@ %{private}@", task.originalRequest.HTTPMethod, task.originalRequest.URL);
    _identifier = identifier;
    _session = WeakPtr { session };

    return self;
}

- (void)dealloc
{
    WTFEndSignpost(self, DataTask);
    [super dealloc];
}

- (IPC::Connection*)connection
{
    if (!_session)
        return nil;
    return _session->networkProcess().parentProcessConnection();
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    WTFEmitSignpost(self, DataTask, "received challenge");
    RefPtr connection = [self connection];
    if (!connection)
        return completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
    connection->sendWithAsyncReply(Messages::NetworkProcessProxy::DataTaskReceivedChallenge(*_identifier, challenge), [completionHandler = makeBlockPtr(completionHandler)](WebKit::AuthenticationChallengeDisposition disposition, WebCore::Credential&& credential) {
        completionHandler(fromAuthenticationChallengeDisposition(disposition), credential.nsCredential());
    });
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    WTFEmitSignpost(self, DataTask, "redirect");
    RefPtr connection = [self connection];
    if (!connection)
        return completionHandler(nil);
    connection->sendWithAsyncReply(Messages::NetworkProcessProxy::DataTaskWillPerformHTTPRedirection(*_identifier, response, request), [completionHandler = makeBlockPtr(completionHandler), request = RetainPtr { request }] (bool allowed) {
        completionHandler(allowed ? request.get() : nil);
    });
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    WTFEmitSignpost(self, DataTask, "received response headers");
    RefPtr connection = [self connection];
    if (!connection)
        return completionHandler(NSURLSessionResponseCancel);
    connection->sendWithAsyncReply(Messages::NetworkProcessProxy::DataTaskDidReceiveResponse(*_identifier, response), [completionHandler = makeBlockPtr(completionHandler)] (bool allowed) {
        completionHandler(allowed ? NSURLSessionResponseAllow : NSURLSessionResponseCancel);
    });
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
    WTFEmitSignpost(self, DataTask, "received %zu bytes", static_cast<size_t>(data.length));
    RefPtr connection = [self connection];
    if (!connection)
        return;
    connection->send(Messages::NetworkProcessProxy::DataTaskDidReceiveData(*_identifier, span(data)), 0);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error
{
    WTFEmitSignpost(self, DataTask, "completed with error: %d", !!error);
    RefPtr connection = [self connection];
    if (!connection)
        return;
    connection->send(Messages::NetworkProcessProxy::DataTaskDidCompleteWithError(*_identifier, error), 0);
    if (_session)
        _session->removeDataTask(*_identifier);
}

@end
