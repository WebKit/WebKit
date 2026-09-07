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

#include "config.h"
#include "Quirks.h"

#include "AccessibilityObject.h"
#include "AccessibilityRole.h"
#include "Attr.h"
#include "ContainerNodeInlines.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "DNS.h"
#include "DatasetDOMStringMap.h"
#include "DeprecatedGlobalSettings.h"
#include "DocumentLoader.h"
#include "DocumentPage.h"
#include "DocumentQuirks.h"
#include "DocumentStorageAccess.h"
#include "DocumentView.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementInlines.h"
#include "ElementTargetingTypes.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTMLArticleElement.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLDivElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLScriptElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLVideoElement.h"
#include "JSEventListener.h"
#include "KeyframeEffect.h"
#include "LayoutUnit.h"
#include "LocalDOMWindow.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "MouseEvent.h"
#include "NetworkStorageSession.h"
#include "NodeRenderStyle.h"
#include "OrganizationStorageAccessPromptQuirk.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "QuirkTable.h"
#include "QuirksData.h"
#include "RegistrableDomain.h"
#include "RenderView.h"
#include "ResourceLoadObserver.h"
#include "ResourceRequest.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathElement.h"
#include "SVGSVGElement.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
#include "SpaceSplitString.h"
#include "StaticNodeList.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "TrustedFonts.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UserAgent.h"
#include "UserContentTypes.h"
#include "UserScript.h"
#include "UserScriptTypes.h"
#include <JavaScriptCore/CodeBlock.h>
#include <JavaScriptCore/IdentifierInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/ScriptExecutable.h>
#include <JavaScriptCore/SourceCode.h>
#include <JavaScriptCore/SourceProvider.h>
#include <JavaScriptCore/StackVisitor.h>
#include <array>
#include <ranges>
#include <wtf/EnumTraits.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#define QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(returnValue) \
    if (!needsQuirks()) [[unlikely]] \
        return returnValue

namespace WTF {
template<> struct EnumTraits<WebCore::SiteSpecificQuirk> {
    static constexpr int min = 0;
    static constexpr int max = static_cast<int>(WebCore::SiteSpecificQuirk::NumberOfQuirks);
};
} // namespace WTF

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Quirks);

#if PLATFORM(IOS_FAMILY)
static constexpr auto chromeUserAgentScript = "(function() { let userAgent = navigator.userAgent; Object.defineProperty(navigator, 'userAgent', { get: () => { return userAgent + ' Chrome/130.0.0.0 Android/15.0'; }, configurable: true }); })();"_s;

// nba.com rdar://147429596
static constexpr auto nbaSeekBarFixScript = R"js(if (!window.__nbaSeekFix) {
    window.__nbaSeekFix = true;
    document.addEventListener('touchmove', function({ target, touches }) {
        if (!target?.getAttribute
            || target.getAttribute('data-id') !== 'video-player:scrub-bar:controls'
            || !touches?.[0])
            return;
        const touch = touches[0];
        const rect = target.getBoundingClientRect();
        const event = new MouseEvent('mousemove', {
            clientX: touch.clientX,
            clientY: touch.clientY,
            screenX: touch.screenX,
            screenY: touch.screenY,
            bubbles: true,
            cancelable: true
        });
        Object.defineProperty(event, 'offsetX', { value: touch.clientX - rect.left, configurable: true });
        Object.defineProperty(event, 'offsetY', { value: touch.clientY - rect.top, configurable: true });
        target.dispatchEvent(event);
    }, false);
})js"_s;
#endif

// ceac.state.gov rdar://170258502
static constexpr auto ceacBeforeUnloadFixScript = R"js((function() {
    if (window.__ceacBeforeUnloadFix) return;
    window.__ceacBeforeUnloadFix = true;
    var origAEL = window.addEventListener;
    window.addEventListener = function(type, fn, opts) {
        if (type === 'beforeunload') {
            return origAEL.call(this, type, function(e) {
                var ae = document.activeElement;
                if (ae && ae.tagName === 'INPUT') {
                    var t = (ae.type || '').toLowerCase();
                    if (t === 'radio' || t === 'checkbox' || t === 'submit' || t === 'button')
                        return;
                }
                if (typeof fn === 'function') fn.call(this, e);
            }, opts);
        }
        return origAEL.apply(this, arguments);
    };
})();)js"_s;

static inline OptionSet<AutoplayQuirk> NODELETE allowedAutoplayQuirks(Document& document)
{
    auto* loader = document.loader();
    if (!loader)
        return { };

    return loader->allowedAutoplayQuirks();
}

static inline OptionSet<AutoplayQuirk> NODELETE allowedAutoplayQuirks(Document* document)
{
    if (!document)
        return { };
    return allowedAutoplayQuirks(*document);
}

static HashMap<RegistrableDomain, String>& NODELETE updatableStorageAccessUserAgentStringQuirks()
{
    // FIXME: Make this a member of Quirks.
    static MainThreadNeverDestroyed<HashMap<RegistrableDomain, String>> map;
    return map.get();
}

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/QuirksAdditions.cpp>
#else
static inline bool NODELETE needsDesktopUserAgentInternal(const URL&) { return false; }
static inline bool NODELETE shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeInternal(const URL&) { return false; }
static inline bool NODELETE shouldNotAutoUpgradeToHTTPSNavigationInternal(const URL&) { return false; }
static inline bool NODELETE shouldDisableBlobFileAccessEnforcementInternal() { return false; }
static inline bool NODELETE needsConsistentQueryParameterFilteringInternal(const URL&) { return false; }
#if PLATFORM(COCOA)
static inline String NODELETE standardUserAgentWithApplicationNameIncludingCompatOverridesInternal(const String&, const String&, UserAgentType) { return { }; }
#endif
#endif

static bool urlHasQuirk(const URL& url, SiteSpecificQuirk quirk)
{
    return resolveTopURLQuirks(url).quirkIsEnabled(quirk);
}

Quirks::Quirks(Document& document)
    : m_document(document)
{
    determineRelevantQuirks();
}

Quirks::~Quirks() = default;

inline bool Quirks::needsQuirks() const
{
    return m_document && m_document->settings().needsSiteSpecificQuirks();
}

bool Quirks::elementMatchesQuirk(SiteSpecificQuirk quirk, const Node* node) const
{
    auto index = static_cast<size_t>(quirk);
    if (m_quirksData.activeQuirks.get(index))
        return true;

    if (!m_quirksData.conditionalQuirks.get(index))
        return false;

    for (auto& condition : m_elementConditions) {
        if (condition.quirk == quirk && quirkSelectorMatches(condition.selector, node))
            return true;
    }
    return false;
}

bool Quirks::shouldIgnoreInvalidSignal() const
{
    return needsQuirks();
}

bool Quirks::shouldDisableBlobFileAccessEnforcement()
{
    return shouldDisableBlobFileAccessEnforcementInternal();
}

// thesaurus.com, dictionary.com https://bugs.webkit.org/show_bug.cgi?id=312692 rdar://174959285
bool Quirks::needsAnchorToBeMouseFocusable() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsAnchorToBeMouseFocusableQuirk);
}

// ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=193478
// weather.com rdar://139689157
// madisoncity.k12.al.us https://bugs.webkit.org/show_bug.cgi?id=296989
bool Quirks::needsFormControlToBeMouseFocusable() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsFormControlToBeMouseFocusableQuirk);
}

bool Quirks::needsAutoplayPlayPauseEvents() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDispatchPlayPauseEventsOnResume))
        return true;

    Ref document = *m_document;
    if (allowedAutoplayQuirks(document).contains(AutoplayQuirk::SynthesizedPauseEvents))
        return true;

    return allowedAutoplayQuirks(document->mainFrameDocument()).contains(AutoplayQuirk::SynthesizedPauseEvents);
}

// netflix.com https://bugs.webkit.org/show_bug.cgi?id=173030
// This quirk handles several scenarios:
// - Inserting / Removing Airpods
// - macOS w/ Touch Bar
// - iOS PiP
bool Quirks::needsSeekingSupportDisabled() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsSeekingSupportDisabledQuirk);
}

// netflix.com https://bugs.webkit.org/show_bug.cgi?id=193301
bool Quirks::needsPerDocumentAutoplayBehavior() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

#if PLATFORM(MAC)
    Ref document = *m_document;
    ASSERT(document->isTopDocument());
    return allowedAutoplayQuirks(document).contains(AutoplayQuirk::PerDocumentAutoplayBehavior);
#else
    return m_quirksData.isSite(QuirkSite::Netflix) || m_quirksData.isSite(QuirkSite::NBA);
#endif
}

// zoom.com https://bugs.webkit.org/show_bug.cgi?id=223180
bool Quirks::shouldAutoplayWebAudioForArbitraryUserGesture() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldAutoplayWebAudioForArbitraryUserGestureQuirk);
}

// youtube.com https://bugs.webkit.org/show_bug.cgi?id=195598
bool Quirks::hasBrokenEncryptedMediaAPISupportQuirk() const
{
#if ENABLE(THUNDER)
    return false;
#else
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::HasBrokenEncryptedMediaAPISupportQuirk);
#endif
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=161984
bool Quirks::isTouchBarUpdateSuppressedForHiddenContentEditable() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::IsTouchBarUpdateSuppressedForHiddenContentEditableQuirk);
}

// icloud.com rdar://26013388
// trix-editor.org rdar://28242210
// onedrive.live.com rdar://26013388
// added in https://bugs.webkit.org/show_bug.cgi?id=161996
bool Quirks::isNeverRichlyEditableForTouchBar() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::IsNeverRichlyEditableForTouchBarQuirk);
}

// docs.google.com rdar://49864669
// FIXME https://bugs.webkit.org/show_bug.cgi?id=260698
bool Quirks::shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreas() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk);
}

// weebly.com rdar://48003980
// medium.com rdar://50457837
bool Quirks::shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const
{
    if (m_document->settings().shouldDispatchSyntheticMouseEventsWhenModifyingSelection())
        return true;

    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk);
}

// www.youtube.com rdar://52361019
bool Quirks::needsYouTubeMouseOutQuirk() const
{
    if (m_document->settings().shouldDispatchSyntheticMouseOutAfterSyntheticClick())
        return true;

    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsYouTubeMouseOutQuirk);
}

bool Quirks::needsYouTubeCaptionsQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsYouTubeCaptionQuirk);
}

bool Quirks::needsCNNCaptionQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsCNNCaptionQuirk);
}

// theguardian.com rdar://166727225
bool Quirks::needsYouTubeEmbedAutoplayQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsYouTubeEmbedAutoplayQuirk);
}

// safe.menlosecurity.com rdar://135114489
// FIXME (rdar://138585709): Remove this quirk for safe.menlosecurity.com once investigation into text corruption on the site is completed and the issue is resolved.
bool Quirks::shouldDisableWritingSuggestionsByDefault() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableWritingSuggestionsByDefaultQuirk);
}

void Quirks::updateStorageAccessUserAgentStringQuirks(HashMap<RegistrableDomain, String>&& userAgentStringQuirks)
{
    auto& quirks = updatableStorageAccessUserAgentStringQuirks();
    quirks.clear();
    for (auto&& [domain, userAgent] : userAgentStringQuirks)
        quirks.add(WTF::move(domain), WTF::move(userAgent));
}

String Quirks::storageAccessUserAgentStringQuirkForDomain(const URL& url)
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE({ });

    const auto& quirks = updatableStorageAccessUserAgentStringQuirks();
    RegistrableDomain domain { url };
    auto iterator = quirks.find(domain);
    if (iterator == quirks.end())
        return { };
    if (domain == "live.com"_s && url.host() != "teams.live.com"_s)
        return { };
    return iterator->value;
}

// apple.com rdar://154434137
bool Quirks::ensureCaptionVisibilityInFullscreenAndPictureInPicture() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::EnsureCaptionVisibilityInFullscreenAndPictureInPicture);
}

bool Quirks::shouldDisableElementFullscreenQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    // Vimeo.com has incorrect layout on iOS on certain videos with wider
    // aspect ratios than the device's screen in landscape mode.
    // (Ref: rdar://116531089)
    // Instagram.com stories flow under the notch and status bar
    // (Ref: rdar://121014613)
    // x.com (Twitter) video embeds have controls that are too tiny and
    // show page behind fullscreen.
    // (Ref: rdar://121473410)
    // YouTube.com does not provide AirPlay controls in fullscreen
    // (Ref: rdar://121471373)
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableElementFullscreenQuirk);
}

#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
// rdar://49124313
// desmos.com rdar://47068176
// flipkart.com rdar://49648520
// soundcloud.com rdar://52915981
// naver.com rdar://48068610
// mybinder.org rdar://51770057
bool Quirks::shouldDispatchSimulatedMouseEvents(const EventTarget* target) const
{
    if (m_document->settings().mouseEventsSimulationEnabled())
        return true;

    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkMayBeEnabled(SiteSpecificQuirk::ShouldDispatchSimulatedMouseEventsQuirk))
        return false;

    auto* loader = m_document->loader();
    if (!loader || loader->simulatedMouseEventsDispatchPolicy() != SimulatedMouseEventsDispatchPolicy::Allow)
        return false;

    return elementMatchesQuirk(SiteSpecificQuirk::ShouldDispatchSimulatedMouseEventsQuirk, dynamicDowncast<Node>(target));
}

bool Quirks::shouldPreventDispatchOfTouchEvent(const AtomString& touchEventType, EventTarget* target) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldPreventDispatchOfTouchEventQuirk))
        return false;

    // yahoo.com : rdar://142894603
    if (RefPtr element = dynamicDowncast<Element>(target); element && touchEventType == eventNames().touchendEvent) {
        if (element->hasClassName("DPvwYc"_s) && element->hasClassName("sm8sCf"_s))
            return true;
        if (element->hasClassName("vjs-subs-cap-button"_s) && element->hasClassName("vjs-menu-button"_s))
            return true;
    }

    // sites.google.com rdar://58653069
    if (RefPtr element = dynamicDowncast<Element>(target); element && touchEventType == eventNames().touchendEvent)
        return element->hasClassName("DPvwYc"_s) && element->hasClassName("sm8sCf"_s);

    // outlook.live.com rdar://48008837
    if (RefPtr element = dynamicDowncast<Element>(target); element && touchEventType == eventNames().touchmoveEvent) {
        static constexpr unsigned max_depth = 15;
        unsigned depth = 0;
        for (Ref ancestor : lineageOfType<HTMLElement>(*element)) {
            if (ancestor->hasClassName("ms-Suggestions"_s))
                return true;
            if (++depth > max_depth)
                break;
        }
    }

    return false;
}
#endif

#if ENABLE(TOUCH_EVENTS)
// amazon.com rdar://49124529
// soundcloud.com rdar://52915981
bool Quirks::shouldDispatchedSimulatedMouseEventsAssumeDefaultPrevented(EventTarget* target) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!shouldDispatchSimulatedMouseEvents(target))
        return false;

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk))
        return false;

    RefPtr element = dynamicDowncast<Element>(target);
    if (!element)
        return false;

    if (m_quirksData.isSite(QuirkSite::Amazon)) {
        // When panning on an Amazon product image, we're either touching on the #magnifierLens element
        // or its previous sibling.
        if (element->getIdAttribute() == "magnifierLens"_s)
            return true;
        if (auto* sibling = element->nextElementSibling())
            return sibling->getIdAttribute() == "magnifierLens"_s;
    }

    if (m_quirksData.isSite(QuirkSite::SoundCloud))
        return element->hasClassName("sceneLayer"_s);

    // facebook.com rdar://174179871 tiktok.com rdar://174179805
    if (m_quirksData.isSite(QuirkSite::Facebook) || m_quirksData.isSite(QuirkSite::TikTok))
        return element->attributeWithoutSynchronization(HTMLNames::roleAttr) == "slider"_s;

    return false;
}

// facebook.com rdar://174179871 tiktok.com rdar://174179805
bool Quirks::shouldComputeSimulatedMouseEventMovementDelta() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.isSite(QuirkSite::TikTok) || m_quirksData.isSite(QuirkSite::Facebook);
}

#if PLATFORM(IOS_FAMILY) && ENABLE(IOS_TOUCH_EVENTS)
bool Quirks::shouldAllowNativeTapsOnMediaElements(const Node* node) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.isSite(QuirkSite::LinkedIn))
        return false;

    return is<HTMLMediaElement>(node) && downcast<HTMLMediaElement>(*node).hasClassName("vjs-tech"_s);
}
#endif

#endif

// live.com rdar://52116170
// sharepoint.com rdar://52116170
// maps.google.com https://bugs.webkit.org/show_bug.cgi?id=214945
bool Quirks::shouldAvoidResizingWhenInputViewBoundsChange() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldAvoidResizingWhenInputViewBoundsChangeQuirk);
}

// mailchimp.com rdar://47868965
bool Quirks::shouldDisablePointerEventsQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisablePointerEventsQuirk);
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
bool Quirks::needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommand() const
{
    if (m_document->settings().needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk())
        return true;

    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.isSite(QuirkSite::GoogleDocs);
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
bool Quirks::inputMethodUsesCorrectKeyEventOrder() const
{
    return false;
}

bool Quirks::inputMethodMustUseCompositionEvents() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::InputMethodMustUseCompositionEvents);
}

bool Quirks::shouldIgnoreInputModeNone() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldIgnoreInputModeNone);
}

// rdar://176981763
bool Quirks::shouldAllowMixedContentConnectionToLoopback(const URL& url)
{
    if (m_document->url().host() != "account.battle.net"_s || m_document->url().path().startsWith("/login"_s))
        return false;
    if (auto address = IPAddress::fromString(url.host().toStringWithoutCopying()))
        return address->isLoopback();
    return false;
}

// FIXME: Remove after the site is fixed, <rdar://problem/50374200>
// mail.google.com rdar://49403416
bool Quirks::needsGMailOverflowScrollQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsGMailOverflowScrollQuirk);
}

// FIXME: Remove after the site is fixed, <rdar://problem/50374311>
// youtube.com rdar://49582231
bool Quirks::needsYouTubeOverflowScrollQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsYouTubeOverflowScrollQuirk);
}

// webex.com rdar://143715630
bool Quirks::needsWebExScrollabilityQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsWebExScrollabilityQuirk);
}
// facebook.com https://webkit.org/b/295071
// FIXME: https://webkit.org/b/295318
bool Quirks::needsFacebookRemoveNotSupportedQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsFacebookRemoveNotSupportedQuirk);
}

// youtube.com rdar://135886305
// NOTE: Also remove `BuilderConverter::convertScrollbarWidth` and related code when removing this quirk.
bool Quirks::needsScrollbarWidthThinDisabledQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsScrollbarWidthThinDisabledQuirk);
}

// spotify.com rdar://138918575
bool Quirks::needsBodyScrollbarWidthNoneDisabledQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsBodyScrollbarWidthNoneDisabledQuirk);
}

// airindiaexpress.com https://webkit.org/b/317375
bool Quirks::needsAirIndiaExpressLayeringQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsAirIndiaExpressLayeringQuirk);
}

// gizmodo.com rdar://102227302
bool Quirks::needsFullscreenDisplayNoneQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsFullscreenDisplayNoneQuirk);
}

// cnn.com rdar://119640248
bool Quirks::needsFullscreenObjectFitQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsFullscreenObjectFitQuirk);
}

// zomato.com <rdar://problem/128962778>
bool Quirks::needsZomatoEmailLoginLabelQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsZomatoEmailLoginLabelQuirk);
}

// maps.google.com rdar://67358928
bool Quirks::needsGoogleMapsScrollingQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsGoogleMapsScrollingQuirk);
}

// translate.google.com rdar://106539018
bool Quirks::needsGoogleTranslateScrollingQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsGoogleTranslateScrollingQuirk);
}

// netflix.com rdar://178545839
bool Quirks::needsNetflixVolumeSliderQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsNetflixVolumeSliderQuirk);
}

// play.geforcenow.com https://webkit.org/b/303622
// FIXME: Remove as soon as nvidia adjusts the site for Safari. https://webkit.org/b/303718
bool Quirks::needsGeforcenowWarningDisplayNoneQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsGeforcenowWarningDisplayNoneQuirk);
}

// yahoo.com rdar://170502516
bool Quirks::needsYahooVolumeSliderQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsYahooVolumeSliderQuirk);
}

// Kugou Music rdar://74602294
bool Quirks::shouldOmitHTMLDocumentSupportedPropertyNames()
{
#if PLATFORM(COCOA)
    static bool shouldOmitHTMLDocumentSupportedPropertyNames = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::HTMLDocumentSupportedPropertyNames);
    return shouldOmitHTMLDocumentSupportedPropertyNames;
#else
    return false;
#endif
}

// rdar://110097836
bool Quirks::shouldSilenceResizeObservers() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    // ResizeObservers are silenced on YouTube during the 'homing out' snapshout sequence to
    // resolve rdar://109837319. This is due to a bug on the site that is causing unexpected
    // content layout and can be removed when it is addressed.
    RefPtr page = m_document->page();
    if (!page || !page->isTakingSnapshotsForApplicationSuspension())
        return false;

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldSilenceResizeObservers);
}

bool Quirks::shouldSilenceWindowResizeEventsDuringApplicationSnapshotting() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting))
        return false;

    // We silence window resize events during the 'homing out' snapshot sequence when on icloud.com/mail
    // to address <rdar://131836301>, on nytimes.com to address <rdar://problem/59763843>, and on
    // x.com (twitter) to address <rdar://problem/58804852> & <rdar://problem/61731801>.
    RefPtr page = m_document->page();
    if (!page || !page->isTakingSnapshotsForApplicationSuspension())
        return false;

    return true;
}

bool Quirks::shouldDeferIntersectionObserversDuringResize() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDeferIntersectionObserversDuringResize);
}

bool Quirks::shouldSilenceMediaQueryListChangeEvents() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldSilenceMediaQueryListChangeEvents))
        return false;

    // We silence MediaQueryList's change events during the 'homing out' snapshot sequence when on x.com (twitter)
    // to address <rdar://problem/58804852> & <rdar://problem/61731801>.
    RefPtr page = m_document->page();
    if (!page || !page->isTakingSnapshotsForApplicationSuspension())
        return false;

    return true;
}

// zillow.com rdar://53103732
bool Quirks::shouldAvoidScrollingWhenFocusedContentIsVisible() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldAvoidScrollingWhenFocusedContentIsVisibleQuirk);
}

// discord.com rdar://162719481
bool Quirks::shouldUseLayoutViewportForClientRects() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldUseLayoutViewportForClientRectsQuirk);
}

// Some input only specify image/* as an acceptable type, which is failing sometimes for certains domain names
// which do not support HEIC.
bool Quirks::shouldTranscodeHeicImagesForURL(const URL& url)
{
    return urlHasQuirk(url, SiteSpecificQuirk::ShouldTranscodeHeicImagesQuirk);
}

// att.com rdar://55185021
bool Quirks::shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk);
}

// ralphlauren.com rdar://55629493
bool Quirks::shouldIgnoreAriaForFastPathContentObservationCheck() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldIgnoreAriaForFastPathContentObservationCheckQuirk);
}

// wikipedia.org https://webkit.org/b/247636
bool Quirks::shouldIgnoreViewportArgumentsToAvoidExcessiveZoom() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk);
}

// slack.com rdar://138614711
bool Quirks::shouldIgnoreViewportArgumentsToAvoidEnlargedView() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk);
}

// slack.com rdar://171190689
bool Quirks::shouldUseDynamicViewportUnitsAsDefault() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldUseDynamicViewportUnitsAsDefaultQuirk);
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199933
bool Quirks::shouldOpenAsAboutBlank(const String& stringToOpen) const
{
#if PLATFORM(IOS_FAMILY)
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.isSite(QuirkSite::GoogleDocs))
        return false;

    auto openerURL = protect(m_document)->url();
    if (!m_document->frame() || !m_document->frame()->loader().userAgent(openerURL).contains("Macintosh"_s))
        return false;

    URL urlToOpen { URL { }, stringToOpen };
    if (!urlToOpen.protocolIsAbout())
        return false;

    return !equalLettersIgnoringASCIICase(urlToOpen.host(), "blank"_s) && !equalLettersIgnoringASCIICase(urlToOpen.host(), "srcdoc"_s);
#else
    UNUSED_PARAM(stringToOpen);
    return false;
#endif
}

// vimeo.com rdar://55759025
bool Quirks::needsPreloadAutoQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsPreloadAutoQuirk);
}

// espn.com rdar://184169028
bool Quirks::needsSuppressedPauseEventOnFullscreenExitQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsSuppressedPauseEventOnFullscreenExitQuirk);
}

// vimeo.com rdar://56996057
// docs.google.com rdar://59893415
// bing.com rdar://133223599
bool Quirks::shouldBypassBackForwardCache() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::MaybeBypassBackForwardCache))
        return false;

    RefPtr document = m_document.get();

    // Vimeo.com used to bypass the back/forward cache by serving "Cache-Control: no-store" over HTTPS.
    // We started caching such content in r250437 but the vimeo.com content unfortunately is not currently compatible
    // because it changes the opacity of its body to 0 when navigating away and fails to restore the original opacity
    // when coming back from the back/forward cache (e.g. in 'pageshow' event handler). See <rdar://problem/56996057>.
    if (m_quirksData.isSite(QuirkSite::Vimeo) && topDocumentURL().protocolIs("https"_s)) {
        if (RefPtr documentLoader = document->frame() ? document->frame()->loader().documentLoader() : nullptr)
            return documentLoader->response().cacheControlContainsNoStore();
    }

    // Spinner issue from image search for bing.com.
    if (m_quirksData.isSite(QuirkSite::Bing)) {
        static MainThreadNeverDestroyed<const AtomString> imageSearchDialogID("sb_sbidialog"_s);
        if (RefPtr element = document->getElementById(imageSearchDialogID.get()))
            return element->renderer();
    }

    // Login issue on bankofamerica.com (rdar://104938789).
    if (m_quirksData.isSite(QuirkSite::BankOfAmerica)) {
        if (RefPtr window = document->window()) {
            if (window->hasEventListeners(eventNames().unloadEvent)) {
                static MainThreadNeverDestroyed<const AtomString> signInId("signIn"_s);
                static MainThreadNeverDestroyed<const AtomString> loadingClass("loading"_s);
                RefPtr signinButton = document->getElementById(signInId.get());
                return signinButton && signinButton->hasClassName(loadingClass.get());
            }
        }
    }

    if (m_quirksData.isSite(QuirkSite::GoogleProperty)) {
        // Google Docs used to bypass the back/forward cache by serving "Cache-Control: no-store" over HTTPS.
        // We started caching such content in r250437 but the Google Docs index page unfortunately is not currently compatible
        // because it puts an overlay (with class "docs-homescreen-freeze-el-full") over the page when navigating away and fails
        // to remove it when coming back from the back/forward cache (e.g. in 'pageshow' event handler). See <rdar://problem/57670064>.
        // Note that this does not check for docs.google.com host because of hosted G Suite apps.
        static MainThreadNeverDestroyed<const AtomString> googleDocsOverlayDivClass("docs-homescreen-freeze-el-full"_s);
        RefPtr firstChildInBody = document->body() ? document->body()->firstChild() : nullptr;
        if (RefPtr div = dynamicDowncast<HTMLDivElement>(firstChildInBody))
            return div->hasClassName(googleDocsOverlayDivClass);
    }

    return false;
}

// bungalow.com: rdar://61658940
// sfusd.edu: rdar://116292738
bool Quirks::shouldBypassAsyncScriptDeferring() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    // Deferring 'mapbox-gl.js' script on bungalow.com causes the script to get in a bad state (rdar://problem/61658940).
    // Deferring the google maps script on sfusd.edu may get the page in a bad state (rdar://116292738).
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldBypassAsyncScriptDeferring);
}

// smoothscroll JS library rdar://52712513
bool Quirks::shouldMakeEventListenerPassive(const EventTarget& eventTarget, const EventTypeInfo& eventType)
{
    auto eventTargetIsRoot = [](const EventTarget& eventTarget) {
        if (is<LocalDOMWindow>(eventTarget))
            return true;

        if (auto* node = dynamicDowncast<Node>(eventTarget)) {
            if (is<Document>(*node))
                return true;
            auto& document = node->document();
            return document.documentElement() == node || document.body() == node;
        }
        return false;
    };

    auto documentFromEventTarget = [](const EventTarget& eventTarget) -> Document* {
        return downcast<Document>(eventTarget.scriptExecutionContext());
    };

    if (eventType.isInCategory(EventCategory::TouchScrollBlocking)) {
        if (eventTargetIsRoot(eventTarget)) {
            if (RefPtr document = documentFromEventTarget(eventTarget))
                return document->settings().passiveTouchListenersAsDefaultOnDocument();
        }
        return false;
    }

    if (eventType.isInCategory(EventCategory::Wheel)) {
        if (eventTargetIsRoot(eventTarget)) {
            if (RefPtr document = documentFromEventTarget(eventTarget))
                return document->settings().passiveWheelListenersAsDefaultOnDocument();
        }
        return false;
    }

    return false;
}

bool Quirks::shouldEnableFacebookFlagQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldEnableFacebookFlagQuirk);
}

static Ref<Element> createFacebookFlagElement(Document& document, ASCIILiteral value)
{
    Ref text = Text::create(document, makeString("{\"require\":[[\"HasteSupportData\",\"handle\",null,[{\"gkxData\":{\""_s, value, "\":{\"result\":true,\"hash\":null}}}]]]}"_s));

    Ref script = HTMLScriptElement::create(HTMLNames::scriptTag, document, false);
    Ref { script->dataset() }->setNamedItem("contentLen"_s, AtomString { makeString(text->length()) });
    script->appendChild(text);

    return script;
}

static Vector<Ref<Element>> copyElements(const NodeList& nodeList)
{
    Vector<Ref<Element>> elements;
    for (size_t cptr = 0; cptr < nodeList.length(); ++cptr) {
        if (RefPtr element = dynamicDowncast<Element>(nodeList.item(cptr)))
            elements.append(element.releaseNonNull());
    }
    return elements;
}

Ref<NodeList> Quirks::applyFacebookFlagQuirk(Document& document, NodeList& nodeList)
{
    m_quirksData.setQuirkState(SiteSpecificQuirk::ShouldEnableFacebookFlagQuirk, false);

    if (!document.settings().facebookLiveRecordingQuirkEnabled())
        return nodeList;

    auto elements = copyElements(nodeList);
    // Live Streaming flag activation
    elements.append(createFacebookFlagElement(document, "23460"_s));
    return StaticElementList::create(WTF::move(elements));
}

bool Quirks::shouldEnableLegacyGetUserMediaQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldEnableLegacyGetUserMediaQuirk);
}

// zoom.us rdar://118185086
bool Quirks::shouldDisableImageCaptureQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableImageCaptureQuirk);
}

bool Quirks::shouldAllowMediaStreamTrackSerializationQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldAllowMediaStreamTrackSerializationQuirk);
}

bool Quirks::shouldEnableCameraAndMicrophonePermissionStateQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldEnableCameraAndMicrophonePermissionStateQuirk);
}

bool Quirks::shouldEnableRemoteTrackLabelQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldEnableRemoteTrackLabelQuirk);
}

bool Quirks::shouldEnableCameraBackgroundPlayback() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldEnableCameraBackgroundPlayback);
}

bool Quirks::shouldEnableSpeakerSelectionPermissionsPolicyQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldEnableSpeakerSelectionPermissionsPolicyQuirk);
}

bool Quirks::shouldEnableEnumerateDeviceQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldEnableEnumerateDeviceQuirk);
}

bool Quirks::shouldEnableRTCEncodedStreamsQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldEnableRTCEncodedStreamsQuirk) && m_document && m_document->settings().rtcEncodedStreamsQuirkEnabled();
}

// FIXME: Remove this Quirk if Pinterest decides to trigger this notification from an user gesture (rdar://165745719)
bool Quirks::shouldAllowNotificationPermissionWithoutUserGesture() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldAllowNotificationPermissionWithoutUserGesture);
}

bool Quirks::shouldUnloadHeavyFrame() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldUnloadHeavyFrames);
}

// hulu.com rdar://55041979
bool Quirks::needsCanPlayAfterSeekedQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsCanPlayAfterSeekedQuirk);
}

// wikipedia.org rdar://54856323
bool Quirks::shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraints() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    // FIXME: We should consider replacing this with a heuristic to determine whether
    // or not the edges of the page mostly lack content after shrinking to fit.
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk);
}

bool Quirks::shouldNotAutoUpgradeToHTTPSNavigation(const URL& url)
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return shouldNotAutoUpgradeToHTTPSNavigationInternal(url);
}

// kinja.com and related sites rdar://60601895
static bool isKinjaLoginAvatarElement(const Element& element)
{
    // The click event handler has been found to trigger on a div or
    // span with these class names, or the svg, or the svg's path.
    if (element.hasClass() && (element.hasClassName("js_switch-to-burner-login"_s)
        || element.hasClassName("js_header-userbutton"_s)
        || element.hasClassName("sc-1il3uru-3"_s) || element.hasClassName("cIhKfd"_s)
        || element.hasClassName("iyvn34-0"_s) || element.hasClassName("bYIjtl"_s))) {
        return true;
    }

    RefPtr<const Element> svgElement;
    if (is<SVGSVGElement>(element))
        svgElement = element;
    else if (is<SVGPathElement>(element) && is<SVGSVGElement>(element.parentElement()))
        svgElement = element.parentElement();

    return svgElement && svgElement->attributeWithoutSynchronization(HTMLNames::aria_labelAttr) == "UserFilled icon"_s;
}

// teams.microsoft.com https://bugs.webkit.org/show_bug.cgi?id=219505
bool Quirks::isMicrosoftTeamsRedirectURL(const URL& url)
{
    return urlHasQuirk(url, SiteSpecificQuirk::IsMicrosoftTeamsRedirectURLQuirk);
}

static bool isStorageAccessQuirkDomainAndElement(const URL& url, const Element& element)
{
    // Microsoft Teams login case.
    // FIXME(218779): Remove this quirk once microsoft.com completes their login flow redesign.
    if (url.host() == "www.microsoft.com"_s) {
        return element.hasClass()
        && (element.hasClassName("glyph_signIn_circle"_s)
        || element.hasClassName("mectrl_headertext"_s)
        || element.hasClassName("mectrl_header"_s));
    }
    // Sony Network Entertainment login case.
    // FIXME(218760): Remove this quirk once playstation.com completes their login flow redesign.
    if (url.host() == "www.playstation.com"_s || url.host() == "my.playstation.com"_s) {
        return element.hasClass()
        && (element.hasClassName("web-toolbar__signin-button"_s)
        || element.hasClassName("web-toolbar__signin-button-label"_s)
        || element.hasClassName("sb-signin-button"_s));
    }

    return false;
}

// playstation.com - rdar://72062985
bool Quirks::hasStorageAccessForAllLoginDomains(const HashSet<RegistrableDomain>& loginDomains, const RegistrableDomain& topFrameDomain)
{
    for (auto& loginDomain : loginDomains) {
        if (!ResourceLoadObserver::singleton().hasCrossPageStorageAccess(loginDomain, topFrameDomain))
            return false;
    }
    return true;
}

Quirks::StorageAccessResult Quirks::requestStorageAccessAndHandleClick(CompletionHandler<void(ShouldDispatchClick)>&& completionHandler) const
{
    RefPtr document = m_document.get();
    auto firstPartyDomain = RegistrableDomain(topDocumentURL());
    auto domainsInNeedOfStorageAccess = NetworkStorageSession::subResourceDomainsInNeedOfStorageAccessForFirstParty(firstPartyDomain);
    if (!domainsInNeedOfStorageAccess || domainsInNeedOfStorageAccess.value().isEmpty()) {
        completionHandler(ShouldDispatchClick::No);
        return Quirks::StorageAccessResult::ShouldNotCancelEvent;
    }

    if (hasStorageAccessForAllLoginDomains(*domainsInNeedOfStorageAccess, firstPartyDomain)) {
        completionHandler(ShouldDispatchClick::No);
        return Quirks::StorageAccessResult::ShouldNotCancelEvent;
    }

    auto domainInNeedOfStorageAccess = RegistrableDomain(*domainsInNeedOfStorageAccess.value().begin().get());

    if (!document) {
        completionHandler(ShouldDispatchClick::No);
        return Quirks::StorageAccessResult::ShouldNotCancelEvent;
    }

    document->addConsoleMessage(MessageSource::Other, MessageLevel::Info, makeString("requestStorageAccess is invoked on behalf of domain \""_s, domainInNeedOfStorageAccess.string(), "\""_s));
    DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(*document, WTF::move(domainInNeedOfStorageAccess), [firstPartyDomain, domainInNeedOfStorageAccess, completionHandler = WTF::move(completionHandler)](StorageAccessWasGranted storageAccessGranted) mutable {
        if (storageAccessGranted == StorageAccessWasGranted::No) {
            completionHandler(ShouldDispatchClick::Yes);
            return;
        }

        ResourceLoadObserver::singleton().setDomainsWithCrossPageStorageAccess({ { firstPartyDomain, Vector<RegistrableDomain> { domainInNeedOfStorageAccess } } }, [completionHandler = WTF::move(completionHandler)] () mutable {
            completionHandler(ShouldDispatchClick::Yes);
        });
    });
    return Quirks::StorageAccessResult::ShouldCancelEvent;
}

static bool isProbablyRegistrableDomainForBrand(const RegistrableDomain& domain, const String& brandName)
{
    return PublicSuffixStore::singleton().topPrivatelyControlledDomain(domain.string()).startsWithIgnoringASCIICase(makeString(brandName, "."_s));
}

void Quirks::triggerOptionalStorageAccessIframeQuirk(const URL& frameURL, CompletionHandler<void()>&& completionHandler) const
{
    if (RefPtr document = m_document.get()) {
        if (document->frame() && !m_document->frame()->isMainFrame()) {
            Ref mainFrame = document->frame()->mainFrame();
            if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(mainFrame); localMainFrame && localMainFrame->document()) {
                protect(localMainFrame->document())->quirks().triggerOptionalStorageAccessIframeQuirk(frameURL, WTF::move(completionHandler));
                return;
            }
        }

        bool isMSOLoginButNotMSTeams = document->url().hasQuery() && document->url().host() == "login.microsoftonline.com"_s && !document->url().query().contains("redirect_uri=https%3A%2F%2Fteams.microsoft.com"_s);
        bool isProbablyGoogleCCTLD = isProbablyRegistrableDomainForBrand(RegistrableDomain { document->url() }, "google"_s);
        bool isGoogleMyAccountForProfilePicture = isProbablyGoogleCCTLD && frameURL.hasQuery() && frameURL.host() == "myaccount.google.com"_s && frameURL.query().contains("startPath=profile-picture"_s);
        RegistrableDomain frameDomain { frameURL };
        if (isGoogleMyAccountForProfilePicture || (!isMSOLoginButNotMSTeams && subFrameDomainsForStorageAccessQuirk().contains(frameDomain))) {
            return DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(*document, WTF::move(frameDomain), [completionHandler = WTF::move(completionHandler)](StorageAccessWasGranted) mutable {
                completionHandler();
            });
        }
    }
    completionHandler();
}

// rdar://64549429
Quirks::StorageAccessResult Quirks::triggerOptionalStorageAccessQuirk(Element& element, const PlatformMouseEvent& platformEvent, const AtomString& eventType, int detail, Element* relatedTarget, bool isParentProcessAFullWebBrowser, IsSyntheticClick isSyntheticClick) const
{
    if (!DeprecatedGlobalSettings::trackingPreventionEnabled() || !isParentProcessAFullWebBrowser)
        return Quirks::StorageAccessResult::ShouldNotCancelEvent;

    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(Quirks::StorageAccessResult::ShouldNotCancelEvent);

    RegistrableDomain domain { protect(m_document)->url() };

    static NeverDestroyed<HashSet<RegistrableDomain>> kinjaQuirks = [] {
        HashSet<RegistrableDomain> set;
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("jalopnik.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("kotaku.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("theroot.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("theinventory.com"_s));
        return set;
    }();
    static NeverDestroyed kinjaURL = URL { "https://kinja.com"_str };
    static NeverDestroyed<RegistrableDomain> kinjaDomain { kinjaURL };

    static NeverDestroyed<RegistrableDomain> youTubeDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString("youtube.com"_s);

    static NeverDestroyed<String> loginPopupWindowFeatureString = "toolbar=no,location=yes,directories=no,status=no,menubar=no,scrollbars=yes,resizable=yes,copyhistory=no,width=599,height=600,top=420,left=980.5"_s;

    static NeverDestroyed<UserScript> kinjaLoginUserScript { "function triggerLoginForm() { let elements = document.getElementsByClassName('js_header-userbutton'); if (elements && elements[0]) { elements[0].click(); clearInterval(interval); } } let interval = setInterval(triggerLoginForm, 200);"_s, URL(aboutBlankURL()), Vector<String>(), Vector<String>(), UserScriptInjectionTime::DocumentEnd, UserContentInjectedFrames::InjectInTopFrameOnly };

    if (isAnyClick(eventType)) {
        RefPtr document = m_document.get();
        if (!document)
            return Quirks::StorageAccessResult::ShouldNotCancelEvent;

        // Embedded YouTube case.
        if (element.hasClass() && domain == youTubeDomain && !document->isTopDocument() && ResourceLoadObserver::singleton().hasHadUserInteraction(youTubeDomain)) {
            if (element.hasClassName("ytp-watch-later-icon"_s))
                DocumentStorageAccess::requestStorageAccessForDocumentQuirk(*document, [](StorageAccessWasGranted) { });
            return Quirks::StorageAccessResult::ShouldNotCancelEvent;
        }

        // Kinja login case.
        if (kinjaQuirks.get().contains(domain) && isKinjaLoginAvatarElement(element)) {
            if (ResourceLoadObserver::singleton().hasHadUserInteraction(kinjaDomain)) {
                DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(*document, kinjaDomain.get().isolatedCopy(), [](StorageAccessWasGranted) { });
                return Quirks::StorageAccessResult::ShouldNotCancelEvent;
            }

            RefPtr window = document->window();
            if (!window)
                return Quirks::StorageAccessResult::ShouldNotCancelEvent;

            ExceptionOr<RefPtr<WindowProxy>> proxyOrException =  window->open(*window, *window, kinjaURL->string(), emptyAtom(), loginPopupWindowFeatureString);
            if (proxyOrException.hasException())
                return Quirks::StorageAccessResult::ShouldNotCancelEvent;
            auto proxy = proxyOrException.releaseReturnValue();

            RefPtr abstractFrame = proxy->frame();
            if (RefPtr frame = dynamicDowncast<LocalFrame>(abstractFrame)) {
                auto world = ScriptController::createWorld("kinjaComQuirkWorld"_s, ScriptController::WorldType::User);
                frame->injectUserScriptImmediately(world.get(), kinjaLoginUserScript);
                return Quirks::StorageAccessResult::ShouldCancelEvent;
            }
        }

        // If the click is synthetic, the user has already gone through the storage access flow and we should not request again.
        if (isStorageAccessQuirkDomainAndElement(document->url(), element) && isSyntheticClick == IsSyntheticClick::No) {
            return requestStorageAccessAndHandleClick([element = WeakPtr { element }, platformEvent, eventType, detail, relatedTarget = WeakPtr { relatedTarget }] (ShouldDispatchClick shouldDispatchClick) mutable {
                RefPtr protectedElement = element.get();
                if (!protectedElement)
                    return;

                if (shouldDispatchClick == ShouldDispatchClick::Yes)
                    protectedElement->dispatchMouseEvent(platformEvent, eventType, detail, RefPtr { relatedTarget.get() }.get(), IsSyntheticClick::Yes);
            });
        }
    }
    return Quirks::StorageAccessResult::ShouldNotCancelEvent;
}

// facebook.com: rdar://67273166
// forbes.com:
// reddit.com: rdar://80550715
// x.com: rdar://73369869
bool Quirks::requiresUserGestureToPauseInPictureInPicture() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    // Facebook, X (twitter), and Reddit will naively pause a <video> element that has scrolled out of the viewport,
    // regardless of whether that element is currently in PiP mode.
    // We should remove the quirk once <rdar://problem/67273166>, <rdar://problem/73369869>, and <rdar://problem/80645747> have been fixed.
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::RequiresUserGestureToPauseInPictureInPictureQuirk);
}

// sports.yahoo.com: rdar://148284059
bool Quirks::requiresUserGestureToPauseInFullscreenAfterOrientationChange() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::RequiresUserGestureToPauseInFullscreenAfterOrientationChangeQuirk);
}

// youtube.com: rdar://178769976
bool Quirks::requiresUserGestureToPlayInFullscreen() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::RequiresUserGestureToPlayInFullscreenQuirk);
}

// bbc.co.uk: rdar://126494734
// bbc.com: rdar://157499149
bool Quirks::returnNullPictureInPictureElementDuringFullscreenChange() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ReturnNullPictureInPictureElementDuringFullscreenChangeQuirk);
}

// x.com: rdar://73369869
bool Quirks::requiresUserGestureToLoadInPictureInPicture() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    // X (Twitter) will remove the "src" attribute of a <video> element that has scrolled out of the viewport and
    // load the <video> element with an empty "src" regardless of whether that element is currently in PiP mode.
    // We should remove the quirk once <rdar://problem/73369869> has been fixed.
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::RequiresUserGestureToLoadInPictureInPictureQuirk);
}

// vimeo.com: rdar://problem/70788878
bool Quirks::blocksReturnToFullscreenFromPictureInPictureQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    // Some sites (e.g., vimeo.com) do not set element's styles properly when a video
    // returns to fullscreen from picture-in-picture. This quirk disables the "return to fullscreen
    // from picture-in-picture" feature for those sites. We should remove the quirk once
    // rdar://problem/73167931 has been fixed.
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::BlocksReturnToFullscreenFromPictureInPictureQuirk);
}

// vimeo.com: rdar://107592139
bool Quirks::blocksEnteringStandardFullscreenFromPictureInPictureQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    // Vimeo enters fullscreen when starting playback from the inline play button while already in PIP.
    // This behavior is revealing a bug in the fullscreen handling. See rdar://107592139.
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::BlocksEnteringStandardFullscreenFromPictureInPictureQuirk);
}

// espn.com: rdar://problem/73227900
// vimeo.com: rdar://problem/73227900
bool Quirks::shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    // This quirk disables the "webkitendfullscreen" event when a video enters picture-in-picture
    // from fullscreen for the sites which cannot handle the event properly in that case.
    // We should remove once the quirks have been fixed.
    // <rdar://90393832> vimeo.com
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk);
}

// teams.live.com rdar://88678598
// teams.microsoft.com rdar://90434296
bool Quirks::shouldAllowNavigationToCustomProtocolWithoutUserGesture(StringView protocol, const SecurityOriginData& requesterOrigin)
{
    if (protocol != "msteams"_s)
        return false;
    return urlHasQuirk(requesterOrigin.toURL(), SiteSpecificQuirk::ShouldAllowMSTeamsProtocolWithoutUserGestureQuirk);
}

// espn.com: rdar://problem/95651814
bool Quirks::allowLayeredFullscreenVideos() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::AllowLayeredFullscreenVideos);
}

// x.com: rdar://132850672
// FIXME (rdar://124579556): Remove once 'x.com' adjusts video handling for visionOS.
bool Quirks::shouldDisableFullscreenVideoAspectRatioAdaptiveSizing() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk);
}

// play.hbomax.com https://bugs.webkit.org/show_bug.cgi?id=244737
bool Quirks::shouldEnableFontLoadingAPIQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (m_document->settings().downloadableBinaryFontTrustedTypes() == DownloadableBinaryFontTrustedTypes::Any)
        return false;

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldEnableFontLoadingAPIQuirk);
}

// play.hbomax.com rdar://158430821
bool Quirks::shouldDisableAdSkippingInPip() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableAdSkippingInPip);
}

// hulu.com rdar://100199996
bool Quirks::needsVideoShouldMaintainAspectRatioQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsVideoShouldMaintainAspectRatioQuirk);
}

// Marcus: <rdar://101086391>.
// Pandora: <rdar://100243111>.
// Soundcloud: <rdar://102913500>.
bool Quirks::shouldExposeShowModalDialog() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldExposeShowModalDialog);
}

// marcus.com rdar://102959860
bool Quirks::shouldNavigatorPluginsBeEmpty() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldNavigatorPluginsBeEmpty);
}

// Fix for the UNIQLO app (rdar://104519846).
bool Quirks::shouldDisableLazyIframeLoadingQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableLazyIframeLoadingQuirk);
}

// Moon Player app (rdar://162452658): the app hides its WKWebView while continuing to display
// the video layer it hosts, so page visibility does not indicate whether the video is on screen.
// Tearing the layer down when the page becomes hidden leaves the app displaying a black frame.
bool Quirks::shouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk);
}

// reddit.com with Sink It extension (rdar://176377447) and apple.com/retail (rdar://181007316).
bool Quirks::shouldDisableScrollAnchoringQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableScrollAnchoringQuirk))
        return false;

#if PLATFORM(IOS_FAMILY)
    // reddit.com only disables scroll anchoring while the Sink It extension's element is present.
    if (m_quirksData.isSite(QuirkSite::Reddit)) {
        RefPtr document = m_document.get();
        if (!document)
            return false;

        static MainThreadNeverDestroyed<const AtomString> sinkItBackToTopID("sink-it-back-to-top"_s);
        return !!document->getElementById(sinkItBackToTopID.get());
    }
#endif

    return true;
}

// Breaks express checkout on victoriassecret.com (rdar://104818312).
bool Quirks::shouldDisableFetchMetadata() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableFetchMetadata);
}

bool Quirks::shouldBlockFetchWithNewlineAndLessThan() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldBlockFetchWithNewlineAndLessThan);
}

// Push state file path restrictions break Mimeo Photo Plugin (rdar://112445672).
bool Quirks::shouldDisablePushStateFilePathRestrictions() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisablePushStateFilePathRestrictions);
}

// ungap/@custom-elements polyfill (rdar://problem/111008826).
bool Quirks::needsConfigurableIndexedPropertiesQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_needsConfigurableIndexedPropertiesQuirk;
}

// Canvas fingerprinting (rdar://107564162)
String Quirks::advancedPrivacyProtectionSubstituteDataURLForScriptWithFeatures(const String& lastDrawnText, int canvasWidth, int canvasHeight) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE({ });

    Ref document = *m_document;
    if (!document->settings().canvasFingerprintingQuirkEnabled() || !document->noiseInjectionHashSalt())
        return { };

    if ("<@nv45. F1n63r,Pr1n71n6!"_s != lastDrawnText || canvasWidth != 280 || canvasHeight != 60)
        return { };

    if (!document->globalObject())
        return { };

    Ref vm = document->globalObject()->vm();
    auto* callFrame = vm->topCallFrame;
    if (!callFrame)
        return { };

    bool sourceMatchesExpectedLength = false;
    JSC::StackVisitor::visit(callFrame, vm.get(), [&](auto& visitor) {
        if (visitor->isImplementationVisibilityPrivate())
            return IterationStatus::Continue;

        auto* codeBlock = visitor->codeBlock();
        if (!codeBlock)
            return IterationStatus::Continue;

        auto* scriptExecutable = codeBlock->ownerExecutable();
        if (!scriptExecutable)
            return IterationStatus::Continue;

        RefPtr sourceProvider = scriptExecutable->source().provider();
        if (!sourceProvider)
            return IterationStatus::Continue;

        auto sourceCodeLength = sourceProvider->source().length();
        sourceMatchesExpectedLength = sourceCodeLength == 212053 || sourceCodeLength == 219192;
        return IterationStatus::Done;
    });

    if (!sourceMatchesExpectedLength)
        return { };

    return "data:image/png;base64,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A"_s;
}

// DOFUS Touch app (rdar://112679186)
bool Quirks::needsResettingTransitionCancelsRunningTransitionQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsResettingTransitionCancelsRunningTransitionQuirk);
#else
    return false;
#endif
}

// Microsoft office online generates data URLs with incorrect padding on Safari only (rdar://114573089).
bool Quirks::shouldDisableDataURLPaddingValidation() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableDataURLPaddingValidation);
}

bool Quirks::needsDisableDOMPasteAccessQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return quirkIsEnabledAfterProbing(SiteSpecificQuirk::NeedsDisableDOMPasteAccessQuirk, [&] {
        RefPtr document = m_document.get();
        if (!document)
            return false;
        auto* globalObject = document->globalObject();
        if (!globalObject)
            return false;

        Ref vm = globalObject->vm();
        JSC::JSLockHolder lock(vm);
        auto tableauPrepProperty = JSC::Identifier::fromString(vm, "tableauPrep"_s);
        return globalObject->hasProperty(globalObject, tableauPrepProperty);
    });
}

// rdar://133423460
bool Quirks::shouldPreventOrientationMediaQueryFromEvaluatingToLandscape() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk);
}

// rdar://133423460
bool Quirks::shouldFlipScreenDimensions() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldFlipScreenDimensionsQuirk);
}

// rdar://175565114
bool Quirks::shouldAvoidProgrammaticScrollClamping() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldAvoidProgrammaticScrollClampingQuirk);
}

// This section is dedicated to UA override for iPad. iPads (but iPad Mini) are sending a desktop user agent
// to websites. In some cases, the website breaks in some ways, not expecting a touch interface for the website.
// Controls not active or too small, form factor, etc. In this case it is better to send the iPad Mini UA.
// FIXME: find the reference radars and/or bugs.webkit.org issues on why these were added in the first place.
// FIXME: There is no check currently on needsQuirks(), this needs to be fixed so it makes it easier
// to deactivate them for testing.
bool Quirks::needsIPadMiniUserAgent(const URL& url)
{
    return urlHasQuirk(url, SiteSpecificQuirk::NeedsIPadMiniUserAgentQuirk);
}

bool Quirks::needsIPhoneUserAgent(const URL& url)
{
    return urlHasQuirk(url, SiteSpecificQuirk::NeedsIPhoneUserAgentQuirk);
}

std::optional<String> Quirks::needsCustomUserAgentOverride(const URL& url, const String& applicationNameForUserAgent, const String& currentUserAgent)
{
    RegistrableDomain hostDomain { url };
    auto& domainString = hostDomain.string();
    auto firefoxUserAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:139.0) Gecko/20100101 Firefox/139.0"_s;
    // FIXME(rdar://83078414): Remove once 101edu.co and aktiv.com removes the unsupported message.
    if (domainString == "app.101edu.co"_s)
        return firefoxUserAgent;
    if (domainString == "app.aktiv.com"_s)
        return firefoxUserAgent;

    auto chromeUserAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36"_s;
#if PLATFORM(IOS)
    // amazon.com rdar://117771731
    if (isProbablyRegistrableDomainForBrand(hostDomain, "amazon"_s) && url.path() == "/gp/video/"_s)
        return chromeUserAgent;
#endif

    if ((domainString == "messenger.com"_s || domainString == "facebook.com"_s) && url.path().startsWith("/groupcall/ROOM:"_s))
        return chromeUserAgent;

    // Outlook detects Safari and handles selections incorrectly in their rich text editor roosterjs
    auto host = url.host();
    if (host == "outlook.live.com"_s)
        return chromeUserAgent;

#if PLATFORM(COCOA)
    // FIXME(rdar://148759791): Remove this once TikTok removes the outdated error message.
    if (domainString == "tiktok.com"_s) {
        auto baseUA = currentUserAgent.isEmpty() ? standardUserAgentWithApplicationName(applicationNameForUserAgent) : currentUserAgent;
        return makeStringByReplacingAll(baseUA, "like Gecko"_s, "like Gecko, like Chrome/136."_s);
    }

    // mms.pinduoduo.com https://bugs.webkit.org/b/318201
    if (url.host() == "mms.pinduoduo.com"_s) {
        auto baseUA = currentUserAgent.isEmpty() ? standardUserAgentWithApplicationName(applicationNameForUserAgent) : currentUserAgent;
        return makeStringByReplacingAll(baseUA, "like Gecko"_s, "like Gecko, like Chrome/149."_s);
    }

    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=319011 or rdar://181825035):
    // github.com serves Safari some JS that tries to adjust the scroll position
    // which interferes with WebKit's scroll to fragment implementation.
    // Presenting a Chrome-like UA takes the working code path.
    if (domainString == "github.com"_s) {
        auto baseUA = currentUserAgent.isEmpty() ? standardUserAgentWithApplicationName(applicationNameForUserAgent) : currentUserAgent;
        return makeStringByReplacingAll(baseUA, "like Gecko"_s, "like Gecko, like Chrome/151."_s);
    }
#else
    UNUSED_PARAM(applicationNameForUserAgent);
    UNUSED_PARAM(currentUserAgent);
#endif
    return { };
}

bool Quirks::needsDesktopUserAgent(const URL& url)
{
    return needsDesktopUserAgentInternal(url);
}

bool Quirks::needsPartitionedCookies(const ResourceRequest& request)
{
    if (request.isTopSite())
        return false;
    return urlHasQuirk(request.url(), SiteSpecificQuirk::NeedsPartitionedCookiesQuirk);
}

// premierleague.com: rdar://123721211
bool Quirks::shouldIgnorePlaysInlineRequirementQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldIgnorePlaysInlineRequirementQuirk);
#else
    return false;
#endif
}

// m365.cloud.microsoft rdar://157794706
// Allow popups from m365.cloud.microsoft to onedrive.live.com
bool Quirks::needsPopupFromMicrosoftOfficeToOneDrive(const URL& targetURL) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return targetURL.host().endsWithIgnoringASCIICase("onedrive.live.com"_s);
}

bool Quirks::needsConsistentQueryParameterFilteringQuirk(const URL& url) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_document->settings().consistentQueryParameterFilteringQuirkEnabled())
        return false;

    static bool wasLoggedOnce { false };

    if (equalLettersIgnoringASCIICase(url.host(), "bundle-file"_s)
        || equalLettersIgnoringASCIICase(RegistrableDomain { url }.string(), "consistentqueryparameterfiltering.internal"_s))
        return true;

    bool enableQuirk = m_document->settings().consistentQueryParameterFilteringInternalQuirkEnabled()
        && needsConsistentQueryParameterFilteringInternal(URL { url.string().foldCase() });

    if (RefPtr page = m_document->page())
        enableQuirk |= page->requiresConsistentPrivacyQuirkForDomain(url);

    if (enableQuirk && !wasLoggedOnce) {
        RELEASE_LOG(Loading, "Quirks::needsConsistentQueryParameterFilteringQuirk: Enabling consistent privacy protections");
        protect(m_document)->addConsoleMessage(MessageSource::Other, MessageLevel::Info, makeString("Enabling consistent privacy protections on \""_s, url.string(), "\""_s));
        wasLoggedOnce = true;
    }

    return enableQuirk;
}

bool Quirks::mayBenefitFromFingerprintingProtectionQuirk(const URL& url) const
{
    // FIXME: Placeholder for now.
    return needsConsistentQueryParameterFilteringQuirk(url);
}

#if PLATFORM(COCOA)

#if !PLATFORM(IOS_FAMILY)
static constexpr auto frozenVersion = "10_15_7"_s;
#elif PLATFORM(WATCHOS)
static constexpr auto frozenVersion = "11_6_1"_s;
#elif PLATFORM(APPLETV)
static constexpr auto frozenVersion = "18_6"_s;
#else
static constexpr auto frozenVersion = "18_7"_s;
#endif

String Quirks::standardUserAgentWithApplicationNameIncludingCompatOverrides(const String& applicationName, const String& userAgentOSVersion, UserAgentType type)
{
    auto overriddenUAString = standardUserAgentWithApplicationNameIncludingCompatOverridesInternal(applicationName, userAgentOSVersion, type);
    if (overriddenUAString.length())
        return overriddenUAString;

    if (userAgentOSVersion == frozenVersion)
        return { };

    return standardUserAgentWithApplicationName(applicationName, frozenVersion, type);
}

#endif

// news.ycombinator.com: rdar://127246368
bool Quirks::shouldIgnoreTextAutoSizing() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldIgnoreTextAutoSizingQuirk);
}

std::optional<TargetedElementSelectors> Quirks::defaultVisibilityAdjustmentSelectors(const URL& requestURL)
{
#if ENABLE(VISIBILITY_ADJUSTMENT_QUIRKS)
    return defaultVisibilityAdjustmentSelectorsInternal(requestURL);
#else
    UNUSED_PARAM(requestURL);
    return { };
#endif
}

String Quirks::scriptToEvaluateBeforeRunningScriptFromURL(const URL& scriptURL)
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE({ });

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk))
        return { };

    if (scriptURL.isEmpty())
        return { };

    // iheart.com rdar://171198911
    if (m_quirksData.isSite(QuirkSite::IHeart))
        return "document.cookie = 'app=listen:60; path=/; domain=.iheart.com';"_s;

    // bestbuy.com rdar://136235936
    if (m_quirksData.isSite(QuirkSite::BestBuy)) [[unlikely]]
        return "Object.defineProperty(navigator,'language',{get:function(){return'en-US'}});Object.defineProperty(navigator,'languages',{get:function(){return['en-US','en']}});"_s;

#if PLATFORM(IOS_FAMILY)
    // player.anyclip.com rdar://138789765
    if ((m_quirksData.isSite(QuirkSite::Dictionary) || m_quirksData.isSite(QuirkSite::Thesaurus)) && scriptURL.lastPathComponent().endsWith("lre.js"_s)) [[unlikely]] {
        if (scriptURL.host() == "player.anyclip.com"_s)
            return chromeUserAgentScript;
    }

    if (m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsGoogleTranslateScrollingQuirk)) [[unlikely]]
        return chromeUserAgentScript;

    // nba.com rdar://147429596
    if (m_quirksData.isSite(QuirkSite::NBA) && !scriptURL.isEmpty()) [[unlikely]]
        return nbaSeekBarFixScript;

#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
    if (m_quirksData.isSite(QuirkSite::WebEx) && scriptURL.lastPathComponent().startsWith("pushdownload."_s)) [[unlikely]]
        return "Object.defineProperty(window, 'Touch', { get: () => undefined });"_s;
#endif
#endif

    // ceac.state.gov rdar://170258502
    if (m_quirksData.isSite(QuirkSite::CEAC) && scriptURL.lastPathComponent() == "CheckBrowserClose.js"_s) [[unlikely]]
        return ceacBeforeUnloadFixScript;

    // invideo.io https://webkit.org/b/311602
    if (m_quirksData.isSite(QuirkSite::InVideo)) [[unlikely]]
        return "if(!window.chrome)window.chrome={};"_s;

    return { };
}

// disneyplus: rdar://137613110
bool Quirks::shouldHideCoarsePointerCharacteristics() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldHideCoarsePointerCharacteristicsQuirk);
}

// hulu.com rdar://126096361
bool Quirks::implicitMuteWhenVolumeSetToZero() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ImplicitMuteWhenVolumeSetToZero);
}

bool Quirks::shouldOmitTouchEventDOMAttributesForDesktopWebsite(const URL& requestURL)
{
    return urlHasQuirk(requestURL, SiteSpecificQuirk::ShouldOmitTouchEventDOMAttributesForDesktopWebsiteQuirk);
}

// netflix.com: rdar://155498882
// soylent.*: rdar://113314067
bool Quirks::shouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDispatchPointerOutAndLeaveAfterHandlingSyntheticClick);
}

// hbomax.com: rdar://138424489
bool Quirks::needsZeroMaxTouchPointsQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsZeroMaxTouchPointsQuirk);
}

// imdb.com: rdar://137991466
bool Quirks::needsChromeMediaControlsPseudoElement() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsChromeMediaControlsPseudoElementQuirk);
}

static AccessibilityRole accessibilityRole(const Element& element)
{
    return AccessibilityObject::ariaRoleToWebCoreRole(element.attributeWithoutSynchronization(HTMLNames::roleAttr));
}

// walmart.com: rdar://123734840
// live.outlook.com: rdar://152277211
bool Quirks::shouldIgnoreContentObservationForClick(const Node& targetNode) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::MayNeedToIgnoreContentObservation))
        return false;

    if (m_quirksData.isSite(QuirkSite::GoogleMaps)) {
        for (Ref ancestor : lineageOfType<HTMLElement>(targetNode)) {
            if (ancestor->attributeWithoutSynchronization(HTMLNames::aria_labelAttr) == "Suggestions"_s)
                return true;
        }
        return false;
    }

    RefPtr target = dynamicDowncast<Element>(targetNode);
    if (m_quirksData.isSite(QuirkSite::Outlook)) {
        if (target && target->getIdAttribute().startsWith("swatchColorPicker"_s))
            return true;
    }

    if (m_quirksData.isSite(QuirkSite::Walmart)) {
        if (!target || accessibilityRole(*target) != AccessibilityRole::Button)
            return false;

        RefPtr parent = target->parentElementInComposedTree();
        if (!parent || accessibilityRole(*parent) != AccessibilityRole::ListItem)
            return false;
    }

    return true;
}

bool Quirks::shouldHideSoftTopScrollEdgeEffectDuringFocus(const Element& focusedElement) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldHideSoftTopScrollEdgeEffectDuringFocusQuirk))
        return false;

    return focusedElement.getIdAttribute().contains("crossword"_s);
}

// cbssports.com <rdar://139478801>.
// docs.google.com <rdar://59402637>.
bool Quirks::shouldSynthesizeTouchEventsAfterNonSyntheticClick(const Element& target) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk))
        return false;

    if (m_quirksData.isSite(QuirkSite::CBSSports))
        return target.nodeName() == "AVIA-BUTTON"_s;

    if (m_quirksData.isSite(QuirkSite::GoogleDocs)) {
        unsigned numberOfAncestorsToCheck = 3;
        for (Ref ancestor : lineageOfType<HTMLElement>(target)) {
            if (ancestor->hasClassName("docs-ml-promotion-action-container"_s))
                return true;

            if (!--numberOfAncestorsToCheck)
                break;
        }
    }

    return false;
}

bool Quirks::needsChromeOSNavigatorUserAgentQuirk(const Document& document) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsChromeOSNavigatorUserAgentQuirk))
        return false;

    if (document.url().lastPathComponent() != "wordeditorframe.aspx"_s)
        return false;

    if (document.currentSourceURL().lastPathComponent() != "wordeditords.js"_s)
        return false;

    return true;
}

// instagram.com: rdar://174936655
bool Quirks::shouldSendFakeTouchForceChangeEvent() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldSendFakeTouchForceChangeEvent);
}

// store.steampowered.com: rdar://142573562
bool Quirks::shouldTreatAddingMouseOutEventListenerAsContentChange() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldTreatAddingMouseOutEventListenerAsContentChange);
}

// outlook.live.com: rdar://136624720
bool Quirks::needsMozillaFileTypeForDataTransfer() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsMozillaFileTypeForDataTransferQuirk);
}

// spotify.com rdar://171119015
bool Quirks::shouldLimitHLSPlaybackRate() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldLimitHLSPlaybackRate);
}

// nfl.com:
bool Quirks::shouldSuppressHLSSubtitles() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldSuppressHLSSubtitles);
}

// spotify.com: block additive audible playback (e.g. Home-page track previews) while another
// audible media element is already playing in the document.
bool Quirks::shouldBlockAudiblePlaybackWhileAudioIsPlaying() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldBlockAudiblePlaybackWhileAudioIsPlaying);
}

bool Quirks::shouldSuppressMediaSessionPauseActionOnInterruption() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldSuppressMediaSessionPauseActionOnInterruption);
}

// spotify.com rdar://140707449
bool Quirks::shouldAvoidStartingSelectionOnMouseDownOverPointerCursor(const Node& target) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor))
        return false;

    if (auto* style = target.renderStyle()) {
        if (style->cursorType() == CursorType::Pointer)
            return true;
    }

    return false;
}

bool Quirks::shouldReuseLiveRangeForSelectionUpdate() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsReuseLiveRangeForSelectionUpdateQuirk);
}

#if PLATFORM(IOS_FAMILY)

bool Quirks::needsPointerTouchCompatibility(const Element& target) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (WTF::IOSApplication::isFeedly()) {
        RefPtr pageContainer = [&target] -> const HTMLElement* {
            for (Ref ancestor : lineageOfType<HTMLElement>(target)) {
                if (ancestor->hasClassName("PageContainer"_s))
                    return ancestor.unsafePtr();
            }
            return nullptr;
        }();
        if (pageContainer) {
            if (RefPtr article = descendantsOfType<HTMLArticleElement>(*pageContainer).first())
                return article->hasClassName("MobileFullEntry"_s);
        }
    } else if (WTF::IOSApplication::isAmazon()) {
        for (Ref ancestor : lineageOfType<HTMLElement>(target)) {
            if (ancestor->hasClassName("a-gesture-horizontal"_s))
                return true;
        }
    }

    return false;
}

#endif

// facebook.com rdar://141103350
bool Quirks::needsFacebookStoriesCreationFormQuirk(const Element& element, const Style::ComputedStyle& computedStyle) const
{
#if PLATFORM(IOS_FAMILY)
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.isSite(QuirkSite::Facebook))
        return false;

    if (!topDocumentURL().path().startsWith("/stories/create"_s)) {
        m_facebookStoriesCreationFormContainer = { };
        return false;
    }

    Ref document = element.document();
    RefPtr loader = document->loader();
    if (!loader) [[unlikely]]
        return false;

    if (loader->metaViewportPolicy() != MetaViewportPolicy::Ignore)
        return false;

    RefPtr view = document->view();
    if (!view) [[unlikely]]
        return false;

    float width = view->sizeForCSSDefaultViewportUnits().width();
    if (width < 800 || width > 900)
        return false;

    if (m_facebookStoriesCreationFormContainer)
        return m_facebookStoriesCreationFormContainer.get() == &element;

    if (computedStyle.display() != Style::DisplayType::None)
        return false;

    if (accessibilityRole(element) != AccessibilityRole::LandmarkNavigation)
        return false;

    if (!descendantsOfType<HTMLTextAreaElement>(element).first())
        return false;

    m_facebookStoriesCreationFormContainer = element;
    return true;
#else
    UNUSED_PARAM(element);
    UNUSED_PARAM(computedStyle);
    return false;
#endif
}

// Expedia Group sites (hotels.com, expedia.*, orbitz.com, …) rdar://126631968
bool Quirks::needsExpediaGroupAnimationQuirk(Element& element) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsExpediaGroupAnimationQuirk))
        return false;

    // Quick pre-filter to avoid running the full selector match on ~99% of elements.
    // We also check for uitk-menu-open to only apply the opening animation fix
    // when the menu is actively being opened, not in its closed state.
    if (!element.hasClassName("uitk-menu-container"_s) || !element.hasClassName("uitk-menu-open"_s))
        return false;

    auto matches = Ref { element }->matches(".uitk-menu-mounted .uitk-menu-container.uitk-menu-container-autoposition.uitk-menu-container-has-intersection-root-el"_s);
    return !matches.hasException() && matches.returnValue();
}

// claude.ai rdar://162616694
bool Quirks::needsClaudeSidebarViewportUnitQuirk(Element& element, const Style::ComputedStyle& style) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsClaudeSidebarViewportUnitQuirk))
        return false;

    if (style.position() != PositionType::Fixed)
        return false;

    if (element.attributeWithoutSynchronization(HTMLNames::aria_labelAttr) != "Sidebar"_s)
        return false;

    if (auto fixedHeight = style.height().tryFixed()) {
        if (fixedHeight->resolveZoom(style.usedZoomForLength()) == protect(m_document->renderView())->sizeForCSSDefaultViewportUnits().height())
            return true;
    }

    return false;
}

bool Quirks::needsHideSelectionDuringOverflowScrollQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);
    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsHideSelectionDuringOverflowScrollQuirk);
}

// amazon.design rdar://175953409
bool Quirks::needsAmazonDesignMenuViewportUnitQuirk(const Style::ComputedStyle& style, const Style::ComputedStyle& parentStyle) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsAmazonDesignMenuViewportUnitQuirk))
        return false;

    if (style.display() != Style::DisplayType::BlockFlex)
        return false;

    if (!style.usesViewportUnits())
        return false;

    auto fixedPaddingTop = parentStyle.paddingTop().tryFixed();
    if (!fixedPaddingTop)
        return false;

    auto fixedHeight = style.height().tryFixed();
    if (!fixedHeight)
        return false;

    auto resolvedPaddingTop = fixedPaddingTop->resolveZoom(Style::ZoomFactor::none());
    auto resolvedHeight = fixedHeight->resolveZoom(Style::ZoomFactor::none());
    auto dynamicViewportHeight = protect(m_document->renderView())->sizeForCSSDynamicViewportUnits().height();

    return (resolvedPaddingTop + resolvedHeight) > dynamicViewportHeight;
}

bool Quirks::needsLimitedMatroskaSupport() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsLimitedMatroskaSupportQuirk);
}

bool Quirks::needsSupportsProgressMonitoring() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsSupportsProgressMonitoringQuirk);
}

// rdar://174779259.
// Anthropic's claude.ai SPA, after a logout, leaves some identification cookies behind.
// On the next /chat boot those non-auth cookies are enough to push the SPA into an
// authenticated boot path; the bootstrap call 403s with "account_session_invalid", and
// the SPA reacts with location.href = '/logout?...', producing an indefinite /chat <->
// /logout loop. The bug seems to be on Anthropic's side; this quirk works around it by
// performing the cookie cleanup the server forgot, when we observe a successful fetch
// to claude.ai/api/auth/logout.
void Quirks::clearLogoutSurvivingIdentityCookiesIfNeeded(const URL& fetchURL, int httpStatusCode)
{
    if (!needsQuirks()) [[unlikely]]
        return;

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsLogoutCookieCleanupQuirk))
        return;

    if (httpStatusCode < 200 || httpStatusCode >= 300)
        return;

    if (!equalLettersIgnoringASCIICase(fetchURL.host(), "claude.ai"_s))
        return;

    if (fetchURL.path() != "/api/auth/logout"_s)
        return;

    RefPtr document = m_document.get();
    if (!document)
        return;

    RefPtr page = document->page();
    if (!page)
        return;

    auto& documentURL = document->url();
    static constexpr std::array cookiesToDelete = { "__ssid"_s, "__cf_bm"_s, "anthropic-device-id"_s, "lastActiveOrg"_s, "activitySessionId"_s };
    for (auto& cookieName : cookiesToDelete)
        page->cookieJar().deleteCookie(*document, documentURL, cookieName, [] { });
}

bool Quirks::needsCustomUserAgentData() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsCustomUserAgentData);
}

bool Quirks::needsNavigatorUserAgentDataQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsNavigatorUserAgentDataQuirk);
}

bool Quirks::needsNowPlayingFullscreenSwapQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsNowPlayingFullscreenSwapQuirk);
}

bool Quirks::needsSuppressPostLayoutBoundaryEventsQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsSuppressPostLayoutBoundaryEventsQuirk);
}

// tiktok.com rdar://149712691
std::optional<Quirks::TikTokOverflowingContentQuirkType> Quirks::needsTikTokOverflowingContentQuirk(const Element& element, const Style::ComputedStyle& parentStyle) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE({ });

    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsTikTokOverflowingContentQuirk))
        return { };

    if (parentStyle.display() != Style::DisplayType::BlockFlex)
        return { };

    if (parentStyle.position() != PositionType::Fixed)
        return { };

    if (!element.elementData() || !element.hasClass())
        return { };

    static NeverDestroyed<AtomString> contentContainerSubstring { "DivContentContainer"_s };
    static NeverDestroyed<AtomString> videoContainerSubstring { "DivVideoContainer"_s };
    static NeverDestroyed<AtomString> browserModeContainerSubstring { "DivBrowserModeContainer"_s };

    auto parentElementClassNamesContainsBrowserModeContainerSubstring = [&] {
        RefPtr parentElement = element.parentElement();
        if (!parentElement || !parentElement->elementData() || !parentElement->hasClass())
            return false;

        for (auto& className : parentElement->classNames()) {
            if (className.contains(browserModeContainerSubstring.get()))
                return true;
        }
        return false;
    };

    if (!parentElementClassNamesContainsBrowserModeContainerSubstring())
        return { };

    for (auto& className : element.classNames()) {
        if (className.contains(contentContainerSubstring.get()))
            return TikTokOverflowingContentQuirkType::CommentsSectionQuirk;

        if (className.contains(videoContainerSubstring.get()))
            return TikTokOverflowingContentQuirkType::VideoSectionQuirk;
    }

    return { };
}

// rdar://166400170
bool Quirks::needsInstagramResizingReelsQuirk(const Element& element, const Style::ComputedStyle& elementStyle, const Style::ComputedStyle& parentStyle) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

#if ENABLE(VIDEO)
    if (!m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsInstagramResizingReelsQuirk))
        return false;

    if (elementStyle.display() != Style::DisplayType::BlockFlow)
        return false;

    if (elementStyle.isOverflowVisible())
        return false;

    if (!elementStyle.width().isAuto())
        return false;

    if (parentStyle.display() != Style::DisplayType::BlockFlex)
        return false;

    if (!parentStyle.width().isPercent())
        return false;

    return descendantsOfType<HTMLVideoElement>(element).first();
#else
    UNUSED_PARAM(element);
    UNUSED_PARAM(elementStyle);
    UNUSED_PARAM(parentStyle);
    return false;
#endif // ENABLE(VIDEO)
}

bool Quirks::needsWebKitMediaTextTrackDisplayQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsWebKitMediaTextTrackDisplayQuirk);
}

// rdar://138806698
bool Quirks::shouldSupportHoverMediaQueries() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldSupportHoverMediaQueriesQuirk);
}

bool Quirks::shouldRewriteMediaRangeRequestForURL(const URL& url) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsMediaRewriteRangeRequestQuirk) && RegistrableDomain(url).string() == "bing.com"_s;
}

// rdar://106770785
bool Quirks::shouldPreventKeyframeEffectAcceleration(const KeyframeEffect& effect) const
{
    if (!needsQuirks() || !m_quirksData.isSite(QuirkSite::EA))
        return false;

    auto target = effect.targetStyleable();
    return target && target->element.localName() == "ea-network-nav"_s;
}

bool Quirks::shouldDisableThreadedAnimationsQuirk() const
{
    return needsQuirks() && m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableThreadedAnimationsQuirk);
}

bool Quirks::shouldEnterNativeFullscreenWhenCallingElementRequestFullscreenQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldEnterNativeFullscreenWhenCallingElementRequestFullscreen);
}

bool Quirks::shouldDelayReloadWhenRegisteringServiceWorker() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDelayReloadWhenRegisteringServiceWorker);
}

bool Quirks::shouldDisableDOMAudioSessionQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldDisableDOMAudioSession);
}

bool Quirks::shouldComparareUsedValuesForBorderWidthForTriggeringTransitions() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldComparareUsedValuesForBorderWidthForTriggeringTransitions);
}

bool Quirks::shouldReportVisibleDueToActivePictureInPictureContent() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::ShouldReportDocumentAsVisibleIfActivePIPQuirk);
}

bool Quirks::needsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.quirkIsEnabled(SiteSpecificQuirk::NeedsWebKitMediaKeysTransportStreamIsTypeSupportedQuirk);
}

URL Quirks::topDocumentURL() const
{
    if (!m_topDocumentURLForTesting.isEmpty()) [[unlikely]]
        return m_topDocumentURLForTesting;

    return protect(m_document)->topURL();
}

void Quirks::setTopDocumentURLForTesting(URL&& url)
{
    m_topDocumentURLForTesting = WTF::move(url);
    determineRelevantQuirks();
}

void Quirks::determineRelevantQuirks()
{
    RELEASE_ASSERT(m_document);
    m_quirksData = { };
    m_elementConditions = { };
    m_probedQuirks = { };

#if PLATFORM(IOS_FAMILY)
    static const bool shouldDisableLazyIframeLoadingQuirk = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoUNIQLOLazyIframeLoadingQuirk) && WTF::IOSApplication::isUNIQLOApp();
    static const bool needsResettingTransitionCancelsRunningTransitionQuirk = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ResettingTransitionCancelsRunningTransitionQuirk) && WTF::IOSApplication::isDOFUSTouch();
    static const bool shouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoMediaLayerTeardownOnPageVisibilityChangeQuirk) && WTF::IOSApplication::isMoonPlayer();

    m_quirksData.setQuirkState(SiteSpecificQuirk::ShouldDisableLazyIframeLoadingQuirk, shouldDisableLazyIframeLoadingQuirk);

    // DOFUS Touch app (rdar://112679186)
    m_quirksData.setQuirkState(SiteSpecificQuirk::NeedsResettingTransitionCancelsRunningTransitionQuirk, needsResettingTransitionCancelsRunningTransitionQuirk);

    // Moon Player app (rdar://162452658)
    m_quirksData.setQuirkState(SiteSpecificQuirk::ShouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk, shouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk);
#endif

#if PLATFORM(MAC)
    static const bool shouldDisablePushStateFilePathRestrictions = WTF::MacApplication::isMimeoPhotoProject();

    // Push state file path restrictions break Mimeo Photo Plugin (rdar://112445672).
    m_quirksData.setQuirkState(SiteSpecificQuirk::ShouldDisablePushStateFilePathRestrictions, shouldDisablePushStateFilePathRestrictions);
#endif

    auto quirksURL = topDocumentURL();
    if (quirksURL.isEmpty())
        return;

    Ref document = *protect(m_document);
    auto resolved = resolveSiteSpecificQuirks(quirksURL, document->url(), document->isTopDocument() ? IsTopDocument::Yes : IsTopDocument::No);
    m_quirksData.merge(resolved.data);
    m_elementConditions = WTF::move(resolved.elementConditions);

#if ENABLE(FLIP_SCREEN_DIMENSIONS_QUIRKS)
    // rdar://133423460
    m_quirksData.setQuirkState(SiteSpecificQuirk::ShouldFlipScreenDimensionsQuirk, shouldFlipScreenDimensionsInternal(quirksURL));
#endif

    // rdar://133423460
    m_quirksData.setQuirkState(SiteSpecificQuirk::ShouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk, shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeInternal(quirksURL));
}

void Quirks::logQuirksToConsoleIfNecessary() const
{
    RefPtr document = m_document.get();
    if (!document)
        return;

    if (!needsQuirks())
        return;

    // FIXME: should we use log english sentences instead of the quirk enum names?
    const auto quirks = activeQuirks();
    if (quirks.isEmpty())
        return;

    const auto formattedQuirksList = makeStringByJoining(quirks, ", "_s);
    const auto message = makeString(topDocumentURL().string(), " has active WebKit quirks: "_s, formattedQuirksList, ". Visit https://docs.webkit.org/Infrastructure/Quirks.html for more information."_s);
    document->addConsoleMessage(MessageSource::Other, MessageLevel::Warning, message);
}

Vector<String> Quirks::activeQuirks() const
{
    Vector<String> result;
    for (auto quirk : m_quirksData.possiblyEnabledQuirks())
        result.append(String { WTF::enumName(static_cast<SiteSpecificQuirk>(quirk)) });

    std::ranges::sort(result, codePointCompareLessThan);
    return result;
}

bool Quirks::hasRelevantQuirks() const
{
    return !m_quirksData.possiblyEnabledQuirks().isEmpty();
}

}

#undef QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE
