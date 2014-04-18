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

@class WKNavigation;
@class WKNavigationAction;
@class WKNavigationResponse;
@class WKWebView;

/*! @enum WKNavigationActionPolicy
 @abstract the policy to pass back to the decision handler in webView:decidePolicyForNavigationAction:decisionHandler:.
 @constant WKNavigationActionPolicyCancel   Cancel the navigation.
 @constant WKNavigationActionPolicyAllow    Allow the navigation to continue.
 */
typedef NS_ENUM(NSInteger, WKNavigationActionPolicy) {
    WKNavigationActionPolicyCancel,
    WKNavigationActionPolicyAllow,
};

/*! @enum WKNavigationResponsePolicy
 @abstract the policy to pass back to the decision handler in webView:decidePolicyForNavigationResponse:decisionHandler:.
 @constant WKNavigationResponsePolicyCancel   Cancel the navigation.
 @constant WKNavigationResponsePolicyAllow    Allow the navigation to continue.
 */
typedef NS_ENUM(NSInteger, WKNavigationResponsePolicy) {
    WKNavigationResponsePolicyCancel,
    WKNavigationResponsePolicyAllow,
};

/*! A class that conforms to WKNavigationDelegate can provide methods for deciding load policy for main frame and subframe loads
 and track load progress for main frame loads.
 */
@protocol WKNavigationDelegate <NSObject>

@optional

/*! @abstract Decides whether a navigation should be allowed or not.
 @param webView The WKWebView invoking the delegate method.
 @param navigationAction A description of the action that triggered the navigation request.
 @param decisionHandler The decision handler that should be called to allow or cancel the load.
 */
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler;

/*! @abstract Decides whether a navigation should be allowed or cancelled once its response is known.
 @param webView The WKWebView invoking the delegate method.
 @param navigationResponse A description of the navigation response.
 @param decisionHandler The decision handler that should be call to allow or cancel the load.
 */
- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler;

/*! @abstract Invoked when a main frame page load starts.
 @param webView The WKWebView invoking the delegate method.
 @param navigation The navigation.
 */
- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation;

/*! @abstract Invoked when a server redirect is received for the main frame.
 @param webView The WKWebView invoking the delegate method.
 @param navigation The navigation.
 */
- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)navigation;

/*! @abstract Invoked if an error occurs when starting to load data for the main frame.
 @param webView The WKWebView invoking the delegate method.
 @param navigation The navigation.
 @param error Specifies the type of error that occurred during the load.
 */
- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error;

/*! @abstract Invoked when content starts arriving for the main frame.
 @param webView The WKWebView invoking the delegate method.
 @param navigation The navigation.
 */
- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation;

/*! @abstract Invoked when a main frame load completes.
 @param webView The WKWebView invoking the delegate method.
 @param navigation The navigation.
 */
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation;

/*! @abstract Invoked if an error occurs loading a committed main frame page load.
 @param webView The WKWebView invoking the delegate method.
 @param navigation The navigation.
 @param error Specifies the type of error that occurred during the load.
 */
- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error;

@end

#endif
