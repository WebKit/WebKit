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
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentPage.h"
#include "DocumentQuirks.h"
#include "DocumentStorageAccess.h"
#include "DocumentView.h"
#include "Element.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementInlines.h"
#include "ElementTargetingTypes.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTMLArticleElement.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLScriptElement.h"
#include "HTMLTextAreaElement.h"
#include "JSEventListener.h"
#include "KeyframeEffect.h"
#include "LayoutUnit.h"
#include "LocalDOMWindow.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "MouseEvent.h"
#include "NodeInlines.h"
#include "NodeRenderStyle.h"
#include "OrganizationStorageAccessPromptQuirk.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "QuirkBehaviorDefinitions.h"
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
#include "SelectorQuery.h"
#include "Settings.h"
#include "SpaceSplitString.h"
#include "StaticNodeList.h"
#include "StorageAccessQuirks.h"
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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include <pal/system/ios/UserInterfaceIdiom.h>
#endif

#define QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(returnValue) \
    if (!needsQuirks()) [[unlikely]] \
        return returnValue

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Quirks);

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

static bool urlHasQuirk(const URL& url, const QuirkBehavior& quirk)
{
    return resolveTopURLQuirks(url).isBehaviorEnabled(quirk.id);
}

Quirks::Quirks(Document& document)
    : QuirksAccessors(document)
{
    determineRelevantQuirks();
}

Quirks::~Quirks() = default;

bool Quirks::shouldIgnoreInvalidSignal() const
{
    return needsQuirks();
}

bool Quirks::shouldDisableBlobFileAccessEnforcement()
{
    return shouldDisableBlobFileAccessEnforcementInternal();
}

bool Quirks::needsAutoplayPlayPauseEvents() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (m_quirksData.isBehaviorEnabled(QuirkBehaviorID::ShouldDispatchPlayPauseEventsOnResume))
        return true;

    Ref document = *m_document;
    if (allowedAutoplayQuirks(document).contains(AutoplayQuirk::SynthesizedPauseEvents))
        return true;

    return allowedAutoplayQuirks(document->mainFrameDocument()).contains(AutoplayQuirk::SynthesizedPauseEvents);
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
    return m_quirksData.isBehaviorEnabled(QuirkBehaviorID::NeedsPerDocumentAutoplayBehaviorQuirk);
#endif
}

// youtube.com https://bugs.webkit.org/show_bug.cgi?id=195598
bool Quirks::hasBrokenEncryptedMediaAPISupportQuirk() const
{
#if ENABLE(THUNDER)
    return false;
#else
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.isBehaviorEnabled(QuirkBehaviorID::HasBrokenEncryptedMediaAPISupportQuirk);
#endif
}

// weebly.com rdar://48003980
// medium.com rdar://50457837
bool Quirks::shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const
{
    if (m_document->settings().shouldDispatchSyntheticMouseEventsWhenModifyingSelection())
        return true;

    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.isBehaviorEnabled(QuirkBehaviorID::ShouldDispatchSyntheticMouseEventsWhenModifyingSelectionQuirk);
}

// www.youtube.com rdar://52361019
bool Quirks::needsYouTubeMouseOutQuirk() const
{
    if (m_document->settings().shouldDispatchSyntheticMouseOutAfterSyntheticClick())
        return true;

    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.isBehaviorEnabled(QuirkBehaviorID::NeedsYouTubeMouseOutQuirk);
}

// The sites whose in-page captions WebKit mirrors into a text track rdar://174708144
bool Quirks::needsCaptionMirroringQuirk() const
{
    return needsYouTubeCaptionsQuirk() || needsCNNCaptionQuirk();
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

#if ENABLE(TOUCH_EVENTS) || ENABLE(TOUCH_EVENT_REGIONS)
// rdar://49124313
// desmos.com rdar://47068176
// flipkart.com rdar://49648520
// soundcloud.com rdar://52915981
// naver.com rdar://48068610

bool Quirks::shouldDispatchSimulatedMouseEvents(const EventTarget* target) const
{
    if (m_document->settings().mouseEventsSimulationEnabled())
        return true;

    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    RefPtr loader = m_document->loader();
    if (!loader || loader->simulatedMouseEventsDispatchPolicy() != SimulatedMouseEventsDispatchPolicy::Allow)
        return false;

    return behaviorAppliesToNode(QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsQuirk, dynamicDowncast<Node>(target));
}

bool Quirks::shouldPreventDispatchOfTouchEvent(const AtomString& touchEventType, EventTarget* target) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    auto& eventNames = WebCore::eventNames();
    if (touchEventType == eventNames.touchendEvent)
        return behaviorAppliesToNode(QuirkBehaviorID::ShouldPreventTouchEndDispatchQuirk, dynamicDowncast<Node>(target));
    if (touchEventType == eventNames.touchmoveEvent)
        return behaviorAppliesToNode(QuirkBehaviorID::ShouldPreventTouchMoveDispatchQuirk, dynamicDowncast<Node>(target));

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

    return behaviorAppliesToNode(QuirkBehaviorID::ShouldDispatchSimulatedMouseEventsAssumeDefaultPreventedQuirk, dynamicDowncast<Node>(target));
}
#endif

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
bool Quirks::needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommand() const
{
    if (m_document->settings().needsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk())
        return true;

    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.isBehaviorEnabled(QuirkBehaviorID::NeedsDeferKeyDownAndKeyPressTimersUntilNextEditingCommandQuirk);
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199587
bool Quirks::inputMethodUsesCorrectKeyEventOrder() const
{
    return false;
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

    return m_quirksData.isBehaviorEnabled(QuirkBehaviorID::ShouldSilenceResizeObservers);
}

bool Quirks::shouldSilenceWindowResizeEventsDuringApplicationSnapshotting() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.isBehaviorEnabled(QuirkBehaviorID::ShouldSilenceWindowResizeEventsDuringApplicationSnapshotting))
        return false;

    // We silence window resize events during the 'homing out' snapshot sequence when on icloud.com/mail
    // to address <rdar://131836301>, on nytimes.com to address <rdar://problem/59763843>, and on
    // x.com (twitter) to address <rdar://problem/58804852> & <rdar://problem/61731801>.
    RefPtr page = m_document->page();
    if (!page || !page->isTakingSnapshotsForApplicationSuspension())
        return false;

    return true;
}

bool Quirks::shouldSilenceMediaQueryListChangeEvents() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.isBehaviorEnabled(QuirkBehaviorID::ShouldSilenceMediaQueryListChangeEvents))
        return false;

    // We silence MediaQueryList's change events during the 'homing out' snapshot sequence when on x.com (twitter)
    // to address <rdar://problem/58804852> & <rdar://problem/61731801>.
    RefPtr page = m_document->page();
    if (!page || !page->isTakingSnapshotsForApplicationSuspension())
        return false;

    return true;
}

// docs.google.com https://bugs.webkit.org/show_bug.cgi?id=199933
bool Quirks::shouldOpenAsAboutBlank(const String& stringToOpen) const
{
#if PLATFORM(IOS_FAMILY)
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.isBehaviorEnabled(QuirkBehaviorID::ShouldOpenAsAboutBlankQuirk))
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

// vimeo.com rdar://56996057
// docs.google.com rdar://59893415
// bing.com rdar://133223599
// bankofamerica.com rdar://104938789
bool Quirks::shouldBypassBackForwardCache() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    RefPtr document = m_document.get();

    if (m_quirksData.isBehaviorEnabled(QuirkBehaviorID::ShouldBypassBackForwardCacheForNoStoreQuirk) && topDocumentURL().protocolIs("https"_s)) {
        if (RefPtr documentLoader = document->frame() ? document->frame()->loader().documentLoader() : nullptr; documentLoader && documentLoader->response().cacheControlContainsNoStore())
            return true;
    }

    if (RefPtr element = elementMatchingDocumentSelectorCondition(QuirkBehaviorID::ShouldBypassBackForwardCacheWhenRenderedElementMatchesQuirk); element && element->renderer())
        return true;

    if (behaviorAppliesToDocument(QuirkBehaviorID::ShouldBypassBackForwardCacheWhenElementMatchesQuirk))
        return true;

    if (m_quirksData.isBehaviorEnabled(QuirkBehaviorID::ShouldBypassBackForwardCacheWhenUnloadListenerAndElementMatchesQuirk)) {
        if (RefPtr window = document->window(); window && window->hasEventListeners(eventNames().unloadEvent))
            return behaviorAppliesToDocument(QuirkBehaviorID::ShouldBypassBackForwardCacheWhenUnloadListenerAndElementMatchesQuirk);
    }

    return false;
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
    if (m_didApplyFacebookFlagQuirk)
        return nodeList;
    m_didApplyFacebookFlagQuirk = true;

    if (!document.settings().facebookLiveRecordingQuirkEnabled())
        return nodeList;

    auto elements = copyElements(nodeList);
    // Live Streaming flag activation
    elements.append(createFacebookFlagElement(document, "23460"_s));
    return StaticElementList::create(WTF::move(elements));
}

bool Quirks::shouldEnableRTCEncodedStreamsQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return m_quirksData.isBehaviorEnabled(QuirkBehaviorID::ShouldEnableRTCEncodedStreamsQuirk) && m_document && m_document->settings().rtcEncodedStreamsQuirkEnabled();
}

bool Quirks::shouldNotAutoUpgradeToHTTPSNavigation(const URL& url)
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return shouldNotAutoUpgradeToHTTPSNavigationInternal(url);
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
    auto domainsInNeedOfStorageAccess = subResourceDomainsInNeedOfStorageAccessForFirstParty(firstPartyDomain);
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

    static NeverDestroyed kinjaURL = URL { "https://kinja.com"_str };
    static NeverDestroyed<RegistrableDomain> kinjaDomain { kinjaURL };

    static NeverDestroyed<RegistrableDomain> youTubeDomain = RegistrableDomain::uncheckedCreateFromRegistrableDomainString("youtube.com"_s);

    static NeverDestroyed<String> loginPopupWindowFeatureString = "toolbar=no,location=yes,directories=no,status=no,menubar=no,scrollbars=yes,resizable=yes,copyhistory=no,width=599,height=600,top=420,left=980.5"_s;

    static NeverDestroyed<UserScript> kinjaLoginUserScript { "function triggerLoginForm() { let elements = document.getElementsByClassName('js_header-userbutton'); if (elements && elements[0]) { elements[0].click(); clearInterval(interval); } } let interval = setInterval(triggerLoginForm, 200);"_s, URL(aboutBlankURL()), Vector<String>(), Vector<String>(), UserScriptInjectionTime::DocumentEnd, UserContentInjectedFrames::InjectInTopFrameOnly };

    if (isAnyClick(eventType)) {
        RefPtr document = m_document.get();
        if (!document)
            return Quirks::StorageAccessResult::ShouldNotCancelEvent;

        constexpr auto watchLaterID = QuirkBehaviorID::NeedsStorageAccessForYouTubeWatchLaterQuirk;
        if (m_quirksData.isBehaviorEnabled(watchLaterID) && ResourceLoadObserver::singleton().hasHadUserInteraction(youTubeDomain)) {
            if (behaviorAppliesToNode(watchLaterID, &element))
                DocumentStorageAccess::requestStorageAccessForDocumentQuirk(*document, [](StorageAccessWasGranted) { });
            return Quirks::StorageAccessResult::ShouldNotCancelEvent;
        }

        // Kinja login case.
        if (behaviorAppliesToNode(QuirkBehaviorID::NeedsKinjaLoginStorageAccessQuirk, &element)) {
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
        if (behaviorAppliesToNode(QuirkBehaviorID::NeedsStorageAccessOnLoginButtonClickQuirk, &element) && isSyntheticClick == IsSyntheticClick::No) {
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

// teams.live.com rdar://88678598
// teams.microsoft.com rdar://90434296
bool Quirks::shouldAllowNavigationToCustomProtocolWithoutUserGesture(StringView protocol, const SecurityOriginData& requesterOrigin)
{
    if (protocol != "msteams"_s)
        return false;
    return urlHasQuirk(requesterOrigin.toURL(), QuirkBehaviors::shouldAllowMSTeamsProtocolWithoutUserGestureQuirk);
}

// play.hbomax.com https://bugs.webkit.org/show_bug.cgi?id=244737
bool Quirks::shouldEnableFontLoadingAPIQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (m_document->settings().downloadableBinaryFontTrustedTypes() == DownloadableBinaryFontTrustedTypes::Any)
        return false;

    return m_quirksData.isBehaviorEnabled(QuirkBehaviorID::ShouldEnableFontLoadingAPIQuirk);
}

// reddit.com with Sink It extension (rdar://176377447) and apple.com/retail (rdar://181007316).
bool Quirks::shouldDisableScrollAnchoringQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return behaviorAppliesToDocument(QuirkBehaviorID::ShouldDisableScrollAnchoringQuirk);
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

bool Quirks::needsDisableDOMPasteAccessQuirk() const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    return isBehaviorEnabledAfterProbing(QuirkBehaviors::needsDisableDOMPasteAccessQuirk, [&] {
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

std::optional<String> Quirks::needsCustomUserAgentOverride(const URL& url, const String& applicationNameForUserAgent, const String& currentUserAgent)
{
    auto quirksData = resolveTopURLQuirks(url);

    std::optional<String> userAgent;
    for (const auto& behavior : quirksData.behaviorsMatching(QuirkBehaviorID::NeedsUserAgentStringOverrideQuirk)) {
        if (!behavior.userAgent.isEmpty()) {
            userAgent = behavior.userAgent;
            break;
        }
    }

#if PLATFORM(COCOA)
    for (const auto& behavior : quirksData.behaviorsMatching(QuirkBehaviorID::NeedsChromeCompatibilityUserAgentQuirk)) {
        if (behavior.chromeCompatibilityVersion.isEmpty())
            continue;

        if (!userAgent)
            userAgent = currentUserAgent.isEmpty() ? standardUserAgentWithApplicationName(applicationNameForUserAgent) : currentUserAgent;
        auto chromeCompatibilityToken = makeString("like Gecko, like Chrome/"_s, behavior.chromeCompatibilityVersion, '.');
        return makeStringByReplacingAll(*userAgent, "like Gecko"_s, chromeCompatibilityToken);
    }
#else
    UNUSED_PARAM(applicationNameForUserAgent);
    UNUSED_PARAM(currentUserAgent);
#endif

    return userAgent;
}

bool Quirks::needsDesktopUserAgent(const URL& url)
{
    return needsDesktopUserAgentInternal(url);
}

bool Quirks::needsPartitionedCookies(const ResourceRequest& request)
{
    if (request.isTopSite())
        return false;
    return urlHasQuirk(request.url(), QuirkBehaviors::needsPartitionedCookiesQuirk);
}

// m365.cloud.microsoft rdar://157794706
// Allow popups from m365.cloud.microsoft to onedrive.live.com
bool Quirks::needsPopupFromMicrosoftOfficeToOneDrive(const String& targetURLString) const
{
    constexpr auto id = QuirkBehaviorID::ShouldAllowPopupFromMicrosoftOfficeToOneDrive;
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.isBehaviorEnabled(id))
        return false;

    return m_quirksData.behaviorAppliesToURL(id, protect(m_document)->encodingParseURL(targetURLString));
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

std::optional<TargetedElementSelectors> Quirks::defaultVisibilityAdjustmentSelectors(const URL& requestURL)
{
#if ENABLE(VISIBILITY_ADJUSTMENT_QUIRKS)
    return defaultVisibilityAdjustmentSelectorsInternal(requestURL);
#else
    UNUSED_PARAM(requestURL);
    return { };
#endif
}

Vector<String, 1> Quirks::scriptsToEvaluateBeforeRunningScriptFromURL(const URL& scriptURL)
{
    constexpr auto id = QuirkBehaviorID::NeedsScriptToEvaluateBeforeRunningScriptFromURLQuirk;
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE({ });

    if (scriptURL.isEmpty())
        return { };

    if (!m_quirksData.isBehaviorEnabled(id))
        return { };

    Vector<String, 1> scripts;
    URLMatchContext scriptURLContext { scriptURL };

    const auto matchingBehaviors = m_quirksData.behaviorsMatching(id);

    for (const auto& behavior : matchingBehaviors) {
        if (behavior.script.isEmpty())
            continue;

        if (!behavior.secondaryURLConditionMatches(scriptURLContext))
            continue;

        scripts.append(behavior.script);
    }

    return scripts;
}

bool Quirks::needsChromeOSNavigatorUserAgentQuirk(const Document& document) const
{
    constexpr auto id = QuirkBehaviorID::NeedsChromeOSNavigatorUserAgentQuirk;
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.isBehaviorEnabled(id))
        return false;

    return m_quirksData.behaviorAppliesToURL(id, document.currentSourceURL());
}

// spotify.com rdar://140707449
bool Quirks::shouldAvoidStartingSelectionOnMouseDownOverPointerCursor(const Node& target) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.isBehaviorEnabled(QuirkBehaviorID::ShouldAvoidStartingSelectionOnMouseDownOverPointerCursor))
        return false;

    if (auto* style = target.renderStyle()) {
        if (style->cursorType() == CursorType::Pointer)
            return true;
    }

    return false;
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

    if (!m_quirksData.behaviorAppliesToURL(QuirkBehaviorID::NeedsFacebookStoriesCreationFormQuirk, topDocumentURL())) {
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

    if (AccessibilityObject::ariaRoleToWebCoreRole(element.attributeWithoutSynchronization(HTMLNames::roleAttr)) != AccessibilityRole::LandmarkNavigation)
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

// claude.ai rdar://162616694
bool Quirks::needsClaudeSidebarViewportUnitQuirk(Element& element, const Style::ComputedStyle& style) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.isBehaviorEnabled(QuirkBehaviorID::NeedsClaudeSidebarViewportUnitQuirk))
        return false;

    if (style.position() != PositionType::Fixed)
        return false;

    if (!behaviorAppliesToNode(QuirkBehaviorID::NeedsClaudeSidebarViewportUnitQuirk, &element))
        return false;

    if (auto fixedHeight = style.height().tryFixed()) {
        if (fixedHeight->resolveZoom(style.usedZoomForLength()) == protect(m_document->renderView())->sizeForCSSDefaultViewportUnits().height())
            return true;
    }

    return false;
}

// amazon.design rdar://175953409
bool Quirks::needsAmazonDesignMenuViewportUnitQuirk(const Style::ComputedStyle& style, const Style::ComputedStyle& parentStyle) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

    if (!m_quirksData.isBehaviorEnabled(QuirkBehaviorID::NeedsAmazonDesignMenuViewportUnitQuirk))
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

    constexpr auto id = QuirkBehaviorID::NeedsLogoutCookieCleanupQuirk;
    if (!m_quirksData.isBehaviorEnabled(id))
        return;

    if (httpStatusCode < 200 || httpStatusCode >= 300)
        return;

    RefPtr document = m_document.get();
    if (!document)
        return;

    RefPtr page = document->page();
    if (!page)
        return;

    auto& documentURL = document->url();
    URLMatchContext fetchURLContext { fetchURL };
    for (const auto& behavior : m_quirksData.behaviors()) {
        if (behavior.id != id || !behavior.secondaryURLConditionMatches(fetchURLContext))
            continue;

        for (auto& cookieName : behavior.cookieNames)
            page->cookieJar().deleteCookie(*document, documentURL, cookieName, [] { });
    }
}

// tiktok.com rdar://149712691
std::optional<Quirks::TikTokOverflowingContentQuirkType> Quirks::needsTikTokOverflowingContentQuirk(const Element& element, const Style::ComputedStyle& parentStyle) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE({ });

    constexpr auto commentsID = QuirkBehaviorID::NeedsTikTokCommentsOverflowingContentQuirk;
    constexpr auto videoID = QuirkBehaviorID::NeedsTikTokVideoOverflowingContentQuirk;
    if (!m_quirksData.isBehaviorEnabled(commentsID) && !m_quirksData.isBehaviorEnabled(videoID))
        return { };

    if (parentStyle.display() != Style::DisplayType::BlockFlex)
        return { };

    if (parentStyle.position() != PositionType::Fixed)
        return { };

    if (!element.hasClass())
        return { };

    if (behaviorAppliesToNode(commentsID, &element))
        return TikTokOverflowingContentQuirkType::CommentsSectionQuirk;

    if (behaviorAppliesToNode(videoID, &element))
        return TikTokOverflowingContentQuirkType::VideoSectionQuirk;

    return { };
}

// rdar://166400170
bool Quirks::needsInstagramResizingReelsQuirk(const Element& element, const Style::ComputedStyle& elementStyle, const Style::ComputedStyle& parentStyle) const
{
    QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE(false);

#if ENABLE(VIDEO)
    if (!m_quirksData.isBehaviorEnabled(QuirkBehaviorID::NeedsInstagramResizingReelsQuirk))
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

    return behaviorAppliesToNode(QuirkBehaviorID::NeedsInstagramResizingReelsQuirk, &element);
#else
    UNUSED_PARAM(element);
    UNUSED_PARAM(elementStyle);
    UNUSED_PARAM(parentStyle);
    return false;
#endif // ENABLE(VIDEO)
}

// rdar://106770785
bool Quirks::shouldPreventKeyframeEffectAcceleration(const KeyframeEffect& effect) const
{
    if (!needsQuirks())
        return false;

    auto target = effect.targetStyleable();
    if (!target)
        return false;

    Ref element = target->element;
    return behaviorAppliesToNode(QuirkBehaviorID::ShouldPreventKeyframeEffectAccelerationQuirk, element.ptr());
}

URL Quirks::topDocumentURL() const
{
    if (!m_topDocumentURLForTesting.isEmpty()) [[unlikely]]
        return m_topDocumentURLForTesting;

    Ref document = *protect(m_document);
    auto url = document->topURL();
    if (RefPtr page = document->page(); page && !page->quirksTopDocumentHostForTesting().isEmpty()) [[unlikely]]
        url.setHost(page->quirksTopDocumentHostForTesting());

    return url;
}

URL Quirks::documentURL() const
{
    Ref document = *protect(m_document);

    if (!document->isTopDocument()) [[unlikely]] {
        if (RefPtr page = document->page(); page && !page->quirksSubframeURLForTesting().isEmpty())
            return page->quirksSubframeURLForTesting();
    }

    return document->url();
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
    m_probedQuirks = { };

#if PLATFORM(IOS_FAMILY)
    static const bool shouldDisableLazyIframeLoadingQuirk = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoUNIQLOLazyIframeLoadingQuirk) && WTF::IOSApplication::isUNIQLOApp();
    static const bool needsResettingTransitionCancelsRunningTransitionQuirk = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ResettingTransitionCancelsRunningTransitionQuirk) && WTF::IOSApplication::isDOFUSTouch();
    static const bool shouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoMediaLayerTeardownOnPageVisibilityChangeQuirk) && WTF::IOSApplication::isMoonPlayer();

    m_quirksData.setEnabled(QuirkBehaviors::shouldDisableLazyIframeLoadingQuirk, shouldDisableLazyIframeLoadingQuirk);

    // DOFUS Touch app (rdar://112679186)
    m_quirksData.setEnabled(QuirkBehaviors::needsResettingTransitionCancelsRunningTransitionQuirk, needsResettingTransitionCancelsRunningTransitionQuirk);

    // Moon Player app (rdar://162452658)
    m_quirksData.setEnabled(QuirkBehaviors::shouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk, shouldDisableMediaLayerTeardownOnPageVisibilityChangeQuirk);
#endif

#if PLATFORM(MAC)
    static const bool shouldDisablePushStateFilePathRestrictions = WTF::MacApplication::isMimeoPhotoProject();

    // Push state file path restrictions break Mimeo Photo Plugin (rdar://112445672).
    m_quirksData.setEnabled(QuirkBehaviors::shouldDisablePushStateFilePathRestrictions, shouldDisablePushStateFilePathRestrictions);
#endif

    auto quirksURL = topDocumentURL();
    if (quirksURL.isEmpty())
        return;

    Ref document = *protect(m_document);
    m_quirksData.merge(resolveSiteSpecificQuirks(quirksURL, documentURL(), document->isTopDocument() ? IsTopDocument::Yes : IsTopDocument::No));

#if ENABLE(FLIP_SCREEN_DIMENSIONS_QUIRKS)
    // rdar://133423460
    m_quirksData.setEnabled(QuirkBehaviors::shouldFlipScreenDimensionsQuirk, shouldFlipScreenDimensionsInternal(quirksURL));
#endif

    // rdar://133423460
    m_quirksData.setEnabled(QuirkBehaviors::shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeQuirk, shouldPreventOrientationMediaQueryFromEvaluatingToLandscapeInternal(quirksURL));

#if PLATFORM(IOS)
    // Use the opt-in Document tracks while parsing and freezes afterwards, rather than the live
    // viewport arguments: a page that rewrites its own <meta name="viewport"> once loaded does not
    // actually get the safe area insets honored, so it must not be able to toggle this quirk either.
    // This is deliberately the local document's own opt-in, not the main frame's, so that embedded
    // content (a YouTube embed, say) can declare that it handles safe area insets independently of
    // whatever its embedder did. Note a subframe's viewport-fit has no effect on the insets the
    // platform actually reports, so for a subframe this is a declaration of intent rather than a
    // description of the platform state.
    if (PAL::currentUserInterfaceIdiomIsSmallScreen())
        m_quirksData.setEnabled(QuirkBehaviors::shouldDisableElementFullscreenQuirk, document->safeAreaInsetOptIn() == Document::SafeAreaInsetOptIn::NotOptedIn);
#endif
}

void Quirks::urlsDidChange()
{
    auto previousQuirksData = m_quirksData;
    determineRelevantQuirks();

    if (!m_quirksData.hasSameBehaviorFlags(previousQuirksData))
        protect(m_document)->scheduleFullStyleRebuild();
}

void Quirks::logQuirksToConsoleIfNecessary() const
{
    RefPtr document = m_document.get();
    if (!document)
        return;

    if (!needsQuirks())
        return;

    // FIXME: should we log english sentences instead of the quirk enum names?
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

    for (auto quirk : m_quirksData.behaviors())
        result.append(String { WTF::enumName(static_cast<QuirkBehaviorID>(quirk.id)) });

    std::ranges::sort(result, codePointCompareLessThan);
    return result;
}

bool Quirks::hasRelevantQuirks() const
{
    return m_quirksData.hasBehaviors();
}

}

#undef QUIRKS_EARLY_RETURN_IF_DISABLED_WITH_VALUE
