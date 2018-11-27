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

#include "config.h"

#import "DataDetectorsCoreSPI.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED && PLATFORM(IOS_FAMILY)

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
        @"ReferenceDate": _referenceDate
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

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

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

#if PLATFORM(IOS)

TEST(WebKit, AddAndRemoveDataDetectors)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
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
        EXPECT_WK_STREQ("December 21, 2021", results[1].value);
        EXPECT_WK_STREQ("FlightInformation", results[2].type);
        EXPECT_WK_STREQ("AC780", results[2].value);

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000
        EXPECT_EQ(DDResultCategoryUnknown, results[0].category);
        EXPECT_EQ(DDResultCategoryCalendarEvent, results[1].category);
        EXPECT_EQ(DDResultCategoryMisc, results[2].category);
#endif
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

#endif // PLATFORM(IOS)

#endif
