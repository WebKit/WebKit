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

#include "AllowedFonts.h"
#include "Attr.h"
#include "DOMTokenList.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentStorageAccess.h"
#include "ElementInlines.h"
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
#include "NamedNodeMap.h"
#include "NetworkStorageSession.h"
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
#include "UserAgent.h"
#include "UserContentTypes.h"
#include "UserScript.h"
#include "UserScriptTypes.h"

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

#if PLATFORM(IOS_FAMILY)
static inline bool isYahooMail(Document& document)
{
    auto host = document.topDocument().url().host();
    return startsWithLettersIgnoringASCIICase(host, "mail."_s) && topPrivatelyControlledDomain(host.toString()).startsWith("yahoo."_s);
}
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

// ceac.state.gov https://bugs.webkit.org/show_bug.cgi?id=193478
bool Quirks::needsFormControlToBeMouseFocusable() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return equalLettersIgnoringASCIICase(host, "ceac.state.gov"_s) || host.endsWithIgnoringASCIICase(".ceac.state.gov"_s);
#else
    return false;
#endif
}

bool Quirks::needsAutoplayPlayPauseEvents() const
{
    if (!needsQuirks())
        return false;

    if (allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::SynthesizedPauseEvents))
        return true;

    return allowedAutoplayQuirks(m_document->topDocument()).contains(AutoplayQuirk::SynthesizedPauseEvents);
}

// netflix.com https://bugs.webkit.org/show_bug.cgi?id=173030
bool Quirks::needsSeekingSupportDisabled() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "netflix.com"_s) || host.endsWithIgnoringASCIICase(".netflix.com"_s);
}

// netflix.com https://bugs.webkit.org/show_bug.cgi?id=193301
bool Quirks::needsPerDocumentAutoplayBehavior() const
{
#if PLATFORM(MAC)
    ASSERT(m_document == &m_document->topDocument());
    return needsQuirks() && allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::PerDocumentAutoplayBehavior);
#else
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "netflix.com"_s) || host.endsWithIgnoringASCIICase(".netflix.com"_s);
#endif
}

// bing.com https://bugs.webkit.org/show_bug.cgi?id=213118
// zoom.com https://bugs.webkit.org/show_bug.cgi?id=223180
bool Quirks::shouldAutoplayWebAudioForArbitraryUserGesture() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "www.bing.com"_s) || host.endsWithIgnoringASCIICase(".zoom.us"_s);
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

    auto domain = RegistrableDomain(m_document->url()).string();
    m_hasBrokenEncryptedMediaAPISupportQuirk = domain == "starz.com"_s || domain == "youtube.com"_s || domain == "hulu.com"_s;

    return m_hasBrokenEncryptedMediaAPISupportQuirk.value();
#endif
}

bool Quirks::shouldDisableContentChangeObserver() const
{
    if (!needsQuirks())
        return false;

    auto& topDocument = m_document->topDocument();

    auto host = topDocument.url().host();
    auto isYouTube = host.endsWith(".youtube.com"_s) || host == "youtube.com"_s;

    if (isYouTube && (topDocument.url().path().startsWithIgnoringASCIICase("/results"_s) || topDocument.url().path().startsWithIgnoringASCIICase("/watch"_s)))
        return true;

    return false;
}

// youtube.com https://bugs.webkit.org/show_bug.cgi?id=200609
bool Quirks::shouldDisableContentChangeObserverTouchEventAdjustment() const
{
    if (!needsQuirks())
        return false;

    auto& topDocument = m_document->topDocument();
    auto* topDocumentLoader = topDocument.loader();
    if (!topDocumentLoader || !topDocumentLoader->allowContentChangeObserverQuirk())
        return false;

    auto host = m_document->topDocument().url().host();
    return host.endsWith(".youtube.com"_s) || host == "youtube.com"_s;
}

// covid.cdc.gov https://bugs.webkit.org/show_bug.cgi?id=223620
bool Quirks::shouldTooltipPreventFromProceedingWithClick(const Element& element) const
{
    if (!needsQuirks())
        return false;

    if (!equalLettersIgnoringASCIICase(m_document->topDocument().url().host(), "covid.cdc.gov"_s))
        return false;
    return element.hasClass() && element.classNames().contains("tooltip"_s);
}

// google.com https://bugs.webkit.org/show_bug.cgi?id=223700
// FIXME: Remove after the site is fixed, <rdar://problem/75792913>
bool Quirks::shouldHideSearchFieldResultsButton() const
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    if (!needsQuirks())
        return false;

    if (topPrivatelyControlledDomain(m_document->topDocument().url().host().toString()).startsWith("google."_s))
        return true;
#endif
    return false;
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=161984
bool Quirks::isTouchBarUpdateSupressedForHiddenContentEditable() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "docs.google.com"_s);
#else
    return false;
#endif
}

// icloud.com rdar://26013388
// twitter.com rdar://28036205
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

    if (equalLettersIgnoringASCIICase(host, "twitter.com"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "onedrive.live.com"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "trix-editor.org"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.icloud.com"_s)) {
        auto path = url.path();
        if (path.contains("notes"_s) || url.fragmentIdentifier().contains("notes"_s))
            return true;
    }
#endif

    return false;
}

// docs.google.com rdar://49864669
static bool shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasForHost(StringView host)
{
#if PLATFORM(IOS_FAMILY)
    return equalLettersIgnoringASCIICase(host, "docs.google.com"_s);
#else
    UNUSED_PARAM(host);
    return false;
#endif
}

// weebly.com rdar://48003980
// medium.com rdar://50457837
bool Quirks::shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const
{
    if (m_document->settings().shouldDispatchSyntheticMouseEventsWhenModifyingSelection())
        return true;

    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    if (equalLettersIgnoringASCIICase(host, "medium.com"_s) || host.endsWithIgnoringASCIICase(".medium.com"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "weebly.com"_s) || host.endsWithIgnoringASCIICase(".weebly.com"_s))
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

    return equalLettersIgnoringASCIICase(m_document->url().host(), "www.youtube.com"_s);
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
    return equalLettersIgnoringASCIICase(url.host(), "mail.google.com"_s);
#else
    return false;
#endif
}

// rdar://49864669
bool Quirks::shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreas() const
{
    if (!needsQuirks())
        return false;

    return shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreasForHost(m_document->topDocument().url().host());
}

#if ENABLE(TOUCH_EVENTS)
bool Quirks::isAmazon() const
{
    return topPrivatelyControlledDomain(m_document->topDocument().url().host().toString()).startsWith("amazon."_s);
}

bool Quirks::isGoogleMaps() const
{
    auto& url = m_document->topDocument().url();
    return topPrivatelyControlledDomain(url.host().toString()).startsWith("google."_s) && startsWithLettersIgnoringASCIICase(url.path(), "/maps/"_s);
}

// rdar://49124313
// desmos.com rdar://47068176
// msn.com rdar://49403260
// flipkart.com rdar://49648520
// iqiyi.com rdar://53235709
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
        auto host = url.host().convertToASCIILowercase();

        if (host == "wix.com"_s || host.endsWith(".wix.com"_s)) {
            // Disable simulated mouse dispatching for template selection.
            return startsWithLettersIgnoringASCIICase(url.path(), "/website/templates/"_s) ? ShouldDispatchSimulatedMouseEvents::No : ShouldDispatchSimulatedMouseEvents::Yes;
        }

        if (host == "trello.com"_s || host.endsWith(".trello.com"_s))
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (host == "airtable.com"_s || host.endsWith(".airtable.com"_s))
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (host == "msn.com"_s || host.endsWith(".msn.com"_s))
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (host == "flipkart.com"_s || host.endsWith(".flipkart.com"_s))
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (host == "iqiyi.com"_s || host.endsWith(".iqiyi.com"_s))
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (host == "trailers.apple.com"_s)
            return ShouldDispatchSimulatedMouseEvents::Yes;
        if (host == "soundcloud.com"_s)
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
        if (host == "mybinder.org"_s || host.endsWith(".mybinder.org"_s))
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
        if (is<Node>(target)) {
            for (const auto* node = downcast<Node>(target); node; node = node->parentNode()) {
                if (is<Element>(node) && const_cast<Element&>(downcast<Element>(*node)).classList().contains("lm-DockPanel-tabBar"_s))
                    return true;
            }
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

    if (isAmazon() && is<Element>(target)) {
        // When panning on an Amazon product image, we're either touching on the #magnifierLens element
        // or its previous sibling.
        auto& element = downcast<Element>(*target);
        if (element.getIdAttribute() == "magnifierLens"_s)
            return true;
        if (auto* sibling = element.nextElementSibling())
            return sibling->getIdAttribute() == "magnifierLens"_s;
    }

    if (equalLettersIgnoringASCIICase(m_document->topDocument().url().host(), "soundcloud.com"_s) && is<Element>(target))
        return downcast<Element>(*target).classList().contains("sceneLayer"_s);

    return false;
}

// maps.google.com https://bugs.webkit.org/show_bug.cgi?id=199904
// desmos.com rdar://50925173
// airtable.com rdar://51557377
std::optional<Event::IsCancelable> Quirks::simulatedMouseEventTypeForTarget(EventTarget* target) const
{
    if (!shouldDispatchSimulatedMouseEvents(target))
        return { };

    // On Google Maps, we want to limit simulated mouse events to dragging the little man that allows entering into Street View.
    if (isGoogleMaps()) {
        if (is<Element>(target) && downcast<Element>(target)->getAttribute(HTMLNames::classAttr) == "widget-expand-button-pegman-icon"_s)
            return Event::IsCancelable::Yes;
        return { };
    }

    auto host = m_document->topDocument().url().host();
    if (equalLettersIgnoringASCIICase(host, "desmos.com"_s) || host.endsWithIgnoringASCIICase(".desmos.com"_s))
        return Event::IsCancelable::No;

    if (equalLettersIgnoringASCIICase(host, "airtable.com"_s) || host.endsWithIgnoringASCIICase(".airtable.com"_s)) {
        // We want to limit simulated mouse events to elements under <div id="paneContainer"> to allow for column re-ordering and multiple cell selection.
        if (is<Node>(target)) {
            auto* node = downcast<Node>(target);
            if (auto* paneContainer = node->treeScope().getElementById(AtomString("paneContainer"_s))) {
                if (paneContainer->contains(node))
                    return Event::IsCancelable::Yes;
            }
        }
        return { };
    }

    return Event::IsCancelable::Yes;
}

// youtube.com rdar://53415195
bool Quirks::shouldMakeTouchEventNonCancelableForTarget(EventTarget* target) const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();

    if (equalLettersIgnoringASCIICase(host, "www.youtube.com"_s)) {
        if (is<Element>(target)) {
            unsigned depth = 3;
            for (auto* element = downcast<Element>(target); element && depth; element = element->parentElement(), --depth) {
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

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "shutterstock.com"_s) || host.endsWithIgnoringASCIICase(".shutterstock.com"_s);
}

// sites.google.com rdar://58653069
bool Quirks::shouldPreventDispatchOfTouchEvent(const AtomString& touchEventType, EventTarget* target) const
{
    if (!needsQuirks())
        return false;

    if (is<Element>(target) && touchEventType == eventNames().touchendEvent && equalLettersIgnoringASCIICase(m_document->topDocument().url().host(), "sites.google.com"_s)) {
        auto& classList = downcast<Element>(*target).classList();
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
        m_shouldSynthesizeTouchEventsQuirk = isYahooMail(*m_document);
    return m_shouldSynthesizeTouchEventsQuirk.value();
}
#endif

// live.com rdar://52116170
// sharepoint.com rdar://52116170
// twitter.com rdar://59016252
// maps.google.com https://bugs.webkit.org/show_bug.cgi?id=214945
bool Quirks::shouldAvoidResizingWhenInputViewBoundsChange() const
{
    if (!needsQuirks())
        return false;

    auto& url = m_document->topDocument().url();
    auto host = url.host();

    if (equalLettersIgnoringASCIICase(host, "live.com"_s) || host.endsWithIgnoringASCIICase(".live.com"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "twitter.com"_s) || host.endsWithIgnoringASCIICase(".twitter.com"_s))
        return true;

    if ((equalLettersIgnoringASCIICase(host, "google.com"_s) || host.endsWithIgnoringASCIICase(".google.com"_s)) && url.path().startsWithIgnoringASCIICase("/maps/"_s))
        return true;

    if (host.endsWithIgnoringASCIICase(".sharepoint.com"_s))
        return true;

    return false;
}

// mailchimp.com rdar://47868965
bool Quirks::shouldDisablePointerEventsQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    auto& url = m_document->topDocument().url();
    auto host = url.host();
    if (equalLettersIgnoringASCIICase(host, "mailchimp.com"_s) || host.endsWithIgnoringASCIICase(".mailchimp.com"_s))
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
    return equalLettersIgnoringASCIICase(url.host(), "docs.google.com"_s) && startsWithLettersIgnoringASCIICase(url.path(), "/spreadsheets/"_s);
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
        m_needsGMailOverflowScrollQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "mail.google.com"_s);

    return *m_needsGMailOverflowScrollQuirk;
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
        m_needsYouTubeOverflowScrollQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "www.youtube.com"_s);

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

    if (!m_needsFullscreenDisplayNoneQuirk) {
        auto host = m_document->topDocument().url().host();
        m_needsFullscreenDisplayNoneQuirk = equalLettersIgnoringASCIICase(host, "gizmodo.com"_s) || host.endsWithIgnoringASCIICase(".gizmodo.com"_s);
    }

    return *m_needsFullscreenDisplayNoneQuirk;
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

    return RegistrableDomain(m_document->topDocument().url()) == "youtube.com"_s;
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

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "nytimes.com"_s) || host.endsWithIgnoringASCIICase(".nytimes.com"_s)
        || equalLettersIgnoringASCIICase(host, "twitter.com"_s) || host.endsWithIgnoringASCIICase(".twitter.com"_s)
        || equalLettersIgnoringASCIICase(host, "zillow.com"_s) || host.endsWithIgnoringASCIICase(".zillow.com"_s);
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

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "twitter.com"_s) || host.endsWithIgnoringASCIICase(".twitter.com"_s);
#else
    return false;
#endif
}

// zillow.com rdar://53103732
bool Quirks::shouldAvoidScrollingWhenFocusedContentIsVisible() const
{
    if (!needsQuirks())
        return false;

    return equalLettersIgnoringASCIICase(m_document->url().host(), "www.zillow.com"_s);
}

// att.com rdar://55185021
bool Quirks::shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return equalLettersIgnoringASCIICase(host, "att.com"_s) || host.endsWithIgnoringASCIICase(".att.com"_s);
}

// ralphlauren.com rdar://55629493
bool Quirks::shouldIgnoreAriaForFastPathContentObservationCheck() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return equalLettersIgnoringASCIICase(host, "www.ralphlauren.com"_s);
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
    if (!equalLettersIgnoringASCIICase(openerURL.host(), "docs.google.com"_s))
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

    auto topURL = m_document->topDocument().url();
    auto host = topURL.host();

    // Vimeo.com used to bypass the back/forward cache by serving "Cache-Control: no-store" over HTTPS.
    // We started caching such content in r250437 but the vimeo.com content unfortunately is not currently compatible
    // because it changes the opacity of its body to 0 when navigating away and fails to restore the original opacity
    // when coming back from the back/forward cache (e.g. in 'pageshow' event handler). See <rdar://problem/56996057>.
    if (topURL.protocolIs("https"_s) && equalLettersIgnoringASCIICase(host, "vimeo.com"_s)) {
        if (auto* documentLoader = m_document->frame() ? m_document->frame()->loader().documentLoader() : nullptr)
            return documentLoader->response().cacheControlContainsNoStore();
    }

    // Google Docs used to bypass the back/forward cache by serving "Cache-Control: no-store" over HTTPS.
    // We started caching such content in r250437 but the Google Docs index page unfortunately is not currently compatible
    // because it puts an overlay (with class "docs-homescreen-freeze-el-full") over the page when navigating away and fails
    // to remove it when coming back from the back/forward cache (e.g. in 'pageshow' event handler). See <rdar://problem/57670064>.
    // Note that this does not check for docs.google.com host because of hosted G Suite apps.
    static MainThreadNeverDestroyed<const AtomString> googleDocsOverlayDivClass("docs-homescreen-freeze-el-full"_s);
    auto* firstChildInBody = m_document->body() ? m_document->body()->firstChild() : nullptr;
    if (is<HTMLDivElement>(firstChildInBody)) {
        auto& div = downcast<HTMLDivElement>(*firstChildInBody);
        if (div.hasClass() && div.classNames().contains(googleDocsOverlayDivClass))
            return true;
    }

    return false;
}

// bungalow.com rdar://61658940
bool Quirks::shouldBypassAsyncScriptDeferring() const
{
    if (!needsQuirks())
        return false;

    if (!m_shouldBypassAsyncScriptDeferring) {
        auto domain = RegistrableDomain { m_document->topDocument().url() };
        // Deferring 'mapbox-gl.js' script on bungalow.com causes the script to get in a bad state (rdar://problem/61658940).
        m_shouldBypassAsyncScriptDeferring = (domain == "bungalow.com"_s);
    }
    return *m_shouldBypassAsyncScriptDeferring;
}

// smoothscroll JS library rdar://52712513
bool Quirks::shouldMakeEventListenerPassive(const EventTarget& eventTarget, const AtomString& eventType, const EventListener& eventListener)
{
    auto eventTargetIsRoot = [](const EventTarget& eventTarget) {
        if (is<LocalDOMWindow>(eventTarget))
            return true;

        if (is<Node>(eventTarget)) {
            auto& node = downcast<Node>(eventTarget);
            return is<Document>(node) || node.document().documentElement() == &node || node.document().body() == &node;
        }
        return false;
    };

    auto documentFromEventTarget = [](const EventTarget& eventTarget) -> Document* {
        return downcast<Document>(eventTarget.scriptExecutionContext());
    };

    if (eventNames().isTouchScrollBlockingEventType(eventType)) {
        if (eventTargetIsRoot(eventTarget)) {
            if (auto* document = documentFromEventTarget(eventTarget))
                return document->settings().passiveTouchListenersAsDefaultOnDocument();
        }
        return false;
    }

    if (eventNames().isWheelEventType(eventType)) {
        if (eventTargetIsRoot(eventTarget)) {
            if (auto* document = documentFromEventTarget(eventTarget))
                return document->settings().passiveWheelListenersAsDefaultOnDocument();
        }
        return false;
    }

    if (eventType == eventNames().mousewheelEvent) {
        if (!is<JSEventListener>(eventListener))
            return false;

        // For SmoothScroll.js
        // Matches Blink intervention in https://chromium.googlesource.com/chromium/src/+/b6b13c9cfe64d52a4168d9d8d1ad9bb8f0b46a2a%5E%21/
        if (is<LocalDOMWindow>(eventTarget)) {
            auto* document = downcast<LocalDOMWindow>(eventTarget).document();
            if (!document || !document->quirks().needsQuirks())
                return false;

            auto& jsEventListener = downcast<JSEventListener>(eventListener);
            if (jsEventListener.functionName() == "ssc_wheel"_s)
                return true;
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
#endif

// nfl.com rdar://58807210
bool Quirks::shouldDisableElementFullscreenQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (m_shouldDisableElementFullscreenQuirk)
        return m_shouldDisableElementFullscreenQuirk.value();

    auto domain = m_document->securityOrigin().domain().convertToASCIILowercase();

    m_shouldDisableElementFullscreenQuirk = domain == "nfl.com"_s || domain.endsWith(".nfl.com"_s);

    return m_shouldDisableElementFullscreenQuirk.value();
#else
    return false;
#endif
}

// hulu.com rdar://55041979
bool Quirks::needsCanPlayAfterSeekedQuirk() const
{
    if (!needsQuirks())
        return false;

    if (m_needsCanPlayAfterSeekedQuirk)
        return *m_needsCanPlayAfterSeekedQuirk;

    auto domain = m_document->securityOrigin().domain().convertToASCIILowercase();

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

// shutterstock.com rdar://58843932
bool Quirks::shouldIgnoreContentObservationForSyntheticClick(bool isFirstSyntheticClickOnPage) const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return isFirstSyntheticClickOnPage && (equalLettersIgnoringASCIICase(host, "shutterstock.com"_s) || host.endsWithIgnoringASCIICase(".shutterstock.com"_s));
}

// mail.yahoo.com rdar://63511613
bool Quirks::shouldAvoidPastingImagesAsWebContent() const
{
    if (!needsQuirks())
        return false;

#if PLATFORM(IOS_FAMILY)
    if (!m_shouldAvoidPastingImagesAsWebContent)
        m_shouldAvoidPastingImagesAsWebContent = isYahooMail(*m_document);
    return *m_shouldAvoidPastingImagesAsWebContent;
#else
    return false;
#endif
}

#if ENABLE(TRACKING_PREVENTION)
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
    return url.host() == "teams.microsoft.com"_s && url.query().toString().contains("Retried+3+times+without+success"_s);
}

// microsoft.com rdar://72453487
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
    // Skype case.
    // FIXME(220105): Remove this quirk once Skype under outlook.live.com completes their login flow redesign.
    if (url.host() == "outlook.live.com"_s) {
        return element.hasClass()
        && (element.classNames().contains("_3ioEp2RGR5vb0gqRDsaFPa"_s)
        || element.classNames().contains("_2Am2jvTaBz17UJ8XnfxFOy"_s));
    }
    // Sony Network Entertainment login case.
    // FIXME(218760): Remove this quirk once playstation.com completes their login flow redesign.
    if (url.host() == "www.playstation.com"_s || url.host() == "my.playstation.com"_s) {
        return element.hasClass()
        && (element.classNames().contains("web-toolbar__signin-button"_s)
        || element.classNames().contains("web-toolbar__signin-button-label"_s)
        || element.classNames().contains("sb-signin-button"_s));
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

const String& Quirks::staticRadioPlayerURLString()
{
    static NeverDestroyed<String> staticRadioPlayerURLString = "https://static.radioplayer.co.uk/"_s;
    return staticRadioPlayerURLString;
}

Quirks::StorageAccessResult Quirks::requestStorageAccessAndHandleClick(CompletionHandler<void(ShouldDispatchClick)>&& completionHandler) const
{
    auto firstPartyDomain = RegistrableDomain(m_document->topDocument().url());
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

    if (!m_document) {
        completionHandler(ShouldDispatchClick::No);
        return Quirks::StorageAccessResult::ShouldNotCancelEvent;
    }

    DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(*m_document, WTFMove(domainInNeedOfStorageAccess), [firstPartyDomain, domainInNeedOfStorageAccess, completionHandler = WTFMove(completionHandler)](StorageAccessWasGranted storageAccessGranted) mutable {
        if (storageAccessGranted == StorageAccessWasGranted::No) {
            completionHandler(ShouldDispatchClick::Yes);
            return;
        }

        ResourceLoadObserver::shared().setDomainsWithCrossPageStorageAccess({{ firstPartyDomain, domainInNeedOfStorageAccess }}, [completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(ShouldDispatchClick::Yes);
        });
    });
    return Quirks::StorageAccessResult::ShouldCancelEvent;
}
#endif

// rdar://64549429
Quirks::StorageAccessResult Quirks::triggerOptionalStorageAccessQuirk(Element& element, const PlatformMouseEvent& platformEvent, const AtomString& eventType, int detail, Element* relatedTarget, bool isParentProcessAFullWebBrowser, IsSyntheticClick isSyntheticClick) const
{
    if (!DeprecatedGlobalSettings::trackingPreventionEnabled() || !isParentProcessAFullWebBrowser)
        return Quirks::StorageAccessResult::ShouldNotCancelEvent;

#if ENABLE(TRACKING_PREVENTION)
    if (!needsQuirks())
        return Quirks::StorageAccessResult::ShouldNotCancelEvent;

    RegistrableDomain domain { m_document->url() };

    static NeverDestroyed<HashSet<RegistrableDomain>> kinjaQuirks = [] {
        HashSet<RegistrableDomain> set;
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("avclub.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("gizmodo.com"_s));
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

    if (eventType == eventNames().clickEvent) {
        if (!m_document)
            return Quirks::StorageAccessResult::ShouldNotCancelEvent;

        // Embedded YouTube case.
        if (element.hasClass() && domain == youTubeDomain && !m_document->isTopDocument() && ResourceLoadObserver::shared().hasHadUserInteraction(youTubeDomain)) {
            auto& classNames = element.classNames();
            if (classNames.contains("ytp-watch-later-icon"_s) || classNames.contains("ytp-watch-later-icon"_s)) {
                if (ResourceLoadObserver::shared().hasHadUserInteraction(youTubeDomain)) {
                    DocumentStorageAccess::requestStorageAccessForDocumentQuirk(*m_document, [](StorageAccessWasGranted) { });
                    return Quirks::StorageAccessResult::ShouldNotCancelEvent;
                }
            }
            return Quirks::StorageAccessResult::ShouldNotCancelEvent;
        }

        // Kinja login case.
        if (kinjaQuirks.get().contains(domain) && isKinjaLoginAvatarElement(element)) {
            if (ResourceLoadObserver::shared().hasHadUserInteraction(kinjaDomain)) {
                DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(*m_document, kinjaDomain.get().isolatedCopy(), [](StorageAccessWasGranted) { });
                return Quirks::StorageAccessResult::ShouldNotCancelEvent;
            }

            auto* domWindow = m_document->domWindow();
            if (!domWindow)
                return Quirks::StorageAccessResult::ShouldNotCancelEvent;

            ExceptionOr<RefPtr<WindowProxy>> proxyOrException =  domWindow->open(*domWindow, *domWindow, kinjaURL->string(), emptyAtom(), loginPopupWindowFeatureString);
            if (proxyOrException.hasException())
                return Quirks::StorageAccessResult::ShouldNotCancelEvent;
            auto proxy = proxyOrException.releaseReturnValue();

            auto* abstractFrame = proxy->frame();
            if (abstractFrame && is<LocalFrame>(*abstractFrame)) {
                auto& frame = downcast<LocalFrame>(*abstractFrame);
                auto world = ScriptController::createWorld("kinjaComQuirkWorld"_s, ScriptController::WorldType::User);
                frame.addUserScriptAwaitingNotification(world.get(), kinjaLoginUserScript);
                return Quirks::StorageAccessResult::ShouldCancelEvent;
            }
        }

        // If the click is synthetic, the user has already gone through the storage access flow and we should not request again.
        if (isStorageAccessQuirkDomainAndElement(m_document->url(), element) && isSyntheticClick == IsSyntheticClick::No) {
            return requestStorageAccessAndHandleClick([element = WeakPtr { element }, platformEvent, eventType, detail, relatedTarget] (ShouldDispatchClick shouldDispatchClick) mutable {
                RefPtr protectedElement { element.get() };
                if (!protectedElement)
                    return;

                if (shouldDispatchClick == ShouldDispatchClick::Yes)
                    protectedElement->dispatchMouseEvent(platformEvent, eventType, detail, relatedTarget, IsSyntheticClick::Yes);
            });
        }
    }
#else
    UNUSED_PARAM(element);
    UNUSED_PARAM(platformEvent);
    UNUSED_PARAM(eventType);
    UNUSED_PARAM(detail);
    UNUSED_PARAM(relatedTarget);
#endif
    return Quirks::StorageAccessResult::ShouldNotCancelEvent;
}

// youtube.com rdar://66242343
bool Quirks::needsVP9FullRangeFlagQuirk() const
{
    if (!needsQuirks())
        return false;

    if (!m_needsVP9FullRangeFlagQuirk)
        m_needsVP9FullRangeFlagQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "www.youtube.com"_s);

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
        m_requiresUserGestureToPauseInPictureInPicture = domain == "facebook.com"_s || domain == "twitter.com"_s || domain == "reddit.com"_s;
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

    if (!m_requiresUserGestureToLoadInPictureInPicture) {
        auto domain = RegistrableDomain(m_document->topDocument().url());
        m_requiresUserGestureToLoadInPictureInPicture = domain.string() == "twitter.com"_s;
    }

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

    if (!m_blocksReturnToFullscreenFromPictureInPictureQuirk) {
        auto domain = RegistrableDomain { m_document->topDocument().url() };
        m_blocksReturnToFullscreenFromPictureInPictureQuirk = domain == "vimeo.com"_s;
    }

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
        auto domain = RegistrableDomain { m_document->topDocument().url() };
        m_blocksEnteringStandardFullscreenFromPictureInPictureQuirk = domain == "vimeo.com"_s;
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

        m_shouldDisableEndFullscreenEventWhenEnteringPictureInPictureFromFullscreenQuirk = equalLettersIgnoringASCIICase(host, "trailers.apple.com"_s) || domain == "espn.com"_s || domain == "vimeo.com"_s;
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

    if (!m_shouldDelayFullscreenEventWhenExitingPictureInPictureQuirk) {
        auto domain = RegistrableDomain(m_document->topDocument().url());
        m_shouldDelayFullscreenEventWhenExitingPictureInPictureQuirk = domain == "bbc.com"_s;
    }

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

// mail.google.com rdar://97351877
bool Quirks::shouldEnableApplicationCacheQuirk() const
{
    bool shouldEnableBySetting = m_document && m_document->settings().offlineWebApplicationCacheEnabled();
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return shouldEnableBySetting;

    if (!m_shouldEnableApplicationCacheQuirk) {
        auto domain = m_document->securityOrigin().domain().convertToASCIILowercase();
        if (domain.endsWith("mail.google.com"_s))
            m_shouldEnableApplicationCacheQuirk = true;
        else
            m_shouldEnableApplicationCacheQuirk = shouldEnableBySetting;
    }

    return m_shouldEnableApplicationCacheQuirk.value();
#else
    return shouldEnableBySetting;
#endif
}

// play.hbomax.com https://bugs.webkit.org/show_bug.cgi?id=244737
bool Quirks::shouldEnableFontLoadingAPIQuirk() const
{
    if (!needsQuirks() || m_document->settings().downloadableBinaryFontAllowedTypes() == DownloadableBinaryFontAllowedTypes::Any)
        return false;

    if (!m_shouldEnableFontLoadingAPIQuirk)
        m_shouldEnableFontLoadingAPIQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "play.hbomax.com"_s);

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
        auto domain = RegistrableDomain(m_document->url()).string();
        // Marcus: <rdar://101086391>.
        // Pandora: <rdar://100243111>.
        // Soundcloud: <rdar://102913500>.
        m_shouldExposeShowModalDialog = domain == "pandora.com"_s || domain == "marcus.com"_s || domain == "soundcloud.com"_s;
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
        auto domain = RegistrableDomain(m_document->url()).string();
        // Marcus login issue: <rdar://103011164>.
        m_shouldNavigatorPluginsBeEmpty = domain == "marcus.com"_s;
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

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "victoriassecret.com"_s) || host.endsWithIgnoringASCIICase(".victoriassecret.com"_s);
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

#if PLATFORM(COCOA)
bool Quirks::shouldAdvertiseSupportForHLSSubtitleTypes() const
{
    if (!needsQuirks())
        return false;

    if (!m_shouldAdvertiseSupportForHLSSubtitleTypes) {
        auto domain = RegistrableDomain(m_document->url()).string();
        m_shouldAdvertiseSupportForHLSSubtitleTypes = domain == "hulu.com"_s;
    }

    return *m_shouldAdvertiseSupportForHLSSubtitleTypes;
}
#endif

// apple-console.lrn.com (rdar://106779034)
bool Quirks::shouldDisablePopoverAttributeQuirk() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "apple-console.lrn.com"_s);
}

// ungap/@custom-elements polyfill (rdar://problem/111008826).
bool Quirks::needsConfigurableIndexedPropertiesQuirk() const
{
    return needsQuirks() && m_needsConfigurableIndexedPropertiesQuirk;
}

// Canvas fingerprinting (rdar://107564162)
bool Quirks::shouldEnableCanvas2DAdvancedPrivacyProtectionQuirk() const
{
    if (!needsQuirks() || !m_document->noiseInjectionHashSalt())
        return false;
    auto& url = m_document->topDocument().url();
    auto host = url.host();
    auto path = url.path();

    if (!equalLettersIgnoringASCIICase(host, "walgreens.com"_s) && !host.endsWithIgnoringASCIICase(".walgreens.com"_s)
        && !equalLettersIgnoringASCIICase(host, "fedex.com"_s) && !host.endsWithIgnoringASCIICase(".fedex.com"_s))
        return false;

    if ((host.endsWithIgnoringASCIICase("walgreens.com"_s) && equalLettersIgnoringASCIICase(path, "/login.jsp"_s))
        || (host.endsWithIgnoringASCIICase("fedex.com"_s) && (path.startsWithIgnoringASCIICase("/secure-login/"_s) || path.startsWithIgnoringASCIICase("/fedextrack/"_s))))
        return true;

    return false;
}

String Quirks::advancedPrivacyProtectionSubstituteDataURLForText(const String& text) const
{
    if (text == "<@nv45. F1n63r,Pr1n71n6!"_s)
        return "data:image/png;base64,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A"_s;
    if (text == "!H71JCaj)]# 1@#"_s)
        return "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAASwAAACWCAYAAABkW7XSAAAAAXNSR0IArs4c6QAAAERlWElmTU0AKgAAAAgAAYdpAAQAAAABAAAAGgAAAAAAA6ABAAMAAAABAAEAAKACAAQAAAABAAABLKADAAQAAAABAAAAlgAAAABJS0H3AAAnpklEQVR4Ae2dCXwVRZ7Hq7rfy00IgQAh3IqM4U4CeKCAiCKKOnjNzognAio66+w6s5/POsqMzs6oO7OzDqLiOeO4OnLsx2VA8QAEHJUzHKKAhoQj4Q4kgSTvve7a37/6db/3cr4AaoB/fz4vXV1XV3+r6l//+ld1Rwg+mAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbQKgnIpkqlnn46VaSlZek4hlElysoOyxkz7KbStCRMvfBCthUKFZg+X7ZQKhFpgyHL2uq///6lLcmnNcelZxRSXqnLaNufyGnTtsdT3uDMmWMluJhSVsupU+fEk+ZsiKOeeSZdJCVNpGcNSLku8Z57Nn5bz62ee26YMIzzdf61tW/JBx+sbe5eJ1rflK+aMcMnsrMzRG1tW9zXxHMeFLt3V6HPBZq779kS7qv7oICWEMzJyfMLMUrYdlJMeDbkyuzZe+G/QVjWJnn//VUx4S24QD79IKQmQliZOpl0ZKeUsgqNMkskJHTR/nv3bmqpkFQvvvhvKF/ssxmGhfyC+B0VPt9GEQyugyCg62/3UCoNN+ipbyLll/HezJeQ0BN8ciDsquNN09J4DXJqIBOMUMfNadP+0EDQt+IFQdEfz24I0zyCOtoZc5O0tETUXU/ySxCiDKdvTWChnWSjrfeie4nUVNxONCuwUO4W17eaObOH8PuHIf8++CWIRBq7cVhosl261Aaee+4j/75969APQk7A2fvXiH50ElYAdJfftsfVE1ZuRKU6a43B53Mq0vVvwRn3MTCCXIUkrrDajzw34Z7f4HcEwmogrn+of9nZnVqQtRPVthOR1hfzIw2OGhMJgWDwKoTdp559lhrX2Xs0xKkuN7pWKvm7gqSUkmgbN0JYTcT54u/qvt/rfRITB+P+/fAjoRh7oN36DWN8qEOH0bEBZ+dVrBbSpcs4NM7OYRQK7p3o2NshQGohSLLtQCDHMIyWC5C6bLt27YbRwxEWSn2JUfRv0VHU88+Pib4+CXcQjX4VjVSYeiaaCQk09czR+SnVDiPoHXDPPIn8z5SkFup5Q2MPY1vWt6bl1bvnnDkxg2i98DPYw1bqEPrXl2iju9HffBDaBXD3pEf2+f0Xq1df3STvvHPvGYyg2UfzBJaePyvVP5xCoaP/Rd5zz466OUCYFKBxXw6Yqm5Y3NeBQFukd6Ibxua407U0olIVeIYPopPBxvADXN+Cn8Svg3rllSx5110HouOcdW6laOr1f2fdc7eWB7asPRg8y4zdu2OmfeiTX8GmdQeK2VUXtbqaBlsWWBpGZmY7nLVKatv2IXPq1HrCiuLBaLwGnX6DuPvukJg8WSelP+qll3pgNMjHrzN+Gcij3PD7D4pQqDDa0Ay7yVj49XYTQvMZAnvWOfpaygqkpenHuThrL4SPQvgxNz6mc0tOxnaGjvkVhO42CN2+Os/aWmoMWmBpI6uUpEEGUeb3vHuGHXjGIXB2hcZmYwFicbRNQU+nc3IusiyrOwzlXVB+EvqHrWCwAna6ull515qbbReAVxfD52uLkfWoFQjsQHpnsQMxYdNLhJZ7hU5kmscghJd4GUQ5UC8jMBC0Qx5KlJa+/20Ya/V0PidnFG5L3DrhXhLPXGr6/ftEdfXyhgzT8aZRs2b1tg4c6OfxkrI76v5a7xF9vn1wF3nXcKDOOlq2PVyaZjdDynao1/0o02bU36fR8Vx3vO3Ujd/Y+UTru6H8qE816A+bVeDFFzfDROMILCk7NBTvbPKL9KTkZAMVrZ8dFZ9JK4Ty4YcjgiKKCjp9UEyd6vlgRetSpB2FTuap83rqaFmd0ID6BWfNWunbv39ZuIPnoyN7xnzTND3hhDzKEZ8Ep3eg8fZ1hZf29PkKcT5hY7/OQ8qQdwMpw6oefFBW/HrARasy76mbbkoQ27b1RHmrxIED5RBU58KfbA1C5OZ6mpv6r//KECkpt6D8tKqng5EPepPKQflpVGzwgDF1GNLQNNwQJSUdRWHhxRQRnT8grrjiPRhiaaFAiMOHlcjJ6YfVo1T7nXeuUsOHfw3BTWWMHKmpf8M981FGqoNK/OoJ3EjkE3Ohs2eivD/Cr2N0DroObftc2+/vCwHytrz33v1ueIvS+Hy9TcsaCmHb3k5NrTDatqVs8ty8IMiLMK2PCCylLsQzDwdjr90Rc/jlQNBlySlTYrTGFrZT97b1zida3/UyisPDL2X7qGjBKPdZ6YwIrD17ymFwt3XnkdKwP//8jyo/fySoKGg5T/g2bHjdJQT/lVAgOuC31pgy5bfo0JchnRNsGEWh2toyX1JSZ2vx4lvN0tJ83GR6aPhwmoZ9JGpqdoi33voj3FiIxKGUhZ8jKcmNeyM/KpduhPY55/yvccklK3Rc+hMMUmcUKAO15i34mUh/vVy37jPyr3uooUPfQwfrFe1vvfRSmnneeZvEpZd+Bo3kUHSYKC9vq1asuBDpJoqiolyo6o7BuVOnUOjVV3eY3buX4N5rjXZRcjU19XqUIVvno1QII34xOhbZ/bLgH9O5KY4aMuQRPOMj4uWXDTFkyDuioOALsN+uVq4cIY8fH6Dz+eADW4wf/y65SVOCZroR+V1s7N+fD698cNLR3D92SsphA1M7ug7Z9lp/AytKWgAXFa1GWkuuWeMIgi1beom1a+9Xw4Z1k6tW/dzNr+4ZGoUP9/9R1PNUIp/t8KPK620cO9bFmDv3t7j+D3XBBWPlZ5993mwaDE4Y2DrYmzY9gvsn2y++OBTPQKvHWgLZWCU0UlN3q9TUr+WoUYvNtm3L6pZLtxdqP4ZRBltbIuWn4yiVp0aO7KcqK2+XhvEUZgSrdDudO/dH4vjxLDF27K9D7dtvoXYKIa81fJ9pjhCdOlF7WF/vPtEeLazv6KQtddvBYHc8k04W8PkOtjT9mRbfE1jhTrEZjWUgPSS0LFpipZ+QWVnDMN14F5qVCywX2w/aoSFU2lKO9oY3234HcbzKVpdeejUElB4hjJSU0WjAS8W8ee/A7y3KN57DKCnZKf/yl780EHca/Lpof6VSGwh3vGybnqF3dLhJmmR1dTEaey3OpV7YqlWDxRdfTJahUIrnF3H4fMFgH/HNN31kUdFI0abNmwjaoG1iYcMorivB73XfffdFaxhD0CGui2QDl2m2QTxatYR0CflDodBnvkOH3hdHj86GVkXCeQj23wyz1q3bZubnOza+QGAdpoXDVd++izAHqxHbt6/QnVWI31PeEKDdcNICy6fUBvKrd3zzTR7SDIRQWe2FlZaeBwYd1DnnDFSTJz/q+Uc5QsHgKgzte/0R4Vsm2rV7Q958s9Z09X69VateF4FAmk6WmztcQGAFs7L6N5kmKWmS9eGHt5q7d2vN0mtHiYlVGJgSDcvKEBUVGbKior94880h4DVJTJ8eVTI4pVwDzfMDdyqK+rgJvloLVsHgBLTT8xDngG6nNTXJ4tChPriuEZs3P+efM0drqbUzZ/ZL8PspHUAal6GdbkB/cAZR7Rn5c0L1HUneIpf6859z0FfIREHPaSfY9tctyuAMjOwJLP1slZXvYqNoRzSMzqg4b9pkdOxInX461OwqhH0mZs92UBiGH4LNUVmVKsFUwBNWOsKxY572AvtMG9G58zkiK6sEGsgcMz3dESIVFUUY8cqdDJ2/ts93kREM9o/2IzdG4SvRwa9E+qG4HFE3vN61YaSJ7t1noyOlQxAkoqPmij17rvLi+XxL3IYeGjRoktiw4UEvDIsBWBX9G+xwm3DPVNswhqHR3IVORcLGjw7VhuKiLAPN8AgIQbQEAtsTVjqvUKgK6bxstSM1tQTl0Y0PU5+dvgMHyN5EHeSYGjToR9DqNsGdIAsLrxaDBn1JaWC32wv+++TIkY6wueyy+ZiW7xb5+Vpgob4coY2tIfK++2J4Unp9SEkaM3Rb/3J9xh+rtDTHxFn26LELz+XJDDeczj7DSLaUGuT5BQLvk7Dy6uONN4ZiWhipj/R0rdGibdRL4+UxZ04O3A9gCphLfqpDh5WyvPwJDARrxM03P4CySLuoqNxYuvSgsqzHIXh6guNSaKW/FrfdRkmoE6/BtO/vzkX4byCwBoK9H+rakMEg2RwtcfXVJShLPqb3XXFtIN0KGRZWlCpx+vQvMJXNA0PStJx2KsR2Cqt7nFB9180kjmsIzQQI4uu8qFJuxrMe967PUkeMwJI/+1k1pnkvBJ9/fijmzgM8Jqbp2FKcTXG0QpgIrYFGI2rr7kGdrOkDWwnQULZjlKJO5ggOpd6INspTBvYll6yEQHDySknRHVb7B4NjoR4/hM7hhDX3l7SYceMoFmUWhJ2oGALLSXXw4JdYHfycLmATSodQ+k8nANedO38uN2wYjZG/2vXD+X9Cv/vdGrFy5UMwuHt2FYgiR2AbRgDTjsLohYiotLHOn/zkY3RIPVWEhPgQQs4bzXHfbdagQU9CwP/SCIXaWcuX53uJQ6F1YK8fCEJtMPx3e2GuwzTXuc56Z8MYjY4sYAv6iMLsqiq/WVPTFfUYED16/B0d2annegkFTXHH6To3zWr5wAM7dPrG6oMGChxIk1U3DfmjMxpiwQLSUHNx7xpMixfKgoK16JCL0f6kHhClVEafPvvkU0+9iSn4ApTtFTCbCI3jUWifL4iuXQ8gfbiRUK7hA9NI7Soqok2fSch/LaZ5ToPZv783hWGapZ8/nEKfMN3aAA2GBBYKiC0vjRwnVN+N5NWkd9eu14Gda06oxKD7fpPxz5LAGIFFz4yRjIxRq9BIqFInkp9dXf0NOhapz3oVEZGckRgaFoXrw7YrXWcT50YbgpsGwuMCaFwX07WVmlpkXn/9J+Ldd3UwtJyFGCmd3cYwtqJBjnHTtfSMzno4Ks2jcDuNIzNzm7z22gVy4cJoYaWjmpmZATFhwgLYfHZAaO1Ex/OJTZsuUWVl3WRlZQo62rjQkCFJMALXWLW1X+H8OhLu9e6zdOlw2K/GYlrcS7Rtq7UQaBSOxuRFwjjQrt1/qOPHH8DUL8MsLh5LHVxrYErRlP1y8Cd7Uq4YNWqRqIzCTp1/8uQtYsqUqNwcZ9h+dRGuLAiST8jXKC6mDixRhiI5ffo75NfQoQVMdvaNOsyyHIGAi0brwzAS1F//mi6OHdOCCx3PS6PzWLToX3EmoX9MjBnzG9GrF02PE9WoUePsgoJhRnb2BCsl5YDZoYOy8vLoYWphc7tB5eW9g2e/VvzjH+OghRHb+keHDkHYIYVVUtLNRCjsWkuwApsBLVHYe/fmUMOFVF5WNyGEVfQCU4PtVNe3O6PAam64r9TN6qSv1csvj8YA2k9nBJsonnnOyayMn3SBWlEG9QSWVzYSDGFDOhr2GvjPhfF9Ajrh+V6caIdphqIvG3RjHt6gf7SnbT/hXQ4ZstRzw+Ffu5autZ8qKLgb7jHR4Q24adf889o/EJiAsztiCdjU0r34lkV50aHE0KHLtKupP2RXatdur9i71xQLF94gbdsR5Eijp4fQYrDSdwku7xFvvDFX3HrrFzq7ffvGYMS/Vhw8KPSPPDt2XIS/C3V4+I9ctqxGXXvtGmiDl+up5/z5JMBXUKOFdroVbpryJKHT5mKKFElqWYXRnQiMZqIOb9ARioqor7bR7lBoKwYkATuT0zGPHv0BrrVBGx38cbOwcFYkU7jS0xO9a+pA4aPR+oCahFVVAyunTsyoNJhG9kLZZ+gA03wcwuoItIduYvHiayB8H9MjYVmZ8wrEN99ciQGKotIg9WcIvoehWY4XR470Ehs39sF0+VOdD/6g/N1x+lQ89ZSJOKmYauoyI/3DxmuvTYZm6oeZQQtQv2kuQny8tmpV+woLSWiTVkVTRe3EueF2mplpIszpM1LSAH7KD6yqDoOwGhnOWOF+c6GB7zzlNzpNM9Tto8Gy27ajyVAg3IB2HMKKtK7I4a7uwScQCKQojNbRv0hEzxXJ0/OKOLAyR9sjHCGUkVFs5ubuhrYVidBSF1YdsTO4hn6iW7f/s0MhSIrwkZnZR739dpoaMKAdGmuG9k1JKcLUqAyNxHSjNXV27V+Io0RycilG8teR10wIdrKrONOV48dvhEbWn/Kxk5O3Ktv+QPl8X9N1+PCEnetBZ6yOrvCuk5MjtgylCl1/2JViB49AYK0bps+GQQKa3lygH7ndw/GjV3Mih/ZDB8+MeDkuMhWAia479OSwFKobK+oaWrp86KEjDaax7WlglIxd3ZvFqlVPa9bLl4+BxqrtXdq/S5d1ApouwjzhSLnTdBnv2c0lJ6aF50bdEWqTFlC0/60ThHwa2lG09t9O+0USdICzM4R7N88LWqHnbuSdQV3fNH3GQSuSUfFPiRODUR6eY3w4M4VnmId+99UpyfwMycQTWNi01w5G3fO853KnXuThuj/8sNwLJ0cgcD6mQY/RL+G11zbCAGzH/IS4Lya+aZLdoanjt15gXt4y7U5Li0t4eOkaccjx42uNrVs/8IJ9viTr4MEbsMjgdXqVmFimwzHa6s2aXuTGHXbfvm+EJkz4o5g0abZx553Tsb3iAWwBmQBNaqyXqrRUT/+M666bZ6xff4XdvftjXlhaWnvPHeUwzj+/DC/cOtPWUOgyL2jv3h1wV9I1bHm5nn8gcBQdKpZvSsptCM/SP8P4jOLCfjUeGgh9DSCbrnEO2Ndf/29wOfEqK5/W/nX/WJYuCwk0vQ+pbnhCQqQDh0LHdHADaSCwnTJL+Zx45pk0u7T0HFFcXKDjKzXPKCjIE9dcs0DceOObYPtc3dtAa1ui/aqqYrhhgWI72mOm6NMnxyoo+KuOk5q6nZ41OHZsHgasVdovJ+ctnPWzelsgKMAwetJJH020Uwx6ug8gbXv95Qg3zUme8QI0TZFpFkAHxn0xH4tYm/UV//EIeAILPt0wOv0YKukk/VKwK6Qoqqv+XnEFTXNO/LDt2A4VlROmL+MxopCNhd6MXy3OPbdEu7HfS59PxZ/du2NsKejwvewuXW5ws5YpKbtdNz7t0cNzw4F9UCR0tOCJ9scesXd92dkV8JMh08x3w+Tq1R9jZrRTXx87FtO5zOrqSjcenjUHI2uKdw0HrrujLrLQOR37V5QmAFtWCJ1rY3R87T5+fE9dP5payrVrD4revSvAth/Cg7BffSw//7wCe7W0oFbp6SVGVtZxiqd/X3/doBaM+E59kHaTmjqi7r3wCklksAsEDlF4TJr09EvDaQbSGR3+i5DPN8zYtetclI0GJZr+/EzOnk3vf2otxkhO1lO4cDrnlJRURA7YIOtrgps2lYvRo49go63WJq3k5PX0rP727bepI0c66Qz69q0QDz0UCD/vUfLTgseytIZHlyhPo+0U5Xa19Jj61vksWkQv3Q8md0sO/YaF338t0kjUu408aBq4qSV5nC1xnfl49NPSJjqf70F7wIAUY2O4X5x7btfQnXfSTm5PGwkn2YuRaxGmQ45qjamjvX//l0Zt7TEYctOxonOLqKrK0XGPH/8CleBWdvQdtRv2hBnu9gArO/tptGDqYHRcjg5soUwBbEjt5qutXaOnG07YSf+VaWm9vUxqanQD1tdSTsR9l0KNUTCgk92l7rM7ybBHCZ2nwNiypb/v4MGJ9kUX+THSZ0ghaKrpaB2WFT3dwEdRyiKaqmEkYhp0NwTiatguaAMkCUr97FZiYrlWL5WK7ZzB4BqUi+xakePAgdLIRR3Xzp158GkDxiths9FzbKxAjgZTYWdm7tD3qJOk7qXP51sOP8onAc9VgIFNYtm9EOWlzcYDxerVEWFeUrKe0sekCYXyQr/5TZKcP787hVlXX90JGzVzYZinZwMusR9CxBHwzgbYjth+4QpyCe1/KKZLEpt5i8FPoPwZaF/12y8yMg4f1mXBJt9tuISFcKElqR2SIOzdex/ueQ+ExGJc74Vw6ISp/OVGeEEJr1Kt9U2f3mg7xdRyFcql6wfPNzI0a1YK7JU0sGTaO3fmQgNt0GCvy9HYH1r5dTdeK7UPba0zXh+jKXr9Iy1ttbz11or6AWeHT6TCY19mTsBrEV09BDk5F8B+5TQm8nSNqErtEhdf/CBWvCbDT1cUKp4OaoCVaCilnsD66is9JdGhdf6EBg++HsJqqPaWciE2ds5HQyKNLx0/GmVvQGcWaOC0h2g7rmM0pTrZxXWJVbhDVEg7I+OQ22Htw4cTUH6azqSi0SThfBU6icCzN57na6+NRJqfopxaOFGeLT3QyNvjfuPQgWKSYpe3K0ANrC5myfXr9chP+6zQoEsQuaeXgN4SaOywLK3dYKPvx24UIyFhDO4pZPfuO12/ps5kwwzMnv0BNoJereNZVj4kkqdRxqQ9cEBrkHXTYNVzuBvP7NSpP9w22ocWShDa+90w1HVk+wZ5kk2R7msYO6B5LxRLltCUqeGjrMzARlMSipbVp48zpaqqugTpfdjztgUako2wNsjrRp0BBAX4ayfKUA7hs0RfNPIHq6kloWef3Yo20ZeiQEsfShzpcPPRFyf6x5mqO9P1hvKord0NbxZYGLW+xMbON9A4SKU9DxUaMXg6H78TqNBDGNmWoCk86LKk9w2hUj+PKdQ18KPXKmJ7nRtxzx7HroHrYChU63eFQCCA1XHjcTcaKv8x7NWy1OWXz0Ve16E87b0wUtf9/sYbqxuR9o3RB/zcqazrH3WWhw9/jZF1n5md7aycIQw73HuiBf4KZaBXbSIjJQlo01yJjtQeXAYgzBZbtkBnyLsD7lcoKWWtUlJKRYcORSIjY7/MyDhmffLJOKxWkb1Ez4GCgUD96ZZSsX5kEwwfTjcKXxjGfr2yF74sufJK7erx4ov6XH777X8onjv3D3SB5xA506ZRl8XsyBTVMCklmwHgMP5998Cr/p3i2PZB/e5TxUdrJgc/3ECrbPS60CBMZUlbaPBImDJlNTTBg0g8ERE8bjqyZelpXN2E0WmwAZm+dRVEej9WWNvh/cjdVkXFPzBg5KGzRxYF7r33cwiFNj7b/kFMfiSUCwtpAJNY/a2CZq/vaQ0e/LBhmg/ruAsWUPvTg4c5d+6zeK7fw0CejDYmjKqqXPHSSw+hDKbVuXOhOX78h1H5f2FUVi5Ce9YaKPmHUF/QosipoM157c48cGAe+so1AKqntxRBH1IeQpoN+BTMZXRN7dwJaPovyleD8rnaZNORg0FHOjYd64wN9VQHbRtxdveSBiPUmDEX4nS3fvIdO+Zg+f0TM/wydHTHofDwatk87FP5X9gySDNri99RvMRKau1Q/GKOhPvvL4QH/QRsV5OwWkOjLX2zah4M1mvJjdGZRv4/0WIAhFQCRmL6AuXhqJU5itbgISdPfqLhACxJu6o35kfmvfdqoy40vLsxUrbFG/8XixdeMLEz+jnRvn0HdAoSlkexo3xX1HaBeW7edn7+9XBruQJhPtX44Q9fh92JNmLSVyOPmCtW0DQyC1OQUnxh4VE3Xcx548Z3xQUXvA+/DihbDc6ewIqJV/eCtAL3WeqE0a4C88gRHZ7l1TAVFO9Q+yOKDCXLMKvc1CQwIitrrm+dc/iTQ7/Xhvf09E4Q4hKCcR9eaboBUR2tpak0hnETBNb5YvnyT+XSpf+DfWt3gRel6Ih9VgOxaLExzPqD0LBh2RBmt+A5SXuciQ3GBxHHeY8yFFrvfjoIbyPQS9nOwBB9b8vSgx0JK++w7XTEFXLfvl3wo7o8CrPGPlqU8eKEHfhU93tw0i/mQF8hQTkftt730Saz3fqWt9/u2hGXxyRo5gLt8KlmonBwmEBUc67DxLLIDpGvfXfs2CTnzg3WiVHvEhVJ0p8EjT4g2BqfpiCG3sxYXPyrcMdTEBq/Dif1To2+ZuLFaIHDsjLDnYM6s9dA0aBJYNyEXwJWgWaYM2bcBjfZhBy7UNSXKeBH2ggtPxfBeSldQ/btgG1otli3ji61wCcHhDGdmj5oC8DUqWWIRD/Ku+n48YT6/WLX66+TGiWmXFiu3u3zU7m2up8Yu/VPOvWT3Z4V97SfJx7ZNdmadejHkAn4IMTgyzKgXblT0GbvErYjQio6B56VPvjoXjZ4pjTWkCFrwPt87LnKpUi4+Uc4Udvyo25eQz7/hAGqCq9FXYd3CX9HcXAQo4PaJeV4OqOeVulr+lNePgPaltPpk5L+jue+CFs+bjBra5dh0KHpXzHq6Ci03V5uGmPfvhrkWe1en8g5vJnTq+8TyYPTtIxAowKLVpiQle6BLcuyBbG3b78NI5TTiAxjPjrMxhakbnlUw7jcTYR3wra4bvyzh19A6lyLa3rb/1ZsbtyMLxc4HcCNFD5j+nEfnH9AuUfirDUSaARtVdeuyTLqVR68E3geVP2eenR33h6ok9O3e4k9X/oGg5I/1+ePjuapcitda4N5KV9pv8UVFwr4aTcM3nELK53gRP/Qu6hCTIIQ+Sn23c1EnZdAa3oV06spEHhDEPYVFi30dDXqFokQZE59SHkjBJIFrs+44dJZ2aylAdDavr0fBj4LtqgPoa1VQIMbThIZ159h60NkscNNzOfTikCjAguN6GY0oif100j5z3g14p1T+WTUwZVh/FL3IDK+2vYjzeVPhmeUKZvi4X2wHnjfzkliGOehvNogDaPNNuzMNjFKvwKhtBiNdyuEURLSjUbkf3USiBCW99eE3QLCaQfS/zfi/Bx+NMV5EprOSFy/hev1QdtOw7uVwzDtuwe2Fj19pbQYtQuR/wh0nkx8HuYllZ39KryD0A7GYGT/ueFuzKT3476nY1T6en3vZZWO5pbhrxaDk74UFaEke31Nf61dnWjR4q0PWVxMg58+YEd62W7T5n5M0nIhoEgI3QbN9F58Ygd6lLoL106lGgZp6k+gXczWCZVybFSkxZnmHNQZhcce+BoF6qMt9nqtMrCdgQJRz2NowQb5LI2NzFenI4FGBRYaTyY6bE/9ULadecofrlOne9CTulO+MH6/Za5f7wz7Td3IMH6B4H+hKJ6wcuLPQlkdV0bGT2A/+giN9GasPN6M5yCDtxMW+fs4Gvy2yCVcFRWPQtBBXEnKnzryeKQdT3EgrOgUvQpEe3VqsFL0n4gzQgda1o9xpp9TNkzHvvcDRveLUtbZFkaGT6vz9UNcmLxWmdKW/zieh38jdFLyirjGVx94cdxlQdoQNKt7we9jsJ5k5ed/Bu1ullizZqoaOPBnQb+/P/7pQik0r12URg0evAz3iW2n+MKpm1/MOfw1CpmQsML1V4HAJVgdJWH4sevH59OXQGxDiH4On89Co3J8ol/TIR+sIOKTHwpToUB0krpubBysppdO9RHeDOjFMYx/Qf6QJniHNjHxCc+/KYdp4lMDTZrFIF3CX+l07SJR+dFGThjWH0NneC3KWzvD04pfQLNagOd7Gg18EH7OvCoSeT/83kYZ/qRfE6HvYeXn/xTxn8Kz6JUpJzN5EHF+iWX/28GJPt8ba/9TKsETsJG8T7nL2LPHamPWmOtq+okjQedR8tO2S3BQyypo9uUsGJzwjVtWH95twH85PufzPAT+NLSOZ+28vKuwT+pOuXEj2amcOWw4tt7B7qVsxoGNuyoUUtBwyS7mfhViAJxV4pxz1oVtjM1kwsGtmUBYLWnNRTyxsmmD/tatOZia0Vc/JYRHMXY972tJbvhy5vnYHNlFp7HtXWEhVS8LNWJEO9hd6KVVWh0tg7a2KiwA68UlD2yHmIhPKTurjUrdAlvL225ECEAS4s0eJfOQHNpjjxudhbnyO+4QFRMm1EvXs2ePen4Ne7gqasOh34Yv7FIP4xkeR96JmOZV4YN7n0q/fzOE/QYIdBotO+NHHxxUYHTbt1EGzvP0ItC4hnV6PUe90oY/0LYDAfQ7oQOf+f0SCenX5CFXriRj7uImI0UFQrO4IUpT/CYqKH5nMyty8Wf0/cWEXfRpfE5oMQT9a+AxBFO3sTiP1SWKej5I8M3fXyn5zq2JwBkrsFoTZNhsumEZ/g4Y57H8ZYxAp7xGl88wDuC9vC9OpKxtScM6RQfN2t3Z/ynKMu5soPXSynCeuvDCHGizA/Tuecs6D5oXXm3wHUfBivDlUG+BJO6MOeIZSYAF1ndTrb0hrH6tbxWRDNj1YE3zOdtHdBC2UzwfpXk1WbKMN99sMjwS2PxUL1KkSKrv2iU//ZQ2XdLvve/63ny/04cAC6zvoq4MoxbGd/peViqmN3th9d4MG9aT2Gy62r09tDB6ufdi97q5c2X41Rw3Xm3v3sj6JI3obmZ8ZgKtlMAZa3RvpbybLVaLjO7N5iZEz549uY7j4MRRTg8C4T0Hp0dhuZRMgAmc3QRYYJ3d9c9PzwROKwI8XWhl1VVcXEy2qFN28JTwlKHkjFoBAdawWkElcBGYABOIjwALrPg4cSwmwARaAQEWWK2gErgITIAJxEeABVZ8nDgWE2ACrYAAC6xWUAlcBCbABOIjwAIrPk4ciwkwgVZAgAVWK6gELgITYALxEWCBFR8njsUEmEArIMACqxVUAheBCTCB+AiwwIqPE8diAkygFRBggdUKKoGLwASYQHwEWGDFx4ljMQEm0AoIsMBqBZXARWACTCA+Aiyw4uP0ncXCZ5Ob+T9m8RflVOYV/105JhNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMgAkwASbABJgAE2ACTIAJMAEmwASYABNgAkyACTABJsAEmAATYAJMoFUS+H9lQdqS8c3B0QAAAABJRU5ErkJggg=="_s;

    return ""_s;
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

}
