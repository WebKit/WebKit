/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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
#import "RuntimeApplicationChecksCocoa.h"

#import <Foundation/Foundation.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/spi/darwin/dyldSPI.h>
#import <wtf/text/WTFString.h>

namespace WTF {

static bool linkedBefore(dyld_build_version_t version, uint32_t fallbackIOSVersion, uint32_t fallbackMacOSVersion)
{
#if USE(APPLE_INTERNAL_SDK)
    // dyld_build_version_t values cannot be forward declared, so we fall back to
    // traditional SDK version checks when building against an SDK that
    // does not have dyld_priv.h, or does not define a given version set.
    if (version.platform || version.version)
        return !dyld_program_sdk_at_least(version);
#else
    UNUSED_PARAM(version);
#endif

#if PLATFORM(IOS_FAMILY)
    // FIXME: On iOS-family platforms that are not iOS itself, this comparison is incorrect
    // (e.g., it's wrong to compare a visionOS SDK version to a DYLD_IOS_VERSION_*).
    UNUSED_PARAM(fallbackMacOSVersion);
    return dyld_get_program_sdk_version() < fallbackIOSVersion;
#else
    UNUSED_PARAM(fallbackIOSVersion);
    return dyld_get_program_sdk_version() < fallbackMacOSVersion;
#endif
}

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/RuntimeApplicationChecksCocoaAdditions.cpp>)
#import <WebKitAdditions/RuntimeApplicationChecksCocoaAdditions.cpp>
#else
static void disableAdditionalSDKAlignedBehaviors(SDKAlignedBehaviors&)
{
}
#endif

static SDKAlignedBehaviors computeSDKAlignedBehaviors()
{
    ASSERT(!isInAuxiliaryProcess());

    SDKAlignedBehaviors behaviors;
    behaviors.setAll();

    // This can be removed once Safari calls _setLinkedOnOrAfterEverything everywhere that WebKit deploys.
#if PLATFORM(IOS_FAMILY)
    bool isSafari = WTF::IOSApplication::isMobileSafari();
#elif PLATFORM(MAC)
    bool isSafari = WTF::MacApplication::isSafari();
#endif
    if (isSafari)
        return behaviors;

    auto disableBehavior = [&] (SDKAlignedBehavior behavior) {
        behaviors.clear(static_cast<size_t>(behavior));
    };

    if (linkedBefore(dyld_fall_2015_os_versions, DYLD_IOS_VERSION_9_0, DYLD_MACOSX_VERSION_10_11))
        disableBehavior(SDKAlignedBehavior::PictureInPictureMediaPlayback);

    if (linkedBefore(dyld_fall_2016_os_versions, DYLD_IOS_VERSION_10_0, DYLD_MACOSX_VERSION_10_12)) {
        disableBehavior(SDKAlignedBehavior::MediaTypesRequiringUserActionForPlayback);
        disableBehavior(SDKAlignedBehavior::RequiresUserGestureToLoadVideo);
        disableBehavior(SDKAlignedBehavior::LinkPreviewEnabledByDefault);
        disableBehavior(SDKAlignedBehavior::ConvertsInvalidURLsToBlank);
        disableBehavior(SDKAlignedBehavior::NoTheSecretSocietyHiddenMysteryWindowOpenQuirk);
        disableBehavior(SDKAlignedBehavior::UnprefixedPlaysInlineAttribute);
    }

#if PLATFORM(IOS_FAMILY)
    if (linkedBefore(dyld_fall_2016_os_versions, DYLD_IOS_VERSION_10_0, DYLD_MACOSX_VERSION_10_10))
        disableBehavior(SDKAlignedBehavior::SupportsInitConstructors);
#else
    if (linkedBefore(dyld_fall_2014_os_versions, DYLD_IOS_VERSION_10_0, DYLD_MACOSX_VERSION_10_10))
        disableBehavior(SDKAlignedBehavior::SupportsInitConstructors);
#endif

    if (linkedBefore(dyld_fall_2017_os_versions, DYLD_IOS_VERSION_11_0, DYLD_MACOSX_VERSION_10_13)) {
        disableBehavior(SDKAlignedBehavior::ExceptionsForDuplicateCompletionHandlerCalls);
        disableBehavior(SDKAlignedBehavior::ExpiredOnlyReloadBehavior);
        disableBehavior(SDKAlignedBehavior::DropToNavigateDisallowedByDefault);
        disableBehavior(SDKAlignedBehavior::WebIconDatabaseWarning);
    }

    if (linkedBefore(dyld_spring_2018_os_versions, DYLD_IOS_VERSION_11_3, DYLD_MACOSX_VERSION_10_13_4)) {
        disableBehavior(SDKAlignedBehavior::DisallowsSettingAnyXHRHeaderFromFileURLs);
        disableBehavior(SDKAlignedBehavior::DefaultsToPassiveTouchListenersOnDocument);
    }

    if (linkedBefore(dyld_fall_2018_os_versions, DYLD_IOS_VERSION_12_0, DYLD_MACOSX_VERSION_10_14)) {
        disableBehavior(SDKAlignedBehavior::ScrollViewContentInsetsAreNotObscuringInsets);
        disableBehavior(SDKAlignedBehavior::UIScrollViewDoesNotApplyKeyboardInsetsUnconditionally);
        disableBehavior(SDKAlignedBehavior::MainThreadReleaseAssertionInWebPageProxy);
        disableBehavior(SDKAlignedBehavior::NoMoviStarPlusCORSPreflightQuirk);
        disableBehavior(SDKAlignedBehavior::TimerThreadSafetyChecks);
    }

    if (linkedBefore(dyld_spring_2019_os_versions, DYLD_IOS_VERSION_12_2, DYLD_MACOSX_VERSION_10_14_4)) {
        disableBehavior(SDKAlignedBehavior::LazyGestureRecognizerInstallation);
        disableBehavior(SDKAlignedBehavior::ProcessSwapOnCrossSiteNavigation);
    }

    if (linkedBefore(dyld_fall_2019_os_versions, DYLD_IOS_VERSION_13_0, DYLD_MACOSX_VERSION_10_15)) {
        disableBehavior(SDKAlignedBehavior::NoUnconditionalUniversalSandboxExtension);
        disableBehavior(SDKAlignedBehavior::SnapshotAfterScreenUpdates);
        disableBehavior(SDKAlignedBehavior::SupportsDeviceOrientationAndMotionPermissionAPI);
        disableBehavior(SDKAlignedBehavior::DecidesPolicyBeforeLoadingQuickLookPreview);
        disableBehavior(SDKAlignedBehavior::ExceptionsForRelatedWebViewsUsingDifferentDataStores);
        disableBehavior(SDKAlignedBehavior::ModernCompabilityModeByDefault);
        disableBehavior(SDKAlignedBehavior::HasUIContextMenuInteraction);
        disableBehavior(SDKAlignedBehavior::WKContentViewDoesNotOverrideKeyCommands);
        disableBehavior(SDKAlignedBehavior::SupportsOverflowHiddenOnMainFrame);
        disableBehavior(SDKAlignedBehavior::NoIMDbCSSOMViewScrollingQuirk);
        disableBehavior(SDKAlignedBehavior::DownloadDelegatesCalledOnTheMainThread);
    }

    if (linkedBefore(dyld_late_fall_2019_os_versions, DYLD_IOS_VERSION_13_2, DYLD_MACOSX_VERSION_10_15_1))
        disableBehavior(SDKAlignedBehavior::SiteSpecificQuirksAreEnabledByDefault);

    if (linkedBefore(dyld_spring_2020_os_versions, DYLD_IOS_VERSION_13_4, DYLD_MACOSX_VERSION_10_15_4)) {
        disableBehavior(SDKAlignedBehavior::RestrictsBaseURLSchemes);
        disableBehavior(SDKAlignedBehavior::SendsNativeMouseEvents);
        disableBehavior(SDKAlignedBehavior::MinimizesLanguages);
    }

    if (linkedBefore(dyld_fall_2020_os_versions, DYLD_IOS_VERSION_14_0, DYLD_MACOSX_VERSION_10_16)) {
        disableBehavior(SDKAlignedBehavior::SessionCleanupByDefault);
        disableBehavior(SDKAlignedBehavior::InitializeWebKit2MainThreadAssertion);
        disableBehavior(SDKAlignedBehavior::WKWebsiteDataStoreInitReturningNil);
        disableBehavior(SDKAlignedBehavior::WebSQLDisabledByDefaultInLegacyWebKit);
        disableBehavior(SDKAlignedBehavior::NoLaBanquePostaleQuirks);
        disableBehavior(SDKAlignedBehavior::NoPokerBrosBuiltInTagQuirk);
    }

    if (linkedBefore(dyld_late_fall_2020_os_versions, DYLD_IOS_VERSION_14_2, DYLD_MACOSX_VERSION_10_16))
        disableBehavior(SDKAlignedBehavior::SupportsiOSAppsOnMacOS);

    if (linkedBefore(dyld_spring_2021_os_versions, DYLD_IOS_VERSION_14_5, DYLD_MACOSX_VERSION_11_3)) {
        disableBehavior(SDKAlignedBehavior::DataURLFragmentRemoval);
        disableBehavior(SDKAlignedBehavior::HTMLDocumentSupportedPropertyNames);
        disableBehavior(SDKAlignedBehavior::ObservesClassProperty);
        disableBehavior(SDKAlignedBehavior::NoWeChatScrollingQuirk);
        disableBehavior(SDKAlignedBehavior::SharedNetworkProcess);
        disableBehavior(SDKAlignedBehavior::BlanksViewOnJSPrompt);
        disableBehavior(SDKAlignedBehavior::NoClientCertificateLookup);
        disableBehavior(SDKAlignedBehavior::DefaultsToPassiveWheelListenersOnDocument);
        disableBehavior(SDKAlignedBehavior::AllowsWheelEventGesturesToBecomeNonBlocking);
    }

    if (linkedBefore(dyld_fall_2021_os_versions, DYLD_IOS_VERSION_15_0, DYLD_MACOSX_VERSION_12_00)) {
        disableBehavior(SDKAlignedBehavior::NullOriginForNonSpecialSchemedURLs);
        disableBehavior(SDKAlignedBehavior::DOMWindowReuseRestriction);
        disableBehavior(SDKAlignedBehavior::NoExpandoIndexedPropertiesOnWindow);
        disableBehavior(SDKAlignedBehavior::DoesNotDrainTheMicrotaskQueueWhenCallingObjC);
    }

    if (linkedBefore(dyld_spring_2022_os_versions, DYLD_IOS_VERSION_15_4, DYLD_MACOSX_VERSION_12_3))
        disableBehavior(SDKAlignedBehavior::AuthorizationHeaderOnSameOriginRedirects);

    if (linkedBefore(dyld_fall_2022_os_versions, DYLD_IOS_VERSION_16_0, DYLD_MACOSX_VERSION_13_0)) {
        disableBehavior(SDKAlignedBehavior::NoTypedArrayAPIQuirk);
        disableBehavior(SDKAlignedBehavior::ForbidsDotPrefixedFonts);
        disableBehavior(SDKAlignedBehavior::ContextMenuTriggersLinkActivationNavigationType);
        disableBehavior(SDKAlignedBehavior::DoesNotParseStringEndingWithFullStopAsFloatingPointNumber);
        disableBehavior(SDKAlignedBehavior::UIBackForwardSkipsHistoryItemsWithoutUserGesture);
    }

    if (linkedBefore(dyld_2022_SU_E_os_versions, DYLD_IOS_VERSION_16_4, DYLD_MACOSX_VERSION_13_3)) {
        disableBehavior(SDKAlignedBehavior::NoShowModalDialog);
        disableBehavior(SDKAlignedBehavior::DoesNotAddIntrinsicMarginsToFormControls);
        disableBehavior(SDKAlignedBehavior::ProgrammaticFocusDuringUserScriptShowsInputViews);
        disableBehavior(SDKAlignedBehavior::DefaultsToExcludingBackgroundsWhenPrinting);
        disableBehavior(SDKAlignedBehavior::InspectableDefaultsToDisabled);
        disableBehavior(SDKAlignedBehavior::PushStateFilePathRestriction);
        disableBehavior(SDKAlignedBehavior::NoUNIQLOLazyIframeLoadingQuirk);
        disableBehavior(SDKAlignedBehavior::UsesGameControllerPhysicalInputProfile);
        disableBehavior(SDKAlignedBehavior::ScreenOrientationAPIEnabled);
    }

    if (linkedBefore(dyld_fall_2023_os_versions, DYLD_IOS_VERSION_17_0, DYLD_MACOSX_VERSION_14_0)) {
        disableBehavior(SDKAlignedBehavior::FullySuspendsBackgroundContent);
        disableBehavior(SDKAlignedBehavior::RunningBoardThrottling);
        disableBehavior(SDKAlignedBehavior::PopoverAttributeEnabled);
        disableBehavior(SDKAlignedBehavior::LiveRangeSelectionEnabledForAllApps);
        disableBehavior(SDKAlignedBehavior::DoesNotOverrideUAFromNSUserDefault);
        disableBehavior(SDKAlignedBehavior::EvaluateJavaScriptWithoutTransientActivation);
        disableBehavior(SDKAlignedBehavior::ResettingTransitionCancelsRunningTransitionQuirk);
    }

    if (linkedBefore(dyld_2023_SU_C_os_versions, DYLD_IOS_VERSION_17_2, DYLD_MACOSX_VERSION_14_2)) {
        disableBehavior(SDKAlignedBehavior::OnlyLoadWellKnownAboutURLs);
        disableBehavior(SDKAlignedBehavior::ThrowIfCanDeclareGlobalFunctionFails);
    }

    if (linkedBefore(dyld_2023_SU_E_os_versions, DYLD_IOS_VERSION_17_4, DYLD_MACOSX_VERSION_14_4)) {
        disableBehavior(SDKAlignedBehavior::AsyncFragmentNavigationPolicyDecision);
        disableBehavior(SDKAlignedBehavior::DoNotLoadStyleSheetIfHTTPStatusIsNotOK);
        disableBehavior(SDKAlignedBehavior::ScrollViewSubclassImplementsAddGestureRecognizer);
    }

    if (linkedBefore(dyld_fall_2024_os_versions, DYLD_IOS_VERSION_18_0, DYLD_MACOSX_VERSION_15_0)) {
        disableBehavior(SDKAlignedBehavior::FullySuspendsBackgroundContentImmediately);
        disableBehavior(SDKAlignedBehavior::NoGetElementsByNameQuirk);
        disableBehavior(SDKAlignedBehavior::ApplicationStateTrackerDoesNotObserveWindow);
        disableBehavior(SDKAlignedBehavior::ThrowOnKVCInstanceVariableAccess);
        disableBehavior(SDKAlignedBehavior::BlockOptionallyBlockableMixedContent);
        disableBehavior(SDKAlignedBehavior::LaxCookieSameSiteAttribute);
        disableBehavior(SDKAlignedBehavior::UseCFNetworkNetworkLoader);
        disableBehavior(SDKAlignedBehavior::BrowsingContextControllerSPIAccessRemoved);
        disableBehavior(SDKAlignedBehavior::BlocksConnectionsToAddressWithOnlyZeros);
        disableBehavior(SDKAlignedBehavior::BlockCrossOriginRedirectDownloads);
    }

    disableAdditionalSDKAlignedBehaviors(behaviors);

    return behaviors;
}

static std::optional<SDKAlignedBehaviors>& sdkAlignedBehaviorsValue()
{
    static NeverDestroyed<std::optional<SDKAlignedBehaviors>> behaviors;
    return behaviors.get();
}

const SDKAlignedBehaviors& sdkAlignedBehaviors()
{
    auto& behaviors = sdkAlignedBehaviorsValue();

    if (!behaviors)
        behaviors = computeSDKAlignedBehaviors();

    return *behaviors;
}

void setSDKAlignedBehaviors(SDKAlignedBehaviors behaviors)
{
    // FIXME: Ideally we would assert that `linkedOnOrAfterSDKWithBehavior` had not
    // been called at this point (because its reply could have been inaccurate),
    // but WebPreferences use in Safari (at least) currently prevents this hardening.

    sdkAlignedBehaviorsValue() = behaviors;
}

void enableAllSDKAlignedBehaviors()
{
    SDKAlignedBehaviors behaviors;
    behaviors.setAll();
    setSDKAlignedBehaviors(behaviors);
}

void disableAllSDKAlignedBehaviors()
{
    setSDKAlignedBehaviors({ });
}

bool linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior behavior)
{
    return sdkAlignedBehaviors().get(static_cast<size_t>(behavior));
}

static bool& processIsExtensionValue()
{
    static bool processIsExtension;
    return processIsExtension;
}

bool processIsExtension()
{
    return processIsExtensionValue();
}

void setProcessIsExtension(bool processIsExtension)
{
    processIsExtensionValue() = processIsExtension;
}

#if !ASSERT_MSG_DISABLED
static bool applicationBundleIdentifierOverrideWasQueried;
#endif

// The application bundle identifier gets set to the UIProcess bundle identifier by the WebProcess and
// the Networking upon initialization. It is unset otherwise.
static String& bundleIdentifierOverride()
{
    static NeverDestroyed<String> identifierOverride;
#if !ASSERT_MSG_DISABLED
    applicationBundleIdentifierOverrideWasQueried = true;
#endif
    return identifierOverride;
}

static String& bundleIdentifier()
{
    static NeverDestroyed<String> identifier;
    return identifier;
}

String applicationBundleIdentifier()
{
    // The override only gets set in WebKitTestRunner. If unset, we use the bundle identifier set
    // in WebKit2's WebProcess and NetworkProcess. If that is also unset, we use the main bundle identifier.
    auto identifier = bundleIdentifierOverride();
    ASSERT(identifier.isNull() || RunLoop::isMain());
    if (identifier.isNull())
        identifier = bundleIdentifier();
    ASSERT_WITH_MESSAGE(!isInAuxiliaryProcess() || !!identifier, "applicationBundleIsEqualTo() and applicationBundleStartsWith() must not be called before setApplicationBundleIdentifier() in auxiliary processes");
    return identifier.isNull() ? String([[NSBundle mainBundle] bundleIdentifier]) : identifier;
}

void setApplicationBundleIdentifier(const String& identifier)
{
    ASSERT(RunLoop::isMain());
    bundleIdentifier() = identifier;
}

void setApplicationBundleIdentifierOverride(const String& identifier)
{
    ASSERT(RunLoop::isMain());
    ASSERT_WITH_MESSAGE(!applicationBundleIdentifierOverrideWasQueried, "applicationBundleIsEqualTo() and applicationBundleStartsWith() must not be called before setApplicationBundleIdentifierOverride()");
    bundleIdentifierOverride() = identifier;
}

void clearApplicationBundleIdentifierTestingOverride()
{
    ASSERT(RunLoop::isMain());
    bundleIdentifierOverride() = String();
#if !ASSERT_MSG_DISABLED
    applicationBundleIdentifierOverrideWasQueried = false;
#endif
}

static String& presentingApplicationBundleIdentifierStorage()
{
    static MainThreadNeverDestroyed<String> identifier;
    return identifier;
}

void setPresentingApplicationBundleIdentifier(const String& identifier)
{
    presentingApplicationBundleIdentifierStorage() = identifier;
}

const String& presentingApplicationBundleIdentifier()
{
    return presentingApplicationBundleIdentifierStorage();
}

static bool applicationBundleIsEqualTo(const String& bundleIdentifierString)
{
    return applicationBundleIdentifier() == bundleIdentifierString;
}

bool CocoaApplication::isIBooks()
{
    static bool isIBooks = applicationBundleIsEqualTo("com.apple.iBooksX"_s) || applicationBundleIsEqualTo("com.apple.iBooks"_s);
    return isIBooks;
}

bool CocoaApplication::isWebkitTestRunner()
{
    static bool isWebkitTestRunner = applicationBundleIsEqualTo("com.apple.WebKit.WebKitTestRunner"_s);
    return isWebkitTestRunner;
}

#if PLATFORM(MAC)

bool MacApplication::isSafari()
{
    static bool isSafari = applicationBundleIsEqualTo("com.apple.Safari"_s)
        || applicationBundleIsEqualTo("com.apple.SafariTechnologyPreview"_s)
        || applicationBundleIdentifier().startsWith("com.apple.Safari."_s);
    return isSafari;
}

bool MacApplication::isAppleMail()
{
    static bool isAppleMail = applicationBundleIsEqualTo("com.apple.mail"_s);
    return isAppleMail;
}

bool MacApplication::isAdobeInstaller()
{
    static bool isAdobeInstaller = applicationBundleIsEqualTo("com.adobe.Installers.Setup"_s);
    return isAdobeInstaller;
}

bool MacApplication::isMiniBrowser()
{
    static bool isMiniBrowser = applicationBundleIsEqualTo("org.webkit.MiniBrowser"_s);
    return isMiniBrowser;
}

bool MacApplication::isQuickenEssentials()
{
    static bool isQuickenEssentials = applicationBundleIsEqualTo("com.intuit.QuickenEssentials"_s);
    return isQuickenEssentials;
}

bool MacApplication::isVersions()
{
    static bool isVersions = applicationBundleIsEqualTo("com.blackpixel.versions"_s);
    return isVersions;
}

bool MacApplication::isHRBlock()
{
    static bool isHRBlock = applicationBundleIsEqualTo("com.hrblock.tax.2010"_s);
    return isHRBlock;
}

bool MacApplication::isSolidStateNetworksDownloader()
{
    static bool isSolidStateNetworksDownloader = applicationBundleIsEqualTo("com.solidstatenetworks.awkhost"_s);
    return isSolidStateNetworksDownloader;
}

bool MacApplication::isEpsonSoftwareUpdater()
{
    static bool isEpsonSoftwareUpdater = applicationBundleIsEqualTo("com.epson.EPSON_Software_Updater"_s);
    return isEpsonSoftwareUpdater;
}

bool MacApplication::isMimeoPhotoProject()
{
    static bool isMimeoPhotoProject = applicationBundleIsEqualTo("com.mimeo.Mimeo.PhotoProject"_s);
    return isMimeoPhotoProject;
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

static bool applicationBundleStartsWith(const String& bundleIdentifierPrefix)
{
    return applicationBundleIdentifier().startsWith(bundleIdentifierPrefix);
}

bool IOSApplication::isMobileMail()
{
    static bool isMobileMail = applicationBundleIsEqualTo("com.apple.mobilemail"_s);
    return isMobileMail;
}

bool IOSApplication::isMailCompositionService()
{
    static bool isMailCompositionService = applicationBundleIsEqualTo("com.apple.MailCompositionService"_s);
    return isMailCompositionService;
}

bool IOSApplication::isMobileSafari()
{
    static bool isMobileSafari = applicationBundleIsEqualTo("com.apple.mobilesafari"_s);
    return isMobileSafari;
}

bool IOSApplication::isSafariViewService()
{
    static bool isSafariViewService = applicationBundleIsEqualTo("com.apple.SafariViewService"_s);
    return isSafariViewService;
}

bool IOSApplication::isIMDb()
{
    static bool isIMDb = applicationBundleIsEqualTo("com.imdb.imdb"_s);
    return isIMDb;
}

bool IOSApplication::isGmail()
{
    static bool isGmail = applicationBundleIsEqualTo("com.google.Gmail"_s);
    return isGmail;
}

bool IOSApplication::isWebBookmarksD()
{
    static bool isWebBookmarksD = applicationBundleIsEqualTo("com.apple.webbookmarksd"_s);
    return isWebBookmarksD;
}

bool IOSApplication::isDumpRenderTree()
{
    // We use a prefix match instead of strict equality since multiple instances of DumpRenderTree
    // may be launched, where the bundle identifier of each instance has a unique suffix.
    static bool isDumpRenderTree = applicationBundleIsEqualTo("org.webkit.DumpRenderTree"_s); // e.g. org.webkit.DumpRenderTree0
    return isDumpRenderTree;
}

bool IOSApplication::isMobileStore()
{
    static bool isMobileStore = applicationBundleIsEqualTo("com.apple.MobileStore"_s);
    return isMobileStore;
}

bool IOSApplication::isSpringBoard()
{
    static bool isSpringBoard = applicationBundleIsEqualTo("com.apple.springboard"_s);
    return isSpringBoard;
}

// FIXME: this needs to be changed when the WebProcess is changed to an XPC service.
bool IOSApplication::isWebProcess()
{
    return isInWebProcess();
}

bool IOSApplication::isBackboneApp()
{
    static bool isBackboneApp = applicationBundleIsEqualTo("com.backbonelabs.backboneapp"_s);
    return isBackboneApp;
}

bool IOSApplication::isIBooksStorytime()
{
    static bool isIBooksStorytime = applicationBundleIsEqualTo("com.apple.TVBooks"_s);
    return isIBooksStorytime;
}

bool IOSApplication::isTheSecretSocietyHiddenMystery()
{
    static bool isTheSecretSocietyHiddenMystery = applicationBundleIsEqualTo("com.g5e.secretsociety"_s);
    return isTheSecretSocietyHiddenMystery;
}

bool IOSApplication::isCardiogram()
{
    static bool isCardiogram = applicationBundleIsEqualTo("com.cardiogram.ios.heart"_s);
    return isCardiogram;
}

bool IOSApplication::isNike()
{
    static bool isNike = applicationBundleIsEqualTo("com.nike.omega"_s);
    return isNike;
}

bool IOSApplication::isMoviStarPlus()
{
    static bool isMoviStarPlus = applicationBundleIsEqualTo("com.prisatv.yomvi"_s);
    return isMoviStarPlus;
}

bool IOSApplication::isFirefox()
{
    static bool isFirefox = applicationBundleIsEqualTo("org.mozilla.ios.Firefox"_s);
    return isFirefox;
}

bool IOSApplication::isHoYoLAB()
{
    static bool isHoYoLAB = applicationBundleIsEqualTo("com.miHoYo.HoYoLAB"_s);
    return isHoYoLAB;
}

bool IOSApplication::isAppleApplication()
{
    static bool isAppleApplication = applicationBundleStartsWith("com.apple."_s);
    return isAppleApplication;
}

bool IOSApplication::isEvernote()
{
    static bool isEvernote = applicationBundleIsEqualTo("com.evernote.iPhone.Evernote"_s);
    return isEvernote;
}

bool IOSApplication::isEventbrite()
{
    static bool isEventbrite = applicationBundleIsEqualTo("com.eventbrite.attendee"_s);
    return isEventbrite;
}

bool IOSApplication::isDataActivation()
{
    static bool isDataActivation = applicationBundleIsEqualTo("com.apple.DataActivation"_s);
    return isDataActivation;
}

bool IOSApplication::isMiniBrowser()
{
    static bool isMiniBrowser = applicationBundleIsEqualTo("org.webkit.MiniBrowser"_s);
    return isMiniBrowser;
}

bool IOSApplication::isNews()
{
    static bool isNews = applicationBundleIsEqualTo("com.apple.news"_s);
    return isNews;
}

bool IOSApplication::isStocks()
{
    static bool isStocks = applicationBundleIsEqualTo("com.apple.stocks"_s);
    return isStocks;
}

bool IOSApplication::isFeedly()
{
    static bool isFeedly = applicationBundleIsEqualTo("com.devhd.feedly"_s);
    return isFeedly;
}

bool IOSApplication::isPocketCity()
{
    static bool isPocketCity = applicationBundleIsEqualTo("com.codebrewgames.pocketcity"_s);
    return isPocketCity;
}

bool IOSApplication::isEssentialSkeleton()
{
    static bool isEssentialSkeleton = applicationBundleIsEqualTo("com.3d4medical.EssentialSkeleton"_s);
    return isEssentialSkeleton;
}

bool IOSApplication::isLaBanquePostale()
{
    static bool isLaBanquePostale = applicationBundleIsEqualTo("fr.labanquepostale.moncompte"_s);
    return isLaBanquePostale;
}

bool IOSApplication::isESPNFantasySports()
{
    static bool isESPNFantasySports = applicationBundleIsEqualTo("com.espn.fantasyFootball"_s);
    return isESPNFantasySports;
}

bool IOSApplication::isDoubleDown()
{
    static bool isDoubleDown = applicationBundleIsEqualTo("com.doubledowninteractive.DDCasino"_s);
    return isDoubleDown;
}

bool IOSApplication::isFIFACompanion()
{
    static bool isFIFACompanion = applicationBundleIsEqualTo("com.ea.ios.fifaultimate"_s);
    return isFIFACompanion;
}

bool IOSApplication::isNoggin()
{
    static bool isNoggin = applicationBundleIsEqualTo("com.mtvn.noggin"_s);
    return isNoggin;
}

bool IOSApplication::isOKCupid()
{
    static bool isOKCupid = applicationBundleIsEqualTo("com.okcupid.app"_s);
    return isOKCupid;
}

bool IOSApplication::isJWLibrary()
{
    static bool isJWLibrary = applicationBundleIsEqualTo("org.jw.jwlibrary"_s);
    return isJWLibrary;
}

bool IOSApplication::isPaperIO()
{
    static bool isPaperIO = applicationBundleIsEqualTo("io.voodoo.paperio"_s);
    return isPaperIO;
}

bool IOSApplication::isCrunchyroll()
{
    static bool isCrunchyroll = applicationBundleIsEqualTo("com.crunchyroll.iphone"_s);
    return isCrunchyroll;
}

bool IOSApplication::isWechat()
{
    static bool isWechat = applicationBundleIsEqualTo("com.tencent.xin"_s);
    return isWechat;
}

bool IOSApplication::isUNIQLOApp()
{
    static bool isUNIQLO = applicationBundleIdentifier().startsWith("com.uniqlo"_s);
    return isUNIQLO;
}

bool IOSApplication::isLutron()
{
    static bool isLutronApp = applicationBundleIsEqualTo("com.lutron.lsb"_s);
    return isLutronApp;
}

bool IOSApplication::isDOFUSTouch()
{
    static bool isDOFUSTouch = applicationBundleIsEqualTo("com.ankama.dofustouch"_s);
    return isDOFUSTouch;
}

bool IOSApplication::isMyRideK12()
{
    static bool isMyRideK12 = applicationBundleIsEqualTo("com.tylertech.myridek12"_s);
    return isMyRideK12;
}

bool IOSApplication::isHimalaya()
{
    static bool isHimalayaApp = applicationBundleIsEqualTo("com.gemd.iting"_s);
    return isHimalayaApp;
}

#endif // PLATFORM(IOS_FAMILY)

} // namespace WTF
