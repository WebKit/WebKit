/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#include <initializer_list>
#include <wtf/Platform.h>

namespace WebCore {

struct QuirksData {
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
    bool isGoogleAccounts : 1 { false };
    bool isNetflix : 1 { false };
    bool isOutlook : 1 { false };
    bool isSoundCloud : 1 { false };
    bool isThesaurus : 1 { false };
    bool isVimeo : 1 { false };
    bool isWalmart : 1 { false };
    bool isWebEx : 1 { false };
    bool isYouTube : 1 { false };
    bool isZoom : 1 { false };

    enum class SiteSpecificQuirk {
#if PLATFORM(IOS) || PLATFORM(VISION)
        AllowLayeredFullscreenVideos,
#endif
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO_PRESENTATION_MODE)
        BlocksEnteringStandardFullscreenFromPictureInPictureQuirk,
        BlocksReturnToFullscreenFromPictureInPictureQuirk,
#endif
        HasBrokenEncryptedMediaAPISupportQuirk,
        ImplicitMuteWhenVolumeSetToZero,
        InputMethodUsesCorrectKeyEventOrder,
#if PLATFORM(MAC)
        IsNeverRichlyEditableForTouchBarQuirk,
        IsTouchBarUpdateSuppressedForHiddenContentEditableQuirk,
#endif
        MaybeBypassBackForwardCache,
#if PLATFORM(IOS_FAMILY)
        MayNeedToIgnoreContentObservation,
#endif
        NeedsBodyScrollbarWidthNoneDisabledQuirk,
        NeedsCanPlayAfterSeekedQuirk,
        NeedsChromeMediaControlsPseudoElementQuirk,
#if PLATFORM(IOS_FAMILY)
        NeedsClaudeSidebarViewportUnitQuirk,
#endif
        NeedsCustomUserAgentData,
#if PLATFORM(IOS_FAMILY)
        NeedsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk,
#endif
        NeedsFacebookRemoveNotSupportedQuirk,
#if PLATFORM(MAC)
        NeedsFormControlToBeMouseFocusableQuirk,
#endif
#if PLATFORM(IOS_FAMILY)
        NeedsFullscreenDisplayNoneQuirk,
        NeedsFullscreenObjectFitQuirk,
        NeedsGMailOverflowScrollQuirk,
        NeedsGoogleMapsScrollingQuirk,
        NeedsGoogleTranslateScrollingQuirk,
#endif
        NeedsHotelsAnimationQuirk,
        NeedsMediaRewriteRangeRequestQuirk,
        NeedsMozillaFileTypeForDataTransferQuirk,
        NeedsNavigatorUserAgentDataQuirk,
        NeedsNowPlayingFullscreenSwapQuirk,
#if PLATFORM(IOS_FAMILY)
        NeedsPreloadAutoQuirk,
#endif
#if PLATFORM(MAC)
        NeedsPrimeVideoUserSelectNoneQuirk,
#endif
        NeedsResettingTransitionCancelsRunningTransitionQuirk,
        NeedsReuseLiveRangeForSelectionUpdateQuirk,
#if PLATFORM(IOS_FAMILY)
        NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk,
#endif
        NeedsScrollbarWidthThinDisabledQuirk,
        NeedsSeekingSupportDisabledQuirk,
        NeedsSuppressPostLayoutBoundaryEventsQuirk,
        NeedsTikTokOverflowingContentQuirk,
        NeedsVP9FullRangeFlagQuirk,
        NeedsVideoShouldMaintainAspectRatioQuirk,
        NeedsWebKitMediaTextTrackDisplayQuirk,
#if PLATFORM(IOS_FAMILY)
        NeedsYouTubeMouseOutQuirk,
        NeedsYouTubeOverflowScrollQuirk,
#endif
#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
        NeedsZeroMaxTouchPointsQuirk,
#endif
#if PLATFORM(MAC)
        NeedsZomatoEmailLoginLabelQuirk,
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
        RequiresUserGestureToLoadInPictureInPictureQuirk,
        RequiresUserGestureToPauseInPictureInPictureQuirk,
#endif
        ReturnNullPictureInPictureElementDuringFullscreenChangeQuirk,
#if PLATFORM(IOS_FAMILY)
        ShouldAllowPopupFromMicrosoftOfficeToOneDrive,
#endif
        ShouldAutoplayWebAudioForArbitraryUserGestureQuirk,
        ShouldAvoidResizingWhenInputViewBoundsChangeQuirk,
        ShouldAvoidScrollingWhenFocusedContentIsVisibleQuirk,
        ShouldBlockFetchWithNewlineAndLessThan,
        ShouldBypassAsyncScriptDeferring,
        ShouldDelayReloadWhenRegisteringServiceWorker,
#if HAVE(PIP_SKIP_PREROLL)
        ShouldDisableAdSkippingInPip,
#endif
        ShouldDisableDataURLPaddingValidation,
        ShouldDisableDOMAudioSession,
#if ENABLE(VIDEO_PRESENTATION_MODE)
        ShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk,
#endif
        ShouldDisableFetchMetadata,
#if PLATFORM(VISION)
        ShouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk,
#endif
#if ENABLE(MEDIA_STREAM)
        ShouldDisableImageCaptureQuirk,
#endif
        ShouldDisableLazyIframeLoadingQuirk,
#if PLATFORM(IOS_FAMILY)
        ShouldDisablePointerEventsQuirk,
#endif
        ShouldDisablePushStateFilePathRestrictions,
        ShouldDisableWritingSuggestionsByDefaultQuirk,
        ShouldDispatchPlayPauseEventsOnResume,
#if ENABLE(TOUCH_EVENTS)
        ShouldDispatchPointerOutAfterHandlingSyntheticClick,
#endif
        ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk,
        ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
#if ENABLE(MEDIA_STREAM)
        ShouldEnableCameraAndMicrophonePermissionStateQuirk,
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
#endif
        ShouldIgnorePlaysInlineRequirementQuirk,
#if ENABLE(TEXT_AUTOSIZING)
        ShouldIgnoreTextAutoSizingQuirk,
#endif
#if ENABLE(META_VIEWPORT)
        ShouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk,
        ShouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk,
#endif
        ShouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk,
#if PLATFORM(IOS_FAMILY)
        ShouldNavigatorPluginsBeEmpty,
#endif
#if ENABLE(TOUCH_EVENTS)
        ShouldPreventDispatchOfTouchEventQuirk,
#endif
        ShouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk,
        ShouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk,
#if PLATFORM(IOS_FAMILY)
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
        ShouldTreatAddingMouseOutEventListenerAsContentChange,
#endif
        ShouldUnloadHeavyFrames,
        ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor,

        NumberOfQuirks
    };

    WTF::BitSet<static_cast<size_t>(SiteSpecificQuirk::NumberOfQuirks)> activeQuirks;

    inline bool quirkIsEnabled(SiteSpecificQuirk quirk) const
    {
        return activeQuirks.get(static_cast<size_t>(quirk));
    }

    inline void enableQuirks()
    {
        // No-op to support macro expansions
    }

    constexpr void enableQuirks(std::initializer_list<SiteSpecificQuirk> quirks)
    {
        for (auto quirk : quirks)
            activeQuirks.set(static_cast<size_t>(quirk));
    }

    inline void enableQuirk(SiteSpecificQuirk quirk)
    {
        return activeQuirks.set(static_cast<size_t>(quirk));
    }

    inline void setQuirkState(SiteSpecificQuirk quirk, bool state)
    {
        return activeQuirks.set(static_cast<size_t>(quirk), state);
    }

    // Requires check at moment of use
    std::optional<bool> needsDisableDOMPasteAccessQuirk;
    std::optional<bool> shouldDisableElementFullscreen;

#if ENABLE(TOUCH_EVENTS)
    enum class ShouldDispatchSimulatedMouseEvents : uint8_t {
        Unknown,
        No,
        DependingOnTargetFor_mybinder_org,
        Yes,
    };
    ShouldDispatchSimulatedMouseEvents shouldDispatchSimulatedMouseEventsQuirk { ShouldDispatchSimulatedMouseEvents::Unknown };
#endif
};

} // namespace WebCore
