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
#include "HTMLLabelElementImpl.h"
#include "HTMLFormElementImpl.h"

#include "rendering/render_form.h"
#include "htmlnames.h"
#include "EventNames.h"
#include "DocumentImpl.h"

using namespace khtml;
using namespace DOM::HTMLNames;
using namespace DOM::EventNames;

namespace DOM {

HTMLLabelElementImpl::HTMLLabelElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(labelTag, doc)
{
}

HTMLLabelElementImpl::~HTMLLabelElementImpl()
{
}

bool HTMLLabelElementImpl::isFocusable() const
{
    return false;
}

void HTMLLabelElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

ElementImpl *HTMLLabelElementImpl::formElement()
{
    const AtomicString& formElementId = getAttribute(forAttr);
    if (formElementId.isNull()) {
        // Search children of the label element for a form element.
        NodeImpl *node = this;
        while ((node = node->traverseNextNode(this))) {
            if (node->isHTMLElement()) {
                HTMLElementImpl *element = static_cast<HTMLElementImpl *>(node);
                if (element->isGenericFormElement())
                    return element;
            }
        }
        return 0;
    }
    if (formElementId.isEmpty())
        return 0;
    return getDocument()->getElementById(formElementId);
}

void HTMLLabelElementImpl::focus()
{
    if (ElementImpl *element = formElement())
        element->focus();
}

void HTMLLabelElementImpl::accessKeyAction(bool sendToAnyElement)
{
    ElementImpl *element = formElement();
    if (element)
        element->accessKeyAction(sendToAnyElement);
}

HTMLFormElementImpl *HTMLLabelElementImpl::form()
{
    for (NodeImpl *p = parentNode(); p != 0; p = p->parentNode()) {
        if (p->hasTagName(formTag))
            return static_cast<HTMLFormElementImpl *>(p);
    }
    
    return 0;
}

DOMString HTMLLabelElementImpl::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLLabelElementImpl::setAccessKey(const DOMString &value)
{
    setAttribute(accesskeyAttr, value);
}

DOMString HTMLLabelElementImpl::htmlFor() const
{
    return getAttribute(forAttr);
}

void HTMLLabelElementImpl::setHtmlFor(const DOMString &value)
{
    setAttribute(forAttr, value);
}

} // namespace
