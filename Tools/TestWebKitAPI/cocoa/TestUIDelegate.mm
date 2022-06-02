/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "TestUIDelegate.h"

#import "Utilities.h"
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

@implementation TestUIDelegate {
    BOOL _showedInspector;
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    if (_createWebViewWithConfiguration)
        return _createWebViewWithConfiguration(configuration, navigationAction, windowFeatures);
    return nil;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    if (_runJavaScriptAlertPanelWithMessage)
        _runJavaScriptAlertPanelWithMessage(webView, message, frame, completionHandler);
    else
        completionHandler();
}

#if PLATFORM(MAC)
- (void)_webView:(WKWebView *)webView getContextMenuFromProposedMenu:(NSMenu *)menu forElement:(_WKContextMenuElementInfo *)element userInfo:(id <NSSecureCoding>)userInfo completionHandler:(void (^)(NSMenu *))completionHandler
{
    if (_getContextMenuFromProposedMenu)
        _getContextMenuFromProposedMenu(menu, element, userInfo, completionHandler);
    else
        completionHandler(menu);
}

- (void)_focusWebView:(WKWebView *)webView
{
    if (_focusWebView)
        _focusWebView(webView);
}
#endif // PLATFORM(MAC)

- (void)_webView:(WKWebView *)webView saveDataToFile:(NSData *)data suggestedFilename:(NSString *)suggestedFilename mimeType:(NSString *)mimeType originatingURL:(NSURL *)url
{
    if (_saveDataToFile)
        _saveDataToFile(webView, data, suggestedFilename, mimeType, url);
}

- (NSString *)waitForAlert
{
    EXPECT_FALSE(self.runJavaScriptAlertPanelWithMessage);

    __block bool finished = false;
    __block RetainPtr<NSString> result;
    self.runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        result = message;
        finished = true;
        completionHandler();
    };

    TestWebKitAPI::Util::run(&finished);

    self.runJavaScriptAlertPanelWithMessage = nil;
    return result.autorelease();
}

- (void)_webView:(WKWebView *)webView didAttachLocalInspector:(_WKInspector *)inspector
{
    _showedInspector = YES;
}

- (void)waitForInspectorToShow
{
    while (!_showedInspector)
        TestWebKitAPI::Util::spinRunLoop();
}

@end

@implementation WKWebView (TestUIDelegateExtras)

- (NSString *)_test_waitForAlert
{
    EXPECT_FALSE(self.UIDelegate);
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    self.UIDelegate = uiDelegate.get();
    NSString *alert = [uiDelegate waitForAlert];
    self.UIDelegate = nil;
    return alert;
}

- (void)_test_waitForInspectorToShow
{
    EXPECT_FALSE(self.UIDelegate);
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    self.UIDelegate = uiDelegate.get();
    [uiDelegate waitForInspectorToShow];
    self.UIDelegate = nil;
}

@end
