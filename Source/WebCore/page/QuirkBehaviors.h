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
    MyBinder,
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

using QuirkSiteBitSet = WTF::BitSet<static_cast<size_t>(QuirkSite::NumberOfSites)>;

namespace BuildCondition {

constexpr bool always = true;

#if PLATFORM(COCOA)
constexpr bool cocoa = true;
#else
constexpr bool cocoa = false;
#endif
#if PLATFORM(MAC)
constexpr bool mac = true;
#else
constexpr bool mac = false;
#endif
#if PLATFORM(IOS)
constexpr bool iOS = true;
#else
constexpr bool iOS = false;
#endif
#if PLATFORM(IOS_FAMILY)
constexpr bool iOSFamily = true;
#else
constexpr bool iOSFamily = false;
#endif
#if PLATFORM(VISION)
constexpr bool vision = true;
#else
constexpr bool vision = false;
#endif
#if ENABLE(CONTENT_CHANGE_OBSERVER)
constexpr bool contentChangeObserver = true;
#else
constexpr bool contentChangeObserver = false;
#endif
#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
constexpr bool desktopContentModeQuirks = true;
#else
constexpr bool desktopContentModeQuirks = false;
#endif
#if ENABLE(FLIP_SCREEN_DIMENSIONS_QUIRKS)
constexpr bool flipScreenDimensionsQuirks = true;
#else
constexpr bool flipScreenDimensionsQuirks = false;
#endif
#if ENABLE(FULLSCREEN_API)
constexpr bool fullscreenAPI = true;
#else
constexpr bool fullscreenAPI = false;
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
constexpr bool videoPresentationMode = true;
#else
constexpr bool videoPresentationMode = false;
#endif
#if ENABLE(MEDIA_RECORDER)
constexpr bool mediaRecorder = true;
#else
constexpr bool mediaRecorder = false;
#endif
#if ENABLE(COCOA_WEBM_PLAYER)
constexpr bool cocoaWebMPlayer = true;
#else
constexpr bool cocoaWebMPlayer = false;
#endif
#if ENABLE(MEDIA_SOURCE)
constexpr bool mediaSource = true;
#else
constexpr bool mediaSource = false;
#endif
#if ENABLE(MEDIA_STREAM)
constexpr bool mediaStream = true;
#else
constexpr bool mediaStream = false;
#endif
#if ENABLE(META_VIEWPORT)
constexpr bool metaViewport = true;
#else
constexpr bool metaViewport = false;
#endif
#if HAVE(PIP_SKIP_PREROLL)
constexpr bool pipSkipPreroll = true;
#else
constexpr bool pipSkipPreroll = false;
#endif
#if ENABLE(PICTURE_IN_PICTURE_API)
constexpr bool pictureInPictureAPI = true;
#else
constexpr bool pictureInPictureAPI = false;
#endif
#if ENABLE(TEXT_AUTOSIZING)
constexpr bool textAutosizing = true;
#else
constexpr bool textAutosizing = false;
#endif
#if ENABLE(THREADED_ANIMATIONS)
constexpr bool threadedAnimations = true;
#else
constexpr bool threadedAnimations = false;
#endif
#if ENABLE(TOUCH_EVENTS)
constexpr bool touchEvents = true;
#else
constexpr bool touchEvents = false;
#endif
#if ENABLE(TOUCH_EVENT_REGIONS)
constexpr bool touchEventRegions = true;
#else
constexpr bool touchEventRegions = false;
#endif
#if ENABLE(TWO_PHASE_CLICKS)
constexpr bool twoPhaseClicks = true;
#else
constexpr bool twoPhaseClicks = false;
#endif
#if ENABLE(WEB_RTC)
constexpr bool webRTC = true;
#else
constexpr bool webRTC = false;
#endif

} // namespace BuildCondition

enum class QuirkID {
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

    NumberOfIDs
};

using QuirkBitSet = WTF::BitSet<static_cast<size_t>(QuirkID::NumberOfIDs)>;

struct QuirkBehavior {
    QuirkID id;
    bool isAvailable { false };
};

// One QuirkBehavior per QuirkID, for use in the quirk table.
namespace Behaviors {

inline constexpr QuirkBehavior allowLayeredFullscreenVideos { QuirkID::AllowLayeredFullscreenVideos, BuildCondition::iOS || BuildCondition::vision };
inline constexpr QuirkBehavior blocksEnteringStandardFullscreenFromPictureInPictureQuirk { QuirkID::BlocksEnteringStandardFullscreenFromPictureInPictureQuirk, BuildCondition::fullscreenAPI && BuildCondition::videoPresentationMode };
inline constexpr QuirkBehavior blocksReturnToFullscreenFromPictureInPictureQuirk { QuirkID::BlocksReturnToFullscreenFromPictureInPictureQuirk, BuildCondition::fullscreenAPI && BuildCondition::videoPresentationMode };
inline constexpr QuirkBehavior ensureCaptionVisibilityInFullscreenAndPictureInPicture { QuirkID::EnsureCaptionVisibilityInFullscreenAndPictureInPicture, BuildCondition::always };
inline constexpr QuirkBehavior hasBrokenEncryptedMediaAPISupportQuirk { QuirkID::HasBrokenEncryptedMediaAPISupportQuirk, BuildCondition::always };
inline constexpr QuirkBehavior implicitMuteWhenVolumeSetToZero { QuirkID::ImplicitMuteWhenVolumeSetToZero, BuildCondition::always };
inline constexpr QuirkBehavior inputMethodUsesCorrectKeyEventOrder { QuirkID::InputMethodUsesCorrectKeyEventOrder, BuildCondition::always };
inline constexpr QuirkBehavior inputMethodMustUseCompositionEvents { QuirkID::InputMethodMustUseCompositionEvents, BuildCondition::mac };
inline constexpr QuirkBehavior isMicrosoftTeamsRedirectURLQuirk { QuirkID::IsMicrosoftTeamsRedirectURLQuirk, BuildCondition::always };
inline constexpr QuirkBehavior isNeverRichlyEditableForTouchBarQuirk { QuirkID::IsNeverRichlyEditableForTouchBarQuirk, BuildCondition::mac };
inline constexpr QuirkBehavior isTouchBarUpdateSuppressedForHiddenContentEditableQuirk { QuirkID::IsTouchBarUpdateSuppressedForHiddenContentEditableQuirk, BuildCondition::mac };
inline constexpr QuirkBehavior maybeBypassBackForwardCache { QuirkID::MaybeBypassBackForwardCache, BuildCondition::always };
inline constexpr QuirkBehavior mayNeedToIgnoreContentObservation { QuirkID::MayNeedToIgnoreContentObservation, BuildCondition::twoPhaseClicks };
inline constexpr QuirkBehavior needsAirIndiaExpressLayeringQuirk { QuirkID::NeedsAirIndiaExpressLayeringQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsBodyScrollbarWidthNoneDisabledQuirk { QuirkID::NeedsBodyScrollbarWidthNoneDisabledQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsCanPlayAfterSeekedQuirk { QuirkID::NeedsCanPlayAfterSeekedQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsChromeMediaControlsPseudoElementQuirk { QuirkID::NeedsChromeMediaControlsPseudoElementQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsCNNCaptionQuirk { QuirkID::NeedsCNNCaptionQuirk, BuildCondition::cocoa };
inline constexpr QuirkBehavior needsLimitedMatroskaSupportQuirk { QuirkID::NeedsLimitedMatroskaSupportQuirk, BuildCondition::mediaRecorder && BuildCondition::cocoaWebMPlayer };
inline constexpr QuirkBehavior needsLogoutCookieCleanupQuirk { QuirkID::NeedsLogoutCookieCleanupQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsAmazonDesignMenuViewportUnitQuirk { QuirkID::NeedsAmazonDesignMenuViewportUnitQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsClaudeSidebarViewportUnitQuirk { QuirkID::NeedsClaudeSidebarViewportUnitQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsHideSelectionDuringOverflowScrollQuirk { QuirkID::NeedsHideSelectionDuringOverflowScrollQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsCustomUserAgentData { QuirkID::NeedsCustomUserAgentData, BuildCondition::always };
inline constexpr QuirkBehavior needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk { QuirkID::NeedsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsDisableDOMPasteAccessQuirk { QuirkID::NeedsDisableDOMPasteAccessQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsFacebookRemoveNotSupportedQuirk { QuirkID::NeedsFacebookRemoveNotSupportedQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsAnchorToBeMouseFocusableQuirk { QuirkID::NeedsAnchorToBeMouseFocusableQuirk, BuildCondition::cocoa };
inline constexpr QuirkBehavior needsFormControlToBeMouseFocusableQuirk { QuirkID::NeedsFormControlToBeMouseFocusableQuirk, BuildCondition::mac };
inline constexpr QuirkBehavior needsFullscreenDisplayNoneQuirk { QuirkID::NeedsFullscreenDisplayNoneQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsFullscreenObjectFitQuirk { QuirkID::NeedsFullscreenObjectFitQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsGMailOverflowScrollQuirk { QuirkID::NeedsGMailOverflowScrollQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsGoogleMapsScrollingQuirk { QuirkID::NeedsGoogleMapsScrollingQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsGoogleTranslateScrollingQuirk { QuirkID::NeedsGoogleTranslateScrollingQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsNetflixVolumeSliderQuirk { QuirkID::NeedsNetflixVolumeSliderQuirk, BuildCondition::iOS || BuildCondition::vision };
inline constexpr QuirkBehavior needsGeforcenowWarningDisplayNoneQuirk { QuirkID::NeedsGeforcenowWarningDisplayNoneQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsExpediaGroupAnimationQuirk { QuirkID::NeedsExpediaGroupAnimationQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsIPadMiniUserAgentQuirk { QuirkID::NeedsIPadMiniUserAgentQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsIPhoneUserAgentQuirk { QuirkID::NeedsIPhoneUserAgentQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsMediaRewriteRangeRequestQuirk { QuirkID::NeedsMediaRewriteRangeRequestQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsMozillaFileTypeForDataTransferQuirk { QuirkID::NeedsMozillaFileTypeForDataTransferQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsNavigatorUserAgentDataQuirk { QuirkID::NeedsNavigatorUserAgentDataQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsNowPlayingFullscreenSwapQuirk { QuirkID::NeedsNowPlayingFullscreenSwapQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsPartitionedCookiesQuirk { QuirkID::NeedsPartitionedCookiesQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsSuppressedPauseEventOnFullscreenExitQuirk { QuirkID::NeedsSuppressedPauseEventOnFullscreenExitQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsPreloadAutoQuirk { QuirkID::NeedsPreloadAutoQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsResettingTransitionCancelsRunningTransitionQuirk { QuirkID::NeedsResettingTransitionCancelsRunningTransitionQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsReuseLiveRangeForSelectionUpdateQuirk { QuirkID::NeedsReuseLiveRangeForSelectionUpdateQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsScriptToEvaluateBeforeRunningScriptFromURLQuirk { QuirkID::NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsScrollbarWidthThinDisabledQuirk { QuirkID::NeedsScrollbarWidthThinDisabledQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsSeekingSupportDisabledQuirk { QuirkID::NeedsSeekingSupportDisabledQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsSupportsProgressMonitoringQuirk { QuirkID::NeedsSupportsProgressMonitoringQuirk, BuildCondition::mediaSource };
inline constexpr QuirkBehavior needsSuppressPostLayoutBoundaryEventsQuirk { QuirkID::NeedsSuppressPostLayoutBoundaryEventsQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsTikTokOverflowingContentQuirk { QuirkID::NeedsTikTokOverflowingContentQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsVideoShouldMaintainAspectRatioQuirk { QuirkID::NeedsVideoShouldMaintainAspectRatioQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsWebKitMediaTextTrackDisplayQuirk { QuirkID::NeedsWebKitMediaTextTrackDisplayQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsYouTubeCaptionQuirk { QuirkID::NeedsYouTubeCaptionQuirk, BuildCondition::cocoa };
inline constexpr QuirkBehavior needsYouTubeEmbedAutoplayQuirk { QuirkID::NeedsYouTubeEmbedAutoplayQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsYouTubeMouseOutQuirk { QuirkID::NeedsYouTubeMouseOutQuirk, BuildCondition::twoPhaseClicks };
inline constexpr QuirkBehavior needsYouTubeOverflowScrollQuirk { QuirkID::NeedsYouTubeOverflowScrollQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsZeroMaxTouchPointsQuirk { QuirkID::NeedsZeroMaxTouchPointsQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsZomatoEmailLoginLabelQuirk { QuirkID::NeedsZomatoEmailLoginLabelQuirk, BuildCondition::mac };
inline constexpr QuirkBehavior requiresUserGestureToLoadInPictureInPictureQuirk { QuirkID::RequiresUserGestureToLoadInPictureInPictureQuirk, BuildCondition::videoPresentationMode };
inline constexpr QuirkBehavior requiresUserGestureToPauseInFullscreenAfterOrientationChangeQuirk { QuirkID::RequiresUserGestureToPauseInFullscreenAfterOrientationChangeQuirk, BuildCondition::videoPresentationMode && BuildCondition::iOS };
inline constexpr QuirkBehavior requiresUserGestureToPauseInPictureInPictureQuirk { QuirkID::RequiresUserGestureToPauseInPictureInPictureQuirk, BuildCondition::videoPresentationMode };
inline constexpr QuirkBehavior requiresUserGestureToPlayInFullscreenQuirk { QuirkID::RequiresUserGestureToPlayInFullscreenQuirk, BuildCondition::fullscreenAPI };
inline constexpr QuirkBehavior returnNullPictureInPictureElementDuringFullscreenChangeQuirk { QuirkID::ReturnNullPictureInPictureElementDuringFullscreenChangeQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldAllowMSTeamsProtocolWithoutUserGestureQuirk { QuirkID::ShouldAllowMSTeamsProtocolWithoutUserGestureQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldAllowPopupFromMicrosoftOfficeToOneDrive { QuirkID::ShouldAllowPopupFromMicrosoftOfficeToOneDrive, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldAutoplayWebAudioForArbitraryUserGestureQuirk { QuirkID::ShouldAutoplayWebAudioForArbitraryUserGestureQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldAvoidProgrammaticScrollClampingQuirk { QuirkID::ShouldAvoidProgrammaticScrollClampingQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldAvoidResizingWhenInputViewBoundsChangeQuirk { QuirkID::ShouldAvoidResizingWhenInputViewBoundsChangeQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldAvoidScrollingWhenFocusedContentIsVisibleQuirk { QuirkID::ShouldAvoidScrollingWhenFocusedContentIsVisibleQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldBlockFetchWithNewlineAndLessThan { QuirkID::ShouldBlockFetchWithNewlineAndLessThan, BuildCondition::always };
inline constexpr QuirkBehavior shouldBypassAsyncScriptDeferring { QuirkID::ShouldBypassAsyncScriptDeferring, BuildCondition::always };
inline constexpr QuirkBehavior shouldComparareUsedValuesForBorderWidthForTriggeringTransitions { QuirkID::ShouldComparareUsedValuesForBorderWidthForTriggeringTransitions, BuildCondition::always };
inline constexpr QuirkBehavior shouldDelayReloadWhenRegisteringServiceWorker { QuirkID::ShouldDelayReloadWhenRegisteringServiceWorker, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableAdSkippingInPip { QuirkID::ShouldDisableAdSkippingInPip, BuildCondition::pipSkipPreroll };
inline constexpr QuirkBehavior shouldDisableDataURLPaddingValidation { QuirkID::ShouldDisableDataURLPaddingValidation, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableDOMAudioSession { QuirkID::ShouldDisableDOMAudioSession, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableElementFullscreenQuirk { QuirkID::ShouldDisableElementFullscreenQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk { QuirkID::ShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk, BuildCondition::videoPresentationMode };
inline constexpr QuirkBehavior shouldDisableFetchMetadata { QuirkID::ShouldDisableFetchMetadata, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk { QuirkID::ShouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk, BuildCondition::vision };
inline constexpr QuirkBehavior shouldDisableImageCaptureQuirk { QuirkID::ShouldDisableImageCaptureQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldAllowMediaStreamTrackSerializationQuirk { QuirkID::ShouldAllowMediaStreamTrackSerializationQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldDisableLazyIframeLoadingQuirk { QuirkID::ShouldDisableLazyIframeLoadingQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk { QuirkID::ShouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisablePointerEventsQuirk { QuirkID::ShouldDisablePointerEventsQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldDisablePushStateFilePathRestrictions { QuirkID::ShouldDisablePushStateFilePathRestrictions, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableScrollAnchoringQuirk { QuirkID::ShouldDisableScrollAnchoringQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableThreadedAnimationsQuirk { QuirkID::ShouldDisableThreadedAnimationsQuirk, BuildCondition::threadedAnimations };
inline constexpr QuirkBehavior shouldDisableWritingSuggestionsByDefaultQuirk { QuirkID::ShouldDisableWritingSuggestionsByDefaultQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDispatchPlayPauseEventsOnResume { QuirkID::ShouldDispatchPlayPauseEventsOnResume, BuildCondition::always };
inline constexpr QuirkBehavior shouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick { QuirkID::ShouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick, BuildCondition::touchEvents };
inline constexpr QuirkBehavior shouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk { QuirkID::ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk { QuirkID::ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDispatchSimulatedMouseEventsQuirk { QuirkID::ShouldDispatchSimulatedMouseEventsQuirk, BuildCondition::touchEvents || BuildCondition::touchEventRegions };
inline constexpr QuirkBehavior shouldEnableCameraAndMicrophonePermissionStateQuirk { QuirkID::ShouldEnableCameraAndMicrophonePermissionStateQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableCameraBackgroundPlayback { QuirkID::ShouldEnableCameraBackgroundPlayback, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableEnumerateDeviceQuirk { QuirkID::ShouldEnableEnumerateDeviceQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableFacebookFlagQuirk { QuirkID::ShouldEnableFacebookFlagQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableFontLoadingAPIQuirk { QuirkID::ShouldEnableFontLoadingAPIQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldEnableLegacyGetUserMediaQuirk { QuirkID::ShouldEnableLegacyGetUserMediaQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableRemoteTrackLabelQuirk { QuirkID::ShouldEnableRemoteTrackLabelQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableRTCEncodedStreamsQuirk { QuirkID::ShouldEnableRTCEncodedStreamsQuirk, BuildCondition::webRTC };
inline constexpr QuirkBehavior shouldEnableSpeakerSelectionPermissionsPolicyQuirk { QuirkID::ShouldEnableSpeakerSelectionPermissionsPolicyQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnterNativeFullscreenWhenCallingElementRequestFullscreen { QuirkID::ShouldEnterNativeFullscreenWhenCallingElementRequestFullscreen, BuildCondition::always };
inline constexpr QuirkBehavior shouldExposeShowModalDialog { QuirkID::ShouldExposeShowModalDialog, BuildCondition::always };
inline constexpr QuirkBehavior shouldFlipScreenDimensionsQuirk { QuirkID::ShouldFlipScreenDimensionsQuirk, BuildCondition::flipScreenDimensionsQuirks };
inline constexpr QuirkBehavior shouldHideCoarsePointerCharacteristicsQuirk { QuirkID::ShouldHideCoarsePointerCharacteristicsQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldHideSoftTopScrollEdgeEffectDuringFocusQuirk { QuirkID::ShouldHideSoftTopScrollEdgeEffectDuringFocusQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldIgnoreAriaForFastPathContentObservationCheckQuirk { QuirkID::ShouldIgnoreAriaForFastPathContentObservationCheckQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldIgnoreInputModeNone { QuirkID::ShouldIgnoreInputModeNone, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldIgnorePlaysInlineRequirementQuirk { QuirkID::ShouldIgnorePlaysInlineRequirementQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldIgnoreTextAutoSizingQuirk { QuirkID::ShouldIgnoreTextAutoSizingQuirk, BuildCondition::textAutosizing };
inline constexpr QuirkBehavior shouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk { QuirkID::ShouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk, BuildCondition::metaViewport };
inline constexpr QuirkBehavior shouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk { QuirkID::ShouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk, BuildCondition::metaViewport };
inline constexpr QuirkBehavior shouldUseDynamicViewportUnitsAsDefaultQuirk { QuirkID::ShouldUseDynamicViewportUnitsAsDefaultQuirk, BuildCondition::metaViewport };
inline constexpr QuirkBehavior shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk { QuirkID::ShouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldNavigatorPluginsBeEmpty { QuirkID::ShouldNavigatorPluginsBeEmpty, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldOmitTouchEventDOMAttributesForDesktopWebsiteQuirk { QuirkID::ShouldOmitTouchEventDOMAttributesForDesktopWebsiteQuirk, BuildCondition::touchEvents };
inline constexpr QuirkBehavior shouldPreventDispatchOfTouchEventQuirk { QuirkID::ShouldPreventDispatchOfTouchEventQuirk, BuildCondition::touchEvents || BuildCondition::touchEventRegions };
inline constexpr QuirkBehavior shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk { QuirkID::ShouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldReportDocumentAsVisibleIfActivePIPQuirk { QuirkID::ShouldReportDocumentAsVisibleIfActivePIPQuirk, BuildCondition::pictureInPictureAPI };
inline constexpr QuirkBehavior shouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk { QuirkID::ShouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldUseLayoutViewportForClientRectsQuirk { QuirkID::ShouldUseLayoutViewportForClientRectsQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldSilenceWindowResizeEventsDuringApplicationSnapshotting { QuirkID::ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting, BuildCondition::iOS || BuildCondition::vision };
inline constexpr QuirkBehavior shouldSilenceMediaQueryListChangeEvents { QuirkID::ShouldSilenceMediaQueryListChangeEvents, BuildCondition::iOS || BuildCondition::vision };
inline constexpr QuirkBehavior shouldSilenceResizeObservers { QuirkID::ShouldSilenceResizeObservers, BuildCondition::iOS || BuildCondition::vision };
inline constexpr QuirkBehavior shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk { QuirkID::ShouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsWebExScrollabilityQuirk { QuirkID::NeedsWebExScrollabilityQuirk, BuildCondition::desktopContentModeQuirks };
inline constexpr QuirkBehavior shouldSupportHoverMediaQueriesQuirk { QuirkID::ShouldSupportHoverMediaQueriesQuirk, BuildCondition::desktopContentModeQuirks };
inline constexpr QuirkBehavior shouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk { QuirkID::ShouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldTranscodeHeicImagesQuirk { QuirkID::ShouldTranscodeHeicImagesQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldTreatAddingMouseOutEventListenerAsContentChange { QuirkID::ShouldTreatAddingMouseOutEventListenerAsContentChange, BuildCondition::contentChangeObserver };
inline constexpr QuirkBehavior shouldUnloadHeavyFrames { QuirkID::ShouldUnloadHeavyFrames, BuildCondition::always };
inline constexpr QuirkBehavior shouldAvoidStartingSelectionOnMouseDownOverPointerCursor { QuirkID::ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor, BuildCondition::always };
inline constexpr QuirkBehavior shouldAllowNotificationPermissionWithoutUserGesture { QuirkID::ShouldAllowNotificationPermissionWithoutUserGesture, BuildCondition::always };
inline constexpr QuirkBehavior needsInstagramResizingReelsQuirk { QuirkID::NeedsInstagramResizingReelsQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsYahooVolumeSliderQuirk { QuirkID::NeedsYahooVolumeSliderQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsChromeOSNavigatorUserAgentQuirk { QuirkID::NeedsChromeOSNavigatorUserAgentQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldSendFakeTouchForceChangeEvent { QuirkID::ShouldSendFakeTouchForceChangeEvent, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldLimitHLSPlaybackRate { QuirkID::ShouldLimitHLSPlaybackRate, BuildCondition::always };
inline constexpr QuirkBehavior shouldDeferIntersectionObserversDuringResize { QuirkID::ShouldDeferIntersectionObserversDuringResize, BuildCondition::always };
inline constexpr QuirkBehavior shouldSuppressHLSSubtitles { QuirkID::ShouldSuppressHLSSubtitles, BuildCondition::always };
inline constexpr QuirkBehavior shouldSuppressMediaSessionPauseActionOnInterruption { QuirkID::ShouldSuppressMediaSessionPauseActionOnInterruption, BuildCondition::always };
inline constexpr QuirkBehavior shouldBlockAudiblePlaybackWhileAudioIsPlaying { QuirkID::ShouldBlockAudiblePlaybackWhileAudioIsPlaying, BuildCondition::always };
inline constexpr QuirkBehavior needsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk { QuirkID::NeedsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk, BuildCondition::cocoa };

} // namespace Behaviors

} // namespace WebCore
