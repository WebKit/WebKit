/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(APP_HIGHLIGHTS)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKAppHighlight.h>
#import <WebKit/_WKAppHighlightDelegate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPIForTesting.h"
#endif

@interface AppHighlightDelegate : NSObject <_WKAppHighlightDelegate>
@property (nonatomic, copy) void (^storeAppHighlightCallback)(WKWebView *, _WKAppHighlight *, BOOL, BOOL);
@end

@implementation AppHighlightDelegate
- (void)_webView:(WKWebView *)webView storeAppHighlight:(_WKAppHighlight *)highlight inNewGroup:(BOOL)inNewGroup requestOriginatedInApp:(BOOL)requestOriginatedInApp
{
    if (_storeAppHighlightCallback)
        _storeAppHighlightCallback(webView, highlight, inNewGroup, requestOriginatedInApp);
}
@end

namespace TestWebKitAPI {

RetainPtr<_WKAppHighlight> createAppHighlightWithHTML(NSString *HTMLString, NSString *javaScript, NSString *highlightText)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto delegate = adoptNS([[AppHighlightDelegate alloc] init]);
    auto webViewCreate = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webViewCreate _setAppHighlightDelegate:delegate.get()];
    [webViewCreate synchronouslyLoadHTMLString:HTMLString];
    [webViewCreate stringByEvaluatingJavaScript:javaScript];
    __block RetainPtr<_WKAppHighlight> resultHighlight;
    __block bool done = false;
    [delegate setStoreAppHighlightCallback:^(WKWebView *delegateWebView, _WKAppHighlight *highlight, BOOL inNewGroup, BOOL requestOriginatedInApp) {
        EXPECT_EQ(delegateWebView, webViewCreate.get());
        EXPECT_NOT_NULL(highlight);
        EXPECT_WK_STREQ(highlight.text, highlightText);
        EXPECT_NOT_NULL(highlight.highlight);
        resultHighlight = highlight;
        done = true;
    }];
    [webViewCreate _addAppHighlight];
    TestWebKitAPI::Util::run(&done);
    return resultHighlight;
}

RetainPtr<WKWebView> createWebViewForAppHighlightsWithHTML(NSString *HTMLString)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webViewRestore = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webViewRestore synchronouslyLoadHTMLString:HTMLString];
    
    return webViewRestore;
}

TEST(AppHighlights, AppHighlightCreateAndRestore)
{
    auto highlight = createAppHighlightWithHTML(@"Test", @"document.execCommand('SelectAll')", @"Test");
    auto webViewRestore = createWebViewForAppHighlightsWithHTML(@"Test");
    
    [webViewRestore _restoreAppHighlights:@[[highlight highlight]]];
    
    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return [webViewRestore stringByEvaluatingJavaScript:@"internals.numberOfAppHighlights()"].intValue == 1;
    }, 2, @"Expected Highlights to be populated.");
}

// FIXME when rdar://141462057 is resolved.
#if PLATFORM(IOS)
TEST(AppHighlights, DISABLED_AppHighlightCreateAndRestoreAndScroll)
#else
TEST(AppHighlights, AppHighlightCreateAndRestoreAndScroll)
#endif
{
    auto highlight = createAppHighlightWithHTML(@"<div style='height: 10000px'></div>Test", @"document.execCommand('SelectAll')", @"Test");
    auto webViewRestore = createWebViewForAppHighlightsWithHTML(@"<div style='height: 10000px'></div>Test");

    [webViewRestore _restoreAndScrollToAppHighlight:[highlight highlight]];

    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return [webViewRestore stringByEvaluatingJavaScript:@"internals.numberOfAppHighlights()"].intValue == 1
            && [[webViewRestore objectByEvaluatingJavaScript:@"pageYOffset"] floatValue] > 0;
    }, 2, @"Expected Highlights to be populated and the page to scroll.");
}

TEST(AppHighlights, AppHighlightRestoreFailure)
{
    auto highlight = createAppHighlightWithHTML(@"Test", @"document.execCommand('SelectAll')", @"Test");
    auto webViewRestore = createWebViewForAppHighlightsWithHTML(@"Not The Same");
    
    [webViewRestore _restoreAppHighlights:@[[highlight highlight]]];
    
    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return ![webViewRestore stringByEvaluatingJavaScript:@"internals.numberOfAppHighlights()"].intValue;
    }, 2, @"Expected Highlights not to be populated.");
}

// Ensure that future versions of the blob format can add additional data and still be decoded successfully by version of WebKit that only know about the current format.
TEST(AppHighlights, AppHighlightCreateAndRestoreWithExtraBytes)
{
    auto highlight = createAppHighlightWithHTML(@"Test", @"document.execCommand('SelectAll')", @"Test");
    auto webViewRestore = createWebViewForAppHighlightsWithHTML(@"Test");
        
    NSMutableData *modifiedHighlightData = [NSMutableData dataWithData: [highlight highlight] ];
    [modifiedHighlightData appendData:[@"TestV2Data" dataUsingEncoding:NSUTF8StringEncoding]];
    
    [webViewRestore synchronouslyLoadHTMLString:@"Test"];
    [webViewRestore _restoreAppHighlights:@[ modifiedHighlightData ]];
    
    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return [webViewRestore stringByEvaluatingJavaScript:@"internals.numberOfAppHighlights()"].intValue == 1;
    }, 2, @"Expected Highlights to be populated.");
}

// Older versions of WebKit need to be able to decode blobs encoded on newer versions of WebKit, so ensure that is possible.
TEST(AppHighlights, AppHighlightCreateAndRestoreWithLaterVersion)
{
    auto highlight = createAppHighlightWithHTML(@"Test", @"document.execCommand('SelectAll')", @"Test");
    auto webViewRestore = createWebViewForAppHighlightsWithHTML(@"Test");
        
    uint64_t maximumVersion = std::numeric_limits<uint64_t>::max();
    NSData *versionData = [NSData dataWithBytes:&maximumVersion length:sizeof(maximumVersion)];
    NSMutableData *modifiedHighlightData = [NSMutableData dataWithData:[highlight highlight]];

    [modifiedHighlightData replaceBytesInRange:NSMakeRange(sizeof(uint64_t), sizeof(maximumVersion)) withBytes:versionData];

    [webViewRestore synchronouslyLoadHTMLString:@"Test"];
    [webViewRestore _restoreAppHighlights:@[ modifiedHighlightData ]];
    
    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return [webViewRestore stringByEvaluatingJavaScript:@"internals.numberOfAppHighlights()"].intValue == 1;
    }, 2, @"Expected Highlights to be populated.");
}

// Ensure that shorter data blobs will correctly fail to decode.
TEST(AppHighlights, AppHighlightCreateAndRestoreAndDropBytes)
{
    auto highlight = createAppHighlightWithHTML(@"Test", @"document.execCommand('SelectAll')", @"Test");
    auto webViewRestore = createWebViewForAppHighlightsWithHTML(@"Test");
        
    NSMutableData *modifiedHighlightData = [NSMutableData dataWithBytes:[highlight highlight].bytes length:[highlight highlight].length / 2];

    [webViewRestore synchronouslyLoadHTMLString:@"Test"];
    [webViewRestore _restoreAppHighlights:@[ modifiedHighlightData ]];
    
    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return ![webViewRestore stringByEvaluatingJavaScript:@"internals.numberOfAppHighlights()"].intValue;
    }, 2, @"Expected Highlights not to be populated.");
}

// Ensure that the original data format will always be able to be decoded in the future.
TEST(AppHighlights, AppHighlightRestoreFromStorageV0)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    const unsigned char bytes[] = {0x04, 0x00, 0x00, 0x00, 0x01, 0x54, 0x65, 0x73, 0x74, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x05, 0x00, 0x00, 0x00, 0x01, 0x23, 0x74, 0x65, 0x78, 0x74, 0x04, 0x00, 0x00, 0x00, 0x01, 0x54, 0x65, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x05, 0x00, 0x00, 0x00, 0x01, 0x23, 0x74, 0x65, 0x78, 0x74, 0x04, 0x00, 0x00, 0x00, 0x01, 0x54, 0x65, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00};
    
    NSData *storedData = [NSData dataWithBytes:&bytes length:87];

    auto webViewRestore = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);

    [webViewRestore synchronouslyLoadHTMLString:@"Test"];
    [webViewRestore _restoreAppHighlights:@[ storedData ]];
    
    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return [webViewRestore stringByEvaluatingJavaScript:@"internals.numberOfAppHighlights()"].intValue == 1;
    }, 2, @"Expected Highlights to be populated.");
}


// Ensure that the V1 data format will always be able to be decoded in the future.
TEST(AppHighlights, AppHighlightRestoreFromStorageV1)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    const unsigned char bytes[] = {0x31, 0x32, 0x30, 0x32, 0x48, 0x50, 0x41, 0x41, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x01, 0x31, 0x36, 0x30, 0x61, 0x36, 0x30, 0x31, 0x62, 0x2d, 0x31, 0x62, 0x37, 0x32, 0x2d, 0x34, 0x31, 0x31, 0x38, 0x2d, 0x62, 0x36, 0x64, 0x31, 0x2d, 0x39, 0x30, 0x32, 0x62, 0x37, 0x34, 0x66, 0x65, 0x61, 0x36, 0x36, 0x31, 0x04, 0x00, 0x00, 0x00, 0x01, 0x54, 0x65, 0x73, 0x74, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x05, 0x00, 0x00, 0x00, 0x01, 0x23, 0x74, 0x65, 0x78, 0x74, 0x04, 0x00, 0x00, 0x00, 0x01, 0x54, 0x65, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x05, 0x00, 0x00, 0x00, 0x01, 0x23, 0x74, 0x65, 0x78, 0x74, 0x04, 0x00, 0x00, 0x00, 0x01, 0x54, 0x65, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00};
    
    NSData *storedData = [NSData dataWithBytes:&bytes length:144];

    auto webViewRestore = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);

    [webViewRestore synchronouslyLoadHTMLString:@"Test"];
    [webViewRestore _restoreAppHighlights:@[ storedData ]];
    
    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return [webViewRestore stringByEvaluatingJavaScript:@"internals.numberOfAppHighlights()"].intValue == 1;
    }, 2, @"Expected Highlights to be populated.");
}

} // namespace TestWebKitAPI

#endif // ENABLE(APP_HIGHLIGHTS)
