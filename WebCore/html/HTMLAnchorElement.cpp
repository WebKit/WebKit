/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "CSSHelper.h"
#include "DNS.h"
#include "Document.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "MappedAttribute.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "Page.h"
#include "RenderBox.h"
#include "RenderImage.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "Settings.h"
#include "UIEvent.h"

namespace WebCore {

using namespace HTMLNames;

HTMLAnchorElement::HTMLAnchorElement(Document* doc)
    : HTMLElement(aTag, doc)
    , m_rootEditableElementForSelectionOnMouseDown(0)
    , m_wasShiftKeyDownOnMouseDown(false)
{
}

HTMLAnchorElement::HTMLAnchorElement(const QualifiedName& tagName, Document* doc)
    : HTMLElement(tagName, doc)
    , m_rootEditableElementForSelectionOnMouseDown(0)
    , m_wasShiftKeyDownOnMouseDown(false)
{
}

HTMLAnchorElement::~HTMLAnchorElement()
{
}

bool HTMLAnchorElement::supportsFocus() const
{
    if (isContentEditable())
        return HTMLElement::supportsFocus();
    return isFocusable() || (isLink() && document() && !document()->haveStylesheetsLoaded());
}

bool HTMLAnchorElement::isFocusable() const
{
    if (isContentEditable())
        return HTMLElement::isFocusable();

    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Dave wants to fix that some day with a "has visible content" flag or the like.
    if (!(isLink() && renderer() && renderer()->style()->visibility() == VISIBLE))
        return false;

    return true;
}

bool HTMLAnchorElement::isMouseFocusable() const
{
#if PLATFORM(GTK)
    return HTMLElement::isMouseFocusable();
#else
    return false;
#endif
}

bool HTMLAnchorElement::isKeyboardFocusable(KeyboardEvent* event) const
{
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

        String url = parseURL(getAttribute(hrefAttr));

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
            document()->frame()->loader()->urlSelected(document()->completeURL(url), getAttribute(targetAttr), evt, false, false, true);

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
            
        switch(editableLinkBehavior) {
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
            String parsedURL = parseURL(attr->value());
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
             attr->name() == titleAttr ||
             attr->name() == relAttr) {
        // Do nothing.
    } else
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

const AtomicString& HTMLAnchorElement::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLAnchorElement::setAccessKey(const AtomicString& value)
{
    setAttribute(accesskeyAttr, value);
}

const AtomicString& HTMLAnchorElement::charset() const
{
    return getAttribute(charsetAttr);
}

void HTMLAnchorElement::setCharset(const AtomicString& value)
{
    setAttribute(charsetAttr, value);
}

const AtomicString& HTMLAnchorElement::coords() const
{
    return getAttribute(coordsAttr);
}

void HTMLAnchorElement::setCoords(const AtomicString& value)
{
    setAttribute(coordsAttr, value);
}

KURL HTMLAnchorElement::href() const
{
    return document()->completeURL(getAttribute(hrefAttr));
}

void HTMLAnchorElement::setHref(const AtomicString& value)
{
    setAttribute(hrefAttr, value);
}

const AtomicString& HTMLAnchorElement::hreflang() const
{
    return getAttribute(hreflangAttr);
}

void HTMLAnchorElement::setHreflang(const AtomicString& value)
{
    setAttribute(hreflangAttr, value);
}

const AtomicString& HTMLAnchorElement::name() const
{
    return getAttribute(nameAttr);
}

void HTMLAnchorElement::setName(const AtomicString& value)
{
    setAttribute(nameAttr, value);
}

const AtomicString& HTMLAnchorElement::rel() const
{
    return getAttribute(relAttr);
}

void HTMLAnchorElement::setRel(const AtomicString& value)
{
    setAttribute(relAttr, value);
}

const AtomicString& HTMLAnchorElement::rev() const
{
    return getAttribute(revAttr);
}

void HTMLAnchorElement::setRev(const AtomicString& value)
{
    setAttribute(revAttr, value);
}

const AtomicString& HTMLAnchorElement::shape() const
{
    return getAttribute(shapeAttr);
}

void HTMLAnchorElement::setShape(const AtomicString& value)
{
    setAttribute(shapeAttr, value);
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

void HTMLAnchorElement::setTarget(const AtomicString& value)
{
    setAttribute(targetAttr, value);
}

const AtomicString& HTMLAnchorElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLAnchorElement::setType(const AtomicString& value)
{
    setAttribute(typeAttr, value);
}

String HTMLAnchorElement::hash() const
{
    String ref = href().ref();
    return ref.isEmpty() ? "" : "#" + ref;
}

String HTMLAnchorElement::host() const
{
    return href().host();
}

String HTMLAnchorElement::hostname() const
{
    const KURL& url = href();
    if (url.port() == 0)
        return url.host();
    return url.host() + ":" + String::number(url.port());
}

String HTMLAnchorElement::pathname() const
{
    return href().path();
}

String HTMLAnchorElement::port() const
{
    return String::number(href().port());
}

String HTMLAnchorElement::protocol() const
{
    return href().protocol() + ":";
}

String HTMLAnchorElement::search() const
{
    String query = href().query();
    return query.isEmpty() ? "" : "?" + query;
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
        
    switch(editableLinkBehavior) {
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
