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

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebsiteDataStore.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

static bool readyToContinue;

@interface LocalStorageClearMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation LocalStorageClearMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    readyToContinue = true;
}

@end

TEST(WKWebView, LocalStorageClear)
{
    RetainPtr<LocalStorageClearMessageHandler> handler = adoptNS([[LocalStorageClearMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    [configuration _setNeedsStorageAccessFromFileURLsQuirk:NO];
    [configuration _setAllowUniversalAccessFromFileURLs:YES];

    @autoreleasepool {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

        NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"LocalStorageClear" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
        [webView loadRequest:request];

        TestWebKitAPI::Util::run(&readyToContinue);
        readyToContinue = false;

        webView = nil;
    }

    NSString *dbPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/file__0.localstorage" stringByExpandingTildeInPath];
    NSString *dbSHMPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/file__0.localstorage-shm" stringByExpandingTildeInPath];
    NSString *dbWALPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/file__0.localstorage-wal" stringByExpandingTildeInPath];
    NSString *trackerPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/StorageTracker.db" stringByExpandingTildeInPath];
    NSString *trackerSHMPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/StorageTracker.db-shm" stringByExpandingTildeInPath];
    NSString *trackerWALPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/StorageTracker.db-wal" stringByExpandingTildeInPath];

    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:dbPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:dbSHMPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:dbWALPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:trackerPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:trackerSHMPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:trackerWALPath]);
        readyToContinue = true;
    }];

    TestWebKitAPI::Util::run(&readyToContinue);
}

static long long fileSize(NSURL* url)
{
    NSError *error = nil;
    NSDictionary *fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:url.path error:&error];
    if (error)
        return -1;
    return [[fileAttributes objectForKey:NSFileSize] longLongValue];
}

NSString *swizzledBundleIdentifierWebBookmarksD()
{
    return @"com.apple.webbookmarksd";
}

NSString *defaultWebsiteCacheDirectory()
{
#if PLATFORM(IOS_FAMILY)
    return nil;
#else
    return @"~/Library/Caches/TestWebKitAPI/WebKit";
#endif
}

NSString *defaultApplicationCacheDirectory()
{
#if PLATFORM(IOS_FAMILY)
    // This is to mirror a quirk in WebsiteDataStore::defaultApplicationCacheDirectory and catch any regressions.
    return [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Caches/com.apple.WebAppCache"];
#else
    return [defaultWebsiteCacheDirectory() stringByAppendingString:@"/OfflineWebApplicationCache"];
#endif
}

TEST(WKWebView, ClearAppCache)
{
#if PLATFORM(IOS_FAMILY)
    // On iOS, MobileSafari and webbookmarksd need to share the same AppCache directory.
    InstanceMethodSwizzler swizzle([NSBundle class], @selector(bundleIdentifier), reinterpret_cast<IMP>(swizzledBundleIdentifierWebBookmarksD));
#endif

    // Start with a clean slate of WebsiteData.
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    // Start with a clean slate of Website caches.
    if (auto *websiteCacheDirectory = defaultWebsiteCacheDirectory()) {
        NSURL *websiteCacheURL = [NSURL fileURLWithPath:[websiteCacheDirectory stringByExpandingTildeInPath]];
        [[NSFileManager defaultManager] removeItemAtURL:websiteCacheURL error:nil];
    }

    if (auto *appCacheDirectory = defaultApplicationCacheDirectory()) {
        NSURL *appCacheURL = [NSURL fileURLWithPath:[appCacheDirectory stringByExpandingTildeInPath]];
        [[NSFileManager defaultManager] removeItemAtURL:appCacheURL error:nil];
    }

    NSURL *dbResourceURL = [[NSBundle mainBundle] URLForResource:@"ApplicationCache" withExtension:@"db" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *shmResourceURL = [[NSBundle mainBundle] URLForResource:@"ApplicationCache" withExtension:@"db-shm" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *walResourceURL = [[NSBundle mainBundle] URLForResource:@"ApplicationCache" withExtension:@"db-wal" subdirectory:@"TestWebKitAPI.resources"];

    NSURL *targetURL = [NSURL fileURLWithPath:[defaultApplicationCacheDirectory() stringByExpandingTildeInPath]];
    [[NSFileManager defaultManager] createDirectoryAtURL:targetURL withIntermediateDirectories:YES attributes:nil error:nil];

    NSURL *dbTargetURL = [targetURL URLByAppendingPathComponent:@"ApplicationCache.db"];
    NSURL *walTargetURL = [targetURL URLByAppendingPathComponent:@"ApplicationCache.db-wal"];
    NSURL *shmTargetURL = [targetURL URLByAppendingPathComponent:@"ApplicationCache.db-shm"];

    EXPECT_EQ(fileSize(dbTargetURL), -1);
    EXPECT_EQ(fileSize(walTargetURL), -1);
    EXPECT_EQ(fileSize(shmTargetURL), -1);

    // Copy the resources from the bundle to ~/Library/...
    [[NSFileManager defaultManager] copyItemAtURL:dbResourceURL toURL:dbTargetURL error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:shmResourceURL toURL:shmTargetURL error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:walResourceURL toURL:walTargetURL error:nil];
    EXPECT_GT(fileSize(dbTargetURL), 0);
    EXPECT_GT(fileSize(shmTargetURL), 0);
    EXPECT_GT(fileSize(walTargetURL), 0);

    static size_t originalWebsiteDataRecordCount;

    // Make sure there is a record in the WKWebsiteDataStore.
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords)
    {
        EXPECT_EQ(websiteDataRecords.count, 1ul);
        for (WKWebsiteDataRecord *record in websiteDataRecords) {
            EXPECT_STREQ("127.0.0.1", [record.displayName UTF8String]);
            for (NSString *type in record.dataTypes)
                EXPECT_STREQ([WKWebsiteDataTypeOfflineWebApplicationCache UTF8String], [type UTF8String]);
        }

        originalWebsiteDataRecordCount = websiteDataRecords.count;
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^()
    {
        NSURL *targetURL = [NSURL fileURLWithPath:[defaultApplicationCacheDirectory() stringByExpandingTildeInPath]];
        NSURL *walTargetURL = [targetURL URLByAppendingPathComponent:@"ApplicationCache.db-wal"];
        
        // Make sure there is no record in the WKWebsiteDataStore.
        EXPECT_EQ(fileSize(walTargetURL), 0);
        [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords)
        {
            EXPECT_EQ(websiteDataRecords.count, originalWebsiteDataRecordCount - 1);
            readyToContinue = true;
        }];
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}
