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
#include "DOMWindow.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentStorageAccess.h"
#include "ElementInlines.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "HTMLBodyElement.h"
#include "HTMLDivElement.h"
#include "HTMLMetaElement.h"
#include "HTMLObjectElement.h"
#include "HTMLVideoElement.h"
#include "JSEventListener.h"
#include "LayoutUnit.h"
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

bool Quirks::needsSeekingSupportDisabled() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "netflix.com"_s) || host.endsWithIgnoringASCIICase(".netflix.com"_s);
}

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

bool Quirks::shouldAutoplayForArbitraryUserGesture() const
{
#if PLATFORM(MAC)
    return needsQuirks() && allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::ArbitraryUserGestures);
#else
    if (!needsQuirks())
        return false;

    auto domain = RegistrableDomain { m_document->topDocument().url() };
    return domain == "twitter.com"_s || domain == "facebook.com"_s;
#endif
}

bool Quirks::shouldAutoplayWebAudioForArbitraryUserGesture() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "www.bing.com"_s) || host.endsWithIgnoringASCIICase(".zoom.us"_s);
}

bool Quirks::hasBrokenEncryptedMediaAPISupportQuirk() const
{
#if ENABLE(THUNDER)
    return false;
#endif

    if (!needsQuirks())
        return false;

    if (m_hasBrokenEncryptedMediaAPISupportQuirk)
        return m_hasBrokenEncryptedMediaAPISupportQuirk.value();

    auto domain = RegistrableDomain(m_document->url()).string();
    m_hasBrokenEncryptedMediaAPISupportQuirk = domain == "starz.com"_s || domain == "youtube.com"_s || domain == "hulu.com"_s;

    return m_hasBrokenEncryptedMediaAPISupportQuirk.value();
}

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

bool Quirks::shouldTooltipPreventFromProceedingWithClick(const Element& element) const
{
    if (!needsQuirks())
        return false;

    if (!equalLettersIgnoringASCIICase(m_document->topDocument().url().host(), "covid.cdc.gov"_s))
        return false;
    return element.hasClass() && element.classNames().contains("tooltip"_s);
}

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

bool Quirks::shouldDisableResolutionMediaQuery() const
{
    if (!needsQuirks())
        return false;
    auto host = m_document->url().host();

    if (equalLettersIgnoringASCIICase(host, "www.carrentals.com"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.cheaptickets.com"_s))
        return true;

#if ENABLE(PUBLIC_SUFFIX_LIST)
    if (topPrivatelyControlledDomain(host.toString()).startsWith("ebookers."_s))
        return true;

    if (topPrivatelyControlledDomain(host.toString()).startsWith("expedia."_s))
        return true;
#endif

    if (host.endsWithIgnoringASCIICase(".hoteis.com"_s))
        return true;

    if (host.endsWithIgnoringASCIICase(".hoteles.com"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.hotels.cn"_s))
        return true;

    if (host.endsWithIgnoringASCIICase(".hotels.com"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.mrjet.se"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.orbitz.com"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.travelocity.ca"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.travelocity.com"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.wotif.com"_s))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.wotif.co.nz"_s))
        return true;

    return false;
}

bool Quirks::needsMillisecondResolutionForHighResTimeStamp() const
{
    if (!needsQuirks())
        return false;
    // webkit.org/b/210527
    auto host = m_document->url().host();
    return equalLettersIgnoringASCIICase(host, "www.icourse163.org"_s);
}

bool Quirks::shouldStripQuotationMarkInFontFaceSetFamily() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "docs.google.com"_s);
}

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

static bool shouldSuppressAutocorrectionAndAutocaptializationInHiddenEditableAreasForHost(StringView host)
{
#if PLATFORM(IOS_FAMILY)
    return equalLettersIgnoringASCIICase(host, "docs.google.com"_s);
#else
    UNUSED_PARAM(host);
    return false;
#endif
}

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

bool Quirks::shouldSuppressAutocorrectionAndAutocaptializationInHiddenEditableAreas() const
{
    if (!needsQuirks())
        return false;

    return shouldSuppressAutocorrectionAndAutocaptializationInHiddenEditableAreasForHost(m_document->topDocument().url().host());
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

bool Quirks::shouldDispatchSimulatedMouseEvents(const EventTarget* target) const
{
    if (DeprecatedGlobalSettings::mouseEventsSimulationEnabled())
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

        if ((host == "desmos.com"_s || host.endsWith(".desmos.com"_s)) && startsWithLettersIgnoringASCIICase(url.path(), "/calculator/"_s))
            return ShouldDispatchSimulatedMouseEvents::Yes;
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
        if (host == "nba.com"_s || host.endsWith(".nba.com"_s))
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

bool Quirks::shouldPreventPointerMediaQueryFromEvaluatingToCoarse() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "shutterstock.com"_s) || host.endsWithIgnoringASCIICase(".shutterstock.com"_s);
}

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
bool Quirks::shouldSynthesizeTouchEvents() const
{
    if (!needsQuirks())
        return false;

    if (!m_shouldSynthesizeTouchEventsQuirk)
        m_shouldSynthesizeTouchEventsQuirk = isYahooMail(*m_document);
    return m_shouldSynthesizeTouchEventsQuirk.value();
}
#endif

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

// FIXME(<rdar://problem/50394969>): Remove after desmos.com adopts inputmode="none".
bool Quirks::needsInputModeNoneImplicitly(const HTMLElement& element) const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (element.hasTagName(HTMLNames::inputTag)) {
        if (!equalLettersIgnoringASCIICase(m_document->url().host(), "calendar.google.com"_s))
            return false;
        static NeverDestroyed<QualifiedName> dataInitialValueAttr(nullAtom(), "data-initial-value"_s, nullAtom());
        static NeverDestroyed<QualifiedName> dataPreviousValueAttr(nullAtom(), "data-previous-value"_s, nullAtom());

        return equalLettersIgnoringASCIICase(element.attributeWithoutSynchronization(HTMLNames::autocompleteAttr), "off"_s)
            && element.hasAttributeWithoutSynchronization(dataInitialValueAttr)
            && element.hasAttributeWithoutSynchronization(dataPreviousValueAttr);
    }

    if (!element.hasTagName(HTMLNames::textareaTag))
        return false;

    auto& url = m_document->url();
    auto host = url.host();
    if (!host.endsWithIgnoringASCIICase(".desmos.com"_s))
        return false;

    return element.parentElement() && element.parentElement()->classNames().contains("dcg-mq-textarea"_s);
#else
    UNUSED_PARAM(element);
    return false;
#endif
}

// FIXME: Remove after the site is fixed, <rdar://problem/50374200>
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

// FIXME: Remove after the site is fixed, <rdar://problem/74377902>
bool Quirks::needsWeChatScrollingQuirk() const
{
#if PLATFORM(IOS)
    return needsQuirks() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoWeChatScrollingQuirk) && IOSApplication::isWechat();
#else
    return false;
#endif
}

bool Quirks::shouldOmitHTMLDocumentSupportedPropertyNames()
{
#if PLATFORM(COCOA)
    static bool shouldOmitHTMLDocumentSupportedPropertyNames = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::HTMLDocumentSupportedPropertyNames);
    return shouldOmitHTMLDocumentSupportedPropertyNames;
#else
    return false;
#endif
}

bool Quirks::shouldSilenceWindowResizeEvents() const
{
#if PLATFORM(IOS)
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
        || equalLettersIgnoringASCIICase(host, "twitter.com"_s) || host.endsWithIgnoringASCIICase(".twitter.com"_s);
#else
    return false;
#endif
}

bool Quirks::shouldSilenceMediaQueryListChangeEvents() const
{
#if PLATFORM(IOS)
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

bool Quirks::shouldAvoidScrollingWhenFocusedContentIsVisible() const
{
    if (!needsQuirks())
        return false;

    return equalLettersIgnoringASCIICase(m_document->url().host(), "www.zillow.com"_s);
}

bool Quirks::shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return equalLettersIgnoringASCIICase(host, "att.com"_s) || host.endsWithIgnoringASCIICase(".att.com"_s);
}

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

bool Quirks::shouldMakeEventListenerPassive(const EventTarget& eventTarget, const AtomString& eventType, const EventListener& eventListener)
{
    auto eventTargetIsRoot = [](const EventTarget& eventTarget) {
        if (is<DOMWindow>(eventTarget))
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
        if (is<DOMWindow>(eventTarget)) {
            auto* document = downcast<DOMWindow>(eventTarget).document();
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

bool Quirks::shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraints() const
{
    if (!needsQuirks())
        return false;

    // FIXME: We should consider replacing this with a heuristic to determine whether
    // or not the edges of the page mostly lack content after shrinking to fit.
    return m_document->url().host().endsWithIgnoringASCIICase(".wikipedia.org"_s);
}

bool Quirks::shouldIgnoreContentObservationForSyntheticClick(bool isFirstSyntheticClickOnPage) const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return isFirstSyntheticClickOnPage && (equalLettersIgnoringASCIICase(host, "shutterstock.com"_s) || host.endsWithIgnoringASCIICase(".shutterstock.com"_s));
}

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

bool Quirks::isMicrosoftTeamsRedirectURL(const URL& url)
{
    return url.host() == "teams.microsoft.com"_s && url.query().toString().contains("Retried+3+times+without+success"_s);
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

bool Quirks::hasStorageAccessForAllLoginDomains(const HashSet<RegistrableDomain>& loginDomains, const RegistrableDomain& topFrameDomain)
{
    for (auto& loginDomain : loginDomains) {
        if (!ResourceLoadObserver::shared().hasCrossPageStorageAccess(loginDomain, topFrameDomain))
            return false;
    }
    return true;
}

const String& Quirks::BBCRadioPlayerURLString()
{
    static NeverDestroyed<String> BBCRadioPlayerURLString = "https://www.bbc.co.uk/sounds/player/bbc_world_service"_s;
    return BBCRadioPlayerURLString;
}

const String& Quirks::staticRadioPlayerURLString()
{
    static NeverDestroyed<String> staticRadioPlayerURLString = "https://static.radioplayer.co.uk/"_s;
    return staticRadioPlayerURLString;
}

static bool isBBCDomain(const RegistrableDomain& domain)
{
    static NeverDestroyed<RegistrableDomain> BBCDomain = RegistrableDomain(URL { Quirks::BBCRadioPlayerURLString() });
    return domain == BBCDomain;
}

static bool isBBCPopUpPlayerElement(const Element& element)
{
    auto* parentElement = element.parentElement();
    if (!element.parentElement() || !element.parentElement()->hasClass() || !parentElement->parentElement() || !parentElement->parentElement()->hasClass())
        return false;

    return element.parentElement()->classNames().contains("p_audioButton_buttonInner"_s) && parentElement->parentElement()->classNames().contains("hidden"_s);
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

Quirks::StorageAccessResult Quirks::triggerOptionalStorageAccessQuirk(Element& element, const PlatformMouseEvent& platformEvent, const AtomString& eventType, int detail, Element* relatedTarget, bool isParentProcessAFullWebBrowser, IsSyntheticClick isSyntheticClick) const
{
    if (!DeprecatedGlobalSettings::resourceLoadStatisticsEnabled() || !isParentProcessAFullWebBrowser)
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
            if (abstractFrame && is<Frame>(*abstractFrame)) {
                auto& frame = downcast<Frame>(*abstractFrame);
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

        static NeverDestroyed<String> BBCRadioPlayerPopUpWindowFeatureString = "featurestring width=400,height=730"_s;
        static NeverDestroyed<UserScript> BBCUserScript { "function triggerRedirect() { document.location.href = \"https://www.bbc.co.uk/sounds/player/bbc_world_service\"; } window.addEventListener('load', function () { triggerRedirect(); })"_s, URL(aboutBlankURL()), Vector<String>(), Vector<String>(), UserScriptInjectionTime::DocumentEnd, UserContentInjectedFrames::InjectInTopFrameOnly, WaitForNotificationBeforeInjecting::Yes };

        // BBC RadioPlayer case.
        if (isBBCDomain(domain) && isBBCPopUpPlayerElement(element)) {
            return requestStorageAccessAndHandleClick([document = m_document] (ShouldDispatchClick shouldDispatchClick) mutable {
                if (!document || shouldDispatchClick == ShouldDispatchClick::No)
                    return;

                auto domWindow = document->domWindow();
                if (domWindow) {
                    ExceptionOr<RefPtr<WindowProxy>> proxyOrException = domWindow->open(*domWindow, *domWindow, staticRadioPlayerURLString(), emptyAtom(), BBCRadioPlayerPopUpWindowFeatureString);
                    if (proxyOrException.hasException())
                        return;
                    auto proxy = proxyOrException.releaseReturnValue();
                    auto* abstractFrame = proxy->frame();
                    if (is<Frame>(abstractFrame)) {
                        auto* frame = downcast<Frame>(abstractFrame);
                        auto world = ScriptController::createWorld("bbcRadioPlayerWorld"_s, ScriptController::WorldType::User);
                        frame->addUserScriptAwaitingNotification(world.get(), BBCUserScript);
                        return;
                    }
                }
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

bool Quirks::needsVP9FullRangeFlagQuirk() const
{
    if (!needsQuirks())
        return false;

    if (!m_needsVP9FullRangeFlagQuirk)
        m_needsVP9FullRangeFlagQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "www.youtube.com"_s);

    return *m_needsVP9FullRangeFlagQuirk;
}

bool Quirks::needsHDRPixelDepthQuirk() const
{
    if (!needsQuirks())
        return false;

    if (!m_needsHDRPixelDepthQuirk)
        m_needsHDRPixelDepthQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "www.youtube.com"_s);

    return *m_needsHDRPixelDepthQuirk;
}

// FIXME: remove this once rdar://66739450 has been fixed.
bool Quirks::needsAkamaiMediaPlayerQuirk(const HTMLVideoElement& element) const
{
#if PLATFORM(IOS_FAMILY)
    // Akamai Media Player begins polling `webkitDisplayingFullscreen` every 100ms immediately after calling
    // `webkitEnterFullscreen` and exits fullscreen as soon as it returns false. r262456 changed the HTMLMediaPlayer state
    // machine so `webkitDisplayingFullscreen` doesn't return true until the fullscreen window has been opened in the
    // UI process, which causes Akamai Media Player to frequently exit fullscreen mode immediately.

    static NeverDestroyed<const AtomString> akamaiHTML5(MAKE_STATIC_STRING_IMPL("akamai-html5"));
    static NeverDestroyed<const AtomString> akamaiMediaElement(MAKE_STATIC_STRING_IMPL("akamai-media-element"));
    static NeverDestroyed<const AtomString> ampHTML5(MAKE_STATIC_STRING_IMPL("amp-html5"));
    static NeverDestroyed<const AtomString> ampMediaElement(MAKE_STATIC_STRING_IMPL("amp-media-element"));

    if (!needsQuirks())
        return false;

    if (!element.hasClass())
        return false;

    auto& classNames = element.classNames();
    return (classNames.contains(akamaiHTML5) && classNames.contains(akamaiMediaElement)) || (classNames.contains(ampHTML5) && classNames.contains(ampMediaElement));
#else
    UNUSED_PARAM(element);
    return false;
#endif
}

// FIXME: remove this once rdar://92531240 has been fixed.
bool Quirks::needsFlightAwareSerializationQuirk() const
{
    if (!needsQuirks())
        return false;

    if (!m_needsFlightAwareSerializationQuirk)
        m_needsFlightAwareSerializationQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "flightaware.com"_s);

    return *m_needsFlightAwareSerializationQuirk;
}

bool Quirks::needsBlackFullscreenBackgroundQuirk() const
{
    // MLB.com sets a black background-color on the :backdrop pseudo element, which WebKit does not yet support. This
    // quirk can be removed once support for :backdrop psedue element is added.
    if (!needsQuirks())
        return false;

    if (!m_needsBlackFullscreenBackgroundQuirk) {
        auto host = m_document->topDocument().url().host();
        m_needsBlackFullscreenBackgroundQuirk = equalLettersIgnoringASCIICase(host, "mlb.com"_s) || host.endsWithIgnoringASCIICase(".mlb.com"_s);
    }

    return *m_needsBlackFullscreenBackgroundQuirk;
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

bool Quirks::shouldAllowNavigationToCustomProtocolWithoutUserGesture(StringView protocol, const SecurityOriginData& requesterOrigin)
{
    return protocol == "msteams"_s && (requesterOrigin.host == "teams.live.com"_s || requesterOrigin.host == "teams.microsoft.com"_s);
}

#if ENABLE(IMAGE_ANALYSIS)

bool Quirks::needsToForceUserSelectAndUserDragWhenInstallingImageOverlay() const
{
    if (!needsQuirks())
        return false;

    auto& url = m_document->topDocument().url();
    if (topPrivatelyControlledDomain(url.host().toString()).startsWith("google."_s) && url.path() == "/search"_s)
        return true;

    auto host = url.host();
    if (equalLettersIgnoringASCIICase(host, "youtube.com"_s) || host.endsWithIgnoringASCIICase(".youtube.com"_s))
        return true;

    return false;
}

#endif // ENABLE(IMAGE_ANALYSIS)

#if PLATFORM(IOS)
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

bool Quirks::shouldEnableFontLoadingAPIQuirk() const
{
    if (!needsQuirks() || m_document->settings().downloadableBinaryFontsEnabled())
        return false;

    if (!m_shouldEnableFontLoadingAPIQuirk)
        m_shouldEnableFontLoadingAPIQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "play.hbomax.com"_s);

    return m_shouldEnableFontLoadingAPIQuirk.value();
}

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

}
