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

#include "config.h"

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKErrorPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, DeviceManagementRestrictions)
{
    RetainPtr<_WKWebsiteDataStoreConfiguration> dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [dataStoreConfiguration setDeviceManagementRestrictionsEnabled:YES];
    [dataStoreConfiguration setAllLoadsBlockedByDeviceManagementRestrictionsForTesting:YES];

    RetainPtr<WKWebViewConfiguration> webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setWebsiteDataStore:[[[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()] autorelease]];

    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:webViewConfiguration.get() addToWindow:YES]);

    RetainPtr<TestNavigationDelegate> delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool done = false;
    [delegate setDidFailProvisionalNavigation:^(WKWebView *webView, WKNavigation *navigation, NSError *error) {
        EXPECT_WK_STREQ(_WKLegacyErrorDomain, error.domain);
        EXPECT_EQ(_WKErrorCodeFrameLoadBlockedByRestrictions, error.code);
        done = true;
    }];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
}
