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
#import "TestProtocol.h"

#import <WebKit/WKBrowsingContextController.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#include <MobileCoreServices/MobileCoreServices.h>
#endif

static NSString *testScheme;

@implementation TestProtocol

+ (BOOL)canInitWithRequest:(NSURLRequest *)request
{
    return [request.URL.scheme caseInsensitiveCompare:testScheme] == NSOrderedSame;
}

+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request
{
    return request;
}

+ (BOOL)requestIsCacheEquivalent:(NSURLRequest *)a toRequest:(NSURLRequest *)b
{
    return NO;
}

+ (NSString *)scheme
{
    return testScheme;
}

+ (void)registerWithScheme:(NSString *)scheme
{
    testScheme = [scheme retain];
    [NSURLProtocol registerClass:[self class]];
    [WKBrowsingContextController registerSchemeForCustomProtocol:testScheme];
}

+ (void)unregister
{
    [WKBrowsingContextController unregisterSchemeForCustomProtocol:testScheme];
    [NSURLProtocol unregisterClass:[self class]];
    [testScheme release];
    testScheme = nil;
}

static NSDictionary<NSString *, NSString *> *additionalResponseHeaders;

+ (NSDictionary<NSString *, NSString *> *)additionalResponseHeaders
{
    return additionalResponseHeaders;
}

+ (void)setAdditionalResponseHeaders:(NSDictionary<NSString *, NSString *> *)additionalHeaders
{
    [additionalResponseHeaders autorelease];
    additionalResponseHeaders = [additionalHeaders copy];
}

static NSURL *createRedirectURL(NSString *query)
{
    return [NSURL URLWithString:[NSString stringWithFormat:@"%@://%@", testScheme, query]];
}

static NSString *contentTypeForFileExtension(NSString *fileExtension)
{
    auto identifier = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (__bridge CFStringRef)fileExtension, nullptr));
    auto mimeType = adoptCF(UTTypeCopyPreferredTagWithClass(identifier.get(), kUTTagClassMIMEType));
    return (__bridge NSString *)mimeType.autorelease();
}

- (void)startLoading
{
    NSURL *requestURL = self.request.URL;
    EXPECT_TRUE([requestURL.scheme isEqualToString:testScheme]);

    if ([requestURL.host isEqualToString:@"307-redirect"]) {
        RetainPtr<NSHTTPURLResponse> response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:requestURL statusCode:307 HTTPVersion:@"HTTP/1.1" headerFields:@{@"Content-Type" : @"text/html"}]);
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:createRedirectURL(requestURL.query)];
        request.HTTPMethod = self.request.HTTPMethod;
        [self.client URLProtocol:self wasRedirectedToRequest:request redirectResponse:response.get()];
        return;
    }

    if ([requestURL.host isEqualToString:@"redirect"]) {
        NSData *data = [@"PASS" dataUsingEncoding:NSASCIIStringEncoding];
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:requestURL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil]);
        [self.client URLProtocol:self wasRedirectedToRequest:[NSURLRequest requestWithURL:createRedirectURL(requestURL.query)] redirectResponse:response.get()];
        return;
    }

    NSString *contentType;
    NSData *data;
    if ([requestURL.host isEqualToString:@"bundle-file"]) {
        NSString *fileName = requestURL.lastPathComponent;
        NSString *fileExtension = fileName.pathExtension;
        contentType = contentTypeForFileExtension(fileExtension);
        data = [NSData dataWithContentsOfURL:[NSBundle.mainBundle URLForResource:fileName.stringByDeletingPathExtension withExtension:fileExtension subdirectory:@"TestWebKitAPI.resources"]];
    } else {
        contentType = @"text/html";
        data = [@"PASS" dataUsingEncoding:NSASCIIStringEncoding];
    }

    NSMutableDictionary *responseHeaders = [NSMutableDictionary dictionaryWithCapacity:2];
    responseHeaders[@"Content-Type"] = contentType;
    responseHeaders[@"Content-Length"] = [NSString stringWithFormat:@"%tu", data.length];
    if (additionalResponseHeaders)
        [responseHeaders addEntriesFromDictionary:additionalResponseHeaders];

    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:requestURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:responseHeaders]);

    [self.client URLProtocol:self didReceiveResponse:response.get() cacheStoragePolicy:NSURLCacheStorageNotAllowed];
    [self.client URLProtocol:self didLoadData:data];
    [self.client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading
{
}

@end
