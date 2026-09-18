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

#include <WebCore/URLMatch.h>
#include <wtf/BitSet.h>
#include <wtf/OptionSet.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

enum class QuirkSite : uint8_t {
    Amazon,
    BankOfAmerica,
    Bing,
    CBSSports,
    Facebook,
    GoogleDocs,
    GoogleProperty,
    GoogleMaps,
    GoogleSearch,
    LinkedIn,
    Outlook,
    Reddit,
    SoundCloud,
    TikTok,
    Vimeo,
    Walmart,

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

enum class QuirkBehaviorID {
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
    NeedsChromeCompatibilityUserAgentQuirk,
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
    NeedsPerDocumentAutoplayBehaviorQuirk,
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
    NeedsUserAgentStringOverrideQuirk,
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
    ShouldComputeSimulatedMouseEventMovementDeltaQuirk,
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
    ShouldPreventKeyframeEffectAccelerationQuirk,
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

using QuirkBitSet = WTF::BitSet<static_cast<size_t>(QuirkBehaviorID::NumberOfIDs)>;

struct QuirkParameters {
    ASCIILiteral script = ""_s;
    std::optional<URLMatch> scriptURLCondition { std::nullopt };
    ASCIILiteral userAgent = ""_s;
    ASCIILiteral chromeCompatibilityVersion = ""_s;

    static consteval QuirkParameters fromScript(ASCIILiteral script)
    {
        return QuirkParameters {
            .script = script
        };
    }

    static consteval QuirkParameters fromScript(ASCIILiteral script, const URLMatch& scriptURLCondition)
    {
        return QuirkParameters {
            .script = script,
            .scriptURLCondition = scriptURLCondition
        };
    }

    static consteval QuirkParameters fromUserAgent(ASCIILiteral userAgent)
    {
        return QuirkParameters {
            .userAgent = userAgent
        };
    }

    static consteval QuirkParameters fromChromeCompatibilityVersion(ASCIILiteral chromeCompatibilityVersion)
    {
        return QuirkParameters {
            .chromeCompatibilityVersion = chromeCompatibilityVersion
        };
    }
};

enum class QuirkParametersNeeded : uint8_t {
    NeedsScript = 1 << 0,
    NeedsUserAgent = 1 << 1,
    NeedsChromeCompatibilityVersion = 1 << 2,
};

enum class QuirkConditionsSupported : uint8_t {
    ElementSelector = 1 << 0,
};

namespace QuirkBehaviorConditions {
struct ElementMatchesSelector {
    ASCIILiteral selector;
};

constexpr ElementMatchesSelector elementMatchesSelector(ASCIILiteral selector)
{
    return ElementMatchesSelector { selector };
}

} // namespace QuirkBehaviorConditions

struct QuirkBehavior {
    QuirkBehaviorID id;
    bool isAvailable { false };
    OptionSet<QuirkParametersNeeded> quirkParametersNeeded { };
    OptionSet<QuirkConditionsSupported> quirkConditionsSupported { };
    std::optional<ASCIILiteral> elementSelectorCondition { std::nullopt };
    std::optional<QuirkParameters> parameters { std::nullopt };

    consteval QuirkBehavior operator()(QuirkParameters params) const
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!quirkParametersNeeded.isEmpty());
        auto copy = *this;
        copy.parameters = params;
        return copy;
    }

    template<typename... Conditions> consteval QuirkBehavior when(Conditions... conditions) const
    {
        static_assert(sizeof...(conditions), "when() must name at least one condition");
        auto copy = *this;
        (applyCondition(copy, conditions), ...);
        return copy;
    }

    consteval void applyCondition(QuirkBehavior& behavior, QuirkBehaviorConditions::ElementMatchesSelector elementMatchesSelector) const
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(behavior.quirkConditionsSupported.contains(QuirkConditionsSupported::ElementSelector));
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!behavior.elementSelectorCondition);
        behavior.elementSelectorCondition = elementMatchesSelector.selector;
    }
};

// One QuirkBehavior per QuirkBehaviorID, for use in the quirk table.
namespace QuirkBehaviors {
inline constexpr QuirkBehavior allowLayeredFullscreenVideos { WebCore::QuirkBehaviorID::AllowLayeredFullscreenVideos, BuildCondition::iOS || BuildCondition::vision };
inline constexpr QuirkBehavior blocksEnteringStandardFullscreenFromPictureInPictureQuirk { WebCore::QuirkBehaviorID::BlocksEnteringStandardFullscreenFromPictureInPictureQuirk, BuildCondition::fullscreenAPI && BuildCondition::videoPresentationMode };
inline constexpr QuirkBehavior blocksReturnToFullscreenFromPictureInPictureQuirk { WebCore::QuirkBehaviorID::BlocksReturnToFullscreenFromPictureInPictureQuirk, BuildCondition::fullscreenAPI && BuildCondition::videoPresentationMode };
inline constexpr QuirkBehavior ensureCaptionVisibilityInFullscreenAndPictureInPicture { WebCore::QuirkBehaviorID::EnsureCaptionVisibilityInFullscreenAndPictureInPicture, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior hasBrokenEncryptedMediaAPISupportQuirk { WebCore::QuirkBehaviorID::HasBrokenEncryptedMediaAPISupportQuirk, BuildCondition::always };
inline constexpr QuirkBehavior implicitMuteWhenVolumeSetToZero { WebCore::QuirkBehaviorID::ImplicitMuteWhenVolumeSetToZero, BuildCondition::always };
inline constexpr QuirkBehavior inputMethodUsesCorrectKeyEventOrder { WebCore::QuirkBehaviorID::InputMethodUsesCorrectKeyEventOrder, BuildCondition::always };
inline constexpr QuirkBehavior inputMethodMustUseCompositionEvents { WebCore::QuirkBehaviorID::InputMethodMustUseCompositionEvents, BuildCondition::mac };
inline constexpr QuirkBehavior isMicrosoftTeamsRedirectURLQuirk { WebCore::QuirkBehaviorID::IsMicrosoftTeamsRedirectURLQuirk, BuildCondition::always };
inline constexpr QuirkBehavior isNeverRichlyEditableForTouchBarQuirk { WebCore::QuirkBehaviorID::IsNeverRichlyEditableForTouchBarQuirk, BuildCondition::mac };
inline constexpr QuirkBehavior isTouchBarUpdateSuppressedForHiddenContentEditableQuirk { WebCore::QuirkBehaviorID::IsTouchBarUpdateSuppressedForHiddenContentEditableQuirk, BuildCondition::mac };
inline constexpr QuirkBehavior maybeBypassBackForwardCache { WebCore::QuirkBehaviorID::MaybeBypassBackForwardCache, BuildCondition::always };
inline constexpr QuirkBehavior mayNeedToIgnoreContentObservation { WebCore::QuirkBehaviorID::MayNeedToIgnoreContentObservation, BuildCondition::twoPhaseClicks };
inline constexpr QuirkBehavior needsAirIndiaExpressLayeringQuirk { WebCore::QuirkBehaviorID::NeedsAirIndiaExpressLayeringQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsBodyScrollbarWidthNoneDisabledQuirk { WebCore::QuirkBehaviorID::NeedsBodyScrollbarWidthNoneDisabledQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsCanPlayAfterSeekedQuirk { WebCore::QuirkBehaviorID::NeedsCanPlayAfterSeekedQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsChromeCompatibilityUserAgentQuirk { WebCore::QuirkBehaviorID::NeedsChromeCompatibilityUserAgentQuirk, BuildCondition::cocoa, QuirkParametersNeeded::NeedsChromeCompatibilityVersion };
inline constexpr QuirkBehavior needsChromeMediaControlsPseudoElementQuirk { WebCore::QuirkBehaviorID::NeedsChromeMediaControlsPseudoElementQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsCNNCaptionQuirk { WebCore::QuirkBehaviorID::NeedsCNNCaptionQuirk, BuildCondition::cocoa };
inline constexpr QuirkBehavior needsLimitedMatroskaSupportQuirk { WebCore::QuirkBehaviorID::NeedsLimitedMatroskaSupportQuirk, BuildCondition::mediaRecorder && BuildCondition::cocoaWebMPlayer };
inline constexpr QuirkBehavior needsLogoutCookieCleanupQuirk { WebCore::QuirkBehaviorID::NeedsLogoutCookieCleanupQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsAmazonDesignMenuViewportUnitQuirk { WebCore::QuirkBehaviorID::NeedsAmazonDesignMenuViewportUnitQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsClaudeSidebarViewportUnitQuirk { WebCore::QuirkBehaviorID::NeedsClaudeSidebarViewportUnitQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsHideSelectionDuringOverflowScrollQuirk { WebCore::QuirkBehaviorID::NeedsHideSelectionDuringOverflowScrollQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsCustomUserAgentData { WebCore::QuirkBehaviorID::NeedsCustomUserAgentData, BuildCondition::always };
inline constexpr QuirkBehavior needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk { WebCore::QuirkBehaviorID::NeedsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsDisableDOMPasteAccessQuirk { WebCore::QuirkBehaviorID::NeedsDisableDOMPasteAccessQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsFacebookRemoveNotSupportedQuirk { WebCore::QuirkBehaviorID::NeedsFacebookRemoveNotSupportedQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsAnchorToBeMouseFocusableQuirk { WebCore::QuirkBehaviorID::NeedsAnchorToBeMouseFocusableQuirk, BuildCondition::cocoa };
inline constexpr QuirkBehavior needsFormControlToBeMouseFocusableQuirk { WebCore::QuirkBehaviorID::NeedsFormControlToBeMouseFocusableQuirk, BuildCondition::mac };
inline constexpr QuirkBehavior needsFullscreenDisplayNoneQuirk { WebCore::QuirkBehaviorID::NeedsFullscreenDisplayNoneQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsFullscreenObjectFitQuirk { WebCore::QuirkBehaviorID::NeedsFullscreenObjectFitQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsGMailOverflowScrollQuirk { WebCore::QuirkBehaviorID::NeedsGMailOverflowScrollQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsGoogleMapsScrollingQuirk { WebCore::QuirkBehaviorID::NeedsGoogleMapsScrollingQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsGoogleTranslateScrollingQuirk { WebCore::QuirkBehaviorID::NeedsGoogleTranslateScrollingQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsNetflixVolumeSliderQuirk { WebCore::QuirkBehaviorID::NeedsNetflixVolumeSliderQuirk, BuildCondition::iOS || BuildCondition::vision };
inline constexpr QuirkBehavior needsGeforcenowWarningDisplayNoneQuirk { WebCore::QuirkBehaviorID::NeedsGeforcenowWarningDisplayNoneQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsExpediaGroupAnimationQuirk { WebCore::QuirkBehaviorID::NeedsExpediaGroupAnimationQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsIPadMiniUserAgentQuirk { WebCore::QuirkBehaviorID::NeedsIPadMiniUserAgentQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsIPhoneUserAgentQuirk { WebCore::QuirkBehaviorID::NeedsIPhoneUserAgentQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsMediaRewriteRangeRequestQuirk { WebCore::QuirkBehaviorID::NeedsMediaRewriteRangeRequestQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsMozillaFileTypeForDataTransferQuirk { WebCore::QuirkBehaviorID::NeedsMozillaFileTypeForDataTransferQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsNavigatorUserAgentDataQuirk { WebCore::QuirkBehaviorID::NeedsNavigatorUserAgentDataQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsNowPlayingFullscreenSwapQuirk { WebCore::QuirkBehaviorID::NeedsNowPlayingFullscreenSwapQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsPartitionedCookiesQuirk { WebCore::QuirkBehaviorID::NeedsPartitionedCookiesQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsPerDocumentAutoplayBehaviorQuirk { WebCore::QuirkBehaviorID::NeedsPerDocumentAutoplayBehaviorQuirk, !BuildCondition::mac };
inline constexpr QuirkBehavior needsSuppressedPauseEventOnFullscreenExitQuirk { WebCore::QuirkBehaviorID::NeedsSuppressedPauseEventOnFullscreenExitQuirk, BuildCondition::iOS };
inline constexpr QuirkBehavior needsPreloadAutoQuirk { WebCore::QuirkBehaviorID::NeedsPreloadAutoQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsResettingTransitionCancelsRunningTransitionQuirk { WebCore::QuirkBehaviorID::NeedsResettingTransitionCancelsRunningTransitionQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsReuseLiveRangeForSelectionUpdateQuirk { WebCore::QuirkBehaviorID::NeedsReuseLiveRangeForSelectionUpdateQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsScriptToEvaluateBeforeRunningScriptFromURLQuirk { WebCore::QuirkBehaviorID::NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk, BuildCondition::always, QuirkParametersNeeded::NeedsScript };
inline constexpr QuirkBehavior needsScrollbarWidthThinDisabledQuirk { WebCore::QuirkBehaviorID::NeedsScrollbarWidthThinDisabledQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsSeekingSupportDisabledQuirk { WebCore::QuirkBehaviorID::NeedsSeekingSupportDisabledQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsSupportsProgressMonitoringQuirk { WebCore::QuirkBehaviorID::NeedsSupportsProgressMonitoringQuirk, BuildCondition::mediaSource };
inline constexpr QuirkBehavior needsSuppressPostLayoutBoundaryEventsQuirk { WebCore::QuirkBehaviorID::NeedsSuppressPostLayoutBoundaryEventsQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsTikTokOverflowingContentQuirk { WebCore::QuirkBehaviorID::NeedsTikTokOverflowingContentQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsUserAgentStringOverrideQuirk { WebCore::QuirkBehaviorID::NeedsUserAgentStringOverrideQuirk, BuildCondition::always, QuirkParametersNeeded::NeedsUserAgent };
inline constexpr QuirkBehavior needsVideoShouldMaintainAspectRatioQuirk { WebCore::QuirkBehaviorID::NeedsVideoShouldMaintainAspectRatioQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsWebKitMediaTextTrackDisplayQuirk { WebCore::QuirkBehaviorID::NeedsWebKitMediaTextTrackDisplayQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsYouTubeCaptionQuirk { WebCore::QuirkBehaviorID::NeedsYouTubeCaptionQuirk, BuildCondition::cocoa };
inline constexpr QuirkBehavior needsYouTubeEmbedAutoplayQuirk { WebCore::QuirkBehaviorID::NeedsYouTubeEmbedAutoplayQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsYouTubeMouseOutQuirk { WebCore::QuirkBehaviorID::NeedsYouTubeMouseOutQuirk, BuildCondition::twoPhaseClicks };
inline constexpr QuirkBehavior needsYouTubeOverflowScrollQuirk { WebCore::QuirkBehaviorID::NeedsYouTubeOverflowScrollQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsZeroMaxTouchPointsQuirk { WebCore::QuirkBehaviorID::NeedsZeroMaxTouchPointsQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsZomatoEmailLoginLabelQuirk { WebCore::QuirkBehaviorID::NeedsZomatoEmailLoginLabelQuirk, BuildCondition::mac };
inline constexpr QuirkBehavior requiresUserGestureToLoadInPictureInPictureQuirk { WebCore::QuirkBehaviorID::RequiresUserGestureToLoadInPictureInPictureQuirk, BuildCondition::videoPresentationMode };
inline constexpr QuirkBehavior requiresUserGestureToPauseInFullscreenAfterOrientationChangeQuirk { WebCore::QuirkBehaviorID::RequiresUserGestureToPauseInFullscreenAfterOrientationChangeQuirk, BuildCondition::videoPresentationMode && BuildCondition::iOS };
inline constexpr QuirkBehavior requiresUserGestureToPauseInPictureInPictureQuirk { WebCore::QuirkBehaviorID::RequiresUserGestureToPauseInPictureInPictureQuirk, BuildCondition::videoPresentationMode };
inline constexpr QuirkBehavior requiresUserGestureToPlayInFullscreenQuirk { WebCore::QuirkBehaviorID::RequiresUserGestureToPlayInFullscreenQuirk, BuildCondition::fullscreenAPI };
inline constexpr QuirkBehavior returnNullPictureInPictureElementDuringFullscreenChangeQuirk { WebCore::QuirkBehaviorID::ReturnNullPictureInPictureElementDuringFullscreenChangeQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldAllowMSTeamsProtocolWithoutUserGestureQuirk { WebCore::QuirkBehaviorID::ShouldAllowMSTeamsProtocolWithoutUserGestureQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldAllowPopupFromMicrosoftOfficeToOneDrive { WebCore::QuirkBehaviorID::ShouldAllowPopupFromMicrosoftOfficeToOneDrive, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldAutoplayWebAudioForArbitraryUserGestureQuirk { WebCore::QuirkBehaviorID::ShouldAutoplayWebAudioForArbitraryUserGestureQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldAvoidProgrammaticScrollClampingQuirk { WebCore::QuirkBehaviorID::ShouldAvoidProgrammaticScrollClampingQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldAvoidResizingWhenInputViewBoundsChangeQuirk { WebCore::QuirkBehaviorID::ShouldAvoidResizingWhenInputViewBoundsChangeQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldAvoidScrollingWhenFocusedContentIsVisibleQuirk { WebCore::QuirkBehaviorID::ShouldAvoidScrollingWhenFocusedContentIsVisibleQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldBlockFetchWithNewlineAndLessThan { WebCore::QuirkBehaviorID::ShouldBlockFetchWithNewlineAndLessThan, BuildCondition::always };
inline constexpr QuirkBehavior shouldBypassAsyncScriptDeferring { WebCore::QuirkBehaviorID::ShouldBypassAsyncScriptDeferring, BuildCondition::always };
inline constexpr QuirkBehavior shouldComparareUsedValuesForBorderWidthForTriggeringTransitions { WebCore::QuirkBehaviorID::ShouldComparareUsedValuesForBorderWidthForTriggeringTransitions, BuildCondition::always };
inline constexpr QuirkBehavior shouldComputeSimulatedMouseEventMovementDeltaQuirk { WebCore::QuirkBehaviorID::ShouldComputeSimulatedMouseEventMovementDeltaQuirk, BuildCondition::touchEvents || BuildCondition::touchEventRegions };
inline constexpr QuirkBehavior shouldDelayReloadWhenRegisteringServiceWorker { WebCore::QuirkBehaviorID::ShouldDelayReloadWhenRegisteringServiceWorker, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableAdSkippingInPip { WebCore::QuirkBehaviorID::ShouldDisableAdSkippingInPip, BuildCondition::pipSkipPreroll };
inline constexpr QuirkBehavior shouldDisableDataURLPaddingValidation { WebCore::QuirkBehaviorID::ShouldDisableDataURLPaddingValidation, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableDOMAudioSession { WebCore::QuirkBehaviorID::ShouldDisableDOMAudioSession, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableElementFullscreenQuirk { WebCore::QuirkBehaviorID::ShouldDisableElementFullscreenQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk { WebCore::QuirkBehaviorID::ShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk, BuildCondition::videoPresentationMode };
inline constexpr QuirkBehavior shouldDisableFetchMetadata { WebCore::QuirkBehaviorID::ShouldDisableFetchMetadata, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk { WebCore::QuirkBehaviorID::ShouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk, BuildCondition::vision };
inline constexpr QuirkBehavior shouldDisableImageCaptureQuirk { WebCore::QuirkBehaviorID::ShouldDisableImageCaptureQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldAllowMediaStreamTrackSerializationQuirk { WebCore::QuirkBehaviorID::ShouldAllowMediaStreamTrackSerializationQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldDisableLazyIframeLoadingQuirk { WebCore::QuirkBehaviorID::ShouldDisableLazyIframeLoadingQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk { WebCore::QuirkBehaviorID::ShouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisablePointerEventsQuirk { WebCore::QuirkBehaviorID::ShouldDisablePointerEventsQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldDisablePushStateFilePathRestrictions { WebCore::QuirkBehaviorID::ShouldDisablePushStateFilePathRestrictions, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableScrollAnchoringQuirk { WebCore::QuirkBehaviorID::ShouldDisableScrollAnchoringQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDisableThreadedAnimationsQuirk { WebCore::QuirkBehaviorID::ShouldDisableThreadedAnimationsQuirk, BuildCondition::threadedAnimations };
inline constexpr QuirkBehavior shouldDisableWritingSuggestionsByDefaultQuirk { WebCore::QuirkBehaviorID::ShouldDisableWritingSuggestionsByDefaultQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDispatchPlayPauseEventsOnResume { WebCore::QuirkBehaviorID::ShouldDispatchPlayPauseEventsOnResume, BuildCondition::always };
inline constexpr QuirkBehavior shouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick { WebCore::QuirkBehaviorID::ShouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick, BuildCondition::touchEvents };
inline constexpr QuirkBehavior shouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk { WebCore::QuirkBehaviorID::ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk { WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldDispatchSimulatedMouseEventsQuirk { .id = WebCore::QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk, .isAvailable = BuildCondition::touchEvents || BuildCondition::touchEventRegions, .quirkConditionsSupported = QuirkConditionsSupported::ElementSelector };
inline constexpr QuirkBehavior shouldEnableCameraAndMicrophonePermissionStateQuirk { WebCore::QuirkBehaviorID::ShouldEnableCameraAndMicrophonePermissionStateQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableCameraBackgroundPlayback { WebCore::QuirkBehaviorID::ShouldEnableCameraBackgroundPlayback, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableEnumerateDeviceQuirk { WebCore::QuirkBehaviorID::ShouldEnableEnumerateDeviceQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableFacebookFlagQuirk { WebCore::QuirkBehaviorID::ShouldEnableFacebookFlagQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableFontLoadingAPIQuirk { WebCore::QuirkBehaviorID::ShouldEnableFontLoadingAPIQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldEnableLegacyGetUserMediaQuirk { WebCore::QuirkBehaviorID::ShouldEnableLegacyGetUserMediaQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableRemoteTrackLabelQuirk { WebCore::QuirkBehaviorID::ShouldEnableRemoteTrackLabelQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnableRTCEncodedStreamsQuirk { WebCore::QuirkBehaviorID::ShouldEnableRTCEncodedStreamsQuirk, BuildCondition::webRTC };
inline constexpr QuirkBehavior shouldEnableSpeakerSelectionPermissionsPolicyQuirk { WebCore::QuirkBehaviorID::ShouldEnableSpeakerSelectionPermissionsPolicyQuirk, BuildCondition::mediaStream };
inline constexpr QuirkBehavior shouldEnterNativeFullscreenWhenCallingElementRequestFullscreen { WebCore::QuirkBehaviorID::ShouldEnterNativeFullscreenWhenCallingElementRequestFullscreen, BuildCondition::always };
inline constexpr QuirkBehavior shouldExposeShowModalDialog { WebCore::QuirkBehaviorID::ShouldExposeShowModalDialog, BuildCondition::always };
inline constexpr QuirkBehavior shouldFlipScreenDimensionsQuirk { WebCore::QuirkBehaviorID::ShouldFlipScreenDimensionsQuirk, BuildCondition::flipScreenDimensionsQuirks };
inline constexpr QuirkBehavior shouldHideCoarsePointerCharacteristicsQuirk { WebCore::QuirkBehaviorID::ShouldHideCoarsePointerCharacteristicsQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldHideSoftTopScrollEdgeEffectDuringFocusQuirk { WebCore::QuirkBehaviorID::ShouldHideSoftTopScrollEdgeEffectDuringFocusQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldIgnoreAriaForFastPathContentObservationCheckQuirk { WebCore::QuirkBehaviorID::ShouldIgnoreAriaForFastPathContentObservationCheckQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldIgnoreInputModeNone { WebCore::QuirkBehaviorID::ShouldIgnoreInputModeNone, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldIgnorePlaysInlineRequirementQuirk { WebCore::QuirkBehaviorID::ShouldIgnorePlaysInlineRequirementQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldIgnoreTextAutoSizingQuirk { WebCore::QuirkBehaviorID::ShouldIgnoreTextAutoSizingQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk { WebCore::QuirkBehaviorID::ShouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk, BuildCondition::metaViewport };
inline constexpr QuirkBehavior shouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk { WebCore::QuirkBehaviorID::ShouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk, BuildCondition::metaViewport };
inline constexpr QuirkBehavior shouldUseDynamicViewportUnitsAsDefaultQuirk { WebCore::QuirkBehaviorID::ShouldUseDynamicViewportUnitsAsDefaultQuirk, BuildCondition::metaViewport };
inline constexpr QuirkBehavior shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk { WebCore::QuirkBehaviorID::ShouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldNavigatorPluginsBeEmpty { WebCore::QuirkBehaviorID::ShouldNavigatorPluginsBeEmpty, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldOmitTouchEventDOMAttributesForDesktopWebsiteQuirk { WebCore::QuirkBehaviorID::ShouldOmitTouchEventDOMAttributesForDesktopWebsiteQuirk, BuildCondition::touchEvents };
inline constexpr QuirkBehavior shouldPreventDispatchOfTouchEventQuirk { WebCore::QuirkBehaviorID::ShouldPreventDispatchOfTouchEventQuirk, BuildCondition::touchEvents || BuildCondition::touchEventRegions };
inline constexpr QuirkBehavior shouldPreventKeyframeEffectAccelerationQuirk { WebCore::QuirkBehaviorID::ShouldPreventKeyframeEffectAccelerationQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk { WebCore::QuirkBehaviorID::ShouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldReportDocumentAsVisibleIfActivePIPQuirk { WebCore::QuirkBehaviorID::ShouldReportDocumentAsVisibleIfActivePIPQuirk, BuildCondition::pictureInPictureAPI };
inline constexpr QuirkBehavior shouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk { WebCore::QuirkBehaviorID::ShouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldUseLayoutViewportForClientRectsQuirk { WebCore::QuirkBehaviorID::ShouldUseLayoutViewportForClientRectsQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldSilenceWindowResizeEventsDuringApplicationSnapshotting { WebCore::QuirkBehaviorID::ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting, BuildCondition::iOS || BuildCondition::vision };
inline constexpr QuirkBehavior shouldSilenceMediaQueryListChangeEvents { WebCore::QuirkBehaviorID::ShouldSilenceMediaQueryListChangeEvents, BuildCondition::iOS || BuildCondition::vision };
inline constexpr QuirkBehavior shouldSilenceResizeObservers { WebCore::QuirkBehaviorID::ShouldSilenceResizeObservers, BuildCondition::iOS || BuildCondition::vision };
inline constexpr QuirkBehavior shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk { WebCore::QuirkBehaviorID::ShouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior needsWebExScrollabilityQuirk { WebCore::QuirkBehaviorID::NeedsWebExScrollabilityQuirk, BuildCondition::iOSFamily && BuildCondition::desktopContentModeQuirks };
inline constexpr QuirkBehavior shouldSupportHoverMediaQueriesQuirk { WebCore::QuirkBehaviorID::ShouldSupportHoverMediaQueriesQuirk, BuildCondition::desktopContentModeQuirks };
inline constexpr QuirkBehavior shouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk { WebCore::QuirkBehaviorID::ShouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldTranscodeHeicImagesQuirk { WebCore::QuirkBehaviorID::ShouldTranscodeHeicImagesQuirk, BuildCondition::always };
inline constexpr QuirkBehavior shouldTreatAddingMouseOutEventListenerAsContentChange { WebCore::QuirkBehaviorID::ShouldTreatAddingMouseOutEventListenerAsContentChange, BuildCondition::contentChangeObserver };
inline constexpr QuirkBehavior shouldUnloadHeavyFrames { WebCore::QuirkBehaviorID::ShouldUnloadHeavyFrames, BuildCondition::always };
inline constexpr QuirkBehavior shouldAvoidStartingSelectionOnMouseDownOverPointerCursor { WebCore::QuirkBehaviorID::ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor, BuildCondition::always };
inline constexpr QuirkBehavior shouldAllowNotificationPermissionWithoutUserGesture { WebCore::QuirkBehaviorID::ShouldAllowNotificationPermissionWithoutUserGesture, BuildCondition::always };
inline constexpr QuirkBehavior needsInstagramResizingReelsQuirk { WebCore::QuirkBehaviorID::NeedsInstagramResizingReelsQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsYahooVolumeSliderQuirk { WebCore::QuirkBehaviorID::NeedsYahooVolumeSliderQuirk, BuildCondition::always };
inline constexpr QuirkBehavior needsChromeOSNavigatorUserAgentQuirk { WebCore::QuirkBehaviorID::NeedsChromeOSNavigatorUserAgentQuirk, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldSendFakeTouchForceChangeEvent { WebCore::QuirkBehaviorID::ShouldSendFakeTouchForceChangeEvent, BuildCondition::iOSFamily };
inline constexpr QuirkBehavior shouldLimitHLSPlaybackRate { WebCore::QuirkBehaviorID::ShouldLimitHLSPlaybackRate, BuildCondition::always };
inline constexpr QuirkBehavior shouldDeferIntersectionObserversDuringResize { WebCore::QuirkBehaviorID::ShouldDeferIntersectionObserversDuringResize, BuildCondition::always };
inline constexpr QuirkBehavior shouldSuppressHLSSubtitles { WebCore::QuirkBehaviorID::ShouldSuppressHLSSubtitles, BuildCondition::always };
inline constexpr QuirkBehavior shouldSuppressMediaSessionPauseActionOnInterruption { WebCore::QuirkBehaviorID::ShouldSuppressMediaSessionPauseActionOnInterruption, BuildCondition::always };
inline constexpr QuirkBehavior shouldBlockAudiblePlaybackWhileAudioIsPlaying { WebCore::QuirkBehaviorID::ShouldBlockAudiblePlaybackWhileAudioIsPlaying, BuildCondition::always };
inline constexpr QuirkBehavior needsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk { WebCore::QuirkBehaviorID::NeedsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk, BuildCondition::cocoa };

} // namespace Behaviors

} // namespace WebCore
