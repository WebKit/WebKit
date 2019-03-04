/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "WKNavigationDataInternal.h"

#import "WKNSURLExtras.h"
#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceResponse.h>

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
@implementation WKNavigationData {
IGNORE_WARNINGS_END
    API::ObjectStorage<API::NavigationData> _data;
}

- (void)dealloc
{
    _data->~NavigationData();

    [super dealloc];
}

- (NSString *)title
{
    return _data->title();
}

- (NSURLRequest *)originalRequest
{
    return _data->originalRequest().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
}

- (NSURL *)destinationURL
{
    return [NSURL _web_URLWithWTFString:_data->url()];
}

- (NSURLResponse *)response
{
    return _data->response().nsURLResponse();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_data;
}

@end
