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
#import "LayoutTestSpellChecker.h"
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
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKStringCF.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKUserMediaPermissionCheck.h>
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
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/MainThread.h>
#import <wtf/RunLoop.h>
#import <wtf/UniqueRef.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/MakeString.h>

#if ENABLE(IMAGE_ANALYSIS)

#import <pal/cocoa/VisionKitCoreSoftLink.h>

#if HAVE(VK_IMAGE_ANALYSIS_FOR_MACHINE_READABLE_CODES)

static const UIMenuIdentifier fakeMachineReadableCodeActionIdentifier = @"org.webkit.FakeMachineReadableCodeMenuAction";
static UIMenu *fakeMachineReadableCodeMenuForTesting()
{
    static NeverDestroyed<RetainPtr<UIMenu>> menu;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, [&] {
        RetainPtr action = [UIAction actionWithTitle:@"QR Code Action" image:nil identifier:fakeMachineReadableCodeActionIdentifier handler:^(UIAction *) {
        }];
        menu.get() = [UIMenu menuWithTitle:@"QR Code" image:nil identifier:nil options:UIMenuOptionsDisplayInline children:@[ action.get() ]];
    });
    return menu->get();
}

@interface FakeMachineReadableCodeImageAnalysis : NSObject
@property (nonatomic, readonly) UIMenu *mrcMenu;
@property (nonatomic, weak) UIViewController *presentingViewControllerForMrcAction;
@property (nonatomic) CGRect rectForMrcActionInPresentingViewController;
@end

@implementation FakeMachineReadableCodeImageAnalysis

- (NSArray<VKWKLineInfo *> *)allLines
{
    return @[ ];
}

- (BOOL)hasResultsForAnalysisTypes:(VKAnalysisTypes)analysisTypes
{
    return analysisTypes == VKAnalysisTypeMachineReadableCode;
}

- (UIMenu *)mrcMenu
{
    return fakeMachineReadableCodeMenuForTesting();
}

@end

#endif // HAVE(VK_IMAGE_ANALYSIS_FOR_MACHINE_READABLE_CODES)

static VKImageAnalysisRequestID gCurrentImageAnalysisRequestID = 0;

VKImageAnalysisRequestID swizzledProcessImageAnalysisRequest(id, SEL, VKCImageAnalyzerRequest *, void (^progressHandler)(double), void (^completionHandler)(VKCImageAnalysis *, NSError *))
{
    RunLoop::main().dispatchAfter(25_ms, [completionHandler = makeBlockPtr(completionHandler)] {
#if HAVE(VK_IMAGE_ANALYSIS_FOR_MACHINE_READABLE_CODES)
        if (WTR::TestController::singleton().shouldUseFakeMachineReadableCodeResultsForImageAnalysis()) {
            auto result = adoptNS([FakeMachineReadableCodeImageAnalysis new]);
            completionHandler(static_cast<VKCImageAnalysis *>(result.get()), nil);
            return;
        }
#endif
        completionHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:404 userInfo:nil]);
    });
    return ++gCurrentImageAnalysisRequestID;
}

#endif // ENABLE(IMAGE_ANALYSIS)

#if ENABLE(DATA_DETECTION)

NSURL *swizzledAppStoreURL(NSURL *url, SEL)
{
    auto components = adoptNS([[NSURLComponents alloc] initWithURL:url resolvingAgainstBaseURL:NO]);
    if (![[components scheme] isEqualToString:@"http"] && ![[components scheme] isEqualToString:@"https"])
        return nil;

    if (![[components host] isEqualToString:@"itunes.apple.com"])
        return nil;

    [components setScheme:@"itms-appss"];
    return [components URL];
}

#endif // ENABLE(DATA_DETECTION)

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

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
        [configuration _setOverlayRegionsEnabled:YES];
#endif

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
        [configuration _setCSSTransformStyleSeparatedEnabled:YES];
#endif

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

    String resourceLoadStatisticsFolder = makeString(String::fromUTF8(dumpRenderTreeTemp), "/ResourceLoadStatistics"_s);
    [[NSFileManager defaultManager] createDirectoryAtPath:resourceLoadStatisticsFolder withIntermediateDirectories:YES attributes:nil error: nil];
    String fullBrowsingSessionResourceLog = makeString(resourceLoadStatisticsFolder, "/full_browsing_session_resourceLog.plist"_s);
    NSDictionary *resourceLogPlist = @{ @"version": @(1) };
    if (![resourceLogPlist writeToFile:fullBrowsingSessionResourceLog atomically:YES])
        WTFCrash();
    
    if (options.webCoreLogChannels.length())
        [[NSUserDefaults standardUserDefaults] setValue:[NSString stringWithUTF8String:options.webCoreLogChannels.c_str()] forKey:@"WebCoreLogging"];

    if (options.webKitLogChannels.length())
        [[NSUserDefaults standardUserDefaults] setValue:[NSString stringWithUTF8String:options.webKitLogChannels.c_str()] forKey:@"WebKit2Logging"];

    if (options.lockdownModeEnabled)
        [WKProcessPool _setCaptivePortalModeEnabledGloballyForTesting:YES];

#if ENABLE(IMAGE_ANALYSIS)
    m_imageAnalysisRequestSwizzler = makeUnique<InstanceMethodSwizzler>(
        PAL::getVKCImageAnalyzerClass(),
        @selector(processRequest:progressHandler:completionHandler:),
        reinterpret_cast<IMP>(swizzledProcessImageAnalysisRequest)
    );
#endif

#if ENABLE(DATA_DETECTION)
    m_appStoreURLSwizzler = makeUnique<InstanceMethodSwizzler>(NSURL.class, @selector(iTunesStoreURL), reinterpret_cast<IMP>(swizzledAppStoreURL));
#endif
}

#if ENABLE(IMAGE_ANALYSIS)

uint64_t TestController::currentImageAnalysisRequestID()
{
    return static_cast<uint64_t>(gCurrentImageAnalysisRequestID);
}

void TestController::installFakeMachineReadableCodeResultsForImageAnalysis()
{
    m_useFakeMachineReadableCodeResultsForImageAnalysis = true;
}

bool TestController::shouldUseFakeMachineReadableCodeResultsForImageAnalysis() const
{
    return m_useFakeMachineReadableCodeResultsForImageAnalysis;
}

#endif // ENABLE(IMAGE_ANALYSIS)

WKContextRef TestController::platformContext()
{
    return (__bridge WKContextRef)[globalWebViewConfiguration() processPool];
}

TestFeatures TestController::platformSpecificFeatureOverridesDefaultsForTest(const TestCommand&) const
{
    TestFeatures features;

    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"EnableProcessSwapOnNavigation"])
        features.boolTestRunnerFeatures.insert({ "enableProcessSwapOnNavigation", true });

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
        auto store = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfig.get()]);
        m_websiteDataStore = (__bridge WKWebsiteDataStoreRef)store.get();
        [store set_delegate:globalWebsiteDataStoreDelegateClient().get()];
    } else
        m_websiteDataStore = (__bridge WKWebsiteDataStoreRef)[globalWebViewConfiguration() websiteDataStore];
}

static bool currentGPUProcessConfigurationCompatibleWithOptions(const TestOptions& options)
{
    if ([WKProcessPool _isMetalDebugDeviceEnabledInGPUProcessForTesting] != options.enableMetalDebugDevice())
        return false;

    if ([WKProcessPool _isMetalShaderValidationEnabledInGPUProcessForTesting] != options.enableMetalShaderValidation())
        return false;

    return true;
}

void TestController::platformEnsureGPUProcessConfiguredForOptions(const TestOptions& options)
{
    [WKProcessPool _setEnableMetalDebugDeviceInNewGPUProcessesForTesting:options.enableMetalDebugDevice()];
    [WKProcessPool _setEnableMetalShaderValidationInNewGPUProcessesForTesting:options.enableMetalShaderValidation()];

    if (!currentGPUProcessConfigurationCompatibleWithOptions(options))
        terminateGPUProcess();
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
    [copiedConfiguration _setLongPressActionsEnabled:options.longPressActionsEnabled()];
#endif

    if (options.enableAttachmentElement())
        [copiedConfiguration _setAttachmentElementEnabled:YES];
    if (options.enableAttachmentWideLayout())
        [copiedConfiguration _setAttachmentWideLayoutEnabled:YES];

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
    
    [copiedConfiguration _setPortsForUpgradingInsecureSchemeForTesting:@[@(options.insecureUpgradePort()), @(options.secureUpgradePort())]];

    m_mainWebView = makeUnique<PlatformWebView>((__bridge WKPageConfigurationRef)copiedConfiguration.get(), options);
    finishCreatingPlatformWebView(m_mainWebView.get(), options);

    if (options.punchOutWhiteBackgroundsInDarkMode())
        m_mainWebView->setDrawsBackground(false);

    if (options.editable())
        m_mainWebView->setEditable(true);

    m_mainWebView->platformView().allowsLinkPreview = options.allowsLinkPreview();
    [m_mainWebView->platformView() _setShareSheetCompletesImmediatelyWithResolutionForTesting:YES];
}

UniqueRef<PlatformWebView> TestController::platformCreateOtherPage(PlatformWebView* parentView, WKPageConfigurationRef configuration, const TestOptions& options)
{
    auto view = makeUniqueRef<PlatformWebView>(configuration, options);
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
    m_preferences = (__bridge WKPreferencesRef)[globalWebViewConfiguration() preferences];
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
#if ENABLE(IMAGE_ANALYSIS)
    m_useFakeMachineReadableCodeResultsForImageAnalysis = false;
#endif
    m_calendarSwizzler = nullptr;
    m_overriddenCalendarAndLocaleIdentifiers = { nil, nil };
    
    if (auto* webView = mainWebView()) {
        TestRunnerWKWebView *platformView = webView->platformView();
        platformView._viewScale = 1;
        platformView._minimumEffectiveDeviceWidth = 0;
        platformView._editable = NO;
#if PLATFORM(MAC)
        platformView.allowsMagnification = NO;
        [platformView setMagnification:1 centeredAtPoint:CGPointZero];
#endif
        [platformView _setContinuousSpellCheckingEnabledForTesting:options.shouldShowSpellCheckingDots()];
        [platformView _setGrammarCheckingEnabledForTesting:YES];
        [platformView resetInteractionCallbacks];
        [platformView _resetNavigationGestureStateForTesting];

        auto configuration = platformView.configuration;
        configuration.preferences.textInteractionEnabled = options.textInteractionEnabled();
        configuration.preferences._textExtractionEnabled = options.textExtractionEnabled();
    }

    [LayoutTestSpellChecker uninstallAndReset];

    WebCoreTestSupport::setAdditionalSupportedImageTypesForTesting(String::fromLatin1(options.additionalSupportedImageTypes().c_str()));

    [globalWebsiteDataStoreDelegateClient() clearReportedWindowProxyAccessDomains];
}

void TestController::platformSetStatisticsCrossSiteLoadWithLinkDecoration(WKStringRef fromHost, WKStringRef toHost, bool wasFiltered, void* context, SetStatisticsCrossSiteLoadWithLinkDecorationCallBack callback)
{
    [m_mainWebView->platformView() _setStatisticsCrossSiteLoadWithLinkDecorationForTesting:toWTFString(fromHost) withToHost:toWTFString(toHost) withWasFiltered:wasFiltered withCompletionHandler:^{
        callback(context);
    }];
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

void TestController::removeAllSessionCredentials(CompletionHandler<void(WKTypeRef)>&& completionHandler)
{
    auto types = adoptNS([[NSSet alloc] initWithObjects:_WKWebsiteDataTypeCredentials, nil]);
    [[globalWebViewConfiguration() websiteDataStore] removeDataOfTypes:types.get() modifiedSince:[NSDate distantPast] completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)] () mutable {
        completionHandler(nullptr);
    }).get()];
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

    auto credentialID = adoptNS([[NSData alloc] initWithBase64EncodedString:applicationLabelBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]);
    if (!!applicationLabelBase64)
        [deleteQuery setObject:credentialID.get() forKey:(id)kSecAttrAlias];

    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)deleteQuery.get());
    if (status == errSecItemNotFound) {
        [deleteQuery removeObjectForKey:(id)kSecAttrAlias];
        [deleteQuery setObject:credentialID.get() forKey:(id)kSecAttrApplicationLabel];
        SecItemDelete((__bridge CFDictionaryRef)deleteQuery.get());
    }
}

bool TestController::keyExistsInKeychain(const String& attrLabel, const String& applicationLabelBase64)
{
    auto credentialID = adoptNS([[NSData alloc] initWithBase64EncodedString:applicationLabelBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]);
    auto query = adoptNS([[NSMutableDictionary alloc] init]);
    [query setDictionary:@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrLabel: attrLabel,
        (id)kSecAttrAlias: credentialID.get(),
        (id)kSecUseDataProtectionKeychain: @YES
    }];
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query.get(), NULL);
    if (status == errSecItemNotFound) {
        [query removeObjectForKey:(id)kSecAttrAlias];
        [query setObject:credentialID.get() forKey:(id)kSecAttrApplicationLabel];
        status = SecItemCopyMatching((__bridge CFDictionaryRef)query.get(), NULL);
    }

    if (!status)
        return true;
    ASSERT(status == errSecItemNotFound);
    return false;
}

void TestController::setAllowStorageQuotaIncrease(bool value)
{
    [globalWebsiteDataStoreDelegateClient() setAllowRaisingQuota: value];
}

void TestController::setQuota(uint64_t quota)
{
    [globalWebsiteDataStoreDelegateClient() setQuota:quota];
}

void TestController::setAllowsAnySSLCertificate(bool allows)
{
    m_allowsAnySSLCertificate = allows;
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
    return m_mainWebView->platformView().microphoneCaptureState != WKMediaCaptureStateNone || m_mainWebView->platformView().cameraCaptureState != WKMediaCaptureStateNone;
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
    if (options.advancedPrivacyProtectionsEnabled())
        [webpagePreferences _setNetworkConnectionIntegrityPolicy:_WKWebsiteNetworkConnectionIntegrityPolicyEnabled];
    else
        [webpagePreferences _setNetworkConnectionIntegrityPolicy:_WKWebsiteNetworkConnectionIntegrityPolicyNone];
#if PLATFORM(IOS_FAMILY)
    [webpagePreferences setPreferredContentMode:contentMode(options)];
#endif
    configuration.defaultWebpagePreferences = webpagePreferences.get();
#if HAVE(INLINE_PREDICTIONS)
    configuration.allowsInlinePredictions = options.allowsInlinePredictions();
#endif
}

WKRetainPtr<WKStringRef> TestController::takeViewPortSnapshot()
{
    return adoptWK(WKImageCreateDataURLFromImage(mainWebView()->windowSnapshotImage().get()));
}

static WKRetainPtr<WKArrayRef> createWKArray(NSArray *nsArray)
{
    auto array = adoptWK(WKMutableArrayCreate());

    for (NSString *nsString in nsArray) {
        auto string = adoptWK(WKStringCreateWithCFString((CFStringRef)nsString));
        WKArrayAppendItem(array.get(), string.get());
    }

    return array;
}

WKRetainPtr<WKArrayRef> TestController::getAndClearReportedWindowProxyAccessDomains()
{
    auto domains = createWKArray([globalWebsiteDataStoreDelegateClient() reportedWindowProxyAccessDomains]);
    [globalWebsiteDataStoreDelegateClient() clearReportedWindowProxyAccessDomains];
    return domains;
}

WKRetainPtr<WKStringRef> TestController::getBackgroundFetchIdentifier()
{
    __block String result;
    __block bool isDone = false;
    [globalWebViewConfiguration().get().websiteDataStore _getAllBackgroundFetchIdentifiers:^(NSArray<NSString *> *identifiers) {
        if ([identifiers count])
            result = identifiers[0];
        isDone = true;
    }];
    platformRunUntil(isDone, noTimeout);

    return adoptWK(WKStringCreateWithUTF8CString(result.utf8().data()));
}

void TestController::abortBackgroundFetch(WKStringRef identifier)
{
    __block bool isDone = false;
    [globalWebViewConfiguration().get().websiteDataStore _abortBackgroundFetch:toWTFString(identifier) completionHandler:^() {
        isDone = true;
    }];
    platformRunUntil(isDone, noTimeout);
}

void TestController::pauseBackgroundFetch(WKStringRef identifier)
{
    __block bool isDone = false;
    [globalWebViewConfiguration().get().websiteDataStore _pauseBackgroundFetch:toWTFString(identifier) completionHandler:^() {
        isDone = true;
    }];
    platformRunUntil(isDone, noTimeout);
}

void TestController::resumeBackgroundFetch(WKStringRef identifier)
{
    __block bool isDone = false;
    [globalWebViewConfiguration().get().websiteDataStore _resumeBackgroundFetch:toWTFString(identifier) completionHandler:^() {
        isDone = true;
    }];
    platformRunUntil(isDone, noTimeout);
}

void TestController::simulateClickBackgroundFetch(WKStringRef identifier)
{
    __block bool isDone = false;
    [globalWebViewConfiguration().get().websiteDataStore _clickBackgroundFetch:toWTFString(identifier) completionHandler:^() {
        isDone = true;
    }];
    platformRunUntil(isDone, noTimeout);
}

void TestController::setBackgroundFetchPermission(bool value)
{
    [globalWebsiteDataStoreDelegateClient() setBackgroundFetchPermission:value];
}

WKRetainPtr<WKStringRef> TestController::lastAddedBackgroundFetchIdentifier() const
{
    return adoptWK(WKStringCreateWithCFString((__bridge CFStringRef)[globalWebsiteDataStoreDelegateClient() lastAddedBackgroundFetchIdentifier]));
}

WKRetainPtr<WKStringRef> TestController::lastRemovedBackgroundFetchIdentifier() const
{
    return adoptWK(WKStringCreateWithCFString((__bridge CFStringRef)[globalWebsiteDataStoreDelegateClient() lastRemovedBackgroundFetchIdentifier]));
}

WKRetainPtr<WKStringRef> TestController::lastUpdatedBackgroundFetchIdentifier() const
{
    return adoptWK(WKStringCreateWithCFString((__bridge CFStringRef)[globalWebsiteDataStoreDelegateClient() lastUpdatedBackgroundFetchIdentifier]));
}

WKRetainPtr<WKStringRef> TestController::backgroundFetchState(WKStringRef identifier)
{
    __block bool isDone = false;
    __block String backgroundFetchState;
    [globalWebViewConfiguration().get().websiteDataStore _getBackgroundFetchState:toWTFString(identifier) completionHandler:^(NSDictionary *state) {
        backgroundFetchState = makeString("{ "_s,
            "\"downloaded\":"_s, [[state valueForKey:@"Downloaded"] unsignedIntegerValue], ',',
            "\"isPaused\":"_s, [[state valueForKey:@"IsPaused"] boolValue] ? "true"_s : "false"_s,
        '}');
        isDone = true;
    }];
    platformRunUntil(isDone, noTimeout);
    return toWK(backgroundFetchState);
}

} // namespace WTR
