/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM)

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "UserMediaCaptureUIDelegate.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>

static bool okToProceed = false;
static bool shouldReleaseInEnumerate = false;

@interface GetUserMeidaNavigationMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation GetUserMeidaNavigationMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
}
@end

@interface NavigationWhileGetUserMediaPromptDisplayedUIDelegate : NSObject<WKUIDelegate>
- (void)webView:(WKWebView *)webView requestMediaCapturePermissionForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame type:(WKMediaCaptureType)type decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler;
- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler;
@end

@implementation NavigationWhileGetUserMediaPromptDisplayedUIDelegate
- (void)webView:(WKWebView *)webView requestMediaCapturePermissionForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame type:(WKMediaCaptureType)type decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler
{
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [webView release];
    okToProceed = true;
    decisionHandler(WKPermissionDecisionGrant);
}

- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler
{
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    if (shouldReleaseInEnumerate)
        [webView release];
    okToProceed = true;
    decisionHandler(@"0x987654321", YES);
}
@end

namespace TestWebKitAPI {

static void initializeMediaCaptureConfiguration(WKWebViewConfiguration* configuration)
{
    auto preferences = [configuration preferences];

    configuration._mediaCaptureEnabled = YES;
    preferences._mediaCaptureRequiresSecureConnection = NO;
    preferences._mockCaptureDevicesEnabled = YES;
    preferences._getUserMediaRequiresFocus = NO;
}

TEST(WebKit, NavigateDuringGetUserMediaPrompt)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[NavigationWhileGetUserMediaPromptDisplayedUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    shouldReleaseInEnumerate = false;
    [webView loadTestPageNamed:@"getUserMedia"];
    TestWebKitAPI::Util::run(&okToProceed);
}

TEST(WebKit, NavigateDuringDeviceEnumeration)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[NavigationWhileGetUserMediaPromptDisplayedUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    shouldReleaseInEnumerate = true;
    [webView loadTestPageNamed:@"enumerateMediaDevices"];
    TestWebKitAPI::Util::run(&okToProceed);
}

TEST(WebKit, DefaultDeviceIdHashSaltsDirectory)
{
    NSFileManager *fileManager = [NSFileManager defaultManager];

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    auto *path = [websiteDataStoreConfiguration deviceIdHashSaltsStorageDirectory].path;

    if ([fileManager fileExistsAtPath:path]) {
        NSError *error = nil;
        [fileManager removeItemAtPath:path error:&error];
        EXPECT_FALSE(error);
    }

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[NavigationWhileGetUserMediaPromptDisplayedUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadTestPageNamed:@"enumerateMediaDevices"];

    while (![fileManager fileExistsAtPath:path])
        Util::spinRunLoop();
    NSError *error = nil;
    [fileManager removeItemAtPath:path error:&error];
    EXPECT_FALSE(error);
}

TEST(WebKit, DeviceIdHashSaltsDirectory)
{
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"CustomPathsTest"] isDirectory:YES];
    NSURL *hashSaltLocation = [tempDir URLByAppendingPathComponent:@"1"];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    EXPECT_FALSE([fileManager fileExistsAtPath:hashSaltLocation.path]);
    
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [websiteDataStoreConfiguration setDeviceIdHashSaltsStorageDirectory:tempDir];
    
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    initializeMediaCaptureConfiguration(configuration.get());
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    auto delegate = adoptNS([[NavigationWhileGetUserMediaPromptDisplayedUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    
    [webView loadTestPageNamed:@"enumerateMediaDevices"];
    
    while (![fileManager fileExistsAtPath:hashSaltLocation.path])
        Util::spinRunLoop();
    NSError *error = nil;
    [fileManager removeItemAtPath:tempDir.path error:&error];
    EXPECT_FALSE(error);
}

TEST(WebKit, DeviceIdHashSaltsRemoval)
{
    auto dataTypeHashSalt = adoptNS([[NSSet alloc] initWithArray:@[WKWebsiteDataTypeHashSalt]]);
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        [configuration setWebsiteDataStore:websiteDataStore.get()];
        auto messageHandler = adoptNS([[GetUserMeidaNavigationMessageHandler alloc] init]);
        [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
        auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        initializeMediaCaptureConfiguration(configuration.get());
        auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
        auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
        [webView setUIDelegate:delegate.get()];

        NSString *htmlString = @"<script> \
        navigator.mediaDevices.enumerateDevices().then(() => { \
            window.webkit.messageHandlers.testHandler.postMessage('done'); \
        }); \
        </script>";
        receivedScriptMessage = false;
        [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org"]];
        TestWebKitAPI::Util::run(&receivedScriptMessage);
        

        done = false;
        [websiteDataStore fetchDataRecordsOfTypes:dataTypeHashSalt.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
            EXPECT_GT(records.count, 0u);
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
    }

    // Create a new WebsiteDataStore to ensure DeviceHashSaltStorage receives delete task before storage is loaded from disk.
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    done = false;
    [websiteDataStore removeDataOfTypes:dataTypeHashSalt.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:dataTypeHashSalt.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_EQ(records.count, 0u);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
