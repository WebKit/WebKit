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

#import <WebKit/WKNavigationDelegate.h>

#import <WebKit/WKFrameInfo.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKSameDocumentNavigationType.h>

#if WK_API_ENABLED

@class _WKWebsitePolicies;

static const WKNavigationActionPolicy _WKNavigationActionPolicyDownload = (WKNavigationActionPolicy)(WKNavigationActionPolicyAllow + 1);
static const WKNavigationActionPolicy WK_API_AVAILABLE(macosx(10.11), ios(9.0)) _WKNavigationActionPolicyAllowWithoutTryingAppLink = (WKNavigationActionPolicy)(WKNavigationActionPolicyAllow + 2);

static const WKNavigationResponsePolicy _WKNavigationResponsePolicyBecomeDownload = (WKNavigationResponsePolicy)(WKNavigationResponsePolicyAllow + 1);

@protocol WKNavigationDelegatePrivate <WKNavigationDelegate>

@optional

- (void)_webView:(WKWebView *)webView navigation:(WKNavigation *)navigation didFailProvisionalLoadInSubframe:(WKFrameInfo *)subframe withError:(NSError *)error;

- (void)_webView:(WKWebView *)webView navigationDidFinishDocumentLoad:(WKNavigation *)navigation;
- (void)_webView:(WKWebView *)webView navigation:(WKNavigation *)navigation didSameDocumentNavigation:(_WKSameDocumentNavigationType)navigationType;

- (void)_webView:(WKWebView *)webView renderingProgressDidChange:(_WKRenderingProgressEvents)progressEvents;

- (BOOL)_webView:(WKWebView *)webView canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace;
- (void)_webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge;

- (void)_webViewWebProcessDidCrash:(WKWebView *)webView;
- (void)_webViewWebProcessDidBecomeResponsive:(WKWebView *)webView;
- (void)_webViewWebProcessDidBecomeUnresponsive:(WKWebView *)webView;

- (NSData *)_webCryptoMasterKeyForWebView:(WKWebView *)webView;

- (void)_webViewDidBeginNavigationGesture:(WKWebView *)webView;
// Item is nil if the gesture ended without navigation.
- (void)_webViewDidEndNavigationGesture:(WKWebView *)webView withNavigationToBackForwardListItem:(WKBackForwardListItem *)item;
// Only called if how the gesture will end (with or without navigation) is known before it ends.
- (void)_webViewWillEndNavigationGesture:(WKWebView *)webView withNavigationToBackForwardListItem:(WKBackForwardListItem *)item;
- (void)_webView:(WKWebView *)webView willSnapshotBackForwardListItem:(WKBackForwardListItem *)item;
- (void)_webViewDidRemoveNavigationGestureSnapshot:(WKWebView *)webView WK_API_AVAILABLE(macosx(10.12), ios(10.0));
- (void)_webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy, _WKWebsitePolicies *))decisionHandler WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));

#if TARGET_OS_IPHONE
- (void)_webView:(WKWebView *)webView didStartLoadForQuickLookDocumentInMainFrameWithFileName:(NSString *)fileName uti:(NSString *)uti;
- (void)_webView:(WKWebView *)webView didFinishLoadForQuickLookDocumentInMainFrame:(NSData *)documentData;
#endif

@end

#endif
