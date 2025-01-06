/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WebScriptWorld.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)

@interface UserStyleSheetParsingWebKitLegacyTest : NSObject <WebFrameLoadDelegate> {
}
@end

@implementation UserStyleSheetParsingWebKitLegacyTest  {
    bool _doneLoading;
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    _doneLoading = true;
}

- (void)waitForLoadToFinish
{
    TestWebKitAPI::Util::run(&_doneLoading);
}

@end

TEST(UseSystemAppearance, UserStyleSheetParsingWebKitLegacy)
{
    RetainPtr webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300) frameName:@"" groupName:@"UserStyleSheetParsingWebKitLegacyTest"]);

    RetainPtr delegate = adoptNS([UserStyleSheetParsingWebKitLegacyTest new]);
    [webView setFrameLoadDelegate:delegate.get()];

    [[webView mainFrame] loadHTMLString:@"<div id=test></div>" baseURL:nil];
    [delegate waitForLoadToFinish];

    [WebView _addUserStyleSheetToGroup:@"UserStyleSheetParsingWebKitLegacyTest" world:[WebScriptWorld world] source:@"@media (prefers-dark-interface) { #test { color: green } }" url:nil includeMatchPatternStrings:nil excludeMatchPatternStrings:nil injectedFrames:WebInjectInAllFrames];
    [WebView _addUserStyleSheetToGroup:@"UserStyleSheetParsingWebKitLegacyTest" world:[WebScriptWorld world] source:@"@media not screen and (prefers-dark-interface) { #test { color: green } }" url:nil includeMatchPatternStrings:nil excludeMatchPatternStrings:nil injectedFrames:WebInjectInAllFrames];

    EXPECT_WK_STREQ("rgb(0, 0, 0)", [webView stringByEvaluatingJavaScriptFromString:@"getComputedStyle(test).color"]);

    [webView _setUseSystemAppearance:YES];
    EXPECT_WK_STREQ("rgb(0, 128, 0)", [webView stringByEvaluatingJavaScriptFromString:@"getComputedStyle(test).color"]);

    [webView _setUseSystemAppearance:NO];
    EXPECT_WK_STREQ("rgb(0, 0, 0)", [webView stringByEvaluatingJavaScriptFromString:@"getComputedStyle(test).color"]);

    [WebView _removeUserStyleSheetsFromGroup:@"UserStyleSheetParsingWebKitLegacyTest" world:[WebScriptWorld world]];
}

#endif // PLATFORM(MAC)

TEST(UseSystemAppearance, UserStyleSheetParsing)
{
    RetainPtr styleSheet1 = adoptNS([[_WKUserStyleSheet alloc] initWithSource:@"@media (prefers-dark-interface) { #test { color: green } }" forMainFrameOnly:NO]);
    RetainPtr styleSheet2 = adoptNS([[_WKUserStyleSheet alloc] initWithSource:@"@media not screen and (prefers-dark-interface) { #test { color: green } }" forMainFrameOnly:NO]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] _addUserStyleSheet:styleSheet1.get()];
    [[configuration userContentController] _addUserStyleSheet:styleSheet2.get()];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<div id=test></div>"];

    EXPECT_WK_STREQ("rgb(0, 0, 0)", [webView stringByEvaluatingJavaScript:@"getComputedStyle(test).color"]);

    [webView _setUseSystemAppearance:YES];
    EXPECT_WK_STREQ("rgb(0, 128, 0)", [webView stringByEvaluatingJavaScript:@"getComputedStyle(test).color"]);

    [webView _setUseSystemAppearance:NO];
    EXPECT_WK_STREQ("rgb(0, 0, 0)", [webView stringByEvaluatingJavaScript:@"getComputedStyle(test).color"]);
}

#endif // PLATFORM(COCOA)
