/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "RuntimeApplicationChecksCocoa.h"

#import <Foundation/Foundation.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/spi/darwin/OSVariantSPI.h>
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
NODELETE static void disableAdditionalSDKAlignedBehaviors(SDKAlignedBehaviors&)
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
        disableBehavior(SDKAlignedBehavior::DefaultsToPassiveTouchListenersOnDocument);
    }

    if (linkedBefore(dyld_fall_2018_os_versions, DYLD_IOS_VERSION_12_0, DYLD_MACOSX_VERSION_10_14)) {
        disableBehavior(SDKAlignedBehavior::ScrollViewContentInsetsAreNotObscuringInsets);
        disableBehavior(SDKAlignedBehavior::UIScrollViewDoesNotApplyKeyboardInsetsUnconditionally);
        disableBehavior(SDKAlignedBehavior::MainThreadReleaseAssertionInWebPageProxy);
        disableBehavior(SDKAlignedBehavior::TimerThreadSafetyChecks);
    }

    if (linkedBefore(dyld_spring_2019_os_versions, DYLD_IOS_VERSION_12_2, DYLD_MACOSX_VERSION_10_14_4)) {
        disableBehavior(SDKAlignedBehavior::LazyGestureRecognizerInstallation);
    }

    if (linkedBefore(dyld_fall_2019_os_versions, DYLD_IOS_VERSION_13_0, DYLD_MACOSX_VERSION_10_15)) {
        disableBehavior(SDKAlignedBehavior::NoUnconditionalUniversalSandboxExtension);
        disableBehavior(SDKAlignedBehavior::SnapshotAfterScreenUpdates);
        disableBehavior(SDKAlignedBehavior::SupportsDeviceOrientationAndMotionPermissionAPI);
        disableBehavior(SDKAlignedBehavior::DecidesPolicyBeforeLoadingQuickLookPreview);
        disableBehavior(SDKAlignedBehavior::ExceptionsForRelatedWebViewsUsingDifferentDataStores);
        disableBehavior(SDKAlignedBehavior::ModernCompabilityModeByDefault);
        disableBehavior(SDKAlignedBehavior::HasUIContextMenuInteraction);
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
        disableBehavior(SDKAlignedBehavior::NoPokerBrosBuiltInTagQuirk);
    }

    if (linkedBefore(dyld_spring_2021_os_versions, DYLD_IOS_VERSION_14_5, DYLD_MACOSX_VERSION_11_3)) {
        disableBehavior(SDKAlignedBehavior::DataURLFragmentRemoval);
        disableBehavior(SDKAlignedBehavior::HTMLDocumentSupportedPropertyNames);
        disableBehavior(SDKAlignedBehavior::ObservesClassProperty);
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
        disableBehavior(SDKAlignedBehavior::ScreenOrientationAPIEnabled);
    }

    if (linkedBefore(dyld_fall_2023_os_versions, DYLD_IOS_VERSION_17_0, DYLD_MACOSX_VERSION_14_0)) {
        disableBehavior(SDKAlignedBehavior::FullySuspendsBackgroundContent);
        disableBehavior(SDKAlignedBehavior::RunningBoardThrottling);
        disableBehavior(SDKAlignedBehavior::PopoverAttributeEnabled);
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
        disableBehavior(SDKAlignedBehavior::ApplicationStateTrackerDoesNotObserveWindow);
        disableBehavior(SDKAlignedBehavior::ThrowOnKVCInstanceVariableAccess);
        disableBehavior(SDKAlignedBehavior::LaxCookieSameSiteAttribute);
        disableBehavior(SDKAlignedBehavior::UseCFNetworkNetworkLoader);
        disableBehavior(SDKAlignedBehavior::BlocksConnectionsToAddressWithOnlyZeros);
        disableBehavior(SDKAlignedBehavior::BlockCrossOriginRedirectDownloads);
        disableBehavior(SDKAlignedBehavior::SupportGameControllerEventInteractionAPI);
    }

    if (linkedBefore(dyld_2024_SU_C_os_versions, DYLD_IOS_VERSION_18_2, DYLD_MACOSX_VERSION_15_2))
        disableBehavior(SDKAlignedBehavior::BlobFileAccessEnforcementAndNetworkProcessRoundTrip);

    if (linkedBefore(dyld_2024_SU_E_os_versions, DYLD_IOS_VERSION_18_4, DYLD_MACOSX_VERSION_15_4)) {
        disableBehavior(SDKAlignedBehavior::DevolvableWidgets);
        disableBehavior(SDKAlignedBehavior::SetSelectionRangeCachesSelectionIfNotFocusedOrSelected);
        disableBehavior(SDKAlignedBehavior::DispatchFocusEventBeforeNotifyingClient);
        disableBehavior(SDKAlignedBehavior::BlobFileAccessEnforcement);
    }

    if (linkedBefore(dyld_2024_SU_F_os_versions, DYLD_IOS_VERSION_18_5, DYLD_MACOSX_VERSION_15_5))
        disableBehavior(SDKAlignedBehavior::NavigationActionSourceFrameNonNull);

    if (linkedBefore(dyld_2025_SU_B_os_versions, DYLD_IOS_VERSION_26_1, DYLD_MACOSX_VERSION_26_1))
        disableBehavior(SDKAlignedBehavior::GetBoundingClientRectZoomed);

    if (linkedBefore(dyld_fall_2026_os_versions, DYLD_IOS_VERSION_27_0, DYLD_MACOSX_VERSION_27_0)) {
        disableBehavior(SDKAlignedBehavior::IgnorePageLocationDuringHardPocketEligibilityCheck);
        disableBehavior(SDKAlignedBehavior::ScrollPocketInFullscreen);
        disableBehavior(SDKAlignedBehavior::UserSelectSupersedesWebkitUserSelect);
    }

    // This should be disabled unconditionally until WTF::String is made thread-safe. See the comment in UserScript.cpp.
    // It's only enabled for clients that purposely enable all LOOA checks.
    disableBehavior(SDKAlignedBehavior::EnableUserScriptAndUserStyleInterning);

    if (linkedBefore(dyld_2025_SU_G_os_versions, DYLD_IOS_VERSION_26_6, DYLD_MACOSX_VERSION_26_6))
        disableBehavior(SDKAlignedBehavior::NetworkProcessInheritsNetworkAccessFromUIProcess);

    if (linkedBefore(dyld_fall_2025_os_versions, DYLD_IOS_VERSION_19_0, DYLD_MACOSX_VERSION_16_0)) {
        disableBehavior(SDKAlignedBehavior::ConvertsInvalidURLsToNull);
        disableBehavior(SDKAlignedBehavior::MutationEventsDisabledByDefault);
        disableBehavior(SDKAlignedBehavior::EnableTrustedTypesByDefault);
        disableBehavior(SDKAlignedBehavior::BrowsingContextControllerMethodStubRemoved);
        disableBehavior(SDKAlignedBehavior::AutoLayoutInWKWebView);
        disableBehavior(SDKAlignedBehavior::DidFailProvisionalNavigationWithErrorForFileURLNavigation);
    }

    if (linkedBefore(dyld_2025_SU_C_os_versions, DYLD_IOS_VERSION_26_2, DYLD_MACOSX_VERSION_26_2)) {
        disableBehavior(SDKAlignedBehavior::ExecutionTimingChangeOfModuleScripts);
        disableBehavior(SDKAlignedBehavior::JavaScriptEvaluationResultWithoutSerializedScriptValue);
    }

    if (linkedBefore(dyld_2025_SU_E_os_versions, DYLD_IOS_VERSION_26_4, DYLD_MACOSX_VERSION_26_4)) {
        disableBehavior(SDKAlignedBehavior::GetBoundingClientRectZoomed);
        disableBehavior(SDKAlignedBehavior::NoFontFaceSetConstructor);
    }

    if (linkedBefore(dyld_fall_2026_os_versions, DYLD_IOS_VERSION_27_0, DYLD_MACOSX_VERSION_27_0)) {
        disableBehavior(SDKAlignedBehavior::AdjustColorExtensionsForHorizontalBannerViewOverlays);
        disableBehavior(SDKAlignedBehavior::CrashWhenPreconnectingFromBackgroundThread);
        disableBehavior(SDKAlignedBehavior::CrashWhenMutatingProcessAssertionsFromBackgroundThread);
        disableBehavior(SDKAlignedBehavior::NoHTMLEnhancedSelectParsingQuirk);
        disableBehavior(SDKAlignedBehavior::SuppressKeypressForModifierShortcuts);
        disableBehavior(SDKAlignedBehavior::ScrollColorExtensionGrowsDuringRubberBanding);
        disableBehavior(SDKAlignedBehavior::ManagedRefreshControlAppearance);
        disableBehavior(SDKAlignedBehavior::ExposePartitionFromWKHTTPCookieStoreAPI);
        disableBehavior(SDKAlignedBehavior::NoMediaLayerTeardownOnPageVisibilityChangeQuirk);
    }

    disableAdditionalSDKAlignedBehaviors(behaviors);

    return behaviors;
}

NODELETE static std::optional<SDKAlignedBehaviors>& sdkAlignedBehaviorsValue()
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

NODELETE static bool& processIsExtensionValue()
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
NODELETE static RefPtr<StringImpl>& bundleIdentifierOverride()
{
    static NeverDestroyed<RefPtr<StringImpl>> identifierOverride;
#if !ASSERT_MSG_DISABLED
    applicationBundleIdentifierOverrideWasQueried = true;
#endif
    return identifierOverride;
}

NODELETE static RefPtr<StringImpl>& bundleIdentifier()
{
    static NeverDestroyed<RefPtr<StringImpl>> identifier;
    return identifier;
}

RefPtr<StringImpl> applicationBundleIdentifier()
{
    // The override only gets set in WebKitTestRunner. If unset, we use the bundle identifier set
    // in WebKit2's WebProcess and NetworkProcess. If that is also unset, we use the main bundle identifier.
    auto identifier = bundleIdentifierOverride();
    ASSERT(!identifier || RunLoop::isMain());
    if (!identifier)
        identifier = bundleIdentifier();
    ASSERT_WITH_MESSAGE(!isInAuxiliaryProcess() || !!identifier, "applicationBundleIsEqualTo() and applicationBundleStartsWith() must not be called before setApplicationBundleIdentifier() in auxiliary processes");
    return !identifier ? StringImpl::create([[NSBundle mainBundle] bundleIdentifier]) : identifier;
}

void setApplicationBundleIdentifier(StringImpl* identifier)
{
    ASSERT(RunLoop::isMain());
    bundleIdentifier() = identifier;
}

void setApplicationBundleIdentifierOverride(StringImpl* identifier)
{
    ASSERT(RunLoop::isMain());
    ASSERT_WITH_MESSAGE(!applicationBundleIdentifierOverrideWasQueried, "applicationBundleIsEqualTo() and applicationBundleStartsWith() must not be called before setApplicationBundleIdentifierOverride()");
    bundleIdentifierOverride() = identifier;
}

void clearApplicationBundleIdentifierTestingOverride()
{
    ASSERT(RunLoop::isMain());
    bundleIdentifierOverride() = nullptr;
#if !ASSERT_MSG_DISABLED
    applicationBundleIdentifierOverrideWasQueried = false;
#endif
}

#if USE(SOURCE_APPLICATION_AUDIT_DATA)

static std::optional<audit_token_t> applicationAuditTokenStorage;

void setApplicationAuditToken(audit_token_t token)
{
    applicationAuditTokenStorage = token;
}

std::optional<audit_token_t> applicationAuditToken()
{
    return applicationAuditTokenStorage;
}

#endif

bool isInBaseSystem()
{
#if PLATFORM(MAC)
    static bool isBaseSystem = os_variant_is_basesystem("WebKit");
    return isBaseSystem;
#else
    return false;
#endif
}

static bool applicationBundleIsEqualTo(StringImpl* bundleIdentifierString)
{
    return equal(applicationBundleIdentifier(), bundleIdentifierString);
}

bool CocoaApplication::isAppleBooks()
{
    static bool isAppleBooks = applicationBundleIsEqualTo("com.apple.iBooksX"_si) || applicationBundleIsEqualTo("com.apple.iBooks"_si);
    return isAppleBooks;
}

bool CocoaApplication::isWebkitTestRunner()
{
    static bool isWebkitTestRunner = applicationBundleIsEqualTo("com.apple.WebKit.WebKitTestRunner"_si);
    return isWebkitTestRunner;
}

bool CocoaApplication::isAppleApplication()
{
    static bool isAppleApplication = startsWith(applicationBundleIdentifier(), "com.apple."_s);
    return isAppleApplication;
}

bool CocoaApplication::isDumpRenderTree()
{
#if PLATFORM(MAC)
    static bool isDumpRenderTree = applicationBundleIsEqualTo("com.apple.WebKit.DumpRenderTree"_si);
    return isDumpRenderTree;
#else
    static bool isDumpRenderTree = applicationBundleIsEqualTo("org.webkit.DumpRenderTree"_si);
    return isDumpRenderTree;
#endif
}

bool CocoaApplication::shouldOSFaultLogForAppleApplicationUsingWebKit1()
{
    static bool isInternalBuild = os_variant_allows_internal_security_policies("com.apple.WebKit");
    if (!isInternalBuild)
        return false;

    static bool bundleIdentifierShouldLog = [] {
        if (!isAppleApplication())
            return false;

        RefPtr<StringImpl> bundleIdentifier = applicationBundleIdentifier();
        if (startsWith(bundleIdentifier, "com.apple.InstallerRemotePluginService."_s))
            return false;
        if (applicationBundleIsEqualTo("com.apple.WebKit.TestWebKitAPI"_si))
            return false;
        if (applicationBundleIsEqualTo("com.apple.dt.Xcode"_si))
            return false;
        if (applicationBundleIsEqualTo("com.apple.ibtool"_si))
            return false;
        if (applicationBundleIsEqualTo("com.apple.xctest"_si))
            return false;
        if (CocoaApplication::isDumpRenderTree())
            return false;

        return true;
    }();

    return bundleIdentifierShouldLog && !((rand() * 1000) % 1000);
}

#if PLATFORM(MAC)

bool MacApplication::isSafari()
{
    static bool isSafari = applicationBundleIsEqualTo("com.apple.Safari"_si)
        || isSafariTechnologyPreview()
        || startsWith(applicationBundleIdentifier(), "com.apple.Safari."_s);
    return isSafari;
}

bool MacApplication::isSafariTechnologyPreview()
{
    static bool isSafari = applicationBundleIsEqualTo("com.apple.SafariTechnologyPreview"_si);
    return isSafari;
}

bool MacApplication::isSafariWebApp()
{
    static bool isSafariWebApp = applicationBundleIsEqualTo("com.apple.Safari.WebApp"_si);
    return isSafariWebApp;
}

bool MacApplication::isAppleMail()
{
    static bool isAppleMail = applicationBundleIsEqualTo("com.apple.mail"_si);
    return isAppleMail;
}

bool MacApplication::isAdobeInstaller()
{
    static bool isAdobeInstaller = applicationBundleIsEqualTo("com.adobe.Installers.Setup"_si);
    return isAdobeInstaller;
}

bool MacApplication::isCaptiveNetworkAssistant()
{
    static bool isCaptiveNetworkAssistant = applicationBundleIsEqualTo("com.apple.CaptiveNetworkAssistant"_si);
    return isCaptiveNetworkAssistant;
}

bool MacApplication::isMiniBrowser()
{
    static bool isMiniBrowser = applicationBundleIsEqualTo("org.webkit.MiniBrowser"_si) || applicationBundleIsEqualTo("org.webkit.SwiftBrowser"_si);
    return isMiniBrowser;
}

bool MacApplication::isQuickenEssentials()
{
    static bool isQuickenEssentials = applicationBundleIsEqualTo("com.intuit.QuickenEssentials"_si);
    return isQuickenEssentials;
}

bool MacApplication::isVersions()
{
    static bool isVersions = applicationBundleIsEqualTo("com.blackpixel.versions"_si);
    return isVersions;
}

bool MacApplication::isHRBlock()
{
    static bool isHRBlock = applicationBundleIsEqualTo("com.hrblock.tax.2010"_si);
    return isHRBlock;
}

bool MacApplication::isTurboTax()
{
    static bool isTurboTax = startsWith(applicationBundleIdentifier(), "com.intuit.turbotax."_s);
    return isTurboTax;
}

bool MacApplication::isEpsonSoftwareUpdater()
{
    static bool isEpsonSoftwareUpdater = applicationBundleIsEqualTo("com.epson.EPSON_Software_Updater"_si);
    return isEpsonSoftwareUpdater;
}

bool MacApplication::isMimeoPhotoProject()
{
    static bool isMimeoPhotoProject = applicationBundleIsEqualTo("com.mimeo.Mimeo.PhotoProject"_si);
    return isMimeoPhotoProject;
}

bool MacApplication::isGridLegends()
{
    static bool isGridLegends = applicationBundleIsEqualTo("com.feralinteractive.gridlegends"_si);
    return isGridLegends;
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

bool IOSApplication::isAppleWebApp()
{
    static bool isWebApp = applicationBundleIsEqualTo("com.apple.webapp"_si);
    return isWebApp;
}

bool IOSApplication::isMobileMail()
{
    static bool isMobileMail = applicationBundleIsEqualTo("com.apple.mobilemail"_si);
    return isMobileMail;
}

bool IOSApplication::isMaild()
{
    static bool isMaild = applicationBundleIsEqualTo("com.apple.email.maild"_si);
    return isMaild;
}

bool IOSApplication::isMailCompositionService()
{
    static bool isMailCompositionService = applicationBundleIsEqualTo("com.apple.MailCompositionService"_si);
    return isMailCompositionService;
}

bool IOSApplication::isMobileSafari()
{
    static bool isMobileSafari = applicationBundleIsEqualTo("com.apple.mobilesafari"_si);
    return isMobileSafari;
}

bool IOSApplication::isSafariViewService()
{
    static bool isSafariViewService = applicationBundleIsEqualTo("com.apple.SafariViewService"_si);
    return isSafariViewService;
}

bool IOSApplication::isWebBookmarksD()
{
    static bool isWebBookmarksD = applicationBundleIsEqualTo("com.apple.webbookmarksd"_si);
    return isWebBookmarksD;
}

bool IOSApplication::isMobileStore()
{
    static bool isMobileStore = applicationBundleIsEqualTo("com.apple.MobileStore"_si);
    return isMobileStore;
}

// FIXME: this needs to be changed when the WebProcess is changed to an XPC service.
bool IOSApplication::isWebProcess()
{
    return isInWebProcess();
}

bool IOSApplication::isHoYoLAB()
{
    static bool isHoYoLAB = applicationBundleIsEqualTo("com.miHoYo.HoYoLAB"_si);
    return isHoYoLAB;
}

bool IOSApplication::isAmazon()
{
    static bool isAmazon = applicationBundleIsEqualTo("com.amazon.Amazon"_si);
    return isAmazon;
}

bool IOSApplication::isDataActivation()
{
    static bool isDataActivation = applicationBundleIsEqualTo("com.apple.DataActivation"_si);
    return isDataActivation;
}

bool IOSApplication::isMiniBrowser()
{
    static bool isMiniBrowser = applicationBundleIsEqualTo("org.webkit.MiniBrowser"_si) || applicationBundleIsEqualTo("org.webkit.SwiftBrowser"_si);
    return isMiniBrowser;
}

bool IOSApplication::isNews()
{
    static bool isNews = applicationBundleIsEqualTo("com.apple.news"_si);
    return isNews;
}

bool IOSApplication::isStocks()
{
    static bool isStocks = applicationBundleIsEqualTo("com.apple.stocks"_si);
    return isStocks;
}

bool IOSApplication::isFeedly()
{
    static bool isFeedly = applicationBundleIsEqualTo("com.devhd.feedly"_si);
    return isFeedly;
}

bool IOSApplication::isGmail()
{
    static bool isGmail = applicationBundleIsEqualTo("com.google.Gmail"_si);
    return isGmail;
}

bool IOSApplication::isEssentialSkeleton()
{
    static bool isEssentialSkeleton = applicationBundleIsEqualTo("com.3d4medical.EssentialSkeleton"_si);
    return isEssentialSkeleton;
}

bool IOSApplication::isUNIQLOApp()
{
    static bool isUNIQLO = startsWith(applicationBundleIdentifier(), "com.uniqlo"_s);
    return isUNIQLO;
}

bool IOSApplication::isDOFUSTouch()
{
    static bool isDOFUSTouch = applicationBundleIsEqualTo("com.ankama.dofustouch"_si);
    return isDOFUSTouch;
}

bool IOSApplication::isMoonPlayer()
{
    static bool isMoonPlayer = applicationBundleIsEqualTo("com.innovis.moonplayer"_si);
    return isMoonPlayer;
}

bool IOSApplication::isMyRideK12()
{
    static bool isMyRideK12 = applicationBundleIsEqualTo("com.tylertech.myridek12"_si);
    return isMyRideK12;
}

bool IOSApplication::isHimalaya()
{
    static bool isHimalayaApp = applicationBundleIsEqualTo("com.gemd.iting"_si);
    return isHimalayaApp;
}

bool IOSApplication::isTableau()
{
    static bool isTableau = startsWith(applicationBundleIdentifier(), "com.tableausoftware"_s);
    return isTableau;
}

bool IOSApplication::isTubular()
{
    static bool isTubular = startsWith(applicationBundleIdentifier(), "com.aaronbrannan.youtubeAVP"_s);
    return isTubular;
}

bool IOSApplication::isLensApp()
{
    static bool isLensApp = applicationBundleIsEqualTo("com.float.lens"_si);
    return isLensApp;
}

#endif // PLATFORM(IOS_FAMILY)

} // namespace WTF
