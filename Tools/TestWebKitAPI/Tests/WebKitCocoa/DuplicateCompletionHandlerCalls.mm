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

#import "config.h"
#import <WebKit/WebKit.h>

#if WK_API_ENABLED

#import "PlatformUtilities.h"
#import "Utilities.h"
#import <WebKit/WKPreferencesRef.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInputDelegate.h>
#import <wtf/RetainPtr.h>

using namespace TestWebKitAPI;

static bool testFinished;

@interface DuplicateCompletionHandlerCallsDelegate : NSObject <WKNavigationDelegate, WKUIDelegate, _WKInputDelegate>
@end

@implementation DuplicateCompletionHandlerCallsDelegate

static void expectException(void (^completionHandler)())
{
    bool exceptionThrown = false;
    @try {
        completionHandler();
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInternalInconsistencyException, exception.name);
        exceptionThrown = true;
    }
    EXPECT_TRUE(exceptionThrown);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    decisionHandler(WKNavigationActionPolicyAllow);

    expectException(^ {
        decisionHandler(WKNavigationActionPolicyAllow);
    });
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    decisionHandler(WKNavigationResponsePolicyAllow);

    expectException(^ {
        decisionHandler(WKNavigationResponsePolicyAllow);
    });
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)())completionHandler
{
    completionHandler();

    expectException(^ {
        completionHandler();
    });
}

- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL))completionHandler
{
    completionHandler(YES);

    expectException(^ {
        completionHandler(YES);
    });
}

- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString * _Nullable))completionHandler
{
    completionHandler(nil);

    expectException(^ {
        completionHandler(nil);
    });
}

- (void)_webView:(WKWebView *)webView decideDatabaseQuotaForSecurityOrigin:(WKSecurityOrigin *)securityOrigin currentQuota:(unsigned long long)currentQuota currentOriginUsage:(unsigned long long)currentOriginUsage currentDatabaseUsage:(unsigned long long)currentUsage expectedUsage:(unsigned long long)expectedUsage decisionHandler:(void (^)(unsigned long long newQuota))decisionHandler
{
    decisionHandler(expectedUsage);

    expectException(^ {
        decisionHandler(expectedUsage);
    });
}

- (void)_webView:(WKWebView *)webView willSubmitFormValues:(NSDictionary *)values userObject:(NSObject<NSSecureCoding> *)userObject submissionHandler:(void (^)())submissionHandler
{
    submissionHandler();

    expectException(^ {
        submissionHandler();
    });

    testFinished = true;
}

@end

TEST(WebKit, DuplicateCompletionHandlerCalls)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    auto preferences = (__bridge WKPreferencesRef)[[webView configuration] preferences];
    WKPreferencesSetWebSQLDisabled(preferences, false);

    auto delegate = adoptNS([[DuplicateCompletionHandlerCallsDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];
    [webView _setInputDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"duplicate-completion-handler-calls" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&testFinished);
}

#endif // WK_API_ENABLED
