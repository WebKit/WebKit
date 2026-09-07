/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <wtf/BitSet.h>

namespace WebCore {

enum class QuirkSite : uint8_t {
    Amazon,
    BankOfAmerica,
    BestBuy,
    Bing,
    CBSSports,
    CEAC,
    Dictionary,
    EA,
    Facebook,
    GoogleDocs,
    GoogleProperty,
    GoogleMaps,
    IHeart,
    InVideo,
    LinkedIn,
    NBA,
    Netflix,
    Outlook,
    Reddit,
    SoundCloud,
    Thesaurus,
    TikTok,
    Vimeo,
    Walmart,
    WebEx,

    NumberOfSites
};

enum class SiteSpecificQuirk {
    AllowLayeredFullscreenVideos,
    BlocksEnteringStandardFullscreenFromPictureInPictureQuirk,
    BlocksReturnToFullscreenFromPictureInPictureQuirk,
    EnsureCaptionVisibilityInFullscreenAndPictureInPicture,
    HasBrokenEncryptedMediaAPISupportQuirk,
    ImplicitMuteWhenVolumeSetToZero,
    InputMethodUsesCorrectKeyEventOrder,
    InputMethodMustUseCompositionEvents,
    IsMicrosoftTeamsRedirectURLQuirk,
    IsNeverRichlyEditableForTouchBarQuirk,
    IsTouchBarUpdateSuppressedForHiddenContentEditableQuirk,
    MaybeBypassBackForwardCache,
    MayNeedToIgnoreContentObservation,
    NeedsAirIndiaExpressLayeringQuirk,
    NeedsBodyScrollbarWidthNoneDisabledQuirk,
    NeedsCanPlayAfterSeekedQuirk,
    NeedsChromeMediaControlsPseudoElementQuirk,
    NeedsCNNCaptionQuirk,
    NeedsLimitedMatroskaSupportQuirk,
    NeedsLogoutCookieCleanupQuirk,
    NeedsAmazonDesignMenuViewportUnitQuirk,
    NeedsClaudeSidebarViewportUnitQuirk,
    NeedsHideSelectionDuringOverflowScrollQuirk,
    NeedsCustomUserAgentData,
    NeedsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk,
    NeedsDisableDOMPasteAccessQuirk,
    NeedsFacebookRemoveNotSupportedQuirk,
    NeedsAnchorToBeMouseFocusableQuirk,
    NeedsFormControlToBeMouseFocusableQuirk,
    NeedsFullscreenDisplayNoneQuirk,
    NeedsFullscreenObjectFitQuirk,
    NeedsGMailOverflowScrollQuirk,
    NeedsGoogleMapsScrollingQuirk,
    NeedsGoogleTranslateScrollingQuirk,
    NeedsNetflixVolumeSliderQuirk,
    NeedsGeforcenowWarningDisplayNoneQuirk,
    NeedsExpediaGroupAnimationQuirk,
    NeedsIPadMiniUserAgentQuirk,
    NeedsIPhoneUserAgentQuirk,
    NeedsMediaRewriteRangeRequestQuirk,
    NeedsMozillaFileTypeForDataTransferQuirk,
    NeedsNavigatorUserAgentDataQuirk,
    NeedsNowPlayingFullscreenSwapQuirk,
    NeedsPartitionedCookiesQuirk,
    NeedsSuppressedPauseEventOnFullscreenExitQuirk,
    NeedsPreloadAutoQuirk,
    NeedsResettingTransitionCancelsRunningTransitionQuirk,
    NeedsReuseLiveRangeForSelectionUpdateQuirk,
    NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk,
    NeedsScrollbarWidthThinDisabledQuirk,
    NeedsSeekingSupportDisabledQuirk,
    NeedsSupportsProgressMonitoringQuirk,
    NeedsSuppressPostLayoutBoundaryEventsQuirk,
    NeedsTikTokOverflowingContentQuirk,
    NeedsVideoShouldMaintainAspectRatioQuirk,
    NeedsWebKitMediaTextTrackDisplayQuirk,
    NeedsYouTubeCaptionQuirk,
    NeedsYouTubeEmbedAutoplayQuirk,
    NeedsYouTubeMouseOutQuirk,
    NeedsYouTubeOverflowScrollQuirk,
    NeedsZeroMaxTouchPointsQuirk,
    NeedsZomatoEmailLoginLabelQuirk,
    RequiresUserGestureToLoadInPictureInPictureQuirk,
    RequiresUserGestureToPauseInFullscreenAfterOrientationChangeQuirk,
    RequiresUserGestureToPauseInPictureInPictureQuirk,
    RequiresUserGestureToPlayInFullscreenQuirk,
    ReturnNullPictureInPictureElementDuringFullscreenChangeQuirk,
    ShouldAllowMSTeamsProtocolWithoutUserGestureQuirk,
    ShouldAllowPopupFromMicrosoftOfficeToOneDrive,
    ShouldAutoplayWebAudioForArbitraryUserGestureQuirk,
    ShouldAvoidProgrammaticScrollClampingQuirk,
    ShouldAvoidResizingWhenInputViewBoundsChangeQuirk,
    ShouldAvoidScrollingWhenFocusedContentIsVisibleQuirk,
    ShouldBlockFetchWithNewlineAndLessThan,
    ShouldBypassAsyncScriptDeferring,
    ShouldComparareUsedValuesForBorderWidthForTriggeringTransitions,
    ShouldDelayReloadWhenRegisteringServiceWorker,
    ShouldDisableAdSkippingInPip,
    ShouldDisableDataURLPaddingValidation,
    ShouldDisableDOMAudioSession,
    ShouldDisableElementFullscreenQuirk,
    ShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk,
    ShouldDisableFetchMetadata,
    ShouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk,
    ShouldDisableImageCaptureQuirk,
    ShouldAllowMediaStreamTrackSerializationQuirk,
    ShouldDisableLazyIframeLoadingQuirk,
    ShouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk,
    ShouldDisablePointerEventsQuirk,
    ShouldDisablePushStateFilePathRestrictions,
    ShouldDisableScrollAnchoringQuirk,
    ShouldDisableThreadedAnimationsQuirk,
    ShouldDisableWritingSuggestionsByDefaultQuirk,
    ShouldDispatchPlayPauseEventsOnResume,
    ShouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick,
    ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk,
    ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
    ShouldDispatchSimulatedMouseEventsQuirk,
    ShouldEnableCameraAndMicrophonePermissionStateQuirk,
    ShouldEnableCameraBackgroundPlayback,
    ShouldEnableEnumerateDeviceQuirk,
    ShouldEnableFacebookFlagQuirk,
    ShouldEnableFontLoadingAPIQuirk,
    ShouldEnableLegacyGetUserMediaQuirk,
    ShouldEnableRemoteTrackLabelQuirk,
    ShouldEnableRTCEncodedStreamsQuirk,
    ShouldEnableSpeakerSelectionPermissionsPolicyQuirk,
    ShouldEnterNativeFullscreenWhenCallingElementRequestFullscreen,
    ShouldExposeShowModalDialog,
    ShouldFlipScreenDimensionsQuirk,
    ShouldHideCoarsePointerCharacteristicsQuirk,
    ShouldHideSoftTopScrollEdgeEffectDuringFocusQuirk,
    ShouldIgnoreAriaForFastPathContentObservationCheckQuirk,
    ShouldIgnoreInputModeNone,
    ShouldIgnorePlaysInlineRequirementQuirk,
    ShouldIgnoreTextAutoSizingQuirk,
    ShouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk,
    ShouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk,
    ShouldUseDynamicViewportUnitsAsDefaultQuirk,
    ShouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk,
    ShouldNavigatorPluginsBeEmpty,
    ShouldOmitTouchEventDOMAttributesForDesktopWebsiteQuirk,
    ShouldPreventDispatchOfTouchEventQuirk,
    ShouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk,
    ShouldReportDocumentAsVisibleIfActivePIPQuirk,
    ShouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk,
    ShouldUseLayoutViewportForClientRectsQuirk,
    ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting,
    ShouldSilenceMediaQueryListChangeEvents,
    ShouldSilenceResizeObservers,
    ShouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk,
    NeedsWebExScrollabilityQuirk,
    ShouldSupportHoverMediaQueriesQuirk,
    ShouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk,
    ShouldTranscodeHeicImagesQuirk,
    ShouldTreatAddingMouseOutEventListenerAsContentChange,
    ShouldUnloadHeavyFrames,
    ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
    ShouldAllowNotificationPermissionWithoutUserGesture,
    NeedsInstagramResizingReelsQuirk,
    NeedsYahooVolumeSliderQuirk,
    NeedsChromeOSNavigatorUserAgentQuirk,
    ShouldSendFakeTouchForceChangeEvent,
    ShouldLimitHLSPlaybackRate,
    ShouldDeferIntersectionObserversDuringResize,
    ShouldSuppressHLSSubtitles,
    ShouldSuppressMediaSessionPauseActionOnInterruption,
    ShouldBlockAudiblePlaybackWhileAudioIsPlaying,
    NeedsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk,

    NumberOfQuirks
};

using QuirkBitSet = WTF::BitSet<static_cast<size_t>(SiteSpecificQuirk::NumberOfQuirks)>;
using QuirkSiteBitSet = WTF::BitSet<static_cast<size_t>(QuirkSite::NumberOfSites)>;

} // namespace WebCore
