/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#import "WKURLSchemeTaskInternal.h"

#import "WebURLSchemeHandler.h"
#import "WebURLSchemeTask.h"
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/SharedBuffer.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>

static WebKit::WebURLSchemeTask::ExceptionType getExceptionTypeFromMainRunLoop(Function<WebKit::WebURLSchemeTask::ExceptionType ()>&& function)
{
    if (RunLoop::isMain())
        return function();

    WebKit::WebURLSchemeTask::ExceptionType exceptionType;
    callOnMainRunLoopAndWait([function = WTFMove(function), &exceptionType] {
        exceptionType = function();
    });

    return exceptionType;
}

static void raiseExceptionIfNecessary(WebKit::WebURLSchemeTask::ExceptionType exceptionType)
{
    switch (exceptionType) {
    case WebKit::WebURLSchemeTask::ExceptionType::None:
        return;
    case WebKit::WebURLSchemeTask::ExceptionType::TaskAlreadyStopped:
        [NSException raise:NSInternalInconsistencyException format:@"This task has already been stopped"];
        break;
    case WebKit::WebURLSchemeTask::ExceptionType::CompleteAlreadyCalled:
        [NSException raise:NSInternalInconsistencyException format:@"[WKURLSchemeTask taskDidCompleteWithError:] has already been called for this task"];
        break;
    case WebKit::WebURLSchemeTask::ExceptionType::DataAlreadySent:
        [NSException raise:NSInternalInconsistencyException format:@"[WKURLSchemeTask taskDidReceiveData:] has already been called for this task"];
        break;
    case WebKit::WebURLSchemeTask::ExceptionType::NoResponseSent:
        [NSException raise:NSInternalInconsistencyException format:@"No response has been sent for this task"];
        break;
    case WebKit::WebURLSchemeTask::ExceptionType::RedirectAfterResponse:
        [NSException raise:NSInternalInconsistencyException format:@"No redirects are allowed after the response"];
        break;
    }
}

@implementation WKURLSchemeTaskImpl

- (void)dealloc
{
    auto func = [task = WTFMove(_urlSchemeTask)] () mutable {
        task->API::URLSchemeTask::~URLSchemeTask();
    };

    if (RunLoop::isMain())
        func();
    else
        callOnMainRunLoop(WTFMove(func));

    [super dealloc];
}

- (NSURLRequest *)request
{
    return _urlSchemeTask->task().nsRequest();
}

- (BOOL)_requestOnlyIfCached
{
    return _urlSchemeTask->task().nsRequest().cachePolicy == NSURLRequestReturnCacheDataDontLoad;
}

- (void)didReceiveResponse:(NSURLResponse *)response
{
    auto function = [protectedSelf = retainPtr(self), self, protectedResponse = retainPtr(response), response] {
        return _urlSchemeTask->task().didReceiveResponse(response);
    };

    auto result = getExceptionTypeFromMainRunLoop(WTFMove(function));
    raiseExceptionIfNecessary(result);
}

- (void)didReceiveData:(NSData *)data
{
    auto function = [protectedSelf = retainPtr(self), self, protectedData = retainPtr(data), data] () mutable {
        return _urlSchemeTask->task().didReceiveData(WebCore::SharedBuffer::create(data));
    };

    auto result = getExceptionTypeFromMainRunLoop(WTFMove(function));
    raiseExceptionIfNecessary(result);
}

- (void)didFinish
{
    auto function = [protectedSelf = retainPtr(self), self] {
        return _urlSchemeTask->task().didComplete({ });
    };

    auto result = getExceptionTypeFromMainRunLoop(WTFMove(function));
    raiseExceptionIfNecessary(result);
}

- (void)didFailWithError:(NSError *)error
{
    auto function = [protectedSelf = retainPtr(self), self, protectedError = retainPtr(error), error] {
        return _urlSchemeTask->task().didComplete(error);
    };

    auto result = getExceptionTypeFromMainRunLoop(WTFMove(function));
    raiseExceptionIfNecessary(result);
}

- (void)_didPerformRedirection:(NSURLResponse *)response newRequest:(NSURLRequest *)request
{
    auto function = [protectedSelf = retainPtr(self), self, protectedResponse = retainPtr(response), response, protectedRequest = retainPtr(request), request] {
        return _urlSchemeTask->task().didPerformRedirection(response, request);
    };

    auto result = getExceptionTypeFromMainRunLoop(WTFMove(function));
    raiseExceptionIfNecessary(result);
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_urlSchemeTask;
}

@end
