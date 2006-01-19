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
#include "HTMLLegendElementImpl.h"

#include "rendering/render_form.h"
#include "htmlnames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLLegendElementImpl::HTMLLegendElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
: HTMLGenericFormElementImpl(legendTag, doc, f)
{
}

HTMLLegendElementImpl::~HTMLLegendElementImpl()
{
}

bool HTMLLegendElementImpl::isFocusable() const
{
    return false;
}

RenderObject* HTMLLegendElementImpl::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderLegend(this);
}

DOMString HTMLLegendElementImpl::type() const
{
    return "legend";
}

DOMString HTMLLegendElementImpl::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLLegendElementImpl::setAccessKey(const DOMString &value)
{
    setAttribute(accesskeyAttr, value);
}

DOMString HTMLLegendElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLLegendElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

ElementImpl *HTMLLegendElementImpl::formElement()
{
    // Check if there's a fieldset belonging to this legend.
    NodeImpl *fieldset = parentNode();
    while (fieldset && !fieldset->hasTagName(fieldsetTag))
        fieldset = fieldset->parentNode();
    if (!fieldset)
        return 0;

    // Find first form element inside the fieldset.
    // FIXME: Should we care about tabindex?
    NodeImpl *node = fieldset;
    while ((node = node->traverseNextNode(fieldset))) {
        if (node->isHTMLElement()) {
            HTMLElementImpl *element = static_cast<HTMLElementImpl *>(node);
            if (!element->hasLocalName(legendTag) && element->isGenericFormElement())
                return element;
        }
    }

    return 0;
}

void HTMLLegendElementImpl::focus()
{
    if (ElementImpl *element = formElement())
        element->focus();
}

void HTMLLegendElementImpl::accessKeyAction(bool sendToAnyElement)
{
    if (ElementImpl *element = formElement())
        element->accessKeyAction(sendToAnyElement);
}
    
} // namespace
