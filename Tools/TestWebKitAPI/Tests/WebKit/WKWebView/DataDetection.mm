/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "Helpers/cocoa/DataDetectorsCoreSPI.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "InstanceMethodSwizzler.h"
#import <WebKit/WebKit.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>

#if ENABLE(DATA_DETECTION)

@interface WKWebView (DataDetection)
- (void)synchronouslyDetectDataWithTypes:(WKDataDetectorTypes)types;
- (void)synchronouslyRemoveDataDetectedLinks;
@end

@implementation WKWebView (DataDetection)

- (void)synchronouslyDetectDataWithTypes:(WKDataDetectorTypes)types
{
    __block bool done = false;
    [self _detectDataWithTypes:types completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

- (void)synchronouslyRemoveDataDetectedLinks
{
    __block bool done = false;
    [self _removeDataDetectedLinks:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

@end

static bool ranScript;

@interface DataDetectionUIDelegate : NSObject <WKUIDelegate>

@property (nonatomic, retain) NSDate *referenceDate;

@end

@implementation DataDetectionUIDelegate

- (NSDictionary *)_dataDetectionContextForWebView:(WKWebView *)webView
{
    if (!_referenceDate)
        return nil;

    return @{
        @"ReferenceDate": _referenceDate,
        @"unused": [NSUUID UUID]
    };
}

@end

void expectLinkCount(WKWebView *webView, NSString *HTMLString, unsigned linkCount)
{
    [webView loadHTMLString:HTMLString baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.querySelectorAll('a[x-apple-data-detectors=true]').length" completionHandler:^(id value, NSError *error) {
        EXPECT_EQ(linkCount, [value unsignedIntValue]);
        ranScript = true;
    }];

    TestWebKitAPI::Util::run(&ranScript);
    ranScript = false;
}

// FIXME: Re-enable this test once webkit.org/b/161967 is fixed.
TEST(WebKit, DISABLED_DataDetectionReferenceDate)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setDataDetectorTypes:WKDataDetectorTypeCalendarEvent];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<DataDetectionUIDelegate> UIDelegate = adoptNS([[DataDetectionUIDelegate alloc] init]);
    [webView setUIDelegate:UIDelegate.get()];

    expectLinkCount(webView.get(), @"tomorrow at 6PM", 1);
    expectLinkCount(webView.get(), @"yesterday at 6PM", 0);
    expectLinkCount(webView.get(), @"<a href='about:blank'>tomorrow at 6PM</a>", 0);
    expectLinkCount(webView.get(), @"<a href='about:blank'>tomorrow</a> at <a href='about:blank'>6PM</a>", 0);


    NSTimeInterval week = 60 * 60 * 24 * 7;

    [UIDelegate setReferenceDate:[NSDate dateWithTimeIntervalSinceNow:-week]];
    expectLinkCount(webView.get(), @"tomorrow at 6PM", 0);
    expectLinkCount(webView.get(), @"yesterday at 6PM", 0);

    [UIDelegate setReferenceDate:[NSDate dateWithTimeIntervalSinceNow:week]];
    expectLinkCount(webView.get(), @"tomorrow at 6PM", 1);
    expectLinkCount(webView.get(), @"yesterday at 6PM", 1);
}

#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(VISION)

TEST(WebKit, AddAndRemoveDataDetectors)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"data-detectors"];
    [webView expectElementCount:0 querySelector:@"a[x-apple-data-detectors=true]"];
    EXPECT_EQ(0U, [webView _dataDetectionResults].count);

    auto checkDataDetectionResults = [] (NSArray<DDScannerResult *> *results) {
        EXPECT_EQ(3U, results.count);
        EXPECT_TRUE([results[0].value containsString:@"+1-234-567-8900"]);
        EXPECT_TRUE([results[0].value containsString:@"https://www.apple.com"]);
        EXPECT_TRUE([results[0].value containsString:@"2 Apple Park Way, Cupertino 95014"]);
        EXPECT_WK_STREQ("SignatureBlock", results[0].type);
        EXPECT_WK_STREQ("Date", results[1].type);
        EXPECT_WK_STREQ("December 21, 2099", results[1].value);
        EXPECT_WK_STREQ("FlightInformation", results[2].type);
        EXPECT_WK_STREQ("AC780", results[2].value);

        EXPECT_EQ(DDResultCategoryUnknown, results[0].category);
        EXPECT_EQ(DDResultCategoryCalendarEvent, results[1].category);
        EXPECT_EQ(DDResultCategoryMisc, results[2].category);
    };

    [webView synchronouslyDetectDataWithTypes:WKDataDetectorTypeAll];
    [webView expectElementCount:5 querySelector:@"a[x-apple-data-detectors=true]"];
    checkDataDetectionResults([webView _dataDetectionResults]);

    [webView synchronouslyRemoveDataDetectedLinks];
    [webView expectElementCount:0 querySelector:@"a[x-apple-data-detectors=true]"];
    EXPECT_EQ(0U, [webView _dataDetectionResults].count);

    [webView synchronouslyDetectDataWithTypes:WKDataDetectorTypeAddress | WKDataDetectorTypePhoneNumber];
    [webView expectElementCount:2 querySelector:@"a[x-apple-data-detectors=true]"];
    checkDataDetectionResults([webView _dataDetectionResults]);
}

TEST(WebKit, DoNotCrashWhenDetectingDataAfterWebProcessTerminates)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"data-detectors"];
    [webView _killWebContentProcessAndResetState];
    [webView synchronouslyDetectDataWithTypes:WKDataDetectorTypeAll];
    [webView synchronouslyRemoveDataDetectedLinks];
}

#endif // PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(VISION)

TEST(DataDetectorTests, LoadWKWebViewWithDataDetectorTypePhoneNumber)
{
    NSString *const phoneNumber = @"(555) 867-5309";

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setDataDetectorTypes:WKDataDetectorTypePhoneNumber];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<!DOCTYPE><html><head></head><body><p>Call Jenny at %@</p></body></html>", phoneNumber]];

    // Ensure that the phone number is linked by Data Detectors. Detection finishes asynchronously after the load.
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.querySelectorAll('a').length"] intValue] == 1;
    }));
    NSString *linkText = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('a')[0].innerText"];
    EXPECT_WK_STREQ(phoneNumber, linkText);
    NSString *linkURL = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('a')[0].href"];
    NSString *expectedLinkURL = [NSString stringWithFormat:@"tel:%@", [phoneNumber stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet whitespaceCharacterSet].invertedSet]];
    EXPECT_WK_STREQ(expectedLinkURL, linkURL);
}

#if PLATFORM(MAC) && ENABLE(REVEAL)

TEST(DataDetectorTests, ClickingDataDetectorLinkShowsRevealMenu)
{
    __block bool didShowMenu = false;
    __block CGRect elementBounds = CGRectNull;
    __block CGPoint menuLocation = CGPointZero;
    InstanceMethodSwizzler showContextMenuSwizzler {
        NSClassFromString(@"WKRevealItemPresenter"),
        NSSelectorFromString(@"showContextMenu"),
        imp_implementationWithBlock(^(NSObject *presenter) {
            elementBounds = [(NSValue *)[presenter valueForKey:@"frameInView"] rectValue];
            menuLocation = [(NSValue *)[presenter valueForKey:@"menuLocationInView"] pointValue];
            didShowMenu = true;
        })
    };

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setDataDetectorTypes:WKDataDetectorTypeAddress];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<body style='margin: 0; font-size: 24px;'><p style='margin: 0;'>Meet me at 2 Apple Park Way, Cupertino 95014 tomorrow.</p></body>"];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.querySelectorAll('a[x-apple-data-detectors=true]').length"] intValue] == 1;
    }));

    RetainPtr<NSArray<NSNumber *>> linkRect = [webView objectByEvaluatingJavaScript:@"(() => {"
        @"    const rect = document.querySelector('a[x-apple-data-detectors=true]').getBoundingClientRect();"
        @"    return [rect.left, rect.top, rect.width, rect.height];"
        @"})()"];
    auto linkBounds = CGRectMake([linkRect.get()[0] doubleValue], [linkRect.get()[1] doubleValue], [linkRect.get()[2] doubleValue], [linkRect.get()[3] doubleValue]);

    __block bool didAttemptNavigation = false;
    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDecidePolicyForNavigationAction:^(WKNavigationAction *, void (^decisionHandler)(WKNavigationActionPolicy)) {
        didAttemptNavigation = true;
        decisionHandler(WKNavigationActionPolicyCancel);
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto clickLocation = CGPointMake(std::floor(CGRectGetMidX(linkBounds)), std::floor(CGRectGetMidY(linkBounds)));
    [webView sendClickAtPoint:[webView convertPoint:clickLocation toView:nil]];
    TestWebKitAPI::Util::run(&didShowMenu);
    [webView waitForNextPresentationUpdate];

    // Clicking a detected link should present the Reveal menu instead of navigating to the x-apple-data-detectors URL.
    EXPECT_FALSE(didAttemptNavigation);
    EXPECT_NEAR(elementBounds.origin.x, linkBounds.origin.x, 2);
    EXPECT_NEAR(elementBounds.origin.y, linkBounds.origin.y, 2);
    EXPECT_NEAR(elementBounds.size.width, linkBounds.size.width, 2);
    EXPECT_NEAR(elementBounds.size.height, linkBounds.size.height, 2);
    EXPECT_NEAR(menuLocation.x, clickLocation.x, 1);
    EXPECT_NEAR(menuLocation.y, clickLocation.y, 1);
}

#endif // PLATFORM(MAC) && ENABLE(REVEAL)

#endif // ENABLE(DATA_DETECTION)
