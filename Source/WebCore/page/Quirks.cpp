/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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
#include "Attr.h"
#include "ContainerNodeInlines.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentStorageAccess.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementInlines.h"
#include "ElementTargetingTypes.h"
#include "EventNames.h"
#include "EventTargetInlines.h"
#include "FrameLoader.h"
#include "HTMLArticleElement.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLDivElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLVideoElement.h"
#include "JSEventListener.h"
#include "KeyframeEffect.h"
#include "LayoutUnit.h"
#include "LocalDOMWindow.h"
#include "LocalFrameView.h"
#include "MouseEvent.h"
#include "NetworkStorageSession.h"
#include "NodeRenderStyle.h"
#include "OrganizationStorageAccessPromptQuirk.h"
#include "PlatformMouseEvent.h"
#include "QuirksData.h"
#include "RegistrableDomain.h"
#include "ResourceLoadObserver.h"
#include "ResourceRequest.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathElement.h"
#include "SVGSVGElement.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
#include "SpaceSplitString.h"
#include "TrustedFonts.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UserAgent.h"
#include "UserContentTypes.h"
#include "UserScript.h"
#include "UserScriptTypes.h"
#include <JavaScriptCore/CodeBlock.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/ScriptExecutable.h>
#include <JavaScriptCore/SourceCode.h>
#include <JavaScriptCore/SourceProvider.h>
#include <JavaScriptCore/StackVisitor.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
#include <pal/system/ios/UserInterfaceIdiom.h>
#endif

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Quirks);

static constexpr auto chromeUserAgentScript = "(function() { let userAgent = navigator.userAgent; Object.defineProperty(navigator, 'userAgent', { get: () => { return userAgent + ' Chrome/130.0.0.0 Android/15.0'; }, configurable: true }); })();"_s;

static inline OptionSet<AutoplayQuirk> allowedAutoplayQuirks(Document& document)
{
    auto* loader = document.loader();
    if (!loader)
        return { };

    return loader->allowedAutoplayQuirks();
}

static inline OptionSet<AutoplayQuirk> allowedAutoplayQuirks(Document* document)
{
    if (!document)
        return { };
    return allowedAutoplayQuirks(*document);
}

static HashMap<RegistrableDomain, String>& updatableStorageAccessUserAgentStringQuirks()
{
    // FIXME: Make this a member of Quirks.
    static MainThreadNeverDestroyed<HashMap<RegistrableDomain, String>> map;
    return map.get();
}

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/QuirksAdditions.cpp>
#else
static inline bool needsDesktopUserAgentInternal(const URL&) { return false; }
static inline bool shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeInternal(const URL&) { return false; }
static inline String standardUserAgentWithApplicationNameIncludingCompatOverridesInternal(const String&, const String&, UserAgentType) { return { }; }
static inline bool shouldNotAutoUpgradeToHTTPSNavigationInternal(const URL&) { return false; }
#endif

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

bool Quirks::shouldIgnoreInvalidSignal() const
{
    return needsQuirks();
}

// FIXME: Add more options to the helper to cover more patterns.
// - end of domain
// - full domain
// - path?
// or make different helpers
bool Quirks::isDomain(const String& domainString) const
{
    return RegistrableDomain(topDocumentURL()).string() == domainString;
}

bool Quirks::domainStartsWith(const String& prefix) const
{
    return RegistrableDomain(topDocumentURL()).string().startsWith(prefix);
}

bool Quirks::isEmbedDomain(const String& domainString) const
{
    if (m_document->isTopDocument())
        return false;
    return RegistrableDomain(m_document->url()).string() == domainString;
}

// ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=193478
// weather.com rdar://139689157
bool Quirks::needsFormControlToBeMouseFocusable() const
{
#if PLATFORM(MAC)
    return needsQuirks() && m_quirksData.needsFormControlToBeMouseFocusableQuirk;
#else
    return false;
#endif // PLATFORM(MAC)
}

bool Quirks::needsAutoplayPlayPauseEvents() const
{
    if (!needsQuirks())
        return false;

    if (m_quirksData.shouldDispatchPlayPauseEventsOnResume)
        return true;

    Ref document = *m_document;
    if (allowedAutoplayQuirks(document).contains(AutoplayQuirk::SynthesizedPauseEvents))
        return true;

    return allowedAutoplayQuirks(document->protectedMainFrameDocument().get()).contains(AutoplayQuirk::SynthesizedPauseEvents);
}

// netflix.com https://bugs.webkit.org/show_bug.cgi?id=173030
// This quirk handles several scenarios:
// - Inserting / Removing Airpods
// - macOS w/ Touch Bar
// - iOS PiP
bool Quirks::needsSeekingSupportDisabled() const
{
    return needsQuirks() && m_quirksData.needsSeekingSupportDisabledQuirk;
}

// netflix.com https://bugs.webkit.org/show_bug.cgi?id=193301
bool Quirks::needsPerDocumentAutoplayBehavior() const
{
#if PLATFORM(MAC)
    Ref document = *m_document;
    ASSERT(document->isTopDocument());
    return needsQuirks() && allowedAutoplayQuirks(document).contains(AutoplayQuirk::PerDocumentAutoplayBehavior);
#else
    return needsQuirks() && m_quirksData.isNetflix;
#endif
}

// zoom.com https://bugs.webkit.org/show_bug.cgi?id=223180
bool Quirks::shouldAutoplayWebAudioForArbitraryUserGesture() const
{
    return needsQuirks() && m_quirksData.shouldAutoplayWebAudioForArbitraryUserGestureQuirk;
}

// youtube.com https://bugs.webkit.org/show_bug.cgi?id=195598
bool Quirks::hasBrokenEncryptedMediaAPISupportQuirk() const
{
#if ENABLE(THUNDER)
    return false;
#else
    return needsQuirks() && m_quirksData.hasBrokenEncryptedMediaAPISupportQuirk;
#endif
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=161984
bool Quirks::isTouchBarUpdateSuppressedForHiddenContentEditable() const
{
#if PLATFORM(MAC)
    return needsQuirks() && m_quirksData.isTouchBarUpdateSuppressedForHiddenContentEditableQuirk;
#else
    return false;
#endif
}

// icloud.com rdar://26013388
// trix-editor.org rdar://28242210
// onedrive.live.com rdar://26013388
// added in https://bugs.webkit.org/show_bug.cgi?id=161996
bool Quirks::isNeverRichlyEditableForTouchBar() const
{
#if PLATFORM(MAC)
    return needsQuirks() && m_quirksData.isNeverRichlyEditableForTouchBarQuirk;
#else
    return false;
#endif
}

// docs.google.com rdar://49864669
// FIXME https://bugs.webkit.org/show_bug.cgi?id=260698
bool Quirks::shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreas() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk;
#else
    return false;
#endif
}

// weebly.com rdar://48003980
// medium.com rdar://50457837
bool Quirks::shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const
{
    if (m_document->settings().shouldDispatchSyntheticMouseEventsWhenModifyingSelection())
        return true;

    return needsQuirks() && m_quirksData.shouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk;
}

// www.youtube.com rdar://52361019
bool Quirks::needsYouTubeMouseOutQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (m_document->settings().shouldDispatchSyntheticMouseOutAfterSyntheticClick())
        return true;

    return needsQuirks() && m_quirksData.needsYouTubeMouseOutQuirk;
#else
    return false;
#endif
}

// safe.menlosecurity.com rdar://135114489
// FIXME (rdar://138585709): Remove this quirk for safe.menlosecurity.com once investigation into text corruption on the site is completed and the issue is resolved.
bool Quirks::shouldDisableWritingSuggestionsByDefault() const
{
    return needsQuirks() && m_quirksData.shouldDisableWritingSuggestionsByDefaultQuirk;
}

void Quirks::updateStorageAccessUserAgentStringQuirks(HashMap<RegistrableDomain, String>&& userAgentStringQuirks)
{
    auto& quirks = updatableStorageAccessUserAgentStringQuirks();
    quirks.clear();
    for (auto&& [domain, userAgent] : userAgentStringQuirks)
        quirks.add(WTFMove(domain), WTFMove(userAgent));
}

String Quirks::storageAccessUserAgentStringQuirkForDomain(const URL& url)
{
    if (!needsQuirks())
        return { };

    const auto& quirks = updatableStorageAccessUserAgentStringQuirks();
    RegistrableDomain domain { url };
    auto iterator = quirks.find(domain);
    if (iterator == quirks.end())
        return { };
    if (domain == "live.com"_s && url.host() != "teams.live.com"_s)
        return { };
    return iterator->value;
}

bool Quirks::isYoutubeEmbedDomain() const
{
    return isEmbedDomain("youtube.com"_s) || isEmbedDomain("youtube-nocookie.com"_s);
}

bool Quirks::shouldDisableElementFullscreenQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

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
    if (!m_quirksData.shouldDisableElementFullscreen && !m_document->isTopDocument()) {
        m_quirksData.shouldDisableElementFullscreen = isEmbedDomain("x.com"_s)
            || (PAL::currentUserInterfaceIdiomIsSmallScreen() && isYoutubeEmbedDomain());
    }

    return m_quirksData.shouldDisableElementFullscreen;
#else
    return false;
#endif
}

#if ENABLE(TOUCH_EVENTS)
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

    if (!needsQuirks())
        return false;

    auto doShouldDispatchChecks = [this] () -> QuirksData::ShouldDispatchSimulatedMouseEvents {
        auto* loader = m_document->loader();
        if (!loader || loader->simulatedMouseEventsDispatchPolicy() != SimulatedMouseEventsDispatchPolicy::Allow)
            return QuirksData::ShouldDispatchSimulatedMouseEvents::No;

        if (m_quirksData.isAmazon)
            return QuirksData::ShouldDispatchSimulatedMouseEvents::Yes;
        if (m_quirksData.isGoogleMaps)
            return QuirksData::ShouldDispatchSimulatedMouseEvents::Yes;
        if (m_quirksData.isSoundCloud)
            return QuirksData::ShouldDispatchSimulatedMouseEvents::Yes;

        const URL& topDocumentURL = this->topDocumentURL();
        const auto registrableDomainString = RegistrableDomain(topDocumentURL).string();

        if (registrableDomainString == "wix.com"_s) {
            // Disable simulated mouse dispatching for template selection.
            return startsWithLettersIgnoringASCIICase(topDocumentURL.path(), "/website/templates/"_s) ? QuirksData::ShouldDispatchSimulatedMouseEvents::No : QuirksData::ShouldDispatchSimulatedMouseEvents::Yes;
        }

        if (registrableDomainString == "airtable.com"_s)
            return QuirksData::ShouldDispatchSimulatedMouseEvents::Yes;
        if (registrableDomainString == "flipkart.com"_s)
            return QuirksData::ShouldDispatchSimulatedMouseEvents::Yes;
        if (registrableDomainString == "mybinder.org"_s)
            return QuirksData::ShouldDispatchSimulatedMouseEvents::DependingOnTargetFor_mybinder_org;

        auto host = topDocumentURL.host();
        if (host == "naver.com"_s)
            return QuirksData::ShouldDispatchSimulatedMouseEvents::Yes;
        if (host.endsWith(".naver.com"_s)) {
            // Disable the quirk for tv.naver.com subdomain to be able to simulate hover on videos.
            if (host == "tv.naver.com"_s)
                return QuirksData::ShouldDispatchSimulatedMouseEvents::No;
            // Disable the quirk for mail.naver.com subdomain to be able to tap on mail subjects.
            if (host == "mail.naver.com"_s)
                return QuirksData::ShouldDispatchSimulatedMouseEvents::No;
            // Disable the quirk on the mobile site.
            // FIXME: Maybe this quirk should be disabled for "m." subdomains on all sites? These are generally mobile sites that don't need mouse events.
            if (host == "m.naver.com"_s)
                return QuirksData::ShouldDispatchSimulatedMouseEvents::No;
            return QuirksData::ShouldDispatchSimulatedMouseEvents::Yes;
        }

        return QuirksData::ShouldDispatchSimulatedMouseEvents::No;
    };

    if (m_quirksData.shouldDispatchSimulatedMouseEventsQuirk == QuirksData::ShouldDispatchSimulatedMouseEvents::Unknown)
        m_quirksData.shouldDispatchSimulatedMouseEventsQuirk = doShouldDispatchChecks();

    switch (m_quirksData.shouldDispatchSimulatedMouseEventsQuirk) {
    case QuirksData::ShouldDispatchSimulatedMouseEvents::Unknown:
        ASSERT_NOT_REACHED();
        return false;

    case QuirksData::ShouldDispatchSimulatedMouseEvents::No:
        return false;

    case QuirksData::ShouldDispatchSimulatedMouseEvents::DependingOnTargetFor_mybinder_org:
        for (RefPtr node = dynamicDowncast<Node>(target); node; node = node->parentNode()) {
            // This uses auto* instead of RefPtr as otherwise GCC does not compile.
            if (auto* element = dynamicDowncast<Element>(*node); element && element->hasClassName("lm-DockPanel-tabBar"_s))
                return true;
        }
        return false;

    case QuirksData::ShouldDispatchSimulatedMouseEvents::Yes:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

// amazon.com rdar://49124529
// soundcloud.com rdar://52915981
bool Quirks::shouldDispatchedSimulatedMouseEventsAssumeDefaultPrevented(EventTarget* target) const
{
    if (!needsQuirks() || !shouldDispatchSimulatedMouseEvents(target))
        return false;

    if (!m_quirksData.shouldDispatchedSimulatedMouseEventsAssumeDefaultPreventedQuirk)
        return false;

    RefPtr element = dynamicDowncast<Element>(target);
    if (!element)
        return false;

    if (m_quirksData.isAmazon) {
        // When panning on an Amazon product image, we're either touching on the #magnifierLens element
        // or its previous sibling.
        if (element->getIdAttribute() == "magnifierLens"_s)
            return true;
        if (auto* sibling = element->nextElementSibling())
            return sibling->getIdAttribute() == "magnifierLens"_s;
    }

    if (m_quirksData.isSoundCloud)
        return element->hasClassName("sceneLayer"_s);

    return false;
}

// sites.google.com rdar://58653069
bool Quirks::shouldPreventDispatchOfTouchEvent(const AtomString& touchEventType, EventTarget* target) const
{
    if (!needsQuirks())
        return false;

    if (!m_quirksData.shouldPreventDispatchOfTouchEventQuirk)
        return false;

    if (RefPtr element = dynamicDowncast<Element>(target); element && touchEventType == eventNames().touchendEvent)
        return element->hasClassName("DPvwYc"_s) && element->hasClassName("sm8sCf"_s);

    return false;
}

#endif

// live.com rdar://52116170
// sharepoint.com rdar://52116170
// maps.google.com https://bugs.webkit.org/show_bug.cgi?id=214945
bool Quirks::shouldAvoidResizingWhenInputViewBoundsChange() const
{
    return needsQuirks() && m_quirksData.shouldAvoidResizingWhenInputViewBoundsChangeQuirk;
}

// mailchimp.com rdar://47868965
bool Quirks::shouldDisablePointerEventsQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.shouldDisablePointerEventsQuirk;
#else
    return false;
#endif
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
bool Quirks::needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommand() const
{
#if PLATFORM(IOS_FAMILY)
    if (m_document->settings().needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk())
        return true;

    return needsQuirks() && m_quirksData.isGoogleDocs;
#else
    return false;
#endif
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
bool Quirks::inputMethodUsesCorrectKeyEventOrder() const
{
    return needsQuirks() && m_quirksData.inputMethodUsesCorrectKeyEventOrder;
}

// FIXME: Remove after the site is fixed, <rdar://problem/50374200>
// mail.google.com rdar://49403416
bool Quirks::needsGMailOverflowScrollQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.needsGMailOverflowScrollQuirk;
#else
    return false;
#endif
}

// FIXME: Remove after the site is fixed, <rdar://problem/50374311>
// youtube.com rdar://49582231
bool Quirks::needsYouTubeOverflowScrollQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.needsYouTubeOverflowScrollQuirk;
#else
    return false;
#endif
}

// amazon.com rdar://128962002
bool Quirks::needsPrimeVideoUserSelectNoneQuirk() const
{
#if PLATFORM(MAC)
    return needsQuirks() && m_quirksData.needsPrimeVideoUserSelectNoneQuirk;
#else
    return false;
#endif
}

// facebook.com https://webkit.org/b/295071
// FIXME: https://webkit.org/b/295318
bool Quirks::needsFacebookRemoveNotSupportedQuirk() const
{
    return needsQuirks() && m_quirksData.needsFacebookRemoveNotSupportedQuirk;
}

// youtube.com rdar://135886305
// NOTE: Also remove `BuilderConverter::convertScrollbarWidth` and related code when removing this quirk.
bool Quirks::needsScrollbarWidthThinDisabledQuirk() const
{
    return needsQuirks() && m_quirksData.needsScrollbarWidthThinDisabledQuirk;
}

// spotify.com rdar://138918575
bool Quirks::needsBodyScrollbarWidthNoneDisabledQuirk() const
{
    return needsQuirks() && m_quirksData.needsBodyScrollbarWidthNoneDisabledQuirk;
}

// gizmodo.com rdar://102227302
bool Quirks::needsFullscreenDisplayNoneQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.needsFullscreenDisplayNoneQuirk;
#else
    return false;
#endif
}

// cnn.com rdar://119640248
bool Quirks::needsFullscreenObjectFitQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.needsFullscreenObjectFitQuirk;
#else
    return false;
#endif
}

// zomato.com <rdar://problem/128962778>
bool Quirks::needsZomatoEmailLoginLabelQuirk() const
{
#if PLATFORM(MAC)
    return needsQuirks() && m_quirksData.needsZomatoEmailLoginLabelQuirk;
#else
    return false;
#endif
}

// maps.google.com rdar://67358928
bool Quirks::needsGoogleMapsScrollingQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.needsGoogleMapsScrollingQuirk;
#else
    return false;
#endif
}

// translate.google.com rdar://106539018
bool Quirks::needsGoogleTranslateScrollingQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.needsGoogleTranslateScrollingQuirk;
#else
    return false;
#endif
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
#if PLATFORM(IOS) || PLATFORM(VISION)
    if (!needsQuirks())
        return false;

    // ResizeObservers are silenced on YouTube during the 'homing out' snapshout sequence to
    // resolve rdar://109837319. This is due to a bug on the site that is causing unexpected
    // content layout and can be removed when it is addressed.
    auto* page = m_document->page();
    if (!page || !page->isTakingSnapshotsForApplicationSuspension())
        return false;

    return m_quirksData.shouldSilenceResizeObservers;
#else
    return false;
#endif
}

bool Quirks::shouldSilenceWindowResizeEventsDuringApplicationSnapshotting() const
{
#if PLATFORM(IOS) || PLATFORM(VISION)
    if (!needsQuirks())
        return false;

    if (!m_quirksData.shouldSilenceWindowResizeEventsDuringApplicationSnapshotting)
        return false;

    // We silence window resize events during the 'homing out' snapshot sequence when on icloud.com/mail
    // to address <rdar://131836301>, on nytimes.com to address <rdar://problem/59763843>, and on
    // x.com (twitter) to address <rdar://problem/58804852> & <rdar://problem/61731801>.
    auto* page = m_document->page();
    if (!page || !page->isTakingSnapshotsForApplicationSuspension())
        return false;

    return true;
#else
    return false;
#endif
}

bool Quirks::shouldSilenceMediaQueryListChangeEvents() const
{
#if PLATFORM(IOS) || PLATFORM(VISION)
    if (!needsQuirks())
        return false;

    if (!m_quirksData.shouldSilenceMediaQueryListChangeEvents)
        return false;

    // We silence MediaQueryList's change events during the 'homing out' snapshot sequence when on x.com (twitter)
    // to address <rdar://problem/58804852> & <rdar://problem/61731801>.
    auto* page = m_document->page();
    if (!page || !page->isTakingSnapshotsForApplicationSuspension())
        return false;

    return true;
#else
    return false;
#endif
}

// zillow.com rdar://53103732
bool Quirks::shouldAvoidScrollingWhenFocusedContentIsVisible() const
{
    return needsQuirks() && m_quirksData.shouldAvoidScrollingWhenFocusedContentIsVisibleQuirk;
}

// Some input only specify image/* as an acceptable type, which is failing sometimes for certains domain names
// which do not support HEIC.
bool Quirks::shouldTranscodeHeicImagesForURL(const URL& url)
{
    auto quirksDomain = RegistrableDomain(url);
    // zillow.com rdar://79872092
    if (quirksDomain.string() == "zillow.com"_s)
        return true;

    // canva.com https://webkit.org/b/293886
    if (quirksDomain.string() == "canva.com"_s)
        return true;

    return false;
}

// att.com rdar://55185021
bool Quirks::shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation() const
{
    return needsQuirks() && m_quirksData.shouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk;
}

// ralphlauren.com rdar://55629493
bool Quirks::shouldIgnoreAriaForFastPathContentObservationCheck() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.shouldIgnoreAriaForFastPathContentObservationCheckQuirk;
#else
    return false;
#endif
}

// wikipedia.org https://webkit.org/b/247636
bool Quirks::shouldIgnoreViewportArgumentsToAvoidExcessiveZoom() const
{
#if ENABLE(META_VIEWPORT)
    return needsQuirks() && m_quirksData.shouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk;
#endif
    return false;
}

// slack.com rdar://138614711
bool Quirks::shouldIgnoreViewportArgumentsToAvoidEnlargedView() const
{
#if ENABLE(META_VIEWPORT)
    return needsQuirks() && m_quirksData.shouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk;
#endif
    return false;
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199933
bool Quirks::shouldOpenAsAboutBlank(const String& stringToOpen) const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    auto openerURL = m_document->url();
    if (openerURL.host() != "docs.google.com"_s)
        return false;

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
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.needsPreloadAutoQuirk;
#else
    return false;
#endif
}

// vimeo.com rdar://56996057
// docs.google.com rdar://59893415
// bing.com rdar://133223599
bool Quirks::shouldBypassBackForwardCache() const
{
    if (!needsQuirks())
        return false;

    if (!m_quirksData.maybeBypassBackForwardCache)
        return false;

    RefPtr document = m_document.get();

    // Vimeo.com used to bypass the back/forward cache by serving "Cache-Control: no-store" over HTTPS.
    // We started caching such content in r250437 but the vimeo.com content unfortunately is not currently compatible
    // because it changes the opacity of its body to 0 when navigating away and fails to restore the original opacity
    // when coming back from the back/forward cache (e.g. in 'pageshow' event handler). See <rdar://problem/56996057>.
    if (m_quirksData.isVimeo && topDocumentURL().protocolIs("https"_s)) {
        if (auto* documentLoader = document->frame() ? document->frame()->loader().documentLoader() : nullptr)
            return documentLoader->response().cacheControlContainsNoStore();
    }

    // Spinner issue from image search for bing.com.
    if (m_quirksData.isBing) {
        static MainThreadNeverDestroyed<const AtomString> imageSearchDialogID("sb_sbidialog"_s);
        if (RefPtr element = document->getElementById(imageSearchDialogID.get()))
            return element->renderer();
    }

    // Login issue on bankofamerica.com (rdar://104938789).
    if (m_quirksData.isBankOfAmerica) {
        if (RefPtr window = document->window()) {
            if (window->hasEventListeners(eventNames().unloadEvent)) {
                static MainThreadNeverDestroyed<const AtomString> signInId("signIn"_s);
                static MainThreadNeverDestroyed<const AtomString> loadingClass("loading"_s);
                RefPtr signinButton = document->getElementById(signInId.get());
                return signinButton && signinButton->hasClassName(loadingClass.get());
            }
        }
    }

    if (m_quirksData.isGoogleProperty) {
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
    // Deferring 'mapbox-gl.js' script on bungalow.com causes the script to get in a bad state (rdar://problem/61658940).
    // Deferring the google maps script on sfusd.edu may get the page in a bad state (rdar://116292738).
    return needsQuirks() && m_quirksData.shouldBypassAsyncScriptDeferring;
}

// smoothscroll JS library rdar://52712513
bool Quirks::shouldMakeEventListenerPassive(const EventTarget& eventTarget, const EventTypeInfo& eventType)
{
    auto eventTargetIsRoot = [](const EventTarget& eventTarget) {
        if (is<LocalDOMWindow>(eventTarget))
            return true;

        if (RefPtr node = dynamicDowncast<Node>(eventTarget)) {
            if (is<Document>(*node))
                return true;
            Ref document = node->document();
            return document->documentElement() == node || document->body() == node;
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

#if ENABLE(MEDIA_STREAM)
// warbyparker.com rdar://72839707
// baidu.com rdar://56421276
bool Quirks::shouldEnableLegacyGetUserMediaQuirk() const
{
    return needsQuirks() && m_quirksData.shouldEnableLegacyGetUserMediaQuirk;
}

// zoom.us rdar://118185086
bool Quirks::shouldDisableImageCaptureQuirk() const
{
    return needsQuirks() && m_quirksData.shouldDisableImageCaptureQuirk;
}

bool Quirks::shouldEnableSpeakerSelectionPermissionsPolicyQuirk() const
{
    return needsQuirks() && m_quirksData.shouldEnableSpeakerSelectionPermissionsPolicyQuirk;
}

bool Quirks::shouldEnableEnumerateDeviceQuirk() const
{
    return needsQuirks() && m_quirksData.shouldEnableEnumerateDeviceQuirk;
}
#endif

bool Quirks::shouldUnloadHeavyFrame() const
{
    return needsQuirks() && m_quirksData.shouldUnloadHeavyFrames;
}

// hulu.com rdar://55041979
bool Quirks::needsCanPlayAfterSeekedQuirk() const
{
    return needsQuirks() && m_quirksData.needsCanPlayAfterSeekedQuirk;
}

// wikipedia.org rdar://54856323
bool Quirks::shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraints() const
{
    // FIXME: We should consider replacing this with a heuristic to determine whether
    // or not the edges of the page mostly lack content after shrinking to fit.
    return needsQuirks() && m_quirksData.shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk;
}

// mail.yahoo.com rdar://63511613
bool Quirks::shouldAvoidPastingImagesAsWebContent() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.shouldAvoidPastingImagesAsWebContent;
#else
    return false;
#endif
}

bool Quirks::shouldNotAutoUpgradeToHTTPSNavigation(const URL& url)
{
    return needsQuirks() && shouldNotAutoUpgradeToHTTPSNavigationInternal(url);
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
    return url.host() == "teams.microsoft.com"_s && url.query().contains("Retried+3+times+without+success"_s);
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
        if (!ResourceLoadObserver::shared().hasCrossPageStorageAccess(loginDomain, topFrameDomain))
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
    DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(*document, WTFMove(domainInNeedOfStorageAccess), [firstPartyDomain, domainInNeedOfStorageAccess, completionHandler = WTFMove(completionHandler)](StorageAccessWasGranted storageAccessGranted) mutable {
        if (storageAccessGranted == StorageAccessWasGranted::No) {
            completionHandler(ShouldDispatchClick::Yes);
            return;
        }

        ResourceLoadObserver::shared().setDomainsWithCrossPageStorageAccess({ { firstPartyDomain, Vector<RegistrableDomain> { domainInNeedOfStorageAccess } } }, [completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(ShouldDispatchClick::Yes);
        });
    });
    return Quirks::StorageAccessResult::ShouldCancelEvent;
}

RefPtr<Document> Quirks::protectedDocument() const
{
    return m_document.get();
}

void Quirks::triggerOptionalStorageAccessIframeQuirk(const URL& frameURL, CompletionHandler<void()>&& completionHandler) const
{
    if (RefPtr document = m_document.get()) {
        if (document->frame() && !m_document->frame()->isMainFrame()) {
            auto& mainFrame = document->frame()->mainFrame();
            if (auto* localMainFrame = dynamicDowncast<LocalFrame>(mainFrame); localMainFrame && localMainFrame->document()) {
                localMainFrame->document()->quirks().triggerOptionalStorageAccessIframeQuirk(frameURL, WTFMove(completionHandler));
                return;
            }
        }
        bool isMSOLoginButNotMSTeams = document->url().hasQuery() && document->url().host() == "login.microsoftonline.com"_s && !document->url().query().contains("redirect_uri=https%3A%2F%2Fteams.microsoft.com"_s);
        if (!isMSOLoginButNotMSTeams && subFrameDomainsForStorageAccessQuirk().contains(RegistrableDomain { frameURL })) {
            return DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(*document, RegistrableDomain { frameURL }, [completionHandler = WTFMove(completionHandler)](StorageAccessWasGranted) mutable {
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

    if (!needsQuirks())
        return Quirks::StorageAccessResult::ShouldNotCancelEvent;

    RegistrableDomain domain { m_document->url() };

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
        if (element.hasClass() && domain == youTubeDomain && !document->isTopDocument() && ResourceLoadObserver::shared().hasHadUserInteraction(youTubeDomain)) {
            if (element.hasClassName("ytp-watch-later-icon"_s) || element.hasClassName("ytp-watch-later-icon"_s)) {
                if (ResourceLoadObserver::shared().hasHadUserInteraction(youTubeDomain)) {
                    DocumentStorageAccess::requestStorageAccessForDocumentQuirk(*document, [](StorageAccessWasGranted) { });
                    return Quirks::StorageAccessResult::ShouldNotCancelEvent;
                }
            }
            return Quirks::StorageAccessResult::ShouldNotCancelEvent;
        }

        // Kinja login case.
        if (kinjaQuirks.get().contains(domain) && isKinjaLoginAvatarElement(element)) {
            if (ResourceLoadObserver::shared().hasHadUserInteraction(kinjaDomain)) {
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

            auto* abstractFrame = proxy->frame();
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

// youtube.com rdar://66242343
bool Quirks::needsVP9FullRangeFlagQuirk() const
{
    return needsQuirks() && m_quirksData.needsVP9FullRangeFlagQuirk;
}

// facebook.com: rdar://67273166
// forbes.com:
// reddit.com: rdar://80550715
// twitter.com: rdar://73369869
bool Quirks::requiresUserGestureToPauseInPictureInPicture() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // Facebook, X (twitter), and Reddit will naively pause a <video> element that has scrolled out of the viewport,
    // regardless of whether that element is currently in PiP mode.
    // We should remove the quirk once <rdar://problem/67273166>, <rdar://problem/73369869>, and <rdar://problem/80645747> have been fixed.
    return needsQuirks() && m_quirksData.requiresUserGestureToPauseInPictureInPictureQuirk;
#else
    return false;
#endif
}

// bbc.co.uk: rdar://126494734
// bbc.com: rdar://157499149
bool Quirks::returnNullPictureInPictureElementDuringFullscreenChange() const
{
    return needsQuirks() && m_quirksData.returnNullPictureInPictureElementDuringFullscreenChangeQuirk;
}

// twitter.com: rdar://73369869
bool Quirks::requiresUserGestureToLoadInPictureInPicture() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // X (Twitter) will remove the "src" attribute of a <video> element that has scrolled out of the viewport and
    // load the <video> element with an empty "src" regardless of whether that element is currently in PiP mode.
    // We should remove the quirk once <rdar://problem/73369869> has been fixed.
    return needsQuirks() && m_quirksData.requiresUserGestureToLoadInPictureInPictureQuirk;
#else
    return false;
#endif
}

// vimeo.com: rdar://problem/70788878
bool Quirks::blocksReturnToFullscreenFromPictureInPictureQuirk() const
{
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO_PRESENTATION_MODE)
    // Some sites (e.g., vimeo.com) do not set element's styles properly when a video
    // returns to fullscreen from picture-in-picture. This quirk disables the "return to fullscreen
    // from picture-in-picture" feature for those sites. We should remove the quirk once
    // rdar://problem/73167931 has been fixed.
    return needsQuirks() && m_quirksData.blocksReturnToFullscreenFromPictureInPictureQuirk;
#else
    return false;
#endif
}

// vimeo.com: rdar://107592139
bool Quirks::blocksEnteringStandardFullscreenFromPictureInPictureQuirk() const
{
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO_PRESENTATION_MODE)
    // Vimeo enters fullscreen when starting playback from the inline play button while already in PIP.
    // This behavior is revealing a bug in the fullscreen handling. See rdar://107592139.
    return needsQuirks() && m_quirksData.blocksEnteringStandardFullscreenFromPictureInPictureQuirk;
#else
    return false;
#endif
}

// espn.com: rdar://problem/73227900
// vimeo.com: rdar://problem/73227900
bool Quirks::shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // This quirk disables the "webkitendfullscreen" event when a video enters picture-in-picture
    // from fullscreen for the sites which cannot handle the event properly in that case.
    // We should remove once the quirks have been fixed.
    // <rdar://90393832> vimeo.com
    return needsQuirks() && m_quirksData.shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk;
#else
    return false;
#endif
}

// teams.live.com rdar://88678598
// teams.microsoft.com rdar://90434296
bool Quirks::shouldAllowNavigationToCustomProtocolWithoutUserGesture(StringView protocol, const SecurityOriginData& requesterOrigin)
{
    return protocol == "msteams"_s && (requesterOrigin.host() == "teams.live.com"_s || requesterOrigin.host() == "teams.microsoft.com"_s);
}

#if PLATFORM(IOS) || PLATFORM(VISION)
// espn.com: rdar://problem/95651814
bool Quirks::allowLayeredFullscreenVideos() const
{
    return needsQuirks() && m_quirksData.allowLayeredFullscreenVideos;
}
#endif

#if PLATFORM(VISION)
// x.com: rdar://132850672
// FIXME (rdar://124579556): Remove once 'x.com' adjusts video handling for visionOS.
bool Quirks::shouldDisableFullscreenVideoAspectRatioAdaptiveSizing() const
{
    return needsQuirks() && m_quirksData.shouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk;
}
#endif

// play.hbomax.com https://bugs.webkit.org/show_bug.cgi?id=244737
bool Quirks::shouldEnableFontLoadingAPIQuirk() const
{
    if (!needsQuirks() || m_document->settings().downloadableBinaryFontTrustedTypes() == DownloadableBinaryFontTrustedTypes::Any)
        return false;

    return m_quirksData.shouldEnableFontLoadingAPIQuirk;
}

// hulu.com rdar://100199996
bool Quirks::needsVideoShouldMaintainAspectRatioQuirk() const
{
    return needsQuirks() && m_quirksData.needsVideoShouldMaintainAspectRatioQuirk;
}

// Marcus: <rdar://101086391>.
// Pandora: <rdar://100243111>.
// Soundcloud: <rdar://102913500>.
bool Quirks::shouldExposeShowModalDialog() const
{
    return needsQuirks() && m_quirksData.shouldExposeShowModalDialog;
}

// marcus.com rdar://102959860
bool Quirks::shouldNavigatorPluginsBeEmpty() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.shouldNavigatorPluginsBeEmpty;
#else
    return false;
#endif
}

// Fix for the UNIQLO app (rdar://104519846).
bool Quirks::shouldDisableLazyIframeLoadingQuirk() const
{
    return needsQuirks() && m_quirksData.shouldDisableLazyIframeLoadingQuirk;
}

// Breaks express checkout on victoriassecret.com (rdar://104818312).
bool Quirks::shouldDisableFetchMetadata() const
{
    return needsQuirks() && m_quirksData.shouldDisableFetchMetadata;
}

bool Quirks::shouldBlockFetchWithNewlineAndLessThan() const
{
    return needsQuirks() && m_quirksData.shouldBlockFetchWithNewlineAndLessThan;
}

// Push state file path restrictions break Mimeo Photo Plugin (rdar://112445672).
bool Quirks::shouldDisablePushStateFilePathRestrictions() const
{
    return needsQuirks() && m_quirksData.shouldDisablePushStateFilePathRestrictions;
}

// ungap/@custom-elements polyfill (rdar://problem/111008826).
bool Quirks::needsConfigurableIndexedPropertiesQuirk() const
{
    return needsQuirks() && m_needsConfigurableIndexedPropertiesQuirk;
}

// Canvas fingerprinting (rdar://107564162)
String Quirks::advancedPrivacyProtectionSubstituteDataURLForScriptWithFeatures(const String& lastDrawnText, int canvasWidth, int canvasHeight) const
{
    if (!needsQuirks())
        return { };

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
    return needsQuirks() && m_quirksData.needsResettingTransitionCancelsRunningTransitionQuirk;
#else
    return false;
#endif
}

// Microsoft office online generates data URLs with incorrect padding on Safari only (rdar://114573089).
bool Quirks::shouldDisableDataURLPaddingValidation() const
{
    return needsQuirks() && m_quirksData.shouldDisableDataURLPaddingValidation;
}

bool Quirks::needsDisableDOMPasteAccessQuirk() const
{
    if (!needsQuirks())
        return false;

    if (m_quirksData.needsDisableDOMPasteAccessQuirk)
        return *m_quirksData.needsDisableDOMPasteAccessQuirk;

    m_quirksData.needsDisableDOMPasteAccessQuirk = [&] {
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
    }();

    return *m_quirksData.needsDisableDOMPasteAccessQuirk;
}

// rdar://133423460
bool Quirks::shouldPreventOrientationMediaQueryFromEvaluatingToLandscape() const
{
    return needsQuirks() && m_quirksData.shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk;
}

// rdar://133423460
bool Quirks::shouldFlipScreenDimensions() const
{
#if ENABLE(FLIP_SCREEN_DIMENSIONS_QUIRKS)
    return needsQuirks() && m_quirksData.shouldFlipScreenDimensionsQuirk;
#else
    return false;
#endif
}

// This section is dedicated to UA override for iPad. iPads (but iPad Mini) are sending a desktop user agent
// to websites. In some cases, the website breaks in some ways, not expecting a touch interface for the website.
// Controls not active or too small, form factor, etc. In this case it is better to send the iPad Mini UA.
// FIXME: find the reference radars and/or bugs.webkit.org issues on why these were added in the first place.
// FIXME: There is no check currently on needsQuirks(), this needs to be fixed so it makes it easier
// to deactivate them for testing.
bool Quirks::needsIPadMiniUserAgent(const URL& url)
{
    auto host = url.host();

    // FIXME: Remove this quirk when <rdar://problem/61733101> is complete.
    if (host == "roblox.com"_s || host.endsWith(".roblox.com"_s))
        return true;

    // FIXME: Remove this quirk if seatguru decides to adjust their site. See https://webkit.org/b/276947
    if (host == "seatguru.com"_s || host.endsWith(".seatguru.com"_s))
        return true;

    // FIXME: Remove this quirk once <rdar://113978106> is no longer happening.
    if (host == "www.indiatimes.com"_s)
        return true;

    return false;
}

bool Quirks::needsIPhoneUserAgent(const URL& url)
{
#if PLATFORM(IOS_FAMILY)
    if (url.host() == "shopee.sg"_s && url.path() == "/payment/account-linking/landing"_s)
        return true;
#else
    UNUSED_PARAM(url);
#endif
    return false;
}

std::optional<String> Quirks::needsCustomUserAgentOverride(const URL& url, const String& applicationNameForUserAgent)
{
    auto hostDomain = RegistrableDomain(url);
    auto firefoxUserAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:139.0) Gecko/20100101 Firefox/139.0"_s;
    // FIXME(rdar://83078414): Remove once 101edu.co and aktiv.com removes the unsupported message.
    if (hostDomain.string() == "app.101edu.co")
        return firefoxUserAgent;
    if (hostDomain.string() == "app.aktiv.com")
        return firefoxUserAgent;

#if PLATFORM(COCOA)
    // FIXME(rdar://148759791): Remove this once TikTok removes the outdated error message.
    if (hostDomain.string() == "tiktok.com"_s)
        return makeStringByReplacingAll(standardUserAgentWithApplicationName(applicationNameForUserAgent), "like Gecko"_s, "like Gecko, like Chrome/136."_s);
#else
    UNUSED_PARAM(applicationNameForUserAgent);
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
    return request.url().protocolIsInHTTPFamily() && request.url().host().endsWith(".billpaysite.com"_s);
}

// premierleague.com: rdar://123721211
bool Quirks::shouldIgnorePlaysInlineRequirementQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.shouldIgnorePlaysInlineRequirementQuirk;
#else
    return false;
#endif
}

bool Quirks::shouldUseEphemeralPartitionedStorageForDOMCookies(const URL& url) const
{
    if (!needsQuirks())
        return false;

    auto firstPartyDomain = RegistrableDomain(protectedDocument()->firstPartyForCookies()).string();
    auto domain = RegistrableDomain(url).string();

    // rdar://113830141
    if (firstPartyDomain == "cagreatamerica.com"_s && domain == "queue-it.net"_s)
        return true;

    return false;
}

// rdar://155649992
bool Quirks::shouldAllowDownloadsInSpiteOfCSP() const
{
    if (!needsQuirks())
        return false;

    return isDomain("dropbox.com"_s);
}

// rdar://127398734
bool Quirks::needsLaxSameSiteCookieQuirk(const URL& requestURL) const
{
    if (!needsQuirks())
        return false;

    auto url = protectedDocument()->url();
    return url.protocolIs("https"_s) && url.host() == "login.microsoftonline.com"_s && requestURL.protocolIs("https"_s) && requestURL.host() == "www.bing.com"_s;
}

String Quirks::standardUserAgentWithApplicationNameIncludingCompatOverrides(const String& applicationName, const String& userAgentOSVersion, UserAgentType type)
{
    return standardUserAgentWithApplicationNameIncludingCompatOverridesInternal(applicationName, userAgentOSVersion, type);
}

#if ENABLE(TEXT_AUTOSIZING)
// news.ycombinator.com: rdar://127246368
bool Quirks::shouldIgnoreTextAutoSizing() const
{
    return needsQuirks() && m_quirksData.shouldIgnoreTextAutoSizingQuirk;
}
#endif

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
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return { };

    if (!m_quirksData.needsScriptToEvaluateBeforeRunningScriptFromURLQuirk)
        return { };

    // player.anyclip.com rdar://138789765
    if (m_quirksData.isThesaurus && scriptURL.lastPathComponent().endsWith("lre.js"_s)) [[unlikely]] {
        if (scriptURL.host() == "player.anyclip.com"_s)
            return chromeUserAgentScript;
    }

    if (m_quirksData.needsGoogleTranslateScrollingQuirk && !scriptURL.isEmpty()) [[unlikely]]
        return chromeUserAgentScript;

#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
    if (m_quirksData.isWebEx && scriptURL.lastPathComponent().startsWith("pushdownload."_s)) [[unlikely]]
        return "Object.defineProperty(window, 'Touch', { get: () => undefined });"_s;
#endif
#else
    UNUSED_PARAM(scriptURL);
#endif

    return { };
}

// disneyplus: rdar://137613110
bool Quirks::shouldHideCoarsePointerCharacteristics() const
{
#if PLATFORM(IOS_FAMILY)
    return needsQuirks() && m_quirksData.shouldHideCoarsePointerCharacteristicsQuirk;
#else
    return false;
#endif
}

// hulu.com rdar://126096361
bool Quirks::implicitMuteWhenVolumeSetToZero() const
{
    return needsQuirks() && m_quirksData.implicitMuteWhenVolumeSetToZero;
}

#if ENABLE(TOUCH_EVENTS)

bool Quirks::shouldOmitTouchEventDOMAttributesForDesktopWebsite(const URL& requestURL)
{
    return requestURL.host() == "secure.chase.com"_s;
}

// soylent.*; rdar://113314067
bool Quirks::shouldDispatchPointerOutAfterHandlingSyntheticClick() const
{
    return needsQuirks() && m_quirksData.shouldDispatchPointerOutAfterHandlingSyntheticClick;
}

#endif // ENABLE(TOUCH_EVENTS)

// max.com: rdar://138424489
bool Quirks::needsZeroMaxTouchPointsQuirk() const
{
#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
    return needsQuirks() && m_quirksData.needsZeroMaxTouchPointsQuirk;
#else
    return false;
#endif
}

// imdb.com: rdar://137991466
bool Quirks::needsChromeMediaControlsPseudoElement() const
{
    return needsQuirks() && m_quirksData.needsChromeMediaControlsPseudoElementQuirk;
}

#if PLATFORM(IOS_FAMILY)

bool Quirks::shouldHideSoftTopScrollEdgeEffectDuringFocus(const Element& focusedElement) const
{
    if (!needsQuirks() || !m_quirksData.shouldHideSoftTopScrollEdgeEffectDuringFocusQuirk)
        return false;

    return focusedElement.getIdAttribute().contains("crossword"_s);
}

// store.steampowered.com: rdar://142573562
bool Quirks::shouldTreatAddingMouseOutEventListenerAsContentChange() const
{
    return needsQuirks() && m_quirksData.shouldTreatAddingMouseOutEventListenerAsContentChange;
}

// cbssports.com <rdar://139478801>.
// docs.google.com <rdar://59402637>.
bool Quirks::shouldSynthesizeTouchEventsAfterNonSyntheticClick(const Element& target) const
{
    if (!needsQuirks())
        return false;

    if (!m_quirksData.shouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk)
        return false;

    if (m_quirksData.isCBSSports)
        return target.nodeName() == "AVIA-BUTTON"_s;

    if (m_quirksData.isGoogleDocs) {
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

static AccessibilityRole accessibilityRole(const Element& element)
{
    return AccessibilityObject::ariaRoleToWebCoreRole(element.attributeWithoutSynchronization(HTMLNames::roleAttr));
}

// walmart.com: rdar://123734840
// live.outlook.com: rdar://152277211
bool Quirks::shouldIgnoreContentObservationForClick(const Node& targetNode) const
{
    if (!needsQuirks())
        return false;

    if (!m_quirksData.mayNeedToIgnoreContentObservation)
        return false;

    if (m_quirksData.isGoogleMaps) {
        for (Ref ancestor : lineageOfType<HTMLElement>(targetNode)) {
            if (ancestor->attributeWithoutSynchronization(HTMLNames::aria_labelAttr) == "Suggestions"_s)
                return true;
        }
        return false;
    }

    RefPtr target = dynamicDowncast<Element>(targetNode);
    if (m_quirksData.isOutlook) {
        if (target && target->getIdAttribute().startsWith("swatchColorPicker"_s))
            return true;
    }

    if (m_quirksData.isWalmart) {
        if (!target || accessibilityRole(*target) != AccessibilityRole::Button)
            return false;

        RefPtr parent = target->parentElementInComposedTree();
        if (!parent || accessibilityRole(*parent) != AccessibilityRole::ListItem)
            return false;
    }

    return true;
}

#endif // PLATFORM(IOS_FAMILY)

// outlook.live.com: rdar://136624720
bool Quirks::needsMozillaFileTypeForDataTransfer() const
{
    return needsQuirks() && m_quirksData.needsMozillaFileTypeForDataTransferQuirk;
}

// bing.com rdar://126573838
bool Quirks::needsBingGestureEventQuirk(EventTarget* target) const
{
    if (!needsQuirks())
        return false;

    if (!m_quirksData.needsBingGestureEventQuirk)
        return false;

    if (RefPtr element = dynamicDowncast<Element>(target)) {
        static MainThreadNeverDestroyed<const AtomString> mapClass("atlas-map-canvas"_s);
        return element->hasClassName(mapClass.get());
    }

    return false;
}

// spotify.com rdar://140707449
bool Quirks::shouldAvoidStartingSelectionOnMouseDownOverPointerCursor(const Node& target) const
{
    if (!needsQuirks())
        return false;

    if (!m_quirksData.shouldAvoidStartingSelectionOnMouseDownOverPointerCursor)
        return false;

    if (CheckedPtr style = target.renderStyle()) {
        if (style->cursorType() == CursorType::Pointer)
            return true;
    }

    return false;
}

bool Quirks::shouldReuseLiveRangeForSelectionUpdate() const
{
    if (!needsQuirks())
        return false;
    if (!m_quirksData.needsReuseLiveRangeForSelectionUpdateQuirk)
        m_quirksData.needsReuseLiveRangeForSelectionUpdateQuirk = isDomain("scribd.com"_s);
    return *m_quirksData.needsReuseLiveRangeForSelectionUpdateQuirk;
}

#if PLATFORM(IOS_FAMILY)

bool Quirks::needsPointerTouchCompatibility(const Element& target) const
{
    if (!needsQuirks())
        return false;

    if (WTF::IOSApplication::isFeedly()) {
        RefPtr pageContainer = [&target] -> const HTMLElement* {
            for (Ref ancestor : lineageOfType<HTMLElement>(target)) {
                if (ancestor->hasClassName("PageContainer"_s))
                    return ancestor.ptr();
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
bool Quirks::needsFacebookStoriesCreationFormQuirk(const Element& element, const RenderStyle& computedStyle) const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (!m_quirksData.isFacebook)
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

    if (computedStyle.display() != DisplayType::None)
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

// hotels.com rdar://126631968
bool Quirks::needsHotelsAnimationQuirk(Element& element, const RenderStyle& style) const
{
    if (!needsQuirks() || !m_quirksData.needsHotelsAnimationQuirk)
        return false;

    if (!style.hasAnimations())
        return false;

    auto matches = Ref { element }->matches(".uitk-menu-mounted .uitk-menu-container.uitk-menu-container-autoposition.uitk-menu-container-has-intersection-root-el"_s);
    return !matches.hasException() && matches.returnValue();
}

bool Quirks::needsLimitedMatroskaSupport() const
{
#if ENABLE(MEDIA_RECORDER) && ENABLE(COCOA_WEBM_PLAYER)
    return isDomain("zencastr.com"_s);
#else
    return false;
#endif
}

bool Quirks::needsCustomUserAgentData() const
{
    return needsQuirks() && m_quirksData.needsCustomUserAgentData;
}

bool Quirks::needsNavigatorUserAgentDataQuirk() const
{
    return needsQuirks() && m_quirksData.needsNavigatorUserAgentDataQuirk;
}

bool Quirks::needsNowPlayingFullscreenSwapQuirk() const
{
    return needsQuirks() && m_quirksData.needsNowPlayingFullscreenSwapQuirk;
}

bool Quirks::needsWebKitMediaTextTrackDisplayQuirk() const
{
    return needsQuirks() && m_quirksData.needsWebKitMediaTextTrackDisplayQuirk;
}

// rdar://138806698
bool Quirks::shouldSupportHoverMediaQueries() const
{
#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
    return needsQuirks() && m_quirksData.shouldSupportHoverMediaQueriesQuirk;
#else
    return false;
#endif
}

bool Quirks::shouldRewriteMediaRangeRequestForURL(const URL& url) const
{
    return needsQuirks() && m_quirksData.needsMediaRewriteRangeRequestQuirk && RegistrableDomain(url).string() == "bing.com"_s;
}

// rdar://106770785
bool Quirks::shouldPreventKeyframeEffectAcceleration(const KeyframeEffect& effect) const
{
    if (!needsQuirks() || !m_quirksData.isEA)
        return false;

    auto target = Ref { effect }->targetStyleable();
    return target && Ref { target->element }->localName() == "ea-network-nav"_s;
}

bool Quirks::shouldEnterNativeFullscreenWhenCallingElementRequestFullscreenQuirk() const
{
    return needsQuirks() && m_quirksData.shouldEnterNativeFullscreenWhenCallingElementRequestFullscreen;
}

bool Quirks::shouldDelayReloadWhenRegisteringServiceWorker() const
{
    return needsQuirks() && m_quirksData.shouldDelayReloadWhenRegisteringServiceWorker;
}

bool Quirks::shouldDisableDOMAudioSessionQuirk() const
{
    return needsQuirks() && m_quirksData.shouldDisableDOMAudioSession;
}

URL Quirks::topDocumentURL() const
{
    if (!m_topDocumentURLForTesting.isEmpty()) [[unlikely]]
        return m_topDocumentURLForTesting;

    return protectedDocument()->topURL();
}

void Quirks::setTopDocumentURLForTesting(URL&& url)
{
    m_topDocumentURLForTesting = WTFMove(url);
    determineRelevantQuirks();
}

// FIXME(rdar://141554467): The set of static functions below will be generated from a JSON file in a future patch. For now, we just move the logic
// for deciding if a particular quirk is needed to domain-specific functions below:
#if PLATFORM(IOS) || PLATFORM(VISION)
static void handle365ScoresQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "365scores.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // 365scores.com rdar://116491386
    quirksData.shouldSilenceWindowResizeEventsDuringApplicationSnapshotting = true;
}

static void handleNYTimesQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "nytimes.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // nytimes.com: rdar://problem/5976384
    quirksData.shouldSilenceWindowResizeEventsDuringApplicationSnapshotting = true;
}
#endif

#if PLATFORM(IOS_FAMILY)
static void handleASQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "as.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // as.com: rdar://121014613
    quirksData.shouldDisableElementFullscreen = PAL::currentUserInterfaceIdiomIsSmallScreen();
}

static void handleATTQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "att.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // att.com rdar://55185021
    quirksData.shouldUseLegacySelectPopoverDismissalBehaviorInDataActivationQuirk = true;
}

static void handleCBSSportsQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "cbssports.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.isCBSSports = true;
    // Remove this once rdar://139478801 is resolved.
    quirksData.shouldSynthesizeTouchEventsAfterNonSyntheticClickQuirk = true;
}

static void handleSteamQuirks(QuirksData& quirksData, const URL& /* quirksURL */, const String& quirksDomainString, const URL& /* documentURL */)
{
    if (quirksDomainString != "steampowered.com"_s)
        return;

    // Remove this once rdar://142573562 is resolved.
    quirksData.shouldTreatAddingMouseOutEventListenerAsContentChange = true;
}

static void handleCNNQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "cnn.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // cnn.com rdar://119640248
    quirksData.needsFullscreenObjectFitQuirk = true;
}

static void handleDigitalTrendsQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "digitaltrends.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // digitaltrends.com rdar://121014613
    quirksData.shouldDisableElementFullscreen = PAL::currentUserInterfaceIdiomIsSmallScreen();
}

static void handleGizmodoQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "gizmodo.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // gizmodo.com rdar://102227302
    quirksData.needsFullscreenDisplayNoneQuirk = true;
}

static void handleInstagramQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "instagram.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // instagram.com rdar://121014613
    quirksData.shouldDisableElementFullscreen = true;
}

static void handleMailChimpQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "mailchimp.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // mailchimp.com rdar://47868965
    quirksData.shouldDisablePointerEventsQuirk = true;
}

static void handleRalphLaurenQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "ralphlauren.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // ralphlauren.com rdar://55629493
    quirksData.shouldIgnoreAriaForFastPathContentObservationCheckQuirk = true;
}

static void handleSlackQuirks(QuirksData& quirksData, const URL&, const String& quirksDomainString, const URL&)
{
    if (quirksDomainString != "slack.com"_s)
        return;

    UNUSED_PARAM(quirksData);
#if ENABLE(META_VIEWPORT)
    // slack.com: rdar://138614711
    quirksData.shouldIgnoreViewportArgumentsToAvoidEnlargedViewQuirk = true;
#endif
}

static void handleWalmartQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "walmart.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // walmart.com: rdar://123734840
    quirksData.mayNeedToIgnoreContentObservation = true;
    quirksData.isWalmart = true;
}

static void handleYahooQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(documentURL);
    UNUSED_PARAM(quirksDomainString);

    auto topDocumentHost = quirksURL.host();
    if (topDocumentHost.startsWith("mail."_s)) {
        // mail.yahoo.com rdar://63511613
        quirksData.shouldAvoidPastingImagesAsWebContent = true;
    }
}

static void handleScriptToEvaluateBeforeRunningScriptFromURLQuirk(QuirksData& quirksData, const URL& quirksURL, const String& topDomain, const URL& documentURL)
{
    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);

    if (topDomain == "thesaurus.com"_s) {
        quirksData.isThesaurus = true;
        quirksData.needsScriptToEvaluateBeforeRunningScriptFromURLQuirk = true;
    }

#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
    if (topDomain == "webex.com"_s) {
        quirksData.isWebEx = true;
        quirksData.needsScriptToEvaluateBeforeRunningScriptFromURLQuirk = true;
    }
#endif
}
#endif

#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
static void handleICloudQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "icloud.com"_s)
        return;

    UNUSED_PARAM(documentURL);
#if PLATFORM(IOS_FAMILY)
    // icloud.com rdar://131836301
    quirksData.shouldSilenceWindowResizeEventsDuringApplicationSnapshotting = quirksURL.path().contains("mail"_s) || quirksURL.fragmentIdentifier().contains("mail"_s);
#endif
#if PLATFORM(MAC)
    // icloud.com rdar://26013388
    quirksData.isNeverRichlyEditableForTouchBarQuirk = quirksURL.path().contains("notes"_s) || quirksURL.fragmentIdentifier().contains("notes"_s);
#endif
}
#endif

static void handleTMobileQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(quirksDomainString);
    UNUSED_PARAM(documentURL);
    auto topDocumentHost = quirksURL.host();
    if (topDocumentHost != "digits.t-mobile.com")
        return;

    quirksData.needsNavigatorUserAgentDataQuirk = true;
    quirksData.needsCustomUserAgentData = true;
}

#if PLATFORM(MAC)
static void handleCEACStateGovQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(documentURL);
    UNUSED_PARAM(quirksDomainString);
    auto topDocumentHost = quirksURL.host();
    if (topDocumentHost == "ceac.state.gov"_s || topDocumentHost.endsWith(".ceac.state.gov"_s)) {
        // ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=193478
        quirksData.needsFormControlToBeMouseFocusableQuirk = true;
    }
}

static void handleTrixEditorQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "trix-editor.org"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // trix-editor.org rdar://28242210
    quirksData.isNeverRichlyEditableForTouchBarQuirk = true;
}

static void handleWeatherQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "weather.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // weather.com rdar://139689157
    quirksData.needsFormControlToBeMouseFocusableQuirk = true;
}
#endif

#if PLATFORM(IOS_FAMILY)
static void handleDisneyPlusQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "disneyplus.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // disneyplus rdar://137613110
    quirksData.shouldHideCoarsePointerCharacteristicsQuirk = true;
#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
    // disneyplus rdar://151715964
    quirksData.needsZeroMaxTouchPointsQuirk = true;
#endif
}

static void handleGuardianQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(quirksDomainString);
    UNUSED_PARAM(documentURL);
    quirksData.shouldHideSoftTopScrollEdgeEffectDuringFocusQuirk = true;
}
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
static void handleMaxQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "max.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // max.com: rdar://138424489
    quirksData.needsZeroMaxTouchPointsQuirk = true;
    // max.com: rdar://138806698
    quirksData.shouldSupportHoverMediaQueriesQuirk = true;
}
#endif

#if ENABLE(MEDIA_STREAM)
static void handleBaiduQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(quirksDomainString);
    UNUSED_PARAM(documentURL);
    auto topDocumentHost = quirksURL.host();
    if (topDocumentHost != "www.baidu.com"_s)
        return;

    // baidu.com rdar://56421276
    quirksData.shouldEnableLegacyGetUserMediaQuirk = true;
}

static void handleCodepenQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(quirksDomainString);
    UNUSED_PARAM(documentURL);
    auto topDocumentHost = quirksURL.host();
    if (topDocumentHost != "codepen.io"_s)
        return;

    quirksData.shouldEnableSpeakerSelectionPermissionsPolicyQuirk = true;
}

static void handleWarbyParkerQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "warbyparker.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // warbyparker.com rdar://72839707
    quirksData.shouldEnableLegacyGetUserMediaQuirk = true;
}

static void handleACTestingQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "actesting.org"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // actesting.org rdar://124017544
    quirksData.shouldEnableLegacyGetUserMediaQuirk = true;
}
#endif

static void handleDailyMailCoUkQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "dailymail.co.uk"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);

    quirksData.shouldUnloadHeavyFrames = true;
}

#if ENABLE(TEXT_AUTOSIZING)
static void handleYCombinatorQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(quirksDomainString);
    UNUSED_PARAM(documentURL);
    auto topDocumentHost = quirksURL.host();
    if (topDocumentHost != "news.ycombinator.com"_s)
        return;

    // news.ycombinator.com: rdar://127246368
    quirksData.shouldIgnoreTextAutoSizingQuirk = true;
}
#endif

#if ENABLE(TOUCH_EVENTS)
static void handleSoylentQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(quirksDomainString);
    UNUSED_PARAM(documentURL);

    // soylent.*: rdar://113314067
    quirksData.shouldDispatchPointerOutAfterHandlingSyntheticClick = true;
}
#endif

static void handleFacebookQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "facebook.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.isFacebook = true;
    // facebook.com rdar://100871402
    quirksData.needsFacebookRemoveNotSupportedQuirk = true;
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // facebook.com rdar://67273166
    quirksData.requiresUserGestureToPauseInPictureInPictureQuirk = true;
#endif
}

#if ENABLE(VIDEO_PRESENTATION_MODE)
static void handleForbesQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "forbes.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // forbes.com rdar://67273166
    quirksData.requiresUserGestureToPauseInPictureInPictureQuirk = true;
}

static void handleRedditQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "reddit.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // reddit.com: rdar://80550715
    quirksData.requiresUserGestureToPauseInPictureInPictureQuirk = true;
}
#endif

static void handleAmazonQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(quirksDomainString);
    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.isAmazon = true;
    // amazon.com rdar://49124529
    quirksData.shouldDispatchedSimulatedMouseEventsAssumeDefaultPreventedQuirk = true;
#if PLATFORM(MAC)
    // amazon.com rdar://128962002
    quirksData.needsPrimeVideoUserSelectNoneQuirk = true;
#endif
}

static void handleBBCQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);

    if (quirksDomainString == "bbc.co.uk"_s || quirksDomainString == "bbc.com"_s) {
        // bbc.co.uk rdar://126494734
        // bbc.com rdar://157499149
        quirksData.returnNullPictureInPictureElementDuringFullscreenChangeQuirk = true;
    }
}

static void handleBankOfAmericaQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "bankofamerica.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.isBankOfAmerica = true;
    // Login issue on bankofamerica.com (rdar://104938789).
    quirksData.maybeBypassBackForwardCache = true;
}

static void handleBingQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "bing.com"_s)
        return;

    UNUSED_PARAM(documentURL);
    quirksData.isBing = true;
    // bing.com rdar://133223599
    quirksData.maybeBypassBackForwardCache = true;
    // bing.com rdar://126573838
    auto topDocumentHost = quirksURL.host();
    quirksData.needsBingGestureEventQuirk = topDocumentHost == "www.bing.com"_s && startsWithLettersIgnoringASCIICase(quirksURL.path(), "/maps"_s);
    quirksData.needsMediaRewriteRangeRequestQuirk = true;
}

static void handleBungalowQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "bungalow.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // bungalow.com rdar://61658940
    quirksData.shouldBypassAsyncScriptDeferring = true;
}

static void handleDescriptQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "descript.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // descript.com rdar://156024693
    quirksData.shouldDisableDOMAudioSession = true;
}

static void handleESPNQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "espn.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.isESPN = true;
#if PLATFORM(IOS) || PLATFORM(VISION)
    // espn.com rdar://problem/95651814
    quirksData.allowLayeredFullscreenVideos = true;
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // espn.com rdar://problem/73227900
    quirksData.shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk = true;
#endif
}

static void handleEAQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "ea.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.isEA = true;
}

static void handleGoogleQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(quirksDomainString);
    UNUSED_PARAM(documentURL);
    quirksData.isGoogleProperty = true;
    auto topDocumentHost = quirksURL.host();
    if (startsWithLettersIgnoringASCIICase(quirksURL.path(), "/maps/"_s)) {
        quirksData.isGoogleMaps = true;
#if PLATFORM(IOS_FAMILY)
        // maps.google.com rdar://152194074
        quirksData.mayNeedToIgnoreContentObservation = true;
        // maps.google.com rdar://67358928
        quirksData.needsGoogleMapsScrollingQuirk = true;
#endif
        // maps.google.com https://bugs.webkit.org/show_bug.cgi?id=214945
        quirksData.shouldAvoidResizingWhenInputViewBoundsChangeQuirk = true;
    }
    quirksData.isGoogleDocs = topDocumentHost == "docs.google.com"_s;
    quirksData.inputMethodUsesCorrectKeyEventOrder = quirksData.isGoogleDocs;
#if PLATFORM(IOS_FAMILY)
    if (quirksData.isGoogleDocs) {
        // docs.google.com rdar://49864669
        quirksData.shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasQuirk = true;
        // docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
        quirksData.needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk = startsWithLettersIgnoringASCIICase(quirksURL.path(), "/spreadsheets/"_s);
    } else if (topDocumentHost == "mail.google.com"_s) {
        // mail.google.com rdar://49403416
        quirksData.needsGMailOverflowScrollQuirk = true;
    } else if (topDocumentHost == "translate.google.com"_s) {
        // translate.google.com rdar://106539018
        quirksData.needsGoogleTranslateScrollingQuirk = true;
        quirksData.needsScriptToEvaluateBeforeRunningScriptFromURLQuirk = true;
    }
#endif
    // docs.google.com rdar://59893415
    quirksData.maybeBypassBackForwardCache = true;
#if ENABLE(TOUCH_EVENTS)
    // sites.google.com rdar://58653069
    quirksData.shouldPreventDispatchOfTouchEventQuirk = topDocumentHost == "sites.google.com"_s;
#endif
#if PLATFORM(MAC)
    // docs.google.com https://bugs.webkit.org/show_bug.cgi?id=161984
    quirksData.isTouchBarUpdateSuppressedForHiddenContentEditableQuirk = quirksData.isGoogleDocs;
#endif
#if ENABLE(MEDIA_STREAM)
    quirksData.shouldEnableEnumerateDeviceQuirk = topDocumentHost == "meet.google.com"_s;
#endif
}

static void handleHBOMaxQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "hbomax.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    auto topDocumentHost = quirksURL.host();
    if (topDocumentHost != "play.hbomax.com"_s)
        return;

    // play.hbomax.com https://bugs.webkit.org/show_bug.cgi?id=244737
    quirksData.shouldEnableFontLoadingAPIQuirk = true;
}

static void handleHotelsQuirks(QuirksData& quirksData, const URL&, const String& quirksDomainString, const URL&)
{
    // hotels.com rdar://126631968
    quirksData.needsHotelsAnimationQuirk = quirksDomainString == "hotels.com"_s;
}

static void handleHuluQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "hulu.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // hulu.com rdar://100199996
    quirksData.needsVideoShouldMaintainAspectRatioQuirk = true;
    // hulu.com rdar://126096361
    quirksData.implicitMuteWhenVolumeSetToZero = true;
}

static void handleIMDBQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "imdb.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // imdb.com: rdar://137991466
    quirksData.needsChromeMediaControlsPseudoElementQuirk = true;
}

static void handleLiveQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "live.com"_s)
        return;

    UNUSED_PARAM(documentURL);
    auto topDocumentHost = quirksURL.host();
    quirksData.isOutlook = topDocumentHost == "outlook.live.com"_s;
    // outlook.live.com: rdar://136624720
    quirksData.needsMozillaFileTypeForDataTransferQuirk = quirksData.isOutlook;
#if PLATFORM(IOS_FAMILY)
    // outlook.live.com: rdar://152277211
    quirksData.mayNeedToIgnoreContentObservation = quirksData.isOutlook;
#endif
    // live.com rdar://52116170
    quirksData.shouldAvoidResizingWhenInputViewBoundsChangeQuirk = true;
    // Microsoft office online generates data URLs with incorrect padding on Safari only (rdar://114573089).
    quirksData.shouldDisableDataURLPaddingValidation = topDocumentHost.endsWith("officeapps.live.com"_s) || topDocumentHost.endsWith("onedrive.live.com"_s);
#if PLATFORM(MAC)
    // onedrive.live.com rdar://26013388
    quirksData.isNeverRichlyEditableForTouchBarQuirk = topDocumentHost == "onedrive.live.com"_s;
#endif
}

static void handleMarcusQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "marcus.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // Marcus: <rdar://101086391>.
    quirksData.shouldExposeShowModalDialog = true;
#if PLATFORM(IOS_FAMILY)
    // marcus.com rdar://102959860
    quirksData.shouldNavigatorPluginsBeEmpty = true;
#endif
}

static void handleMediumQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "medium.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // medium.com rdar://50457837
    quirksData.shouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk = true;
}

static void handleMenloSecurityQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(quirksDomainString);
    UNUSED_PARAM(documentURL);
    auto topDocumentHost = quirksURL.host();
    if (topDocumentHost != "safe.menlosecurity.com"_s)
        return;

    // safe.menlosecurity.com rdar://135114489
    quirksData.shouldDisableWritingSuggestionsByDefaultQuirk = true;
}

static void handleNBAQuirks(QuirksData& quirksData, const URL&, const String& quirksDomainString, const URL&)
{
#if PLATFORM(IOS)
    if (quirksDomainString != "nba.com"_s)
        return;

    quirksData.shouldEnterNativeFullscreenWhenCallingElementRequestFullscreen = PAL::currentUserInterfaceIdiomIsSmallScreen();
#else
    UNUSED_PARAM(quirksData);
    UNUSED_PARAM(quirksDomainString);
#endif
}

static void handleNHLQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "nhl.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.needsWebKitMediaTextTrackDisplayQuirk = true;
}

static void handleNetflixQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "netflix.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.isNetflix = true;
    // netflix.com https://bugs.webkit.org/show_bug.cgi?id=173030
    quirksData.needsSeekingSupportDisabledQuirk = true;

#if PLATFORM(VISION)
    quirksData.needsNowPlayingFullscreenSwapQuirk = true;
#endif
}

static void handlePandoraQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "pandora.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // Pandora: <rdar://100243111>.
    quirksData.shouldExposeShowModalDialog = true;
}

static void handlePremierLeagueQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "premierleague.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // premierleague.com: rdar://123721211
    quirksData.shouldIgnorePlaysInlineRequirementQuirk = true;

    // premierleague.com: rdar://68938833
    quirksData.shouldDispatchPlayPauseEventsOnResume = true;

    // premierleague.com: rdar://136791737
    quirksData.shouldAvoidStartingSelectionOnMouseDownOverPointerCursor = true;
}

static void handleSFUSDQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "sfusd.edu"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // sfusd.edu: rdar://116292738
    quirksData.shouldBypassAsyncScriptDeferring = true;
}

static void handleSharePointQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "sharepoint.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // sharepoint.com rdar://52116170
    quirksData.shouldAvoidResizingWhenInputViewBoundsChangeQuirk = true;
}

static void handleSoundCloudQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "soundcloud.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.isSoundCloud = true;
    // soundcloud.com rdar://52915981
    quirksData.shouldDispatchedSimulatedMouseEventsAssumeDefaultPreventedQuirk = true;
    // Soundcloud: rdar://102913500
    quirksData.shouldExposeShowModalDialog = true;
}

static void handleSpotifyQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    UNUSED_PARAM(quirksDomainString);
    UNUSED_PARAM(documentURL);
    auto topDocumentHost = quirksURL.host();
    if (topDocumentHost != "open.spotify.com"_s)
        return;

    // spotify.com rdar://138918575
    quirksData.needsBodyScrollbarWidthNoneDisabledQuirk = true;
    quirksData.shouldAvoidStartingSelectionOnMouseDownOverPointerCursor = true;
}

static void handleVictoriasSecretQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "victoriassecret.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // Breaks express checkout on victoriassecret.com (rdar://104818312).
    quirksData.shouldDisableFetchMetadata = true;
}

static void handleTympanusQuirks(QuirksData& quirksData, const URL&, const String& quirksDomainString, const URL&)
{
    if (quirksDomainString != "tympanus.net"_s)
        return;

    // https://tympanus.net/Tutorials/WebGPUFluid/ does not load (rdar://143839620).
    quirksData.shouldBlockFetchWithNewlineAndLessThan = true;
}

static void handleVimeoQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "vimeo.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.isVimeo = true;
    // vimeo.com rdar://56996057
    quirksData.maybeBypassBackForwardCache = true;
#if PLATFORM(IOS_FAMILY)
    // vimeo.com rdar://55759025
    quirksData.needsPreloadAutoQuirk = true;
    // Vimeo.com has incorrect layout on iOS on certain videos with wider
    // aspect ratios than the device's screen in landscape mode.
    // (Ref: rdar://116531089)
    quirksData.shouldDisableElementFullscreen = true;
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // vimeo.com: rdar://problem/73227900
    quirksData.shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk = true;
#endif
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO_PRESENTATION_MODE)
    // vimeo.com: rdar://107592139
    quirksData.blocksEnteringStandardFullscreenFromPictureInPictureQuirk = true;
    // vimeo.com: rdar://problem/70788878
    quirksData.blocksReturnToFullscreenFromPictureInPictureQuirk = true;
#endif
}

static void handleWeeblyQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "weebly.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // weebly.com rdar://48003980
    quirksData.shouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk = true;
}

static void handleWikipediaQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "wikipedia.org"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    // wikipedia.org rdar://54856323
    quirksData.shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraintsQuirk = true;
#if ENABLE(META_VIEWPORT)
    // wikipedia.org https://webkit.org/b/247636
    quirksData.shouldIgnoreViewportArgumentsToAvoidExcessiveZoomQuirk = true;
#endif
}

static void handleTwitterXQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "x.com"_s)
        return;

    UNUSED_PARAM(quirksData);
    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
#if PLATFORM(VISION)
    // x.com: rdar://132850672
    quirksData.shouldDisableFullscreenVideoAspectRatioAdaptiveSizingQuirk = true;
#endif
#if PLATFORM(IOS) || PLATFORM(VISION)
    // Twitter.com video embeds have controls that are too tiny and
    // show page behind fullscreen.
    // (Ref: rdar://121473410)
    quirksData.shouldSilenceMediaQueryListChangeEvents = true;
    // twitter.com: rdar://problem/58804852 and rdar://problem/61731801
    quirksData.shouldSilenceWindowResizeEventsDuringApplicationSnapshotting = true;
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // twitter.com: rdar://73369869
    quirksData.requiresUserGestureToLoadInPictureInPictureQuirk = true;
    // twitter.com: rdar://73369869
    quirksData.requiresUserGestureToPauseInPictureInPictureQuirk = true;
#endif
}

static void handleYouTubeQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "youtube.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.isYouTube = true;
    // youtube.com https://bugs.webkit.org/show_bug.cgi?id=195598
    quirksData.hasBrokenEncryptedMediaAPISupportQuirk = true;
    // youtube.com rdar://135886305
    quirksData.needsScrollbarWidthThinDisabledQuirk = true;
    // youtube.com rdar://66242343
    quirksData.needsVP9FullRangeFlagQuirk = true;
#if PLATFORM(IOS_FAMILY)
    // YouTube.com does not provide AirPlay controls in fullscreen
    // (Ref: rdar://121471373)
    quirksData.shouldDisableElementFullscreen = PAL::currentUserInterfaceIdiomIsSmallScreen();
    auto topDocumentHost = quirksURL.host();
    if (topDocumentHost == "www.youtube.com"_s) {
        // www.youtube.com rdar://52361019
        quirksData.needsYouTubeMouseOutQuirk = true;
        // youtube.com rdar://49582231
        quirksData.needsYouTubeOverflowScrollQuirk = true;
    }
#endif
#if PLATFORM(IOS) || PLATFORM(VISION)
    // youtube.com: rdar://110097836
    quirksData.shouldSilenceResizeObservers = true;
#endif
}

static void handleZillowQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "zillow.com"_s)
        return;

    UNUSED_PARAM(documentURL);
    // zillow.com rdar://53103732
    auto topDocumentHost = quirksURL.host();
    quirksData.shouldAvoidScrollingWhenFocusedContentIsVisibleQuirk = topDocumentHost == "www.zillow.com"_s;
#if PLATFORM(IOS) || PLATFORM(VISION)
    // rdar://110097836
    quirksData.shouldSilenceResizeObservers = true;
#endif
}

#if PLATFORM(MAC)
static void handleZomatoQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "zomato.com"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.needsZomatoEmailLoginLabelQuirk = true;
}
#endif

static void handleZoomQuirks(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL)
{
    if (quirksDomainString != "zoom.us"_s)
        return;

    UNUSED_PARAM(quirksURL);
    UNUSED_PARAM(documentURL);
    quirksData.isZoom = true;
    // zoom.com https://bugs.webkit.org/show_bug.cgi?id=223180
    quirksData.shouldAutoplayWebAudioForArbitraryUserGestureQuirk = true;
#if ENABLE(MEDIA_STREAM)
    // zoom.us rdar://118185086
    quirksData.shouldDisableImageCaptureQuirk = true;
#endif
}

static void handleCapitalGroupQuirks(QuirksData& quirksData, const URL&, const String& quirksDomainString, const URL&)
{
    if (quirksDomainString != "capitalgroup.com"_s)
        return;
    quirksData.shouldDelayReloadWhenRegisteringServiceWorker = true;
}

void Quirks::determineRelevantQuirks()
{
    RELEASE_ASSERT(m_document);
    m_quirksData = { };

#if PLATFORM(IOS_FAMILY)
    static const bool shouldDisableLazyIframeLoadingQuirk = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoUNIQLOLazyIframeLoadingQuirk) && WTF::IOSApplication::isUNIQLOApp();
    static const bool needsResettingTransitionCancelsRunningTransitionQuirk = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ResettingTransitionCancelsRunningTransitionQuirk) && WTF::IOSApplication::isDOFUSTouch();

    m_quirksData.shouldDisableLazyIframeLoadingQuirk = shouldDisableLazyIframeLoadingQuirk;
    // DOFUS Touch app (rdar://112679186)
    m_quirksData.needsResettingTransitionCancelsRunningTransitionQuirk = needsResettingTransitionCancelsRunningTransitionQuirk;
#endif

#if PLATFORM(MAC)
    // Push state file path restrictions break Mimeo Photo Plugin (rdar://112445672).
    m_quirksData.shouldDisablePushStateFilePathRestrictions = WTF::MacApplication::isMimeoPhotoProject();
#endif

    auto quirksURL = topDocumentURL();
    if (quirksURL.isEmpty())
        return;

    auto quirksDomainString = RegistrableDomain(quirksURL).string();
    auto quirkDomainWithoutPSL = PublicSuffixStore::singleton().domainWithoutPublicSuffix(quirksDomainString);

    using QuirkHandler = void (*)(QuirksData& quirksData, const URL& quirksURL, const String& quirksDomainString, const URL& documentURL);
    using DispatchMap = HashMap<String, QuirkHandler>;
    static NeverDestroyed<DispatchMap> dispatchMap = DispatchMap({
#if PLATFORM(IOS) || PLATFORM(VISION)
        { "365scores"_s, &handle365ScoresQuirks },
#endif
#if ENABLE(MEDIA_STREAM)
        { "actesting"_s, &handleACTestingQuirks },
#endif
        { "amazon"_s, &handleAmazonQuirks },
#if PLATFORM(IOS_FAMILY)
        { "as"_s, &handleASQuirks },
        { "att"_s, &handleATTQuirks },
#endif
        { "bbc"_s, &handleBBCQuirks },
#if ENABLE(MEDIA_STREAM)
        { "baidu"_s, &handleBaiduQuirks },
        { "codepen"_s, &handleCodepenQuirks },
#endif
        { "bankofamerica"_s, &handleBankOfAmericaQuirks },
        { "bing"_s, &handleBingQuirks },
        { "bungalow"_s, &handleBungalowQuirks },
        { "capitalgroup"_s, &handleCapitalGroupQuirks },
#if PLATFORM(IOS_FAMILY)
        { "cbssports"_s, &handleCBSSportsQuirks },
        { "cnn"_s, &handleCNNQuirks },
        { "digitaltrends"_s, &handleDigitalTrendsQuirks },
        { "steampowered"_s, &handleSteamQuirks },
#endif
        { "t-mobile"_s, &handleTMobileQuirks },
        { "descript"_s, &handleDescriptQuirks },
#if PLATFORM(IOS_FAMILY)
        { "disneyplus"_s, &handleDisneyPlusQuirks },
#endif
        { "ea"_s, &handleEAQuirks },
        { "espn"_s, &handleESPNQuirks },
        { "facebook"_s, &handleFacebookQuirks },
#if ENABLE(VIDEO_PRESENTATION_MODE)
        { "forbes"_s, &handleForbesQuirks },
#endif
#if PLATFORM(IOS_FAMILY)
        { "gizmodo"_s, &handleGizmodoQuirks },
#endif
        { "google"_s, &handleGoogleQuirks },
        { "hbomax"_s, &handleHBOMaxQuirks },
        { "hotels"_s, &handleHotelsQuirks },
        { "hulu"_s, &handleHuluQuirks },
#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
        { "icloud"_s, &handleICloudQuirks },
#endif
        { "imdb"_s, &handleIMDBQuirks },
#if PLATFORM(IOS_FAMILY)
        { "instagram"_s, &handleInstagramQuirks },
#endif
        { "live"_s, &handleLiveQuirks },
#if PLATFORM(IOS_FAMILY)
        { "mailchimp"_s, &handleMailChimpQuirks },
#endif
        { "marcus"_s, &handleMarcusQuirks },
#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
        { "max"_s, &handleMaxQuirks },
#endif
        { "medium"_s, &handleMediumQuirks },
        { "menlosecurity"_s, &handleMenloSecurityQuirks },
        { "netflix"_s, &handleNetflixQuirks },
        { "nba"_s, &handleNBAQuirks },
        { "nhl"_s, &handleNHLQuirks },
#if PLATFORM(IOS) || PLATFORM(VISION)
        { "nytimes"_s, &handleNYTimesQuirks },
#endif
        { "pandora"_s, &handlePandoraQuirks },
        { "premierleague"_s, &handlePremierLeagueQuirks },
#if PLATFORM(IOS_FAMILY)
        { "ralphlauren"_s, &handleRalphLaurenQuirks },
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
        { "reddit"_s, &handleRedditQuirks },
#endif
        { "sfusd"_s, &handleSFUSDQuirks },
#if PLATFORM(IOS_FAMILY)
        { "slack"_s, &handleSlackQuirks },
#endif
        { "sharepoint"_s, &handleSharePointQuirks },
        { "soundcloud"_s, &handleSoundCloudQuirks },
#if ENABLE(TOUCH_EVENTS)
        { "soylent"_s, &handleSoylentQuirks },
#endif
        { "spotify"_s, &handleSpotifyQuirks },
#if PLATFORM(MAC)
        { "state"_s, &handleCEACStateGovQuirks },
#endif
#if PLATFORM(IOS_FAMILY)
        { "theguardian"_s, &handleGuardianQuirks },
        { "thesaurus"_s, &handleScriptToEvaluateBeforeRunningScriptFromURLQuirk },
#endif
#if PLATFORM(MAC)
        { "trix-editor"_s, &handleTrixEditorQuirks },
#endif
        { "tympanus"_s, &handleTympanusQuirks },
        { "victoriassecret"_s, &handleVictoriasSecretQuirks },
        { "vimeo"_s, &handleVimeoQuirks },
#if PLATFORM(IOS_FAMILY)
        { "walmart"_s, &handleWalmartQuirks },
#endif
        { "wikipedia"_s, &handleWikipediaQuirks },
#if ENABLE(MEDIA_STREAM)
        { "warbyparker"_s, &handleWarbyParkerQuirks },
#endif
#if PLATFORM(MAC)
        { "weather"_s, &handleWeatherQuirks },
#endif
#if PLATFORM(IOS_FAMILY) && ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
        { "webex"_s, &handleScriptToEvaluateBeforeRunningScriptFromURLQuirk },
#endif
        { "weebly"_s, &handleWeeblyQuirks },
        { "x"_s, &handleTwitterXQuirks },
#if PLATFORM(IOS_FAMILY)
        { "yahoo"_s, &handleYahooQuirks },
#endif
#if ENABLE(TEXT_AUTOSIZING)
        { "ycombinator"_s, &handleYCombinatorQuirks },
#endif
        { "youtube"_s, &handleYouTubeQuirks },
        { "zillow"_s, &handleZillowQuirks },
#if PLATFORM(MAC)
        { "zomato"_s, &handleZomatoQuirks },
#endif
        { "zoom"_s, &handleZoomQuirks },
        { "dailymail"_s, &handleDailyMailCoUkQuirks }
    });

    auto findResult = dispatchMap->find(quirkDomainWithoutPSL);
    if (findResult != dispatchMap->end())
        (findResult->value)(m_quirksData, quirksURL, quirksDomainString, m_document->url());

    // Note: `needsDisableDOMPasteAccessQuirk` needs a live document to assess
    // Note: `shouldDisableElementFullscreen` needs a live document for embedded sites

    // FIXME: The below quirks should be handled more efficiently in a
#if ENABLE(FLIP_SCREEN_DIMENSIONS_QUIRKS)
    // rdar://133423460
    m_quirksData.shouldFlipScreenDimensionsQuirk = shouldFlipScreenDimensionsInternal(quirksURL);
#endif

    // rdar://133423460
    m_quirksData.shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk = shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeInternal(quirksURL);
}

}
