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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

static NSString *loadableURL = @"data:text/html,no%20error%20A";

TEST(WKProcessPool, WarmInitialProcess)
{
    auto pool = adoptNS([[WKProcessPool alloc] init]);

    EXPECT_FALSE([pool _hasPrewarmedWebProcess]);

    [pool _warmInitialProcess];

    EXPECT_TRUE([pool _hasPrewarmedWebProcess]);

    [pool _warmInitialProcess]; // No-op.

    EXPECT_TRUE([pool _hasPrewarmedWebProcess]);
}

enum class ShouldUseEphemeralStore { No, Yes };
static void runInitialWarmedProcessUsedTest(ShouldUseEphemeralStore shouldUseEphemeralStore)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().prewarmsProcessesAutomatically = NO;

    auto pool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    [pool _warmInitialProcess];

    EXPECT_TRUE([pool _hasPrewarmedWebProcess]);
    EXPECT_EQ(1U, [pool _webPageContentProcessCount]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = pool.get();
    if (shouldUseEphemeralStore == ShouldUseEphemeralStore::Yes)
        configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL]]];

    EXPECT_FALSE([pool _hasPrewarmedWebProcess]);
    EXPECT_EQ(1U, [pool _webPageContentProcessCount]);

    [webView _test_waitForDidFinishNavigation];

    EXPECT_FALSE([pool _hasPrewarmedWebProcess]);
    EXPECT_EQ(1U, [pool _webPageContentProcessCount]);
}

TEST(WKProcessPool, InitialWarmedProcessUsed)
{
    runInitialWarmedProcessUsedTest(ShouldUseEphemeralStore::No);
}

TEST(WKProcessPool, InitialWarmedProcessUsedForEphemeralSession)
{
    runInitialWarmedProcessUsedTest(ShouldUseEphemeralStore::Yes);
}

TEST(WKProcessPool, AutomaticProcessWarming)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().prewarmsProcessesAutomatically = YES;
    auto pool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    EXPECT_FALSE([pool _hasPrewarmedWebProcess]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = pool.get();
    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    EXPECT_FALSE([pool _hasPrewarmedWebProcess]);
    EXPECT_EQ(1U, [pool _webPageContentProcessCount]);

    [webView1 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL]]];
    [webView1 _test_waitForDidFinishNavigation];

    while (![pool _hasPrewarmedWebProcess])
        TestWebKitAPI::Util::sleep(0.01);

    EXPECT_TRUE([pool _hasPrewarmedWebProcess]);
    EXPECT_EQ(2U, [pool _webPageContentProcessCount]);

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL]]];

    EXPECT_FALSE([pool _hasPrewarmedWebProcess]);
    EXPECT_EQ(2U, [pool _webPageContentProcessCount]);
}

TEST(WKProcessPool, PrewarmedProcessCrash)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().prewarmsProcessesAutomatically = NO;

    auto pool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    [pool _warmInitialProcess];

    EXPECT_TRUE([pool _hasPrewarmedWebProcess]);
    EXPECT_EQ(1U, [pool _webPageContentProcessCount]);

    // Wait for prewarmed process to finish launching.
    while (![pool _prewarmedProcessIdentifier])
        TestWebKitAPI::Util::sleep(0.01);

    kill([pool _prewarmedProcessIdentifier], 9);

    while ([pool _hasPrewarmedWebProcess])
        TestWebKitAPI::Util::sleep(0.01);
}

TEST(WebKit, TryUsingPrewarmedProcessThatJustCrashed)
{
    auto pool = adoptNS([[WKProcessPool alloc] init]);

    EXPECT_FALSE([pool _hasPrewarmedWebProcess]);

    [pool _warmInitialProcess];
    EXPECT_TRUE([pool _hasPrewarmedWebProcess]);

    // Wait for prewarmed process to finish launching.
    while (![pool _prewarmedProcessIdentifier])
        TestWebKitAPI::Util::sleep(0.01);

    // Kill the prewarmed process.
    kill([pool _prewarmedProcessIdentifier], 9);

    // Try using the prewarmed process right away.
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = pool.get();
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL]]];
    [webView _test_waitForDidFinishNavigation];
}
