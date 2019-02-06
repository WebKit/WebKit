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

#include "AdClickAttribution.h"
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
#include "MouseEvent.h"
#include "PingLoader.h"
#include "PlatformMouseEvent.h"
#include "RenderImage.h"
#include "ResourceRequest.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGImage.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "URLUtils.h"
#include "UserGestureIndicator.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLAnchorElement);

using namespace HTMLNames;

HTMLAnchorElement::HTMLAnchorElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_hasRootEditableElementForSelectionOnMouseDown(false)
    , m_wasShiftKeyDownOnMouseDown(false)
    , m_cachedVisitedLinkHash(0)
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
    // Only allow links with tabIndex or contentEditable to be mouse focusable.
    if (isLink())
        return HTMLElement::supportsFocus();

    return HTMLElement::isMouseFocusable();
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

    // FIXME: This should probably pass UseTransforms in the MapCoordinatesFlags.
    auto absolutePosition = downcast<RenderImage>(*renderer).absoluteToLocal(FloatPoint(mouseEvent.pageX(), mouseEvent.pageY()));
    url.append('?');
    url.appendNumber(std::lround(absolutePosition.x()));
    url.append(',');
    url.appendNumber(std::lround(absolutePosition.y()));
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
            if (event.type() == eventNames().mousedownEvent && is<MouseEvent>(event) && downcast<MouseEvent>(event).button() != RightButton && document().frame()) {
                setRootEditableElementForSelectionOnMouseDown(document().frame()->selection().selection().rootEditableElement());
                m_wasShiftKeyDownOnMouseDown = downcast<MouseEvent>(event).shiftKey();
            } else if (event.type() == eventNames().mouseoverEvent) {
                // These are cleared on mouseover and not mouseout because their values are needed for drag events,
                // but drag events happen after mouse out events.
                clearRootEditableElementForSelectionOnMouseDown();
                m_wasShiftKeyDownOnMouseDown = false;
            }
        }
    }

    HTMLElement::defaultEventHandler(event);
}

void HTMLAnchorElement::setActive(bool down, bool pause)
{
    if (hasEditableStyle()) {
        EditableLinkBehavior editableLinkBehavior = document().settings().editableLinkBehavior();
            
        switch (editableLinkBehavior) {
            default:
            case EditableLinkDefaultBehavior:
            case EditableLinkAlwaysLive:
                break;

            case EditableLinkNeverLive:
                return;

            // Don't set the link to be active if the current selection is in the same editable block as
            // this link
            case EditableLinkLiveWhenNotFocused:
                if (down && document().frame() && document().frame()->selection().selection().rootEditableElement() == rootEditableElement())
                    return;
                break;
            
            case EditableLinkOnlyLiveWithShiftKey:
                return;
        }

    }
    
    HTMLElement::setActive(down, pause);
}

void HTMLAnchorElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == hrefAttr) {
        bool wasLink = isLink();
        setIsLink(!value.isNull() && !shouldProhibitLinks(this));
        if (wasLink != isLink())
            invalidateStyleForSubtree();
        if (isLink()) {
            String parsedURL = stripLeadingAndTrailingHTMLSpaces(value);
            if (document().isDNSPrefetchEnabled() && document().frame()) {
                if (protocolIsInHTTPFamily(parsedURL) || parsedURL.startsWith("//"))
                    document().frame()->loader().client().prefetchDNS(document().completeURL(parsedURL).host().toString());
            }
        }
        invalidateCachedVisitedLinkHash();
    } else if (name == nameAttr || name == titleAttr) {
        // Do nothing.
    } else if (name == relAttr) {
        // Update HTMLAnchorElement::relList() if more rel attributes values are supported.
        static NeverDestroyed<AtomicString> noReferrer("noreferrer", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> noOpener("noopener", AtomicString::ConstructFromLiteral);
        static NeverDestroyed<AtomicString> opener("opener", AtomicString::ConstructFromLiteral);
        const bool shouldFoldCase = true;
        SpaceSplitString relValue(value, shouldFoldCase);
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

void HTMLAnchorElement::accessKeyAction(bool sendMouseEvents)
{
    dispatchSimulatedClick(0, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
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
    const AtomicString& value = attributeWithoutSynchronization(draggableAttr);
    if (equalLettersIgnoringASCIICase(value, "true"))
        return true;
    if (equalLettersIgnoringASCIICase(value, "false"))
        return false;
    return hasAttributeWithoutSynchronization(hrefAttr);
}

URL HTMLAnchorElement::href() const
{
    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(attributeWithoutSynchronization(hrefAttr)));
}

void HTMLAnchorElement::setHref(const AtomicString& value)
{
    setAttributeWithoutSynchronization(hrefAttr, value);
}

bool HTMLAnchorElement::hasRel(Relation relation) const
{
    return m_linkRelations.contains(relation);
}

DOMTokenList& HTMLAnchorElement::relList() const
{
    if (!m_relList) {
        m_relList = std::make_unique<DOMTokenList>(const_cast<HTMLAnchorElement&>(*this), HTMLNames::relAttr, [](Document&, StringView token) {
#if USE(SYSTEM_PREVIEW)
            return equalIgnoringASCIICase(token, "noreferrer") || equalIgnoringASCIICase(token, "noopener") || equalIgnoringASCIICase(token, "ar");
#else
            return equalIgnoringASCIICase(token, "noreferrer") || equalIgnoringASCIICase(token, "noopener");
#endif
        });
    }
    return *m_relList;
}

const AtomicString& HTMLAnchorElement::name() const
{
    return getNameAttribute();
}

int HTMLAnchorElement::tabIndex() const
{
    // Skip the supportsFocus check in HTMLElement.
    return Element::tabIndex();
}

String HTMLAnchorElement::target() const
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

void HTMLAnchorElement::setText(const String& text)
{
    setTextContent(text);
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

    SpaceSplitString pingURLs(attributeWithoutSynchronization(pingAttr), false);
    for (unsigned i = 0; i < pingURLs.size(); i++)
        PingLoader::sendPing(*document().frame(), document().completeURL(pingURLs[i]), destinationURL);
}

#if USE(SYSTEM_PREVIEW)
bool HTMLAnchorElement::isSystemPreviewLink() const
{
    if (!RuntimeEnabledFeatures::sharedFeatures().systemPreviewEnabled())
        return false;

    static NeverDestroyed<AtomicString> systemPreviewRelValue("ar", AtomicString::ConstructFromLiteral);

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

Optional<AdClickAttribution> HTMLAnchorElement::parseAdClickAttribution() const
{
    using Campaign = AdClickAttribution::Campaign;
    using Source = AdClickAttribution::Source;
    using Destination = AdClickAttribution::Destination;

    if (!RuntimeEnabledFeatures::sharedFeatures().adClickAttributionEnabled() || !UserGestureIndicator::processingUserGesture())
        return WTF::nullopt;

    if (!hasAttributeWithoutSynchronization(adcampaignidAttr) && !hasAttributeWithoutSynchronization(addestinationAttr))
        return WTF::nullopt;
    
    auto adCampaignIDAttr = attributeWithoutSynchronization(adcampaignidAttr);
    auto adDestinationAttr = attributeWithoutSynchronization(addestinationAttr);
    
    if (adCampaignIDAttr.isEmpty() || adDestinationAttr.isEmpty()) {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "Both adcampaignid and addestination need to be set for Ad Click Attribution to work."_s);
        return WTF::nullopt;
    }

    RefPtr<Frame> frame = document().frame();
    if (!frame || !frame->isMainFrame()) {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "Ad Click Attribution is only supported in the main frame."_s);
        return WTF::nullopt;
    }
    
    auto adCampaignID = parseHTMLNonNegativeInteger(adCampaignIDAttr);
    if (!adCampaignID) {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "adcampaignid can not be converted to a non-negative integer which is required for Ad Click Attribution."_s);
        return WTF::nullopt;
    }
    
    if (adCampaignID.value() >= AdClickAttribution::MaxEntropy) {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, makeString("adcampaignid must have a non-negative value less than ", AdClickAttribution::MaxEntropy, " for Ad Click Attribution."));
        return WTF::nullopt;
    }

    URL adDestinationURL { URL(), adDestinationAttr };
    if (!adDestinationURL.isValid() || !adDestinationURL.protocolIsInHTTPFamily()) {
        document().addConsoleMessage(MessageSource::Other, MessageLevel::Warning, "adddestination could not be converted to a valid HTTP-family URL."_s);
        return WTF::nullopt;
    }

    return AdClickAttribution { Campaign(adCampaignID.value()), Source(document().domain()), Destination(adDestinationURL.host().toString()) };
}

void HTMLAnchorElement::handleClick(Event& event)
{
    event.setDefaultHandled();

    RefPtr<Frame> frame = document().frame();
    if (!frame)
        return;

    StringBuilder url;
    url.append(stripLeadingAndTrailingHTMLSpaces(attributeWithoutSynchronization(hrefAttr)));
    appendServerMapMousePosition(url, event);
    URL completedURL = document().completeURL(url.toString());

    String downloadAttribute;
#if ENABLE(DOWNLOAD_ATTRIBUTE)
    if (RuntimeEnabledFeatures::sharedFeatures().downloadAttributeEnabled()) {
        // Ignore the download attribute completely if the href URL is cross origin.
        bool isSameOrigin = completedURL.protocolIsData() || document().securityOrigin().canRequest(completedURL);
        if (isSameOrigin)
            downloadAttribute = ResourceResponse::sanitizeSuggestedFilename(attributeWithoutSynchronization(downloadAttr));
        else if (hasAttributeWithoutSynchronization(downloadAttr))
            document().addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "The download attribute on anchor was ignored because its href URL has a different security origin.");
    }
#endif

    SystemPreviewInfo systemPreviewInfo;
#if USE(SYSTEM_PREVIEW)
    systemPreviewInfo.isSystemPreview = isSystemPreviewLink() && RuntimeEnabledFeatures::sharedFeatures().systemPreviewEnabled();

    if (systemPreviewInfo.isSystemPreview) {
        if (auto* child = firstElementChild())
            systemPreviewInfo.systemPreviewRect = child->boundsInRootViewSpace();
    }
#endif

    ShouldSendReferrer shouldSendReferrer = hasRel(Relation::NoReferrer) ? NeverSendReferrer : MaybeSendReferrer;

    auto effectiveTarget = this->effectiveTarget();
    Optional<NewFrameOpenerPolicy> newFrameOpenerPolicy;
    if (hasRel(Relation::Opener))
        newFrameOpenerPolicy = NewFrameOpenerPolicy::Allow;
    else if (hasRel(Relation::NoOpener) || (RuntimeEnabledFeatures::sharedFeatures().blankAnchorTargetImpliesNoOpenerEnabled() && equalIgnoringASCIICase(effectiveTarget, "_blank")))
        newFrameOpenerPolicy = NewFrameOpenerPolicy::Suppress;

    auto adClickAttribution = parseAdClickAttribution();
    // A matching conversion event needs to happen before the complete ad click attributionURL can be
    // created. Thus, it should be empty for now.
    ASSERT(!adClickAttribution || adClickAttribution->url().isNull());
    
    frame->loader().urlSelected(completedURL, effectiveTarget, &event, LockHistory::No, LockBackForwardList::No, shouldSendReferrer, document().shouldOpenExternalURLsPolicyToPropagate(), newFrameOpenerPolicy, downloadAttribute, systemPreviewInfo, WTFMove(adClickAttribution));

    sendPings(completedURL);
}

// Falls back to using <base> element's target if the anchor does not have one.
String HTMLAnchorElement::effectiveTarget() const
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
    case EditableLinkDefaultBehavior:
    case EditableLinkAlwaysLive:
        return true;

    case EditableLinkNeverLive:
        return false;

    // If the selection prior to clicking on this link resided in the same editable block as this link,
    // and the shift key isn't pressed, we don't want to follow the link.
    case EditableLinkLiveWhenNotFocused:
        return eventType == MouseEventWithShiftKey || (eventType == MouseEventWithoutShiftKey && rootEditableElementForSelectionOnMouseDown() != rootEditableElement());

    case EditableLinkOnlyLiveWithShiftKey:
        return eventType == MouseEventWithShiftKey;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool isEnterKeyKeydownEvent(Event& event)
{
    return event.type() == eventNames().keydownEvent && is<KeyboardEvent>(event) && downcast<KeyboardEvent>(event).keyIdentifier() == "Enter";
}

bool shouldProhibitLinks(Element* element)
{
    return isInSVGImage(element);
}

bool HTMLAnchorElement::willRespondToMouseClickEvents()
{
    return isLink() || HTMLElement::willRespondToMouseClickEvents();
}

typedef HashMap<const HTMLAnchorElement*, RefPtr<Element>> RootEditableElementMap;

static RootEditableElementMap& rootEditableElementMap()
{
    static NeverDestroyed<RootEditableElementMap> map;
    return map;
}

Element* HTMLAnchorElement::rootEditableElementForSelectionOnMouseDown() const
{
    if (!m_hasRootEditableElementForSelectionOnMouseDown)
        return 0;
    return rootEditableElementMap().get(this);
}

void HTMLAnchorElement::clearRootEditableElementForSelectionOnMouseDown()
{
    if (!m_hasRootEditableElementForSelectionOnMouseDown)
        return;
    rootEditableElementMap().remove(this);
    m_hasRootEditableElementForSelectionOnMouseDown = false;
}

void HTMLAnchorElement::setRootEditableElementForSelectionOnMouseDown(Element* element)
{
    if (!element) {
        clearRootEditableElementForSelectionOnMouseDown();
        return;
    }

    rootEditableElementMap().set(this, element);
    m_hasRootEditableElementForSelectionOnMouseDown = true;
}

}
