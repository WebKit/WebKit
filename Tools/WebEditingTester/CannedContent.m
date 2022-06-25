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

#import "CannedContent.h"

@implementation CannedContent

- (instancetype)initWithRequest:(NSURLRequest *)request cachedResponse:(NSCachedURLResponse *)cachedResponse client:(id<NSURLProtocolClient>)client
{
    if (!(self = [super initWithRequest:request cachedResponse:cachedResponse client:client]))
        return nil;

    return self;
}

+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request
{
    return request;
}

+ (BOOL)canInitWithRequest:(NSURLRequest *)request
{
    return [request.URL.scheme isEqualToString:@"canned"];
}

- (void)startLoading
{
    NSData *data = nil;
    NSString *MIMEType = nil;

    if ([self.request.URL.host isEqualToString:@"dice"]) {
        data = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"dice" ofType:@"png"]];
        MIMEType = @"image/png";
    } else if ([self.request.URL.host isEqualToString:@"text"]) {
        data = [@"canned text" dataUsingEncoding:NSUTF8StringEncoding];
        MIMEType = @"text/html";
    }

    if (!data) {
        [self.client URLProtocol:self didFailWithError:[NSError errorWithDomain:@"WebEditingTester" code:1 userInfo:nil]];
        return;
    }

    [self.client URLProtocol:self didReceiveResponse:[[NSURLResponse alloc] initWithURL:self.request.URL MIMEType:MIMEType expectedContentLength:data.length textEncodingName:nil] cacheStoragePolicy:NSURLCacheStorageNotAllowed];
    [self.client URLProtocol:self didLoadData:data];
    [self.client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading
{

}

@end
