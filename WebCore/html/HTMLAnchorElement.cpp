/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "DNS.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoaderTypes.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "MappedAttribute.h"
#include "MouseEvent.h"
#include "Page.h"
#include "RenderImage.h"
#include "Settings.h"

namespace WebCore {

using namespace HTMLNames;

HTMLAnchorElement::HTMLAnchorElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document, CreateElement)
    , m_wasShiftKeyDownOnMouseDown(false)
    , m_linkRelations(0)
{
}

PassRefPtr<HTMLAnchorElement> HTMLAnchorElement::create(Document* document)
{
    return adoptRef(new HTMLAnchorElement(aTag, document));
}

PassRefPtr<HTMLAnchorElement> HTMLAnchorElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLAnchorElement(tagName, document));
}

// This function does not allow leading spaces before the port number.
static unsigned parsePortFromStringPosition(const String& value, unsigned portStart, unsigned& portEnd)
{
    portEnd = portStart;
    while (isASCIIDigit(value[portEnd]))
        ++portEnd;
    return value.substring(portStart, portEnd - portStart).toUInt();
}

bool HTMLAnchorElement::supportsFocus() const
{
    if (isContentEditable())
        return HTMLElement::supportsFocus();
    // If not a link we should still be able to focus the element if it has tabIndex.
    return isLink() || HTMLElement::supportsFocus();
}

bool HTMLAnchorElement::isMouseFocusable() const
{
    // Anchor elements should be mouse focusable, https://bugs.webkit.org/show_bug.cgi?id=26856
#if !PLATFORM(GTK) && !PLATFORM(QT)
    if (isLink())
        // Only allow links with tabIndex or contentEditable to be mouse focusable.
        return HTMLElement::supportsFocus();
#endif

    // Allow tab index etc to control focus.
    return HTMLElement::isMouseFocusable();
}

bool HTMLAnchorElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (!isLink())
        return HTMLElement::isKeyboardFocusable(event);

    if (!isFocusable())
        return false;
    
    if (!document()->frame())
        return false;

    if (!document()->frame()->eventHandler()->tabsToLinks(event))
        return false;

    if (!renderer() || !renderer()->isBoxModelObject())
        return false;
    
    // Before calling absoluteRects, check for the common case where the renderer
    // is non-empty, since this is a faster check and almost always returns true.
    RenderBoxModelObject* box = toRenderBoxModelObject(renderer());
    if (!box->borderBoundingBox().isEmpty())
        return true;

    Vector<IntRect> rects;
    FloatPoint absPos = renderer()->localToAbsolute();
    renderer()->absoluteRects(rects, absPos.x(), absPos.y());
    size_t n = rects.size();
    for (size_t i = 0; i < n; ++i)
        if (!rects[i].isEmpty())
            return true;

    return false;
}

void HTMLAnchorElement::defaultEventHandler(Event* evt)
{
    // React on clicks and on keypresses.
    // Don't make this KEYUP_EVENT again, it makes khtml follow links it shouldn't,
    // when pressing Enter in the combo.
    if (isLink() && (evt->type() == eventNames().clickEvent || (evt->type() == eventNames().keydownEvent && focused()))) {
        MouseEvent* e = 0;
        if (evt->type() == eventNames().clickEvent && evt->isMouseEvent())
            e = static_cast<MouseEvent*>(evt);

        KeyboardEvent* k = 0;
        if (evt->type() == eventNames().keydownEvent && evt->isKeyboardEvent())
            k = static_cast<KeyboardEvent*>(evt);

        if (e && e->button() == RightButton) {
            HTMLElement::defaultEventHandler(evt);
            return;
        }

        // If the link is editable, then we need to check the settings to see whether or not to follow the link
        if (isContentEditable()) {
            EditableLinkBehavior editableLinkBehavior = EditableLinkDefaultBehavior;
            if (Settings* settings = document()->settings())
                editableLinkBehavior = settings->editableLinkBehavior();
                
            switch (editableLinkBehavior) {
                // Always follow the link (Safari 2.0 behavior)
                default:
                case EditableLinkDefaultBehavior:
                case EditableLinkAlwaysLive:
                    break;

                case EditableLinkNeverLive:
                    HTMLElement::defaultEventHandler(evt);
                    return;

                // If the selection prior to clicking on this link resided in the same editable block as this link,
                // and the shift key isn't pressed, we don't want to follow the link
                case EditableLinkLiveWhenNotFocused:
                    if (e && !e->shiftKey() && m_rootEditableElementForSelectionOnMouseDown == rootEditableElement()) {
                        HTMLElement::defaultEventHandler(evt);
                        return;
                    }
                    break;

                // Only follow the link if the shift key is down (WinIE/Firefox behavior)
                case EditableLinkOnlyLiveWithShiftKey:
                    if (e && !e->shiftKey()) {
                        HTMLElement::defaultEventHandler(evt);
                        return;
                    }
                    break;
            }
        }

        if (k) {
            if (k->keyIdentifier() != "Enter") {
                HTMLElement::defaultEventHandler(evt);
                return;
            }
            evt->setDefaultHandled();
            dispatchSimulatedClick(evt);
            return;
        }

        String url = deprecatedParseURL(getAttribute(hrefAttr));

        ASSERT(evt->target());
        ASSERT(evt->target()->toNode());
        if (evt->target()->toNode()->hasTagName(imgTag)) {
            HTMLImageElement* img = static_cast<HTMLImageElement*>(evt->target()->toNode());
            if (img && img->isServerMap()) {
                RenderImage* r = toRenderImage(img->renderer());
                if (r && e) {
                    // FIXME: broken with transforms
                    FloatPoint absPos = r->localToAbsolute();
                    int x = e->pageX() - absPos.x();
                    int y = e->pageY() - absPos.y();
                    url += "?";
                    url += String::number(x);
                    url += ",";
                    url += String::number(y);
                } else {
                    evt->setDefaultHandled();
                    HTMLElement::defaultEventHandler(evt);
                    return;
                }
            }
        }

        if (!evt->defaultPrevented() && document()->frame())
            document()->frame()->loader()->urlSelected(document()->completeURL(url), getAttribute(targetAttr), evt, false, false, true, hasRel(RelationNoReferrer) ? NoReferrer : SendReferrer);

        evt->setDefaultHandled();
    } else if (isLink() && isContentEditable()) {
        // This keeps track of the editable block that the selection was in (if it was in one) just before the link was clicked
        // for the LiveWhenNotFocused editable link behavior
        if (evt->type() == eventNames().mousedownEvent && evt->isMouseEvent() && static_cast<MouseEvent*>(evt)->button() != RightButton && document()->frame() && document()->frame()->selection()) {
            MouseEvent* e = static_cast<MouseEvent*>(evt);

            m_rootEditableElementForSelectionOnMouseDown = document()->frame()->selection()->rootEditableElement();
            m_wasShiftKeyDownOnMouseDown = e && e->shiftKey();
        } else if (evt->type() == eventNames().mouseoverEvent) {
            // These are cleared on mouseover and not mouseout because their values are needed for drag events, but these happen
            // after mouse out events.
            m_rootEditableElementForSelectionOnMouseDown = 0;
            m_wasShiftKeyDownOnMouseDown = false;
        }
    }

    HTMLElement::defaultEventHandler(evt);
}

void HTMLAnchorElement::setActive(bool down, bool pause)
{
    if (isContentEditable()) {
        EditableLinkBehavior editableLinkBehavior = EditableLinkDefaultBehavior;
        if (Settings* settings = document()->settings())
            editableLinkBehavior = settings->editableLinkBehavior();
            
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
                if (down && document()->frame() && document()->frame()->selection() &&
                    document()->frame()->selection()->rootEditableElement() == rootEditableElement())
                    return;
                break;
            
            case EditableLinkOnlyLiveWithShiftKey:
                return;
        }

    }
    
    ContainerNode::setActive(down, pause);
}

void HTMLAnchorElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == hrefAttr) {
        bool wasLink = isLink();
        setIsLink(!attr->isNull());
        if (wasLink != isLink())
            setNeedsStyleRecalc();
        if (isLink()) {
            String parsedURL = deprecatedParseURL(attr->value());
            if (document()->isDNSPrefetchEnabled()) {
                if (protocolIs(parsedURL, "http") || protocolIs(parsedURL, "https") || parsedURL.startsWith("//"))
                    prefetchDNS(document()->completeURL(parsedURL).host());
            }
            if (document()->page() && !document()->page()->javaScriptURLsAreAllowed() && protocolIsJavaScript(parsedURL)) {
                setIsLink(false);
                attr->setValue(nullAtom);
            }
        }
    } else if (attr->name() == nameAttr ||
             attr->name() == titleAttr) {
        // Do nothing.
    } else if (attr->name() == relAttr)
        setRel(attr->value());
    else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLAnchorElement::accessKeyAction(bool sendToAnyElement)
{
    // send the mouse button events if the caller specified sendToAnyElement
    dispatchSimulatedClick(0, sendToAnyElement);
}

bool HTMLAnchorElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == hrefAttr;
}

bool HTMLAnchorElement::canStartSelection() const
{
    // FIXME: We probably want this same behavior in SVGAElement too
    if (!isLink())
        return HTMLElement::canStartSelection();
    return isContentEditable();
}

bool HTMLAnchorElement::draggable() const
{
    // Should be draggable if we have an href attribute.
    const AtomicString& value = getAttribute(draggableAttr);
    if (equalIgnoringCase(value, "true"))
        return true;
    if (equalIgnoringCase(value, "false"))
        return false;
    return hasAttribute(hrefAttr);
}

KURL HTMLAnchorElement::href() const
{
    return document()->completeURL(deprecatedParseURL(getAttribute(hrefAttr)));
}

void HTMLAnchorElement::setHref(const AtomicString& value)
{
    setAttribute(hrefAttr, value);
}

bool HTMLAnchorElement::hasRel(uint32_t relation) const
{
    return m_linkRelations & relation;
}

void HTMLAnchorElement::setRel(const String& value)
{
    m_linkRelations = 0;
    SpaceSplitString newLinkRelations(value, true);
    // FIXME: Add link relations as they are implemented
    if (newLinkRelations.contains("noreferrer"))
        m_linkRelations |= RelationNoReferrer;
}

const AtomicString& HTMLAnchorElement::name() const
{
    return getAttribute(nameAttr);
}

short HTMLAnchorElement::tabIndex() const
{
    // Skip the supportsFocus check in HTMLElement.
    return Element::tabIndex();
}

String HTMLAnchorElement::target() const
{
    return getAttribute(targetAttr);
}

String HTMLAnchorElement::hash() const
{
    String fragmentIdentifier = href().fragmentIdentifier();
    return fragmentIdentifier.isEmpty() ? "" : "#" + fragmentIdentifier;
}

void HTMLAnchorElement::setHash(const String& value)
{
    KURL url = href();
    if (value[0] == '#')
        url.setFragmentIdentifier(value.substring(1));
    else
        url.setFragmentIdentifier(value);
    setHref(url.string());
}

String HTMLAnchorElement::host() const
{
    const KURL& url = href();
    if (url.hostEnd() == url.pathStart())
        return url.host();
    if (isDefaultPortForProtocol(url.port(), url.protocol()))
        return url.host();
    return url.host() + ":" + String::number(url.port());
}

void HTMLAnchorElement::setHost(const String& value)
{
    if (value.isEmpty())
        return;
    KURL url = href();
    if (!url.canSetHostOrPort())
        return;

    int separator = value.find(':');
    if (!separator)
        return;

    if (separator == -1)
        url.setHostAndPort(value);
    else {
        unsigned portEnd;
        unsigned port = parsePortFromStringPosition(value, separator + 1, portEnd);
        if (!port) {
            // http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes
            // specifically goes against RFC 3986 (p3.2) and
            // requires setting the port to "0" if it is set to empty string.
            url.setHostAndPort(value.substring(0, separator + 1) + "0");
        } else {
            if (isDefaultPortForProtocol(port, url.protocol()))
                url.setHostAndPort(value.substring(0, separator));
            else
                url.setHostAndPort(value.substring(0, portEnd));
        }
    }
    setHref(url.string());
}

String HTMLAnchorElement::hostname() const
{
    return href().host();
}

void HTMLAnchorElement::setHostname(const String& value)
{
    // Before setting new value:
    // Remove all leading U+002F SOLIDUS ("/") characters.
    unsigned i = 0;
    unsigned hostLength = value.length();
    while (value[i] == '/')
        i++;

    if (i == hostLength)
        return;

    KURL url = href();
    if (!url.canSetHostOrPort())
        return;

    url.setHost(value.substring(i));
    setHref(url.string());
}

String HTMLAnchorElement::pathname() const
{
    return href().path();
}

void HTMLAnchorElement::setPathname(const String& value)
{
    KURL url = href();
    if (!url.canSetPathname())
        return;

    if (value[0] == '/')
        url.setPath(value);
    else
        url.setPath("/" + value);

    setHref(url.string());
}

String HTMLAnchorElement::port() const
{
    return String::number(href().port());
}

void HTMLAnchorElement::setPort(const String& value)
{
    KURL url = href();
    if (!url.canSetHostOrPort())
        return;

    // http://dev.w3.org/html5/spec/infrastructure.html#url-decomposition-idl-attributes
    // specifically goes against RFC 3986 (p3.2) and
    // requires setting the port to "0" if it is set to empty string.
    unsigned port = value.toUInt();
    if (isDefaultPortForProtocol(port, url.protocol()))
        url.removePort();
    else
        url.setPort(port);

    setHref(url.string());
}

String HTMLAnchorElement::protocol() const
{
    return href().protocol() + ":";
}

void HTMLAnchorElement::setProtocol(const String& value)
{
    KURL url = href();
    url.setProtocol(value);
    setHref(url.string());
}

String HTMLAnchorElement::search() const
{
    String query = href().query();
    return query.isEmpty() ? "" : "?" + query;
}

void HTMLAnchorElement::setSearch(const String& value)
{
    KURL url = href();
    String newSearch = (value[0] == '?') ? value.substring(1) : value;
    // Make sure that '#' in the query does not leak to the hash.
    url.setQuery(newSearch.replace('#', "%23"));

    setHref(url.string());
}

String HTMLAnchorElement::text() const
{
    return innerText();
}

String HTMLAnchorElement::toString() const
{
    return href().string();
}

bool HTMLAnchorElement::isLiveLink() const
{
    if (!isLink())
        return false;
    if (!isContentEditable())
        return true;
    
    EditableLinkBehavior editableLinkBehavior = EditableLinkDefaultBehavior;
    if (Settings* settings = document()->settings())
        editableLinkBehavior = settings->editableLinkBehavior();
        
    switch (editableLinkBehavior) {
        default:
        case EditableLinkDefaultBehavior:
        case EditableLinkAlwaysLive:
            return true;

        case EditableLinkNeverLive:
            return false;

        // Don't set the link to be live if the current selection is in the same editable block as
        // this link or if the shift key is down
        case EditableLinkLiveWhenNotFocused:
            return m_wasShiftKeyDownOnMouseDown || m_rootEditableElementForSelectionOnMouseDown != rootEditableElement();
            
        case EditableLinkOnlyLiveWithShiftKey:
            return m_wasShiftKeyDownOnMouseDown;
    }
}

}
