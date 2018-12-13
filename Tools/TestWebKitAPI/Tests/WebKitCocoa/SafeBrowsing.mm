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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

static bool committedNavigation;
static bool warningShown;

@interface SafeBrowsingNavigationDelegate : NSObject <WKNavigationDelegate, WKUIDelegatePrivate>
@end

@implementation SafeBrowsingNavigationDelegate

- (void)webView:(WKWebView *)webView didCommitNavigation:(null_unspecified WKNavigation *)navigation
{
    committedNavigation = true;
}

- (void)_webViewDidShowSafeBrowsingWarning:(WKWebView *)webView
{
    warningShown = true;
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

static NSURL *resourceURL(NSString *resource)
{
    return [[NSBundle mainBundle] URLForResource:resource withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
}

TEST(SafeBrowsing, Preference)
{
    ClassMethodSwizzler swizzler(objc_getClass("SSBLookupContext"), @selector(sharedLookupContext), [TestLookupContext methodForSelector:@selector(sharedLookupContext)]);

    __block bool done = false;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().didStartProvisionalNavigation = ^(WKWebView *, WKNavigation *) {
        done = true;
    };

    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    EXPECT_TRUE([webView configuration].preferences.safeBrowsingEnabled);
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple")]];
    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
    [webView configuration].preferences.safeBrowsingEnabled = NO;
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple2")]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE([webView configuration].preferences.safeBrowsingEnabled);
    EXPECT_FALSE([webView _safeBrowsingWarning]);
}

static RetainPtr<WKWebView> safeBrowsingView()
{
    ClassMethodSwizzler swizzler(objc_getClass("SSBLookupContext"), @selector(sharedLookupContext), [TestLookupContext methodForSelector:@selector(sharedLookupContext)]);

    static auto delegate = adoptNS([SafeBrowsingNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple")]];
    EXPECT_FALSE(warningShown);
    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_TRUE(warningShown);
#if !PLATFORM(MAC)
    [[webView _safeBrowsingWarning] didMoveToWindow];
#endif
    return webView;
}

#if PLATFORM(MAC)
static void checkTitleAndClick(NSButton *button, const char* expectedTitle)
{
    EXPECT_STREQ(button.title.UTF8String, expectedTitle);
    [button performClick:nil];
}
#else
static void checkTitleAndClick(UIButton *button, const char* expectedTitle)
{
    EXPECT_STREQ([button attributedTitleForState:UIControlStateNormal].string.UTF8String, expectedTitle);
    UIView *target = button.superview.superview;
    SEL selector = NSSelectorFromString(strcmp(expectedTitle, "Go Back") ? @"showDetailsClicked" : @"goBackClicked");
    [target performSelector:selector];
}
#endif

TEST(SafeBrowsing, GoBack)
{
    auto webView = safeBrowsingView();
    auto warning = [webView _safeBrowsingWarning];
    auto box = warning.subviews.firstObject;
    checkTitleAndClick(box.subviews[3], "Go Back");
    EXPECT_EQ([webView _safeBrowsingWarning], nil);
}

TEST(SafeBrowsing, VisitUnsafeWebsite)
{
    auto webView = safeBrowsingView();
    auto warning = [webView _safeBrowsingWarning];
    EXPECT_EQ(warning.subviews.count, 1ull);
#if PLATFORM(MAC)
    EXPECT_GT(warning.subviews.firstObject.subviews[2].frame.size.height, 0);
#endif
    checkTitleAndClick(warning.subviews.firstObject.subviews[4], "Show Details");
    EXPECT_EQ(warning.subviews.count, 2ull);
    EXPECT_FALSE(committedNavigation);
    [warning performSelector:NSSelectorFromString(@"clickedOnLink:") withObject:[NSURL URLWithString:@"WKVisitUnsafeWebsiteSentinel"]];
    TestWebKitAPI::Util::run(&committedNavigation);
}

TEST(SafeBrowsing, NavigationClearsWarning)
{
    auto webView = safeBrowsingView();
    EXPECT_NE([webView _safeBrowsingWarning], nil);
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    while ([webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(SafeBrowsing, ShowWarningSPI)
{
    __block bool completionHandlerCalled = false;
    __block BOOL shouldContinueValue = NO;
    auto webView = adoptNS([WKWebView new]);
    auto showWarning = ^{
        completionHandlerCalled = false;
        [webView _showSafeBrowsingWarningWithTitle:@"test title" warning:@"test warning" details:[[[NSAttributedString alloc] initWithString:@"test details"] autorelease] completionHandler:^(BOOL shouldContinue) {
            shouldContinueValue = shouldContinue;
            completionHandlerCalled = true;
        }];
#if !PLATFORM(MAC)
        [[webView _safeBrowsingWarning] didMoveToWindow];
#endif
    };

    showWarning();
    checkTitleAndClick([webView _safeBrowsingWarning].subviews.firstObject.subviews[3], "Go Back");
    TestWebKitAPI::Util::run(&completionHandlerCalled);
    EXPECT_FALSE(shouldContinueValue);

    showWarning();
    [[webView _safeBrowsingWarning] performSelector:NSSelectorFromString(@"clickedOnLink:") withObject:[WKWebView _visitUnsafeWebsiteSentinel]];
    TestWebKitAPI::Util::run(&completionHandlerCalled);
    EXPECT_TRUE(shouldContinueValue);
}

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
    ClassMethodSwizzler swizzler(objc_getClass("SSBLookupContext"), @selector(sharedLookupContext), [NullLookupContext methodForSelector:@selector(sharedLookupContext)]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
}

#endif // WK_API_ENABLED
