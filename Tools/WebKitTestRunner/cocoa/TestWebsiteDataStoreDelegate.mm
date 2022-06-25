/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "TestWebsiteDataStoreDelegate.h"

#import "PlatformWebView.h"
#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import <WebKit/WKWebView.h>
#import <wtf/UniqueRef.h>

@implementation TestWebsiteDataStoreDelegate { }
- (instancetype)init
{
    _shouldAllowRaisingQuota = false;
    return self;
}

- (void)requestStorageSpace:(NSURL *)mainFrameURL frameOrigin:(NSURL *)frameURL quota:(NSUInteger)quota currentSize:(NSUInteger)currentSize spaceRequired:(NSUInteger)spaceRequired decisionHandler:(void (^)(unsigned long long))decisionHandler
{
    decisionHandler(_shouldAllowRaisingQuota ? quota + currentSize + spaceRequired : quota);
}

- (void)setAllowRaisingQuota:(BOOL)shouldAllowRaisingQuota
{
    _shouldAllowRaisingQuota = shouldAllowRaisingQuota;
}

- (void)didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential * _Nullable credential))completionHandler
{
    NSString *method = challenge.protectionSpace.authenticationMethod;
    if ([method isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        if (_shouldAllowAnySSLCertificate)
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust: challenge.protectionSpace.serverTrust]);
        else
            completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        return;
    }
    completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
}

- (void)setAllowAnySSLCertificate:(BOOL)shouldAllowAnySSLCertificate
{
    _shouldAllowAnySSLCertificate = shouldAllowAnySSLCertificate;
}

- (void)websiteDataStore:(WKWebsiteDataStore *)dataStore openWindow:(NSURL *)url fromServiceWorkerOrigin:(WKSecurityOrigin *)serviceWorkerOrigin completionHandler:(void (^)(WKWebView *newWebView))completionHandler
{
    auto* newView = WTR::TestController::singleton().createOtherPlatformWebView(nullptr, nullptr, nullptr, nullptr);
    WKWebView *webView = newView->platformView();

    ASSERT(webView.configuration.websiteDataStore == dataStore);

    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    completionHandler(webView);
}

@end
