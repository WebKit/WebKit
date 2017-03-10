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
#import "WKURLSchemeHandlerTaskInternal.h"

#if WK_API_ENABLED

#include "WebURLSchemeHandlerTask.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>

using namespace WebCore;

static void raiseExceptionIfNecessary(WebKit::WebURLSchemeHandlerTask::ExceptionType exceptionType)
{
    switch (exceptionType) {
    case WebKit::WebURLSchemeHandlerTask::ExceptionType::None:
        return;
    case WebKit::WebURLSchemeHandlerTask::ExceptionType::TaskAlreadyStopped:
        [NSException raise:NSInternalInconsistencyException format:@"This task has already been stopped"];
        break;
    case WebKit::WebURLSchemeHandlerTask::ExceptionType::CompleteAlreadyCalled:
        [NSException raise:NSInternalInconsistencyException format:@"[WKURLSchemeHandlerTask taskDidCompleteWithError:] has already been called for this task"];
        break;
    case WebKit::WebURLSchemeHandlerTask::ExceptionType::DataAlreadySent:
        [NSException raise:NSInternalInconsistencyException format:@"[WKURLSchemeHandlerTask taskDidReceiveData:] has already been called for this task"];
        break;
    case WebKit::WebURLSchemeHandlerTask::ExceptionType::NoResponseSent:
        [NSException raise:NSInternalInconsistencyException format:@"No response has been sent for this task"];
        break;
    }
}

@implementation WKURLSchemeHandlerTaskImpl

- (NSURLRequest *)request
{
    return _urlSchemeHandlerTask->task().request().nsURLRequest(DoNotUpdateHTTPBody);
}

- (void)didReceiveResponse:(NSURLResponse *)response
{
    auto result = _urlSchemeHandlerTask->task().didReceiveResponse(response);
    raiseExceptionIfNecessary(result);
}

- (void)didReceiveData:(NSData *)data
{
    auto result = _urlSchemeHandlerTask->task().didReceiveData(WebCore::SharedBuffer::wrapNSData(data));
    raiseExceptionIfNecessary(result);
}

- (void)didFinish
{
    auto result = _urlSchemeHandlerTask->task().didComplete({ });
    raiseExceptionIfNecessary(result);
}

- (void)didFailWithError:(NSError *)error
{
    auto result = _urlSchemeHandlerTask->task().didComplete(error);
    raiseExceptionIfNecessary(result);
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_urlSchemeHandlerTask;
}

@end

#endif // #if WK_API_ENABLED
