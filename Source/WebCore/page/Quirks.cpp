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

#include "config.h"
#include "Quirks.h"

#include "Attr.h"
#include "DOMTokenList.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentStorageAccess.h"
#include "ElementInlines.h"
#include "ElementTargetingTypes.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLDivElement.h"
#include "HTMLMetaElement.h"
#include "HTMLObjectElement.h"
#include "HTMLVideoElement.h"
#include "JSEventListener.h"
#include "LayoutUnit.h"
#include "LocalDOMWindow.h"
#include "MouseEvent.h"
#include "NamedNodeMap.h"
#include "NetworkStorageSession.h"
#include "OrganizationStorageAccessPromptQuirk.h"
#include "PlatformMouseEvent.h"
#include "RegistrableDomain.h"
#include "ResourceLoadObserver.h"
#include "RuntimeApplicationChecks.h"
#include "SVGElementTypeHelpers.h"
#include "SVGPathElement.h"
#include "SVGSVGElement.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
#include "SpaceSplitString.h"
#include "TrustedFonts.h"
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
#include <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
#include <pal/system/ios/UserInterfaceIdiom.h>
#endif

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

static inline OptionSet<AutoplayQuirk> allowedAutoplayQuirks(Document& document)
{
    auto* loader = document.loader();
    if (!loader)
        return { };

    return loader->allowedAutoplayQuirks();
}

static HashMap<RegistrableDomain, String>& updatableStorageAccessUserAgentStringQuirks()
{
    // FIXME: Make this a member of Quirks.
    static MainThreadNeverDestroyed<HashMap<RegistrableDomain, String>> map;
    return map.get();
}

#if PLATFORM(IOS_FAMILY)
static inline bool isYahooMail(Document& document)
{
    auto host = document.topDocument().url().host();
    return host.startsWith("mail."_s) && PublicSuffixStore::singleton().topPrivatelyControlledDomain(host).startsWith("yahoo."_s);
}
#endif

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/QuirksAdditions.cpp>
#else
static inline bool needsDesktopUserAgentInternal(const URL&) { return false; }
static inline bool shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeInternal(const URL&) { return false; }
#endif

Quirks::Quirks(Document& document)
    : m_document(document)
{
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
    return RegistrableDomain(m_document->topDocument().url()).string() == domainString;
}

bool Quirks::isEmbedDomain(const String& domainString) const
{
    if (m_document->isTopDocument())
        return false;
    return RegistrableDomain(m_document->url()).string() == domainString;
}

// ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=193478
bool Quirks::needsFormControlToBeMouseFocusable() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return host == "ceac.state.gov"_s || host.endsWith(".ceac.state.gov"_s);
#else
    return false;
#endif
}

bool Quirks::needsAutoplayPlayPauseEvents() const
{
    if (!needsQuirks())
        return false;

    Ref document = *m_document;
    if (allowedAutoplayQuirks(document).contains(AutoplayQuirk::SynthesizedPauseEvents))
        return true;

    return allowedAutoplayQuirks(document->topDocument()).contains(AutoplayQuirk::SynthesizedPauseEvents);
}

// netflix.com https://bugs.webkit.org/show_bug.cgi?id=173030
// This quirk handles several scenarios:
// - Inserting / Removing Airpods
// - macOS w/ Touch Bar
// - iOS PiP
bool Quirks::needsSeekingSupportDisabled() const
{
    if (!needsQuirks())
        return false;

    return isDomain("netflix.com"_s);
}

// netflix.com https://bugs.webkit.org/show_bug.cgi?id=193301
bool Quirks::needsPerDocumentAutoplayBehavior() const
{
#if PLATFORM(MAC)
    Ref document = *m_document;
    ASSERT(document.ptr() == &document->topDocument());
    return needsQuirks() && allowedAutoplayQuirks(document).contains(AutoplayQuirk::PerDocumentAutoplayBehavior);
#else
    if (!needsQuirks())
        return false;

    return isDomain("netflix.com"_s);
#endif
}

// zoom.com https://bugs.webkit.org/show_bug.cgi?id=223180
bool Quirks::shouldAutoplayWebAudioForArbitraryUserGesture() const
{
    if (!needsQuirks())
        return false;

    return isDomain("zoom.us"_s);
}

// hulu.com starz.com https://bugs.webkit.org/show_bug.cgi?id=190051
// youtube.com https://bugs.webkit.org/show_bug.cgi?id=195598
bool Quirks::hasBrokenEncryptedMediaAPISupportQuirk() const
{
#if ENABLE(THUNDER)
    return false;
#else

    if (!needsQuirks())
        return false;

    if (m_hasBrokenEncryptedMediaAPISupportQuirk)
        return m_hasBrokenEncryptedMediaAPISupportQuirk.value();

    m_hasBrokenEncryptedMediaAPISupportQuirk = isDomain("starz.com"_s) || isDomain("youtube.com"_s) || isDomain("hulu.com"_s);

    return m_hasBrokenEncryptedMediaAPISupportQuirk.value();
#endif
}

// covid.cdc.gov https://bugs.webkit.org/show_bug.cgi?id=223620
bool Quirks::shouldTooltipPreventFromProceedingWithClick(const Element& element) const
{
    if (!needsQuirks())
        return false;

    if (!isDomain("covid.cdc.gov"_s))
        return false;

    static MainThreadNeverDestroyed<const AtomString> tooltipClass("tooltip"_s);
    return element.hasClassName(tooltipClass.get());
}

// google.com https://bugs.webkit.org/show_bug.cgi?id=223700
// FIXME: Remove after the site is fixed, <rdar://problem/75792913>
bool Quirks::shouldHideSearchFieldResultsButton() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (PublicSuffixStore::singleton().topPrivatelyControlledDomain(m_document->topDocument().url().host()).startsWith("google."_s))
        return true;
#endif
    return false;
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=161984
bool Quirks::isTouchBarUpdateSuppressedForHiddenContentEditable() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    return m_document->topDocument().url().host() == "docs.google.com"_s;
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
    if (!needsQuirks())
        return false;

    auto& url = m_document->topDocument().url();
    auto host = url.host();

    if (host == "onedrive.live.com"_s)
        return true;

    if (isDomain("trix-editor.org"_s))
        return true;

    if (host == "www.icloud.com"_s) {
        auto path = url.path();
        if (path.contains("notes"_s) || url.fragmentIdentifier().contains("notes"_s))
            return true;
    }
#endif

    return false;
}

// docs.google.com rdar://49864669
// FIXME https://bugs.webkit.org/show_bug.cgi?id=260698
bool Quirks::shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreas() const
{

#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    if (host == "docs.google.com"_s)
        return true;
#endif
    return false;
}

// weebly.com rdar://48003980
// medium.com rdar://50457837
bool Quirks::shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const
{
    if (m_document->settings().shouldDispatchSyntheticMouseEventsWhenModifyingSelection())
        return true;

    if (!needsQuirks())
        return false;

    if (isDomain("medium.com"_s))
        return true;

    if (isDomain("weebly.com"_s))
        return true;

    return false;
}

// www.youtube.com rdar://52361019
bool Quirks::needsYouTubeMouseOutQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (m_document->settings().shouldDispatchSyntheticMouseOutAfterSyntheticClick())
        return true;

    if (!needsQuirks())
        return false;

    return m_document->url().host() == "www.youtube.com"_s;
#else
    return false;
#endif
}

// mail.google.com https://bugs.webkit.org/show_bug.cgi?id=200605
bool Quirks::shouldAvoidUsingIOS13ForGmail() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    auto& url = m_document->topDocument().url();
    return url.host() == "mail.google.com"_s;
#else
    return false;
#endif
}

// mail.google.com rdar://128360054
// FIXME (rdar://130624461): Remove this quirk once Gmail adopts the `writingsuggestions` attribute.
bool Quirks::shouldDisableWritingSuggestionsByDefault() const
{
    if (!needsQuirks())
        return false;
    auto& url = m_document->topDocument().url();
    return url.host() == "mail.google.com"_s;
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
    // Twitter.com video embeds have controls that are too tiny and
    // show page behind fullscreen.
    // (Ref: rdar://121473410)
    // YouTube.com does not provide AirPlay controls in fullscreen
    // (Ref: rdar://121471373)
    if (!m_shouldDisableElementFullscreen) {
        m_shouldDisableElementFullscreen = isDomain("vimeo.com"_s)
            || isDomain("instagram.com"_s)
            || (PAL::currentUserInterfaceIdiomIsSmallScreen() && isDomain("digitaltrends.com"_s))
            || (PAL::currentUserInterfaceIdiomIsSmallScreen() && isDomain("as.com"_s))
            || isEmbedDomain("twitter.com"_s)
            || (PAL::currentUserInterfaceIdiomIsSmallScreen() && (isDomain("youtube.com"_s) || isYoutubeEmbedDomain()));
    }

    return m_shouldDisableElementFullscreen.value();
#else
    return false;
#endif
}

#if ENABLE(TOUCH_EVENTS)
bool Quirks::isAmazon() const
{
    return PublicSuffixStore::singleton().topPrivatelyControlledDomain(m_document->topDocument().url().host()).startsWith("amazon."_s);
}

bool Quirks::isGoogleMaps() const
{
    auto& url = m_document->topDocument().url();
    return PublicSuffixStore::singleton().topPrivatelyControlledDomain(url.host()).startsWith("google."_s) && startsWithLettersIgnoringASCIICase(url.path(), "/maps/"_s);
}

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

    auto doShouldDispatchChecks = [this] () -> ShouldDispatchSimulatedMouseEvents {
        auto* loader = m_document->loader();
        if (!loader || loader->simulatedMouseEventsDispatchPolicy() != SimulatedMouseEventsDispatchPolicy::Allow)
            return ShouldDispatchSimulatedMouseEvents::No;

        if (isAmazon())
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (isGoogleMaps())
            return ShouldDispatchSimulatedMouseEvents::Yes;

        auto& url = m_document->topDocument().url();
        auto host = url.host();

        if (isDomain("wix.com"_s)) {
            // Disable simulated mouse dispatching for template selection.
            return startsWithLettersIgnoringASCIICase(url.path(), "/website/templates/"_s) ? ShouldDispatchSimulatedMouseEvents::No : ShouldDispatchSimulatedMouseEvents::Yes;
        }

        if (isDomain("trello.com"_s))
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (isDomain("airtable.com"_s))
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (isDomain("flipkart.com"_s))
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (host == "trailers.apple.com"_s)
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (isDomain("soundcloud.com"_s))
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (host == "naver.com"_s)
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (host.endsWith(".naver.com"_s)) {
            // Disable the quirk for tv.naver.com subdomain to be able to simulate hover on videos.
            if (host == "tv.naver.com"_s)
                return ShouldDispatchSimulatedMouseEvents::No;
            // Disable the quirk for mail.naver.com subdomain to be able to tap on mail subjects.
            if (host == "mail.naver.com"_s)
                return ShouldDispatchSimulatedMouseEvents::No;
            // Disable the quirk on the mobile site.
            // FIXME: Maybe this quirk should be disabled for "m." subdomains on all sites? These are generally mobile sites that don't need mouse events.
            if (host == "m.naver.com"_s)
                return ShouldDispatchSimulatedMouseEvents::No;
            return ShouldDispatchSimulatedMouseEvents::Yes;
        }
        if (isDomain("mybinder.org"_s))
            return ShouldDispatchSimulatedMouseEvents::DependingOnTargetFor_mybinder_org;
        return ShouldDispatchSimulatedMouseEvents::No;
    };

    if (m_shouldDispatchSimulatedMouseEventsQuirk == ShouldDispatchSimulatedMouseEvents::Unknown)
        m_shouldDispatchSimulatedMouseEventsQuirk = doShouldDispatchChecks();

    switch (m_shouldDispatchSimulatedMouseEventsQuirk) {
    case ShouldDispatchSimulatedMouseEvents::Unknown:
        ASSERT_NOT_REACHED();
        return false;

    case ShouldDispatchSimulatedMouseEvents::No:
        return false;

    case ShouldDispatchSimulatedMouseEvents::DependingOnTargetFor_mybinder_org:
        for (RefPtr node = dynamicDowncast<Node>(target); node; node = node->parentNode()) {
            // This uses auto* instead of RefPtr as otherwise GCC does not compile.
            if (auto* element = dynamicDowncast<Element>(*node); element && const_cast<Element&>(*element).classList().contains("lm-DockPanel-tabBar"_s))
                return true;
        }
        return false;

    case ShouldDispatchSimulatedMouseEvents::Yes:
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

    RefPtr element = dynamicDowncast<Element>(target);
    if (!element)
        return false;

    if (isAmazon()) {
        // When panning on an Amazon product image, we're either touching on the #magnifierLens element
        // or its previous sibling.
        if (element->getIdAttribute() == "magnifierLens"_s)
            return true;
        if (auto* sibling = element->nextElementSibling())
            return sibling->getIdAttribute() == "magnifierLens"_s;
    }

    if (isDomain("soundcloud.com"_s))
        return element->classList().contains("sceneLayer"_s);

    return false;
}

// maps.google.com https://bugs.webkit.org/show_bug.cgi?id=199904
// desmos.com rdar://50925173
std::optional<Event::IsCancelable> Quirks::simulatedMouseEventTypeForTarget(EventTarget* target) const
{
    if (!shouldDispatchSimulatedMouseEvents(target))
        return { };

    // On Google Maps, we want to limit simulated mouse events to dragging the little man that allows entering into Street View.
    if (isGoogleMaps()) {
        if (RefPtr element = dynamicDowncast<Element>(target); element && element->getAttribute(HTMLNames::classAttr) == "widget-expand-button-pegman-icon"_s)
            return Event::IsCancelable::Yes;
        return { };
    }

    if (isDomain("desmos.com"_s))
        return Event::IsCancelable::No;

    return Event::IsCancelable::Yes;
}

// youtube.com rdar://53415195
bool Quirks::shouldMakeTouchEventNonCancelableForTarget(EventTarget* target) const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();

    if (host == "www.youtube.com"_s) {
        if (RefPtr element = dynamicDowncast<Element>(target)) {
            unsigned depth = 3;
            for (; element && depth; element = element->parentElement(), --depth) {
                if (element->localName() == "paper-item"_s && element->classList().contains("yt-dropdown-menu"_s))
                    return true;
            }
        }
    }

    return false;
}

// shutterstock.com rdar://58844166
bool Quirks::shouldPreventPointerMediaQueryFromEvaluatingToCoarse() const
{
    if (!needsQuirks())
        return false;

    return isDomain("shutterstock.com"_s);
}

// sites.google.com rdar://58653069
bool Quirks::shouldPreventDispatchOfTouchEvent(const AtomString& touchEventType, EventTarget* target) const
{
    if (!needsQuirks())
        return false;

    if (RefPtr element = dynamicDowncast<Element>(target); element && touchEventType == eventNames().touchendEvent && m_document->topDocument().url().host() == "sites.google.com"_s) {
        auto& classList = element->classList();
        return classList.contains("DPvwYc"_s) && classList.contains("sm8sCf"_s);
    }

    return false;
}

#endif

#if ENABLE(IOS_TOUCH_EVENTS)
// mail.yahoo.com rdar://59824469
bool Quirks::shouldSynthesizeTouchEvents() const
{
    if (!needsQuirks())
        return false;

    if (!m_shouldSynthesizeTouchEventsQuirk)
        m_shouldSynthesizeTouchEventsQuirk = isYahooMail(*protectedDocument());
    return m_shouldSynthesizeTouchEventsQuirk.value();
}
#endif

// live.com rdar://52116170
// sharepoint.com rdar://52116170
// maps.google.com https://bugs.webkit.org/show_bug.cgi?id=214945
bool Quirks::shouldAvoidResizingWhenInputViewBoundsChange() const
{
    if (!needsQuirks())
        return false;

    auto& url = m_document->topDocument().url();
    auto host = url.host();

    if (isDomain("live.com"_s))
        return true;

    if (isDomain("google.com"_s) && url.path().startsWithIgnoringASCIICase("/maps/"_s))
        return true;

    if (host.endsWith(".sharepoint.com"_s))
        return true;

    return false;
}

// mailchimp.com rdar://47868965
bool Quirks::shouldDisablePointerEventsQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (isDomain("mailchimp.com"_s))
        return true;
#endif
    return false;
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
bool Quirks::needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommand() const
{
#if PLATFORM(IOS_FAMILY)
    if (m_document->settings().needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk())
        return true;

    if (!needsQuirks())
        return false;

    auto& url = m_document->topDocument().url();
    return url.host() == "docs.google.com"_s && startsWithLettersIgnoringASCIICase(url.path(), "/spreadsheets/"_s);
#else
    return false;
#endif
}

// FIXME: Remove after the site is fixed, <rdar://problem/50374200>
// mail.google.com rdar://49403416
bool Quirks::needsGMailOverflowScrollQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (!m_needsGMailOverflowScrollQuirk)
        m_needsGMailOverflowScrollQuirk = m_document->url().host() == "mail.google.com"_s;

    return *m_needsGMailOverflowScrollQuirk;
#else
    return false;
#endif
}

// web.skype.com webkit.org/b/275941
bool Quirks::needsIPadSkypeOverflowScrollQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (!m_needsIPadSkypeOverflowScrollQuirk)
        m_needsIPadSkypeOverflowScrollQuirk = m_document->url().host() == "web.skype.com"_s;

    return *m_needsIPadSkypeOverflowScrollQuirk;
#else
    return false;
#endif
}

// FIXME: Remove after the site is fixed, <rdar://problem/50374311>
// youtube.com rdar://49582231
bool Quirks::needsYouTubeOverflowScrollQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (!m_needsYouTubeOverflowScrollQuirk)
        m_needsYouTubeOverflowScrollQuirk = m_document->url().host() == "www.youtube.com"_s;

    return *m_needsYouTubeOverflowScrollQuirk;
#else
    return false;
#endif
}

// gizmodo.com rdar://102227302
bool Quirks::needsFullscreenDisplayNoneQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (!m_needsFullscreenDisplayNoneQuirk)
        m_needsFullscreenDisplayNoneQuirk = isDomain("gizmodo.com"_s);

    return *m_needsFullscreenDisplayNoneQuirk;
#else
    return false;
#endif
}

// cnn.com rdar://119640248
bool Quirks::needsFullscreenObjectFitQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (!m_needsFullscreenObjectFitQuirk)
        m_needsFullscreenObjectFitQuirk = isDomain("cnn.com"_s);

    return *m_needsFullscreenObjectFitQuirk;
#else
    return false;
#endif
}

// FIXME: weChat <rdar://problem/74377902>
bool Quirks::needsWeChatScrollingQuirk() const
{
#if PLATFORM(IOS) || PLATFORM(VISION)
    return needsQuirks() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoWeChatScrollingQuirk) && IOSApplication::isWechat();
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

    return isDomain("youtube.com"_s);
#else
    return false;
#endif
}

bool Quirks::shouldSilenceWindowResizeEvents() const
{
#if PLATFORM(IOS) || PLATFORM(VISION)
    if (!needsQuirks())
        return false;

    // We silence window resize events during the 'homing out' snapshot sequence when on nytimes.com
    // to address <rdar://problem/59763843>, and on twitter.com to address <rdar://problem/58804852> &
    // <rdar://problem/61731801>.
    auto* page = m_document->page();
    if (!page || !page->isTakingSnapshotsForApplicationSuspension())
        return false;

    return isDomain("nytimes.com"_s) || isDomain("twitter.com"_s) || isDomain("zillow.com"_s) || isDomain("365scores.com"_s);
#else
    return false;
#endif
}

bool Quirks::shouldSilenceMediaQueryListChangeEvents() const
{
#if PLATFORM(IOS) || PLATFORM(VISION)
    if (!needsQuirks())
        return false;

    // We silence MediaQueryList's change events during the 'homing out' snapshot sequence when on twitter.com
    // to address <rdar://problem/58804852> & <rdar://problem/61731801>.
    auto* page = m_document->page();
    if (!page || !page->isTakingSnapshotsForApplicationSuspension())
        return false;

    return isDomain("twitter.com"_s);
#else
    return false;
#endif
}

// zillow.com rdar://53103732
bool Quirks::shouldAvoidScrollingWhenFocusedContentIsVisible() const
{
    if (!needsQuirks())
        return false;

    return m_document->url().host() == "www.zillow.com"_s;
}

// att.com rdar://55185021
bool Quirks::shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation() const
{
    if (!needsQuirks())
        return false;

    return isDomain("att.com"_s);
}

// ralphlauren.com rdar://55629493
bool Quirks::shouldIgnoreAriaForFastPathContentObservationCheck() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    return m_document->url().host() == "www.ralphlauren.com"_s;
#endif
    return false;
}

static bool isWikipediaDomain(const URL& url)
{
    static NeverDestroyed wikipediaDomain = RegistrableDomain { URL { "https://wikipedia.org"_s } };
    return wikipediaDomain->matches(url);
}

// wikipedia.org https://webkit.org/b/247636
bool Quirks::shouldIgnoreViewportArgumentsToAvoidExcessiveZoom() const
{
    if (!needsQuirks())
        return false;

#if ENABLE(META_VIEWPORT)
    return isWikipediaDomain(m_document->url());
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
    if (!needsQuirks())
        return false;

    if (m_needsPreloadAutoQuirk)
        return m_needsPreloadAutoQuirk.value();

    auto domain = RegistrableDomain(m_document->url()).string();
    m_needsPreloadAutoQuirk = domain == "vimeo.com"_s;

    return m_needsPreloadAutoQuirk.value();
#else
    return false;
#endif
}

// vimeo.com rdar://56996057
// docs.google.com rdar://59893415
bool Quirks::shouldBypassBackForwardCache() const
{
    if (!needsQuirks())
        return false;

    RefPtr document = m_document.get();
    auto topURL = document->topDocument().url();
    auto host = topURL.host();
    RegistrableDomain registrableDomain { topURL };

    // Vimeo.com used to bypass the back/forward cache by serving "Cache-Control: no-store" over HTTPS.
    // We started caching such content in r250437 but the vimeo.com content unfortunately is not currently compatible
    // because it changes the opacity of its body to 0 when navigating away and fails to restore the original opacity
    // when coming back from the back/forward cache (e.g. in 'pageshow' event handler). See <rdar://problem/56996057>.
    if (topURL.protocolIs("https"_s) && host == "vimeo.com"_s) {
        if (auto* documentLoader = document->frame() ? document->frame()->loader().documentLoader() : nullptr)
            return documentLoader->response().cacheControlContainsNoStore();
    }

    // Login issue on bankofamerica.com (rdar://104938789).
    if (registrableDomain == "bankofamerica.com"_s) {
        if (RefPtr window = document->domWindow()) {
            if (window->hasEventListeners(eventNames().unloadEvent)) {
                static MainThreadNeverDestroyed<const AtomString> signInId("signIn"_s);
                static MainThreadNeverDestroyed<const AtomString> loadingClass("loading"_s);
                RefPtr signinButton = document->getElementById(signInId.get());
                return signinButton && signinButton->hasClassName(loadingClass.get());
            }
        }
    }

    // Google Docs used to bypass the back/forward cache by serving "Cache-Control: no-store" over HTTPS.
    // We started caching such content in r250437 but the Google Docs index page unfortunately is not currently compatible
    // because it puts an overlay (with class "docs-homescreen-freeze-el-full") over the page when navigating away and fails
    // to remove it when coming back from the back/forward cache (e.g. in 'pageshow' event handler). See <rdar://problem/57670064>.
    // Note that this does not check for docs.google.com host because of hosted G Suite apps.
    static MainThreadNeverDestroyed<const AtomString> googleDocsOverlayDivClass("docs-homescreen-freeze-el-full"_s);
    auto* firstChildInBody = document->body() ? document->body()->firstChild() : nullptr;
    if (RefPtr div = dynamicDowncast<HTMLDivElement>(firstChildInBody)) {
        if (div->hasClassName(googleDocsOverlayDivClass))
            return true;
    }

    return false;
}

// bungalow.com: rdar://61658940
// sfusd.edu: rdar://116292738
bool Quirks::shouldBypassAsyncScriptDeferring() const
{
    if (!needsQuirks())
        return false;

    if (!m_shouldBypassAsyncScriptDeferring) {
        auto domain = RegistrableDomain { m_document->topDocument().url() };
        // Deferring 'mapbox-gl.js' script on bungalow.com causes the script to get in a bad state (rdar://problem/61658940).
        // Deferring the google maps script on sfusd.edu may get the page in a bad state (rdar://116292738).
        m_shouldBypassAsyncScriptDeferring = domain == "bungalow.com"_s || domain == "sfusd.edu"_s;
    }
    return *m_shouldBypassAsyncScriptDeferring;
}

// smoothscroll JS library rdar://52712513
bool Quirks::shouldMakeEventListenerPassive(const EventTarget& eventTarget, const EventTypeInfo& eventType)
{
    auto eventTargetIsRoot = [](const EventTarget& eventTarget) {
        if (is<LocalDOMWindow>(eventTarget))
            return true;

        if (RefPtr node = dynamicDowncast<Node>(eventTarget)) {
            return is<Document>(*node) || node->document().documentElement() == node || node->document().body() == node;
        }
        return false;
    };

    auto documentFromEventTarget = [](const EventTarget& eventTarget) -> Document* {
        return downcast<Document>(eventTarget.scriptExecutionContext());
    };

    if (eventType.isInCategory(EventCategory::TouchScrollBlocking)) {
        if (eventTargetIsRoot(eventTarget)) {
            if (auto* document = documentFromEventTarget(eventTarget))
                return document->settings().passiveTouchListenersAsDefaultOnDocument();
        }
        return false;
    }

    if (eventType.isInCategory(EventCategory::Wheel)) {
        if (eventTargetIsRoot(eventTarget)) {
            if (auto* document = documentFromEventTarget(eventTarget))
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
    if (!needsQuirks())
        return false;

    if (!m_shouldEnableLegacyGetUserMediaQuirk) {
        auto host = m_document->securityOrigin().host();
        m_shouldEnableLegacyGetUserMediaQuirk = host == "www.baidu.com"_s || host == "www.warbyparker.com"_s;
    }
    return m_shouldEnableLegacyGetUserMediaQuirk.value();
}

// zoom.us rdar://118185086
bool Quirks::shouldDisableImageCaptureQuirk() const
{
    if (!needsQuirks())
        return false;

    if (!m_shouldDisableImageCaptureQuirk)
        m_shouldDisableImageCaptureQuirk = isDomain("zoom.us"_s);

    return m_shouldDisableImageCaptureQuirk.value();
}
#endif

// hulu.com rdar://55041979
bool Quirks::needsCanPlayAfterSeekedQuirk() const
{
    if (!needsQuirks())
        return false;

    if (m_needsCanPlayAfterSeekedQuirk)
        return *m_needsCanPlayAfterSeekedQuirk;

    auto domain = m_document->securityOrigin().domain();

    m_needsCanPlayAfterSeekedQuirk = domain == "hulu.com"_s || domain.endsWith(".hulu.com"_s);

    return m_needsCanPlayAfterSeekedQuirk.value();
}

// wikipedia.org rdar://54856323
bool Quirks::shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraints() const
{
    if (!needsQuirks())
        return false;

    // FIXME: We should consider replacing this with a heuristic to determine whether
    // or not the edges of the page mostly lack content after shrinking to fit.
    return isWikipediaDomain(m_document->url());
}

// mail.yahoo.com rdar://63511613
bool Quirks::shouldAvoidPastingImagesAsWebContent() const
{
    if (!needsQuirks())
        return false;

#if PLATFORM(IOS_FAMILY)
    if (!m_shouldAvoidPastingImagesAsWebContent)
        m_shouldAvoidPastingImagesAsWebContent = isYahooMail(*protectedDocument());
    return *m_shouldAvoidPastingImagesAsWebContent;
#else
    return false;
#endif
}

// kinja.com and related sites rdar://60601895
static bool isKinjaLoginAvatarElement(const Element& element)
{
    // The click event handler has been found to trigger on a div or
    // span with these class names, or the svg, or the svg's path.
    if (element.hasClass()) {
        auto& classNames = element.classNames();
        if (classNames.contains("js_switch-to-burner-login"_s)
            || classNames.contains("js_header-userbutton"_s)
            || classNames.contains("sc-1il3uru-3"_s) || classNames.contains("cIhKfd"_s)
            || classNames.contains("iyvn34-0"_s) || classNames.contains("bYIjtl"_s))
            return true;
    }

    const Element* svgElement = nullptr;
    if (is<SVGSVGElement>(element))
        svgElement = &element;
    else if (is<SVGPathElement>(element) && is<SVGSVGElement>(element.parentElement()))
        svgElement = element.parentElement();

    if (svgElement && svgElement->hasAttributes()) {
        auto ariaLabelAttr = svgElement->attributes().getNamedItem("aria-label"_s);
        if (ariaLabelAttr && ariaLabelAttr->value() == "UserFilled icon"_s)
            return true;
    }

    return false;
}

// teams.microsoft.com https://bugs.webkit.org/show_bug.cgi?id=219505
bool Quirks::isMicrosoftTeamsRedirectURL(const URL& url)
{
    return url.host() == "teams.microsoft.com"_s && url.query().contains("Retried+3+times+without+success"_s);
}

static bool elementHasClassInClosestAncestors(const Element& element, const AtomString& className, unsigned distance)
{
    if (element.hasClassName(className))
        return true;

    unsigned currentDistance = 0;
    RefPtr ancestor = dynamicDowncast<Element>(element.parentNode());
    while (ancestor && currentDistance < distance) {
        ++currentDistance;
        if (ancestor->hasClassName(className))
            return true;

        ancestor = dynamicDowncast<Element>(ancestor->parentNode());
    }
    return false;
}

static bool isStorageAccessQuirkDomainAndElement(const URL& url, const Element& element)
{
    // Microsoft Teams login case.
    // FIXME(218779): Remove this quirk once microsoft.com completes their login flow redesign.
    if (url.host() == "www.microsoft.com"_s) {
        return element.hasClass()
        && (element.classNames().contains("glyph_signIn_circle"_s)
        || element.classNames().contains("mectrl_headertext"_s)
        || element.classNames().contains("mectrl_header"_s));
    }
    // Sony Network Entertainment login case.
    // FIXME(218760): Remove this quirk once playstation.com completes their login flow redesign.
    if (url.host() == "www.playstation.com"_s || url.host() == "my.playstation.com"_s) {
        return element.hasClass()
        && (element.classNames().contains("web-toolbar__signin-button"_s)
        || element.classNames().contains("web-toolbar__signin-button-label"_s)
        || element.classNames().contains("sb-signin-button"_s));
    }

    // Gizmodo login case: rdar://106782128.
    if (url.host() == "gizmodo.com"_s)
        return elementHasClassInClosestAncestors(element, "js_user-button"_s, 6);

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
    auto firstPartyDomain = RegistrableDomain(document->topDocument().url());
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
        if (subFrameDomainsForStorageAccessQuirk().contains(RegistrableDomain { frameURL })) {
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
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("avclub.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("deadspin.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("jalopnik.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("jezebel.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("kotaku.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("lifehacker.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("theroot.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("thetakeout.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("theonion.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("theinventory.com"_s));
        return set;
    }();
    static NeverDestroyed kinjaURL = URL { "https://kinja.com"_str };
    static NeverDestroyed<RegistrableDomain> kinjaDomain { kinjaURL };

    static NeverDestroyed<RegistrableDomain> youTubeDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString("youtube.com"_s);

    static NeverDestroyed<String> loginPopupWindowFeatureString = "toolbar=no,location=yes,directories=no,status=no,menubar=no,scrollbars=yes,resizable=yes,copyhistory=no,width=599,height=600,top=420,left=980.5"_s;

    static NeverDestroyed<UserScript> kinjaLoginUserScript { "function triggerLoginForm() { let elements = document.getElementsByClassName('js_header-userbutton'); if (elements && elements[0]) { elements[0].click(); clearInterval(interval); } } let interval = setInterval(triggerLoginForm, 200);"_s, URL(aboutBlankURL()), Vector<String>(), Vector<String>(), UserScriptInjectionTime::DocumentEnd, UserContentInjectedFrames::InjectInTopFrameOnly, WaitForNotificationBeforeInjecting::Yes };

    if (isAnyClick(eventType)) {
        RefPtr document = m_document.get();
        if (!document)
            return Quirks::StorageAccessResult::ShouldNotCancelEvent;

        // Embedded YouTube case.
        if (element.hasClass() && domain == youTubeDomain && !document->isTopDocument() && ResourceLoadObserver::shared().hasHadUserInteraction(youTubeDomain)) {
            auto& classNames = element.classNames();
            if (classNames.contains("ytp-watch-later-icon"_s) || classNames.contains("ytp-watch-later-icon"_s)) {
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

            RefPtr domWindow = document->domWindow();
            if (!domWindow)
                return Quirks::StorageAccessResult::ShouldNotCancelEvent;

            ExceptionOr<RefPtr<WindowProxy>> proxyOrException =  domWindow->open(*domWindow, *domWindow, kinjaURL->string(), emptyAtom(), loginPopupWindowFeatureString);
            if (proxyOrException.hasException())
                return Quirks::StorageAccessResult::ShouldNotCancelEvent;
            auto proxy = proxyOrException.releaseReturnValue();

            auto* abstractFrame = proxy->frame();
            if (RefPtr frame = dynamicDowncast<LocalFrame>(abstractFrame)) {
                auto world = ScriptController::createWorld("kinjaComQuirkWorld"_s, ScriptController::WorldType::User);
                frame->addUserScriptAwaitingNotification(world.get(), kinjaLoginUserScript);
                return Quirks::StorageAccessResult::ShouldCancelEvent;
            }
        }

        // If the click is synthetic, the user has already gone through the storage access flow and we should not request again.
        if (isStorageAccessQuirkDomainAndElement(document->url(), element) && isSyntheticClick == IsSyntheticClick::No) {
            return requestStorageAccessAndHandleClick([element = WeakPtr { element }, platformEvent, eventType, detail, relatedTarget] (ShouldDispatchClick shouldDispatchClick) mutable {
                RefPtr protectedElement { element.get() };
                if (!protectedElement)
                    return;

                if (shouldDispatchClick == ShouldDispatchClick::Yes)
                    protectedElement->dispatchMouseEvent(platformEvent, eventType, detail, relatedTarget, IsSyntheticClick::Yes);
            });
        }
    }
    return Quirks::StorageAccessResult::ShouldNotCancelEvent;
}

// youtube.com rdar://66242343
bool Quirks::needsVP9FullRangeFlagQuirk() const
{
    if (!needsQuirks())
        return false;

    if (!m_needsVP9FullRangeFlagQuirk)
        m_needsVP9FullRangeFlagQuirk = m_document->url().host() == "www.youtube.com"_s;

    return *m_needsVP9FullRangeFlagQuirk;
}

bool Quirks::requiresUserGestureToPauseInPictureInPicture() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // Facebook, Twitter, and Reddit will naively pause a <video> element that has scrolled out of the viewport,
    // regardless of whether that element is currently in PiP mode.
    // We should remove the quirk once <rdar://problem/67273166>, <rdar://problem/73369869>, and <rdar://problem/80645747> have been fixed.
    if (!needsQuirks())
        return false;

    if (!m_requiresUserGestureToPauseInPictureInPicture) {
        auto domain = RegistrableDomain(m_document->topDocument().url()).string();
        m_requiresUserGestureToPauseInPictureInPicture = isDomain("facebook.com"_s) || isDomain("twitter.com"_s) || isDomain("reddit.com"_s);
    }

    return *m_requiresUserGestureToPauseInPictureInPicture;
#else
    return false;
#endif
}

bool Quirks::requiresUserGestureToLoadInPictureInPicture() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // Twitter will remove the "src" attribute of a <video> element that has scrolled out of the viewport and
    // load the <video> element with an empty "src" regardless of whether that element is currently in PiP mode.
    // We should remove the quirk once <rdar://problem/73369869> has been fixed.
    if (!needsQuirks())
        return false;

    if (!m_requiresUserGestureToLoadInPictureInPicture)
        m_requiresUserGestureToLoadInPictureInPicture = isDomain("twitter.com"_s);

    return *m_requiresUserGestureToLoadInPictureInPicture;
#else
    return false;
#endif
}

bool Quirks::blocksReturnToFullscreenFromPictureInPictureQuirk() const
{
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO_PRESENTATION_MODE)
    // Some sites (e.g., vimeo.com) do not set element's styles properly when a video
    // returns to fullscreen from picture-in-picture. This quirk disables the "return to fullscreen
    // from picture-in-picture" feature for those sites. We should remove the quirk once
    // rdar://problem/73167931 has been fixed.
    if (!needsQuirks())
        return false;

    if (!m_blocksReturnToFullscreenFromPictureInPictureQuirk)
        m_blocksReturnToFullscreenFromPictureInPictureQuirk = isDomain("vimeo.com"_s);

    return *m_blocksReturnToFullscreenFromPictureInPictureQuirk;
#else
    return false;
#endif
}

bool Quirks::blocksEnteringStandardFullscreenFromPictureInPictureQuirk() const
{
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO_PRESENTATION_MODE)
    // Vimeo enters fullscreen when starting playback from the inline play button while already in PIP.
    // This behavior is revealing a bug in the fullscreen handling. See rdar://107592139.
    if (!needsQuirks())
        return false;

    if (!m_blocksEnteringStandardFullscreenFromPictureInPictureQuirk) {
        m_blocksEnteringStandardFullscreenFromPictureInPictureQuirk = isDomain("vimeo.com"_s);
    }

    return *m_blocksEnteringStandardFullscreenFromPictureInPictureQuirk;
#else
    return false;
#endif
}

bool Quirks::shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // This quirk disables the "webkitendfullscreen" event when a video enters picture-in-picture
    // from fullscreen for the sites which cannot handle the event properly in that case.
    // We should remove the quirk once <rdar://problem/73261957> and <rdar://problem/90393832> have been fixed.
    if (!needsQuirks())
        return false;

    if (!m_shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk) {
        auto host = m_document->topDocument().url().host();
        auto domain = RegistrableDomain(m_document->topDocument().url());

        m_shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk = host == "trailers.apple.com"_s || domain == "espn.com"_s || domain == "vimeo.com"_s;
    }

    return *m_shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk;
#else
    return false;
#endif
}

bool Quirks::shouldDelayFullscreenEventWhenExitingPictureInPictureQuirk() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // This quirk delay the "webkitstartfullscreen" and "fullscreenchange" event when a video exits picture-in-picture
    // to fullscreen.
    if (!needsQuirks())
        return false;

    if (!m_shouldDelayFullscreenEventWhenExitingPictureInPictureQuirk)
        m_shouldDelayFullscreenEventWhenExitingPictureInPictureQuirk = isDomain("bbc.com"_s);

    return *m_shouldDelayFullscreenEventWhenExitingPictureInPictureQuirk;
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
bool Quirks::allowLayeredFullscreenVideos() const
{
    if (!needsQuirks())
        return false;

    if (!m_allowLayeredFullscreenVideos) {
        auto domain = RegistrableDomain(m_document->topDocument().url());

        m_allowLayeredFullscreenVideos = domain == "espn.com"_s;
    }

    return *m_allowLayeredFullscreenVideos;
}
#endif

#if PLATFORM(VISION)
// twitter.com: rdar://124180748
bool Quirks::shouldDisableFullscreenVideoAspectRatioAdaptiveSizing() const
{
    if (!needsQuirks())
        return false;

    return isDomain("twitter.com"_s);
}
#endif

bool Quirks::shouldEnableApplicationCacheQuirk() const
{
    // FIXME: Remove this when deleting ApplicationCache APIs.
    return false;
}

// play.hbomax.com https://bugs.webkit.org/show_bug.cgi?id=244737
bool Quirks::shouldEnableFontLoadingAPIQuirk() const
{
    if (!needsQuirks() || m_document->settings().downloadableBinaryFontTrustedTypes() == DownloadableBinaryFontTrustedTypes::Any)
        return false;

    if (!m_shouldEnableFontLoadingAPIQuirk)
        m_shouldEnableFontLoadingAPIQuirk = m_document->url().host() == "play.hbomax.com"_s;

    return m_shouldEnableFontLoadingAPIQuirk.value();
}

// hulu.com rdar://100199996
bool Quirks::needsVideoShouldMaintainAspectRatioQuirk() const
{
    if (!needsQuirks())
        return false;

    if (m_needsVideoShouldMaintainAspectRatioQuirk)
        return m_needsVideoShouldMaintainAspectRatioQuirk.value();

    auto domain = RegistrableDomain(m_document->url()).string();
    m_needsVideoShouldMaintainAspectRatioQuirk = domain == "hulu.com"_s;

    return m_needsVideoShouldMaintainAspectRatioQuirk.value();
}

bool Quirks::shouldExposeShowModalDialog() const
{
    if (!needsQuirks())
        return false;
    if (!m_shouldExposeShowModalDialog) {
        // Marcus: <rdar://101086391>.
        // Pandora: <rdar://100243111>.
        // Soundcloud: <rdar://102913500>.
        m_shouldExposeShowModalDialog = isDomain("pandora.com"_s) || isDomain("marcus.com"_s) || isDomain("soundcloud.com"_s);
    }
    return *m_shouldExposeShowModalDialog;
}

// marcus.com rdar://102959860
bool Quirks::shouldNavigatorPluginsBeEmpty() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;
    if (!m_shouldNavigatorPluginsBeEmpty) {
        // Marcus login issue: <rdar://103011164>.
        m_shouldNavigatorPluginsBeEmpty = isDomain("marcus.com"_s);
    }
    return *m_shouldNavigatorPluginsBeEmpty;
#else
    return false;
#endif
}

// Fix for the UNIQLO app (rdar://104519846).
bool Quirks::shouldDisableLazyIframeLoadingQuirk() const
{
    if (!needsQuirks())
        return false;

    if (!m_shouldDisableLazyIframeLoadingQuirk) {
#if PLATFORM(IOS_FAMILY)
        m_shouldDisableLazyIframeLoadingQuirk = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoUNIQLOLazyIframeLoadingQuirk) && IOSApplication::isUNIQLOApp();
#else
        m_shouldDisableLazyIframeLoadingQuirk = false;
#endif
    }
    return *m_shouldDisableLazyIframeLoadingQuirk;
}

// Breaks express checkout on victoriassecret.com (rdar://104818312).
bool Quirks::shouldDisableFetchMetadata() const
{
    if (!needsQuirks())
        return false;

    return isDomain("victoriassecret.com"_s);
}

// Push state file path restrictions break Mimeo Photo Plugin (rdar://112445672).
bool Quirks::shouldDisablePushStateFilePathRestrictions() const
{
    if (!needsQuirks())
        return false;

#if PLATFORM(MAC)
    return MacApplication::isMimeoPhotoProject();
#else
    return false;
#endif
}

// ungap/@custom-elements polyfill (rdar://problem/111008826).
bool Quirks::needsConfigurableIndexedPropertiesQuirk() const
{
    return needsQuirks() && m_needsConfigurableIndexedPropertiesQuirk;
}

// Canvas fingerprinting (rdar://107564162)
String Quirks::advancedPrivacyProtectionSubstituteDataURLForScriptWithFeatures(const String& lastDrawnText, int canvasWidth, int canvasHeight) const
{
    if (!needsQuirks() || !m_document->settings().canvasFingerprintingQuirkEnabled() || !m_document->noiseInjectionHashSalt())
        return { };

    if ("<@nv45. F1n63r,Pr1n71n6!"_s != lastDrawnText || canvasWidth != 280 || canvasHeight != 60)
        return { };

    if (!m_document->globalObject())
        return { };

    auto& vm = m_document->globalObject()->vm();
    auto* callFrame = vm.topCallFrame;
    if (!callFrame)
        return { };

    bool sourceMatchesExpectedLength = false;
    JSC::StackVisitor::visit(callFrame, vm, [&](auto& visitor) {
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
    return needsQuirks() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ResettingTransitionCancelsRunningTransitionQuirk) && IOSApplication::isDOFUSTouch();
#else
    return false;
#endif
}

bool Quirks::shouldStarBePermissionsPolicyDefaultValue() const
{
    if (!needsQuirks())
        return false;

    if (!m_shouldStarBePermissionsPolicyDefaultValueQuirk) {
        auto domain = m_document->securityOrigin().domain();
        m_shouldStarBePermissionsPolicyDefaultValueQuirk = domain == "jsfiddle.net"_s;
    }
    return *m_shouldStarBePermissionsPolicyDefaultValueQuirk;
}

// Microsoft office online generates data URLs with incorrect padding on Safari only (rdar://114573089).
bool Quirks::shouldDisableDataURLPaddingValidation() const
{
    if (!needsQuirks())
        return false;

    if (!m_shouldDisableDataURLPaddingValidation)
        m_shouldDisableDataURLPaddingValidation = m_document->url().host().endsWith("officeapps.live.com"_s) || m_document->url().host().endsWith("onedrive.live.com"_s);
    return *m_shouldDisableDataURLPaddingValidation;
}

bool Quirks::needsDisableDOMPasteAccessQuirk() const
{
    if (!needsQuirks())
        return false;

    if (m_needsDisableDOMPasteAccessQuirk)
        return *m_needsDisableDOMPasteAccessQuirk;

    m_needsDisableDOMPasteAccessQuirk = [&] {
        if (!m_document)
            return false;
        auto* globalObject = m_document->globalObject();
        if (!globalObject)
            return false;

        Ref vm = globalObject->vm();
        JSC::JSLockHolder lock(vm);
        auto tableauPrepProperty = JSC::Identifier::fromString(vm, "tableauPrep"_s);
        return globalObject->hasProperty(globalObject, tableauPrepProperty);
    }();
    return *m_needsDisableDOMPasteAccessQuirk;
}

// oracle.com rdar://117673533
bool Quirks::shouldDisableNavigatorStandaloneQuirk() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    if (isDomain("oracle.com"_s))
        return true;
#endif
    return false;
}

bool Quirks::shouldPreventOrientationMediaQueryFromEvaluatingToLandscape() const
{
    if (!needsQuirks())
        return false;

    return shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeInternal(m_document->topDocument().url());
}

bool Quirks::shouldFlipScreenDimensions() const
{
#if ENABLE(FLIP_SCREEN_DIMENSIONS_QUIRKS)
    if (!needsQuirks())
        return false;

    return shouldFlipScreenDimensionsInternal(m_document->topDocument().url());
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

    if (host == "tv.kakao.com"_s || host.endsWith(".tv.kakao.com"_s))
        return true;

    if (host == "tving.com"_s || host.endsWith(".tving.com"_s))
        return true;

    if (host == "live.iqiyi.com"_s || host.endsWith(".live.iqiyi.com"_s))
        return true;

    if (host == "jsfiddle.net"_s || host.endsWith(".jsfiddle.net"_s))
        return true;

    if (host == "video.sina.com.cn"_s || host.endsWith(".video.sina.com.cn"_s))
        return true;

    if (host == "huya.com"_s || host.endsWith(".huya.com"_s))
        return true;

    if (host == "video.tudou.com"_s || host.endsWith(".video.tudou.com"_s))
        return true;

    if (host == "cctv.com"_s || host.endsWith(".cctv.com"_s))
        return true;

    if (host == "v.china.com.cn"_s)
        return true;

    if (host == "trello.com"_s || host.endsWith(".trello.com"_s))
        return true;

    if (host == "ted.com"_s || host.endsWith(".ted.com"_s))
        return true;

    if (host.contains("hsbc."_s)) {
        if (host == "hsbc.com.au"_s || host.endsWith(".hsbc.com.au"_s))
            return true;
        if (host == "hsbc.com.eg"_s || host.endsWith(".hsbc.com.eg"_s))
            return true;
        if (host == "hsbc.lk"_s || host.endsWith(".hsbc.lk"_s))
            return true;
        if (host == "hsbc.co.uk"_s || host.endsWith(".hsbc.co.uk"_s))
            return true;
        if (host == "hsbc.com.hk"_s || host.endsWith(".hsbc.com.hk"_s))
            return true;
        if (host == "hsbc.com.mx"_s || host.endsWith(".hsbc.com.mx"_s))
            return true;
        if (host == "hsbc.ca"_s || host.endsWith(".hsbc.ca"_s))
            return true;
        if (host == "hsbc.com.ar"_s || host.endsWith(".hsbc.com.ar"_s))
            return true;
        if (host == "hsbc.com.ph"_s || host.endsWith(".hsbc.com.ph"_s))
            return true;
        if (host == "hsbc.com"_s || host.endsWith(".hsbc.com"_s))
            return true;
        if (host == "hsbc.com.cn"_s || host.endsWith(".hsbc.com.cn"_s))
            return true;
    }

    if (host == "nhl.com"_s || host.endsWith(".nhl.com"_s))
        return true;

    // FIXME: Remove this quirk when <rdar://problem/59480381> is complete.
    if (host == "fidelity.com"_s || host.endsWith(".fidelity.com"_s))
        return true;

    // FIXME: Remove this quirk when <rdar://problem/61733101> is complete.
    if (host == "roblox.com"_s || host.endsWith(".roblox.com"_s))
        return true;

    // FIXME: Remove this quirk when <rdar://122481999> is complete
    if (host == "spotify.com"_s || host.endsWith(".spotify.com"_s) || host.endsWith(".spotifycdn.com"_s))
        return true;

    // FIXME: Remove this quirk if seatguru decides to adjust their site. See https://webkit.org/b/276947
    if (host == "seatguru.com"_s || host.endsWith(".seatguru.com"_s))
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

bool Quirks::needsDesktopUserAgent(const URL& url)
{
    return needsDesktopUserAgentInternal(url);
}

bool Quirks::shouldIgnorePlaysInlineRequirementQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (m_shouldIgnorePlaysInlineRequirementQuirk)
        return *m_shouldIgnorePlaysInlineRequirementQuirk;

    m_shouldIgnorePlaysInlineRequirementQuirk = isDomain("premierleague.com"_s);

    return *m_shouldIgnorePlaysInlineRequirementQuirk;
#else
    return false;
#endif
}

bool Quirks::shouldUseEphemeralPartitionedStorageForDOMCookies(const URL& url) const
{
    if (!needsQuirks())
        return false;

    auto firstPartyDomain = RegistrableDomain(m_document->firstPartyForCookies()).string();
    auto domain = RegistrableDomain(url).string();

    // rdar://113830141
    if (firstPartyDomain == "cagreatamerica.com"_s && domain == "queue-it.net"_s)
        return true;

    return false;
}

// This quirk has intentionally limited exposure to increase the odds of being able to remove it
// again within a reasonable timeframe as the impacted apps are being updated. <rdar://122548304>
bool Quirks::needsGetElementsByNameQuirk() const
{
#if PLATFORM(IOS)
    return needsQuirks() && !PAL::currentUserInterfaceIdiomIsSmallScreen() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoGetElementsByNameQuirk);
#else
    return false;
#endif
}

bool Quirks::needsRelaxedCorsMixedContentCheckQuirk() const
{
    if (!needsQuirks())
        return false;

    if (m_needsRelaxedCorsMixedContentCheckQuirk)
        return *m_needsRelaxedCorsMixedContentCheckQuirk;

    m_needsRelaxedCorsMixedContentCheckQuirk = false;

    auto host = m_document->url().host();

    // FIXME: Remove this quirk when <rdar://127247321> is complete
    if (host == "tripadvisor.com"_s || host.endsWith(".tripadvisor.com"_s))
        m_needsRelaxedCorsMixedContentCheckQuirk = true;

    return *m_needsRelaxedCorsMixedContentCheckQuirk;
}

// rdar://127398734
bool Quirks::needsLaxSameSiteCookieQuirk(const URL& requestURL) const
{
    if (!needsQuirks())
        return false;

    auto url = m_document->url();
    return url.protocolIs("https"_s) && url.host() == "login.microsoftonline.com"_s && requestURL.protocolIs("https"_s) && requestURL.host() == "www.bing.com"_s;
}
#if ENABLE(TEXT_AUTOSIZING)
// rdar://127246368
bool Quirks::shouldIgnoreTextAutoSizing() const
{
    if (!needsQuirks())
        return false;
    return m_document->topDocument().url().host() == "news.ycombinator.com"_s;
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

}
