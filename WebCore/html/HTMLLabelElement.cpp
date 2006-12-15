/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "HTMLLabelElement.h"
#include "HTMLFormElement.h"

#include "HTMLNames.h"
#include "EventNames.h"
#include "Event.h"
#include "Document.h"

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

HTMLLabelElement::HTMLLabelElement(Document *doc)
    : HTMLElement(labelTag, doc)
{
}

HTMLLabelElement::~HTMLLabelElement()
{
}

bool HTMLLabelElement::isFocusable() const
{
    return false;
}

HTMLElement* HTMLLabelElement::formElement()
{
    const AtomicString& formElementId = getAttribute(forAttr);
    if (formElementId.isNull()) {
        // Search children of the label element for a form element.
        Node *node = this;
        while ((node = node->traverseNextNode(this))) {
            if (node->isHTMLElement()) {
                HTMLElement *element = static_cast<HTMLElement *>(node);
                if (element->isGenericFormElement())
                    return element;
            }
        }
        return 0;
    }
    if (formElementId.isEmpty())
        return 0;
        
    // Only return HTML elements.
    Element* elt = document()->getElementById(formElementId);
    if (elt && elt->isHTMLElement())
        return static_cast<HTMLElement*>(elt);
    return 0;
}

void HTMLLabelElement::setActive(bool down, bool pause)
{
    if (down == active())
        return;

    // Update our status first.
    HTMLElement::setActive(down, pause);

    // Also update our corresponding control.
    if (Element* element = formElement())
        element->setActive(down, pause);
}

void HTMLLabelElement::setHovered(bool over)
{
    if (over == hovered())
        return;
        
    // Update our status first.
    HTMLElement::setHovered(over);

    // Also update our corresponding control.
    if (Element* element = formElement())
        element->setHovered(over);
}

void HTMLLabelElement::defaultEventHandler(Event* evt)
{
    static bool processingClick = false;

    if (evt->type() == clickEvent && !processingClick) {
        RefPtr<HTMLElement> element = formElement();

        // If we can't find a control or if the control received the click
        // event, then there's no need for us to do anything.
        if (!element || (evt->target() && element->contains(evt->target()->toNode())))
            return;

        processingClick = true;

        // Click the corresponding control.
        element->dispatchSimulatedClick(evt);

        // If the control can be focused via the mouse, then do that too.
        if (element->isMouseFocusable())
            element->focus();

        processingClick = false;
    }
    
    return HTMLElement::defaultEventHandler(evt);
}

void HTMLLabelElement::focus()
{
    if (Element* element = formElement())
        element->focus();
}

void HTMLLabelElement::accessKeyAction(bool sendToAnyElement)
{
    Element *element = formElement();
    if (element)
        element->accessKeyAction(sendToAnyElement);
}

HTMLFormElement *HTMLLabelElement::form()
{
    for (Node *p = parentNode(); p != 0; p = p->parentNode()) {
        if (p->hasTagName(formTag))
            return static_cast<HTMLFormElement *>(p);
    }
    
    return 0;
}

String HTMLLabelElement::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLLabelElement::setAccessKey(const String &value)
{
    setAttribute(accesskeyAttr, value);
}

String HTMLLabelElement::htmlFor() const
{
    return getAttribute(forAttr);
}

void HTMLLabelElement::setHtmlFor(const String &value)
{
    setAttribute(forAttr, value);
}

} // namespace
