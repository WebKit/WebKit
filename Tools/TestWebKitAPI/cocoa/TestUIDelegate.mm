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

- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL))completionHandler
{
    if (_runJavaScriptConfirmPanelWithMessage)
        _runJavaScriptConfirmPanelWithMessage(webView, message, frame, completionHandler);
    else
        completionHandler(YES);
}

- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString *))completionHandler
{
    if (_runJavaScriptPromptPanelWithMessage)
        _runJavaScriptPromptPanelWithMessage(webView, prompt, defaultText, frame, completionHandler);
    else
        completionHandler(@"foo");
}

- (void)_webView:(WKWebView *)webView didAdjustVisibilityWithSelectors:(NSArray<NSString *> *)selectors
{
    if (_webViewDidAdjustVisibilityWithSelectors)
        _webViewDidAdjustVisibilityWithSelectors(webView, selectors);
}

#if PLATFORM(MAC)
- (void)_webView:(WKWebView *)webView getContextMenuFromProposedMenu:(NSMenu *)menu forElement:(_WKContextMenuElementInfo *)element userInfo:(id <NSSecureCoding>)userInfo completionHandler:(void (^)(NSMenu *))completionHandler
{
    if (_getContextMenuFromProposedMenu)
        _getContextMenuFromProposedMenu(menu, element, userInfo, completionHandler);
    else
        completionHandler(menu);
}

- (void)_webView:(WKWebView *)webView getWindowFrameWithCompletionHandler:(void (^)(CGRect))completionHandler
{
    if (_getWindowFrameWithCompletionHandler)
        _getWindowFrameWithCompletionHandler(webView, completionHandler);
    else
        completionHandler(CGRectZero);
}

- (void)_focusWebView:(WKWebView *)webView
{
    if (_focusWebView)
        _focusWebView(webView);
}

- (void)_unfocusWebView:(WKWebView *)webView
{
    if (_unfocusWebView)
        _unfocusWebView(webView);
}

- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> *))completionHandler
{
    if (_runOpenPanelWithParameters)
        _runOpenPanelWithParameters(webView, parameters, frame, completionHandler);
}

- (void)_webView:(WKWebView *)webView takeFocus:(_WKFocusDirection)direction
{
    if (_takeFocus)
        _takeFocus(webView, direction);
}

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)

- (NSColor *)_webView:(WKWebView *)webView adjustedColorForTopContentInsetColor:(NSColor *)proposedColor
{
    if (_adjustedColorForTopContentInsetColor)
        return _adjustedColorForTopContentInsetColor(webView, proposedColor);

    return proposedColor;
}

#endif // ENABLE(CONTENT_INSET_BACKGROUND_FILL)

#endif // PLATFORM(MAC)

- (void)webViewDidClose:(WKWebView *)webView
{
    if (_webViewDidClose)
        _webViewDidClose(webView);
}

- (void)_webView:(WKWebView *)webView requestStorageAccessPanelForDomain:(NSString *)requestingDomain underCurrentDomain:(NSString *)currentDomain completionHandler:(void (^)(BOOL result))completionHandler
{
    if (_requestStorageAccessPanelForDomain)
        _requestStorageAccessPanelForDomain(webView, requestingDomain, currentDomain, completionHandler);
    else
        completionHandler(YES);
}

- (void)_webView:(WKWebView *)webView requestStorageAccessPanelForDomain:(NSString *)requestingDomain underCurrentDomain:(NSString *)currentDomain forQuirkDomains:(NSDictionary<NSString *, NSArray<NSString *> *> *)quirkDomains completionHandler:(void (^)(BOOL result))completionHandler
{
    if (_requestStorageAccessPanelForQuirksForDomain)
        _requestStorageAccessPanelForQuirksForDomain(webView, requestingDomain, currentDomain, quirkDomains, completionHandler);
    else
        completionHandler(YES);
}

- (void)_webView:(WKWebView *)webView saveDataToFile:(NSData *)data suggestedFilename:(NSString *)suggestedFilename mimeType:(NSString *)mimeType originatingURL:(NSURL *)url
{
    if (_saveDataToFile)
        _saveDataToFile(webView, data, suggestedFilename, mimeType, url);
}

#if HAVE(UI_CONVERSATION_CONTEXT)

- (void)webView:(WKWebView *)webView insertInputSuggestion:(UIInputSuggestion *)suggestion
{
    if (_insertInputSuggestion)
        _insertInputSuggestion(webView, suggestion);
}

#endif

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

- (NSString *)waitForConfirm
{
    EXPECT_FALSE(self.runJavaScriptConfirmPanelWithMessage);

    __block bool finished = false;
    __block RetainPtr<NSString> result;
    self.runJavaScriptConfirmPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(BOOL)) {
        result = message;
        finished = true;
        completionHandler(YES);
    };

    TestWebKitAPI::Util::run(&finished);

    self.runJavaScriptConfirmPanelWithMessage = nil;
    return result.autorelease();
}

- (NSString *)waitForPromptWithReply:(NSString *)reply
{
    EXPECT_FALSE(self.runJavaScriptPromptPanelWithMessage);

    __block bool finished = false;
    __block RetainPtr<NSString> result;
    self.runJavaScriptPromptPanelWithMessage = ^(WKWebView *, NSString *message, NSString *defaultInput, WKFrameInfo *, void (^completionHandler)(NSString *)) {
        EXPECT_TRUE([reply isEqualToString:defaultInput]);
        result = message;
        finished = true;
        completionHandler(@"foo");
    };

    TestWebKitAPI::Util::run(&finished);

    self.runJavaScriptPromptPanelWithMessage = nil;
    return result.autorelease();
}

- (void)waitForDidClose
{
    EXPECT_FALSE(self.webViewDidClose);
    __block bool closed { false };
    self.webViewDidClose = ^(WKWebView *) {
        closed = true;
    };
    TestWebKitAPI::Util::run(&closed);
}

- (void)_webView:(WKWebView *)webView didAttachLocalInspector:(_WKInspector *)inspector
{
    _showedInspector = YES;
}

- (void)_webView:(WKWebView *)webView didReceiveConsoleLogForTesting:(NSString *)log
{
    if (_didReceiveConsoleLogForTesting)
        _didReceiveConsoleLogForTesting(log);
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

- (NSString *)_test_waitForConfirm
{
    EXPECT_FALSE(self.UIDelegate);
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    self.UIDelegate = uiDelegate.get();
    NSString *message = [uiDelegate waitForConfirm];
    self.UIDelegate = nil;
    return message;
}

- (NSString *)_test_waitForPromptWithReply:(NSString *)reply
{
    EXPECT_FALSE(self.UIDelegate);
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    self.UIDelegate = uiDelegate.get();
    NSString *prompt = [uiDelegate waitForPromptWithReply:reply];
    self.UIDelegate = nil;
    return prompt;
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
