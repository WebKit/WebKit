/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ((PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)) && !defined(__i386__) && !PLATFORM(IOSMAC)

#import "ClassMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

static bool committedNavigation;

@interface SafeBrowsingNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation SafeBrowsingNavigationDelegate

- (void)webView:(WKWebView *)webView didCommitNavigation:(null_unspecified WKNavigation *)navigation
{
    committedNavigation = true;
}

@end

@interface TestServiceLookupResult : NSObject {
    RetainPtr<NSString> _provider;
    BOOL _isPhishing;
    BOOL _isMalware;
    BOOL _isUnwantedSoftware;
}
@end

@implementation TestServiceLookupResult

+ (instancetype)resultWithProvider:(RetainPtr<NSString>&&)provider phishing:(BOOL)phishing malware:(BOOL)malware unwantedSoftware:(BOOL)unwantedSoftware
{
    TestServiceLookupResult *result = [[TestServiceLookupResult alloc] init];
    if (!result)
        return nil;

    result->_provider = WTFMove(provider);
    result->_isPhishing = phishing;
    result->_isMalware = malware;
    result->_isUnwantedSoftware = unwantedSoftware;

    return [result autorelease];
}

- (NSString *)provider
{
    return _provider.get();
}

- (BOOL)isPhishing
{
    return _isPhishing;
}

- (BOOL)isMalware
{
    return _isMalware;
}

- (BOOL)isUnwantedSoftware
{
    return _isUnwantedSoftware;
}

@end

@interface TestLookupResult : NSObject {
    RetainPtr<NSArray> _results;
}
@end

@implementation TestLookupResult

+ (instancetype)resultWithResults:(RetainPtr<NSArray<TestServiceLookupResult *>>&&)results
{
    TestLookupResult *result = [[TestLookupResult alloc] init];
    if (!result)
        return nil;
    
    result->_results = WTFMove(results);
    
    return [result autorelease];
}

- (NSArray<TestServiceLookupResult *> *)serviceLookupResults
{
    return _results.get();
}

@end

@interface TestLookupContext : NSObject
@end

@implementation TestLookupContext

+ (TestLookupContext *)sharedLookupContext
{
    static TestLookupContext *context = [[TestLookupContext alloc] init];
    return context;
}

- (void)lookUpURL:(NSURL *)URL completionHandler:(void (^)(TestLookupResult *, NSError *))completionHandler
{
    completionHandler([TestLookupResult resultWithResults:@[[TestServiceLookupResult resultWithProvider:@"TestProvider" phishing:YES malware:NO unwantedSoftware:NO]]], nil);
}

@end

#if PLATFORM(MAC) // FIXME: Test on iOS once implemented.

static NSURL *simpleURL()
{
    return [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
}

static RetainPtr<WKWebView> safeBrowsingView()
{
    TestWebKitAPI::ClassMethodSwizzler swizzler(objc_getClass("SSBLookupContext"), @selector(sharedLookupContext), [TestLookupContext methodForSelector:@selector(sharedLookupContext)]);

    static auto delegate = adoptNS([SafeBrowsingNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:simpleURL()]];
    while (![webView _safeBrowsingWarningForTesting])
        TestWebKitAPI::Util::spinRunLoop();
    return webView;
}

TEST(SafeBrowsing, GoBack)
{
    auto webView = safeBrowsingView();
    NSView *bottom = [webView _safeBrowsingWarningForTesting].subviews.firstObject.subviews.lastObject;
    NSButton *goBack = (NSButton *)bottom.subviews.lastObject;
    EXPECT_STREQ(goBack.title.UTF8String, "Go back");
    [goBack performClick:nil];
    EXPECT_EQ([webView _safeBrowsingWarningForTesting], nil);
}

TEST(SafeBrowsing, VisitUnsafeWebsite)
{
    auto webView = safeBrowsingView();
    NSView *warning = [webView _safeBrowsingWarningForTesting];
    NSView *box = warning.subviews.firstObject;
    NSButton *showDetails = (NSButton *)box.subviews.lastObject.subviews.firstObject;
    EXPECT_STREQ(showDetails.title.UTF8String, "Show details");
    EXPECT_EQ(box.subviews.count, 3ull);
    [showDetails performClick:nil];
    EXPECT_EQ(box.subviews.count, 4ull);
    EXPECT_FALSE(committedNavigation);
    [warning performSelector:NSSelectorFromString(@"clickedOnLink:") withObject:@"WKVisitUnsafeWebsiteSentinel"];
    TestWebKitAPI::Util::run(&committedNavigation);
}

TEST(SafeBrowsing, NavigationClearsWarning)
{
    auto webView = safeBrowsingView();
    EXPECT_NE([webView _safeBrowsingWarningForTesting], nil);
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    while ([webView _safeBrowsingWarningForTesting])
        TestWebKitAPI::Util::spinRunLoop();
}

#endif // PLATFORM(MAC)

@interface NullLookupContext : NSObject
@end
@implementation NullLookupContext
+ (NullLookupContext *)sharedLookupContext
{
    return nil;
}
@end

TEST(SafeBrowsing, MissingFramework)
{
    TestWebKitAPI::ClassMethodSwizzler swizzler(objc_getClass("SSBLookupContext"), @selector(sharedLookupContext), [NullLookupContext methodForSelector:@selector(sharedLookupContext)]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
}

#endif // WK_API_ENABLED
