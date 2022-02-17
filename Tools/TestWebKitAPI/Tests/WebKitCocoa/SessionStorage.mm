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

#import "config.h"

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <wtf/RetainPtr.h>

enum class ShouldSessionBeCloned : bool { No, Yes };
static void runSessionStorageInNewWindowTest(NSString* createWindowJS, ShouldSessionBeCloned shouldSessionBeCloned)
{
    __block RetainPtr<TestWKWebView> openedWebView;

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
        [openedWebView setUIDelegate:uiDelegate.get()];
        return openedWebView.get();
    };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setUIDelegate:uiDelegate.get()];
    [webView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;

    [webView loadTestPageNamed:@"alert"];
    [uiDelegate waitForAlert];

    bool ranScript = false;
    [webView evaluateJavaScript:@"sessionStorage.setItem('foo', 'bar')" completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
        ranScript = true;
    }];
    TestWebKitAPI::Util::run(&ranScript);

    // Create the new window.
    [webView evaluateJavaScript:createWindowJS completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
        ranScript = true;
    }];
    [uiDelegate waitForAlert];
    TestWebKitAPI::Util::run(&ranScript);
    EXPECT_TRUE(!!openedWebView);

    // Check if the session storage was cloned or not.
    [openedWebView evaluateJavaScript:@"sessionStorage.getItem('foo') || ''" completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
        NSString* sessionValue = result;
        if (shouldSessionBeCloned == ShouldSessionBeCloned::Yes)
            EXPECT_WK_STREQ(@"bar", sessionValue);
        else
            EXPECT_WK_STREQ(@"", sessionValue);
        ranScript = true;
    }];
    TestWebKitAPI::Util::run(&ranScript);
}

TEST(SessionStorage, WindowOpenClonesSession)
{
    runSessionStorageInNewWindowTest(@"open(document.URL) && true", ShouldSessionBeCloned::Yes);
}

TEST(SessionStorage, WindowOpenNoOpenerDoesNotCloneSession)
{
    runSessionStorageInNewWindowTest(@"open(document.URL, 'newWindow', 'noopener') && true", ShouldSessionBeCloned::No);
}

TEST(SessionStorage, WindowOpenNoReferrerDoesNotCloneSession)
{
    runSessionStorageInNewWindowTest(@"open(document.URL, 'newWindow', 'noreferrer') && true", ShouldSessionBeCloned::No);
}

// target=_blank implies rel=noopener.
TEST(SessionStorage, BlankLinkTargetDoesNotCloneSession)
{
    runSessionStorageInNewWindowTest(@"a = document.createElement('a'); a.href = document.URL; a.target = '_blank'; a.click()", ShouldSessionBeCloned::No);
}

TEST(SessionStorage, BlankLinkTargetRelOpenerClonesSession)
{
    runSessionStorageInNewWindowTest(@"a = document.createElement('a'); a.href = document.URL; a.target = '_blank'; a.rel='opener'; a.click()", ShouldSessionBeCloned::Yes);
}

TEST(SessionStorage, NonBlankLinkTargetClonesSession)
{
    runSessionStorageInNewWindowTest(@"a = document.createElement('a'); a.href = document.URL; a.target = 'newWindow'; a.click()", ShouldSessionBeCloned::Yes);
}

TEST(SessionStorage, NonBlankLinkTargetRelNoopenerDoesNotCloneSession)
{
    runSessionStorageInNewWindowTest(@"a = document.createElement('a'); a.href = document.URL; a.target = 'newWindow'; a.rel='noopener'; a.click()", ShouldSessionBeCloned::No);
}

TEST(SessionStorage, ClearByModificationTime)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<script>sessionStorage.setItem('key', 'value')</script>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];

    auto websiteDataTypes = adoptNS([[NSSet alloc] initWithArray:@[WKWebsiteDataTypeSessionStorage]]);
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(1u, dataRecords.count);
        EXPECT_WK_STREQ("webkit.org", [[dataRecords firstObject] displayName]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:websiteDataTypes.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(0u, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}
