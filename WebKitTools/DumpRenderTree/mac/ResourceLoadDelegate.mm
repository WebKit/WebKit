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

#import "config.h"
#import "ResourceLoadDelegate.h"

#import "DumpRenderTree.h"
#import "LayoutTestController.h"
#import <WebKit/WebKit.h>
#import <WebKit/WebTypesInternal.h>
#import <wtf/Assertions.h>

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
        return [self absoluteString];

    WebDataSource *dataSource = [mainFrame dataSource];
    if (!dataSource)
        dataSource = [mainFrame provisionalDataSource];

    NSString *basePath = [[[[dataSource request] URL] path] stringByDeletingLastPathComponent];

    return [[self path] substringFromIndex:[basePath length] + 1];
}

@end

@implementation NSURLResponse (DRTExtras)

- (NSString *)_drt_descriptionSuitableForTestResult
{
    int statusCode = 0;
    if ([self isKindOfClass:[NSHTTPURLResponse class]])
        statusCode = [(NSHTTPURLResponse *)self statusCode];
    return [NSString stringWithFormat:@"<NSURLResponse %@, http status code %i>", [[self URL] _drt_descriptionSuitableForTestResult], statusCode];
}

@end

@implementation NSURLRequest (DRTExtras)

- (NSString *)_drt_descriptionSuitableForTestResult
{
    NSString *httpMethod = [self HTTPMethod];
    if (!httpMethod)
        httpMethod = @"(none)";
    return [NSString stringWithFormat:@"<NSURLRequest URL %@, main document URL %@, http method %@>", [[self URL] _drt_descriptionSuitableForTestResult], [[self mainDocumentURL] _drt_descriptionSuitableForTestResult], httpMethod];
}

@end

@implementation ResourceLoadDelegate

- webView: (WebView *)wv identifierForInitialRequest: (NSURLRequest *)request fromDataSource: (WebDataSource *)dataSource
{
    ASSERT([[dataSource webFrame] dataSource] || [[dataSource webFrame] provisionalDataSource]);

    if (!done && gLayoutTestController->dumpResourceLoadCallbacks())
        return [[request URL] _drt_descriptionSuitableForTestResult];

    return @"<unknown>";
}

-(NSURLRequest *)webView: (WebView *)wv resource:identifier willSendRequest: (NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse fromDataSource:(WebDataSource *)dataSource
{
    if (!done && gLayoutTestController->dumpResourceLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - willSendRequest %@ redirectResponse %@", identifier, [newRequest _drt_descriptionSuitableForTestResult],
            [redirectResponse _drt_descriptionSuitableForTestResult]];
        printf("%s\n", [string UTF8String]);
    }

    if (!done && gLayoutTestController->willSendRequestReturnsNull())
        return nil;

    if (!done && gLayoutTestController->willSendRequestReturnsNullOnRedirect() && redirectResponse) {
        printf("Returning null for this redirect\n");
        return nil;
    }

    NSURL *url = [newRequest URL];
    NSString *host = [url host];
    if (host
        && (NSOrderedSame == [[url scheme] caseInsensitiveCompare:@"http"] || NSOrderedSame == [[url scheme] caseInsensitiveCompare:@"https"])
        && NSOrderedSame != [host compare:@"127.0.0.1"]
        && NSOrderedSame != [host compare:@"255.255.255.255"] // used in some tests that expect to get back an error
        && NSOrderedSame != [host caseInsensitiveCompare:@"localhost"]) {
        printf("Blocked access to external URL %s\n", [[url absoluteString] cStringUsingEncoding:NSUTF8StringEncoding]);
        return nil;
    }

    if (disallowedURLs && CFSetContainsValue(disallowedURLs, url))
        return nil;

    return newRequest;
}

- (void)webView:(WebView *)wv resource:(id)identifier didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge fromDataSource:(WebDataSource *)dataSource
{
    if (!gLayoutTestController->handlesAuthenticationChallenges())
        return;
    
    const char* user = gLayoutTestController->authenticationUsername().c_str();
    NSString *nsUser = [NSString stringWithFormat:@"%s", user ? user : ""];

    const char* password = gLayoutTestController->authenticationPassword().c_str();
    NSString *nsPassword = [NSString stringWithFormat:@"%s", password ? password : ""];

    NSString *string = [NSString stringWithFormat:@"%@ - didReceiveAuthenticationChallenge - Responding with %@:%@", identifier, nsUser, nsPassword];
    printf("%s\n", [string UTF8String]);
    
    [[challenge sender] useCredential:[NSURLCredential credentialWithUser:nsUser password:nsPassword persistence:NSURLCredentialPersistenceForSession]
                              forAuthenticationChallenge:challenge];
}

- (void)webView:(WebView *)wv resource:(id)identifier didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge fromDataSource:(WebDataSource *)dataSource
{
}

-(void)webView: (WebView *)wv resource:identifier didReceiveResponse: (NSURLResponse *)response fromDataSource:(WebDataSource *)dataSource
{
    if (!done && gLayoutTestController->dumpResourceLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didReceiveResponse %@", identifier, [response _drt_descriptionSuitableForTestResult]];
        printf("%s\n", [string UTF8String]);
    }
    if (!done && gLayoutTestController->dumpResourceResponseMIMETypes())
        printf("%s has MIME type %s\n", [[[[response URL] relativePath] lastPathComponent] UTF8String], [[response MIMEType] UTF8String]);
}

-(void)webView: (WebView *)wv resource:identifier didReceiveContentLength: (NSInteger)length fromDataSource:(WebDataSource *)dataSource
{
}

-(void)webView: (WebView *)wv resource:identifier didFinishLoadingFromDataSource:(WebDataSource *)dataSource
{
    if (!done && gLayoutTestController->dumpResourceLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFinishLoading", identifier];
        printf("%s\n", [string UTF8String]);
    }
}

-(void)webView: (WebView *)wv resource:identifier didFailLoadingWithError:(NSError *)error fromDataSource:(WebDataSource *)dataSource
{
    if (!done && gLayoutTestController->dumpResourceLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFailLoadingWithError: %@", identifier, [error _drt_descriptionSuitableForTestResult]];
        printf("%s\n", [string UTF8String]);
    }
}

- (void)webView: (WebView *)wv plugInFailedWithError:(NSError *)error dataSource:(WebDataSource *)dataSource
{
    // The call to -display here simulates the "Plug-in not found" sheet that Safari shows.
    // It is used for platform/mac/plugins/update-widget-from-style-recalc.html
    [wv display];
}

-(NSCachedURLResponse *) webView: (WebView *)wv resource:(id)identifier willCacheResponse:(NSCachedURLResponse *)response fromDataSource:(WebDataSource *)dataSource
{
    if (!done && gLayoutTestController->dumpWillCacheResponse()) {
        NSString *string = [NSString stringWithFormat:@"%@ - willCacheResponse: called", identifier];
        printf("%s\n", [string UTF8String]);
    }
    return response;
}

@end
