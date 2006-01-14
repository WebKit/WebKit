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
#include "HTMLOptionElementImpl.h"
#include "rendering/render_form.h"

#include "DocumentImpl.h"
#include "dom_textimpl.h"
#include "dom_exception.h"

using namespace khtml;

namespace DOM {

using namespace HTMLNames;

HTMLOptionElementImpl::HTMLOptionElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(optionTag, doc, f)
{
    m_selected = false;
}

bool HTMLOptionElementImpl::isFocusable() const
{
    return false;
}

DOMString HTMLOptionElementImpl::type() const
{
    return "option";
}

DOMString HTMLOptionElementImpl::text() const
{
    DOMString text;

    // WinIE does not use the label attribute, so as a quirk, we ignore it.
    if (getDocument() && !getDocument()->inCompatMode()) {
        DOMString text = getAttribute(labelAttr);
        if (!text.isEmpty())
            return text;
    }

    const NodeImpl *n = this->firstChild();
    while (n) {
        if (n->nodeType() == Node::TEXT_NODE || n->nodeType() == Node::CDATA_SECTION_NODE)
            text += n->nodeValue();
        // skip script content
        if (n->isElementNode() && n->hasTagName(HTMLNames::scriptTag))
            n = n->traverseNextSibling(this);
        else
            n = n->traverseNextNode(this);
    }

    return text;
}

void HTMLOptionElementImpl::setText(const DOMString &text, int &exception)
{
    // Handle the common special case where there's exactly 1 child node, and it's a text node.
    NodeImpl *child = firstChild();
    if (child && child->isTextNode() && !child->nextSibling()) {
        static_cast<TextImpl *>(child)->setData(text, exception);
        return;
    }

    removeChildren();
    appendChild(new TextImpl(getDocument(), text), exception);
}

int HTMLOptionElementImpl::index() const
{
    // Let's do this dynamically. Might be a bit slow, but we're sure
    // we won't forget to update a member variable in some cases...
    HTMLSelectElementImpl *select = getSelect();
    if (select) {
        Array<HTMLElementImpl*> items = select->listItems();
        int l = items.count();
        int optionIndex = 0;
        for(int i = 0; i < l; i++) {
            if (items[i]->hasLocalName(optionTag)) {
                if (static_cast<HTMLOptionElementImpl*>(items[i]) == this)
                    return optionIndex;
                optionIndex++;
            }
        }
    }
    return 0;
}

void HTMLOptionElementImpl::setIndex(int, int &exception)
{
    exception = DOMException::NO_MODIFICATION_ALLOWED_ERR;
    // ###
}

void HTMLOptionElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == selectedAttr)
        m_selected = (!attr->isNull());
    else if (attr->name() == valueAttr)
        m_value = attr->value();
    else
        HTMLGenericFormElementImpl::parseMappedAttribute(attr);
}

DOMString HTMLOptionElementImpl::value() const
{
    if ( !m_value.isNull() )
        return m_value;
    // Use the text if the value wasn't set.
    return text().qstring().stripWhiteSpace();
}

void HTMLOptionElementImpl::setValue(const DOMString &value)
{
    setAttribute(valueAttr, value);
}

void HTMLOptionElementImpl::setSelected(bool _selected)
{
    if(m_selected == _selected)
        return;
    m_selected = _selected;
    HTMLSelectElementImpl *select = getSelect();
    if (select)
        select->notifyOptionSelected(this,_selected);
}

void HTMLOptionElementImpl::childrenChanged()
{
   HTMLSelectElementImpl *select = getSelect();
   if (select)
       select->childrenChanged();
}

HTMLSelectElementImpl *HTMLOptionElementImpl::getSelect() const
{
    NodeImpl *select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();
    return static_cast<HTMLSelectElementImpl*>(select);
}

bool HTMLOptionElementImpl::defaultSelected() const
{
    return !getAttribute(selectedAttr).isNull();
}

void HTMLOptionElementImpl::setDefaultSelected(bool b)
{
    setAttribute(selectedAttr, b ? "" : 0);
}

DOMString HTMLOptionElementImpl::label() const
{
    return getAttribute(labelAttr);
}

void HTMLOptionElementImpl::setLabel(const DOMString &value)
{
    setAttribute(labelAttr, value);
}

} // namespace
