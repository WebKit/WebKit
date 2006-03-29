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

#include "rendering/render_form.h"
#include "HTMLNames.h"
#include "EventNames.h"
#include "Document.h"

using namespace WebCore;
using namespace WebCore::HTMLNames;
using namespace WebCore::EventNames;

namespace WebCore {

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

void HTMLLabelElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else
        HTMLElement::parseMappedAttribute(attr);
}

Element *HTMLLabelElement::formElement()
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
    return document()->getElementById(formElementId);
}

void HTMLLabelElement::focus()
{
    if (Element *element = formElement())
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
