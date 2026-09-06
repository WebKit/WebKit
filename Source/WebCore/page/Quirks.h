/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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

#include <WebCore/Event.h>
#include <WebCore/QuirkTable.h>
#include <WebCore/QuirksData.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/UserAgent.h>
#include <optional>
#include <wtf/Forward.h>
#include <wtf/Platform.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class Document;
class Element;
class EventListener;
class EventTarget;
class EventTypeInfo;
class HTMLElement;
class HTMLVideoElement;
class KeyframeEffect;
class LayoutUnit;
class LocalFrame;
class Node;
class NodeList;
class PlatformMouseEvent;
class ResourceRequest;
class SecurityOriginData;
class WeakPtrImplWithEventTargetData;

enum class IsSyntheticClick : bool;
enum class StorageAccessWasGranted : uint8_t;
enum class UserAgentType;

namespace Style {
class ComputedStyle;
}

class Quirks {
    WTF_MAKE_TZONE_ALLOCATED(Quirks);
    WTF_MAKE_NONCOPYABLE(Quirks);
public:
    Quirks(Document&);
    ~Quirks();

    bool NODELETE hasRelevantQuirks() const;

    bool shouldSilenceResizeObservers() const;
    bool shouldSilenceWindowResizeEventsDuringApplicationSnapshotting() const;
    bool shouldDeferIntersectionObserversDuringResize() const;
    bool shouldSilenceMediaQueryListChangeEvents() const;
    bool shouldIgnoreInvalidSignal() const;
    bool needsAnchorToBeMouseFocusable() const;
    bool needsFormControlToBeMouseFocusable() const;
    bool needsAutoplayPlayPauseEvents() const;
    bool needsSeekingSupportDisabled() const;
    bool needsPerDocumentAutoplayBehavior() const;
    bool needsExpediaGroupAnimationQuirk(Element&) const;
    bool shouldAutoplayWebAudioForArbitraryUserGesture() const;
    bool hasBrokenEncryptedMediaAPISupportQuirk() const;
#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
    bool shouldDispatchSimulatedMouseEvents(const EventTarget*) const;
    bool shouldPreventDispatchOfTouchEvent(const AtomString&, EventTarget*) const;
#endif
#if ENABLE(TOUCH_EVENTS)
    bool shouldDispatchedSimulatedMouseEventsAssumeDefaultPrevented(EventTarget*) const;
    bool shouldComputeSimulatedMouseEventMovementDelta() const;
#endif
    bool NODELETE shouldDisablePointerEventsQuirk() const;
    bool NODELETE needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommand() const;
    WEBCORE_EXPORT bool NODELETE inputMethodUsesCorrectKeyEventOrder() const;
    WEBCORE_EXPORT bool inputMethodMustUseCompositionEvents() const;
    bool shouldExposeShowModalDialog() const;
    bool NODELETE shouldIgnoreInputModeNone() const;
    bool NODELETE shouldNavigatorPluginsBeEmpty() const;
    bool returnNullPictureInPictureElementDuringFullscreenChange() const;

    bool shouldPreventOrientationMediaQueryFromEvaluatingToLandscape() const;
    bool NODELETE shouldFlipScreenDimensions() const;
    bool shouldAvoidProgrammaticScrollClamping() const;

    WEBCORE_EXPORT bool shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const;
    WEBCORE_EXPORT bool NODELETE shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreas() const;
    WEBCORE_EXPORT bool isTouchBarUpdateSuppressedForHiddenContentEditable() const;
    WEBCORE_EXPORT bool isNeverRichlyEditableForTouchBar() const;
    WEBCORE_EXPORT bool shouldAvoidResizingWhenInputViewBoundsChange() const;
    WEBCORE_EXPORT bool shouldAvoidScrollingWhenFocusedContentIsVisible() const;
    WEBCORE_EXPORT bool shouldUseLayoutViewportForClientRects() const;
    WEBCORE_EXPORT bool shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation() const;
    WEBCORE_EXPORT bool NODELETE shouldIgnoreAriaForFastPathContentObservationCheck() const;
    WEBCORE_EXPORT bool NODELETE shouldIgnoreViewportArgumentsToAvoidExcessiveZoom() const;
    WEBCORE_EXPORT bool NODELETE shouldIgnoreViewportArgumentsToAvoidEnlargedView() const;
    WEBCORE_EXPORT bool shouldUseDynamicViewportUnitsAsDefault() const;
    WEBCORE_EXPORT bool shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraints() const;
    WEBCORE_EXPORT bool shouldAllowNotificationPermissionWithoutUserGesture() const;
    WEBCORE_EXPORT static bool shouldAllowNavigationToCustomProtocolWithoutUserGesture(StringView protocol, const SecurityOriginData& requesterOrigin);

    WEBCORE_EXPORT bool needsYouTubeCaptionsQuirk() const;
    bool needsYouTubeEmbedAutoplayQuirk() const;
    WEBCORE_EXPORT bool NODELETE needsYouTubeMouseOutQuirk() const;

    WEBCORE_EXPORT bool needsCNNCaptionQuirk() const;

    WEBCORE_EXPORT bool shouldDisableWritingSuggestionsByDefault() const;

    WEBCORE_EXPORT static void updateStorageAccessUserAgentStringQuirks(HashMap<RegistrableDomain, String>&&);
    WEBCORE_EXPORT String storageAccessUserAgentStringQuirkForDomain(const URL&);
    WEBCORE_EXPORT static bool needsIPadMiniUserAgent(const URL&);
    WEBCORE_EXPORT static bool needsIPhoneUserAgent(const URL&);
    WEBCORE_EXPORT static bool NODELETE needsDesktopUserAgent(const URL&);
    WEBCORE_EXPORT static std::optional<String> needsCustomUserAgentOverride(const URL&, const String& applicationNameForUserAgent, const String& currentUserAgent);

    WEBCORE_EXPORT static bool needsPartitionedCookies(const ResourceRequest&);

    WEBCORE_EXPORT static std::optional<Vector<HashSet<String>>> NODELETE defaultVisibilityAdjustmentSelectors(const URL&);

    WEBCORE_EXPORT bool static NODELETE shouldDisableBlobFileAccessEnforcement();

    bool shouldAllowMixedContentConnectionToLoopback(const URL&);

    bool NODELETE needsGMailOverflowScrollQuirk() const;
    bool NODELETE needsYouTubeOverflowScrollQuirk() const;
    bool NODELETE needsWebExScrollabilityQuirk() const;
    bool NODELETE needsFullscreenDisplayNoneQuirk() const;
    bool NODELETE needsFullscreenObjectFitQuirk() const;
    bool needsZomatoEmailLoginLabelQuirk() const;
    bool NODELETE needsGoogleMapsScrollingQuirk() const;
    bool NODELETE needsGoogleTranslateScrollingQuirk() const;
    bool NODELETE needsNetflixVolumeSliderQuirk() const;
    bool needsGeforcenowWarningDisplayNoneQuirk() const;

    bool needsYahooVolumeSliderQuirk() const;

    bool needsFacebookRemoveNotSupportedQuirk() const;

    bool needsScrollbarWidthThinDisabledQuirk() const;
    bool needsBodyScrollbarWidthNoneDisabledQuirk() const;
    bool needsAirIndiaExpressLayeringQuirk() const;

    bool NODELETE shouldOpenAsAboutBlank(const String&) const;

    bool NODELETE needsPreloadAutoQuirk() const;

    bool NODELETE needsSuppressedPauseEventOnFullscreenExitQuirk() const;

    bool shouldBypassBackForwardCache() const;
    bool shouldBypassAsyncScriptDeferring() const;

    static bool shouldMakeEventListenerPassive(const EventTarget&, const EventTypeInfo&);

    WEBCORE_EXPORT static bool shouldTranscodeHeicImagesForURL(const URL&);

    bool shouldEnableFacebookFlagQuirk() const;
    Ref<NodeList> applyFacebookFlagQuirk(Document&, NodeList&);
    bool shouldEnableLegacyGetUserMediaQuirk() const;
    bool shouldDisableImageCaptureQuirk() const;
    bool shouldAllowMediaStreamTrackSerializationQuirk() const;
    bool shouldEnableSpeakerSelectionPermissionsPolicyQuirk() const;
    bool shouldEnableEnumerateDeviceQuirk() const;
    bool shouldEnableCameraAndMicrophonePermissionStateQuirk() const;
    bool shouldEnableRemoteTrackLabelQuirk() const;
    bool shouldEnableCameraBackgroundPlayback() const;
    bool shouldEnableRTCEncodedStreamsQuirk() const;

    bool shouldUnloadHeavyFrame() const;

    bool needsCanPlayAfterSeekedQuirk() const;

    bool shouldNotAutoUpgradeToHTTPSNavigation(const URL&);

    enum StorageAccessResult : bool { ShouldNotCancelEvent, ShouldCancelEvent };
    enum ShouldDispatchClick : bool { No, Yes };

    void triggerOptionalStorageAccessIframeQuirk(const URL& frameURL, CompletionHandler<void()>&&) const;
    StorageAccessResult triggerOptionalStorageAccessQuirk(Element&, const PlatformMouseEvent&, const AtomString& eventType, int, Element*, bool isParentProcessAFullWebBrowser, IsSyntheticClick) const;
    void setSubFrameDomainsForStorageAccessQuirk(Vector<RegistrableDomain>&& domains) { m_subFrameDomainsForStorageAccessQuirk = WTF::move(domains); }
    const Vector<RegistrableDomain>& subFrameDomainsForStorageAccessQuirk() const LIFETIME_BOUND { return m_subFrameDomainsForStorageAccessQuirk; }

    bool requiresUserGestureToPauseInPictureInPicture() const;
    bool requiresUserGestureToLoadInPictureInPicture() const;
    bool requiresUserGestureToPlayInFullscreen() const;
    bool requiresUserGestureToPauseInFullscreenAfterOrientationChange() const;

    WEBCORE_EXPORT bool blocksReturnToFullscreenFromPictureInPictureQuirk() const;
    WEBCORE_EXPORT bool blocksEnteringStandardFullscreenFromPictureInPictureQuirk() const;
    bool shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk() const;

    static bool isMicrosoftTeamsRedirectURL(const URL&);
    static bool hasStorageAccessForAllLoginDomains(const HashSet<RegistrableDomain>&, const RegistrableDomain&);
    StorageAccessResult requestStorageAccessAndHandleClick(CompletionHandler<void(ShouldDispatchClick)>&&) const;

    WEBCORE_EXPORT static bool shouldOmitTouchEventDOMAttributesForDesktopWebsite(const URL&);
    bool shouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick() const;

    WEBCORE_EXPORT void setTopDocumentURLForTesting(URL&&);

    static bool shouldOmitHTMLDocumentSupportedPropertyNames();

    WEBCORE_EXPORT Vector<String> activeQuirks() const;

    WEBCORE_EXPORT bool allowLayeredFullscreenVideos() const;
    bool shouldEnableFontLoadingAPIQuirk() const;
    bool needsVideoShouldMaintainAspectRatioQuirk() const;

    bool shouldIgnoreTextAutoSizing() const;

    WEBCORE_EXPORT bool shouldDisableFullscreenVideoAspectRatioAdaptiveSizing() const;

    WEBCORE_EXPORT bool shouldDisableAdSkippingInPip() const;
    bool shouldDisableLazyIframeLoadingQuirk() const;
    bool shouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk() const;

    bool shouldBlockFetchWithNewlineAndLessThan() const;
    bool shouldDisableFetchMetadata() const;
    bool shouldDisablePushStateFilePathRestrictions() const;

    bool shouldDisableScrollAnchoringQuirk() const;

    void setNeedsConfigurableIndexedPropertiesQuirk() { m_needsConfigurableIndexedPropertiesQuirk = true; }
    bool needsConfigurableIndexedPropertiesQuirk() const;

    // webkit.org/b/259091.
    bool needsToCopyUserSelectNoneQuirk() const { return m_needsToCopyUserSelectNoneQuirk; }
    void setNeedsToCopyUserSelectNoneQuirk() { m_needsToCopyUserSelectNoneQuirk = true; }

    String advancedPrivacyProtectionSubstituteDataURLForScriptWithFeatures(const String& lastDrawnText, int canvasWidth, int canvasHeight) const;

    bool NODELETE needsResettingTransitionCancelsRunningTransitionQuirk() const;

    bool shouldDisableDataURLPaddingValidation() const;

    bool needsDisableDOMPasteAccessQuirk() const;

    bool NODELETE shouldDisableElementFullscreenQuirk() const;
    bool NODELETE shouldIgnorePlaysInlineRequirementQuirk() const;

    bool shouldAllowPopupFromMicrosoftOfficeToOneDrive() const { return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldAllowPopupFromMicrosoftOfficeToOneDrive); }
    bool needsPopupFromMicrosoftOfficeToOneDrive(const URL& targetURL) const;

    WEBCORE_EXPORT bool needsConsistentQueryParameterFilteringQuirk(const URL&) const;
    bool mayBenefitFromFingerprintingProtectionQuirk(const URL&) const;
    static String standardUserAgentWithApplicationNameIncludingCompatOverrides(const String&, const String&, UserAgentType);

    String scriptToEvaluateBeforeRunningScriptFromURL(const URL&);

    bool NODELETE shouldHideCoarsePointerCharacteristics() const;

    bool implicitMuteWhenVolumeSetToZero() const;

    bool needsZeroMaxTouchPointsQuirk() const;
    bool needsChromeMediaControlsPseudoElement() const;

    WEBCORE_EXPORT bool shouldIgnoreContentObservationForClick(const Node&) const;

    WEBCORE_EXPORT bool shouldSynthesizeTouchEventsAfterNonSyntheticClick(const Element&) const;
#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT bool needsPointerTouchCompatibility(const Element&) const;
#endif
    WEBCORE_EXPORT bool shouldHideSoftTopScrollEdgeEffectDuringFocus(const Element&) const;

    bool needsAmazonDesignMenuViewportUnitQuirk(const Style::ComputedStyle&, const Style::ComputedStyle& parentStyle) const;
    bool needsClaudeSidebarViewportUnitQuirk(Element&, const Style::ComputedStyle&) const;
    WEBCORE_EXPORT bool needsHideSelectionDuringOverflowScrollQuirk() const;
    bool needsChromeOSNavigatorUserAgentQuirk(const Document&) const;

    bool shouldTreatAddingMouseOutEventListenerAsContentChange() const;

    bool needsMozillaFileTypeForDataTransfer() const;

    WEBCORE_EXPORT bool shouldAvoidStartingSelectionOnMouseDownOverPointerCursor(const Node&) const;

    bool shouldReuseLiveRangeForSelectionUpdate() const;

    bool NODELETE needsFacebookStoriesCreationFormQuirk(const Element&, const Style::ComputedStyle&) const;

    bool needsLimitedMatroskaSupport() const;
    bool needsSupportsProgressMonitoring() const;

    bool needsCustomUserAgentData() const;
    bool needsNavigatorUserAgentDataQuirk() const;

    WEBCORE_EXPORT bool needsNowPlayingFullscreenSwapQuirk() const;

    enum class TikTokOverflowingContentQuirkType : bool { VideoSectionQuirk, CommentsSectionQuirk };
    std::optional<TikTokOverflowingContentQuirkType> needsTikTokOverflowingContentQuirk(const Element&, const Style::ComputedStyle& parentStyle) const;

    bool needsInstagramResizingReelsQuirk(const Element&, const Style::ComputedStyle& elementStyle, const Style::ComputedStyle& parentStyle) const;

    bool needsWebKitMediaTextTrackDisplayQuirk() const;

    bool NODELETE shouldSupportHoverMediaQueries() const;

    bool shouldRewriteMediaRangeRequestForURL(const URL&) const;
    bool shouldDelayReloadWhenRegisteringServiceWorker() const;

    bool NODELETE ensureCaptionVisibilityInFullscreenAndPictureInPicture() const;

    bool shouldPreventKeyframeEffectAcceleration(const KeyframeEffect&) const;
    bool shouldDisableThreadedAnimationsQuirk() const;

    bool shouldEnterNativeFullscreenWhenCallingElementRequestFullscreenQuirk() const;

    bool shouldDisableDOMAudioSessionQuirk() const;

    bool needsSuppressPostLayoutBoundaryEventsQuirk() const;

    bool shouldReportVisibleDueToActivePictureInPictureContent() const;

    bool shouldComparareUsedValuesForBorderWidthForTriggeringTransitions() const;

    bool shouldLimitHLSPlaybackRate() const;
    bool shouldSuppressHLSSubtitles() const;
    bool shouldBlockAudiblePlaybackWhileAudioIsPlaying() const;

    bool shouldSuppressMediaSessionPauseActionOnInterruption() const;

    void clearLogoutSurvivingIdentityCookiesIfNeeded(const URL& fetchURL, int httpStatusCode);

    void determineRelevantQuirks();
    void logQuirksToConsoleIfNecessary() const;

#if PLATFORM(IOS_FAMILY) && ENABLE(IOS_TOUCH_EVENTS)
    WEBCORE_EXPORT bool shouldAllowNativeTapsOnMediaElements(const Node*) const;
#endif

    bool NODELETE shouldSendFakeTouchForceChangeEvent() const;

    bool needsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk() const;

private:
    bool needsQuirks() const;
    URL topDocumentURL() const;

    bool elementMatchesQuirk(SiteSpecificQuirk, const Node*) const;

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
    mutable WeakPtr<const Element, WeakPtrImplWithEventTargetData> m_facebookStoriesCreationFormContainer;

    mutable QuirksData m_quirksData;
    Vector<QuirkElementCondition, 2> m_elementConditions;
    mutable QuirkBitSet m_probedQuirks;

    template<typename Probe>
    bool quirkIsEnabledAfterProbing(SiteSpecificQuirk quirk, NOESCAPE Probe&& probe) const
    {
        auto index = static_cast<size_t>(quirk);
        if (!m_probedQuirks.get(index)) {
            m_probedQuirks.set(index);
            m_quirksData.setQuirkState(quirk, probe());
        }
        return m_quirksData.quirkIsEnabled(quirk);
    }

    bool m_needsConfigurableIndexedPropertiesQuirk { false };
    bool m_needsToCopyUserSelectNoneQuirk { false };

    Vector<RegistrableDomain> m_subFrameDomainsForStorageAccessQuirk;
    URL m_topDocumentURLForTesting;
};

} // namespace WebCore
