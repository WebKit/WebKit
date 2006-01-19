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
#include "HTMLOptGroupElementImpl.h"
#include "HTMLSelectElementImpl.h"

#include "rendering/render_form.h"
#include "htmlnames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLOptGroupElementImpl::HTMLOptGroupElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(optgroupTag, doc, f)
{
}

HTMLOptGroupElementImpl::~HTMLOptGroupElementImpl()
{
}

bool HTMLOptGroupElementImpl::isFocusable() const
{
    return false;
}

DOMString HTMLOptGroupElementImpl::type() const
{
    return "optgroup";
}

NodeImpl *HTMLOptGroupElementImpl::insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::insertBefore(newChild,refChild, exceptioncode);
    if ( !exceptioncode )
        recalcSelectOptions();
    return result;
}

NodeImpl *HTMLOptGroupElementImpl::replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::replaceChild(newChild,oldChild, exceptioncode);
    if(!exceptioncode)
        recalcSelectOptions();
    return result;
}

NodeImpl *HTMLOptGroupElementImpl::removeChild ( NodeImpl *oldChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::removeChild(oldChild, exceptioncode);
    if( !exceptioncode )
        recalcSelectOptions();
    return result;
}

NodeImpl *HTMLOptGroupElementImpl::appendChild ( NodeImpl *newChild, int &exceptioncode )
{
    NodeImpl *result = HTMLGenericFormElementImpl::appendChild(newChild, exceptioncode);
    if( !exceptioncode )
        recalcSelectOptions();
    return result;
}

NodeImpl* HTMLOptGroupElementImpl::addChild(NodeImpl* newChild)
{
    recalcSelectOptions();

    return HTMLGenericFormElementImpl::addChild(newChild);
}

void HTMLOptGroupElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    HTMLGenericFormElementImpl::parseMappedAttribute(attr);
    recalcSelectOptions();
}

void HTMLOptGroupElementImpl::recalcSelectOptions()
{
    NodeImpl *select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();
    if (select)
        static_cast<HTMLSelectElementImpl*>(select)->setRecalcListItems();
}

DOMString HTMLOptGroupElementImpl::label() const
{
    return getAttribute(labelAttr);
}

void HTMLOptGroupElementImpl::setLabel(const DOMString &value)
{
    setAttribute(labelAttr, value);
}
 
} // namespace
