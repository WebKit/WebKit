/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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

#if ENABLE(RESOURCE_LOAD_STATISTICS)
class RegistrableDomain;
enum class StorageAccessWasGranted : bool;
#endif

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
    bool shouldDispatchSimulatedMouseEvents() const;
    bool shouldDispatchedSimulatedMouseEventsAssumeDefaultPrevented(EventTarget*) const;
    Optional<Event::IsCancelable> simulatedMouseEventTypeForTarget(EventTarget*) const;
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

    bool needsMillisecondResolutionForHighResTimeStamp() const;

    WEBCORE_EXPORT bool shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const;
    WEBCORE_EXPORT bool shouldSuppressAutocorrectionAndAutocaptializationInHiddenEditableAreas() const;
    WEBCORE_EXPORT bool isTouchBarUpdateSupressedForHiddenContentEditable() const;
    WEBCORE_EXPORT bool isNeverRichlyEditableForTouchBar() const;
    WEBCORE_EXPORT bool shouldAvoidResizingWhenInputViewBoundsChange() const;
    WEBCORE_EXPORT bool shouldAvoidScrollingWhenFocusedContentIsVisible() const;
    WEBCORE_EXPORT bool shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation() const;
    WEBCORE_EXPORT bool shouldIgnoreAriaForFastPathContentObservationCheck() const;
    WEBCORE_EXPORT bool shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraints() const;
    WEBCORE_EXPORT bool shouldIgnoreContentObservationForSyntheticClick(bool isFirstSyntheticClickOnPage) const;

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
    StorageAccessResult triggerOptionalStorageAccessQuirk(Element&, const PlatformMouseEvent&, const AtomString& eventType, int, Element*) const;

    bool needsVP9FullRangeFlagQuirk() const;
    bool needsHDRPixelDepthQuirk() const;
    
    bool needsAkamaiMediaPlayerQuirk(const HTMLVideoElement&) const;

    bool needsBlackFullscreenBackgroundQuirk() const;

    bool requiresUserGestureToPauseInPictureInPicture() const;
    bool requiresUserGestureToLoadInPictureInPicture() const;

    WEBCORE_EXPORT bool blocksReturnToFullscreenFromPictureInPictureQuirk() const;
    bool shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk() const;

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    static bool isMicrosoftTeamsRedirectURL(const URL&);
    static bool hasStorageAccessForAllLoginDomains(const HashSet<RegistrableDomain>&, const RegistrableDomain&);
    static const String& BBCRadioPlayerURLString();
    WEBCORE_EXPORT static const String& staticRadioPlayerURLString();
    StorageAccessResult requestStorageAccessAndHandleClick(CompletionHandler<void(StorageAccessWasGranted)>&&) const;
    static RegistrableDomain mapToTopDomain(const URL&);
#endif

#if ENABLE(WEB_AUTHN)
    WEBCORE_EXPORT bool shouldBypassUserGestureRequirementForWebAuthn() const;
#endif

    static bool shouldOmitHTMLDocumentSupportedPropertyNames();

private:
    bool needsQuirks() const;

#if ENABLE(TOUCH_EVENTS)
    bool isAmazon() const;
    bool isGoogleMaps() const;
#endif

    WeakPtr<Document> m_document;

    mutable Optional<bool> m_hasBrokenEncryptedMediaAPISupportQuirk;
    mutable Optional<bool> m_needsFullWidthHeightFullscreenStyleQuirk;
#if PLATFORM(IOS_FAMILY)
    mutable Optional<bool> m_needsGMailOverflowScrollQuirk;
    mutable Optional<bool> m_needsYouTubeOverflowScrollQuirk;
    mutable Optional<bool> m_needsPreloadAutoQuirk;
    mutable Optional<bool> m_needsFullscreenDisplayNoneQuirk;
    mutable Optional<bool> m_shouldAvoidPastingImagesAsWebContent;
#endif
    mutable Optional<bool> m_shouldDisableElementFullscreenQuirk;
#if ENABLE(TOUCH_EVENTS)
    mutable Optional<bool> m_shouldDispatchSimulatedMouseEventsQuirk;
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    mutable Optional<bool> m_shouldSynthesizeTouchEventsQuirk;
#endif
    mutable Optional<bool> m_needsCanPlayAfterSeekedQuirk;
    mutable Optional<bool> m_shouldBypassAsyncScriptDeferring;
    mutable Optional<bool> m_needsVP9FullRangeFlagQuirk;
    mutable Optional<bool> m_needsHDRPixelDepthQuirk;
    mutable Optional<bool> m_needsBlackFullscreenBackgroundQuirk;
    mutable Optional<bool> m_requiresUserGestureToPauseInPictureInPicture;
    mutable Optional<bool> m_requiresUserGestureToLoadInPictureInPicture;
#if ENABLE(MEDIA_STREAM)
    mutable Optional<bool> m_shouldEnableLegacyGetUserMediaQuirk;
#endif
    mutable Optional<bool> m_blocksReturnToFullscreenFromPictureInPictureQuirk;
    mutable Optional<bool> m_shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk;
};

} // namespace WebCore
