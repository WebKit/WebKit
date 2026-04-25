/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(CONTENT_FILTERING) && HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import <WebKit/WKErrorPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>

@interface WebContentRestrictionsNavigationDelegate : TestNavigationDelegate
@property (nonatomic) BOOL failedLoadDueToContentFilter;
@end

@implementation WebContentRestrictionsNavigationDelegate

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    if ([error.domain isEqualToString:_WKLegacyErrorDomain] && error.code == _WKErrorCodeFrameLoadBlockedByContentFilter)
        _failedLoadDueToContentFilter = YES;
    else
        FAIL();
}

@end

#define ALLOWLIST_ENABLED_KEY @"white" @"listEnabled"
#define SITE_ALLOWLIST_KEY @"site" @"White" @"list"

static NSDictionary *permissiveConfiguration()
{
    return @{
        @"restrictWeb" : @(0),
        @"useContentFilter" : @(0),
        @"useContentFilterOverrides" : @(0),
        ALLOWLIST_ENABLED_KEY : @(0),
    };
}

static NSDictionary *restrictiveConfiguration()
{
    return @{
        @"restrictWeb" : @(1),
        @"useContentFilter" : @(0),
        @"useContentFilterOverrides" : @(0),
        ALLOWLIST_ENABLED_KEY : @(1),
        SITE_ALLOWLIST_KEY: @[
            @{
                @"address": @"webkit.org",
                @"pageTitle": @"WebKit",
            },
        ],
    };
}

TEST(WebContentRestrictions, ConfigurationFileDoesNotExist)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { "Hello."_s } },
    });

    NSURL *configurationURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];
    EXPECT_FALSE([NSFileManager.defaultManager fileExistsAtPath:configurationURL.path]);

    RetainPtr dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [dataStoreConfiguration setWebContentRestrictionsConfigurationURL:configurationURL];
    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([[WebContentRestrictionsNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_FALSE([navigationDelegate failedLoadDueToContentFilter]);
}

TEST(WebContentRestrictions, ConfigurationFileIsPermissive)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { "Hello."_s } },
    });

    NSURL *configurationURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];
    EXPECT_TRUE([permissiveConfiguration() writeToURL:configurationURL error:nil]);

    RetainPtr dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [dataStoreConfiguration setWebContentRestrictionsConfigurationURL:configurationURL];
    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([[WebContentRestrictionsNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_FALSE([navigationDelegate failedLoadDueToContentFilter]);

    [NSFileManager.defaultManager removeItemAtURL:configurationURL error:nil];
}

TEST(WebContentRestrictions, ConfigurationFileIsRestrictive)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { "Hello."_s } },
    });

    NSURL *configurationURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];
    EXPECT_TRUE([restrictiveConfiguration() writeToURL:configurationURL error:nil]);

    RetainPtr dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [dataStoreConfiguration setWebContentRestrictionsConfigurationURL:configurationURL];
    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([[WebContentRestrictionsNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE([navigationDelegate failedLoadDueToContentFilter]);

    [NSFileManager.defaultManager removeItemAtURL:configurationURL error:nil];
}

TEST(WebContentRestrictions, MultipleConfigurationFiles)
{
    TestWebKitAPI::HTTPServer server({
        { "/a"_s, { "Hello."_s } },
        { "/b"_s, { "Also Hello."_s } },
    });

    NSString *urlStringA = [NSString stringWithFormat:@"http://127.0.0.1:%d/a", server.port()];
    NSString *urlStringB = [NSString stringWithFormat:@"http://127.0.0.1:%d/b", server.port()];

    RetainPtr configurationA = @{
        @"restrictWeb" : @(1),
        @"useContentFilter" : @(0),
        @"useContentFilterOverrides" : @(0),
        ALLOWLIST_ENABLED_KEY : @(1),
        SITE_ALLOWLIST_KEY: @[
            @{
                @"address": urlStringA,
                @"pageTitle": @"Page A",
            },
        ],
    };

    RetainPtr configurationB = @{
        @"restrictWeb" : @(1),
        @"useContentFilter" : @(0),
        @"useContentFilterOverrides" : @(0),
        ALLOWLIST_ENABLED_KEY : @(1),
        SITE_ALLOWLIST_KEY: @[
            @{
                @"address": urlStringB,
                @"pageTitle": @"Page B",
            },
        ],
    };

    NSURL *configurationURLA = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];
    NSURL *configurationURLB = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];

    EXPECT_TRUE([configurationA writeToURL:configurationURLA error:nil]);
    EXPECT_TRUE([configurationB writeToURL:configurationURLB error:nil]);

    RetainPtr dataStoreConfigurationA = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [dataStoreConfigurationA setWebContentRestrictionsConfigurationURL:configurationURLA];
    RetainPtr dataStoreA = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfigurationA.get()]);
    RetainPtr webViewConfigurationA = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfigurationA setWebsiteDataStore:dataStoreA.get()];
    RetainPtr webViewA = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfigurationA.get()]);
    RetainPtr navigationDelegateA = adoptNS([[WebContentRestrictionsNavigationDelegate alloc] init]);
    [webViewA setNavigationDelegate:navigationDelegateA.get()];

    RetainPtr dataStoreConfigurationB = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [dataStoreConfigurationB setWebContentRestrictionsConfigurationURL:configurationURLB];
    RetainPtr dataStoreB = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfigurationB.get()]);
    RetainPtr webViewConfigurationB = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfigurationB setWebsiteDataStore:dataStoreB.get()];
    RetainPtr webViewB = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfigurationB.get()]);
    RetainPtr navigationDelegateB = adoptNS([[WebContentRestrictionsNavigationDelegate alloc] init]);
    [webViewB setNavigationDelegate:navigationDelegateB.get()];

    NSURLRequest *requestA = [NSURLRequest requestWithURL:[NSURL URLWithString:urlStringA]];
    NSURLRequest *requestB = [NSURLRequest requestWithURL:[NSURL URLWithString:urlStringB]];

    [webViewA loadRequest:requestA];
    [navigationDelegateA waitForDidFinishNavigation];
    EXPECT_FALSE([navigationDelegateA failedLoadDueToContentFilter]);

    [webViewA loadRequest:requestB];
    [navigationDelegateA waitForDidFinishNavigation];
    EXPECT_TRUE([navigationDelegateA failedLoadDueToContentFilter]);

    [webViewB loadRequest:requestA];
    [navigationDelegateB waitForDidFinishNavigation];
    EXPECT_TRUE([navigationDelegateB failedLoadDueToContentFilter]);
    [navigationDelegateB setFailedLoadDueToContentFilter:NO];

    [webViewB loadRequest:requestB];
    [navigationDelegateB waitForDidFinishNavigation];
    EXPECT_FALSE([navigationDelegateB failedLoadDueToContentFilter]);

    EXPECT_EQ([webViewA _networkProcessIdentifier], [webViewB _networkProcessIdentifier]);

    [NSFileManager.defaultManager removeItemAtURL:configurationURLA error:nil];
    [NSFileManager.defaultManager removeItemAtURL:configurationURLB error:nil];
}

#endif // ENABLE(CONTENT_FILTERING) && HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
