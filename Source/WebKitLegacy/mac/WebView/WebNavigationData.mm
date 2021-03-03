/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WebNavigationData.h"
#import <wtf/RetainPtr.h>

@interface WebNavigationDataPrivate : NSObject
{
@public
    RetainPtr<NSString> url;
    RetainPtr<NSString> title;
    RetainPtr<NSURLRequest> originalRequest;
    RetainPtr<NSURLResponse> response;
    BOOL hasSubstituteData;
    RetainPtr<NSString> clientRedirectSource;
}

@end

@implementation WebNavigationDataPrivate

@end

@implementation WebNavigationData

- (id)initWithURLString:(NSString *)url title:(NSString *)title originalRequest:(NSURLRequest *)request response:(NSURLResponse *)response hasSubstituteData:(BOOL)hasSubstituteData clientRedirectSource:(NSString *)redirectSource
{
    self = [super init];
    if (!self)
        return nil;
    _private = [[WebNavigationDataPrivate alloc] init];
    
    _private->url = url;
    _private->title = title;
    _private->originalRequest = request;
    _private->response = response;
    _private->hasSubstituteData = hasSubstituteData;
    _private->clientRedirectSource = redirectSource;
    
    return self;
}

- (NSString *)url
{
    return _private->url.get();
}

- (NSString *)title
{
    return _private->title.get();
}

- (NSURLRequest *)originalRequest
{
    return _private->originalRequest.get();
}

- (NSURLResponse *)response
{
    return _private->response.get();
}

- (BOOL)hasSubstituteData
{
    return _private->hasSubstituteData;
}

- (NSString *)clientRedirectSource
{
    return _private->clientRedirectSource.get();
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

@end
