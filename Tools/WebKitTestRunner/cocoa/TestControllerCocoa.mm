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
#import "StringFunctions.h"
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
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserContentExtensionStore.h>
#import <WebKit/_WKUserContentExtensionStorePrivate.h>
#import <wtf/MainThread.h>

namespace WTR {

#if WK_API_ENABLED
static NSString* toNSString(WKStringRef string)
{
    return [NSString stringWithCString:toWTFString(string).utf8().data()];
}
#endif

static WKWebViewConfiguration *globalWebViewConfiguration;

void initializeWebViewConfiguration(const char* libraryPath, WKStringRef injectedBundlePath, WKContextRef context, WKContextConfigurationRef contextConfiguration)
{
#if WK_API_ENABLED
    [globalWebViewConfiguration release];
    globalWebViewConfiguration = [[WKWebViewConfiguration alloc] init];

    globalWebViewConfiguration.processPool = WTF::adoptNS([[WKProcessPool alloc] _initWithConfiguration:(_WKProcessPoolConfiguration *)contextConfiguration]).get();
    globalWebViewConfiguration.websiteDataStore = (WKWebsiteDataStore *)WKContextGetWebsiteDataStore(context);
    globalWebViewConfiguration._allowUniversalAccessFromFileURLs = YES;

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200) || PLATFORM(IOS)
    globalWebViewConfiguration._applePayEnabled = YES;
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || PLATFORM(IOS)
    WKCookieManagerSetCookieStoragePartitioningEnabled(WKContextGetCookieManager(context), true);
#endif

    [globalWebViewConfiguration.websiteDataStore _setResourceLoadStatisticsEnabled:YES];
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetShouldSubmitTelemetry:NO];

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

void TestController::removeAllSessionCredentials()
{
#if WK_API_ENABLED
    auto types = adoptNS([[NSSet alloc] initWithObjects:_WKWebsiteDataTypeCredentials, nil]);
    [globalWebViewConfiguration.websiteDataStore removeDataOfTypes:types.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        m_currentInvocation->didRemoveAllSessionCredentials();
    }];
#endif
}

#if WK_API_ENABLED
void TestController::setStatisticsLastSeen(WKStringRef hostName, double seconds)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetLastSeen:seconds forHost:toNSString(hostName)];
}
    
void TestController::setStatisticsPrevalentResource(WKStringRef hostName, bool value)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetIsPrevalentResource:value forHost:toNSString(hostName)];
}

bool TestController::isStatisticsPrevalentResource(WKStringRef hostName)
{
    __block bool isDataReady = false;
    __block bool isPrevalentResource = false;
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsIsPrevalentResource:toNSString(hostName) completionHandler:^(BOOL _isPrevalentResource) {
        isPrevalentResource = _isPrevalentResource;
        isDataReady = true;
    }];
    platformRunUntil(isDataReady, 0);

    return isPrevalentResource;
}

bool TestController::isStatisticsRegisteredAsSubFrameUnder(WKStringRef subFrameHost, WKStringRef topFrameHost)
{
    __block bool isDataReady = false;
    __block bool isRegisteredAsSubFrameUnder = false;
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsIsRegisteredAsSubFrameUnder:toNSString(subFrameHost) topFrameHost:toNSString(topFrameHost) completionHandler:^(BOOL _isRegisteredAsSubFrameUnder) {
        isRegisteredAsSubFrameUnder = _isRegisteredAsSubFrameUnder;
        isDataReady = true;
    }];
    platformRunUntil(isDataReady, 0);
    
    return isRegisteredAsSubFrameUnder;
}

bool TestController::isStatisticsRegisteredAsRedirectingTo(WKStringRef hostRedirectedFrom, WKStringRef hostRedirectedTo)
{
    __block bool isDataReady = false;
    __block bool isRegisteredAsRedirectingTo = false;
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsIsRegisteredAsRedirectingTo:toNSString(hostRedirectedFrom) hostRedirectedTo:toNSString(hostRedirectedTo) completionHandler:^(BOOL _isRegisteredAsRedirectingTo) {
        isRegisteredAsRedirectingTo = _isRegisteredAsRedirectingTo;
        isDataReady = true;
    }];
    platformRunUntil(isDataReady, 0);
    
    return isRegisteredAsRedirectingTo;
}

void TestController::setStatisticsHasHadUserInteraction(WKStringRef hostName, bool value)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetHadUserInteraction:value forHost:toNSString(hostName)];
}

bool TestController::isStatisticsHasHadUserInteraction(WKStringRef hostName)
{
    __block bool isDataReady = false;
    __block bool hasHadUserInteraction = false;
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsHadUserInteraction:toNSString(hostName) completionHandler:^(BOOL _hasHadUserInteraction) {
        hasHadUserInteraction = _hasHadUserInteraction;
        isDataReady = true;
    }];
    platformRunUntil(isDataReady, 0);

    return hasHadUserInteraction;
}

void TestController::setStatisticsGrandfathered(WKStringRef hostName, bool value)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetIsGrandfathered:value forHost:toNSString(hostName)];
}

bool TestController::isStatisticsGrandfathered(WKStringRef hostName)
{
    __block bool isDataReady = false;
    __block bool isGrandfathered = false;
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsIsGrandfathered:toNSString(hostName) completionHandler:^(BOOL _isGrandfathered) {
        isGrandfathered = _isGrandfathered;
        isDataReady = true;
    }];
    platformRunUntil(isDataReady, 0);

    return isGrandfathered;
}

void TestController::setStatisticsSubframeUnderTopFrameOrigin(WKStringRef hostName, WKStringRef topFrameHostName)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetSubframeUnderTopFrameOrigin:toNSString(topFrameHostName) forHost:toNSString(hostName)];
}

void TestController::setStatisticsSubresourceUnderTopFrameOrigin(WKStringRef hostName, WKStringRef topFrameHostName)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetSubresourceUnderTopFrameOrigin:toNSString(topFrameHostName) forHost:toNSString(hostName)];
}

void TestController::setStatisticsSubresourceUniqueRedirectTo(WKStringRef hostName, WKStringRef hostNameRedirectedTo)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetSubresourceUniqueRedirectTo:toNSString(hostNameRedirectedTo) forHost:toNSString(hostName)];
}

void TestController::setStatisticsTimeToLiveUserInteraction(double seconds)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetTimeToLiveUserInteraction:seconds];
}

void TestController::setStatisticsTimeToLiveCookiePartitionFree(double seconds)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetTimeToLiveCookiePartitionFree:seconds];
}

void TestController::statisticsProcessStatisticsAndDataRecords()
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsProcessStatisticsAndDataRecords];
}

void TestController::statisticsUpdateCookiePartitioning()
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsUpdateCookiePartitioning];
}

void TestController::statisticsSetShouldPartitionCookiesForHost(WKStringRef hostName, bool value)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetShouldPartitionCookies:value forHost:toNSString(hostName)];
}

void TestController::statisticsSubmitTelemetry()
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSubmitTelemetry];
}

void TestController::setStatisticsNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetNotifyPagesWhenDataRecordsWereScanned:value];
}

void TestController::setStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetShouldClassifyResourcesBeforeDataRecordsRemoval:value];
}

void TestController::setStatisticsNotifyPagesWhenTelemetryWasCaptured(bool value)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetNotifyPagesWhenTelemetryWasCaptured:value];
}

void TestController::setStatisticsMinimumTimeBetweenDataRecordsRemoval(double seconds)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetMinimumTimeBetweenDataRecordsRemoval:seconds];
}

void TestController::setStatisticsGrandfatheringTime(double seconds)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetGrandfatheringTime:seconds];
}

void TestController::setStatisticsMaxStatisticsEntries(unsigned entries)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetMaxStatisticsEntries:entries];
}
    
void TestController::setStatisticsPruneEntriesDownTo(unsigned entries)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetPruneEntriesDownTo:entries];
}
    
void TestController::statisticsClearInMemoryAndPersistentStore()
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsClearInMemoryAndPersistentStore];
}

void TestController::statisticsClearInMemoryAndPersistentStoreModifiedSinceHours(unsigned hours)
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsClearInMemoryAndPersistentStoreModifiedSinceHours:hours];
}

void TestController::statisticsClearThroughWebsiteDataRemoval()
{
#if WK_API_ENABLED
    auto types = adoptNS([[NSSet alloc] initWithObjects:_WKWebsiteDataTypeResourceLoadStatistics, nil]);
    [globalWebViewConfiguration.websiteDataStore removeDataOfTypes:types.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        m_currentInvocation->didClearStatisticsThroughWebsiteDataRemoval();
    }];
#endif
}

void TestController::statisticsResetToConsistentState()
{
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsResetToConsistentState];
}
#endif // WK_API_ENABLED

} // namespace WTR
