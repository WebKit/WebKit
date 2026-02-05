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
#import "WKURLSchemeTask.h"

#import "WKFrameInfoInternal.h"
#import "WKURLSchemeTaskInternal.h"
#import "WebURLSchemeHandler.h"
#import "WebURLSchemeTask.h"
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/SharedBuffer.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/MainThread.h>

static WebKit::WebURLSchemeTask::ExceptionType getExceptionTypeFromMainRunLoop(Function<WebKit::WebURLSchemeTask::ExceptionType ()>&& function)
{
    WebKit::WebURLSchemeTask::ExceptionType exceptionType;
    callOnMainRunLoopAndWait([function = WTF::move(function), &exceptionType] {
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
    case WebKit::WebURLSchemeTask::ExceptionType::WaitingForRedirectCompletionHandler:
        [NSException raise:NSInternalInconsistencyException format:@"No callbacks are allowed while waiting for the redirection completion handler to be invoked"];
        break;
    }
}

static Ref<WebKit::WebURLSchemeTask> protectedURLSchemeTask(WKURLSchemeTaskImpl *urlSchemeTaskImpl)
{
    return *urlSchemeTaskImpl->_urlSchemeTask;
}

@implementation WKURLSchemeTaskImpl

- (instancetype)init
{
    RELEASE_ASSERT_NOT_REACHED();
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKURLSchemeTaskImpl.class, self))
        return;
    SUPPRESS_UNCOUNTED_ARG _urlSchemeTask->WebURLSchemeTask::~WebURLSchemeTask();
    [super dealloc];
}

- (NSURLRequest *)request
{
    return protectedURLSchemeTask(self)->nsRequest();
}

- (BOOL)_requestOnlyIfCached
{
    return protect(protectedURLSchemeTask(self)->nsRequest()).get().cachePolicy == NSURLRequestReturnCacheDataDontLoad;
}

- (void)_willPerformRedirection:(NSURLResponse *)response newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    auto function = [strongSelf = retainPtr(self), self, response = retainPtr(response), request = retainPtr(request), handler = makeBlockPtr(completionHandler)] () mutable {
        return protectedURLSchemeTask(self)->willPerformRedirection(response.get(), request.get(), [handler = WTF::move(handler)] (WebCore::ResourceRequest&& actualNewRequest) {
            handler.get()(actualNewRequest.protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get());
        });
    };

    auto result = getExceptionTypeFromMainRunLoop(WTF::move(function));
    raiseExceptionIfNecessary(result);
}

- (void)didReceiveResponse:(NSURLResponse *)response
{
    auto function = [strongSelf = retainPtr(self), self, response = retainPtr(response)] {
        return protectedURLSchemeTask(self)->didReceiveResponse(response.get());
    };

    auto result = getExceptionTypeFromMainRunLoop(WTF::move(function));
    raiseExceptionIfNecessary(result);
}

- (void)didReceiveData:(NSData *)data
{
    auto function = [strongSelf = retainPtr(self), self, data = retainPtr(data)] () mutable {
        return protectedURLSchemeTask(self)->didReceiveData(WebCore::SharedBuffer::create(data.get()));
    };

    auto result = getExceptionTypeFromMainRunLoop(WTF::move(function));
    raiseExceptionIfNecessary(result);
}

- (void)didFinish
{
    auto function = [strongSelf = retainPtr(self), self] {
        return protectedURLSchemeTask(self)->didComplete({ });
    };

    auto result = getExceptionTypeFromMainRunLoop(WTF::move(function));
    raiseExceptionIfNecessary(result);
}

- (void)didFailWithError:(NSError *)error
{
    auto function = [strongSelf = retainPtr(self), self, error = retainPtr(error)] {
        return protectedURLSchemeTask(self)->didComplete(error.get());
    };

    auto result = getExceptionTypeFromMainRunLoop(WTF::move(function));
    raiseExceptionIfNecessary(result);
}

- (void)_didPerformRedirection:(NSURLResponse *)response newRequest:(NSURLRequest *)request
{
    auto function = [strongSelf = retainPtr(self), self, response = retainPtr(response), request = retainPtr(request)] {
        return protectedURLSchemeTask(self)->didPerformRedirection(response.get(), request.get());
    };

    auto result = getExceptionTypeFromMainRunLoop(WTF::move(function));
    raiseExceptionIfNecessary(result);
}

- (WKFrameInfo *)_frame
{
    return wrapper(_urlSchemeTask->frameInfo());
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_urlSchemeTask;
}

@end
