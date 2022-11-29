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
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class Element;
class EventListener;
class EventTarget;
class HTMLElement;
class HTMLVideoElement;
class LayoutUnit;
class PlatformMouseEvent;
class WeakPtrImplWithEventTargetData;
struct SecurityOriginData;

#if ENABLE(TRACKING_PREVENTION)
class RegistrableDomain;
enum class StorageAccessWasGranted : bool;
#endif

enum class IsSyntheticClick : bool;

class Quirks {
    WTF_MAKE_NONCOPYABLE(Quirks); WTF_MAKE_FAST_ALLOCATED;
public:
    Quirks(Document&);
    ~Quirks();

    bool shouldSilenceWindowResizeEvents() const;
    bool shouldSilenceMediaQueryListChangeEvents() const;
    bool shouldIgnoreInvalidSignal() const;
    bool needsFormControlToBeMouseFocusable() const;
    bool needsAutoplayPlayPauseEvents() const;
    bool needsSeekingSupportDisabled() const;
    bool needsPerDocumentAutoplayBehavior() const;
    bool shouldAutoplayForArbitraryUserGesture() const;
    bool shouldAutoplayWebAudioForArbitraryUserGesture() const;
    bool hasBrokenEncryptedMediaAPISupportQuirk() const;
    bool shouldStripQuotationMarkInFontFaceSetFamily() const;
#if ENABLE(TOUCH_EVENTS)
    bool shouldDispatchSimulatedMouseEvents(const EventTarget*) const;
    bool shouldDispatchedSimulatedMouseEventsAssumeDefaultPrevented(EventTarget*) const;
    std::optional<Event::IsCancelable> simulatedMouseEventTypeForTarget(EventTarget*) const;
    bool shouldMakeTouchEventNonCancelableForTarget(EventTarget*) const;
    bool shouldPreventPointerMediaQueryFromEvaluatingToCoarse() const;
    bool shouldPreventDispatchOfTouchEvent(const AtomString&, EventTarget*) const;
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    WEBCORE_EXPORT bool shouldSynthesizeTouchEvents() const;
#endif
    bool shouldDisablePointerEventsQuirk() const;
    bool needsInputModeNoneImplicitly(const HTMLElement&) const;
    bool needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommand() const;
    bool shouldDisableContentChangeObserverTouchEventAdjustment() const;
    bool shouldTooltipPreventFromProceedingWithClick(const Element&) const;
    bool shouldHideSearchFieldResultsButton() const;
    bool shouldExposeShowModalDialog() const;

    bool needsMillisecondResolutionForHighResTimeStamp() const;

    WEBCORE_EXPORT bool shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const;
    WEBCORE_EXPORT bool shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreas() const;
    WEBCORE_EXPORT bool isTouchBarUpdateSupressedForHiddenContentEditable() const;
    WEBCORE_EXPORT bool isNeverRichlyEditableForTouchBar() const;
    WEBCORE_EXPORT bool shouldAvoidResizingWhenInputViewBoundsChange() const;
    WEBCORE_EXPORT bool shouldAvoidScrollingWhenFocusedContentIsVisible() const;
    WEBCORE_EXPORT bool shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation() const;
    WEBCORE_EXPORT bool shouldIgnoreAriaForFastPathContentObservationCheck() const;
    WEBCORE_EXPORT bool shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraints() const;
    WEBCORE_EXPORT bool shouldIgnoreContentObservationForSyntheticClick(bool isFirstSyntheticClickOnPage) const;
    WEBCORE_EXPORT static bool shouldAllowNavigationToCustomProtocolWithoutUserGesture(StringView protocol, const SecurityOriginData& requesterOrigin);

    WEBCORE_EXPORT bool needsYouTubeMouseOutQuirk() const;
    
    WEBCORE_EXPORT bool shouldAvoidUsingIOS13ForGmail() const;

    bool needsGMailOverflowScrollQuirk() const;
    bool needsYouTubeOverflowScrollQuirk() const;
    bool needsFullscreenDisplayNoneQuirk() const;
    bool needsWeChatScrollingQuirk() const;

    bool shouldOpenAsAboutBlank(const String&) const;

    bool needsPreloadAutoQuirk() const;

    bool shouldBypassBackForwardCache() const;
    bool shouldBypassAsyncScriptDeferring() const;

    static bool shouldMakeEventListenerPassive(const EventTarget&, const AtomString& eventType, const EventListener&);

#if ENABLE(MEDIA_STREAM)
    bool shouldEnableLegacyGetUserMediaQuirk() const;
#endif

    bool shouldDisableElementFullscreenQuirk() const;

    bool needsCanPlayAfterSeekedQuirk() const;

    bool shouldAvoidPastingImagesAsWebContent() const;

    enum StorageAccessResult : bool { ShouldNotCancelEvent, ShouldCancelEvent };
    enum ShouldDispatchClick : bool { No, Yes };
    StorageAccessResult triggerOptionalStorageAccessQuirk(Element&, const PlatformMouseEvent&, const AtomString& eventType, int, Element*, bool isParentProcessAFullWebBrowser, IsSyntheticClick) const;

    bool needsVP9FullRangeFlagQuirk() const;
    bool needsHDRPixelDepthQuirk() const;
    
    bool needsFlightAwareSerializationQuirk() const;

    bool needsBlackFullscreenBackgroundQuirk() const;

    bool requiresUserGestureToPauseInPictureInPicture() const;
    bool requiresUserGestureToLoadInPictureInPicture() const;

    WEBCORE_EXPORT bool blocksReturnToFullscreenFromPictureInPictureQuirk() const;
    bool shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk() const;

#if ENABLE(TRACKING_PREVENTION)
    static bool isMicrosoftTeamsRedirectURL(const URL&);
    static bool hasStorageAccessForAllLoginDomains(const HashSet<RegistrableDomain>&, const RegistrableDomain&);
    static const String& BBCRadioPlayerURLString();
    WEBCORE_EXPORT static const String& staticRadioPlayerURLString();
    StorageAccessResult requestStorageAccessAndHandleClick(CompletionHandler<void(ShouldDispatchClick)>&&) const;
#endif

    static bool shouldOmitHTMLDocumentSupportedPropertyNames();

#if ENABLE(IMAGE_ANALYSIS)
    bool needsToForceUserSelectAndUserDragWhenInstallingImageOverlay() const;
#endif
    
#if PLATFORM(IOS)
    WEBCORE_EXPORT bool allowLayeredFullscreenVideos() const;
#endif
    bool shouldEnableApplicationCacheQuirk() const;
    bool shouldEnableFontLoadingAPIQuirk() const;
    bool needsVideoShouldMaintainAspectRatioQuirk() const;

    bool shouldDisableLazyImageLoadingQuirk() const;
    
private:
    bool needsQuirks() const;

#if ENABLE(TOUCH_EVENTS)
    bool isAmazon() const;
    bool isGoogleMaps() const;
#endif

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;

    mutable std::optional<bool> m_hasBrokenEncryptedMediaAPISupportQuirk;
    mutable std::optional<bool> m_needsFullWidthHeightFullscreenStyleQuirk;
#if PLATFORM(IOS_FAMILY)
    mutable std::optional<bool> m_needsGMailOverflowScrollQuirk;
    mutable std::optional<bool> m_needsYouTubeOverflowScrollQuirk;
    mutable std::optional<bool> m_needsPreloadAutoQuirk;
    mutable std::optional<bool> m_needsFullscreenDisplayNoneQuirk;
    mutable std::optional<bool> m_shouldAvoidPastingImagesAsWebContent;
#endif
    mutable std::optional<bool> m_shouldDisableElementFullscreenQuirk;
#if ENABLE(TOUCH_EVENTS)
    enum class ShouldDispatchSimulatedMouseEvents : uint8_t {
        Unknown,
        No,
        DependingOnTargetFor_mybinder_org,
        Yes,
    };
    mutable ShouldDispatchSimulatedMouseEvents m_shouldDispatchSimulatedMouseEventsQuirk { ShouldDispatchSimulatedMouseEvents::Unknown };
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    mutable std::optional<bool> m_shouldSynthesizeTouchEventsQuirk;
#endif
    mutable std::optional<bool> m_needsCanPlayAfterSeekedQuirk;
    mutable std::optional<bool> m_shouldBypassAsyncScriptDeferring;
    mutable std::optional<bool> m_needsVP9FullRangeFlagQuirk;
    mutable std::optional<bool> m_needsHDRPixelDepthQuirk;
    mutable std::optional<bool> m_needsFlightAwareSerializationQuirk;
    mutable std::optional<bool> m_needsBlackFullscreenBackgroundQuirk;
    mutable std::optional<bool> m_requiresUserGestureToPauseInPictureInPicture;
    mutable std::optional<bool> m_requiresUserGestureToLoadInPictureInPicture;
#if ENABLE(MEDIA_STREAM)
    mutable std::optional<bool> m_shouldEnableLegacyGetUserMediaQuirk;
#endif
    mutable std::optional<bool> m_blocksReturnToFullscreenFromPictureInPictureQuirk;
    mutable std::optional<bool> m_shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk;
#if PLATFORM(IOS)
    mutable std::optional<bool> m_allowLayeredFullscreenVideos;
#endif
#if PLATFORM(IOS_FAMILY)
    mutable std::optional<bool> m_shouldEnableApplicationCacheQuirk;
#endif
    mutable std::optional<bool> m_shouldEnableFontLoadingAPIQuirk;
    mutable std::optional<bool> m_needsVideoShouldMaintainAspectRatioQuirk;
    mutable std::optional<bool> m_shouldExposeShowModalDialog;
    mutable std::optional<bool> m_shouldDisableLazyImageLoadingQuirk;
};

} // namespace WebCore
