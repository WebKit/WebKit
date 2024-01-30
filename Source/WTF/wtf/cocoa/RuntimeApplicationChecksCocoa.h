/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
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
#include <wtf/BitSet.h>
#include <wtf/Forward.h>

namespace WTF {

enum class SDKAlignedBehavior {
    AllowsWheelEventGesturesToBecomeNonBlocking,
    AuthorizationHeaderOnSameOriginRedirects,
    BlanksViewOnJSPrompt,
    ContextMenuTriggersLinkActivationNavigationType,
    ConvertsInvalidURLsToBlank,
    DataURLFragmentRemoval,
    DecidesPolicyBeforeLoadingQuickLookPreview,
    DefaultsToExcludingBackgroundsWhenPrinting,
    DefaultsToPassiveTouchListenersOnDocument,
    DefaultsToPassiveWheelListenersOnDocument,
    DisallowsSettingAnyXHRHeaderFromFileURLs,
    DoesNotDrainTheMicrotaskQueueWhenCallingObjC,
    DoesNotParseStringEndingWithFullStopAsFloatingPointNumber,
    DoesNotAddIntrinsicMarginsToFormControls,
    DOMWindowReuseRestriction,
    DownloadDelegatesCalledOnTheMainThread,
    DropToNavigateDisallowedByDefault,
    ExceptionsForDuplicateCompletionHandlerCalls,
    ExceptionsForRelatedWebViewsUsingDifferentDataStores,
    ExpiredOnlyReloadBehavior,
    ForbidsDotPrefixedFonts,
    FullySuspendsBackgroundContent,
    FullySuspendsBackgroundContentImmediately,
    HasUIContextMenuInteraction,
    HTMLDocumentSupportedPropertyNames,
    InitializeWebKit2MainThreadAssertion,
    InspectableDefaultsToDisabled,
    LazyGestureRecognizerInstallation,
    LinkPreviewEnabledByDefault,
    MainThreadReleaseAssertionInWebPageProxy,
    MediaTypesRequiringUserActionForPlayback,
    MinimizesLanguages,
    ModernCompabilityModeByDefault,
    NoClientCertificateLookup,
    NoExpandoIndexedPropertiesOnWindow,
    NoIMDbCSSOMViewScrollingQuirk,
    NoLaBanquePostaleQuirks,
    NoMoviStarPlusCORSPreflightQuirk,
    NoPokerBrosBuiltInTagQuirk,
    NoSearchInputIncrementalAttributeAndSearchEvent,
    NoShowModalDialog,
    NoTheSecretSocietyHiddenMysteryWindowOpenQuirk,
    NoTypedArrayAPIQuirk,
    NoUnconditionalUniversalSandboxExtension,
    NoWeChatScrollingQuirk,
    NoUNIQLOLazyIframeLoadingQuirk,
    NullOriginForNonSpecialSchemedURLs,
    ObservesClassProperty,
    PictureInPictureMediaPlayback,
    ProcessSwapOnCrossSiteNavigation,
    PushStateFilePathRestriction,
    RequiresUserGestureToLoadVideo,
    RestrictsBaseURLSchemes,
    RunningBoardThrottling,
    ScrollViewContentInsetsAreNotObscuringInsets,
    SendsNativeMouseEvents,
    SessionCleanupByDefault,
    SharedNetworkProcess,
    SiteSpecificQuirksAreEnabledByDefault,
    SnapshotAfterScreenUpdates,
    SupportsDeviceOrientationAndMotionPermissionAPI,
    SupportsInitConstructors,
    SupportsiOSAppsOnMacOS,
    SupportsOverflowHiddenOnMainFrame,
    TimerThreadSafetyChecks,
    UIScrollViewDoesNotApplyKeyboardInsetsUnconditionally,
    UnprefixedPlaysInlineAttribute,
    WebIconDatabaseWarning,
    WebSQLDisabledByDefaultInLegacyWebKit,
    WKContentViewDoesNotOverrideKeyCommands,
    WKWebsiteDataStoreInitReturningNil,
    UIBackForwardSkipsHistoryItemsWithoutUserGesture,
    ProgrammaticFocusDuringUserScriptShowsInputViews,
    UsesGameControllerPhysicalInputProfile,
    ScreenOrientationAPIEnabled,
    PopoverAttributeEnabled,
    LiveRangeSelectionEnabledForAllApps,
    DoesNotOverrideUAFromNSUserDefault,
    EvaluateJavaScriptWithoutTransientActivation,
    ResettingTransitionCancelsRunningTransitionQuirk,
    OnlyLoadWellKnownAboutURLs,
    AsyncFragmentNavigationPolicyDecision,
    DoNotLoadStyleSheetIfHTTPStatusIsNotOK,
    ScrollViewSubclassImplementsAddGestureRecognizer,
    ThrowIfCanDeclareGlobalFunctionFails,

    NumberOfBehaviors
};

using SDKAlignedBehaviors = WTF::BitSet<static_cast<size_t>(SDKAlignedBehavior::NumberOfBehaviors), uint32_t>;

WTF_EXPORT_PRIVATE const SDKAlignedBehaviors& sdkAlignedBehaviors();
WTF_EXPORT_PRIVATE void setSDKAlignedBehaviors(SDKAlignedBehaviors);

WTF_EXPORT_PRIVATE void enableAllSDKAlignedBehaviors();
WTF_EXPORT_PRIVATE void disableAllSDKAlignedBehaviors();

WTF_EXPORT_PRIVATE bool linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior);

WTF_EXPORT_PRIVATE bool processIsExtension();
WTF_EXPORT_PRIVATE void setProcessIsExtension(bool);

}

using WTF::disableAllSDKAlignedBehaviors;
using WTF::enableAllSDKAlignedBehaviors;
using WTF::linkedOnOrAfterSDKWithBehavior;
using WTF::processIsExtension;
using WTF::SDKAlignedBehavior;
using WTF::sdkAlignedBehaviors;
using WTF::SDKAlignedBehaviors;
using WTF::setProcessIsExtension;
using WTF::setSDKAlignedBehaviors;
