/*
 * Copyright (C) 2007, Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "ResourceLoadDelegate.h"
#import "DumpRenderTree.h"

#import <WebKit/WebKit.h>

@interface NSURL (DRTExtras)
- (NSString *)_drt_descriptionSuitableForTestResult;
@end

@interface NSError (DRTExtras)
- (NSString *)_drt_descriptionSuitableForTestResult;
@end

@interface NSURLResponse (DRTExtras)
- (NSString *)_drt_descriptionSuitableForTestResult;
@end

@interface NSURLRequest (DRTExtras)
- (NSString *)_drt_descriptionSuitableForTestResult;
@end

@implementation NSError (DRTExtras)
- (NSString *)_drt_descriptionSuitableForTestResult 
{
    NSString *str = [NSString stringWithFormat:@"<NSError domain %@, code %d", [self domain], [self code]];
    NSURL *failingURL;
    
    if ((failingURL = [[self userInfo] objectForKey:@"NSErrorFailingURLKey"]))
        str = [str stringByAppendingFormat:@", failing URL \"%@\"", [failingURL _drt_descriptionSuitableForTestResult]];
        
    str = [str stringByAppendingFormat:@">"];
    
    return str;
}

@end

@implementation NSURL (DRTExtras)

- (NSString *)_drt_descriptionSuitableForTestResult 
{
    if (![self isFileURL])
        return [self description];

    WebDataSource *dataSource = [frame dataSource];
    if (!dataSource)
        dataSource = [frame provisionalDataSource];
    
    NSString *basePath = [[[[dataSource request] URL] path] stringByDeletingLastPathComponent];
    
    return [[self path] substringFromIndex:[basePath length] + 1];
}

@end

@implementation NSURLResponse (DRTExtras)

- (NSString *)_drt_descriptionSuitableForTestResult
{
    return [NSString stringWithFormat:@"<NSURLResponse %@>", [[self URL] _drt_descriptionSuitableForTestResult]];
}

@end

@implementation NSURLRequest (DRTExtras)

- (NSString *)_drt_descriptionSuitableForTestResult
{
    return [NSString stringWithFormat:@"<NSURLRequest %@>", [[self URL] _drt_descriptionSuitableForTestResult]];
}

@end

@implementation ResourceLoadDelegate

- webView: (WebView *)wv identifierForInitialRequest: (NSURLRequest *)request fromDataSource: (WebDataSource *)dataSource
{
    if (shouldDumpResourceLoadCallbacks && !done)
        return [[request URL] _drt_descriptionSuitableForTestResult];
    
    return @"<unknown>";
}

-(NSURLRequest *)webView: (WebView *)wv resource:identifier willSendRequest: (NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse fromDataSource:(WebDataSource *)dataSource
{
    if (shouldDumpResourceLoadCallbacks && !done) {
        NSString *string = [NSString stringWithFormat:@"%@ - willSendRequest %@ redirectResponse %@", identifier, [newRequest _drt_descriptionSuitableForTestResult],
            [redirectResponse _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }    
    
    if ([disallowedURLs containsObject:[newRequest URL]])
        return nil;
    
    return newRequest;
}

- (void)webView:(WebView *)wv resource:(id)identifier didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge fromDataSource:(WebDataSource *)dataSource
{
}

- (void)webView:(WebView *)wv resource:(id)identifier didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge fromDataSource:(WebDataSource *)dataSource
{
}

-(void)webView: (WebView *)wv resource:identifier didReceiveResponse: (NSURLResponse *)response fromDataSource:(WebDataSource *)dataSource
{
    if (shouldDumpResourceLoadCallbacks && !done) {
        NSString *string = [NSString stringWithFormat:@"%@ - didReceiveResponse %@", identifier, [response _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }    
}

-(void)webView: (WebView *)wv resource:identifier didReceiveContentLength: (unsigned)length fromDataSource:(WebDataSource *)dataSource
{
}

-(void)webView: (WebView *)wv resource:identifier didFinishLoadingFromDataSource:(WebDataSource *)dataSource
{
    if (shouldDumpResourceLoadCallbacks && !done) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFinishLoading", identifier];
        printf ("%s\n", [string UTF8String]);
    }
}

-(void)webView: (WebView *)wv resource:identifier didFailLoadingWithError:(NSError *)error fromDataSource:(WebDataSource *)dataSource
{
    if (shouldDumpResourceLoadCallbacks && !done) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFailLoadingWithError: %@", identifier, [error _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
}

- (void)webView: (WebView *)wv plugInFailedWithError:(NSError *)error dataSource:(WebDataSource *)dataSource
{
}

@end
