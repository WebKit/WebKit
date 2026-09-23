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

#import <WebKit/WKDownloadDelegate.h>
#import <WebKit/WebKit.h>

enum class DownloadCallback : uint8_t {
    WillRedirect,
    AuthenticationChallenge,
    DecideDestination,
#if HAVE(MODERN_DOWNLOADPROGRESS)
    DecidePlaceholderPolicy,
    DidReceivePlaceholderURL,
    DidReceiveFinalURL,
#endif
    DidFinish,
    DidFailWithError,
    NavigationActionBecameDownload,
    NavigationResponseBecameDownload,
    NavigationAction,
    NavigationResponse,
};

@interface TestDownloadDelegate : NSObject<WKDownloadDelegate, WKNavigationDelegate>

@property (nonatomic, copy) void (^willPerformHTTPRedirection)(WKDownload *, NSHTTPURLResponse *, NSURLRequest *, void (^)(WKDownloadRedirectPolicy));
@property (nonatomic, copy) void (^didReceiveAuthenticationChallenge)(WKDownload *, NSURLAuthenticationChallenge *, void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*));
@property (nonatomic, copy) void (^decideDestinationUsingResponse)(WKDownload *, NSURLResponse *, NSString *, void (^)(NSURL *));
@property (nonatomic, copy) void (^downloadDidFinish)(WKDownload *);
@property (nonatomic, copy) void (^didFailWithError)(WKDownload *, NSError *, NSData *);

@property (nonatomic, copy) void (^navigationActionDidBecomeDownload)(WKWebView *, WKNavigationAction *, WKDownload *);
@property (nonatomic, copy) void (^navigationResponseDidBecomeDownload)(WKWebView *, WKNavigationResponse *, WKDownload *);
@property (nonatomic, copy) void (^decidePolicyForNavigationAction)(WKNavigationAction *, void (^)(WKNavigationActionPolicy));
@property (nonatomic, copy) void (^decidePolicyForNavigationResponse)(WKNavigationResponse *, void (^)(WKNavigationResponsePolicy));

#if HAVE(MODERN_DOWNLOADPROGRESS)
@property (nonatomic, copy) void (^decidePlaceholderPolicy)(WKDownload *, void (^)(WKDownloadPlaceholderPolicy, NSURL *));
#endif

- (void)waitForDownloadDidFinish;
- (Vector<DownloadCallback>)takeCallbackRecord;

@end
