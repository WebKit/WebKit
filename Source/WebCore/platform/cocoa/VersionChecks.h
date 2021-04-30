/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#pragma once

#import <wtf/spi/darwin/dyldSPI.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/VersionChecksAdditions.h>
#else
#define DYLD_IOS_VERSION_FIRST_WITH_IOS_APPS_ON_MACOS 0
#endif

namespace WebCore {

enum class SDKVersion : uint32_t {
#if PLATFORM(IOS_FAMILY)
    FirstWithNetworkCache = DYLD_IOS_VERSION_9_0,
    FirstWithMediaTypesRequiringUserActionForPlayback = DYLD_IOS_VERSION_10_0,
    FirstWithExceptionsForDuplicateCompletionHandlerCalls = DYLD_IOS_VERSION_11_0,
    FirstWithExpiredOnlyReloadBehavior = DYLD_IOS_VERSION_11_0,
    FirstThatDisallowsSettingAnyXHRHeaderFromFileURLs = DYLD_IOS_VERSION_11_3,
    FirstThatDefaultsToPassiveTouchListenersOnDocument = DYLD_IOS_VERSION_11_3,
    FirstWhereScrollViewContentInsetsAreNotObscuringInsets = DYLD_IOS_VERSION_12_0,
    FirstWhereUIScrollViewDoesNotApplyKeyboardInsetsUnconditionally = DYLD_IOS_VERSION_12_0,
    FirstWithMainThreadReleaseAssertionInWebPageProxy = DYLD_IOS_VERSION_12_0,
    FirstWithoutUnconditionalUniversalSandboxExtension = DYLD_IOS_VERSION_13_0,
    FirstWithLazyGestureRecognizerInstallation = DYLD_IOS_VERSION_12_2,
    FirstWithProcessSwapOnCrossSiteNavigation = DYLD_IOS_VERSION_12_2,
    FirstWithSnapshotAfterScreenUpdates = DYLD_IOS_VERSION_13_0,
    FirstWithDeviceOrientationAndMotionPermissionAPI = DYLD_IOS_VERSION_13_0,
    FirstThatDecidesPolicyBeforeLoadingQuickLookPreview = DYLD_IOS_VERSION_13_0,
    FirstWithExceptionsForRelatedWebViewsUsingDifferentDataStores = DYLD_IOS_VERSION_13_0,
    FirstWithModernCompabilityModeByDefault = DYLD_IOS_VERSION_13_0,
    FirstThatHasUIContextMenuInteraction = DYLD_IOS_VERSION_13_0,
    FirstWhereWKContentViewDoesNotOverrideKeyCommands = DYLD_IOS_VERSION_13_0,
    FirstThatSupportsOverflowHiddenOnMainFrame = DYLD_IOS_VERSION_13_0,
    FirstWhereSiteSpecificQuirksAreEnabledByDefault = DYLD_IOS_VERSION_13_2,
    FirstThatRestrictsBaseURLSchemes = DYLD_IOS_VERSION_13_4,
    FirstWithSessionCleanupByDefault = DYLD_IOS_VERSION_14_0,
    FirstThatSendsNativeMouseEvents = DYLD_IOS_VERSION_13_4,
    FirstWithInitializeWebKit2MainThreadAssertion = DYLD_IOS_VERSION_14_0,
    FirstWithWKWebsiteDataStoreInitReturningNil = DYLD_IOS_VERSION_14_0,
    FirstVersionWithiOSAppsOnMacOS = DYLD_IOS_VERSION_FIRST_WITH_IOS_APPS_ON_MACOS,
    FirstWithDataURLFragmentRemoval = DYLD_IOS_VERSION_14_5,
    FirstWithHTMLDocumentSupportedPropertyNames = DYLD_IOS_VERSION_14_5,
    FirstThatObservesClassProperty = DYLD_IOS_VERSION_14_5,
    FirstWithNullOriginForNonSpecialSchemedURLs = DYLD_IOS_VERSION_15_0,
    FirstWithoutWeChatScrollingQuirk = DYLD_IOS_VERSION_14_5,
    FirstWithDOMWindowReuseRestriction  = DYLD_IOS_VERSION_14_5,
    FirstWithSharedNetworkProcess = DYLD_IOS_VERSION_14_5,
    FirstWithBlankViewOnJSPrompt = DYLD_IOS_VERSION_14_5,
    FirstWithApplicationCacheDisabledByDefault = DYLD_IOS_VERSION_15_0,
#elif PLATFORM(MAC)
    FirstWithNetworkCache = DYLD_MACOSX_VERSION_10_11,
    FirstWithExceptionsForDuplicateCompletionHandlerCalls = DYLD_MACOSX_VERSION_10_13,
    FirstWithDropToNavigateDisallowedByDefault = DYLD_MACOSX_VERSION_10_13,
    FirstWithExpiredOnlyReloadBehavior = DYLD_MACOSX_VERSION_10_13,
    FirstThatDefaultsToPassiveWheelListenersOnDocument = DYLD_MACOSX_VERSION_11_3,
    FirstThatAllowsWheelEventGesturesToBecomeNonBlocking = DYLD_MACOSX_VERSION_11_3,
    FirstWithWebIconDatabaseWarning = DYLD_MACOSX_VERSION_10_13,
    FirstWithMainThreadReleaseAssertionInWebPageProxy = DYLD_MACOSX_VERSION_10_14,
    FirstWithoutUnconditionalUniversalSandboxExtension = DYLD_MACOSX_VERSION_10_15,
    FirstWithSnapshotAfterScreenUpdates = DYLD_MACOSX_VERSION_10_15,
    FirstWithExceptionsForRelatedWebViewsUsingDifferentDataStores = DYLD_MACOSX_VERSION_10_15,
    FirstWhereSiteSpecificQuirksAreEnabledByDefault = DYLD_MACOSX_VERSION_10_15_1,
    FirstThatRestrictsBaseURLSchemes = DYLD_MACOSX_VERSION_10_15_4,
    FirstWithSessionCleanupByDefault = DYLD_MACOSX_VERSION_10_16,
    FirstWithInitializeWebKit2MainThreadAssertion = DYLD_MACOSX_VERSION_10_16,
    FirstWithWKWebsiteDataStoreInitReturningNil = DYLD_MACOSX_VERSION_10_16,
    FirstWithDataURLFragmentRemoval = DYLD_MACOSX_VERSION_11_3,
    FirstWithHTMLDocumentSupportedPropertyNames = DYLD_MACOSX_VERSION_11_3,
    FirstWithNullOriginForNonSpecialSchemedURLs = DYLD_MACOSX_VERSION_12_00,
    FirstWithDOMWindowReuseRestriction = DYLD_MACOSX_VERSION_11_3,
    FirstWithBlankViewOnJSPrompt = DYLD_MACOSX_VERSION_11_3,
    FirstWithApplicationCacheDisabledByDefault = DYLD_MACOSX_VERSION_12_00,
#endif
};

enum class AssumeSafariIsAlwaysLinkedOnAfter : bool { No, Yes };
WEBCORE_EXPORT bool linkedOnOrAfter(SDKVersion, AssumeSafariIsAlwaysLinkedOnAfter = AssumeSafariIsAlwaysLinkedOnAfter::Yes);

}
