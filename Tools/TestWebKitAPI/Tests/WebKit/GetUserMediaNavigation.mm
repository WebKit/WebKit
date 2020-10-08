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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>

static bool okToProceed = false;
static bool shouldReleaseInEnumerate = false;

@interface NavigationWhileGetUserMediaPromptDisplayedUIDelegate : NSObject<WKUIDelegate>
- (void)_webView:(WKWebView *)webView requestMediaCaptureAuthorization: (_WKCaptureDevices)devices decisionHandler:(void (^)(BOOL))decisionHandler;
- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler;
@end

@implementation NavigationWhileGetUserMediaPromptDisplayedUIDelegate
- (void)_webView:(WKWebView *)webView requestMediaCaptureAuthorization: (_WKCaptureDevices)devices decisionHandler:(void (^)(BOOL))decisionHandler
{
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [webView release];
    okToProceed = true;
    decisionHandler(YES);
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

TEST(WebKit, NavigateDuringGetUserMediaPrompt)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;
    auto webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()];
    auto delegate = adoptNS([[NavigationWhileGetUserMediaPromptDisplayedUIDelegate alloc] init]);
    webView.UIDelegate = delegate.get();

    shouldReleaseInEnumerate = false;
    [webView loadTestPageNamed:@"getUserMedia"];
    TestWebKitAPI::Util::run(&okToProceed);
}

TEST(WebKit, NavigateDuringDeviceEnumeration)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;
    auto webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()];
    auto delegate = adoptNS([[NavigationWhileGetUserMediaPromptDisplayedUIDelegate alloc] init]);
    webView.UIDelegate = delegate.get();

    shouldReleaseInEnumerate = true;
    [webView loadTestPageNamed:@"enumerateMediaDevices"];
    TestWebKitAPI::Util::run(&okToProceed);
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
    [configuration setWebsiteDataStore:[[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease]];
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get()._mediaCaptureEnabled = YES;
    preferences._mockCaptureDevicesEnabled = YES;
    auto webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()];
    auto delegate = adoptNS([[NavigationWhileGetUserMediaPromptDisplayedUIDelegate alloc] init]);
    webView.UIDelegate = delegate.get();
    
    [webView loadTestPageNamed:@"enumerateMediaDevices"];
    
    while (![fileManager fileExistsAtPath:hashSaltLocation.path])
        Util::spinRunLoop();
    NSError *error = nil;
    [fileManager removeItemAtPath:tempDir.path error:&error];
    EXPECT_FALSE(error);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
