/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit2/WKFoundation.h>

#if WK_API_ENABLED

@class WKFrameInfo;
@class WKWebViewConfiguration;
@class WKWindowFeatures;

/*! A class that conforms to WKUIDelegate provides methods for presenting native UI on behalf of the webpage.
 */
@protocol WKUIDelegate <NSObject>

@optional

/*! @abstract Create a new WKWebView.
 @param webView The WKWebView invoking the delegate method.
 @param configuration The configuration that must be used when creating the new WKWebView.
 @param navigationAction The navigation action that is causing the new WKWebView to be created.
 @param windowFeatures Window features requested by the webpage.
 @result A new WKWebView or nil.
 @discussion The WKWebView returned must be created with the specified configuration. WebKit will load the request in the returned WKWebView.
 */
- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures;

/*! @abstract Display a JavaScript alert panel.
 @param webView The WKWebView invoking the delegate method.
 @param message The message to display.
 @param frame Information about the frame whose JavaScript initiated this call.
 @param completionHandler The completion handler that should get called after the alert panel has been dismissed.
 @discussion Clients should visually indicate that this panel comes from JavaScript initiated by the specified frame.
 The panel should have a single "OK" button.
 */
- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)())completionHandler;

/*! @abstract Display a JavaScript confirm panel.
 @param webView The WKWebView invoking the delegate method.
 @param message The message to display.
 @param frame Information about the frame whose JavaScript initiated this call.
 @param completionHandler The completion handler that should get called after the confirm panel has been dismissed.
 Pass YES if the user chose OK, NO if the user chose Cancel.
 @discussion Clients should visually indicate that this panel comes from JavaScript initiated by the specified frame.
 The panel should have two buttons, e.g. "OK" and "Cancel".
 */
- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler;

/*! @abstract Display a JavaScript text input panel.
 @param webView The WKWebView invoking the delegate method.
 @param message The message to display.
 @param defaultText The initial text for the text entry area.
 @param frame Information about the frame whose JavaScript initiated this call.
 @param completionHandler The completion handler that should get called after the text input panel has been dismissed. Pass the typed text if the user chose OK, otherwise nil.
 @discussion Clients should visually indicate that this panel comes from JavaScript initiated by the specified frame.
 The panel should have two buttons, e.g. "OK" and "Cancel", and an area to type text.
 */
- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString *result))completionHandler;

@end

#endif // WK_API_ENABLED
