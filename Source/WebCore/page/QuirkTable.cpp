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

#include "config.h"
#include "QuirkTable.h"

#include <algorithm>
#include <array>
#include <span>
#include <utility>

namespace WebCore {

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

static constexpr std::array bbcDomains { "bbc.co.uk"_s, "bbc.com"_s };
static constexpr std::array expediaGroupDomains {
    "carrentals.com"_s, "cheaptickets.com"_s, "hoteis.com"_s, "hoteles.com"_s,
    "hotels.com"_s, "mrjet.se"_s, "orbitz.com"_s, "travelocity.ca"_s,
    "travelocity.com"_s, "wotif.co.nz"_s, "wotif.com"_s
};
static constexpr std::array microsoftTeamsHosts { "teams.live.com"_s, "teams.microsoft.com"_s };
static constexpr std::array naverHostsWithoutSimulatedMouseEvents { "tv.naver.com"_s, "mail.naver.com"_s, "m.naver.com"_s };
static constexpr std::array youTubeEmbedDomains { "youtube.com"_s, "youtube-nocookie.com"_s };
static constexpr std::array claudeDomains { "claude.ai"_s, "claude.com"_s };

namespace SiteSpecificQuirks {
using enum SiteSpecificQuirk;
using namespace URLRefinement;
using namespace BuildCondition;

consteval bool isAvailable(SiteSpecificQuirk quirk)
{
    switch (quirk) {
    case AllowLayeredFullscreenVideos: return iOS || vision;
    case BlocksEnteringStandardFullscreenFromPictureInPictureQuirk: return fullscreenAPI && videoPresentationMode;
    case BlocksReturnToFullscreenFromPictureInPictureQuirk: return fullscreenAPI && videoPresentationMode;
    case EnsureCaptionVisibilityInFullscreenAndPictureInPicture: return iOSFamily;
    case HasBrokenEncryptedMediaAPISupportQuirk: return always;
    case ImplicitMuteWhenVolumeSetToZero: return always;
    case InputMethodMustUseCompositionEvents: return mac;
    case InputMethodUsesCorrectKeyEventOrder: return always;
    case IsMicrosoftTeamsRedirectURLQuirk: return always;
    case IsNeverRichlyEditableForTouchBarQuirk: return mac;
    case IsTouchBarUpdateSuppressedForHiddenContentEditableQuirk: return mac;
    case MayNeedToIgnoreContentObservation: return twoPhaseClicks;
    case MaybeBypassBackForwardCache: return always;
    case NeedsAirIndiaExpressLayeringQuirk: return always;
    case NeedsAmazonDesignMenuViewportUnitQuirk: return iOSFamily;
    case NeedsAnchorToBeMouseFocusableQuirk: return cocoa;
    case NeedsBodyScrollbarWidthNoneDisabledQuirk: return always;
    case NeedsCNNCaptionQuirk: return cocoa;
    case NeedsCanPlayAfterSeekedQuirk: return always;
    case NeedsChromeMediaControlsPseudoElementQuirk: return always;
    case NeedsChromeOSNavigatorUserAgentQuirk: return iOSFamily;
    case NeedsClaudeSidebarViewportUnitQuirk: return iOSFamily;
    case NeedsCustomUserAgentData: return always;
    case NeedsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk: return iOSFamily;
    case NeedsDisableDOMPasteAccessQuirk: return always;
    case NeedsExpediaGroupAnimationQuirk: return always;
    case NeedsFacebookRemoveNotSupportedQuirk: return always;
    case NeedsFormControlToBeMouseFocusableQuirk: return mac;
    case NeedsFullscreenDisplayNoneQuirk: return iOSFamily;
    case NeedsFullscreenObjectFitQuirk: return iOSFamily;
    case NeedsGMailOverflowScrollQuirk: return iOSFamily;
    case NeedsGeforcenowWarningDisplayNoneQuirk: return always;
    case NeedsGoogleMapsScrollingQuirk: return iOSFamily;
    case NeedsGoogleTranslateScrollingQuirk: return iOSFamily;
    case NeedsHideSelectionDuringOverflowScrollQuirk: return iOSFamily;
    case NeedsIPadMiniUserAgentQuirk: return always;
    case NeedsIPhoneUserAgentQuirk: return iOSFamily;
    case NeedsInstagramResizingReelsQuirk: return always;
    case NeedsLimitedMatroskaSupportQuirk: return mediaRecorder && cocoaWebMPlayer;
    case NeedsLogoutCookieCleanupQuirk: return always;
    case NeedsMediaRewriteRangeRequestQuirk: return always;
    case NeedsMozillaFileTypeForDataTransferQuirk: return always;
    case NeedsNavigatorUserAgentDataQuirk: return always;
    case NeedsNetflixVolumeSliderQuirk: return iOS || vision;
    case NeedsNowPlayingFullscreenSwapQuirk: return always;
    case NeedsPartitionedCookiesQuirk: return always;
    case NeedsPreloadAutoQuirk: return iOSFamily;
    case NeedsResettingTransitionCancelsRunningTransitionQuirk: return always;
    case NeedsReuseLiveRangeForSelectionUpdateQuirk: return always;
    case NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk: return always;
    case NeedsScrollbarWidthThinDisabledQuirk: return always;
    case NeedsSeekingSupportDisabledQuirk: return always;
    case NeedsSupportsProgressMonitoringQuirk: return mediaSource;
    case NeedsSuppressPostLayoutBoundaryEventsQuirk: return always;
    case NeedsSuppressedPauseEventOnFullscreenExitQuirk: return iOS;
    case NeedsTikTokOverflowingContentQuirk: return always;
    case NeedsVideoShouldMaintainAspectRatioQuirk: return always;
    case NeedsWebExScrollabilityQuirk: return iOSFamily && desktopContentModeQuirks;
    case NeedsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk: return cocoa;
    case NeedsWebKitMediaTextTrackDisplayQuirk: return always;
    case NeedsYahooVolumeSliderQuirk: return always;
    case NeedsYouTubeCaptionQuirk: return cocoa;
    case NeedsYouTubeEmbedAutoplayQuirk: return iOSFamily;
    case NeedsYouTubeMouseOutQuirk: return twoPhaseClicks;
    case NeedsYouTubeOverflowScrollQuirk: return iOSFamily;
    case NeedsZeroMaxTouchPointsQuirk: return always;
    case NeedsZomatoEmailLoginLabelQuirk: return mac;
    case RequiresUserGestureToLoadInPictureInPictureQuirk: return videoPresentationMode;
    case RequiresUserGestureToPauseInFullscreenAfterOrientationChangeQuirk: return videoPresentationMode && iOS;
    case RequiresUserGestureToPauseInPictureInPictureQuirk: return videoPresentationMode;
    case RequiresUserGestureToPlayInFullscreenQuirk: return fullscreenAPI;
    case ReturnNullPictureInPictureElementDuringFullscreenChangeQuirk: return always;
    case ShouldAllowMSTeamsProtocolWithoutUserGestureQuirk: return always;
    case ShouldAllowMediaStreamTrackSerializationQuirk: return mediaStream;
    case ShouldAllowNotificationPermissionWithoutUserGesture: return always;
    case ShouldAllowPopupFromMicrosoftOfficeToOneDrive: return iOSFamily;
    case ShouldAutoplayWebAudioForArbitraryUserGestureQuirk: return always;
    case ShouldAvoidProgrammaticScrollClampingQuirk: return always;
    case ShouldAvoidResizingWhenInputViewBoundsChangeQuirk: return always;
    case ShouldAvoidScrollingWhenFocusedContentIsVisibleQuirk: return always;
    case ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor: return always;
    case ShouldBlockAudiblePlaybackWhileAudioIsPlaying: return always;
    case ShouldBlockFetchWithNewlineAndLessThan: return always;
    case ShouldBypassAsyncScriptDeferring: return always;
    case ShouldComparareUsedValuesForBorderWidthForTriggeringTransitions: return always;
    case ShouldDeferIntersectionObserversDuringResize: return always;
    case ShouldDelayReloadWhenRegisteringServiceWorker: return always;
    case ShouldDisableAdSkippingInPip: return pipSkipPreroll;
    case ShouldDisableDOMAudioSession: return always;
    case ShouldDisableDataURLPaddingValidation: return always;
    case ShouldDisableElementFullscreenQuirk: return iOSFamily;
    case ShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk: return videoPresentationMode;
    case ShouldDisableFetchMetadata: return always;
    case ShouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk: return vision;
    case ShouldDisableImageCaptureQuirk: return mediaStream;
    case ShouldDisableLazyIframeLoadingQuirk: return always;
    case ShouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk: return always;
    case ShouldDisablePointerEventsQuirk: return iOSFamily;
    case ShouldDisablePushStateFilePathRestrictions: return always;
    case ShouldDisableScrollAnchoringQuirk: return always;
    case ShouldDisableThreadedAnimationsQuirk: return threadedAnimations;
    case ShouldDisableWritingSuggestionsByDefaultQuirk: return always;
    case ShouldDispatchPlayPauseEventsOnResume: return always;
    case ShouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick: return touchEvents;
    case ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk: return always;
    case ShouldDispatchSimulatedMouseEventsQuirk: return touchEvents || touchEventRegions;
    case ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk: return always;
    case ShouldEnableCameraAndMicrophonePermissionStateQuirk: return mediaStream;
    case ShouldEnableCameraBackgroundPlayback: return mediaStream;
    case ShouldEnableEnumerateDeviceQuirk: return mediaStream;
    case ShouldEnableFacebookFlagQuirk: return mediaStream;
    case ShouldEnableFontLoadingAPIQuirk: return always;
    case ShouldEnableLegacyGetUserMediaQuirk: return mediaStream;
    case ShouldEnableRTCEncodedStreamsQuirk: return webRTC;
    case ShouldEnableRemoteTrackLabelQuirk: return mediaStream;
    case ShouldEnableSpeakerSelectionPermissionsPolicyQuirk: return mediaStream;
    case ShouldEnterNativeFullscreenWhenCallingElementRequestFullscreen: return always;
    case ShouldExposeShowModalDialog: return always;
    case ShouldFlipScreenDimensionsQuirk: return flipScreenDimensionsQuirks;
    case ShouldHideCoarsePointerCharacteristicsQuirk: return iOSFamily;
    case ShouldHideSoftTopScrollEdgeEffectDuringFocusQuirk: return iOSFamily;
    case ShouldIgnoreAriaForFastPathContentObservationCheckQuirk: return iOSFamily;
    case ShouldIgnoreInputModeNone: return iOSFamily;
    case ShouldIgnorePlaysInlineRequirementQuirk: return always;
    case ShouldIgnoreTextAutoSizingQuirk: return always;
    case ShouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk: return metaViewport;
    case ShouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk: return metaViewport;
    case ShouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk: return always;
    case ShouldLimitHLSPlaybackRate: return always;
    case ShouldNavigatorPluginsBeEmpty: return iOSFamily;
    case ShouldOmitTouchEventDOMAttributesForDesktopWebsiteQuirk: return touchEvents;
    case ShouldPreventDispatchOfTouchEventQuirk: return touchEvents || touchEventRegions;
    case ShouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk: return always;
    case ShouldReportDocumentAsVisibleIfActivePIPQuirk: return pictureInPictureAPI;
    case ShouldSendFakeTouchForceChangeEvent: return iOSFamily;
    case ShouldSilenceMediaQueryListChangeEvents: return iOS || vision;
    case ShouldSilenceResizeObservers: return iOS || vision;
    case ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting: return iOS || vision;
    case ShouldSupportHoverMediaQueriesQuirk: return desktopContentModeQuirks;
    case ShouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk: return iOSFamily;
    case ShouldSuppressHLSSubtitles: return always;
    case ShouldSuppressMediaSessionPauseActionOnInterruption: return always;
    case ShouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk: return iOSFamily;
    case ShouldTranscodeHeicImagesQuirk: return always;
    case ShouldTreatAddingMouseOutEventListenerAsContentChange: return contentChangeObserver;
    case ShouldUnloadHeavyFrames: return always;
    case ShouldUseDynamicViewportUnitsAsDefaultQuirk: return metaViewport;
    case ShouldUseLayoutViewportForClientRectsQuirk: return iOSFamily;
    case ShouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk: return always;
    case NumberOfQuirks: break;
    }
    return false;
}

consteval QuirkBitSet computeUnavailableQuirks()
{
    QuirkBitSet bits;
    for (size_t i = 0; i < static_cast<size_t>(NumberOfQuirks); ++i) {
        if (!isAvailable(static_cast<SiteSpecificQuirk>(i)))
            bits.set(i);
    }
    return bits;
}

static constexpr QuirkBitSet unavailableQuirks = computeUnavailableQuirks();

static constexpr Quirk fullTable[] = {
    // 365scores.com rdar://116491386
    { .match = URLMatch::domain("365scores.com"_s),
        .behaviors = { ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting },
        .availableWhen = iOS || vision },

    // actesting.org rdar://124017544
    { .match = URLMatch::domain("actesting.org"_s),
        .behaviors = { ShouldEnableLegacyGetUserMediaQuirk } },

    // airindiaexpress.com https://webkit.org/b/317375
    { .match = URLMatch::domain("airindiaexpress.com"_s),
        .behaviors = { NeedsAirIndiaExpressLayeringQuirk } },

    // Note: There is a userAgent override for rdar://117771731, see needsCustomUserAgentOverride()
    // airtable.com rdar://49124313
    { .match = URLMatch::domain("airtable.com"_s),
        .behaviors = { ShouldDispatchSimulatedMouseEventsQuirk } },

    { .match = URLMatch::anyTopLevelDomain("amazon"_s),
        .behaviors = {
            // amazon.com rdar://49124529
            ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
            // amazon.com rdar://49124313
            ShouldDispatchSimulatedMouseEventsQuirk,
        },
        .site = QuirkSite::Amazon },

    { .match = URLMatch::domain("amazon.design"_s),
        .behaviors = { NeedsAmazonDesignMenuViewportUnitQuirk } },

    // apple.com rdar://154434137
    // FIXME: Maybe EnsureCaptionVisibilityInFullscreenAndPictureInPicture should apply to apple.com.cn too?
    { .match = URLMatch::domain("apple.com"_s),
        .behaviors = { EnsureCaptionVisibilityInFullscreenAndPictureInPicture } },

    // Quirk added for rdar://181007316, remove when rdar://182134549 is fixed.
    { .match = URLMatch::anyTopLevelDomain("apple"_s).when(pathContains("/retail"_s)),
        .behaviors = { ShouldDisableScrollAnchoringQuirk } },

    // as.com: rdar://121014613
    { .match = URLMatch::domain("as.com"_s).when(smallScreen()),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    // att.com rdar://55185021
    { .match = URLMatch::domain("att.com"_s),
        .behaviors = { ShouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk },
        .availableWhen = iOSFamily },

    // Login issue on bankofamerica.com (rdar://104938789).
    { .match = URLMatch::domain("bankofamerica.com"_s),
        .behaviors = {
            MaybeBypassBackForwardCache,
        },
        .site = QuirkSite::BankOfAmerica },

    // bbc.co.uk rdar://126494734
    // bbc.com rdar://157499149
    { .match = URLMatch::domain(bbcDomains),
        .behaviors = { ReturnNullPictureInPictureElementDuringFullscreenChangeQuirk } },

    // bestbuy.com rdar://136235936
    { .match = URLMatch::domain("bestbuy.com"_s),
        .behaviors = {
            NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk,
        },
        .site = QuirkSite::BestBuy },

    // billpaysite.com rdar://141328971
    { .match = URLMatch::hostOrSubdomainOf("billpaysite.com"_s),
        .behaviors = { NeedsPartitionedCookiesQuirk } },

    { .match = URLMatch::domain("bing.com"_s),
        .behaviors = {
            // bing.com rdar://133223599
            MaybeBypassBackForwardCache,
            // bing.com rdar://126573838
            NeedsMediaRewriteRangeRequestQuirk,
        },
        .site = QuirkSite::Bing },

    // bungalow.com rdar://61658940
    { .match = URLMatch::domain("bungalow.com"_s),
        .behaviors = { ShouldBypassAsyncScriptDeferring } },

    // canva.com https://webkit.org/b/293886
    { .match = URLMatch::domain("canva.com"_s),
        .behaviors = { ShouldTranscodeHeicImagesQuirk } },

    { .match = URLMatch::domain("capitalgroup.com"_s),
        .behaviors = { ShouldDelayReloadWhenRegisteringServiceWorker } },

    // Remove this once rdar://139478801 is resolved.
    { .match = URLMatch::domain("cbssports.com"_s),
        .behaviors = {
            ShouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk,
        },
        .site = QuirkSite::CBSSports,
        .availableWhen = iOSFamily },

    { .match = URLMatch::hostOrSubdomainOf("ceac.state.gov"_s),
        .behaviors = {
            // ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=193478
            NeedsFormControlToBeMouseFocusableQuirk,
            // ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=311383
            NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk,
        },
        .site = QuirkSite::CEAC },

    // secure.chase.com rdar://126715227
    { .match = URLMatch::host("secure.chase.com"_s),
        .behaviors = { ShouldOmitTouchEventDOMAttributesForDesktopWebsiteQuirk },
        .availableWhen = touchEvents },

    { .match = URLMatch::domain("chess.com"_s).when(smallScreen()),
        .behaviors = { ShouldEnterNativeFullscreenWhenCallingElementRequestFullscreen },
        .availableWhen = iOS },

    { .match = URLMatch::domain("claude.ai"_s),
        .behaviors = {
            NeedsClaudeSidebarViewportUnitQuirk,
            // rdar://174779259 - logout flow leaves identification cookies
            // causing redirect loop on next /chat boot.
            // See Quirks::clearLogoutSurvivingIdentityCookiesIfNeeded().
            NeedsLogoutCookieCleanupQuirk,
        } },

    { .match = URLMatch::domain(claudeDomains),
        .behaviors = { NeedsHideSelectionDuringOverflowScrollQuirk } },

    { .match = URLMatch::domain("cnn.com"_s),
        .behaviors = {
            // cnn.com rdar://119640248
            NeedsFullscreenObjectFitQuirk,
            NeedsCNNCaptionQuirk,
            // cnn.com rdar://176539646
            ShouldDisableThreadedAnimationsQuirk,
        },
        .availableWhen = iOSFamily },

    { .match = URLMatch::host("codepen.io"_s),
        .behaviors = { ShouldEnableSpeakerSelectionPermissionsPolicyQuirk } },

    { .match = URLMatch::domain("crunchyroll.com"_s),
        .behaviors = { NeedsSuppressPostLayoutBoundaryEventsQuirk } },

    { .match = URLMatch::domain("dailymail.co.uk"_s),
        .behaviors = { ShouldUnloadHeavyFrames } },

    { .match = URLMatch::host("digits.t-mobile.com"_s),
        .behaviors = { NeedsNavigatorUserAgentDataQuirk, NeedsCustomUserAgentData } },

    // descript.com rdar://156024693
    { .match = URLMatch::domain("descript.com"_s),
        .behaviors = { ShouldDisableDOMAudioSession } },

    { .match = URLMatch::domain("dictionary.com"_s),
        .behaviors = { NeedsAnchorToBeMouseFocusableQuirk },
        .site = QuirkSite::Dictionary },

    { .match = URLMatch::domain("dictionary.com"_s),
        .behaviors = { NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk },
        .availableWhen = iOSFamily },

    // digitaltrends.com rdar://121014613
    { .match = URLMatch::domain("digitaltrends.com"_s).when(smallScreen()),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    // discord.com rdar://162719481
    { .match = URLMatch::domain("discord.com"_s),
        .behaviors = { ShouldUseLayoutViewportForClientRectsQuirk } },

    // disneyplus rdar://137613110
    { .match = URLMatch::domain("disneyplus.com"_s),
        .behaviors = { ShouldHideCoarsePointerCharacteristicsQuirk } },

    // disneyplus rdar://151715964
    { .match = URLMatch::domain("disneyplus.com"_s),
        .behaviors = { NeedsZeroMaxTouchPointsQuirk },
        .availableWhen = iOSFamily && desktopContentModeQuirks },

    { .match = URLMatch::domain("ea.com"_s),
        .site = QuirkSite::EA },

    { .match = URLMatch::domain("espn.com"_s),
        .behaviors = {
            // espn.com rdar://184169028
            NeedsSuppressedPauseEventOnFullscreenExitQuirk,
            // espn.com rdar://problem/95651814
            AllowLayeredFullscreenVideos,
            // espn.com rdar://problem/73227900
            ShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk,
        } },

    // Expedia Group rdar://126631968
    { .match = URLMatch::domain(expediaGroupDomains),
        .behaviors = { NeedsExpediaGroupAnimationQuirk } },
    { .match = URLMatch::anyTopLevelDomain("ebookers"_s),
        .behaviors = { NeedsExpediaGroupAnimationQuirk } },
    { .match = URLMatch::anyTopLevelDomain("expedia"_s),
        .behaviors = { NeedsExpediaGroupAnimationQuirk } },

    { .match = URLMatch::domain("facebook.com"_s),
        .behaviors = {
            // facebook.com rdar://100871402
            NeedsFacebookRemoveNotSupportedQuirk,
            // facebook.com rdar://174179871
            ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
            // facebook.com rdar://67273166
            RequiresUserGestureToPauseInPictureInPictureQuirk,
            // facebook.com rdar://158736355
            ShouldEnableCameraAndMicrophonePermissionStateQuirk,
            ShouldEnableRemoteTrackLabelQuirk,
            // facebook.com rdar://41104397
            ShouldEnableFacebookFlagQuirk,
            // facebook.com rdar://161269819
            ShouldEnableEnumerateDeviceQuirk,
            // facebook.com rdar://158736355
            ShouldEnableRTCEncodedStreamsQuirk,
            // facebook.com rdar://174179871
            ShouldDispatchSimulatedMouseEventsQuirk,
        },
        .site = QuirkSite::Facebook },

    // flipkart.com rdar://49648520
    { .match = URLMatch::domain("flipkart.com"_s),
        .behaviors = { ShouldDispatchSimulatedMouseEventsQuirk } },

    // forbes.com rdar://67273166
    { .match = URLMatch::domain("forbes.com"_s),
        .behaviors = { RequiresUserGestureToPauseInPictureInPictureQuirk } },

    { .match = URLMatch::host("play.geforcenow.com"_s),
        .behaviors = { NeedsGeforcenowWarningDisplayNoneQuirk } },

    // gizmodo.com rdar://102227302
    { .match = URLMatch::domain("gizmodo.com"_s),
        .behaviors = { NeedsFullscreenDisplayNoneQuirk } },

    { .match = URLMatch::anyTopLevelDomain("google"_s),
        .behaviors = {
            // docs.google.com rdar://59893415
            MaybeBypassBackForwardCache,
        },
        .site = QuirkSite::GoogleProperty },

    { .match = URLMatch::anyTopLevelDomain("google"_s).when(pathStartsWith("/maps/"_s)),
        .behaviors = {
            // maps.google.com rdar://152194074
            MayNeedToIgnoreContentObservation,
            // maps.google.com rdar://67358928
            NeedsGoogleMapsScrollingQuirk,
            // maps.google.com https://bugs.webkit.org/show_bug.cgi?id=214945
            ShouldAvoidResizingWhenInputViewBoundsChangeQuirk,
            // maps.google.com rdar://49124313
            ShouldDispatchSimulatedMouseEventsQuirk,
        },
        .site = QuirkSite::GoogleMaps },

    { .match = URLMatch::host("docs.google.com"_s),
        .behaviors = {
            InputMethodUsesCorrectKeyEventOrder,
            InputMethodMustUseCompositionEvents,
            // docs.google.com https://bugs.webkit.org/show_bug.cgi?id=161984
            IsTouchBarUpdateSuppressedForHiddenContentEditableQuirk,
            // docs.google.com rdar://49864669
            ShouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk,
        },
        .site = QuirkSite::GoogleDocs },

    // docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
    { .match = URLMatch::host("docs.google.com"_s).when(pathStartsWith("/spreadsheets/"_s)),
        .behaviors = { NeedsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk } },

    { .match = URLMatch::host("docs.google.com"_s).when(pathStartsWith("/presentation/"_s)),
        .behaviors = { ShouldIgnoreInputModeNone } },

    // mail.google.com rdar://49403416
    { .match = URLMatch::host("mail.google.com"_s),
        .behaviors = { NeedsGMailOverflowScrollQuirk } },

    // translate.google.com rdar://106539018
    { .match = URLMatch::host("translate.google.com"_s),
        .behaviors = { NeedsGoogleTranslateScrollingQuirk } },

    { .match = URLMatch::host("translate.google.com"_s),
        .behaviors = { NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk },
        .availableWhen = iOSFamily },

    // sites.google.com rdar://58653069
    { .match = URLMatch::host("sites.google.com"_s),
        .behaviors = { ShouldPreventDispatchOfTouchEventQuirk } },

    { .match = URLMatch::host("meet.google.com"_s),
        .behaviors = { ShouldEnableCameraBackgroundPlayback } },

    // hbomax.com https://bugs.webkit.org/show_bug.cgi?id=244737
    { .match = URLMatch::domain("hbomax.com"_s),
        .behaviors = { ShouldEnableFontLoadingAPIQuirk } },

    { .match = URLMatch::host("play.hbomax.com"_s),
        .behaviors = {
            // play.hbomax.com rdar://158430821
            ShouldDisableAdSkippingInPip,
            // hbomax.com: rdar://138806698
            ShouldSupportHoverMediaQueriesQuirk,
        } },

    // hbomax.com: rdar://138424489
    { .match = URLMatch::host("play.hbomax.com"_s),
        .behaviors = { NeedsZeroMaxTouchPointsQuirk },
        .availableWhen = desktopContentModeQuirks },

    { .match = URLMatch::domain("hulu.com"_s),
        .behaviors = {
            // hulu.com rdar://55041979
            NeedsCanPlayAfterSeekedQuirk,
            // hulu.com rdar://100199996
            NeedsVideoShouldMaintainAspectRatioQuirk,
            // hulu.com rdar://126096361
            ImplicitMuteWhenVolumeSetToZero,
        } },

    // icloud.com rdar://131836301
    { .match = URLMatch::domain("icloud.com"_s).when(pathOrFragmentContains("mail"_s)),
        .behaviors = { ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting } },
    // icloud.com rdar://26013388
    { .match = URLMatch::domain("icloud.com"_s).when(pathOrFragmentContains("notes"_s)),
        .behaviors = { IsNeverRichlyEditableForTouchBarQuirk } },

    { .match = URLMatch::domain("iheart.com"_s),
        .behaviors = {
            NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk,
        },
        .site = QuirkSite::IHeart },

    { .match = URLMatch::domain("imdb.com"_s),
        .behaviors = {
            // imdb.com: rdar://137991466
            NeedsChromeMediaControlsPseudoElementQuirk,
            // imdb.com: rdar://162684936
            NeedsZeroMaxTouchPointsQuirk,
        } },

    // FIXME: Remove this quirk once <rdar://113978106> is no longer happening.
    { .match = URLMatch::host("www.indiatimes.com"_s),
        .behaviors = { NeedsIPadMiniUserAgentQuirk } },

    { .match = URLMatch::domain("instagram.com"_s),
        .behaviors = {
            // rdar://166400170
            NeedsInstagramResizingReelsQuirk,
            // instagram.com: rdar://174936655
            ShouldSendFakeTouchForceChangeEvent,
        } },

    // instagram.com rdar://121014613
    { .match = URLMatch::domain("instagram.com"_s),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    // invideo.io rdar://171741842 https://webkit.org/b/311602
    { .match = URLMatch::domain("invideo.io"_s),
        .behaviors = {
            NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk,
        },
        .site = QuirkSite::InVideo },

    { .match = URLMatch::domain("linkedin.com"_s),
        .site = QuirkSite::LinkedIn },

    { .match = URLMatch::domain("live.com"_s),
        .behaviors = {
            // live.com: rdar://167489768
            NeedsChromeOSNavigatorUserAgentQuirk,
            // live.com rdar://52116170
            ShouldAvoidResizingWhenInputViewBoundsChangeQuirk,
        } },

    { .match = URLMatch::host("outlook.live.com"_s),
        .behaviors = {
            // outlook.live.com: rdar://136624720
            NeedsMozillaFileTypeForDataTransferQuirk,
            // outlook.live.com: rdar://152277211
            MayNeedToIgnoreContentObservation,
        },
        .site = QuirkSite::Outlook },

    // outlook.live.com rdar://48008837
    { .match = URLMatch::host("outlook.live.com"_s),
        .behaviors = { ShouldPreventDispatchOfTouchEventQuirk },
        .availableWhen = touchEvents },

    // Microsoft office online generates data URLs with incorrect padding on Safari only (rdar://114573089).
    { .match = URLMatch::hostOrSubdomainOf("officeapps.live.com"_s),
        .behaviors = { ShouldDisableDataURLPaddingValidation } },
    { .match = URLMatch::hostOrSubdomainOf("onedrive.live.com"_s),
        .behaviors = { ShouldDisableDataURLPaddingValidation } },

    // onedrive.live.com rdar://26013388
    { .match = URLMatch::host("onedrive.live.com"_s),
        .behaviors = { IsNeverRichlyEditableForTouchBarQuirk } },

    // madisoncity.k12.al.us https://bugs.webkit.org/show_bug.cgi?id=296989
    { .match = URLMatch::domain("madisoncity.k12.al.us"_s),
        .behaviors = { NeedsFormControlToBeMouseFocusableQuirk } },

    // mailchimp.com rdar://47868965
    { .match = URLMatch::domain("mailchimp.com"_s),
        .behaviors = { ShouldDisablePointerEventsQuirk } },

    { .match = URLMatch::domain("marcus.com"_s),
        .behaviors = {
            // Marcus: <rdar://101086391>.
            ShouldExposeShowModalDialog,
            // marcus.com rdar://102959860
            ShouldNavigatorPluginsBeEmpty,
        } },

    // medium.com rdar://50457837
    { .match = URLMatch::domain("medium.com"_s),
        .behaviors = { ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk } },

    // m365.cloud.microsoft rdar://157794706
    { .match = URLMatch::hostOrSubdomainOf("m365.cloud.microsoft"_s),
        .behaviors = { ShouldAllowPopupFromMicrosoftOfficeToOneDrive } },

    // safe.menlosecurity.com rdar://135114489
    { .match = URLMatch::host("safe.menlosecurity.com"_s),
        .behaviors = { ShouldDisableWritingSuggestionsByDefaultQuirk } },

    { .match = URLMatch::domain("messenger.com"_s),
        .behaviors = {
            // facebook.com rdar://158736355
            ShouldEnableCameraAndMicrophonePermissionStateQuirk,
            ShouldEnableRemoteTrackLabelQuirk,
            // facebook.com rdar://161269819
            ShouldEnableEnumerateDeviceQuirk,
            // facebook.com rdar://158736355
            ShouldEnableRTCEncodedStreamsQuirk,
        } },

    // rdar://147429596
    { .match = URLMatch::domain("nba.com"_s),
        .behaviors = {
            NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk,
        },
        .site = QuirkSite::NBA,
        .availableWhen = iOSFamily },

    { .match = URLMatch::domain("nba.com"_s).when(smallScreen()),
        .behaviors = { ShouldEnterNativeFullscreenWhenCallingElementRequestFullscreen },
        .availableWhen = iOS },

    // mybinder.org rdar://51770057
    { .match = URLMatch::domain("mybinder.org"_s),
        .behaviors = { ShouldDispatchSimulatedMouseEventsQuirk },
        .site = QuirkSite::MyBinder,
        .availableWhen = touchEvents || touchEventRegions },

    // naver.com rdar://48068610
    { .match = URLMatch::hostOrSubdomainOf("naver.com"_s).exceptWhen(hostIs(naverHostsWithoutSimulatedMouseEvents)),
        .behaviors = { ShouldDispatchSimulatedMouseEventsQuirk } },

    { .match = URLMatch::domain("netflix.com"_s),
        .behaviors = {
            // netflix.com https://bugs.webkit.org/show_bug.cgi?id=173030
            NeedsSeekingSupportDisabledQuirk,
            // netflix.com rdar://178545839
            NeedsNetflixVolumeSliderQuirk,
            // netflix.com https://bugs.webkit.org/show_bug.cgi?id=304608
            ShouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick,
        },
        .site = QuirkSite::Netflix },

    { .match = URLMatch::domain("netflix.com"_s),
        .behaviors = { NeedsNowPlayingFullscreenSwapQuirk },
        .availableWhen = vision },

    { .match = URLMatch::domain("nfl.com"_s),
        .behaviors = { ShouldSuppressHLSSubtitles } },

    { .match = URLMatch::domain("nhl.com"_s),
        .behaviors = { NeedsWebKitMediaTextTrackDisplayQuirk } },

    // nytimes.com: rdar://problem/5976384
    { .match = URLMatch::domain("nytimes.com"_s),
        .behaviors = { ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting },
        .availableWhen = iOS || vision },

    // Pandora: <rdar://100243111>.
    { .match = URLMatch::domain("pandora.com"_s),
        .behaviors = { ShouldExposeShowModalDialog } },

    // pinterest.com rdar://104979314
    // FIXME: Remove this Quirk if Pinterest decides to trigger this notification from an user gesture (rdar://165745719)
    { .match = URLMatch::domain("pinterest.com"_s),
        .behaviors = { ShouldAllowNotificationPermissionWithoutUserGesture } },

    { .match = URLMatch::domain("premierleague.com"_s),
        .behaviors = {
            // premierleague.com: rdar://123721211
            ShouldIgnorePlaysInlineRequirementQuirk,
            // premierleague.com: rdar://68938833
            ShouldDispatchPlayPauseEventsOnResume,
            // premierleague.com: rdar://136791737
            ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
        } },

    // ralphlauren.com rdar://55629493
    { .match = URLMatch::domain("ralphlauren.com"_s),
        .behaviors = { ShouldIgnoreAriaForFastPathContentObservationCheckQuirk } },

    // reddit.com: rdar://80550715
    { .match = URLMatch::domain("reddit.com"_s),
        .behaviors = { RequiresUserGestureToPauseInPictureInPictureQuirk },
        .site = QuirkSite::Reddit,
        .availableWhen = videoPresentationMode || iOSFamily },

    // reddit.com with Sink It extension: rdar://176377447.
    { .match = URLMatch::domain("reddit.com"_s),
        .behaviors = { ShouldDisableScrollAnchoringQuirk },
        .availableWhen = iOSFamily },

    // FIXME: Remove this quirk when <rdar://problem/61733101> is complete.
    { .match = URLMatch::hostOrSubdomainOf("roblox.com"_s),
        .behaviors = { NeedsIPadMiniUserAgentQuirk } },

    { .match = URLMatch::domain("scribd.com"_s),
        .behaviors = { NeedsReuseLiveRangeForSelectionUpdateQuirk } },

    // sfusd.edu: rdar://116292738
    { .match = URLMatch::domain("sfusd.edu"_s),
        .behaviors = { ShouldBypassAsyncScriptDeferring } },

    // sharepoint.com rdar://52116170
    { .match = URLMatch::domain("sharepoint.com"_s),
        .behaviors = { ShouldAvoidResizingWhenInputViewBoundsChangeQuirk } },

    { .match = URLMatch::host("shopee.sg"_s).when(pathIs("/payment/account-linking/landing"_s)),
        .behaviors = { NeedsIPhoneUserAgentQuirk },
        .availableWhen = iOSFamily },

    { .match = URLMatch::domain("slack.com"_s),
        .behaviors = {
            // slack.com: rdar://138614711
            ShouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk,
            // slack.com: rdar://171190689
            ShouldUseDynamicViewportUnitsAsDefaultQuirk,
        },
        .availableWhen = iOSFamily },

    { .match = URLMatch::domain("soundcloud.com"_s),
        .behaviors = {
            // soundcloud.com rdar://52915981
            ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
            // Soundcloud: rdar://102913500
            ShouldExposeShowModalDialog,
            // soundcloud.com rdar://52915981
            ShouldDispatchSimulatedMouseEventsQuirk,
        },
        .site = QuirkSite::SoundCloud },

    // soylent.*: rdar://113314067
    { .match = URLMatch::anyTopLevelDomain("soylent"_s),
        .behaviors = { ShouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick } },

    // spotify.com rdar://138918575
    { .match = URLMatch::host("open.spotify.com"_s),
        .behaviors = {
            NeedsBodyScrollbarWidthNoneDisabledQuirk,
            ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
            ShouldLimitHLSPlaybackRate,
            NeedsWebKitMediaTextTrackDisplayQuirk,
            ShouldDeferIntersectionObserversDuringResize,
            ShouldBlockAudiblePlaybackWhileAudioIsPlaying,
            NeedsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk,
        } },

    // Remove this once rdar://142573562 is resolved.
    { .match = URLMatch::domain("steampowered.com"_s),
        .behaviors = { ShouldTreatAddingMouseOutEventListenerAsContentChange } },

    { .match = URLMatch::anyTopLevelDomain("theguardian"_s),
        .behaviors = { ShouldHideSoftTopScrollEdgeEffectDuringFocusQuirk } },

    // theguardian.com rdar://166727225
    { .match = QuirkURLMatch::embeddedDocumentInTopMatch(URLMatch::anyTopLevelDomain("theguardian"_s), URLMatch::domain(youTubeEmbedDomains)),
        .behaviors = { NeedsYouTubeEmbedAutoplayQuirk } },

    // teams.live.com rdar://88678598
    // teams.microsoft.com rdar://90434296
    { .match = URLMatch::host(microsoftTeamsHosts),
        .behaviors = { ShouldAllowMSTeamsProtocolWithoutUserGestureQuirk } },

    // teams.microsoft.com https://bugs.webkit.org/show_bug.cgi?id=219505
    { .match = URLMatch::host("teams.microsoft.com"_s).when(queryContains("Retried+3+times+without+success"_s)),
        .behaviors = { IsMicrosoftTeamsRedirectURLQuirk } },

    { .match = URLMatch::domain("thesaurus.com"_s),
        .behaviors = { NeedsAnchorToBeMouseFocusableQuirk },
        .site = QuirkSite::Thesaurus },

    { .match = URLMatch::domain("thesaurus.com"_s),
        .behaviors = { NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk },
        .availableWhen = iOSFamily },

    { .match = URLMatch::domain("tiktok.com"_s),
        .behaviors = {
            NeedsTikTokOverflowingContentQuirk,
            // tiktok.com rdar://174179805
            ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk,
            // tiktok.com rdar://174179805
            ShouldDispatchSimulatedMouseEventsQuirk,
        },
        .site = QuirkSite::TikTok },

    // trix-editor.org rdar://28242210
    { .match = URLMatch::domain("trix-editor.org"_s),
        .behaviors = { IsNeverRichlyEditableForTouchBarQuirk } },

    // twitch.tv rdar://102420527
    { .match = URLMatch::domain("twitch.tv"_s),
        .behaviors = { ShouldReportDocumentAsVisibleIfActivePIPQuirk } },

    // https://tympanus.net/Tutorials/WebGPUFluid/ does not load (rdar://143839620).
    { .match = URLMatch::domain("tympanus.net"_s),
        .behaviors = { ShouldBlockFetchWithNewlineAndLessThan } },

    // uhc.com rdar://173206598
    { .match = URLMatch::domain("uhc.com"_s),
        .behaviors = { ShouldTranscodeHeicImagesQuirk } },

    // unifi.ui.com rdar://180411019
    { .match = URLMatch::domain("ui.com"_s),
        .behaviors = { NeedsSupportsProgressMonitoringQuirk } },

    // Breaks express checkout on victoriassecret.com (rdar://104818312).
    { .match = URLMatch::domain("victoriassecret.com"_s),
        .behaviors = { ShouldDisableFetchMetadata } },

    { .match = URLMatch::domain("vimeo.com"_s),
        .behaviors = {
            // vimeo.com rdar://56996057
            MaybeBypassBackForwardCache,
            // vimeo.com rdar://55759025
            NeedsPreloadAutoQuirk,
            // vimeo.com: rdar://problem/73227900
            ShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk,
            // vimeo.com: rdar://107592139
            BlocksEnteringStandardFullscreenFromPictureInPictureQuirk,
            // vimeo.com: rdar://problem/70788878
            BlocksReturnToFullscreenFromPictureInPictureQuirk,
        },
        .site = QuirkSite::Vimeo },

    // rdar://116531089
    { .match = URLMatch::domain("vimeo.com"_s).when(smallScreen()),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    // walmart.com: rdar://123734840
    { .match = URLMatch::domain("walmart.com"_s),
        .behaviors = {
            MayNeedToIgnoreContentObservation,
        },
        .site = QuirkSite::Walmart,
        .availableWhen = twoPhaseClicks },

    // weather.com rdar://139689157
    { .match = URLMatch::domain("weather.com"_s),
        .behaviors = { NeedsFormControlToBeMouseFocusableQuirk } },

    { .match = URLMatch::domain("webex.com"_s),
        .behaviors = {
            NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk,
            // webex.com rdar://143715630
            NeedsWebExScrollabilityQuirk,
        },
        .site = QuirkSite::WebEx,
        .availableWhen = iOSFamily && desktopContentModeQuirks },

    // weebly.com rdar://48003980
    { .match = URLMatch::domain("weebly.com"_s),
        .behaviors = { ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk } },

    { .match = URLMatch::domain("wikipedia.org"_s),
        .behaviors = {
            // wikipedia.org rdar://54856323
            ShouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk,
            // wikipedia.org https://webkit.org/b/247636
            ShouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk,
        } },

    // rdar://170412045, https://bugs.webkit.org/show_bug.cgi?id=307933
    // wix.com rdar://49124313, except while picking a template.
    { .match = URLMatch::domain("wix.com"_s).exceptWhen(pathStartsWith("/website/templates/"_s)),
        .behaviors = { ShouldDispatchSimulatedMouseEventsQuirk } },

    { .match = URLMatch::domain("workspaces.xyz"_s),
        .behaviors = { ShouldComparareUsedValuesForBorderWidthForTriggeringTransitions } },

    // wpdevelopment.ca rdar://156109518
    { .match = URLMatch::domain("wpdevelopment.ca"_s),
        .behaviors = { NeedsFormControlToBeMouseFocusableQuirk } },

    { .match = URLMatch::domain("x.com"_s),
        .behaviors = {
            // x.com: rdar://132850672
            ShouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk,
            // rdar://121473410
            ShouldSilenceMediaQueryListChangeEvents,
            // x.com: rdar://73369869
            RequiresUserGestureToLoadInPictureInPictureQuirk,
            // x.com: rdar://73369869
            RequiresUserGestureToPauseInPictureInPictureQuirk,
        } },

    { .match = URLMatch::domain("x.com"_s),
        .behaviors = {
            // x.com: rdar://problem/58804852 and rdar://problem/61731801
            ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting,
            // x.com: rdar://175565114
            ShouldAvoidProgrammaticScrollClampingQuirk,
        },
        .availableWhen = iOS || vision },

    { .match = URLMatch::anyTopLevelDomain("yahoo"_s),
        .behaviors = {
            // yahoo.com: rdar://170502516
            NeedsYahooVolumeSliderQuirk,
            // yahoo.com: rdar://136767005
            ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor,
        } },

    // yahoo.com: rdar://148284059
    { .match = QuirkURLMatch::embeddedDocumentInTopMatch(URLMatch::anyTopLevelDomain("yahoo"_s), URLMatch::domain("yimg.com"_s)),
        .behaviors = { RequiresUserGestureToPauseInFullscreenAfterOrientationChangeQuirk } },

    // yahoo.com : rdar://142894603
    { .match = URLMatch::anyTopLevelDomain("yahoo"_s),
        .behaviors = { ShouldPreventDispatchOfTouchEventQuirk },
        .availableWhen = touchEvents },

    // news.ycombinator.com: rdar://127246368
    { .match = URLMatch::host("news.ycombinator.com"_s),
        .behaviors = { ShouldIgnoreTextAutoSizingQuirk } },

    { .match = URLMatch::domain("youtube.com"_s),
        .behaviors = {
            // youtube.com https://bugs.webkit.org/show_bug.cgi?id=195598
            HasBrokenEncryptedMediaAPISupportQuirk,
            // youtube.com rdar://135886305
            NeedsScrollbarWidthThinDisabledQuirk,
            NeedsYouTubeCaptionQuirk,
            // youtube.com: rdar://110097836
            ShouldSilenceResizeObservers,
        } },

    // Embedded youtube.com players need the caption quirk regardless of the embedding site.
    { .match = QuirkURLMatch::embeddedDocument(URLMatch::domain(youTubeEmbedDomains)),
        .behaviors = { NeedsYouTubeCaptionQuirk } },

    // YouTube.com does not provide AirPlay controls in fullscreen
    // (Ref: rdar://121471373)
    { .match = URLMatch::domain("youtube.com"_s).when(smallScreen()),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    // tiny. (Ref: rdar://121471373, rdar://121473410)
    { .match = QuirkURLMatch::embeddedDocument(URLMatch::domain(youTubeEmbedDomains).when(smallScreen())),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    { .match = QuirkURLMatch::embeddedDocument(URLMatch::domain("x.com"_s)),
        .behaviors = { ShouldDisableElementFullscreenQuirk } },

    // youtube.com rdar://49582231
    { .match = URLMatch::host("www.youtube.com"_s),
        .behaviors = { NeedsYouTubeOverflowScrollQuirk } },

    { .match = URLMatch::domain("youtube.com"_s).when(tubularApp()),
        .behaviors = { ShouldSuppressMediaSessionPauseActionOnInterruption },
        .availableWhen = iOSFamily },

    // www.youtube.com rdar://52361019
    { .match = URLMatch::host("www.youtube.com"_s),
        .behaviors = { NeedsYouTubeMouseOutQuirk } },

    // Lens.app rdar://178769976
    { .match = URLMatch::domain("youtube.com"_s).when(lensApp()),
        .behaviors = { RequiresUserGestureToPlayInFullscreenQuirk },
        .availableWhen = vision },

    // zencastr.com rdar://143087016
    { .match = URLMatch::domain("zencastr.com"_s),
        .behaviors = { NeedsLimitedMatroskaSupportQuirk } },

    // zillow.com rdar://53103732
    { .match = URLMatch::host("www.zillow.com"_s),
        .behaviors = { ShouldAvoidScrollingWhenFocusedContentIsVisibleQuirk } },

    { .match = URLMatch::domain("zillow.com"_s),
        .behaviors = {
            // zillow.com rdar://79872092
            ShouldTranscodeHeicImagesQuirk,
            // zillow.com rdar://110097836
            ShouldSilenceResizeObservers,
        } },

    { .match = URLMatch::domain("zomato.com"_s),
        .behaviors = { NeedsZomatoEmailLoginLabelQuirk } },

    { .match = URLMatch::domain("zoom.us"_s),
        .behaviors = {
            // zoom.com https://bugs.webkit.org/show_bug.cgi?id=223180
            ShouldAutoplayWebAudioForArbitraryUserGestureQuirk,
            // zoom.us rdar://118185086
            ShouldDisableImageCaptureQuirk,
            ShouldAllowMediaStreamTrackSerializationQuirk,
        } },
};

consteval bool shouldEmit(const Quirk& quirk)
{
    if (!quirk.availableWhen)
        return false;

    if (quirk.site)
        return true;

    auto usable = quirk.behaviors.bits();
    usable.exclude(unavailableQuirks);
    return !usable.isEmpty();
}

consteval size_t emittedQuirkCount()
{
    return std::ranges::count_if(fullTable, shouldEmit);
}

consteval auto emittedQuirkIndices()
{
    std::array<size_t, emittedQuirkCount()> indices { };
    size_t index = 0;
    size_t next = 0;
    for (auto& quirk : fullTable) {
        if (shouldEmit(quirk))
            indices[next++] = index;
        ++index;
    }
    return indices;
}

consteval Quirk maskedQuirk(const Quirk& quirk)
{
    Quirk masked = quirk;
    masked.behaviors.exclude(unavailableQuirks);
    return masked;
}

template<size_t... indices> consteval auto prunedTable(std::index_sequence<indices...>)
{
    constexpr auto emitted = emittedQuirkIndices();
    constexpr auto quirks = std::span { fullTable };
    return std::array<Quirk, sizeof...(indices)> { maskedQuirk(quirks[emitted[indices]])... };
}

static constexpr auto table = prunedTable(std::make_index_sequence<emittedQuirkCount()> { });

} // namespace SiteSpecificQuirks

bool QuirkURLMatch::matches(const URLMatchContext& topContext, const URLMatchContext& documentContext, IsTopDocument isTopDocument) const
{
    switch (m_kind) {
    case Kind::TopURL:
        return m_match.matches(topContext);
    case Kind::EmbeddedDocument:
        if (isTopDocument == IsTopDocument::Yes)
            return false;
        return m_match.matches(documentContext);
    case Kind::EmbeddedDocumentInTopURL:
        if (isTopDocument == IsTopDocument::Yes)
            return false;
        ASSERT(m_topMatch);
        return m_topMatch->matches(topContext) && m_match.matches(documentContext);
    }

    ASSERT_NOT_REACHED();
    return false;
}

void Quirk::apply(QuirksData& quirksData) const
{
    quirksData.activeQuirks.merge(behaviors.bits());

    if (site)
        quirksData.addSite(*site);
}

static QuirksData resolveSiteSpecificQuirks(const URLMatchContext& topContext, const URLMatchContext& documentContext, IsTopDocument documentIsTopDocument)
{
    QuirksData quirksData;
    for (auto& quirk : SiteSpecificQuirks::table) {
        if (quirk.match.matches(topContext, documentContext, documentIsTopDocument))
            quirk.apply(quirksData);
    }

    return quirksData;
}

QuirksData resolveSiteSpecificQuirks(const URL& topURL, const URL& documentURL, IsTopDocument documentIsTopDocument)
{
    return resolveSiteSpecificQuirks(URLMatchContext { topURL }, URLMatchContext { documentURL }, documentIsTopDocument);
}

QuirksData resolveTopURLQuirks(const URL& url)
{
    URLMatchContext context { url };
    return resolveSiteSpecificQuirks(context, context, IsTopDocument::Yes);
}

} // namespace WebCore
