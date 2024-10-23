/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

#include "Event.h"
#include <optional>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class Document;
class Element;
class EventListener;
class EventTarget;
class EventTypeInfo;
class HTMLElement;
class HTMLVideoElement;
class LayoutUnit;
class LocalFrame;
class PlatformMouseEvent;
class RegistrableDomain;
class SecurityOriginData;
class WeakPtrImplWithEventTargetData;

enum class IsSyntheticClick : bool;
enum class StorageAccessWasGranted : uint8_t;

class Quirks {
    WTF_MAKE_TZONE_ALLOCATED(Quirks);
    WTF_MAKE_NONCOPYABLE(Quirks);
public:
    Quirks(Document&);
    ~Quirks();

    bool shouldSilenceResizeObservers() const;
    bool shouldSilenceWindowResizeEvents() const;
    bool shouldSilenceMediaQueryListChangeEvents() const;
    bool shouldIgnoreInvalidSignal() const;
    bool needsFormControlToBeMouseFocusable() const;
    bool needsAutoplayPlayPauseEvents() const;
    bool needsSeekingSupportDisabled() const;
    bool needsPerDocumentAutoplayBehavior() const;
    bool shouldAutoplayWebAudioForArbitraryUserGesture() const;
    bool hasBrokenEncryptedMediaAPISupportQuirk() const;
#if ENABLE(TOUCH_EVENTS)
    bool shouldDispatchSimulatedMouseEvents(const EventTarget*) const;
    bool shouldDispatchedSimulatedMouseEventsAssumeDefaultPrevented(EventTarget*) const;
    bool shouldPreventDispatchOfTouchEvent(const AtomString&, EventTarget*) const;
#endif
    bool shouldDisablePointerEventsQuirk() const;
    bool needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommand() const;
    bool shouldExposeShowModalDialog() const;
    bool shouldNavigatorPluginsBeEmpty() const;

    bool shouldPreventOrientationMediaQueryFromEvaluatingToLandscape() const;
    bool shouldFlipScreenDimensions() const;
    bool shouldAllowDownloadsInSpiteOfCSP() const;

    WEBCORE_EXPORT bool shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const;
    WEBCORE_EXPORT bool shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreas() const;
    WEBCORE_EXPORT bool isTouchBarUpdateSuppressedForHiddenContentEditable() const;
    WEBCORE_EXPORT bool isNeverRichlyEditableForTouchBar() const;
    WEBCORE_EXPORT bool shouldAvoidResizingWhenInputViewBoundsChange() const;
    WEBCORE_EXPORT bool shouldAvoidScrollingWhenFocusedContentIsVisible() const;
    WEBCORE_EXPORT bool shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation() const;
    WEBCORE_EXPORT bool shouldIgnoreAriaForFastPathContentObservationCheck() const;
    WEBCORE_EXPORT bool shouldIgnoreViewportArgumentsToAvoidExcessiveZoom() const;
    WEBCORE_EXPORT bool shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraints() const;
    WEBCORE_EXPORT static bool shouldAllowNavigationToCustomProtocolWithoutUserGesture(StringView protocol, const SecurityOriginData& requesterOrigin);

    WEBCORE_EXPORT bool needsYouTubeMouseOutQuirk() const;

    WEBCORE_EXPORT bool shouldDisableWritingSuggestionsByDefault() const;

    WEBCORE_EXPORT static void updateStorageAccessUserAgentStringQuirks(HashMap<RegistrableDomain, String>&&);
    WEBCORE_EXPORT String storageAccessUserAgentStringQuirkForDomain(const URL&);
    WEBCORE_EXPORT static bool needsIPadMiniUserAgent(const URL&);
    WEBCORE_EXPORT static bool needsIPhoneUserAgent(const URL&);
    WEBCORE_EXPORT static bool needsDesktopUserAgent(const URL&);

    WEBCORE_EXPORT static std::optional<Vector<HashSet<String>>> defaultVisibilityAdjustmentSelectors(const URL&);

    bool needsGMailOverflowScrollQuirk() const;
    bool needsIPadSkypeOverflowScrollQuirk() const;
    bool needsYouTubeOverflowScrollQuirk() const;
    bool needsFullscreenDisplayNoneQuirk() const;
    bool needsFullscreenObjectFitQuirk() const;
    bool needsWeChatScrollingQuirk() const;

    bool needsPrimeVideoUserSelectNoneQuirk() const;

    bool needsScrollbarWidthThinDisabledQuirk() const;

    bool shouldOpenAsAboutBlank(const String&) const;

    bool needsPreloadAutoQuirk() const;

    bool shouldBypassBackForwardCache() const;
    bool shouldBypassAsyncScriptDeferring() const;

    static bool shouldMakeEventListenerPassive(const EventTarget&, const EventTypeInfo&);

#if ENABLE(MEDIA_STREAM)
    bool shouldEnableLegacyGetUserMediaQuirk() const;
    bool shouldDisableImageCaptureQuirk() const;
#endif

    bool needsCanPlayAfterSeekedQuirk() const;

    bool shouldAvoidPastingImagesAsWebContent() const;

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
    bool shouldDelayFullscreenEventWhenExitingPictureInPictureQuirk() const;

    static bool isMicrosoftTeamsRedirectURL(const URL&);
    static bool hasStorageAccessForAllLoginDomains(const HashSet<RegistrableDomain>&, const RegistrableDomain&);
    StorageAccessResult requestStorageAccessAndHandleClick(CompletionHandler<void(ShouldDispatchClick)>&&) const;

#if ENABLE(TOUCH_EVENTS)
    WEBCORE_EXPORT static bool shouldOmitTouchEventDOMAttributesForDesktopWebsite(const URL&);
#endif

    static bool shouldOmitHTMLDocumentSupportedPropertyNames();

#if PLATFORM(IOS) || PLATFORM(VISION)
    WEBCORE_EXPORT bool allowLayeredFullscreenVideos() const;
#endif
    bool shouldEnableApplicationCacheQuirk() const;
    bool shouldEnableFontLoadingAPIQuirk() const;
    bool needsVideoShouldMaintainAspectRatioQuirk() const;

#if ENABLE(TEXT_AUTOSIZING)
    bool shouldIgnoreTextAutoSizing() const;
#endif

#if PLATFORM(VISION)
    WEBCORE_EXPORT bool shouldDisableFullscreenVideoAspectRatioAdaptiveSizing() const;
#endif

    bool shouldDisableLazyIframeLoadingQuirk() const;

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

    bool shouldStarBePermissionsPolicyDefaultValue() const;
    bool shouldDisableDataURLPaddingValidation() const;

    bool needsDisableDOMPasteAccessQuirk() const;

    bool shouldDisableElementFullscreenQuirk() const;
    bool shouldIgnorePlaysInlineRequirementQuirk() const;
    WEBCORE_EXPORT bool shouldUseEphemeralPartitionedStorageForDOMCookies(const URL&) const;

    bool needsGetElementsByNameQuirk() const;
    bool needsRelaxedCorsMixedContentCheckQuirk() const;
    bool needsLaxSameSiteCookieQuirk(const URL&) const;

    String scriptToEvaluateBeforeRunningScriptFromURL(const URL&);

    bool shouldHideCoarsePointerCharacteristics() const;

private:
    bool needsQuirks() const;
    bool isDomain(const String&) const;
    bool isEmbedDomain(const String&) const;
    bool isYoutubeEmbedDomain() const;

    bool isAmazon() const;
#if ENABLE(TOUCH_EVENTS)
    bool isGoogleMaps() const;
#endif

    RefPtr<Document> protectedDocument() const;

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;

    mutable std::optional<bool> m_hasBrokenEncryptedMediaAPISupportQuirk;
    mutable std::optional<bool> m_needsFullWidthHeightFullscreenStyleQuirk;
#if PLATFORM(IOS_FAMILY)
    mutable std::optional<bool> m_needsGMailOverflowScrollQuirk;
    mutable std::optional<bool> m_needsIPadSkypeOverflowScrollQuirk;
    mutable std::optional<bool> m_needsYouTubeOverflowScrollQuirk;
    mutable std::optional<bool> m_needsPreloadAutoQuirk;
    mutable std::optional<bool> m_needsFullscreenDisplayNoneQuirk;
    mutable std::optional<bool> m_needsFullscreenObjectFitQuirk;
    mutable std::optional<bool> m_shouldAvoidPastingImagesAsWebContent;
#endif
#if ENABLE(TOUCH_EVENTS)
    enum class ShouldDispatchSimulatedMouseEvents : uint8_t {
        Unknown,
        No,
        DependingOnTargetFor_mybinder_org,
        Yes,
    };
    mutable ShouldDispatchSimulatedMouseEvents m_shouldDispatchSimulatedMouseEventsQuirk { ShouldDispatchSimulatedMouseEvents::Unknown };
#endif
    mutable std::optional<bool> m_needsCanPlayAfterSeekedQuirk;
    mutable std::optional<bool> m_shouldBypassAsyncScriptDeferring;
    mutable std::optional<bool> m_needsVP9FullRangeFlagQuirk;
    mutable std::optional<bool> m_needsBlackFullscreenBackgroundQuirk;
    mutable std::optional<bool> m_requiresUserGestureToPauseInPictureInPicture;
    mutable std::optional<bool> m_requiresUserGestureToLoadInPictureInPicture;
#if ENABLE(MEDIA_STREAM)
    mutable std::optional<bool> m_shouldEnableLegacyGetUserMediaQuirk;
    mutable std::optional<bool> m_shouldDisableImageCaptureQuirk;
#endif
    mutable std::optional<bool> m_blocksReturnToFullscreenFromPictureInPictureQuirk;
    mutable std::optional<bool> m_blocksEnteringStandardFullscreenFromPictureInPictureQuirk;
    mutable std::optional<bool> m_shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk;
    mutable std::optional<bool> m_shouldDelayFullscreenEventWhenExitingPictureInPictureQuirk;
#if PLATFORM(IOS) || PLATFORM(VISION)
    mutable std::optional<bool> m_allowLayeredFullscreenVideos;
#endif
#if PLATFORM(IOS_FAMILY)
    mutable std::optional<bool> m_shouldEnableApplicationCacheQuirk;
#endif
    mutable std::optional<bool> m_shouldEnableFontLoadingAPIQuirk;
    mutable std::optional<bool> m_needsVideoShouldMaintainAspectRatioQuirk;
    mutable std::optional<bool> m_shouldExposeShowModalDialog;
#if PLATFORM(IOS_FAMILY)
    mutable std::optional<bool> m_shouldNavigatorPluginsBeEmpty;
#endif
    mutable std::optional<bool> m_shouldDisableLazyIframeLoadingQuirk;
    bool m_needsConfigurableIndexedPropertiesQuirk { false };
    bool m_needsToCopyUserSelectNoneQuirk { false };
    mutable std::optional<bool> m_shouldStarBePermissionsPolicyDefaultValueQuirk;
    mutable std::optional<bool> m_shouldDisableDataURLPaddingValidation;
    mutable std::optional<bool> m_needsDisableDOMPasteAccessQuirk;
    mutable std::optional<bool> m_shouldDisableElementFullscreen;
    mutable std::optional<bool> m_shouldIgnorePlaysInlineRequirementQuirk;
    mutable std::optional<bool> m_needsRelaxedCorsMixedContentCheckQuirk;
    mutable std::optional<bool> m_needsScrollbarWidthThinDisabledQuirk;
    mutable std::optional<bool> m_needsPrimeVideoUserSelectNoneQuirk;

    Vector<RegistrableDomain> m_subFrameDomainsForStorageAccessQuirk;
};

} // namespace WebCore
