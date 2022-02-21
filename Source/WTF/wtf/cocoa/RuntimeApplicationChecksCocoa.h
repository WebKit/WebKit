/*
 * Copyright (C) 2009-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <optional>
#include <wtf/Forward.h>

#include <wtf/spi/darwin/dyldSPI.h>

namespace WTF {

enum class SDKVersion : uint32_t {
#if PLATFORM(IOS_FAMILY)
    FirstWithPictureInPictureMediaPlayback = DYLD_IOS_VERSION_9_0,
    FirstWithMediaTypesRequiringUserActionForPlayback = DYLD_IOS_VERSION_10_0,
    FirstThatRequiresUserGestureToLoadVideo = DYLD_IOS_VERSION_10_0,
    FirstWithLinkPreviewEnabledByDefault = DYLD_IOS_VERSION_10_0,
    FirstThatConvertsInvalidURLsToBlank = DYLD_IOS_VERSION_10_0,
    FirstWithoutTheSecretSocietyHiddenMysteryWindowOpenQuirk = DYLD_IOS_VERSION_10_0,
    FirstWithUnprefixedPlaysInlineAttribute = DYLD_IOS_VERSION_10_0,
    FirstVersionThatSupportsInitConstructors = DYLD_IOS_VERSION_10_0,
    FirstWithExceptionsForDuplicateCompletionHandlerCalls = DYLD_IOS_VERSION_11_0,
    FirstWithExpiredOnlyReloadBehavior = DYLD_IOS_VERSION_11_0,
    FirstThatDisallowsSettingAnyXHRHeaderFromFileURLs = DYLD_IOS_VERSION_11_3,
    FirstThatDefaultsToPassiveTouchListenersOnDocument = DYLD_IOS_VERSION_11_3,
    FirstWhereScrollViewContentInsetsAreNotObscuringInsets = DYLD_IOS_VERSION_12_0,
    FirstWhereUIScrollViewDoesNotApplyKeyboardInsetsUnconditionally = DYLD_IOS_VERSION_12_0,
    FirstWithMainThreadReleaseAssertionInWebPageProxy = DYLD_IOS_VERSION_12_0,
    FirstWithoutMoviStarPlusCORSPreflightQuirk = DYLD_IOS_VERSION_12_0,
    FirstWithTimerThreadSafetyChecks = DYLD_IOS_VERSION_12_0,
    FirstWithLazyGestureRecognizerInstallation = DYLD_IOS_VERSION_12_2,
    FirstWithProcessSwapOnCrossSiteNavigation = DYLD_IOS_VERSION_12_2,
    FirstWithoutUnconditionalUniversalSandboxExtension = DYLD_IOS_VERSION_13_0,
    FirstWithSnapshotAfterScreenUpdates = DYLD_IOS_VERSION_13_0,
    FirstWithDeviceOrientationAndMotionPermissionAPI = DYLD_IOS_VERSION_13_0,
    FirstThatDecidesPolicyBeforeLoadingQuickLookPreview = DYLD_IOS_VERSION_13_0,
    FirstWithExceptionsForRelatedWebViewsUsingDifferentDataStores = DYLD_IOS_VERSION_13_0,
    FirstWithModernCompabilityModeByDefault = DYLD_IOS_VERSION_13_0,
    FirstThatHasUIContextMenuInteraction = DYLD_IOS_VERSION_13_0,
    FirstWhereWKContentViewDoesNotOverrideKeyCommands = DYLD_IOS_VERSION_13_0,
    FirstThatSupportsOverflowHiddenOnMainFrame = DYLD_IOS_VERSION_13_0,
    FirstWithoutIMDbCSSOMViewScrollingQuirk = DYLD_IOS_VERSION_13_0,
    FirstWhereSiteSpecificQuirksAreEnabledByDefault = DYLD_IOS_VERSION_13_2,
    FirstThatRestrictsBaseURLSchemes = DYLD_IOS_VERSION_13_4,
    FirstThatSendsNativeMouseEvents = DYLD_IOS_VERSION_13_4,
    FirstThatMinimizesLanguages = DYLD_IOS_VERSION_13_4,
    FirstWithSessionCleanupByDefault = DYLD_IOS_VERSION_14_0,
    FirstWithInitializeWebKit2MainThreadAssertion = DYLD_IOS_VERSION_14_0,
    FirstWithWKWebsiteDataStoreInitReturningNil = DYLD_IOS_VERSION_14_0,
    FirstWithWebSQLDisabledByDefaultInLegacyWebKit = DYLD_IOS_VERSION_14_0,
    FirstWithoutLaBanquePostaleQuirks = DYLD_IOS_VERSION_14_0,
    FirstWithoutPokerBrosBuiltInTagQuirk = DYLD_IOS_VERSION_14_0,
    FirstVersionWithiOSAppsOnMacOS = DYLD_IOS_VERSION_14_2,
    FirstWithDataURLFragmentRemoval = DYLD_IOS_VERSION_14_5,
    FirstWithHTMLDocumentSupportedPropertyNames = DYLD_IOS_VERSION_14_5,
    FirstThatObservesClassProperty = DYLD_IOS_VERSION_14_5,
    FirstWithoutWeChatScrollingQuirk = DYLD_IOS_VERSION_14_5,
    FirstWithSharedNetworkProcess = DYLD_IOS_VERSION_14_5,
    FirstWithBlankViewOnJSPrompt = DYLD_IOS_VERSION_14_5,
    FirstWithNullOriginForNonSpecialSchemedURLs = DYLD_IOS_VERSION_15_0,
    FirstWithDOMWindowReuseRestriction  = DYLD_IOS_VERSION_15_0,
    FirstWithApplicationCacheDisabledByDefault = DYLD_IOS_VERSION_15_0,
    FirstWithoutExpandoIndexedPropertiesOnWindow = DYLD_IOS_VERSION_15_0,
    FirstThatDoesNotDrainTheMicrotaskQueueWhenCallingObjC = DYLD_IOS_VERSION_15_0,
    FirstWithAuthorizationHeaderOnSameOriginRedirects = DYLD_IOS_VERSION_15_4,
    FirstForbiddingDotPrefixedFonts = DYLD_IOS_VERSION_16_0,
#elif PLATFORM(MAC)
    FirstVersionThatSupportsInitConstructors = 0xA0A00, // OS X 10.10
    FirstThatConvertsInvalidURLsToBlank = DYLD_MACOSX_VERSION_10_12,
    FirstWithExceptionsForDuplicateCompletionHandlerCalls = DYLD_MACOSX_VERSION_10_13,
    FirstWithDropToNavigateDisallowedByDefault = DYLD_MACOSX_VERSION_10_13,
    FirstWithExpiredOnlyReloadBehavior = DYLD_MACOSX_VERSION_10_13,
    FirstWithWebIconDatabaseWarning = DYLD_MACOSX_VERSION_10_13,
    FirstWithMainThreadReleaseAssertionInWebPageProxy = DYLD_MACOSX_VERSION_10_14,
    FirstWithTimerThreadSafetyChecks = DYLD_MACOSX_VERSION_10_14,
    FirstWithoutUnconditionalUniversalSandboxExtension = DYLD_MACOSX_VERSION_10_15,
    FirstWithSnapshotAfterScreenUpdates = DYLD_MACOSX_VERSION_10_15,
    FirstWithExceptionsForRelatedWebViewsUsingDifferentDataStores = DYLD_MACOSX_VERSION_10_15,
    FirstWithDownloadDelegatesCalledOnTheMainThread = DYLD_MACOSX_VERSION_10_15,
    FirstWhereSiteSpecificQuirksAreEnabledByDefault = DYLD_MACOSX_VERSION_10_15_1,
    FirstThatRestrictsBaseURLSchemes = DYLD_MACOSX_VERSION_10_15_4,
    FirstThatMinimizesLanguages = DYLD_MACOSX_VERSION_10_15_4,
    FirstWithSessionCleanupByDefault = DYLD_MACOSX_VERSION_10_16,
    FirstWithInitializeWebKit2MainThreadAssertion = DYLD_MACOSX_VERSION_10_16,
    FirstWithWKWebsiteDataStoreInitReturningNil = DYLD_MACOSX_VERSION_10_16,
    FirstWithDataURLFragmentRemoval = DYLD_MACOSX_VERSION_11_3,
    FirstWithHTMLDocumentSupportedPropertyNames = DYLD_MACOSX_VERSION_11_3,
    FirstWithBlankViewOnJSPrompt = DYLD_MACOSX_VERSION_11_3,
    FirstWithoutClientCertificateLookup = DYLD_MACOSX_VERSION_11_3,
    FirstThatDefaultsToPassiveWheelListenersOnDocument = DYLD_MACOSX_VERSION_11_3,
    FirstThatAllowsWheelEventGesturesToBecomeNonBlocking = DYLD_MACOSX_VERSION_11_3,
    FirstWithNullOriginForNonSpecialSchemedURLs = DYLD_MACOSX_VERSION_12_00,
    FirstWithDOMWindowReuseRestriction = DYLD_MACOSX_VERSION_12_00,
    FirstThatDoesNotDrainTheMicrotaskQueueWhenCallingObjC = DYLD_MACOSX_VERSION_12_00,
    FirstWithApplicationCacheDisabledByDefault = DYLD_MACOSX_VERSION_12_00,
    FirstWithoutExpandoIndexedPropertiesOnWindow = DYLD_MACOSX_VERSION_12_00,
    FirstWithAuthorizationHeaderOnSameOriginRedirects = DYLD_MACOSX_VERSION_12_3,
    FirstForbiddingDotPrefixedFonts = DYLD_MACOSX_VERSION_13_0,
#endif
};

WTF_EXPORT_PRIVATE bool linkedOnOrAfter(SDKVersion);

WTF_EXPORT_PRIVATE void setApplicationSDKVersion(uint32_t);

// Do not use applicationSDKVersion() directly; add a version to the enum above,
// and use linkedOnOrAfter() at call sites.
WTF_EXPORT_PRIVATE uint32_t applicationSDKVersion();

enum class LinkedOnOrAfterOverride : uint8_t {
    BeforeEverything,
    AfterEverything
};
WTF_EXPORT_PRIVATE void setLinkedOnOrAfterOverride(std::optional<LinkedOnOrAfterOverride>);
WTF_EXPORT_PRIVATE std::optional<LinkedOnOrAfterOverride> linkedOnOrAfterOverride();

}

using WTF::setApplicationSDKVersion;
using WTF::applicationSDKVersion;
using WTF::LinkedOnOrAfterOverride;
using WTF::setLinkedOnOrAfterOverride;
using WTF::linkedOnOrAfterOverride;
using WTF::linkedOnOrAfter;
using WTF::SDKVersion;
using WTF::setApplicationSDKVersion;
