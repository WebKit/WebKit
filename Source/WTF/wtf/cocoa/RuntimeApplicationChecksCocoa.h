/*
 * Copyright (C) 2009-2025 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#include <optional>
#include <wtf/BitSet.h>
#include <wtf/Forward.h>
#include <wtf/RuntimeApplicationChecks.h>

namespace WTF {

enum class SDKAlignedBehavior {
    AllowsWheelEventGesturesToBecomeNonBlocking,
    ApplicationStateTrackerDoesNotObserveWindow,
    AuthorizationHeaderOnSameOriginRedirects,
    BlanksViewOnJSPrompt,
    BlocksConnectionsToAddressWithOnlyZeros,
    BrowsingContextControllerMethodStubRemoved,
    ContextMenuTriggersLinkActivationNavigationType,
    ConvertsInvalidURLsToBlank,
    ConvertsInvalidURLsToNull,
    DataURLFragmentRemoval,
    DecidesPolicyBeforeLoadingQuickLookPreview,
    DefaultsToExcludingBackgroundsWhenPrinting,
    DefaultsToPassiveTouchListenersOnDocument,
    DefaultsToPassiveWheelListenersOnDocument,
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
    JavaScriptEvaluationResultWithoutSerializedScriptValue,
    LazyGestureRecognizerInstallation,
    LinkPreviewEnabledByDefault,
    MainThreadReleaseAssertionInWebPageProxy,
    MediaTypesRequiringUserActionForPlayback,
    MinimizesLanguages,
    ModernCompabilityModeByDefault,
    MutationEventsDisabledByDefault,
    NavigationActionSourceFrameNonNull,
    NoClientCertificateLookup,
    NoExpandoIndexedPropertiesOnWindow,
    NoPokerBrosBuiltInTagQuirk,
    NoShowModalDialog,
    NoTypedArrayAPIQuirk,
    NoUnconditionalUniversalSandboxExtension,
    NoUNIQLOLazyIframeLoadingQuirk,
    NullOriginForNonSpecialSchemedURLs,
    ObservesClassProperty,
    PictureInPictureMediaPlayback,
    PushStateFilePathRestriction,
    RequiresUserGestureToLoadVideo,
    RestrictsBaseURLSchemes,
    RunningBoardThrottling,
    ScrollViewContentInsetsAreNotObscuringInsets,
    SendsNativeMouseEvents,
    SessionCleanupByDefault,
    SiteSpecificQuirksAreEnabledByDefault,
    SnapshotAfterScreenUpdates,
    SupportsDeviceOrientationAndMotionPermissionAPI,
    SupportsInitConstructors,
    TimerThreadSafetyChecks,
    UIScrollViewDoesNotApplyKeyboardInsetsUnconditionally,
    UnprefixedPlaysInlineAttribute,
    WebIconDatabaseWarning,
    WebSQLDisabledByDefaultInLegacyWebKit,
    WKWebsiteDataStoreInitReturningNil,
    UIBackForwardSkipsHistoryItemsWithoutUserGesture,
    ProgrammaticFocusDuringUserScriptShowsInputViews,
    ScreenOrientationAPIEnabled,
    PopoverAttributeEnabled,
    DoesNotOverrideUAFromNSUserDefault,
    EvaluateJavaScriptWithoutTransientActivation,
    ResettingTransitionCancelsRunningTransitionQuirk,
    OnlyLoadWellKnownAboutURLs,
    AsyncFragmentNavigationPolicyDecision,
    DoNotLoadStyleSheetIfHTTPStatusIsNotOK,
    ScrollViewSubclassImplementsAddGestureRecognizer,
    ThrowIfCanDeclareGlobalFunctionFails,
    ThrowOnKVCInstanceVariableAccess,
    LaxCookieSameSiteAttribute,
    UseCFNetworkNetworkLoader,
    AutoLayoutInWKWebView,
    BlockCrossOriginRedirectDownloads,
    BlobFileAccessEnforcementAndNetworkProcessRoundTrip,
    DevolvableWidgets,
    SetSelectionRangeCachesSelectionIfNotFocusedOrSelected,
    DispatchFocusEventBeforeNotifyingClient,
    EnableTrustedTypesByDefault,
    BlobFileAccessEnforcement,
    SupportGameControllerEventInteractionAPI,
    DidFailProvisionalNavigationWithErrorForFileURLNavigation,
    CrashWhenPreconnectingFromBackgroundThread,
    ExecutionTimingChangeOfModuleScripts,
    GetBoundingClientRectZoomed,
    CrashWhenMutatingProcessAssertionsFromBackgroundThread,

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

WTF_EXPORT_PRIVATE void setApplicationBundleIdentifier(const String&);
WTF_EXPORT_PRIVATE void setApplicationBundleIdentifierOverride(const String&);
WTF_EXPORT_PRIVATE String applicationBundleIdentifier();
WTF_EXPORT_PRIVATE void clearApplicationBundleIdentifierTestingOverride();

namespace CocoaApplication {

WTF_EXPORT_PRIVATE bool isAppleApplication();
WTF_EXPORT_PRIVATE bool isAppleBooks();
WTF_EXPORT_PRIVATE bool isDumpRenderTree();
WTF_EXPORT_PRIVATE bool isWebkitTestRunner();
WTF_EXPORT_PRIVATE bool shouldOSFaultLogForAppleApplicationUsingWebKit1();

}

#if PLATFORM(MAC)

namespace MacApplication {

WTF_EXPORT_PRIVATE bool isAdobeInstaller();
WTF_EXPORT_PRIVATE bool isAppleMail();
WTF_EXPORT_PRIVATE bool isMiniBrowser();
WTF_EXPORT_PRIVATE bool isQuickenEssentials();
WTF_EXPORT_PRIVATE bool isSafari();
WTF_EXPORT_PRIVATE bool isSafariTechnologyPreview();
WTF_EXPORT_PRIVATE bool isVersions();
WTF_EXPORT_PRIVATE bool isHRBlock();
WTF_EXPORT_PRIVATE bool isTurboTax();
WTF_EXPORT_PRIVATE bool isEpsonSoftwareUpdater();
WTF_EXPORT_PRIVATE bool isMimeoPhotoProject();

} // MacApplication

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

namespace IOSApplication {

WTF_EXPORT_PRIVATE bool isAmazon();
WTF_EXPORT_PRIVATE bool isAppleWebApp();
WTF_EXPORT_PRIVATE bool isDataActivation();
WTF_EXPORT_PRIVATE bool isEssentialSkeleton();
WTF_EXPORT_PRIVATE bool isFeedly();
WTF_EXPORT_PRIVATE bool isHimalaya();
WTF_EXPORT_PRIVATE bool isHoYoLAB();
WTF_EXPORT_PRIVATE bool isMailCompositionService();
WTF_EXPORT_PRIVATE bool isMiniBrowser();
WTF_EXPORT_PRIVATE bool isMobileMail();
WTF_EXPORT_PRIVATE bool isMobileSafari();
WTF_EXPORT_PRIVATE bool isNews();
WTF_EXPORT_PRIVATE bool isSafariViewService();
WTF_EXPORT_PRIVATE bool isStocks();
WTF_EXPORT_PRIVATE bool isWebBookmarksD();
WTF_EXPORT_PRIVATE bool isWebProcess();
WTF_EXPORT_PRIVATE bool isMobileStore();
WTF_EXPORT_PRIVATE bool isUNIQLOApp();
WTF_EXPORT_PRIVATE bool isDOFUSTouch();
WTF_EXPORT_PRIVATE bool isMyRideK12();

} // IOSApplication

#endif // PLATFORM(IOS_FAMILY)

} // namespace WTF

using WTF::applicationBundleIdentifier;
using WTF::clearApplicationBundleIdentifierTestingOverride;
using WTF::disableAllSDKAlignedBehaviors;
using WTF::enableAllSDKAlignedBehaviors;
using WTF::linkedOnOrAfterSDKWithBehavior;
using WTF::processIsExtension;
using WTF::SDKAlignedBehavior;
using WTF::sdkAlignedBehaviors;
using WTF::SDKAlignedBehaviors;
using WTF::setApplicationBundleIdentifier;
using WTF::setApplicationBundleIdentifierOverride;
using WTF::setProcessIsExtension;
using WTF::setSDKAlignedBehaviors;

#endif // PLATFORM(COCOA)
