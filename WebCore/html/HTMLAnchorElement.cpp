/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2007 Apple Inc. All rights reserved.
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
#include "Document.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "RenderFlow.h"
#include "RenderImage.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "Settings.h"
#include "UIEvent.h"

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

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
    return isFocusable() || (m_isLink && document() && !document()->haveStylesheetsLoaded());
}

bool HTMLAnchorElement::isFocusable() const
{
    if (isContentEditable())
        return HTMLElement::isFocusable();

    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Dave wants to fix that some day with a "has visible content" flag or the like.
    if (!(m_isLink && renderer() && renderer()->style()->visibility() == VISIBLE))
        return false;

    // Before calling absoluteRects, check for the common case where the renderer
    // or one of the continuations is non-empty, since this is a faster check and
    // almost always returns true.
    for (RenderObject* r = renderer(); r; r = r->continuation())
        if (r->width() > 0 && r->height() > 0)
            return true;

    Vector<IntRect> rects;
    int x, y;
    renderer()->absolutePosition(x, y);
    renderer()->absoluteRects(rects, x, y);
    size_t n = rects.size();
    for (size_t i = 0; i < n; ++i)
        if (!rects[i].isEmpty())
            return true;

    return false;
}

bool HTMLAnchorElement::isMouseFocusable() const
{
    return false;
}

bool HTMLAnchorElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (!isFocusable())
        return false;
    
    if (!document()->frame())
        return false;

    return document()->frame()->eventHandler()->tabsToLinks(event);
}

void HTMLAnchorElement::defaultEventHandler(Event* evt)
{
    // React on clicks and on keypresses.
    // Don't make this KEYUP_EVENT again, it makes khtml follow links it shouldn't,
    // when pressing Enter in the combo.
    if (m_isLink && (evt->type() == clickEvent || (evt->type() == keydownEvent && m_focused))) {
        MouseEvent* e = 0;
        if (evt->type() == clickEvent && evt->isMouseEvent())
            e = static_cast<MouseEvent*>(evt);

        KeyboardEvent* k = 0;
        if (evt->type() == keydownEvent && evt->isKeyboardEvent())
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
                RenderImage* r = static_cast<RenderImage*>(img->renderer());
                if (r && e) {
                    int absx, absy;
                    r->absolutePosition(absx, absy);
                    int x = e->pageX() - absx;
                    int y = e->pageY() - absy;
                    url += "?";
                    url += DeprecatedString::number(x);
                    url += ",";
                    url += DeprecatedString::number(y);
                } else {
                    evt->setDefaultHandled();
                    HTMLElement::defaultEventHandler(evt);
                    return;
                }
            }
        }

        if (!evt->defaultPrevented() && document()->frame())
            document()->frame()->loader()->urlSelected(document()->completeURL(url), getAttribute(targetAttr), evt, false, true);

        evt->setDefaultHandled();
    } else if (m_isLink && isContentEditable()) {
    // This keeps track of the editable block that the selection was in (if it was in one) just before the link was clicked
    // for the LiveWhenNotFocused editable link behavior
        if (evt->type() == mousedownEvent && evt->isMouseEvent() && static_cast<MouseEvent*>(evt)->button() != RightButton && document()->frame() && document()->frame()->selectionController()) {
            MouseEvent* e = static_cast<MouseEvent*>(evt);

            m_rootEditableElementForSelectionOnMouseDown = document()->frame()->selectionController()->rootEditableElement();
            m_wasShiftKeyDownOnMouseDown = e && e->shiftKey();
        } else if (evt->type() == mouseoverEvent) {
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
                if (down && document()->frame() && document()->frame()->selectionController() &&
                    document()->frame()->selectionController()->rootEditableElement() == rootEditableElement())
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
        bool wasLink = m_isLink;
        m_isLink = !attr->isNull();
        if (wasLink != m_isLink)
            setChanged();
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

String HTMLAnchorElement::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLAnchorElement::setAccessKey(const String &value)
{
    setAttribute(accesskeyAttr, value);
}

String HTMLAnchorElement::charset() const
{
    return getAttribute(charsetAttr);
}

void HTMLAnchorElement::setCharset(const String &value)
{
    setAttribute(charsetAttr, value);
}

String HTMLAnchorElement::coords() const
{
    return getAttribute(coordsAttr);
}

void HTMLAnchorElement::setCoords(const String &value)
{
    setAttribute(coordsAttr, value);
}

String HTMLAnchorElement::href() const
{
    return document()->completeURL(getAttribute(hrefAttr));
}

void HTMLAnchorElement::setHref(const String &value)
{
    setAttribute(hrefAttr, value);
}

String HTMLAnchorElement::hreflang() const
{
    return getAttribute(hreflangAttr);
}

void HTMLAnchorElement::setHreflang(const String &value)
{
    setAttribute(hreflangAttr, value);
}

String HTMLAnchorElement::name() const
{
    return getAttribute(nameAttr);
}

void HTMLAnchorElement::setName(const String &value)
{
    setAttribute(nameAttr, value);
}

String HTMLAnchorElement::rel() const
{
    return getAttribute(relAttr);
}

void HTMLAnchorElement::setRel(const String &value)
{
    setAttribute(relAttr, value);
}

String HTMLAnchorElement::rev() const
{
    return getAttribute(revAttr);
}

void HTMLAnchorElement::setRev(const String &value)
{
    setAttribute(revAttr, value);
}

String HTMLAnchorElement::shape() const
{
    return getAttribute(shapeAttr);
}

void HTMLAnchorElement::setShape(const String &value)
{
    setAttribute(shapeAttr, value);
}

void HTMLAnchorElement::setTabIndex(int tabIndex)
{
    setAttribute(tabindexAttr, String::number(tabIndex));
}

String HTMLAnchorElement::target() const
{
    return getAttribute(targetAttr);
}

void HTMLAnchorElement::setTarget(const String &value)
{
    setAttribute(targetAttr, value);
}

String HTMLAnchorElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLAnchorElement::setType(const String &value)
{
    setAttribute(typeAttr, value);
}

String HTMLAnchorElement::hash() const
{
    return '#' + KURL(href().deprecatedString()).ref();
}

String HTMLAnchorElement::host() const
{
    return KURL(href().deprecatedString()).host();
}

String HTMLAnchorElement::hostname() const
{
    KURL url(href().deprecatedString());
    if (url.port()==0)
        return url.host();
    else
        return url.host() + ":" + String::number(url.port());
}

String HTMLAnchorElement::pathname() const
{
    return KURL(href().deprecatedString()).path();
}

String HTMLAnchorElement::port() const
{
    return DeprecatedString::number(KURL(href().deprecatedString()).port());
}

String HTMLAnchorElement::protocol() const
{
    return KURL(href().deprecatedString()).protocol() + ":";
}

String HTMLAnchorElement::search() const
{
    return KURL(href().deprecatedString()).query();
}

String HTMLAnchorElement::text() const
{
    return innerText();
}

String HTMLAnchorElement::toString() const
{
    return href();
}

bool HTMLAnchorElement::isLiveLink() const
{
    if (!m_isLink)
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
