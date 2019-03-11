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
#import "TestWebsiteDataStoreDelegate.h"
#import <Foundation/Foundation.h>
#import <Security/SecItem.h>
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
#import <WebKit/_WKApplicationManifest.h>
#import <WebKit/_WKUserContentExtensionStore.h>
#import <WebKit/_WKUserContentExtensionStorePrivate.h>
#import <wtf/MainThread.h>

namespace WTR {

static WKWebViewConfiguration *globalWebViewConfiguration;
static TestWebsiteDataStoreDelegate *globalWebsiteDataStoreDelegateClient;

void initializeWebViewConfiguration(const char* libraryPath, WKStringRef injectedBundlePath, WKContextRef context, WKContextConfigurationRef contextConfiguration)
{
    [globalWebViewConfiguration release];
    globalWebViewConfiguration = [[WKWebViewConfiguration alloc] init];

    globalWebViewConfiguration.processPool = (__bridge WKProcessPool *)context;
    globalWebViewConfiguration.websiteDataStore = (__bridge WKWebsiteDataStore *)WKContextGetWebsiteDataStore(context);
    globalWebViewConfiguration._allowUniversalAccessFromFileURLs = YES;
    globalWebViewConfiguration._applePayEnabled = YES;

    WKCookieManagerSetStorageAccessAPIEnabled(WKContextGetCookieManager(context), true);

    WKWebsiteDataStore* poolWebsiteDataStore = (__bridge WKWebsiteDataStore *)WKContextGetWebsiteDataStore((__bridge WKContextRef)globalWebViewConfiguration.processPool);
    if (libraryPath) {
        String cacheStorageDirectory = String(libraryPath) + '/' + "CacheStorage";
        [poolWebsiteDataStore _setCacheStorageDirectory: cacheStorageDirectory];

        String serviceWorkerRegistrationDirectory = String(libraryPath) + '/' + "ServiceWorkers";
        [poolWebsiteDataStore _setServiceWorkerRegistrationDirectory: serviceWorkerRegistrationDirectory];
    }

    [globalWebViewConfiguration.websiteDataStore _setResourceLoadStatisticsEnabled:YES];
    [globalWebViewConfiguration.websiteDataStore _resourceLoadStatisticsSetShouldSubmitTelemetry:NO];

    [globalWebsiteDataStoreDelegateClient release];
    globalWebsiteDataStoreDelegateClient = [[TestWebsiteDataStoreDelegate alloc] init];
    [globalWebViewConfiguration.websiteDataStore set_delegate:globalWebsiteDataStoreDelegateClient];

#if PLATFORM(IOS_FAMILY)
    globalWebViewConfiguration.allowsInlineMediaPlayback = YES;
    globalWebViewConfiguration._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
    globalWebViewConfiguration._invisibleAutoplayNotPermitted = NO;
    globalWebViewConfiguration._mediaDataLoadsAutomatically = YES;
    globalWebViewConfiguration.requiresUserActionForMediaPlayback = NO;
#endif
    globalWebViewConfiguration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;

#if USE(SYSTEM_PREVIEW)
    globalWebViewConfiguration._systemPreviewEnabled = YES;
#endif
}

void TestController::cocoaPlatformInitialize()
{
    const char* dumpRenderTreeTemp = libraryPathForTesting();
    if (!dumpRenderTreeTemp)
        return;

    String resourceLoadStatisticsFolder = String(dumpRenderTreeTemp) + '/' + "ResourceLoadStatistics";
    [[NSFileManager defaultManager] createDirectoryAtPath:resourceLoadStatisticsFolder withIntermediateDirectories:YES attributes:nil error: nil];
    String fullBrowsingSessionResourceLog = resourceLoadStatisticsFolder + '/' + "full_browsing_session_resourceLog.plist";
    NSDictionary *resourceLogPlist = [[NSDictionary alloc] initWithObjectsAndKeys: [NSNumber numberWithInt:1], @"version", nil];
    if (![resourceLogPlist writeToFile:fullBrowsingSessionResourceLog atomically:YES])
        WTFCrash();
    [resourceLogPlist release];
}

WKContextRef TestController::platformContext()
{
    return (__bridge WKContextRef)globalWebViewConfiguration.processPool;
}

WKPreferencesRef TestController::platformPreferences()
{
    return (__bridge WKPreferencesRef)globalWebViewConfiguration.preferences;
}

void TestController::platformAddTestOptions(TestOptions& options) const
{
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"EnableProcessSwapOnNavigation"])
        options.contextOptions.enableProcessSwapOnNavigation = true;
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"EnableProcessSwapOnWindowOpen"])
        options.contextOptions.enableProcessSwapOnWindowOpen = true;
}

void TestController::platformCreateWebView(WKPageConfigurationRef, const TestOptions& options)
{
    RetainPtr<WKWebViewConfiguration> copiedConfiguration = adoptNS([globalWebViewConfiguration copy]);

#if PLATFORM(IOS_FAMILY)
    if (options.useDataDetection)
        [copiedConfiguration setDataDetectorTypes:WKDataDetectorTypeAll];
    if (options.ignoresViewportScaleLimits)
        [copiedConfiguration setIgnoresViewportScaleLimits:YES];
    if (options.useCharacterSelectionGranularity)
        [copiedConfiguration setSelectionGranularity:WKSelectionGranularityCharacter];
    if (options.useCharacterSelectionGranularity)
        [copiedConfiguration setSelectionGranularity:WKSelectionGranularityCharacter];
#endif

    if (options.enableAttachmentElement)
        [copiedConfiguration _setAttachmentElementEnabled:YES];

    if (options.enableColorFilter)
        [copiedConfiguration _setColorFilterEnabled:YES];

    if (options.enableEditableImages)
        [copiedConfiguration _setEditableImagesEnabled:YES];

    if (options.enableUndoManagerAPI)
        [copiedConfiguration _setUndoManagerAPIEnabled:YES];

    if (options.applicationManifest.length()) {
        auto manifestPath = [NSString stringWithUTF8String:options.applicationManifest.c_str()];
        NSString *text = [NSString stringWithContentsOfFile:manifestPath usedEncoding:nullptr error:nullptr];
        [copiedConfiguration _setApplicationManifest:[_WKApplicationManifest applicationManifestFromJSON:text manifestURL:nil documentURL:nil]];
    }

    m_mainWebView = std::make_unique<PlatformWebView>(copiedConfiguration.get(), options);

    if (options.punchOutWhiteBackgroundsInDarkMode)
        m_mainWebView->setDrawsBackground(false);

    if (options.editable)
        m_mainWebView->setEditable(true);
}

PlatformWebView* TestController::platformCreateOtherPage(PlatformWebView* parentView, WKPageConfigurationRef, const TestOptions& options)
{
    WKWebViewConfiguration *newConfiguration = [[globalWebViewConfiguration copy] autorelease];
    newConfiguration._relatedWebView = static_cast<WKWebView*>(parentView->platformView());
    return new PlatformWebView(newConfiguration, options);
}

WKContextRef TestController::platformAdjustContext(WKContextRef context, WKContextConfigurationRef contextConfiguration)
{
    initializeWebViewConfiguration(libraryPathForTesting(), injectedBundlePath(), context, contextConfiguration);
    return (__bridge WKContextRef)globalWebViewConfiguration.processPool;
}

void TestController::platformRunUntil(bool& done, WTF::Seconds timeout)
{
    NSDate *endDate = (timeout > 0_s) ? [NSDate dateWithTimeIntervalSinceNow:timeout.seconds()] : [NSDate distantFuture];

    while (!done && [endDate compare:[NSDate date]] == NSOrderedDescending)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:endDate];
}

static NSCalendar *swizzledCalendar()
{
    return [NSCalendar calendarWithIdentifier:TestController::singleton().getOverriddenCalendarIdentifier().get()];
}
    
RetainPtr<NSString> TestController::getOverriddenCalendarIdentifier() const
{
    return m_overriddenCalendarIdentifier;
}

void TestController::setDefaultCalendarType(NSString *identifier)
{
    m_overriddenCalendarIdentifier = identifier;
    if (!m_calendarSwizzler)
        m_calendarSwizzler = std::make_unique<ClassMethodSwizzler>([NSCalendar class], @selector(currentCalendar), reinterpret_cast<IMP>(swizzledCalendar));
}

void TestController::resetContentExtensions()
{
    __block bool doneRemoving = false;
    [[_WKUserContentExtensionStore defaultStore] removeContentExtensionForIdentifier:@"TestContentExtensions" completionHandler:^(NSError *error) {
        doneRemoving = true;
    }];
    platformRunUntil(doneRemoving, noTimeout);
    [[_WKUserContentExtensionStore defaultStore] _removeAllContentExtensions];

    if (auto* webView = mainWebView()) {
        TestRunnerWKWebView *platformView = webView->platformView();
        [platformView.configuration.userContentController _removeAllUserContentFilters];
    }
}

void TestController::cocoaResetStateToConsistentValues(const TestOptions& options)
{
    m_calendarSwizzler = nullptr;
    m_overriddenCalendarIdentifier = nil;
    
    if (auto* webView = mainWebView()) {
        TestRunnerWKWebView *platformView = webView->platformView();
        platformView._viewScale = 1;
        platformView._minimumEffectiveDeviceWidth = 0;

        // Toggle on before the test, and toggle off after the test.
        if (options.shouldShowSpellCheckingDots)
            [platformView toggleContinuousSpellChecking:nil];
    }

    [globalWebsiteDataStoreDelegateClient setAllowRaisingQuota: true];
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
#elif PLATFORM(IOS_FAMILY)
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
    auto types = adoptNS([[NSSet alloc] initWithObjects:_WKWebsiteDataTypeCredentials, nil]);
    [globalWebViewConfiguration.websiteDataStore removeDataOfTypes:types.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        m_currentInvocation->didRemoveAllSessionCredentials();
    }];
}

void TestController::getAllStorageAccessEntries()
{
    auto* parentView = mainWebView();
    if (!parentView)
        return;

    [globalWebViewConfiguration.websiteDataStore _getAllStorageAccessEntriesFor:parentView->platformView() completionHandler:^(NSArray<NSString *> *nsDomains) {
        Vector<String> domains;
        domains.reserveInitialCapacity(nsDomains.count);
        for (NSString *domain : nsDomains)
            domains.uncheckedAppend(domain);
        m_currentInvocation->didReceiveAllStorageAccessEntries(domains);
    }];
}

void TestController::injectUserScript(WKStringRef script)
{
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource: toWTFString(script) injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);

    [[globalWebViewConfiguration userContentController] addUserScript: userScript.get()];
}

void TestController::addTestKeyToKeychain(const String& privateKeyBase64, const String& attrLabel, const String& applicationTagBase64)
{
    // FIXME(182772)
#if PLATFORM(IOS_FAMILY)
    NSDictionary* options = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrKeySizeInBits: @256
    };
    CFErrorRef errorRef = nullptr;
    auto key = adoptCF(SecKeyCreateWithData(
        (__bridge CFDataRef)adoptNS([[NSData alloc] initWithBase64EncodedString:privateKeyBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get(),
        (__bridge CFDictionaryRef)options,
        &errorRef
    ));
    ASSERT(!errorRef);

    NSDictionary* addQuery = @{
        (id)kSecValueRef: (id)key.get(),
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: attrLabel,
        (id)kSecAttrApplicationTag: adoptNS([[NSData alloc] initWithBase64EncodedString:applicationTagBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get()
    };
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, NULL);
    ASSERT_UNUSED(status, !status);
#endif
}

void TestController::cleanUpKeychain(const String& attrLabel)
{
    // FIXME(182772)
#if PLATFORM(IOS_FAMILY)
    NSDictionary* deleteQuery = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: attrLabel
    };
    SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
#endif
}

bool TestController::keyExistsInKeychain(const String& attrLabel, const String& applicationTagBase64)
{
    // FIXME(182772)
#if PLATFORM(IOS_FAMILY)
    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrLabel: attrLabel,
        (id)kSecAttrApplicationTag: adoptNS([[NSData alloc] initWithBase64EncodedString:applicationTagBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get(),
    };
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL);
    if (!status)
        return true;
    ASSERT(status == errSecItemNotFound);
#endif
    return false;
}

void TestController::setAllowStorageQuotaIncrease(bool value)
{
    [globalWebsiteDataStoreDelegateClient setAllowRaisingQuota: value];
}

bool TestController::canDoServerTrustEvaluationInNetworkProcess() const
{
#if HAVE(CFNETWORK_NSURLSESSION_STRICTRUSTEVALUATE)
    return true;
#else
    return false;
#endif
}

bool TestController::isDoingMediaCapture() const
{
    return m_mainWebView->platformView()._mediaCaptureState != _WKMediaCaptureStateNone;
}

} // namespace WTR
