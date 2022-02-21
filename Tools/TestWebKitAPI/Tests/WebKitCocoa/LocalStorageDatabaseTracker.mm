/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "Test.h"
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/text/WTFString.h>

@interface LocalStorageUIDelegate : NSObject <WKUIDelegate> {
@private
    NSString *expectedMessage;
}
- (void)setExpectedMessage:(NSString *)_expectedMessage;
@end

@implementation LocalStorageUIDelegate
- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    EXPECT_WK_STREQ(expectedMessage, message);
    readyToContinue = true;
    completionHandler();
}

- (void)setExpectedMessage:(NSString *)_expectedMessage
{
    expectedMessage = _expectedMessage;
}
@end

TEST(WKWebView, LocalStorageFetchDataRecords)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto uiDelegate = adoptNS([[LocalStorageUIDelegate alloc] init]);
    webView.get().UIDelegate = uiDelegate.get();
    [uiDelegate.get() setExpectedMessage:@"testValue"];
    [webView loadHTMLString:@"<script>localStorage.setItem('testKey', 'testValue');alert(localStorage.getItem('testKey'));</script>" baseURL:[NSURL URLWithString:@"http://localhost"]];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        readyToContinue = true;
        ASSERT_EQ(1u, dataRecords.count);
        auto origins = [[dataRecords objectAtIndex:0] _originsStrings];
        ASSERT_EQ(1u, origins.count);
        EXPECT_STREQ("http://localhost", [origins objectAtIndex:0].UTF8String);
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(WKWebView, LocalStorageNoRecordAfterGetItem)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto uiDelegate = adoptNS([[LocalStorageUIDelegate alloc] init]);
    webView.get().UIDelegate = uiDelegate.get();
    readyToContinue = false;
    [uiDelegate.get() setExpectedMessage:@"null"];
    [webView loadHTMLString:@"<script>alert(localStorage.getItem('testKey'));</script>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(0u, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    RetainPtr<NSURL> url = [[[configuration websiteDataStore] _configuration] _webStorageDirectory];
    NSArray *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:url.get().path error:nil];
    EXPECT_EQ(0U, [files count]);
}
