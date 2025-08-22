/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>
namespace WebCore {

struct WEBCORE_EXPORT QuirksData {
    bool isAmazon : 1 { false };
    bool isBankOfAmerica : 1 { false };
    bool isBing : 1 { false };
    bool isCBSSports : 1 { false };
    bool isEA : 1 { false };
    bool isESPN : 1 { false };
    bool isFacebook : 1 { false };
    bool isGoogleDocs : 1 { false };
    bool isGoogleProperty : 1 { false };
    bool isGoogleMaps : 1 { false };
    bool isNetflix : 1 { false };
    bool isOutlook : 1 { false };
    bool isSoundCloud : 1 { false };
    bool isThesaurus : 1 { false };
    bool isVimeo : 1 { false };
    bool isWalmart : 1 { false };
    bool isWebEx : 1 { false };
    bool isYouTube : 1 { false };
    bool isZoom : 1 { false };

    bool hasBrokenEncryptedMediaAPISupportQuirk : 1 { false };
    bool implicitMuteWhenVolumeSetToZero : 1 { false };
    bool inputMethodUsesCorrectKeyEventOrder : 1 { false };
    bool maybeBypassBackForwardCache : 1 { false };
    bool needsBingGestureEventQuirk : 1 { false };
    bool needsBodyScrollbarWidthNoneDisabledQuirk : 1 { false };
    bool needsCanPlayAfterSeekedQuirk : 1 { false };
    bool needsChromeMediaControlsPseudoElementQuirk : 1 { false };
    bool needsFacebookRemoveNotSupportedQuirk : 1 { false };
    bool needsHotelsAnimationQuirk : 1 { false };
    bool needsMozillaFileTypeForDataTransferQuirk : 1 { false };
    bool needsResettingTransitionCancelsRunningTransitionQuirk : 1 { false };
    bool needsScrollbarWidthThinDisabledQuirk : 1 { false };
    bool needsSeekingSupportDisabledQuirk : 1 { false };
    bool needsVP9FullRangeFlagQuirk : 1 { false };
    bool needsVideoShouldMaintainAspectRatioQuirk : 1 { false };
    bool returnNullPictureInPictureElementDuringFullscreenChangeQuirk : 1 { false };
    bool shouldAutoplayWebAudioForArbitraryUserGestureQuirk : 1 { false };
    bool shouldAvoidResizingWhenInputViewBoundsChangeQuirk : 1 { false };
    bool shouldAvoidScrollingWhenFocusedContentIsVisibleQuirk : 1 { false };
    bool shouldBypassAsyncScriptDeferring : 1 { false };
    bool shouldDisableDataURLPaddingValidation : 1 { false };
    bool shouldDisableElementFullscreen : 1 { false };
    bool shouldDisableFetchMetadata : 1 { false };
    bool shouldBlockFetchWithNewlineAndLessThan : 1 { false };
    bool shouldDisableLazyIframeLoadingQuirk : 1 { false };
    bool shouldDisablePushStateFilePathRestrictions : 1 { false };
    bool shouldDisableWritingSuggestionsByDefaultQuirk : 1 { false };
    bool shouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk : 1 { false };
    bool shouldDispatchedSimulatedMouseEventsAssumeDefaultPreventedQuirk : 1 { false };
    bool shouldEnableFontLoadingAPIQuirk : 1 { false };
    bool shouldExposeShowModalDialog : 1 { false };
    bool shouldIgnorePlaysInlineRequirementQuirk : 1 { false };
    bool shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk : 1 { false };
    bool shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk : 1 { false };
    bool shouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk : 1 { false };
    bool shouldDispatchPlayPauseEventsOnResume : 1 { false };
    bool shouldUnloadHeavyFrames : 1 { false };
    bool shouldAvoidStartingSelectionOnMouseDownOverPointerCursor : 1 { false };

    // Requires check at moment of use
    std::optional<bool> needsDisableDOMPasteAccessQuirk;

    std::optional<bool> needsReuseLiveRangeForSelectionUpdateQuirk;

#if PLATFORM(IOS_FAMILY)
    bool mayNeedToIgnoreContentObservation : 1 { false };
    bool needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk : 1 { false };
    bool needsFullscreenDisplayNoneQuirk : 1 { false };
    bool needsFullscreenObjectFitQuirk : 1 { false };
    bool needsGMailOverflowScrollQuirk : 1 { false };
    bool needsGoogleMapsScrollingQuirk : 1 { false };
    bool needsGoogleTranslateScrollingQuirk : 1 { false };
    bool needsPreloadAutoQuirk : 1 { false };
    bool needsScriptToEvaluateBeforeRunningScriptFromURLQuirk : 1 { false };
    bool needsYouTubeMouseOutQuirk : 1 { false };
    bool needsYouTubeOverflowScrollQuirk : 1 { false };
    bool shouldAvoidPastingImagesAsWebContent : 1 { false };
    bool shouldDisablePointerEventsQuirk : 1 { false };
    bool shouldIgnoreAriaForFastPathContentObservationCheckQuirk : 1 { false };
    bool shouldNavigatorPluginsBeEmpty : 1 { false };
    bool shouldSilenceWindowResizeEventsDuringApplicationSnapshotting : 1 { false };
    bool shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk : 1 { false };
    bool shouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk : 1 { false };
    bool shouldTreatAddingMouseOutEventListenerAsContentChange : 1 { false };
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(IOS) || PLATFORM(VISION)
    bool allowLayeredFullscreenVideos : 1 { false };
    bool shouldSilenceMediaQueryListChangeEvents : 1 { false };
    bool shouldSilenceResizeObservers : 1 { false };
#endif

#if PLATFORM(VISION)
    bool shouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk : 1 { false };
#endif

#if PLATFORM(MAC)
    bool isNeverRichlyEditableForTouchBarQuirk : 1 { false };
    bool isTouchBarUpdateSuppressedForHiddenContentEditableQuirk : 1 { false };
    bool needsFormControlToBeMouseFocusableQuirk : 1 { false };
    bool needsPrimeVideoUserSelectNoneQuirk : 1 { false };
    bool needsZomatoEmailLoginLabelQuirk : 1 { false };
#endif

#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
    bool needsZeroMaxTouchPointsQuirk : 1 { false };
    bool shouldSupportHoverMediaQueriesQuirk : 1 { false };
#endif

#if PLATFORM(IOS_FAMILY)
    bool shouldHideCoarsePointerCharacteristicsQuirk : 1 { false };
    bool shouldHideSoftTopScrollEdgeEffectDuringFocusQuirk : 1 { false };
#endif

#if ENABLE(FLIP_SCREEN_DIMENSIONS_QUIRKS)
    bool shouldFlipScreenDimensionsQuirk : 1 { false };
#endif

#if ENABLE(MEDIA_STREAM)
    bool shouldDisableImageCaptureQuirk : 1 { false };
    bool shouldEnableLegacyGetUserMediaQuirk : 1 { false };
    bool shouldEnableSpeakerSelectionPermissionsPolicyQuirk : 1 { false };
    bool shouldEnableEnumerateDeviceQuirk : 1 { false };
#endif

#if ENABLE(META_VIEWPORT)
    bool shouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk : 1 { false };
    bool shouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk : 1 { false };
#endif

#if ENABLE(TEXT_AUTOSIZING)
    bool shouldIgnoreTextAutoSizingQuirk : 1 { false };
#endif

#if ENABLE(TOUCH_EVENTS)
    enum class ShouldDispatchSimulatedMouseEvents : uint8_t {
        Unknown,
        No,
        DependingOnTargetFor_mybinder_org,
        Yes,
    };
    ShouldDispatchSimulatedMouseEvents shouldDispatchSimulatedMouseEventsQuirk { ShouldDispatchSimulatedMouseEvents::Unknown };
    bool shouldDispatchPointerOutAfterHandlingSyntheticClick : 1 { false };
    bool shouldPreventDispatchOfTouchEventQuirk : 1 { false };
#endif

#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO_PRESENTATION_MODE)
    bool blocksEnteringStandardFullscreenFromPictureInPictureQuirk : 1 { false };
    bool blocksReturnToFullscreenFromPictureInPictureQuirk : 1 { false };
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    bool requiresUserGestureToLoadInPictureInPictureQuirk : 1 { false };
    bool requiresUserGestureToPauseInPictureInPictureQuirk : 1 { false };
    bool shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk : 1 { false };
#endif

    bool needsNavigatorUserAgentDataQuirk : 1 { false };
    bool needsCustomUserAgentData : 1 { false };
    bool needsNowPlayingFullscreenSwapQuirk : 1 { false };
    bool needsWebKitMediaTextTrackDisplayQuirk : 1 { false };
    bool needsMediaRewriteRangeRequestQuirk : 1 { false };
    bool shouldEnterNativeFullscreenWhenCallingElementRequestFullscreen : 1 { false };
    bool shouldDelayReloadWhenRegisteringServiceWorker : 1 { false };
    bool shouldDisableDOMAudioSession : 1 { false };
};

} // namespace WebCore
