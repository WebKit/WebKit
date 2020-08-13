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

#include "config.h"
#include "Quirks.h"

#include "Attr.h"
#include "DOMTokenList.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentStorageAccess.h"
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
#include "ResourceLoadObserver.h"
#include "RuntimeEnabledFeatures.h"
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

namespace WebCore {

static inline OptionSet<AutoplayQuirk> allowedAutoplayQuirks(Document& document)
{
    auto* loader = document.loader();
    if (!loader)
        return { };

    return loader->allowedAutoplayQuirks();
}

#if ENABLE(PUBLIC_SUFFIX_LIST)
static inline bool isYahooMail(Document& document)
{
    auto host = document.topDocument().url().host();
    return startsWithLettersIgnoringASCIICase(host, "mail.") && topPrivatelyControlledDomain(host.toString()).startsWith("yahoo.");
}
#endif

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
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "netflix.com") || host.endsWithIgnoringASCIICase(".netflix.com");
#endif
}

bool Quirks::shouldAutoplayForArbitraryUserGesture() const
{
#if PLATFORM(MAC)
    return needsQuirks() && allowedAutoplayQuirks(*m_document).contains(AutoplayQuirk::ArbitraryUserGestures);
#else
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return equalLettersIgnoringASCIICase(host, "twitter.com") || host.endsWithIgnoringASCIICase(".twitter.com");
#endif
}

bool Quirks::shouldAutoplayWebAudioForArbitraryUserGesture() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "www.bing.com");
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

bool Quirks::needsMillisecondResolutionForHighResTimeStamp() const
{
    if (!needsQuirks())
        return false;
    // webkit.org/b/210527
    auto host = m_document->url().host();
    return equalLettersIgnoringASCIICase(host, "www.icourse163.org");
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

    auto doShouldDispatchChecks = [this] () -> bool {
        auto* loader = m_document->loader();
        if (!loader || loader->simulatedMouseEventsDispatchPolicy() != SimulatedMouseEventsDispatchPolicy::Allow)
            return false;

        if (isAmazon())
            return true;
        if (isGoogleMaps())
            return true;

        auto& url = m_document->topDocument().url();
        auto host = url.host().convertToASCIILowercase();

        if (host == "wix.com" || host.endsWith(".wix.com")) {
            // Disable simulated mouse dispatching for template selection.
            return !url.path().startsWithIgnoringASCIICase("/website/templates/");
        }

        if ((host == "desmos.com" || host.endsWith(".desmos.com")) && url.path().startsWithIgnoringASCIICase("/calculator/"))
            return true;
        if (host == "figma.com" || host.endsWith(".figma.com"))
            return true;
        if (host == "trello.com" || host.endsWith(".trello.com"))
            return true;
        if (host == "airtable.com" || host.endsWith(".airtable.com"))
            return true;
        if (host == "msn.com" || host.endsWith(".msn.com"))
            return true;
        if (host == "flipkart.com" || host.endsWith(".flipkart.com"))
            return true;
        if (host == "iqiyi.com" || host.endsWith(".iqiyi.com"))
            return true;
        if (host == "trailers.apple.com")
            return true;
        if (host == "soundcloud.com")
            return true;
        if (host == "naver.com")
            return true;
        if (host == "nhl.com" || (host.endsWith(".nhl.com") && !host.startsWith("account.")))
            return true;
        if (host == "nba.com" || host.endsWith(".nba.com"))
            return true;
        if (host.endsWith(".naver.com")) {
            // Disable the quirk for tv.naver.com subdomain to be able to simulate hover on videos.
            if (host == "tv.naver.com")
                return false;
            // Disable the quirk for mail.naver.com subdomain to be able to tap on mail subjects.
            if (host == "mail.naver.com")
                return false;
            // Disable the quirk on the mobile site.
            // FIXME: Maybe this quirk should be disabled for "m." subdomains on all sites? These are generally mobile sites that don't need mouse events.
            if (host == "m.naver.com")
                return false;
            return true;
        }
        return false;
    };

    if (!m_shouldDispatchSimulatedMouseEventsQuirk)
        m_shouldDispatchSimulatedMouseEventsQuirk = doShouldDispatchChecks();
    return *m_shouldDispatchSimulatedMouseEventsQuirk;
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

    if (equalLettersIgnoringASCIICase(host, "airtable.com") || host.endsWithIgnoringASCIICase(".airtable.com")) {
        // We want to limit simulated mouse events to elements under <div id="paneContainer"> to allow for column re-ordering and multiple cell selection.
        if (is<Node>(target)) {
            auto* node = downcast<Node>(target);
            if (auto* paneContainer = node->treeScope().getElementById(AtomString("paneContainer"))) {
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

bool Quirks::shouldPreventPointerMediaQueryFromEvaluatingToCoarse() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->topDocument().url().host();
    return equalLettersIgnoringASCIICase(host, "shutterstock.com") || host.endsWithIgnoringASCIICase(".shutterstock.com");
}

bool Quirks::shouldPreventDispatchOfTouchEvent(const AtomString& touchEventType, EventTarget* target) const
{
    if (!needsQuirks())
        return false;

    if (is<Element>(target) && touchEventType == eventNames().touchendEvent && equalLettersIgnoringASCIICase(m_document->topDocument().url().host(), "sites.google.com")) {
        auto& classList = downcast<Element>(*target).classList();
        return classList.contains("DPvwYc") && classList.contains("sm8sCf");
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

    if (equalLettersIgnoringASCIICase(host, "live.com") || host.endsWithIgnoringASCIICase(".live.com"))
        return true;

    if (equalLettersIgnoringASCIICase(host, "twitter.com") || host.endsWithIgnoringASCIICase(".twitter.com"))
        return true;

    if ((equalLettersIgnoringASCIICase(host, "google.com") || host.endsWithIgnoringASCIICase(".google.com")) && url.path().startsWithIgnoringASCIICase("/maps/"))
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

bool Quirks::needsFullscreenDisplayNoneQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (!m_needsFullscreenDisplayNoneQuirk) {
        auto host = m_document->topDocument().url().host();
        m_needsFullscreenDisplayNoneQuirk = equalLettersIgnoringASCIICase(host, "gizmodo.com") || host.endsWithIgnoringASCIICase(".gizmodo.com");
    }

    return *m_needsFullscreenDisplayNoneQuirk;
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

bool Quirks::shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation() const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return equalLettersIgnoringASCIICase(host, "att.com") || host.endsWithIgnoringASCIICase(".att.com");
}

bool Quirks::shouldIgnoreAriaForFastPathContentObservationCheck() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return equalLettersIgnoringASCIICase(host, "www.ralphlauren.com");
#endif
    return false;
}

bool Quirks::shouldOpenAsAboutBlank(const String& stringToOpen) const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    auto openerURL = m_document->url();
    if (!equalLettersIgnoringASCIICase(openerURL.host(), "docs.google.com"))
        return false;

    if (!m_document->frame() || !m_document->frame()->loader().userAgent(openerURL).contains("Macintosh"))
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

bool Quirks::needsPreloadAutoQuirk() const
{
#if PLATFORM(IOS_FAMILY)
    if (!needsQuirks())
        return false;

    if (m_needsPreloadAutoQuirk)
        return m_needsPreloadAutoQuirk.value();

    auto domain = m_document->securityOrigin().domain().convertToASCIILowercase();

    m_needsPreloadAutoQuirk = domain == "vimeo.com" || domain.endsWith("vimeo.com");

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
    if (topURL.protocolIs("https") && equalLettersIgnoringASCIICase(host, "vimeo.com")) {
        if (auto* documentLoader = m_document->frame() ? m_document->frame()->loader().documentLoader() : nullptr)
            return documentLoader->response().cacheControlContainsNoStore();
    }

    // Google Docs used to bypass the back/forward cache by serving "Cache-Control: no-store" over HTTPS.
    // We started caching such content in r250437 but the Google Docs index page unfortunately is not currently compatible
    // because it puts an overlay (with class "docs-homescreen-freeze-el-full") over the page when navigating away and fails
    // to remove it when coming back from the back/forward cache (e.g. in 'pageshow' event handler). See <rdar://problem/57670064>.
    // Note that this does not check for docs.google.com host because of hosted G Suite apps.
    static MainThreadNeverDestroyed<const AtomString> googleDocsOverlayDivClass("docs-homescreen-freeze-el-full", AtomString::ConstructFromLiteral);
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
        m_shouldBypassAsyncScriptDeferring = (domain == "bungalow.com");
    }
    return *m_shouldBypassAsyncScriptDeferring;
}

bool Quirks::shouldMakeEventListenerPassive(const EventTarget& eventTarget, const AtomString& eventType, const EventListener& eventListener)
{
    if (eventNames().isTouchScrollBlockingEventType(eventType)) {
        if (is<DOMWindow>(eventTarget)) {
            auto& window = downcast<DOMWindow>(eventTarget);
            if (auto* document = window.document())
                return document->settings().passiveTouchListenersAsDefaultOnDocument();
        } else if (is<Node>(eventTarget)) {
            auto& node = downcast<Node>(eventTarget);
            if (is<Document>(node) || node.document().documentElement() == &node || node.document().body() == &node)
                return node.document().settings().passiveTouchListenersAsDefaultOnDocument();
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
            if (jsEventListener.functionName() == "ssc_wheel")
                return true;
        }

        return false;
    }

    return false;
}

#if ENABLE(MEDIA_STREAM)
bool Quirks::shouldEnableLegacyGetUserMedia() const
{
    if (!needsQuirks())
        return false;

    return m_document->url().protocolIs("https") && equalLettersIgnoringASCIICase(m_document-> url().host(), "www.baidu.com");
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

    m_shouldDisableElementFullscreenQuirk = domain == "nfl.com" || domain.endsWith(".nfl.com");

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

    m_needsCanPlayAfterSeekedQuirk = domain == "hulu.com" || domain.endsWith(".hulu.com");

    return m_needsCanPlayAfterSeekedQuirk.value();
}

bool Quirks::shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraints() const
{
    if (!needsQuirks())
        return false;

    // FIXME: We should consider replacing this with a heuristic to determine whether
    // or not the edges of the page mostly lack content after shrinking to fit.
    return m_document->url().host().endsWithIgnoringASCIICase(".wikipedia.org");
}

bool Quirks::shouldIgnoreContentObservationForSyntheticClick(bool isFirstSyntheticClickOnPage) const
{
    if (!needsQuirks())
        return false;

    auto host = m_document->url().host();
    return isFirstSyntheticClickOnPage && (equalLettersIgnoringASCIICase(host, "shutterstock.com") || host.endsWithIgnoringASCIICase(".shutterstock.com"));
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

#if ENABLE(RESOURCE_LOAD_STATISTICS)
static bool isKinjaLoginAvatarElement(const Element& element)
{
    // The click event handler has been found to trigger on a div or
    // span with these class names, or the svg, or the svg's path.
    if (element.hasClass()) {
        auto& classNames = element.classNames();
        if (classNames.contains("js_switch-to-burner-login")
            || classNames.contains("js_header-userbutton")
            || classNames.contains("sc-1il3uru-3") || classNames.contains("cIhKfd")
            || classNames.contains("iyvn34-0") || classNames.contains("bYIjtl"))
            return true;
    }

    const Element* svgElement = nullptr;
    if (is<SVGSVGElement>(element))
        svgElement = &element;
    else if (is<SVGPathElement>(element) && is<SVGSVGElement>(element.parentElement()))
        svgElement = element.parentElement();

    if (svgElement && svgElement->hasAttributes()) {
        auto ariaLabelAttr = svgElement->attributes().getNamedItem("aria-label");
        if (ariaLabelAttr && ariaLabelAttr->value() == "UserFilled icon")
            return true;
    }

    return false;
}
#endif

Quirks::StorageAccessResult Quirks::triggerOptionalStorageAccessQuirk(const Element& element, const AtomString& eventType) const
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
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
    static NeverDestroyed<URL> kinjaURL = URL(URL(), "https://kinja.com");
    static NeverDestroyed<RegistrableDomain> kinjaDomain { kinjaURL };

    static NeverDestroyed<RegistrableDomain> youTubeDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString("youtube.com"_s);
    
    static NeverDestroyed<String> loginPopupWindowFeatureString = "toolbar=no,location=yes,directories=no,status=no,menubar=no,scrollbars=yes,resizable=yes,copyhistory=no,width=599,height=600,top=420,left=980.5"_s;

    static NeverDestroyed<UserScript> kinjaLoginUserScript { "function triggerLoginForm() { let elements = document.getElementsByClassName('js_header-userbutton'); if (elements && elements[0]) { elements[0].click(); clearInterval(interval); } } let interval = setInterval(triggerLoginForm, 200);", URL(aboutBlankURL()), Vector<String>(), Vector<String>(), UserScriptInjectionTime::DocumentEnd, UserContentInjectedFrames::InjectInTopFrameOnly, WaitForNotificationBeforeInjecting::Yes };

    if (eventType == "click") {

        // Embedded YouTube case.
        if (element.hasClass() && domain == youTubeDomain && !m_document->isTopDocument() && ResourceLoadObserver::shared().hasHadUserInteraction(youTubeDomain)) {
            auto& classNames = element.classNames();
            if (classNames.contains("ytp-watch-later-icon") || classNames.contains("ytp-watch-later-icon")) {
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

            ExceptionOr<RefPtr<WindowProxy>> proxyOrException =  domWindow->open(*domWindow, *domWindow, kinjaURL->string(), emptyString(), loginPopupWindowFeatureString);
            if (proxyOrException.hasException())
                return Quirks::StorageAccessResult::ShouldNotCancelEvent;
            auto proxy = proxyOrException.releaseReturnValue();

            auto* abstractFrame = proxy->frame();
            if (abstractFrame && is<Frame>(*abstractFrame)) {
                auto& frame = downcast<Frame>(*abstractFrame);
                auto world = ScriptController::createWorld("kinjaComQuirkWorld", ScriptController::WorldType::User);
                frame.addUserScriptAwaitingNotification(world.get(), kinjaLoginUserScript);
                return Quirks::StorageAccessResult::ShouldCancelEvent;
            }
        }
    }
#else
    UNUSED_PARAM(element);
    UNUSED_PARAM(eventType);
#endif
    return Quirks::StorageAccessResult::ShouldNotCancelEvent;
}

bool Quirks::needsVP9FullRangeFlagQuirk() const
{
    if (!needsQuirks())
        return false;

    if (!m_needsVP9FullRangeFlagQuirk)
        m_needsVP9FullRangeFlagQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "www.youtube.com");

    return *m_needsVP9FullRangeFlagQuirk;
}

bool Quirks::needsHDRPixelDepthQuirk() const
{
    if (!needsQuirks())
        return false;

    if (!m_needsHDRPixelDepthQuirk)
        m_needsHDRPixelDepthQuirk = equalLettersIgnoringASCIICase(m_document->url().host(), "www.youtube.com");

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

    if (!needsQuirks())
        return false;

    if (!element.hasClass())
        return false;

    auto& classNames = element.classNames();
    return classNames.contains(akamaiHTML5) && classNames.contains(akamaiMediaElement);
#else
    UNUSED_PARAM(element);
    return false;
#endif
}

}
