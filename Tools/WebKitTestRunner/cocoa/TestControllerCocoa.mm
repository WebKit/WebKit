/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#import "TestController.h"

#import "CrashReporterInfo.h"
#import "PlatformWebView.h"
#import "TestInvocation.h"
#import "TestRunnerWKWebView.h"
#import <Foundation/Foundation.h>
#import <WebKit/WKContextConfigurationRef.h>
#import <WebKit/WKCookieManager.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKStringCF.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserContentExtensionStore.h>
#import <WebKit/_WKUserContentExtensionStorePrivate.h>
#import <wtf/MainThread.h>

namespace WTR {

static WKWebViewConfiguration *globalWebViewConfiguration;

void initializeWebViewConfiguration(const char* libraryPath, WKStringRef injectedBundlePath, WKContextRef context, WKContextConfigurationRef contextConfiguration)
{
#if WK_API_ENABLED
    [globalWebViewConfiguration release];
    globalWebViewConfiguration = [[WKWebViewConfiguration alloc] init];

    globalWebViewConfiguration.processPool = WTF::adoptNS([[WKProcessPool alloc] _initWithConfiguration:(_WKProcessPoolConfiguration *)contextConfiguration]).get();
    globalWebViewConfiguration.websiteDataStore = (WKWebsiteDataStore *)WKContextGetWebsiteDataStore(context);
    globalWebViewConfiguration._allowUniversalAccessFromFileURLs = YES;

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000)
    globalWebViewConfiguration._applePayEnabled = YES;
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000)
    WKCookieManagerSetCookieStoragePartitioningEnabled(WKContextGetCookieManager(context), true);
#endif

    WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(WKContextGetWebsiteDataStore(context), true);

#if PLATFORM(IOS)
    globalWebViewConfiguration.allowsInlineMediaPlayback = YES;
    globalWebViewConfiguration._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
    globalWebViewConfiguration._invisibleAutoplayNotPermitted = NO;
    globalWebViewConfiguration._mediaDataLoadsAutomatically = YES;
    globalWebViewConfiguration.requiresUserActionForMediaPlayback = NO;
#endif
    globalWebViewConfiguration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
#endif
}

WKContextRef TestController::platformContext()
{
#if WK_API_ENABLED
    return (WKContextRef)globalWebViewConfiguration.processPool;
#else
    return nullptr;
#endif
}

WKPreferencesRef TestController::platformPreferences()
{
#if WK_API_ENABLED
    return (WKPreferencesRef)globalWebViewConfiguration.preferences;
#else
    return nullptr;
#endif
}

void TestController::platformCreateWebView(WKPageConfigurationRef, const TestOptions& options)
{
#if WK_API_ENABLED
    RetainPtr<WKWebViewConfiguration> copiedConfiguration = adoptNS([globalWebViewConfiguration copy]);

#if PLATFORM(IOS)
    if (options.useDataDetection)
        [copiedConfiguration setDataDetectorTypes:WKDataDetectorTypeAll];
    if (options.ignoresViewportScaleLimits)
        [copiedConfiguration setIgnoresViewportScaleLimits:YES];
    if (options.useCharacterSelectionGranularity)
        [copiedConfiguration setSelectionGranularity:WKSelectionGranularityCharacter];
#endif

    m_mainWebView = std::make_unique<PlatformWebView>(copiedConfiguration.get(), options);
#else
    m_mainWebView = std::make_unique<PlatformWebView>(globalWebViewConfiguration, options);
#endif
}

PlatformWebView* TestController::platformCreateOtherPage(PlatformWebView* parentView, WKPageConfigurationRef, const TestOptions& options)
{
#if WK_API_ENABLED
    WKWebViewConfiguration *newConfiguration = [[globalWebViewConfiguration copy] autorelease];
    newConfiguration._relatedWebView = static_cast<WKWebView*>(parentView->platformView());
    return new PlatformWebView(newConfiguration, options);
#else
    return nullptr;
#endif
}

WKContextRef TestController::platformAdjustContext(WKContextRef context, WKContextConfigurationRef contextConfiguration)
{
#if WK_API_ENABLED
    initializeWebViewConfiguration(libraryPathForTesting(), injectedBundlePath(), context, contextConfiguration);
    return (WKContextRef)globalWebViewConfiguration.processPool;
#else
    return nullptr;
#endif
}

void TestController::platformRunUntil(bool& done, double timeout)
{
    NSDate *endDate = (timeout > 0) ? [NSDate dateWithTimeIntervalSinceNow:timeout] : [NSDate distantFuture];

    while (!done && [endDate compare:[NSDate date]] == NSOrderedDescending)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:endDate];
}

void TestController::cocoaResetStateToConsistentValues()
{
#if WK_API_ENABLED
    __block bool doneRemoving = false;
    [[_WKUserContentExtensionStore defaultStore] removeContentExtensionForIdentifier:@"TestContentExtensions" completionHandler:^(NSError *error) {
        doneRemoving = true;
    }];
    platformRunUntil(doneRemoving, 0);
    [[_WKUserContentExtensionStore defaultStore] _removeAllContentExtensions];

    if (PlatformWebView* webView = mainWebView())
        [webView->platformView().configuration.userContentController _removeAllUserContentFilters];
#endif
}

void TestController::platformWillRunTest(const TestInvocation& testInvocation)
{
    setCrashReportApplicationSpecificInformationToURL(testInvocation.url());
}

static NSString * const WebArchivePboardType = @"Apple Web Archive pasteboard type";
static NSString * const WebSubresourcesKey = @"WebSubresources";
static NSString * const WebSubframeArchivesKey = @"WebResourceMIMEType like 'image*'";

unsigned TestController::imageCountInGeneralPasteboard() const
{
#if PLATFORM(MAC)
    NSData *data = [[NSPasteboard generalPasteboard] dataForType:WebArchivePboardType];
#elif PLATFORM(IOS)
    NSData *data = [[UIPasteboard generalPasteboard] valueForPasteboardType:WebArchivePboardType];
#endif
    if (!data)
        return 0;
    
    NSError *error = nil;
    id webArchive = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:NULL error:&error];
    if (error) {
        NSLog(@"Encountered error while serializing Web Archive pasteboard data: %@", error);
        return 0;
    }
    
    NSArray *subItems = [NSArray arrayWithArray:[webArchive objectForKey:WebSubresourcesKey]];
    NSPredicate *predicate = [NSPredicate predicateWithFormat:WebSubframeArchivesKey];
    NSArray *imagesArray = [subItems filteredArrayUsingPredicate:predicate];
    
    if (!imagesArray)
        return 0;
    
    return imagesArray.count;
}

} // namespace WTR
