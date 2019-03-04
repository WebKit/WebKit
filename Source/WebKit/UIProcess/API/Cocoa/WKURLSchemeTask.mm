/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
    _urlSchemeTask->API::URLSchemeTask::~URLSchemeTask();

    [super dealloc];
}

- (NSURLRequest *)request
{
    return _urlSchemeTask->task().request().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
}

- (void)didReceiveResponse:(NSURLResponse *)response
{
    auto result = _urlSchemeTask->task().didReceiveResponse(response);
    raiseExceptionIfNecessary(result);
}

- (void)didReceiveData:(NSData *)data
{
    auto result = _urlSchemeTask->task().didReceiveData(WebCore::SharedBuffer::create(data));
    raiseExceptionIfNecessary(result);
}

- (void)didFinish
{
    auto result = _urlSchemeTask->task().didComplete({ });
    raiseExceptionIfNecessary(result);
}

- (void)didFailWithError:(NSError *)error
{
    auto result = _urlSchemeTask->task().didComplete(error);
    raiseExceptionIfNecessary(result);
}

- (void)_didPerformRedirection:(NSURLResponse *)response newRequest:(NSURLRequest *)request
{
    auto result = _urlSchemeTask->task().didPerformRedirection(response, request);
    raiseExceptionIfNecessary(result);
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_urlSchemeTask;
}

@end
