/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "CustomHeaderFields.h"
#include "DOMTokenList.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FrameLoader.h"
#include "HTMLMetaElement.h"
#include "HTMLObjectElement.h"
#include "LayoutUnit.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "UserAgent.h"

namespace WebCore {

static inline OptionSet<AutoplayQuirk> allowedAutoplayQuirks(Document& document)
{
    auto* loader = document.loader();
    if (!loader)
        return { };

    return loader->allowedAutoplayQuirks();
}

Quirks::Quirks(Document& document)
    : m_document(makeWeakPtr(document))
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
    return equalLettersIgnoringASCIICase(host, "ceac.state.gov") || host.endsWithIgnoringASCIICase(".ceac.state.gov");
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
    return equalLettersIgnoringASCIICase(host, "netflix.com") || host.endsWithIgnoringASCIICase(".netflix.com");
}

bool Quirks::needsPerDocumentAutoplayBehavior() const
{
#if PLATFORM(MAC)
    ASSERT(m_document == &m_document->topDocument());
    return needsQuirks() && allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::PerDocumentAutoplayBehavior);
#else
    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "netflix.com") || host.endsWithIgnoringASCIICase(".netflix.com");
#endif
}

bool Quirks::shouldAutoplayForArbitraryUserGesture() const
{
#if PLATFORM(MAC)
    return needsQuirks() && allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::ArbitraryUserGestures);
#else
    return false;
#endif
}

bool Quirks::hasBrokenEncryptedMediaAPISupportQuirk() const
{
    if (!needsQuirks())
        return false;

    if (m_hasBrokenEncryptedMediaAPISupportQuirk)
        return m_hasBrokenEncryptedMediaAPISupportQuirk.value();

    auto domain = m_document->securityOrigin().domain().convertToASCIILowercase();

    m_hasBrokenEncryptedMediaAPISupportQuirk = domain == "starz.com"
        || domain.endsWith(".starz.com")
        || domain == "youtube.com"
        || domain.endsWith(".youtube.com")
        || domain == "hulu.com"
        || domain.endsWith("hulu.com");

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
    return host.endsWith(".youtube.com") || host == "youtube.com";
}

bool Quirks::shouldStripQuotationMarkInFontFaceSetFamily() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "docs.google.com");
}

bool Quirks::isTouchBarUpdateSupressedForHiddenContentEditable() const
{
#if PLATFORM(MAC)
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "docs.google.com");
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

    if (equalLettersIgnoringASCIICase(host, "twitter.com"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "onedrive.live.com"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "trix-editor.org"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "www.icloud.com")) {
        auto path = url.path();
        if (path.contains("notes") || url.fragmentIdentifier().contains("notes"))
            return true;
    }
#endif

    return false;
}

static bool shouldSuppressAutocorrectionAndAutocaptializationInHiddenEditableAreasForHost(const StringView& host)
{
#if PLATFORM(IOS_FAMILY)
    return equalLettersIgnoringASCIICase(host, "docs.google.com");
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
    if (equalLettersIgnoringASCIICase(host, "medium.com") || host.endsWithIgnoringASCIICase(".medium.com"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "weebly.com") || host.endsWithIgnoringASCIICase(".weebly.com"))
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

    return equalLettersIgnoringASCIICase(m_document->url().host(), "www.youtube.com");
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
    return equalLettersIgnoringASCIICase(url.host(), "mail.google.com");
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
    return topPrivatelyControlledDomain(m_document->topDocument().url().host().toString()).startsWith("amazon.");
}

bool Quirks::isGoogleMaps() const
{
    auto& url = m_document->topDocument().url();
    return topPrivatelyControlledDomain(url.host().toString()).startsWith("google.") && url.path().startsWithIgnoringASCIICase("/maps/");
}

bool Quirks::shouldDispatchSimulatedMouseEvents() const
{
    if (RuntimeEnabledFeatures::sharedFeatures().mouseEventsSimulationEnabled())
        return true;

    if (!needsQuirks())
        return false;

    auto* loader = m_document->loader();
    if (!loader || loader->simulatedMouseEventsDispatchPolicy() != SimulatedMouseEventsDispatchPolicy::Allow)
        return false;

    if (isAmazon())
        return true;
    if (isGoogleMaps())
        return true;

    auto& url = m_document->topDocument().url();
    auto host = url.host();

    if (equalLettersIgnoringASCIICase(host, "wix.com") || host.endsWithIgnoringASCIICase(".wix.com")) {
        // Disable simulated mouse dispatching for template selection.
        return !url.path().startsWithIgnoringASCIICase("/website/templates/");
    }
    if ((equalLettersIgnoringASCIICase(host, "desmos.com") || host.endsWithIgnoringASCIICase(".desmos.com")) && url.path().startsWithIgnoringASCIICase("/calculator/"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "figma.com") || host.endsWithIgnoringASCIICase(".figma.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "trello.com") || host.endsWithIgnoringASCIICase(".trello.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "airtable.com") || host.endsWithIgnoringASCIICase(".airtable.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "msn.com") || host.endsWithIgnoringASCIICase(".msn.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "flipkart.com") || host.endsWithIgnoringASCIICase(".flipkart.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "iqiyi.com") || host.endsWithIgnoringASCIICase(".iqiyi.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "trailers.apple.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "soundcloud.com"))
        return true;
    if (equalLettersIgnoringASCIICase(host, "naver.com"))
        return true;
    if (host.endsWithIgnoringASCIICase(".naver.com")) {
        // Disable the quirk for tv.naver.com subdomain to be able to simulate hover on videos.
        if (equalLettersIgnoringASCIICase(host, "tv.naver.com"))
            return false;
        // Disable the quirk for mail.naver.com subdomain to be able to tap on mail subjects.
        if (equalLettersIgnoringASCIICase(host, "mail.naver.com"))
            return false;
        // Disable the quirk on the mobile site.
        // FIXME: Maybe this quirk should be disabled for "m." subdomains on all sites? These are generally mobile sites that don't need mouse events.
        if (equalLettersIgnoringASCIICase(host, "m.naver.com"))
            return false;
        return true;
    }
    return false;
}

bool Quirks::shouldDispatchedSimulatedMouseEventsAssumeDefaultPrevented(EventTarget* target) const
{
    if (!needsQuirks() || !shouldDispatchSimulatedMouseEvents())
        return false;

    if (isAmazon() && is<Element>(target)) {
        // When panning on an Amazon product image, we're either touching on the #magnifierLens element
        // or its previous sibling.
        auto& element = downcast<Element>(*target);
        if (element.getIdAttribute() == "magnifierLens")
            return true;
        if (auto* sibling = element.nextElementSibling())
            return sibling->getIdAttribute() == "magnifierLens";
    }

    if (equalLettersIgnoringASCIICase(m_document->topDocument().url().host(), "soundcloud.com") && is<Element>(target))
        return downcast<Element>(*target).classList().contains("sceneLayer");

    return false;
}

Optional<Event::IsCancelable> Quirks::simulatedMouseEventTypeForTarget(EventTarget* target) const
{
    if (!shouldDispatchSimulatedMouseEvents())
        return { };

    // On Google Maps, we want to limit simulated mouse events to dragging the little man that allows entering into Street View.
    if (isGoogleMaps()) {
        if (is<Element>(target) && downcast<Element>(target)->getAttribute("class") == "widget-expand-button-pegman-icon")
            return Event::IsCancelable::Yes;
        return { };
    }

    auto host = m_document->topDocument().url().host();
    if (equalLettersIgnoringASCIICase(host, "desmos.com") || host.endsWithIgnoringASCIICase(".desmos.com"))
        return Event::IsCancelable::No;

    return Event::IsCancelable::Yes;
}

bool Quirks::shouldMakeTouchEventNonCancelableForTarget(EventTarget* target) const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();

    if (equalLettersIgnoringASCIICase(host, "www.youtube.com")) {
        if (is<Element>(target)) {
            unsigned depth = 3;
            for (auto* element = downcast<Element>(target); element && depth; element = element->parentElement(), --depth) {
                if (element->localName() == "paper-item" && element->classList().contains("yt-dropdown-menu"))
                    return true;
            }
        }
    }

    return false;
}
#endif

bool Quirks::shouldAvoidResizingWhenInputViewBoundsChange() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    if (equalLettersIgnoringASCIICase(host, "live.com") || host.endsWithIgnoringASCIICase(".live.com"))
        return true;

    if (host.endsWithIgnoringASCIICase(".sharepoint.com"))
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
    if (equalLettersIgnoringASCIICase(host, "mailchimp.com") || host.endsWithIgnoringASCIICase(".mailchimp.com"))
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
    return equalLettersIgnoringASCIICase(url.host(), "docs.google.com") && url.path().startsWithIgnoringASCIICase("/spreadsheets/");
#else
    return false;
#endif
}

bool Quirks::shouldLightenJapaneseBoldSansSerif() const
{
#if USE(HIRAGINO_SANS_WORKAROUND)
    if (!needsQuirks())
        return false;

    // lang="ja" style="font: bold sans-serif;" content would naturally get HiraginoSans-W8 here, but that's visually
    // too bold. Instead, we should pick HiraginoSans-W6 instead.
    // FIXME: webkit.org/b/200047 Remove this quirk.
    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "m.yahoo.co.jp");
#else
    return false;
#endif
}

bool Quirks::shouldIgnoreContentChange(const Element& element) const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    auto* parentElement = element.parentElement();
    if (!parentElement || !parentElement->hasClass())
        return false;

    DOMTokenList& classList = parentElement->classList();
    if (!classList.contains("feedback") || !classList.contains("feedback-mid"))
        return false;

    if (!equalLettersIgnoringASCIICase(topPrivatelyControlledDomain(m_document->url().host().toString()), "united.com"))
        return false;

    return true;
#else
    UNUSED_PARAM(element);
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
        if (!equalLettersIgnoringASCIICase(m_document->url().host(), "calendar.google.com"))
            return false;
        static NeverDestroyed<QualifiedName> dataInitialValueAttr(nullAtom(), "data-initial-value", nullAtom());
        static NeverDestroyed<QualifiedName> dataPreviousValueAttr(nullAtom(), "data-previous-value", nullAtom());

        return equalLettersIgnoringASCIICase(element.attributeWithoutSynchronization(HTMLNames::autocompleteAttr), "off")
            && element.hasAttributeWithoutSynchronization(dataInitialValueAttr)
            && element.hasAttributeWithoutSynchronization(dataPreviousValueAttr);
    }

    if (!element.hasTagName(HTMLNames::textareaTag))
        return false;

    auto& url = m_document->url();
    auto host = url.host();
    if (!host.endsWithIgnoringASCIICase(".desmos.com"))
        return false;

    return element.parentElement() && element.parentElement()->classNames().contains("dcg-mq-textarea");
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
        m_needsGMailOverflowScrollQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "mail.google.com");

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
        m_needsYouTubeOverflowScrollQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "www.youtube.com");

    return *m_needsYouTubeOverflowScrollQuirk;
#else
    return false;
#endif
}

bool Quirks::shouldAvoidScrollingWhenFocusedContentIsVisible() const
{
    if (!needsQuirks())
        return false;

    return equalLettersIgnoringASCIICase(m_document->url().host(), "www.zillow.com");
}

bool Quirks::shouldOpenAsAboutBlank(const String& stringToOpen) const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    auto openerURL = m_document->url();
    if (!equalLettersIgnoringASCIICase(openerURL.host(), "docs.google.com"))
        return false;

    if (!m_document->frame() || !m_document->frame()->loader().userAgentForJavaScript(openerURL).contains("Macintosh"))
        return false;

    URL urlToOpen { URL { }, stringToOpen };
    if (!urlToOpen.protocolIsAbout())
        return false;

    return !equalLettersIgnoringASCIICase(urlToOpen.host(), "blank") && !equalLettersIgnoringASCIICase(urlToOpen.host(), "srcdoc");
#else
    UNUSED_PARAM(stringToOpen);
    return false;
#endif
}

}
