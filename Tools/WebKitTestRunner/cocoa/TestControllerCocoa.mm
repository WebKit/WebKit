/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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
#import "Options.h"
#import "PlatformWebView.h"
#import "StringFunctions.h"
#import "TestInvocation.h"
#import "TestRunnerWKWebView.h"
#import "TestWebsiteDataStoreDelegate.h"
#import "WebCoreTestSupport.h"
#import <Foundation/Foundation.h>
#import <Security/SecItem.h>
#import <WebKit/WKContentRuleListStorePrivate.h>
#import <WebKit/WKContextConfigurationRef.h>
#import <WebKit/WKContextPrivate.h>
#import <WebKit/WKImageCG.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKStringCF.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <WebKit/_WKApplicationManifest.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/MainThread.h>
#import <wtf/UniqueRef.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

namespace WTR {

static RetainPtr<WKWebViewConfiguration>& globalWebViewConfiguration()
{
    static NeverDestroyed<RetainPtr<WKWebViewConfiguration>> globalWebViewConfiguration;
    return globalWebViewConfiguration;
}

static RetainPtr<TestWebsiteDataStoreDelegate>& globalWebsiteDataStoreDelegateClient()
{
    static NeverDestroyed<RetainPtr<TestWebsiteDataStoreDelegate>> globalWebsiteDataStoreDelegateClient;
    return globalWebsiteDataStoreDelegateClient;
}

void initializeWebViewConfiguration(const char* libraryPath, WKStringRef injectedBundlePath, WKContextRef context, WKContextConfigurationRef contextConfiguration)
{
    globalWebViewConfiguration() = [&] {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        [configuration setProcessPool:(__bridge WKProcessPool *)context];
        [configuration setWebsiteDataStore:(__bridge WKWebsiteDataStore *)TestController::defaultWebsiteDataStore()];
        [configuration _setAllowUniversalAccessFromFileURLs:YES];
        [configuration _setAllowTopNavigationToDataURLs:YES];
        [configuration _setApplePayEnabled:YES];

        globalWebsiteDataStoreDelegateClient() = adoptNS([[TestWebsiteDataStoreDelegate alloc] init]);
        [[configuration websiteDataStore] set_delegate:globalWebsiteDataStoreDelegateClient().get()];

#if PLATFORM(IOS_FAMILY)
        [configuration setAllowsInlineMediaPlayback:YES];
        [configuration _setInlineMediaPlaybackRequiresPlaysInlineAttribute:NO];
        [configuration _setInvisibleAutoplayNotPermitted:NO];
        [configuration _setMediaDataLoadsAutomatically:YES];
        [configuration setRequiresUserActionForMediaPlayback:NO];
#endif
        [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];

#if USE(SYSTEM_PREVIEW)
        [configuration _setSystemPreviewEnabled:YES];
#endif
        return configuration;
    }();
}

void TestController::cocoaPlatformInitialize(const Options& options)
{
    const char* dumpRenderTreeTemp = libraryPathForTesting();
    if (!dumpRenderTreeTemp)
        return;

    String resourceLoadStatisticsFolder = makeString(dumpRenderTreeTemp, "/ResourceLoadStatistics");
    [[NSFileManager defaultManager] createDirectoryAtPath:resourceLoadStatisticsFolder withIntermediateDirectories:YES attributes:nil error: nil];
    String fullBrowsingSessionResourceLog = makeString(resourceLoadStatisticsFolder, "/full_browsing_session_resourceLog.plist");
    NSDictionary *resourceLogPlist = @{ @"version": @(1) };
    if (![resourceLogPlist writeToFile:fullBrowsingSessionResourceLog atomically:YES])
        WTFCrash();
    
    if (options.webCoreLogChannels.length())
        [[NSUserDefaults standardUserDefaults] setValue:[NSString stringWithUTF8String:options.webCoreLogChannels.c_str()] forKey:@"WebCoreLogging"];

    if (options.webKitLogChannels.length())
        [[NSUserDefaults standardUserDefaults] setValue:[NSString stringWithUTF8String:options.webKitLogChannels.c_str()] forKey:@"WebKit2Logging"];
}

WKContextRef TestController::platformContext()
{
    return (__bridge WKContextRef)[globalWebViewConfiguration() processPool];
}

WKPreferencesRef TestController::platformPreferences()
{
    return (__bridge WKPreferencesRef)[globalWebViewConfiguration() preferences];
}

TestFeatures TestController::platformSpecificFeatureOverridesDefaultsForTest(const TestCommand&) const
{
    TestFeatures features;

    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"EnableProcessSwapOnNavigation"])
        features.boolTestRunnerFeatures.insert({ "enableProcessSwapOnNavigation", true });
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"EnableProcessSwapOnWindowOpen"])
        features.boolTestRunnerFeatures.insert({ "enableProcessSwapOnWindowOpen", true });

    return features;
}

void TestController::platformInitializeDataStore(WKPageConfigurationRef, const TestOptions& options)
{
    bool useEphemeralSession = options.useEphemeralSession();
    auto standaloneWebApplicationURL = options.standaloneWebApplicationURL();
    if (useEphemeralSession || standaloneWebApplicationURL.length() || options.enableInAppBrowserPrivacy()) {
        auto websiteDataStoreConfig = useEphemeralSession ? adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]) : adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
        if (!useEphemeralSession)
            configureWebsiteDataStoreTemporaryDirectories((WKWebsiteDataStoreConfigurationRef)websiteDataStoreConfig.get());
        if (standaloneWebApplicationURL.length())
            [websiteDataStoreConfig setStandaloneApplicationURL:[NSURL URLWithString:[NSString stringWithUTF8String:standaloneWebApplicationURL.c_str()]]];
#if PLATFORM(IOS_FAMILY)
        if (options.enableInAppBrowserPrivacy())
            [websiteDataStoreConfig setEnableInAppBrowserPrivacyForTesting:YES];
#endif
        m_websiteDataStore = (__bridge WKWebsiteDataStoreRef)adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfig.get()]).get();
    } else
        m_websiteDataStore = (__bridge WKWebsiteDataStoreRef)[globalWebViewConfiguration() websiteDataStore];
}

void TestController::platformCreateWebView(WKPageConfigurationRef, const TestOptions& options)
{
    auto copiedConfiguration = adoptNS([globalWebViewConfiguration() copy]);

#if PLATFORM(IOS_FAMILY)
    if (options.useDataDetection())
        [copiedConfiguration setDataDetectorTypes:WKDataDetectorTypeAll];
    if (options.ignoresViewportScaleLimits())
        [copiedConfiguration setIgnoresViewportScaleLimits:YES];
    if (options.useCharacterSelectionGranularity())
        [copiedConfiguration setSelectionGranularity:WKSelectionGranularityCharacter];
    if (options.isAppBoundWebView())
        [copiedConfiguration setLimitsNavigationsToAppBoundDomains:YES];

    [copiedConfiguration _setAppInitiatedOverrideValueForTesting:options.isAppInitiated() ? _WKAttributionOverrideTestingAppInitiated : _WKAttributionOverrideTestingUserInitiated];
#endif

    if (options.enableAttachmentElement())
        [copiedConfiguration _setAttachmentElementEnabled:YES];

    [copiedConfiguration setWebsiteDataStore:(WKWebsiteDataStore *)websiteDataStore()];
    [copiedConfiguration _setAllowTopNavigationToDataURLs:options.allowTopNavigationToDataURLs()];
    [copiedConfiguration _setAppHighlightsEnabled:options.appHighlightsEnabled()];

    if (!options.contentSecurityPolicyExtensionMode().empty()) {
        if (options.contentSecurityPolicyExtensionMode() == "v2")
            [copiedConfiguration _setContentSecurityPolicyModeForExtension:_WKContentSecurityPolicyModeForExtensionManifestV2];
        if (options.contentSecurityPolicyExtensionMode() == "v3")
            [copiedConfiguration _setContentSecurityPolicyModeForExtension:_WKContentSecurityPolicyModeForExtensionManifestV3];
    }

    configureWebpagePreferences(copiedConfiguration.get(), options);

    auto applicationManifest = options.applicationManifest();
    if (applicationManifest.length()) {
        auto manifestPath = [NSString stringWithUTF8String:applicationManifest.c_str()];
        NSString *text = [NSString stringWithContentsOfFile:manifestPath usedEncoding:nullptr error:nullptr];
        [copiedConfiguration _setApplicationManifest:[_WKApplicationManifest applicationManifestFromJSON:text manifestURL:nil documentURL:nil]];
    }

    m_mainWebView = makeUnique<PlatformWebView>(copiedConfiguration.get(), options);
    finishCreatingPlatformWebView(m_mainWebView.get(), options);

    if (options.punchOutWhiteBackgroundsInDarkMode())
        m_mainWebView->setDrawsBackground(false);

    if (options.editable())
        m_mainWebView->setEditable(true);

    m_mainWebView->platformView().allowsLinkPreview = options.allowsLinkPreview();
    [m_mainWebView->platformView() _setShareSheetCompletesImmediatelyWithResolutionForTesting:YES];
}

UniqueRef<PlatformWebView> TestController::platformCreateOtherPage(PlatformWebView* parentView, WKPageConfigurationRef, const TestOptions& options)
{
    auto newConfiguration = adoptNS([globalWebViewConfiguration() copy]);
    if (parentView)
        [newConfiguration _setRelatedWebView:static_cast<WKWebView*>(parentView->platformView())];
    if ([newConfiguration _relatedWebView])
        [newConfiguration setWebsiteDataStore:[newConfiguration _relatedWebView].configuration.websiteDataStore];
    auto view = makeUniqueRef<PlatformWebView>(newConfiguration.get(), options);
    finishCreatingPlatformWebView(view.ptr(), options);
    return view;
}

// Code that needs to run after TestController::m_mainWebView is initialized goes into this function.
void TestController::finishCreatingPlatformWebView(PlatformWebView* view, const TestOptions& options)
{
#if PLATFORM(MAC)
    if (options.shouldShowWindow())
        [view->platformWindow() orderFront:nil];
    else
        [view->platformWindow() orderBack:nil];
#endif
}

WKContextRef TestController::platformAdjustContext(WKContextRef context, WKContextConfigurationRef contextConfiguration)
{
    initializeWebViewConfiguration(libraryPathForTesting(), injectedBundlePath(), context, contextConfiguration);
    return (__bridge WKContextRef)[globalWebViewConfiguration() processPool];
}

void TestController::platformRunUntil(bool& done, WTF::Seconds timeout)
{
    NSDate *endDate = (timeout > 0_s) ? [NSDate dateWithTimeIntervalSinceNow:timeout.seconds()] : [NSDate distantFuture];

    while (!done && [endDate compare:[NSDate date]] == NSOrderedDescending)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:endDate];
}

static NSCalendar *swizzledCalendar()
{
    NSCalendar *calendar = [NSCalendar calendarWithIdentifier:TestController::singleton().overriddenCalendarIdentifier()];
    calendar.locale = [NSLocale localeWithLocaleIdentifier:TestController::singleton().overriddenCalendarLocaleIdentifier()];
    return calendar;
}

NSString *TestController::overriddenCalendarIdentifier() const
{
    return m_overriddenCalendarAndLocaleIdentifiers.first.get();
}

NSString *TestController::overriddenCalendarLocaleIdentifier() const
{
    return m_overriddenCalendarAndLocaleIdentifiers.second.get();
}

void TestController::setDefaultCalendarType(NSString *identifier, NSString *localeIdentifier)
{
    m_overriddenCalendarAndLocaleIdentifiers = { identifier, localeIdentifier };
    if (!m_calendarSwizzler)
        m_calendarSwizzler = makeUnique<ClassMethodSwizzler>([NSCalendar class], @selector(currentCalendar), reinterpret_cast<IMP>(swizzledCalendar));
}

#if ENABLE(CONTENT_EXTENSIONS)
void TestController::resetContentExtensions()
{
    __block bool doneRemoving = false;
    [[WKContentRuleListStore defaultStore] removeContentRuleListForIdentifier:@"TestContentExtensions" completionHandler:^(NSError *error) {
        doneRemoving = true;
    }];
    platformRunUntil(doneRemoving, noTimeout);
    [[WKContentRuleListStore defaultStore] _removeAllContentRuleLists];

    if (auto* webView = mainWebView()) {
        TestRunnerWKWebView *platformView = webView->platformView();
        [platformView.configuration.userContentController _removeAllUserContentFilters];
    }
}
#endif

void TestController::setApplicationBundleIdentifier(const std::string& bundleIdentifier)
{
    if (bundleIdentifier.empty())
        return;
    
    [TestRunnerWKWebView _setApplicationBundleIdentifier:(NSString *)toWTFString(bundleIdentifier).createCFString().get()];
}

void TestController::clearApplicationBundleIdentifierTestingOverride()
{
    [TestRunnerWKWebView _clearApplicationBundleIdentifierTestingOverride];
    m_hasSetApplicationBundleIdentifier = false;
}

void TestController::cocoaResetStateToConsistentValues(const TestOptions& options)
{
    m_calendarSwizzler = nullptr;
    m_overriddenCalendarAndLocaleIdentifiers = { nil, nil };
    
    if (auto* webView = mainWebView()) {
        TestRunnerWKWebView *platformView = webView->platformView();
        platformView._viewScale = 1;
        platformView._minimumEffectiveDeviceWidth = 0;
        platformView._editable = NO;
        [platformView _setContinuousSpellCheckingEnabledForTesting:options.shouldShowSpellCheckingDots()];
        [platformView resetInteractionCallbacks];
        [platformView _resetNavigationGestureStateForTesting];
        [platformView.configuration.preferences setTextInteractionEnabled:options.textInteractionEnabled()];
    }

    [globalWebsiteDataStoreDelegateClient() setAllowRaisingQuota:YES];

    WebCoreTestSupport::setAdditionalSupportedImageTypesForTesting(String::fromLatin1(options.additionalSupportedImageTypes().c_str()));
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
    [[globalWebViewConfiguration() websiteDataStore] removeDataOfTypes:types.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        m_currentInvocation->didRemoveAllSessionCredentials();
    }];
}

void TestController::getAllStorageAccessEntries()
{
    auto* parentView = mainWebView();
    if (!parentView)
        return;

    [[globalWebViewConfiguration() websiteDataStore] _getAllStorageAccessEntriesFor:parentView->platformView() completionHandler:^(NSArray<NSString *> *domains) {
        m_currentInvocation->didReceiveAllStorageAccessEntries(makeVector<String>(domains));
    }];
}

void TestController::loadedSubresourceDomains()
{
    auto* parentView = mainWebView();
    if (!parentView)
        return;
    
    [[globalWebViewConfiguration() websiteDataStore] _loadedSubresourceDomainsFor:parentView->platformView() completionHandler:^(NSArray<NSString *> *domains) {
        m_currentInvocation->didReceiveLoadedSubresourceDomains(makeVector<String>(domains));
    }];
}

void TestController::clearLoadedSubresourceDomains()
{
    auto* parentView = mainWebView();
    if (!parentView)
        return;

    [[globalWebViewConfiguration() websiteDataStore] _clearLoadedSubresourceDomainsFor:parentView->platformView()];
}

bool TestController::didLoadAppInitiatedRequest()
{
    auto* parentView = mainWebView();
    if (!parentView)
        return false;

    __block bool isDone = false;
    __block bool didLoadResult = false;
    [m_mainWebView->platformView() _didLoadAppInitiatedRequest:^(BOOL result) {
        didLoadResult = result;
        isDone = true;
    }];
    platformRunUntil(isDone, noTimeout);
    return didLoadResult;
}

bool TestController::didLoadNonAppInitiatedRequest()
{
    auto* parentView = mainWebView();
    if (!parentView)
        return false;

    __block bool isDone = false;
    __block bool didLoadResult = false;
    [m_mainWebView->platformView() _didLoadNonAppInitiatedRequest:^(BOOL result) {
        didLoadResult = result;
        isDone = true;
    }];
    platformRunUntil(isDone, noTimeout);
    return didLoadResult;
}

void TestController::clearAppPrivacyReportTestingData()
{
    auto* parentView = mainWebView();
    if (!parentView)
        return;

    __block bool doneClearing = false;
    [m_mainWebView->platformView() _clearAppPrivacyReportTestingData:^{
        doneClearing = true;
    }];
    platformRunUntil(doneClearing, noTimeout);
}

void TestController::injectUserScript(WKStringRef script)
{
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource: toWTFString(script) injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);

    [[globalWebViewConfiguration() userContentController] addUserScript: userScript.get()];
}

void TestController::addTestKeyToKeychain(const String& privateKeyBase64, const String& attrLabel, const String& applicationTagBase64)
{
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
        (id)kSecAttrApplicationTag: adoptNS([[NSData alloc] initWithBase64EncodedString:applicationTagBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get(),
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecUseDataProtectionKeychain: @YES
    };
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, NULL);
    ASSERT_UNUSED(status, !status);
}

void TestController::cleanUpKeychain(const String& attrLabel, const String& applicationLabelBase64)
{
    auto deleteQuery = adoptNS([[NSMutableDictionary alloc] init]);
    [deleteQuery setObject:(id)kSecClassKey forKey:(id)kSecClass];
    [deleteQuery setObject:attrLabel forKey:(id)kSecAttrLabel];
    [deleteQuery setObject:@YES forKey:(id)kSecUseDataProtectionKeychain];
    if (!!applicationLabelBase64)
        [deleteQuery setObject:adoptNS([[NSData alloc] initWithBase64EncodedString:applicationLabelBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get() forKey:(id)kSecAttrApplicationLabel];

    SecItemDelete((__bridge CFDictionaryRef)deleteQuery.get());
}

bool TestController::keyExistsInKeychain(const String& attrLabel, const String& applicationLabelBase64)
{
    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrLabel: attrLabel,
        (id)kSecAttrApplicationLabel: adoptNS([[NSData alloc] initWithBase64EncodedString:applicationLabelBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get(),
        (id)kSecUseDataProtectionKeychain: @YES
    };
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL);
    if (!status)
        return true;
    ASSERT(status == errSecItemNotFound);
    return false;
}

void TestController::setAllowStorageQuotaIncrease(bool value)
{
    [globalWebsiteDataStoreDelegateClient() setAllowRaisingQuota: value];
}

void TestController::setAllowsAnySSLCertificate(bool allows)
{
    m_allowsAnySSLCertificate = allows;
    WKWebsiteDataStoreSetAllowsAnySSLCertificateForWebSocketTesting(websiteDataStore(), allows);
    [globalWebsiteDataStoreDelegateClient() setAllowAnySSLCertificate: allows];
}

void TestController::setAllowedMenuActions(const Vector<String>& actions)
{
#if PLATFORM(IOS_FAMILY)
    [m_mainWebView->platformView() setAllowedMenuActions:createNSArray(actions).get()];
#else
    UNUSED_PARAM(actions);
#endif
}

bool TestController::isDoingMediaCapture() const
{
    return m_mainWebView->platformView()._mediaCaptureState != _WKMediaCaptureStateDeprecatedNone;
}

#if PLATFORM(IOS_FAMILY)

static WKContentMode contentMode(const TestOptions& options)
{
    auto mode = options.contentMode();
    if (mode == "desktop")
        return WKContentModeDesktop;
    if (mode == "mobile")
        return WKContentModeMobile;
    return WKContentModeRecommended;
}

#endif // PLATFORM(IOS_FAMILY)

void TestController::configureWebpagePreferences(WKWebViewConfiguration *configuration, const TestOptions& options)
{
    auto webpagePreferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [webpagePreferences _setNetworkConnectionIntegrityEnabled:options.networkConnectionIntegrityEnabled()];
#if PLATFORM(IOS_FAMILY)
    [webpagePreferences setPreferredContentMode:contentMode(options)];
#endif
    configuration.defaultWebpagePreferences = webpagePreferences.get();
}

WKRetainPtr<WKStringRef> TestController::takeViewPortSnapshot()
{
    return adoptWK(WKImageCreateDataURLFromImage(mainWebView()->windowSnapshotImage().get()));
}

} // namespace WTR
