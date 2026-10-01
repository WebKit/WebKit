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
#include <WebCore/QuirksAccessors.h>
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

class Quirks : public QuirksAccessors {
    WTF_MAKE_TZONE_ALLOCATED(Quirks);
    WTF_MAKE_NONCOPYABLE(Quirks);
public:
    Quirks(Document&);
    ~Quirks();

    bool NODELETE hasRelevantQuirks() const;

    bool shouldSilenceResizeObservers() const;
    bool shouldSilenceWindowResizeEventsDuringApplicationSnapshotting() const;
    bool shouldSilenceMediaQueryListChangeEvents() const;
    bool shouldIgnoreInvalidSignal() const;
    bool needsAutoplayPlayPauseEvents() const;
    bool needsPerDocumentAutoplayBehavior() const;
    bool hasBrokenEncryptedMediaAPISupportQuirk() const;

#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
    bool shouldDispatchSimulatedMouseEvents(const EventTarget*) const;
    bool shouldPreventDispatchOfTouchEvent(const AtomString&, EventTarget*) const;
#endif
#if ENABLE(TOUCH_EVENTS)
    bool shouldDispatchedSimulatedMouseEventsAssumeDefaultPrevented(EventTarget*) const;
#endif
    bool NODELETE needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommand() const;
    WEBCORE_EXPORT bool NODELETE inputMethodUsesCorrectKeyEventOrder() const;

    WEBCORE_EXPORT bool shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const;
    WEBCORE_EXPORT static bool shouldAllowNavigationToCustomProtocolWithoutUserGesture(StringView protocol, const SecurityOriginData& requesterOrigin);

    WEBCORE_EXPORT bool NODELETE needsYouTubeMouseOutQuirk() const;

    WEBCORE_EXPORT bool needsCaptionMirroringQuirk() const;

    WEBCORE_EXPORT static void updateStorageAccessUserAgentStringQuirks(HashMap<RegistrableDomain, String>&&);
    WEBCORE_EXPORT String storageAccessUserAgentStringQuirkForDomain(const URL&);
    WEBCORE_EXPORT static bool NODELETE needsDesktopUserAgent(const URL&);
    WEBCORE_EXPORT static std::optional<String> needsCustomUserAgentOverride(const URL&, const String& applicationNameForUserAgent, const String& currentUserAgent);

    WEBCORE_EXPORT static bool needsPartitionedCookies(const ResourceRequest&);

    WEBCORE_EXPORT static std::optional<Vector<HashSet<String>>> NODELETE defaultVisibilityAdjustmentSelectors(const URL&);

    WEBCORE_EXPORT bool static NODELETE shouldDisableBlobFileAccessEnforcement();

    bool shouldAllowMixedContentConnectionToLoopback(const URL&);

    bool NODELETE shouldOpenAsAboutBlank(const String&) const;

    bool shouldBypassBackForwardCache() const;

    static bool shouldMakeEventListenerPassive(const EventTarget&, const EventTypeInfo&);

    Ref<NodeList> applyFacebookFlagQuirk(Document&, NodeList&);
    bool shouldEnableRTCEncodedStreamsQuirk() const;

    bool shouldNotAutoUpgradeToHTTPSNavigation(const URL&);

    enum StorageAccessResult : bool { ShouldNotCancelEvent, ShouldCancelEvent };
    enum ShouldDispatchClick : bool { No, Yes };

    void triggerOptionalStorageAccessIframeQuirk(const URL& frameURL, CompletionHandler<void()>&&) const;
    StorageAccessResult triggerOptionalStorageAccessQuirk(Element&, const PlatformMouseEvent&, const AtomString& eventType, int, Element*, bool isParentProcessAFullWebBrowser, IsSyntheticClick) const;
    void setSubFrameDomainsForStorageAccessQuirk(Vector<RegistrableDomain>&& domains) { m_subFrameDomainsForStorageAccessQuirk = WTF::move(domains); }
    const Vector<RegistrableDomain>& subFrameDomainsForStorageAccessQuirk() const LIFETIME_BOUND { return m_subFrameDomainsForStorageAccessQuirk; }

    static bool hasStorageAccessForAllLoginDomains(const HashSet<RegistrableDomain>&, const RegistrableDomain&);
    StorageAccessResult requestStorageAccessAndHandleClick(CompletionHandler<void(ShouldDispatchClick)>&&) const;

    WEBCORE_EXPORT void setTopDocumentURLForTesting(URL&&);

    static bool shouldOmitHTMLDocumentSupportedPropertyNames();

    WEBCORE_EXPORT Vector<String> activeQuirks() const;

    bool shouldEnableFontLoadingAPIQuirk() const;

    bool shouldDisableScrollAnchoringQuirk() const;

    void setNeedsConfigurableIndexedPropertiesQuirk() { m_needsConfigurableIndexedPropertiesQuirk = true; }
    bool needsConfigurableIndexedPropertiesQuirk() const;

    // webkit.org/b/259091.
    bool needsToCopyUserSelectNoneQuirk() const { return m_needsToCopyUserSelectNoneQuirk; }
    void setNeedsToCopyUserSelectNoneQuirk() { m_needsToCopyUserSelectNoneQuirk = true; }

    String advancedPrivacyProtectionSubstituteDataURLForScriptWithFeatures(const String& lastDrawnText, int canvasWidth, int canvasHeight) const;

    bool needsDisableDOMPasteAccessQuirk() const;

    bool needsPopupFromMicrosoftOfficeToOneDrive(const String& targetURLString) const;

    WEBCORE_EXPORT bool needsConsistentQueryParameterFilteringQuirk(const URL&) const;
    bool mayBenefitFromFingerprintingProtectionQuirk(const URL&) const;
    static String standardUserAgentWithApplicationNameIncludingCompatOverrides(const String&, const String&, UserAgentType);

    Vector<String, 1> scriptsToEvaluateBeforeRunningScriptFromURL(const URL&);

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT bool needsPointerTouchCompatibility(const Element&) const;
#endif

    bool needsAmazonDesignMenuViewportUnitQuirk(const Style::ComputedStyle&, const Style::ComputedStyle& parentStyle) const;
    bool needsClaudeSidebarViewportUnitQuirk(Element&, const Style::ComputedStyle&) const;
    bool needsChromeOSNavigatorUserAgentQuirk(const Document&) const;

    WEBCORE_EXPORT bool shouldAvoidStartingSelectionOnMouseDownOverPointerCursor(const Node&) const;

    bool NODELETE needsFacebookStoriesCreationFormQuirk(const Element&, const Style::ComputedStyle&) const;

    enum class TikTokOverflowingContentQuirkType : bool { VideoSectionQuirk, CommentsSectionQuirk };
    std::optional<TikTokOverflowingContentQuirkType> needsTikTokOverflowingContentQuirk(const Element&, const Style::ComputedStyle& parentStyle) const;

    bool needsInstagramResizingReelsQuirk(const Element&, const Style::ComputedStyle& elementStyle, const Style::ComputedStyle& parentStyle) const;

    bool shouldPreventKeyframeEffectAcceleration(const KeyframeEffect&) const;

    void clearLogoutSurvivingIdentityCookiesIfNeeded(const URL& fetchURL, int httpStatusCode);

    void determineRelevantQuirks();
    void logQuirksToConsoleIfNecessary() const;

private:
    URL topDocumentURL() const;
    URL documentURL() const;

    mutable WeakPtr<const Element, WeakPtrImplWithEventTargetData> m_facebookStoriesCreationFormContainer;

    mutable QuirkBitSet m_probedQuirks;

    template<typename Probe>
    bool isBehaviorEnabledAfterProbing(const QuirkBehavior& quirk, NOESCAPE const Probe& probe) const
    {
        auto index = static_cast<size_t>(quirk.id);
        if (!m_probedQuirks.get(index)) {
            m_probedQuirks.set(index);
            m_quirksData.setEnabled(quirk, probe());
        }
        return m_quirksData.isBehaviorEnabled(quirk.id);
    }

    bool m_needsConfigurableIndexedPropertiesQuirk { false };
    bool m_needsToCopyUserSelectNoneQuirk { false };

    Vector<RegistrableDomain> m_subFrameDomainsForStorageAccessQuirk;
    URL m_topDocumentURLForTesting;
};

} // namespace WebCore
