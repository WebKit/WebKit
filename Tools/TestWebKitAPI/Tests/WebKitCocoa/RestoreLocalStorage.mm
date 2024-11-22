/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataRecord.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static void testRestoreLocalStorage(RetainPtr<WKWebView> webView)
{
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadHTMLString:@"<body>webkit.org</body>" baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"localStorage.setItem('key', 'value');" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        doneEvaluatingJavaScript = true;
    }];
    Util::run(&doneEvaluatingJavaScript);

    RetainPtr set = [NSSet setWithObject:WKWebsiteDataTypeLocalStorage];

    __block bool doneFetching = false;
    [[[webView configuration] websiteDataStore] _fetchDataOfTypes:set.get() completionHandler:^(NSData *data) {
        EXPECT_NOT_NULL(data);
        EXPECT_GT([data length], 0u);
        doneFetching = true;
    }];
    Util::run(&doneFetching);
}

TEST(WebKit, RestoreLocalStoragePersistentDataStore)
{
    testRestoreLocalStorage(adoptNS([WKWebView new]));
}

TEST(WebKit, RestoreLocalStorageEphemeralDataStore)
{
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);

    testRestoreLocalStorage(webView);
}

} // namespace TestWebKitAPI
