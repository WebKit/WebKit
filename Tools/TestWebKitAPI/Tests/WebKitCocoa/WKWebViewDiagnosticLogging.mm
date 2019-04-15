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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKDiagnosticLoggingDelegate.h>
#import <wtf/RetainPtr.h>

static bool isDone;

@interface TestLoggingDelegate : NSObject <_WKDiagnosticLoggingDelegate>
@end

@implementation TestLoggingDelegate

- (void)_webView:(WKWebView *)webView logDiagnosticMessage:(NSString *)message description:(NSString *)description valueDictionary:(NSDictionary *)valueDictionary
{
    EXPECT_TRUE([[valueDictionary objectForKey:@"stringKey"] isEqualToString:@"stringValue"]);
    EXPECT_TRUE([[valueDictionary objectForKey:@"uint64Key"] unsignedLongLongValue] == std::numeric_limits<uint64_t>::max());
    EXPECT_TRUE([[valueDictionary objectForKey:@"int64Key"] longLongValue] == std::numeric_limits<int64_t>::min());
    EXPECT_TRUE([[valueDictionary objectForKey:@"boolKey"] boolValue]);
    EXPECT_TRUE([[valueDictionary objectForKey:@"doubleKey"] doubleValue] == 2.7182818284590452353602874);

    isDone = true;
}

@end

TEST(WKWebView, PrivateSessionDiagnosticLoggingDelegate)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    
    auto testLoggingDelegate = adoptNS([[TestLoggingDelegate alloc] init]);
    webView.get()._diagnosticLoggingDelegate = testLoggingDelegate.get();
    
    EXPECT_EQ(testLoggingDelegate.get(), webView.get()._diagnosticLoggingDelegate);
    
    webView.get()._diagnosticLoggingDelegate = nil;
    EXPECT_EQ(nil, webView.get()._diagnosticLoggingDelegate);
}

TEST(WKWebView, DiagnosticLoggingDelegateAfterClose)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    
    auto testLoggingDelegate = adoptNS([[TestLoggingDelegate alloc] init]);
    webView.get()._diagnosticLoggingDelegate = testLoggingDelegate.get();
    
    EXPECT_EQ(testLoggingDelegate.get(), webView.get()._diagnosticLoggingDelegate);
    
    [webView.get() _close];
    EXPECT_EQ(nil, webView.get()._diagnosticLoggingDelegate);
    
    webView.get()._diagnosticLoggingDelegate = testLoggingDelegate.get();
    EXPECT_EQ(nil, webView.get()._diagnosticLoggingDelegate);
}

TEST(WKWebView, DiagnosticLoggingDictionary)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:[WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals"]]);
    auto testLoggingDelegate = adoptNS([TestLoggingDelegate new]);
    [webView _setDiagnosticLoggingDelegate:testLoggingDelegate.get()];
    [webView configuration].preferences._diagnosticLoggingEnabled = YES;

    [webView loadHTMLString:@"<script>window.internals.testDictionaryLogging()</script>" baseURL:nil];
    TestWebKitAPI::Util::run(&isDone);
}
