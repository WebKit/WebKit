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

#include "config.h"
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

#include <wtf/NeverDestroyed.h>
#include <wtf/spi/darwin/dyldSPI.h>

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
    SDKAlignedBehaviors behaviors;
    behaviors.setAll();

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

    if (linkedBefore(dyld_spring_2023_os_versions, DYLD_IOS_VERSION_16_4, DYLD_MACOSX_VERSION_13_3)) {
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

} // namespace WTF
