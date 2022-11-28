/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003-2016 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "HTMLAnchorElement.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMTokenList.h"
#include "ElementIterator.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameLoaderTypes.h"
#include "FrameSelection.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLPictureElement.h"
#include "KeyboardEvent.h"
#include "LoaderStrategy.h"
#include "MouseEvent.h"
#include "PingLoader.h"
#include "PlatformMouseEvent.h"
#include "PlatformStrategies.h"
#include "PrivateClickMeasurement.h"
#include "RegistrableDomain.h"
#include "RenderImage.h"
#include "ResourceRequest.h"
#include "RuntimeApplicationChecks.h"
#include "SVGImage.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/WeakHashMap.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

#if PLATFORM(COCOA)
#include "DataDetection.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLAnchorElement);

using namespace HTMLNames;

HTMLAnchorElement::HTMLAnchorElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
}

Ref<HTMLAnchorElement> HTMLAnchorElement::create(Document& document)
{
    return adoptRef(*new HTMLAnchorElement(aTag, document));
}

Ref<HTMLAnchorElement> HTMLAnchorElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLAnchorElement(tagName, document));
}

HTMLAnchorElement::~HTMLAnchorElement()
{
    clearRootEditableElementForSelectionOnMouseDown();
}

bool HTMLAnchorElement::supportsFocus() const
{
    if (hasEditableStyle())
        return HTMLElement::supportsFocus();
    // If not a link we should still be able to focus the element if it has tabIndex.
    return isLink() || HTMLElement::supportsFocus();
}

bool HTMLAnchorElement::isMouseFocusable() const
{
#if !(PLATFORM(GTK) || PLATFORM(WPE))
    // Only allow links with tabIndex or contentEditable to be mouse focusable.
    if (isLink())
        return HTMLElement::supportsFocus();
#endif

    return HTMLElement::isMouseFocusable();
}

bool HTMLAnchorElement::isInteractiveContent() const
{
    return isLink();
}

static bool hasNonEmptyBox(RenderBoxModelObject* renderer)
{
    if (!renderer)
        return false;

    // Before calling absoluteRects, check for the common case where borderBoundingBox
    // is non-empty, since this is a faster check and almost always returns true.
    // FIXME: Why do we need to call absoluteRects at all?
    if (!renderer->borderBoundingBox().isEmpty())
        return true;

    // FIXME: Since all we are checking is whether the rects are empty, could we just
    // pass in 0,0 for the layout point instead of calling localToAbsolute?
    Vector<IntRect> rects;
    renderer->absoluteRects(rects, flooredLayoutPoint(renderer->localToAbsolute()));
    for (auto& rect : rects) {
        if (!rect.isEmpty())
            return true;
    }

    return false;
}

bool HTMLAnchorElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (!isLink())
        return HTMLElement::isKeyboardFocusable(event);

    if (!isFocusable())
        return false;
    
    if (!document().frame())
        return false;

    if (!document().frame()->eventHandler().tabsToLinks(event))
        return false;

    if (!renderer() && ancestorsOfType<HTMLCanvasElement>(*this).first())
        return true;

    return hasNonEmptyBox(renderBoxModelObject());
}

static void appendServerMapMousePosition(StringBuilder& url, Event& event)
{
    if (!is<MouseEvent>(event))
        return;
    auto& mouseEvent = downcast<MouseEvent>(event);

    if (!is<HTMLImageElement>(mouseEvent.target()))
        return;

    auto& imageElement = downcast<HTMLImageElement>(*mouseEvent.target());
    if (!imageElement.isServerMap())
        return;

    auto* renderer = imageElement.renderer();
    if (!is<RenderImage>(renderer))
        return;

    // FIXME: This should probably pass UseTransforms in the OptionSet<MapCoordinatesMode>.
    auto absolutePosition = downcast<RenderImage>(*renderer).absoluteToLocal(FloatPoint(mouseEvent.pageX(), mouseEvent.pageY()));
    url.append('?', std::lround(absolutePosition.x()), ',', std::lround(absolutePosition.y()));
}

void HTMLAnchorElement::defaultEventHandler(Event& event)
{
    if (isLink()) {
        if (focused() && isEnterKeyKeydownEvent(event) && treatLinkAsLiveForEventType(NonMouseEvent)) {
            event.setDefaultHandled();
            dispatchSimulatedClick(&event);
            return;
        }

        if (MouseEvent::canTriggerActivationBehavior(event) && treatLinkAsLiveForEventType(eventType(event))) {
            handleClick(event);
            return;
        }

        if (hasEditableStyle()) {
            // This keeps track of the editable block that the selection was in (if it was in one) just before the link was clicked
            // for the LiveWhenNotFocused editable link behavior
            auto& eventNames = WebCore::eventNames();
            if (event.type() == eventNames.mousedownEvent && is<MouseEvent>(event) && downcast<MouseEvent>(event).button() != RightButton && document().frame()) {
                setRootEditableElementForSelectionOnMouseDown(document().frame()->selection().selection().rootEditableElement());
                m_wasShiftKeyDownOnMouseDown = downcast<MouseEvent>(event).shiftKey();
            } else if (event.type() == eventNames.mouseoverEvent) {
                // These are cleared on mouseover and not mouseout because their values are needed for drag events,
                // but drag events happen after mouse out events.
                clearRootEditableElementForSelectionOnMouseDown();
                m_wasShiftKeyDownOnMouseDown = false;
            }
        }
    }

    HTMLElement::defaultEventHandler(event);
}

void HTMLAnchorElement::setActive(bool down, Style::InvalidationScope invalidationScope)
{
    if (down && hasEditableStyle()) {
        switch (document().settings().editableLinkBehavior()) {
        case EditableLinkBehavior::Default:
        case EditableLinkBehavior::AlwaysLive:
            break;

        // Don't set the link to be active if the current selection is in the same editable block as this link.
        case EditableLinkBehavior::LiveWhenNotFocused:
            if (down && document().frame() && document().frame()->selection().selection().rootEditableElement() == rootEditableElement())
                return;
            break;
        
        case EditableLinkBehavior::NeverLive:
        case EditableLinkBehavior::OnlyLiveWithShiftKey:
            return;

        }
    }
    
    HTMLElement::setActive(down, invalidationScope);
}

void HTMLAnchorElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == hrefAttr) {
        bool wasLink = isLink();
        setIsLink(!value.isNull() && !shouldProhibitLinks(this));
        if (wasLink != isLink())
            invalidateStyleForSubtree();
        if (isLink()) {
            String parsedURL = stripLeadingAndTrailingHTMLSpaces(value);
            if (document().isDNSPrefetchEnabled() && document().frame()) {
                if (protocolIsInHTTPFamily(parsedURL) || parsedURL.startsWith("//"_s))
                    document().frame()->loader().client().prefetchDNS(document().completeURL(parsedURL).host().toString());
            }
        }
    } else if (name == nameAttr || name == titleAttr) {
        // Do nothing.
    } else if (name == relAttr) {
        // Update HTMLAnchorElement::relList() if more rel attributes values are supported.
        static MainThreadNeverDestroyed<const AtomString> noReferrer("noreferrer"_s);
        static MainThreadNeverDestroyed<const AtomString> noOpener("noopener"_s);
        static MainThreadNeverDestroyed<const AtomString> opener("opener"_s);
        SpaceSplitString relValue(value, SpaceSplitString::ShouldFoldCase::Yes);
        if (relValue.contains(noReferrer))
            m_linkRelations.add(Relation::NoReferrer);
        if (relValue.contains(noOpener))
            m_linkRelations.add(Relation::NoOpener);
        if (relValue.contains(opener))
            m_linkRelations.add(Relation::Opener);
        if (m_relList)
            m_relList->associatedAttributeValueChanged(value);
    }
    else
        HTMLElement::parseAttribute(name, value);
}

bool HTMLAnchorElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name().localName() == hrefAttr || HTMLElement::isURLAttribute(attribute);
}

bool HTMLAnchorElement::canStartSelection() const
{
    if (!isLink())
        return HTMLElement::canStartSelection();
    return hasEditableStyle();
}

bool HTMLAnchorElement::draggable() const
{
    const AtomString& value = attributeWithoutSynchronization(draggableAttr);
    if (equalLettersIgnoringASCIICase(value, "true"_s))
        return true;
    if (equalLettersIgnoringASCIICase(value, "false"_s))
        return false;
    return hasAttributeWithoutSynchronization(hrefAttr);
}

URL HTMLAnchorElement::href() const
{
    return document().completeURL(attributeWithoutSynchronization(hrefAttr));
}

void HTMLAnchorElement::setHref(const AtomString& value)
{
    setAttributeWithoutSynchronization(hrefAttr, value);
}

bool HTMLAnchorElement::hasRel(Relation relation) const
{
    return m_linkRelations.contains(relation);
}

DOMTokenList& HTMLAnchorElement::relList()
{
    if (!m_relList) {
        m_relList = makeUnique<DOMTokenList>(*this, HTMLNames::relAttr, [](Document& document, StringView token) {
#if USE(SYSTEM_PREVIEW)
            if (equalLettersIgnoringASCIICase(token, "ar"_s))
                return document.settings().systemPreviewEnabled();
#else
            UNUSED_PARAM(document);
#endif
            return equalLettersIgnoringASCIICase(token, "noreferrer"_s) || equalLettersIgnoringASCIICase(token, "noopener"_s) || equalLettersIgnoringASCIICase(token, "opener"_s);
        });
    }
    return *m_relList;
}

const AtomString& HTMLAnchorElement::name() const
{
    return getNameAttribute();
}

int HTMLAnchorElement::defaultTabIndex() const
{
    return 0;
}

AtomString HTMLAnchorElement::target() const
{
    return attributeWithoutSynchronization(targetAttr);
}

String HTMLAnchorElement::origin() const
{
    return SecurityOrigin::create(href()).get().toString();
}

String HTMLAnchorElement::text()
{
    return textContent();
}

void HTMLAnchorElement::setText(String&& text)
{
    setTextContent(WTFMove(text));
}

bool HTMLAnchorElement::isLiveLink() const
{
    return isLink() && treatLinkAsLiveForEventType(m_wasShiftKeyDownOnMouseDown ? MouseEventWithShiftKey : MouseEventWithoutShiftKey);
}

void HTMLAnchorElement::sendPings(const URL& destinationURL)
{
    if (!document().frame())
        return;

    if (!hasAttributeWithoutSynchronization(pingAttr) || !document().settings().hyperlinkAuditingEnabled())
        return;

    SpaceSplitString pingURLs(attributeWithoutSynchronization(pingAttr), SpaceSplitString::ShouldFoldCase::No);
    for (unsigned i = 0; i < pingURLs.size(); i++)
        PingLoader::sendPing(*document().frame(), document().completeURL(pingURLs[i]), destinationURL);
}

#if USE(SYSTEM_PREVIEW)
bool HTMLAnchorElement::isSystemPreviewLink()
{
    if (!document().settings().systemPreviewEnabled())
        return false;

    static MainThreadNeverDestroyed<const AtomString> systemPreviewRelValue("ar"_s);

    if (!relList().contains(systemPreviewRelValue))
        return false;

    if (auto* child = firstElementChild()) {
        if (is<HTMLImageElement>(child) || is<HTMLPictureElement>(child)) {
            auto numChildren = childElementCount();
            // FIXME: We've documented that it should be the only child, but some early demos have two children.
            return numChildren == 1 || numChildren == 2;
        }
    }

    return false;
}
#endif

std::optional<URL> HTMLAnchorElement::attributionDestinationURLForPCM() const
{
    URL destinationURL { attributeWithoutSynchronization(attributiondestinationAttr) };
    if (destinationURL.isValid() && destinationURL.protocolIsInHTTPFamily())
        return destinationURL;

    document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "attributiondestination could not be converted to a valid HTTP-family URL."_s);
    return std::nullopt;
}

std::optional<RegistrableDomain> HTMLAnchorElement::mainDocumentRegistrableDomainForPCM() const
{
    if (auto frame = document().frame()) {
        if (auto mainDocument = frame->mainFrame().document()) {
            if (auto mainDocumentRegistrableDomain = RegistrableDomain { mainDocument->url() }; !mainDocumentRegistrableDomain.isEmpty())
                return mainDocumentRegistrableDomain;
        }
    }

    document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "Could not find a main document to use as source site for Private Click Measurement."_s);
    return std::nullopt;
}

std::optional<PCM::EphemeralNonce> HTMLAnchorElement::attributionSourceNonceForPCM() const
{
    auto attributionSourceNonceAttr = attributeWithoutSynchronization(attributionsourcenonceAttr);
    if (attributionSourceNonceAttr.isEmpty())
        return std::nullopt;

    auto ephemeralNonce = PCM::EphemeralNonce { attributionSourceNonceAttr };
    if (!ephemeralNonce.isValid()) {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "attributionsourcenonce was not valid."_s);
        return std::nullopt;
    }

    return ephemeralNonce;
}

std::optional<PrivateClickMeasurement> HTMLAnchorElement::parsePrivateClickMeasurementForSKAdNetwork(const URL& hrefURL) const
{
    if (!document().settings().sKAttributionEnabled())
        return std::nullopt;

    using SourceID = PrivateClickMeasurement::SourceID;
    using SourceSite = PCM::SourceSite;
    using AttributionDestinationSite = PCM::AttributionDestinationSite;

    auto adamID = PrivateClickMeasurement::appStoreURLAdamID(hrefURL);
    if (!adamID)
        return std::nullopt;

    auto attributionDestinationDomain = attributionDestinationURLForPCM();
    if (!attributionDestinationDomain)
        return std::nullopt;

    auto mainDocumentRegistrableDomain = mainDocumentRegistrableDomainForPCM();
    if (!mainDocumentRegistrableDomain)
        return std::nullopt;

    auto attributionSourceNonce = attributionSourceNonceForPCM();
    if (!attributionSourceNonce)
        return std::nullopt;

#if PLATFORM(COCOA)
    auto bundleID = applicationBundleIdentifier();
#else
    String bundleID;
#endif

    auto privateClickMeasurement = PrivateClickMeasurement {
        SourceID(0),
        SourceSite(WTFMove(*mainDocumentRegistrableDomain)),
        AttributionDestinationSite(*attributionDestinationDomain),
        bundleID,
        WallTime::now(),
        PCM::AttributionEphemeral::No
    };
    privateClickMeasurement.setEphemeralSourceNonce(WTFMove(*attributionSourceNonce));
    privateClickMeasurement.setAdamID(*adamID);
    return privateClickMeasurement;
}

std::optional<PrivateClickMeasurement> HTMLAnchorElement::parsePrivateClickMeasurement(const URL& hrefURL) const
{
    using SourceID = PrivateClickMeasurement::SourceID;
    using SourceSite = PCM::SourceSite;
    using AttributionDestinationSite = PCM::AttributionDestinationSite;

    RefPtr<Frame> frame = document().frame();
    auto* page = document().page();
    if (!frame || !page || page->sessionID().isEphemeral()
        || !document().settings().privateClickMeasurementEnabled()
        || !UserGestureIndicator::processingUserGesture())
        return std::nullopt;

    if (auto pcm = parsePrivateClickMeasurementForSKAdNetwork(hrefURL))
        return pcm;

    auto hasAttributionSourceIDAttr = hasAttributeWithoutSynchronization(attributionsourceidAttr);
    auto hasAttributionDestinationAttr = hasAttributeWithoutSynchronization(attributiondestinationAttr);
    if (!hasAttributionSourceIDAttr && !hasAttributionDestinationAttr)
        return std::nullopt;

    auto attributionSourceIDAttr = attributeWithoutSynchronization(attributionsourceidAttr);
    auto attributionDestinationAttr = attributeWithoutSynchronization(attributiondestinationAttr);

    if (!hasAttributionSourceIDAttr || !hasAttributionDestinationAttr || attributionSourceIDAttr.isEmpty() || attributionDestinationAttr.isEmpty()) {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "Both attributionsourceid and attributiondestination need to be set for Private Click Measurement to work."_s);
        return std::nullopt;
    }

    auto attributionSourceID = parseHTMLNonNegativeInteger(attributionSourceIDAttr);
    if (!attributionSourceID) {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "attributionsourceid is not a non-negative integer which is required for Private Click Measurement."_s);
        return std::nullopt;
    }
    
    if (attributionSourceID.value() > std::numeric_limits<uint8_t>::max()) {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, makeString("attributionsourceid must have a non-negative value less than or equal to ", std::numeric_limits<uint8_t>::max(), " for Private Click Measurement."));
        return std::nullopt;
    }

    URL destinationURL { attributionDestinationAttr };
    if (!destinationURL.isValid() || !destinationURL.protocolIsInHTTPFamily()) {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "attributiondestination could not be converted to a valid HTTP-family URL."_s);
        return std::nullopt;
    }

    RegistrableDomain mainDocumentRegistrableDomain;
    if (auto mainDocument = frame->mainFrame().document())
        mainDocumentRegistrableDomain = RegistrableDomain { mainDocument->url() };
    else {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "Could not find a main document to use as source site for Private Click Measurement."_s);
        return std::nullopt;
    }

    if (mainDocumentRegistrableDomain.matches(destinationURL)) {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "attributiondestination can not be the same site as the current website."_s);
        return std::nullopt;
    }

#if PLATFORM(COCOA)
    auto bundleID = applicationBundleIdentifier();
#else
    String bundleID;
#endif
    auto privateClickMeasurement = PrivateClickMeasurement { SourceID(attributionSourceID.value()), SourceSite(WTFMove(mainDocumentRegistrableDomain)), AttributionDestinationSite(destinationURL), bundleID, WallTime::now(), PCM::AttributionEphemeral::No };

    if (auto ephemeralNonce = attributionSourceNonceForPCM())
        privateClickMeasurement.setEphemeralSourceNonce(WTFMove(*ephemeralNonce));

    return privateClickMeasurement;
}

void HTMLAnchorElement::handleClick(Event& event)
{
    event.setDefaultHandled();

    RefPtr<Frame> frame = document().frame();
    if (!frame)
        return;

    if (!hasTagName(aTag) && !isConnected())
        return;

    StringBuilder url;
    url.append(stripLeadingAndTrailingHTMLSpaces(attributeWithoutSynchronization(hrefAttr)));
    appendServerMapMousePosition(url, event);
    URL completedURL = document().completeURL(url.toString());

#if ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)
    if (DataDetection::isDataDetectorLink(*this) && DataDetection::canPresentDataDetectorsUIForElement(*this)) {
        if (auto* page = document().page()) {
            if (page->chrome().client().showDataDetectorsUIForElement(*this, event))
                return;
        }
    }
#endif

    AtomString downloadAttribute;
#if ENABLE(DOWNLOAD_ATTRIBUTE)
    if (document().settings().downloadAttributeEnabled()) {
        // Ignore the download attribute completely if the href URL is cross origin.
        bool isSameOrigin = completedURL.protocolIsData() || document().securityOrigin().canRequest(completedURL);
        if (isSameOrigin)
            downloadAttribute = AtomString { ResourceResponse::sanitizeSuggestedFilename(attributeWithoutSynchronization(downloadAttr)) };
        else if (hasAttributeWithoutSynchronization(downloadAttr))
            document().addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "The download attribute on anchor was ignored because its href URL has a different security origin."_s);
    }
#endif

    SystemPreviewInfo systemPreviewInfo;
#if USE(SYSTEM_PREVIEW)
    systemPreviewInfo.isPreview = isSystemPreviewLink() && document().settings().systemPreviewEnabled();

    if (systemPreviewInfo.isPreview) {
        systemPreviewInfo.element.elementIdentifier = identifier();
        systemPreviewInfo.element.documentIdentifier = document().identifier();
        systemPreviewInfo.element.webPageIdentifier = valueOrDefault(document().frame()->loader().pageID());
        if (auto* child = firstElementChild())
            systemPreviewInfo.previewRect = child->boundsInRootViewSpace();
    }
#endif

    auto referrerPolicy = hasRel(Relation::NoReferrer) ? ReferrerPolicy::NoReferrer : this->referrerPolicy();

    auto effectiveTarget = this->effectiveTarget();
    NewFrameOpenerPolicy newFrameOpenerPolicy = NewFrameOpenerPolicy::Allow;
    if (hasRel(Relation::NoOpener) || hasRel(Relation::NoReferrer) || (!hasRel(Relation::Opener) && isBlankTargetFrameName(effectiveTarget) && !completedURL.protocolIsJavaScript()))
        newFrameOpenerPolicy = NewFrameOpenerPolicy::Suppress;

    auto privateClickMeasurement = parsePrivateClickMeasurement(completedURL);
    // A matching triggering event needs to happen before an attribution report can be sent.
    // Thus, URLs should be empty for now.
    ASSERT(!privateClickMeasurement || (privateClickMeasurement->attributionReportClickSourceURL().isNull() && privateClickMeasurement->attributionReportClickDestinationURL().isNull()));
    
    frame->loader().changeLocation(completedURL, effectiveTarget, &event, referrerPolicy, document().shouldOpenExternalURLsPolicyToPropagate(), newFrameOpenerPolicy, downloadAttribute, systemPreviewInfo, WTFMove(privateClickMeasurement));

    sendPings(completedURL);

    // Preconnect to the link's target for improved page load time.
    if (completedURL.protocolIsInHTTPFamily() && document().settings().linkPreconnectEnabled() && ((frame->isMainFrame() && isSelfTargetFrameName(effectiveTarget)) || isBlankTargetFrameName(effectiveTarget))) {
        auto storageCredentialsPolicy = frame->page() && frame->page()->canUseCredentialStorage() ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;
        platformStrategies()->loaderStrategy()->preconnectTo(frame->loader(), completedURL, storageCredentialsPolicy, LoaderStrategy::ShouldPreconnectAsFirstParty::Yes, nullptr);
    }
}

// Falls back to using <base> element's target if the anchor does not have one.
AtomString HTMLAnchorElement::effectiveTarget() const
{
    auto effectiveTarget = target();
    if (effectiveTarget.isEmpty())
        effectiveTarget = document().baseTarget();
    return effectiveTarget;
}

HTMLAnchorElement::EventType HTMLAnchorElement::eventType(Event& event)
{
    if (!is<MouseEvent>(event))
        return NonMouseEvent;
    return downcast<MouseEvent>(event).shiftKey() ? MouseEventWithShiftKey : MouseEventWithoutShiftKey;
}

bool HTMLAnchorElement::treatLinkAsLiveForEventType(EventType eventType) const
{
    if (!hasEditableStyle())
        return true;

    switch (document().settings().editableLinkBehavior()) {
    case EditableLinkBehavior::Default:
    case EditableLinkBehavior::AlwaysLive:
        return true;

    case EditableLinkBehavior::NeverLive:
        return false;

    // If the selection prior to clicking on this link resided in the same editable block as this link,
    // and the shift key isn't pressed, we don't want to follow the link.
    case EditableLinkBehavior::LiveWhenNotFocused:
        return eventType == MouseEventWithShiftKey || (eventType == MouseEventWithoutShiftKey && rootEditableElementForSelectionOnMouseDown() != rootEditableElement());

    case EditableLinkBehavior::OnlyLiveWithShiftKey:
        return eventType == MouseEventWithShiftKey;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool isEnterKeyKeydownEvent(Event& event)
{
    return event.type() == eventNames().keydownEvent && is<KeyboardEvent>(event) && downcast<KeyboardEvent>(event).keyIdentifier() == "Enter"_s;
}

bool shouldProhibitLinks(Element* element)
{
    return isInSVGImage(element);
}

bool HTMLAnchorElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
    return isLink() || HTMLElement::willRespondToMouseClickEventsWithEditability(editability);
}

static auto& rootEditableElementMap()
{
    static NeverDestroyed<WeakHashMap<HTMLAnchorElement, WeakPtr<Element, WeakPtrImplWithEventTargetData>, WeakPtrImplWithEventTargetData>> map;
    return map.get();
}

Element* HTMLAnchorElement::rootEditableElementForSelectionOnMouseDown() const
{
    if (!m_hasRootEditableElementForSelectionOnMouseDown)
        return 0;
    return rootEditableElementMap().get(*this).get();
}

void HTMLAnchorElement::clearRootEditableElementForSelectionOnMouseDown()
{
    if (!m_hasRootEditableElementForSelectionOnMouseDown)
        return;
    rootEditableElementMap().remove(*this);
    m_hasRootEditableElementForSelectionOnMouseDown = false;
}

void HTMLAnchorElement::setRootEditableElementForSelectionOnMouseDown(Element* element)
{
    if (!element) {
        clearRootEditableElementForSelectionOnMouseDown();
        return;
    }

    rootEditableElementMap().set(*this, element);
    m_hasRootEditableElementForSelectionOnMouseDown = true;
}

void HTMLAnchorElement::setReferrerPolicyForBindings(const AtomString& value)
{
    setAttributeWithoutSynchronization(referrerpolicyAttr, value);
}

String HTMLAnchorElement::referrerPolicyForBindings() const
{
    return referrerPolicyToString(referrerPolicy());
}

ReferrerPolicy HTMLAnchorElement::referrerPolicy() const
{
    if (document().settings().referrerPolicyAttributeEnabled())
        return parseReferrerPolicy(attributeWithoutSynchronization(referrerpolicyAttr), ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
    return ReferrerPolicy::EmptyString;
}

}
