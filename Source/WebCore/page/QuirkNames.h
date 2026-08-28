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
#include <wtf/Platform.h>

namespace WebCore {

enum class QuirkSite : uint8_t {
    Amazon,
    BankOfAmerica,
    Bing,
    CBSSports,
    EA,
    Facebook,
    GoogleDocs,
    GoogleProperty,
    GoogleMaps,
    LinkedIn,
    MyBinder,
    NBA,
    Netflix,
    Outlook,
    Reddit,
    SoundCloud,
    TikTok,
    Vimeo,
    Walmart,

    NumberOfSites
};

enum class SiteSpecificQuirk {
#if PLATFORM(IOS) || PLATFORM(VISION)
    AllowLayeredFullscreenVideos,
#endif
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO_PRESENTATION_MODE)
    BlocksEnteringStandardFullscreenFromPictureInPictureQuirk,
    BlocksReturnToFullscreenFromPictureInPictureQuirk,
#endif
    EnsureCaptionVisibilityInFullscreenAndPictureInPicture,
    HasBrokenEncryptedMediaAPISupportQuirk,
    ImplicitMuteWhenVolumeSetToZero,
    InputMethodUsesCorrectKeyEventOrder,
    InputMethodMustUseCompositionEvents,
#if PLATFORM(MAC)
    IsNeverRichlyEditableForTouchBarQuirk,
    IsTouchBarUpdateSuppressedForHiddenContentEditableQuirk,
#endif
    MaybeBypassBackForwardCache,
#if ENABLE(TWO_PHASE_CLICKS)
    MayNeedToIgnoreContentObservation,
#endif
    NeedsAirIndiaExpressLayeringQuirk,
    NeedsBodyScrollbarWidthNoneDisabledQuirk,
    NeedsCanPlayAfterSeekedQuirk,
    NeedsChromeMediaControlsPseudoElementQuirk,
#if PLATFORM(COCOA)
    NeedsCNNCaptionQuirk,
#endif
#if ENABLE(MEDIA_RECORDER) && ENABLE(COCOA_WEBM_PLAYER)
    NeedsLimitedMatroskaSupportQuirk,
#endif
    NeedsLogoutCookieCleanupQuirk,
#if PLATFORM(IOS_FAMILY)
    NeedsAmazonDesignMenuViewportUnitQuirk,
    NeedsClaudeSidebarViewportUnitQuirk,
    NeedsHideSelectionDuringOverflowScrollQuirk,
#endif
    NeedsCustomUserAgentData,
#if PLATFORM(IOS_FAMILY)
    NeedsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk,
#endif
    NeedsDisableDOMPasteAccessQuirk,
    NeedsFacebookRemoveNotSupportedQuirk,
#if PLATFORM(COCOA)
    NeedsAnchorToBeMouseFocusableQuirk,
    NeedsFormControlToBeMouseFocusableQuirk,
#endif
#if PLATFORM(IOS_FAMILY)
    NeedsFullscreenDisplayNoneQuirk,
    NeedsFullscreenObjectFitQuirk,
    NeedsGMailOverflowScrollQuirk,
    NeedsGoogleMapsScrollingQuirk,
    NeedsGoogleTranslateScrollingQuirk,
#endif
#if PLATFORM(IOS) || PLATFORM(VISION)
    NeedsNetflixVolumeSliderQuirk,
#endif
    NeedsGeforcenowWarningDisplayNoneQuirk,
    NeedsExpediaGroupAnimationQuirk,
    NeedsMediaRewriteRangeRequestQuirk,
    NeedsMozillaFileTypeForDataTransferQuirk,
    NeedsNavigatorUserAgentDataQuirk,
    NeedsNowPlayingFullscreenSwapQuirk,
#if PLATFORM(IOS_FAMILY)
    NeedsSuppressedPauseEventOnFullscreenExitQuirk,
    NeedsPreloadAutoQuirk,
#endif
#if PLATFORM(MAC)
    NeedsPrimeVideoUserSelectNoneQuirk,
#endif
    NeedsResettingTransitionCancelsRunningTransitionQuirk,
    NeedsReuseLiveRangeForSelectionUpdateQuirk,
    NeedsScrollbarWidthThinDisabledQuirk,
    NeedsSeekingSupportDisabledQuirk,
#if ENABLE(MEDIA_SOURCE)
    NeedsSupportsProgressMonitoringQuirk,
#endif
    NeedsSuppressPostLayoutBoundaryEventsQuirk,
    NeedsTikTokOverflowingContentQuirk,
    NeedsVideoShouldMaintainAspectRatioQuirk,
    NeedsWebKitMediaTextTrackDisplayQuirk,
#if PLATFORM(COCOA)
    NeedsYouTubeCaptionQuirk,
#endif
#if PLATFORM(IOS_FAMILY)
    NeedsYouTubeEmbedAutoplayQuirk,
#endif
#if ENABLE(TWO_PHASE_CLICKS)
    NeedsYouTubeMouseOutQuirk,
#endif
#if PLATFORM(IOS_FAMILY)
    NeedsYouTubeOverflowScrollQuirk,
#endif
    NeedsZeroMaxTouchPointsQuirk,
#if PLATFORM(MAC)
    NeedsZomatoEmailLoginLabelQuirk,
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    RequiresUserGestureToLoadInPictureInPictureQuirk,
    RequiresUserGestureToPauseInPictureInPictureQuirk,
#endif
#if ENABLE(FULLSCREEN_API)
    RequiresUserGestureToPlayInFullscreenQuirk,
#endif
    ReturnNullPictureInPictureElementDuringFullscreenChangeQuirk,
#if PLATFORM(IOS_FAMILY)
    ShouldAllowPopupFromMicrosoftOfficeToOneDrive,
#endif
    ShouldAutoplayWebAudioForArbitraryUserGestureQuirk,
    ShouldAvoidProgrammaticScrollClampingQuirk,
    ShouldAvoidResizingWhenInputViewBoundsChangeQuirk,
    ShouldAvoidScrollingWhenFocusedContentIsVisibleQuirk,
    ShouldBlockFetchWithNewlineAndLessThan,
    ShouldBypassAsyncScriptDeferring,
    ShouldComparareUsedValuesForBorderWidthForTriggeringTransitions,
    ShouldDelayReloadWhenRegisteringServiceWorker,
#if HAVE(PIP_SKIP_PREROLL)
    ShouldDisableAdSkippingInPip,
#endif
    ShouldDisableDataURLPaddingValidation,
    ShouldDisableDOMAudioSession,
#if PLATFORM(IOS_FAMILY)
    ShouldDisableElementFullscreenQuirk,
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    ShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk,
#endif
    ShouldDisableFetchMetadata,
#if PLATFORM(VISION)
    ShouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk,
#endif
#if ENABLE(MEDIA_STREAM)
    ShouldDisableImageCaptureQuirk,
    ShouldAllowMediaStreamTrackSerializationQuirk,
#endif
    ShouldDisableLazyIframeLoadingQuirk,
    ShouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk,
#if PLATFORM(IOS_FAMILY)
    ShouldDisablePointerEventsQuirk,
#endif
    ShouldDisablePushStateFilePathRestrictions,
    ShouldDisableScrollAnchoringQuirk,
#if ENABLE(THREADED_ANIMATIONS)
    ShouldDisableThreadedAnimationsQuirk,
#endif
    ShouldDisableWritingSuggestionsByDefaultQuirk,
    ShouldDispatchPlayPauseEventsOnResume,
#if ENABLE(TOUCH_EVENTS)
    ShouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick,
#endif
    ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk,
    ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
    ShouldDispatchSimulatedMouseEventsQuirk,
#endif
#if ENABLE(MEDIA_STREAM)
    ShouldEnableCameraAndMicrophonePermissionStateQuirk,
    ShouldEnableCameraBackgroundPlayback,
    ShouldEnableEnumerateDeviceQuirk,
    ShouldEnableFacebookFlagQuirk,
#endif
    ShouldEnableFontLoadingAPIQuirk,
#if ENABLE(MEDIA_STREAM)
    ShouldEnableLegacyGetUserMediaQuirk,
    ShouldEnableRemoteTrackLabelQuirk,
#endif
#if ENABLE(WEB_RTC)
    ShouldEnableRTCEncodedStreamsQuirk,
#endif
#if ENABLE(MEDIA_STREAM)
    ShouldEnableSpeakerSelectionPermissionsPolicyQuirk,
#endif
    ShouldEnterNativeFullscreenWhenCallingElementRequestFullscreen,
    ShouldExposeShowModalDialog,
#if ENABLE(FLIP_SCREEN_DIMENSIONS_QUIRKS)
    ShouldFlipScreenDimensionsQuirk,
#endif
#if PLATFORM(IOS_FAMILY)
    ShouldHideCoarsePointerCharacteristicsQuirk,
    ShouldHideSoftTopScrollEdgeEffectDuringFocusQuirk,
    ShouldIgnoreAriaForFastPathContentObservationCheckQuirk,
    ShouldIgnoreInputModeNone,
#endif
    ShouldIgnorePlaysInlineRequirementQuirk,
#if ENABLE(TEXT_AUTOSIZING)
    ShouldIgnoreTextAutoSizingQuirk,
#endif
#if ENABLE(META_VIEWPORT)
    ShouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk,
    ShouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk,
    ShouldUseDynamicViewportUnitsAsDefaultQuirk,
#endif
    ShouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk,
#if PLATFORM(IOS_FAMILY)
    ShouldNavigatorPluginsBeEmpty,
#endif
#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
    ShouldPreventDispatchOfTouchEventQuirk,
#endif
    ShouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk,
#if ENABLE(PICTURE_IN_PICTURE_API)
    ShouldReportDocumentAsVisibleIfActivePIPQuirk,
#endif
    ShouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk,
#if PLATFORM(IOS_FAMILY)
    ShouldUseLayoutViewportForClientRectsQuirk,
    ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting,
#endif
#if PLATFORM(IOS) || PLATFORM(VISION)
    ShouldSilenceMediaQueryListChangeEvents,
    ShouldSilenceResizeObservers,
#endif
#if PLATFORM(IOS_FAMILY)
    ShouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk,
#endif
#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
    ShouldSupportHoverMediaQueriesQuirk,
#endif
#if PLATFORM(IOS_FAMILY)
    ShouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk,
#endif
#if ENABLE(CONTENT_CHANGE_OBSERVER)
    ShouldTreatAddingMouseOutEventListenerAsContentChange,
#endif
    ShouldUnloadHeavyFrames,
    ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
    ShouldAllowNotificationPermissionWithoutUserGesture,
    NeedsInstagramResizingReelsQuirk,
    NeedsYahooVolumeSliderQuirk,
#if PLATFORM(IOS_FAMILY)
    NeedsChromeOSNavigatorUserAgentQuirk,
    ShouldSendFakeTouchForceChangeEvent,
#endif
    ShouldLimitHLSPlaybackRate,
    ShouldDeferIntersectionObserversDuringResize,
    ShouldSuppressHLSSubtitles,
    ShouldSuppressMediaSessionPauseActionOnInterruption,
    ShouldBlockAudiblePlaybackWhileAudioIsPlaying,
#if PLATFORM(COCOA)
    NeedsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk,
#endif
    NumberOfQuirks
};

using QuirkBitSet = WTF::BitSet<static_cast<size_t>(SiteSpecificQuirk::NumberOfQuirks)>;
using QuirkSiteBitSet = WTF::BitSet<static_cast<size_t>(QuirkSite::NumberOfSites)>;

} // namespace WebCore
