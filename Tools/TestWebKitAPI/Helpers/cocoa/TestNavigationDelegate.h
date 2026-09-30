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

#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WebKit.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@class _WKContentRuleListAction;

NS_SWIFT_UI_ACTOR
@interface TestNavigationDelegate : NSObject <WKNavigationDelegate>

@property (nonatomic, copy, nullable) void (^decidePolicyForNavigationAction)(WKNavigationAction *, void (^)(WKNavigationActionPolicy));
@property (nonatomic, copy, nullable) void (^decidePolicyForNavigationActionWithPreferences)(WKNavigationAction *, WKWebpagePreferences *, void (^)(WKNavigationActionPolicy, WKWebpagePreferences *));
@property (nonatomic, copy, nullable) void (^decidePolicyForNavigationResponse)(WKNavigationResponse *, void (^)(WKNavigationResponsePolicy));
@property (nonatomic, copy, nullable) void (^didFailProvisionalNavigation)(WKWebView *, WKNavigation * _Null_unspecified, NSError *);
@property (nonatomic, copy, nullable) void (^didFailProvisionalLoadWithRequestInFrameWithError)(WKWebView *, NSURLRequest *, WKFrameInfo *, NSError *);
@property (nonatomic, copy, nullable) void (^didFailProvisionalLoadInSubframeWithError)(WKWebView *, WKFrameInfo *, NSError *);
@property (nonatomic, copy, nullable) void (^didStartProvisionalNavigation)(WKWebView *, WKNavigation * _Null_unspecified);
@property (nonatomic, copy, nullable) void (^didCommitNavigation)(WKWebView *, WKNavigation * _Null_unspecified);
@property (nonatomic, copy, nullable) void (^didCommitLoadWithRequestInFrame)(WKWebView *, NSURLRequest *, WKFrameInfo *);
@property (nonatomic, copy, nullable) void (^didFinishNavigation)(WKWebView *, WKNavigation * _Null_unspecified);
@property (nonatomic, copy, nullable) void (^didFinishLoadWithRequestInFrame)(WKWebView *, NSURLRequest *, WKFrameInfo *);
@property (nonatomic, copy, nullable) void (^didSameDocumentNavigation)(WKWebView *, WKNavigation * _Null_unspecified);
@property (nonatomic, copy, nullable) void (^renderingProgressDidChange)(WKWebView *, _WKRenderingProgressEvents);
@property (nonatomic, copy, nullable) void (^webContentProcessDidTerminate)(WKWebView *, _WKProcessTerminationReason);
@property (nonatomic, copy, nullable) void (^didReceiveAuthenticationChallenge)(WKWebView *, NSURLAuthenticationChallenge *, void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential * _Nullable));
@property (nonatomic, copy, nullable) void (^contentRuleListPerformedAction)(WKWebView *, NSString *, _WKContentRuleListAction *, NSURL *);
@property (nonatomic, copy, nullable) void (^didChangeLookalikeCharactersFromURL)(WKWebView *, NSURL *, NSURL *);
@property (nonatomic, copy, nullable) void (^didPromptForStorageAccess)(WKWebView *, NSString *, NSString *, BOOL);
@property (nonatomic, copy, nullable) void (^navigationActionDidBecomeDownload)(WKNavigationAction *, WKDownload *);
@property (nonatomic, copy, nullable) void (^navigationResponseDidBecomeDownload)(WKNavigationResponse *, WKDownload *);
@property (nonatomic, copy, nullable) void (^didGeneratePageLoadTiming)(_WKPageLoadTiming *);
@property (nonatomic, copy, nullable) void (^willSubmitForm)(WKFormInfo *);

- (void)allowAnyTLSCertificate;
- (void)waitForDidStartProvisionalNavigation NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (void)waitForDidFinishNavigation NS_SWIFT_UNAVAILABLE("Use the async variant instead.");
- (void)waitForDidFinishNavigationWithCompletionHandler:(void (^)(NSError * _Nullable))completionHandler;
- (void)waitForDidFinishLoadInSubframe NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (void)waitForDidFinishNavigationAndLoadInSubframe NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (void)waitForDidFinishNavigationWithPreferences:(WKWebpagePreferences *)preferences NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (void)waitForDidSameDocumentNavigation NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (_WKProcessTerminationReason)waitForWebContentProcessDidTerminate NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (NSError *)waitForDidFailProvisionalNavigation NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");

@end

@interface WKWebView (TestWebKitAPIExtras)
- (void)_test_waitForDidStartProvisionalNavigation NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (void)_test_waitForDidFinishNavigation NS_SWIFT_UNAVAILABLE("Use the async variant instead.");
- (void)_test_waitForDidFinishNavigationWithCompletionHandler:(void (^)(NSError * _Nullable))completionHandler;
- (void)_test_waitForDidSameDocumentNavigation NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (void)_test_waitForDidFinishNavigationWithPreferences:(WKWebpagePreferences *)preferences NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (void)_test_waitForDidFinishNavigationWithoutPresentationUpdate NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (void)_test_waitForDidFinishNavigationWhileIgnoringSSLErrors NS_SWIFT_UNAVAILABLE("Use the async variant instead.");
- (void)_test_waitForDidFinishNavigationWhileIgnoringSSLErrorsWithCompletionHandler:(void (^)(NSError * _Nullable))completionHandler;
- (void)_test_waitForDidFailProvisionalNavigation NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (_WKProcessTerminationReason)_test_waitForWebContentProcessDidTerminate NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
@end

NS_HEADER_AUDIT_END(nullability, sendability)
