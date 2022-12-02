/*
 * Copyright (C) 2007-2022 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#import "TestRunner.h"
#import <JavaScriptCore/RegularExpression.h>
#import <WebCore/ProtectionSpaceCocoa.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebKitLegacy.h>
#import <wtf/Assertions.h>

using namespace std;

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
    NSString *str = [NSString stringWithFormat:@"<NSError domain %@, code %ld", [self domain], static_cast<long>([self code])];
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
    basePath = [basePath stringByAppendingString:@"/"];

    if (basePath && [[self path] hasPrefix:basePath])
        return [[self path] substringFromIndex:[basePath length]];
    return [self lastPathComponent]; // We lose some information here, but it's better than exposing a full path, which is always machine specific.
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

- (id)webView: (WebView *)wv identifierForInitialRequest: (NSURLRequest *)request fromDataSource: (WebDataSource *)dataSource
{
    ASSERT([[dataSource webFrame] dataSource] || [[dataSource webFrame] provisionalDataSource]);

    if (!done)
        return [[request URL] _drt_descriptionSuitableForTestResult];

    return @"<unknown>";
}

BOOL isLocalhost(NSString *host)
{
    // FIXME: Support IPv6 loopbacks.
    return NSOrderedSame == [host compare:@"127.0.0.1"] || NSOrderedSame == [host caseInsensitiveCompare:@"localhost"];
}

BOOL hostIsUsedBySomeTestsToGenerateError(NSString *host)
{
    return NSOrderedSame == [host compare:@"255.255.255.255"];
}

BOOL isAllowedHost(NSString *host)
{
    return gTestRunner->allowedHosts().count(host.UTF8String);
}

BOOL canAuthenticateServerTrustAgainstProtectionSpace(NSString *host)
{
    return isLocalhost(host) || gTestRunner->localhostAliases().contains(host.UTF8String) || (gTestRunner->allowAnyHTTPSCertificateForAllowedHosts() && isAllowedHost(host));
}

-(NSURLRequest *)webView: (WebView *)wv resource:identifier willSendRequest: (NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse fromDataSource:(WebDataSource *)dataSource
{
    if (!done && gTestRunner->dumpResourceLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - willSendRequest %@ redirectResponse %@", identifier, [request _drt_descriptionSuitableForTestResult],
            [redirectResponse _drt_descriptionSuitableForTestResult]];
        printf("%s\n", [string UTF8String]);
    }

    if (!done && gTestRunner->willSendRequestReturnsNull())
        return nil;

    if (!done && gTestRunner->willSendRequestReturnsNullOnRedirect() && redirectResponse) {
        printf("Returning null for this redirect\n");
        return nil;
    }

    NSURL *url = [request URL];
    NSString *host = [url host];
    if (host && (NSOrderedSame == [[url scheme] caseInsensitiveCompare:@"http"] || NSOrderedSame == [[url scheme] caseInsensitiveCompare:@"https"])) {
        NSString *testURL = [NSString stringWithUTF8String:gTestRunner->testURL().c_str()];
        NSString *lowercaseTestURL = [testURL lowercaseString];
        NSString *testHost = 0;
        if ([lowercaseTestURL hasPrefix:@"http:"] || [lowercaseTestURL hasPrefix:@"https:"])
            testHost = [[NSURL URLWithString:testURL] host];
        if (!isLocalhost(host) && !hostIsUsedBySomeTestsToGenerateError(host) && !isAllowedHost(host) && (!testHost || isLocalhost(testHost))) {
            String blockedURL = [url absoluteString];
            replace(blockedURL, JSC::Yarr::RegularExpression("&key=[^&]+&"_s), "&key=GENERATED_KEY&"_s);
            replace(blockedURL, JSC::Yarr::RegularExpression("reportID=[-0123456789abcdefABCDEF]+"_s), "reportID=GENERATED_REPORT_ID"_s);
            printf("Blocked access to external URL %s\n", blockedURL.utf8().data());
            return nil;
        }
    }

    if (disallowedURLs && CFSetContainsValue(disallowedURLs.get(), (__bridge CFURLRef)url))
        return nil;

    auto newRequest = adoptNS([request mutableCopy]);
    const set<string>& clearHeaders = gTestRunner->willSendRequestClearHeaders();
    for (set<string>::const_iterator header = clearHeaders.begin(); header != clearHeaders.end(); ++header) {
        auto nsHeader = adoptNS([[NSString alloc] initWithUTF8String:header->c_str()]);
        [newRequest setValue:nil forHTTPHeaderField:nsHeader.get()];
    }
    if (auto* destination = gTestRunner->redirectionDestinationForURL([[url absoluteString] UTF8String]))
        [newRequest setURL:[NSURL URLWithString:[NSString stringWithUTF8String:destination]]];

    return newRequest.autorelease();
}

- (void)webView:(WebView *)wv resource:(id)identifier didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge fromDataSource:(WebDataSource *)dataSource
{
    if (gTestRunner->rejectsProtectionSpaceAndContinueForAuthenticationChallenges()) {
        printf("Simulating reject protection space and continue for authentication challenge\n");

        [[challenge sender] rejectProtectionSpaceAndContinueWithChallenge:challenge];
        return;
    }

    if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        if (gTestRunner->allowsAnySSLCertificate()) {
            [[challenge sender] useCredential:[NSURLCredential credentialWithUser:@"accept server trust" password:@"" persistence:NSURLCredentialPersistenceNone]
                forAuthenticationChallenge:challenge];
            return;
        }
    }

    if (!gTestRunner->handlesAuthenticationChallenges()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didReceiveAuthenticationChallenge - Simulating cancelled authentication sheet", identifier];
        printf("%s\n", [string UTF8String]);

        [[challenge sender] continueWithoutCredentialForAuthenticationChallenge:challenge];
        return;
    }
    
    const char* user = gTestRunner->authenticationUsername().c_str();
    NSString *nsUser = [NSString stringWithFormat:@"%s", user ? user : ""];

    const char* password = gTestRunner->authenticationPassword().c_str();
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
    if (!done && gTestRunner->dumpResourceLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didReceiveResponse %@", identifier, [response _drt_descriptionSuitableForTestResult]];
        printf("%s\n", [string UTF8String]);
    }
    if (!done && gTestRunner->dumpResourceResponseMIMETypes())
        printf("%s has MIME type %s\n", [[[[response URL] relativePath] lastPathComponent] UTF8String], [[response MIMEType] UTF8String]);
}

-(void)webView: (WebView *)wv resource:identifier didReceiveContentLength: (NSInteger)length fromDataSource:(WebDataSource *)dataSource
{
}

-(void)webView: (WebView *)wv resource:identifier didFinishLoadingFromDataSource:(WebDataSource *)dataSource
{
    if (!done && gTestRunner->dumpResourceLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFinishLoading", identifier];
        printf("%s\n", [string UTF8String]);
    }
}

-(void)webView: (WebView *)wv resource:identifier didFailLoadingWithError:(NSError *)error fromDataSource:(WebDataSource *)dataSource
{
    if (!done && gTestRunner->dumpResourceLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFailLoadingWithError: %@", identifier, [error _drt_descriptionSuitableForTestResult]];
        printf("%s\n", [string UTF8String]);
    }
}

- (void)webView: (WebView *)wv plugInFailedWithError:(NSError *)error dataSource:(WebDataSource *)dataSource
{
}

-(NSCachedURLResponse *) webView: (WebView *)wv resource:(id)identifier willCacheResponse:(NSCachedURLResponse *)response fromDataSource:(WebDataSource *)dataSource
{
    if (!done && gTestRunner->dumpWillCacheResponse()) {
        NSString *string = [NSString stringWithFormat:@"%@ - willCacheResponse: called", identifier];
        printf("%s\n", [string UTF8String]);
    }
    return response;
}

-(BOOL)webView: (WebView*)webView shouldPaintBrokenImageForURL:(NSURL*)imageURL
{
    // Only log the message when shouldPaintBrokenImage() returns NO; this avoids changing results of layout tests with failed
    // images, e.g., security/block-test-no-port.html.
    if (!done && gTestRunner->dumpResourceLoadCallbacks() && !gTestRunner->shouldPaintBrokenImage()) {
        NSString *string = [NSString stringWithFormat:@"%@ - shouldPaintBrokenImage: NO", [imageURL _drt_descriptionSuitableForTestResult]];
        printf("%s\n", [string UTF8String]);
    }

    return gTestRunner->shouldPaintBrokenImage();
}

-(BOOL)webView:(WebView *)webView resource:(id)identifier canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpaceNS forDataSource:(WebDataSource *)dataSource
{
    if (!done && gTestRunner->dumpResourceLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - canAuthenticateAgainstProtectionSpace", identifier];
        printf("%s\n", [string UTF8String]);
    }

    WebCore::ProtectionSpace protectionSpace(protectionSpaceNS);

    auto scheme = protectionSpace.authenticationScheme();
    if (scheme == WebCore::ProtectionSpaceBase::AuthenticationScheme::ServerTrustEvaluationRequested)
        return canAuthenticateServerTrustAgainstProtectionSpace(protectionSpaceNS.host);

    return scheme <= WebCore::ProtectionSpaceBase::AuthenticationScheme::HTTPDigest || scheme == WebCore::ProtectionSpaceBase::AuthenticationScheme::OAuth;
}

@end
