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

namespace WebCore {

struct WEBCORE_EXPORT QuirksData {
    std::optional<bool> isAmazon;
    std::optional<bool> isESPN;
    std::optional<bool> isGoogleMaps;
    std::optional<bool> isNetflix;
    std::optional<bool> isSoundCloud;
    std::optional<bool> isVimeo;
    std::optional<bool> isYouTube;
    std::optional<bool> isZoom;

    std::optional<bool> implicitMuteWhenVolumeSetToZero;
    std::optional<bool> needsBingGestureEventQuirk;
    std::optional<bool> needsBodyScrollbarWidthNoneDisabledQuirk;
    std::optional<bool> needsCanPlayAfterSeekedQuirk;
    std::optional<bool> needsChromeMediaControlsPseudoElementQuirk;
    std::optional<bool> needsDisableDOMPasteAccessQuirk;
    std::optional<bool> needsMozillaFileTypeForDataTransferQuirk;
    std::optional<bool> needsRelaxedCorsMixedContentCheckQuirk;
    std::optional<bool> needsVP9FullRangeFlagQuirk;
    std::optional<bool> needsVideoShouldMaintainAspectRatioQuirk;
    std::optional<bool> returnNullPictureInPictureElementDuringFullscreenChangeQuirk;
    std::optional<bool> shouldAllowDownloadsInSpiteOfCSPQuirk;
    std::optional<bool> shouldAutoplayWebAudioForArbitraryUserGestureQuirk;
    std::optional<bool> shouldAvoidResizingWhenInputViewBoundsChangeQuirk;
    std::optional<bool> shouldAvoidScrollingWhenFocusedContentIsVisibleQuirk;
    std::optional<bool> shouldBypassAsyncScriptDeferring;
    std::optional<bool> shouldDisableDataURLPaddingValidation;
    std::optional<bool> shouldDisableElementFullscreen;
    std::optional<bool> shouldDisableFetchMetadata;
    std::optional<bool> shouldDisableLazyIframeLoadingQuirk;
    std::optional<bool> shouldDisableWritingSuggestionsByDefaultQuirk;
    std::optional<bool> shouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk;
    std::optional<bool> shouldEnableFontLoadingAPIQuirk;
    std::optional<bool> shouldExposeShowModalDialog;
    std::optional<bool> shouldIgnorePlaysInlineRequirementQuirk;
    std::optional<bool> shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk;
    std::optional<bool> shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk;
    std::optional<bool> shouldStarBePermissionsPolicyDefaultValueQuirk;
    std::optional<bool> shouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk;

#if PLATFORM(IOS_FAMILY)
    std::optional<bool> mayNeedToIgnoreContentObservation;
    std::optional<bool> needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk;
    std::optional<bool> needsFullscreenDisplayNoneQuirk;
    std::optional<bool> needsFullscreenObjectFitQuirk;
    std::optional<bool> needsGMailOverflowScrollQuirk;
    std::optional<bool> needsIPadSkypeOverflowScrollQuirk;
    std::optional<bool> needsYouTubeMouseOutQuirk;
    std::optional<bool> needsYouTubeOverflowScrollQuirk;
    std::optional<bool> shouldAvoidPastingImagesAsWebContent;
    std::optional<bool> shouldDisablePointerEventsQuirk;
    std::optional<bool> shouldEnableApplicationCacheQuirk;
    std::optional<bool> shouldIgnoreAriaForFastPathContentObservationCheckQuirk;
    std::optional<bool> shouldNavigatorPluginsBeEmpty;
    std::optional<bool> shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk;
    std::optional<bool> shouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk;
#endif

#if PLATFORM(IOS) || PLATFORM(VISION)
    std::optional<bool> allowLayeredFullscreenVideos;
    std::optional<bool> shouldSilenceMediaQueryListChangeEvents;
    std::optional<bool> shouldSilenceWindowResizeEvents;
#endif

#if PLATFORM(VISION)
    std::optional<bool> shouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk;
#endif

#if PLATFORM(MAC)
    std::optional<bool> isNeverRichlyEditableForTouchBarQuirk;
    std::optional<bool> isTouchBarUpdateSuppressedForHiddenContentEditableQuirk;
    std::optional<bool> needsFormControlToBeMouseFocusableQuirk;
    std::optional<bool> needsPrimeVideoUserSelectNoneQuirk;
#endif

#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
    std::optional<bool> needsZeroMaxTouchPointsQuirk;
    std::optional<bool> shouldHideCoarsePointerCharacteristicsQuirk;
#endif

#if ENABLE(FLIP_SCREEN_DIMENSIONS_QUIRKS)
    std::optional<bool> shouldFlipScreenDimensionsQuirk;
#endif

#if ENABLE(MEDIA_STREAM)
    std::optional<bool> shouldDisableImageCaptureQuirk;
    std::optional<bool> shouldEnableLegacyGetUserMediaQuirk;
#endif

#if ENABLE(META_VIEWPORT)
    std::optional<bool> shouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk;
#endif

#if ENABLE(TEXT_AUTOSIZING)
    std::optional<bool> shouldIgnoreTextAutoSizingQuirk;
#endif

#if ENABLE(TOUCH_EVENTS)
    enum class ShouldDispatchSimulatedMouseEvents : uint8_t {
        Unknown,
        No,
        DependingOnTargetFor_mybinder_org,
        Yes,
    };
    ShouldDispatchSimulatedMouseEvents shouldDispatchSimulatedMouseEventsQuirk { ShouldDispatchSimulatedMouseEvents::Unknown };
    std::optional<bool> shouldDispatchPointerOutAfterHandlingSyntheticClick;
    std::optional<bool> shouldPreventDispatchOfTouchEventQuirk;
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    std::optional<bool> requiresUserGestureToLoadInPictureInPictureQuirk;
    std::optional<bool> requiresUserGestureToPauseInPictureInPictureQuirk;
    std::optional<bool> shouldDelayFullscreenEventWhenExitingPictureInPictureQuirk;
#endif
};

} // namespace WebCore
