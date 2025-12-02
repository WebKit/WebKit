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
#include <WebCore/QuirksData.h>
#include <WebCore/RegistrableDomain.h>
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
class RenderStyle;
class SecurityOriginData;
class WeakPtrImplWithEventTargetData;

enum class IsSyntheticClick : bool;
enum class StorageAccessWasGranted : uint8_t;
enum class UserAgentType;

class Quirks {
    WTF_MAKE_TZONE_ALLOCATED(Quirks);
    WTF_MAKE_NONCOPYABLE(Quirks);
public:
    Quirks(Document&);
    ~Quirks();

    bool hasRelevantQuirks() const;

    bool shouldSilenceResizeObservers() const;
    bool shouldSilenceWindowResizeEventsDuringApplicationSnapshotting() const;
    bool shouldSilenceMediaQueryListChangeEvents() const;
    bool shouldIgnoreInvalidSignal() const;
    bool needsFormControlToBeMouseFocusable() const;
    bool needsAutoplayPlayPauseEvents() const;
    bool needsSeekingSupportDisabled() const;
    bool needsPerDocumentAutoplayBehavior() const;
    bool needsHotelsAnimationQuirk(Element&, const RenderStyle&) const;
    bool shouldAutoplayWebAudioForArbitraryUserGesture() const;
    bool hasBrokenEncryptedMediaAPISupportQuirk() const;
#if ENABLE(TOUCH_EVENTS)
    bool shouldDispatchSimulatedMouseEvents(const EventTarget*) const;
    bool shouldDispatchedSimulatedMouseEventsAssumeDefaultPrevented(EventTarget*) const;
    bool shouldPreventDispatchOfTouchEvent(const AtomString&, EventTarget*) const;
#endif
    bool shouldDisablePointerEventsQuirk() const;
    bool needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommand() const;
    WEBCORE_EXPORT bool inputMethodUsesCorrectKeyEventOrder() const;
    bool shouldExposeShowModalDialog() const;
    bool shouldNavigatorPluginsBeEmpty() const;
    bool returnNullPictureInPictureElementDuringFullscreenChange() const;

    bool shouldPreventOrientationMediaQueryFromEvaluatingToLandscape() const;
    bool shouldFlipScreenDimensions() const;

    WEBCORE_EXPORT bool shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const;
    WEBCORE_EXPORT bool shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreas() const;
    WEBCORE_EXPORT bool isTouchBarUpdateSuppressedForHiddenContentEditable() const;
    WEBCORE_EXPORT bool isNeverRichlyEditableForTouchBar() const;
    WEBCORE_EXPORT bool shouldAvoidResizingWhenInputViewBoundsChange() const;
    WEBCORE_EXPORT bool shouldAvoidScrollingWhenFocusedContentIsVisible() const;
    WEBCORE_EXPORT bool shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation() const;
    WEBCORE_EXPORT bool shouldIgnoreAriaForFastPathContentObservationCheck() const;
    WEBCORE_EXPORT bool shouldIgnoreViewportArgumentsToAvoidExcessiveZoom() const;
    WEBCORE_EXPORT bool shouldIgnoreViewportArgumentsToAvoidEnlargedView() const;
    WEBCORE_EXPORT bool shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraints() const;
    WEBCORE_EXPORT static bool shouldAllowNavigationToCustomProtocolWithoutUserGesture(StringView protocol, const SecurityOriginData& requesterOrigin);

    WEBCORE_EXPORT bool needsYouTubeMouseOutQuirk() const;

    WEBCORE_EXPORT bool shouldDisableWritingSuggestionsByDefault() const;

    WEBCORE_EXPORT static void updateStorageAccessUserAgentStringQuirks(HashMap<RegistrableDomain, String>&&);
    WEBCORE_EXPORT String storageAccessUserAgentStringQuirkForDomain(const URL&);
    WEBCORE_EXPORT static bool needsIPadMiniUserAgent(const URL&);
    WEBCORE_EXPORT static bool needsIPhoneUserAgent(const URL&);
    WEBCORE_EXPORT static bool needsDesktopUserAgent(const URL&);
    WEBCORE_EXPORT static std::optional<String> needsCustomUserAgentOverride(const URL&, const String& applicationNameForUserAgent);

    WEBCORE_EXPORT static bool needsPartitionedCookies(const ResourceRequest&);

    WEBCORE_EXPORT static std::optional<Vector<HashSet<String>>> defaultVisibilityAdjustmentSelectors(const URL&);

    WEBCORE_EXPORT bool static shouldDisableBlobFileAccessEnforcement();

    bool needsGMailOverflowScrollQuirk() const;
    bool needsYouTubeOverflowScrollQuirk() const;
    bool needsFullscreenDisplayNoneQuirk() const;
    bool needsFullscreenObjectFitQuirk() const;
    bool needsZomatoEmailLoginLabelQuirk() const;
    bool needsGoogleMapsScrollingQuirk() const;
    bool needsGoogleTranslateScrollingQuirk() const;

    bool needsPrimeVideoUserSelectNoneQuirk() const;

    bool needsFacebookRemoveNotSupportedQuirk() const;

    bool needsScrollbarWidthThinDisabledQuirk() const;
    bool needsBodyScrollbarWidthNoneDisabledQuirk() const;

    bool shouldOpenAsAboutBlank(const String&) const;

    bool needsPreloadAutoQuirk() const;

    bool shouldBypassBackForwardCache() const;
    bool shouldBypassAsyncScriptDeferring() const;

    static bool shouldMakeEventListenerPassive(const EventTarget&, const EventTypeInfo&);

    WEBCORE_EXPORT static bool shouldTranscodeHeicImagesForURL(const URL&);

#if ENABLE(MEDIA_STREAM)
    bool shouldEnableFacebookFlagQuirk() const;
    Ref<NodeList> applyFacebookFlagQuirk(Document&, NodeList&);
    bool shouldEnableLegacyGetUserMediaQuirk() const;
    bool shouldDisableImageCaptureQuirk() const;
    bool shouldEnableSpeakerSelectionPermissionsPolicyQuirk() const;
    bool shouldEnableEnumerateDeviceQuirk() const;
    bool shouldEnableCameraAndMicrophonePermissionStateQuirk() const;
    bool shouldEnableRemoteTrackLabelQuirk() const;
#endif
#if ENABLE(WEB_RTC)
    bool shouldEnableRTCEncodedStreamsQuirk() const;
#endif

    bool shouldUnloadHeavyFrame() const;

    bool needsCanPlayAfterSeekedQuirk() const;

    bool shouldNotAutoUpgradeToHTTPSNavigation(const URL&);

    enum StorageAccessResult : bool { ShouldNotCancelEvent, ShouldCancelEvent };
    enum ShouldDispatchClick : bool { No, Yes };

    void triggerOptionalStorageAccessIframeQuirk(const URL& frameURL, CompletionHandler<void()>&&) const;
    StorageAccessResult triggerOptionalStorageAccessQuirk(Element&, const PlatformMouseEvent&, const AtomString& eventType, int, Element*, bool isParentProcessAFullWebBrowser, IsSyntheticClick) const;
    void setSubFrameDomainsForStorageAccessQuirk(Vector<RegistrableDomain>&& domains) { m_subFrameDomainsForStorageAccessQuirk = WTFMove(domains); }
    const Vector<RegistrableDomain>& subFrameDomainsForStorageAccessQuirk() const { return m_subFrameDomainsForStorageAccessQuirk; }

    bool needsVP9FullRangeFlagQuirk() const;

    bool requiresUserGestureToPauseInPictureInPicture() const;
    bool requiresUserGestureToLoadInPictureInPicture() const;

    WEBCORE_EXPORT bool blocksReturnToFullscreenFromPictureInPictureQuirk() const;
    WEBCORE_EXPORT bool blocksEnteringStandardFullscreenFromPictureInPictureQuirk() const;
    bool shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk() const;

    static bool isMicrosoftTeamsRedirectURL(const URL&);
    static bool hasStorageAccessForAllLoginDomains(const HashSet<RegistrableDomain>&, const RegistrableDomain&);
    StorageAccessResult requestStorageAccessAndHandleClick(CompletionHandler<void(ShouldDispatchClick)>&&) const;

#if ENABLE(TOUCH_EVENTS)
    WEBCORE_EXPORT static bool shouldOmitTouchEventDOMAttributesForDesktopWebsite(const URL&);
    bool shouldDispatchPointerOutAfterHandlingSyntheticClick() const;
#endif

    WEBCORE_EXPORT void setTopDocumentURLForTesting(URL&&);

    static bool shouldOmitHTMLDocumentSupportedPropertyNames();

#if PLATFORM(IOS) || PLATFORM(VISION)
    WEBCORE_EXPORT bool allowLayeredFullscreenVideos() const;
#endif
    bool shouldEnableFontLoadingAPIQuirk() const;
    bool needsVideoShouldMaintainAspectRatioQuirk() const;

#if ENABLE(TEXT_AUTOSIZING)
    bool shouldIgnoreTextAutoSizing() const;
#endif

#if PLATFORM(VISION)
    WEBCORE_EXPORT bool shouldDisableFullscreenVideoAspectRatioAdaptiveSizing() const;
#endif

#if HAVE(PIP_SKIP_PREROLL)
    WEBCORE_EXPORT bool shouldDisableAdSkippingInPip() const;
#endif
    bool shouldDisableLazyIframeLoadingQuirk() const;

    bool shouldBlockFetchWithNewlineAndLessThan() const;
    bool shouldDisableFetchMetadata() const;
    bool shouldDisablePushStateFilePathRestrictions() const;

    void setNeedsConfigurableIndexedPropertiesQuirk() { m_needsConfigurableIndexedPropertiesQuirk = true; }
    bool needsConfigurableIndexedPropertiesQuirk() const;

    // webkit.org/b/259091.
    bool needsToCopyUserSelectNoneQuirk() const { return m_needsToCopyUserSelectNoneQuirk; }
    void setNeedsToCopyUserSelectNoneQuirk() { m_needsToCopyUserSelectNoneQuirk = true; }

    bool shouldEnableCanvas2DAdvancedPrivacyProtectionQuirk() const;
    String advancedPrivacyProtectionSubstituteDataURLForScriptWithFeatures(const String& lastDrawnText, int canvasWidth, int canvasHeight) const;

    bool needsResettingTransitionCancelsRunningTransitionQuirk() const;

    bool shouldDisableDataURLPaddingValidation() const;

    bool needsDisableDOMPasteAccessQuirk() const;

    bool shouldDisableElementFullscreenQuirk() const;
    bool shouldIgnorePlaysInlineRequirementQuirk() const;
    WEBCORE_EXPORT bool shouldUseEphemeralPartitionedStorageForDOMCookies(const URL&) const;

#if PLATFORM(IOS_FAMILY)
    bool shouldAllowPopupFromMicrosoftOfficeToOneDrive() const { return m_quirksData.quirkIsEnabled(QuirksData::SiteSpecificQuirk::ShouldAllowPopupFromMicrosoftOfficeToOneDrive); }
    bool needsPopupFromMicrosoftOfficeToOneDrive(const URL& targetURL) const;
#endif

    bool needsLaxSameSiteCookieQuirk(const URL&) const;
    static String standardUserAgentWithApplicationNameIncludingCompatOverrides(const String&, const String&, UserAgentType);

    String scriptToEvaluateBeforeRunningScriptFromURL(const URL&);

    bool shouldHideCoarsePointerCharacteristics() const;

    bool implicitMuteWhenVolumeSetToZero() const;

    bool needsZeroMaxTouchPointsQuirk() const;
    bool needsChromeMediaControlsPseudoElement() const;

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT bool shouldIgnoreContentObservationForClick(const Node&) const;
    WEBCORE_EXPORT bool shouldSynthesizeTouchEventsAfterNonSyntheticClick(const Element&) const;
    WEBCORE_EXPORT bool needsPointerTouchCompatibility(const Element&) const;
    bool shouldTreatAddingMouseOutEventListenerAsContentChange() const;
    WEBCORE_EXPORT bool shouldHideSoftTopScrollEdgeEffectDuringFocus(const Element&) const;

    bool needsClaudeSidebarViewportUnitQuirk(Element&, const RenderStyle&) const;
#endif

    bool needsMozillaFileTypeForDataTransfer() const;

    WEBCORE_EXPORT bool shouldAvoidStartingSelectionOnMouseDownOverPointerCursor(const Node&) const;

    bool shouldReuseLiveRangeForSelectionUpdate() const;

    bool needsFacebookStoriesCreationFormQuirk(const Element&, const RenderStyle&) const;

    bool needsLimitedMatroskaSupport() const;

    bool needsCustomUserAgentData() const;
    bool needsNavigatorUserAgentDataQuirk() const;

    WEBCORE_EXPORT bool needsNowPlayingFullscreenSwapQuirk() const;


    enum class TikTokOverflowingContentQuirkType : bool { VideoSectionQuirk, CommentsSectionQuirk };
    std::optional<TikTokOverflowingContentQuirkType> needsTikTokOverflowingContentQuirk(const Element&, const RenderStyle& parentStyle) const;

    bool needsWebKitMediaTextTrackDisplayQuirk() const;

    bool shouldSupportHoverMediaQueries() const;

    bool shouldRewriteMediaRangeRequestForURL(const URL&) const;
    bool shouldDelayReloadWhenRegisteringServiceWorker() const;

    bool shouldPreventKeyframeEffectAcceleration(const KeyframeEffect&) const;

    bool shouldEnterNativeFullscreenWhenCallingElementRequestFullscreenQuirk() const;

    bool shouldDisableDOMAudioSessionQuirk() const;

    bool needsSuppressPostLayoutBoundaryEventsQuirk() const;

    bool shouldExposeCredentialsContainerQuirk() const;

    void determineRelevantQuirks();

private:
    bool needsQuirks() const;
    bool isDomain(const String&) const;
    bool domainStartsWith(const String&) const;
    bool isEmbedDomain(const String&) const;
    bool isYoutubeEmbedDomain() const;

    static bool domainNeedsAvoidResizingWhenInputViewBoundsChangeQuirk(const URL&, QuirksData&);
    static bool domainNeedsScrollbarWidthThinDisabledQuirk(const URL&, QuirksData&);
#if ENABLE(VIDEO_PRESENTATION_MODE)
    static bool domainShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk(const URL&, QuirksData&);
#endif

    RefPtr<Document> protectedDocument() const;

    URL topDocumentURL() const;

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
    mutable WeakPtr<const Element, WeakPtrImplWithEventTargetData> m_facebookStoriesCreationFormContainer;

    mutable QuirksData m_quirksData;

    bool m_needsConfigurableIndexedPropertiesQuirk { false };
    bool m_needsToCopyUserSelectNoneQuirk { false };

    Vector<RegistrableDomain> m_subFrameDomainsForStorageAccessQuirk;
    URL m_topDocumentURLForTesting;
};

} // namespace WebCore
