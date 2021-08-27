/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

// FIXME: Implement migration from older databases and get these tests working as they should again.

TEST(PrivateClickMeasurement, AddMissingPCMFraudPreventionColumns)
{
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];

    auto defaultFileManager = [NSFileManager defaultManager];
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];
    NSURL *itpRootURL = [[dataStore _configuration] _resourceLoadStatisticsDirectory];
    NSURL *fileURL = [itpRootURL URLByAppendingPathComponent:@"observations.db"];
    [defaultFileManager removeItemAtPath:itpRootURL.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:itpRootURL.path]);

    // Load an incorrect database schema.
    [defaultFileManager createDirectoryAtURL:itpRootURL withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *newFileURL = [[NSBundle mainBundle] URLForResource:@"pcmWithoutFraudPreventionDatabase" withExtension:@"db" subdirectory:@"TestWebKitAPI.resources"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:newFileURL.path]);
    [defaultFileManager copyItemAtPath:newFileURL.path toPath:fileURL.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:fileURL.path]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool: sharedProcessPool];
    configuration.get().websiteDataStore = dataStore;

    // We need an active NetworkProcess to perform PCM operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    // Check the columns of PCM tables to make sure fraud prevention columns were added.
    __block bool doneFlag = false;
    [dataStore _statisticsDatabaseColumnsForTable:@"UnattributedPrivateClickMeasurement" completionHandler:^(NSArray<NSString *> *tableSchema) {
        EXPECT_FALSE([tableSchema containsObject:@"token"]);
        EXPECT_FALSE([tableSchema containsObject:@"keyID"]);
        EXPECT_FALSE([tableSchema containsObject:@"signature"]);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    doneFlag = false;
    [dataStore _statisticsDatabaseColumnsForTable:@"AttributedPrivateClickMeasurement" completionHandler:^(NSArray<NSString *> *tableSchema) {
        EXPECT_FALSE([tableSchema containsObject:@"token"]);
        EXPECT_FALSE([tableSchema containsObject:@"keyID"]);
        EXPECT_FALSE([tableSchema containsObject:@"signature"]);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
}

TEST(PrivateClickMeasurement, AddMissingPCMReportingColumns)
{
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];

    auto defaultFileManager = [NSFileManager defaultManager];
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];
    NSURL *itpRootURL = [[dataStore _configuration] _resourceLoadStatisticsDirectory];
    NSURL *fileURL = [itpRootURL URLByAppendingPathComponent:@"observations.db"];
    [defaultFileManager removeItemAtPath:itpRootURL.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:itpRootURL.path]);

    // Load an incorrect database schema.
    [defaultFileManager createDirectoryAtURL:itpRootURL withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *newFileURL = [[NSBundle mainBundle] URLForResource:@"pcmWithoutReportingColumns" withExtension:@"db" subdirectory:@"TestWebKitAPI.resources"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:newFileURL.path]);
    [defaultFileManager copyItemAtPath:newFileURL.path toPath:fileURL.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:fileURL.path]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool: sharedProcessPool];
    configuration.get().websiteDataStore = dataStore;

    // We need an active NetworkProcess to perform PCM operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    // Check the columns of the AttributedPrivateClickMeasurement table to make sure new reporting columns were added.
    __block bool doneFlag = false;
    [dataStore _statisticsDatabaseColumnsForTable:@"AttributedPrivateClickMeasurement" completionHandler:^(NSArray<NSString *> *tableSchema) {
        EXPECT_FALSE([tableSchema containsObject:@"earliestTimeToSendToSource"]);
        EXPECT_FALSE([tableSchema containsObject:@"earliestTimeToSendToDestination"]);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
}

TEST(PrivateClickMeasurement, RenameDestinationSiteColumn)
{
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];

    auto defaultFileManager = [NSFileManager defaultManager];
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];
    NSURL *itpRootURL = [[dataStore _configuration] _resourceLoadStatisticsDirectory];
    NSURL *fileURL = [itpRootURL URLByAppendingPathComponent:@"observations.db"];
    [defaultFileManager removeItemAtPath:itpRootURL.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:itpRootURL.path]);

    // Load an incorrect database schema.
    [defaultFileManager createDirectoryAtURL:itpRootURL withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *newFileURL = [[NSBundle mainBundle] URLForResource:@"pcmWithoutReportingColumns" withExtension:@"db" subdirectory:@"TestWebKitAPI.resources"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:newFileURL.path]);
    [defaultFileManager copyItemAtPath:newFileURL.path toPath:fileURL.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:fileURL.path]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool: sharedProcessPool];
    configuration.get().websiteDataStore = dataStore;

    // We need an active NetworkProcess to perform PCM operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    // Check the columns of the AttributedPrivateClickMeasurement table to make sure new reporting columns were added.
    __block bool doneFlag = false;
    [dataStore _statisticsDatabaseColumnsForTable:@"AttributedPrivateClickMeasurement" completionHandler:^(NSArray<NSString *> *tableSchema) {
        EXPECT_FALSE([tableSchema containsObject:@"destinationSiteDomainID"]);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
}
