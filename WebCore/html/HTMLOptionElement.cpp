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
#include "HTMLOptionElement.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "HTMLSelectElement.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLOptionElement::HTMLOptionElement(Document *doc, HTMLFormElement *f)
    : HTMLGenericFormElement(optionTag, doc, f)
{
    m_selected = false;
}

bool HTMLOptionElement::isFocusable() const
{
    return false;
}

const AtomicString& HTMLOptionElement::type() const
{
    static const AtomicString option("option");
    return option;
}

String HTMLOptionElement::text() const
{
    String text;

    // WinIE does not use the label attribute, so as a quirk, we ignore it.
    if (!document()->inCompatMode()) {
        String text = getAttribute(labelAttr);
        if (!text.isEmpty())
            return text;
    }

    const Node *n = this->firstChild();
    while (n) {
        if (n->nodeType() == TEXT_NODE || n->nodeType() == CDATA_SECTION_NODE)
            text += n->nodeValue();
        // skip script content
        if (n->isElementNode() && n->hasTagName(HTMLNames::scriptTag))
            n = n->traverseNextSibling(this);
        else
            n = n->traverseNextNode(this);
    }

    return text;
}

void HTMLOptionElement::setText(const String &text, ExceptionCode& ec)
{
    // Handle the common special case where there's exactly 1 child node, and it's a text node.
    Node *child = firstChild();
    if (child && child->isTextNode() && !child->nextSibling()) {
        static_cast<Text *>(child)->setData(text, ec);
        return;
    }

    removeChildren();
    appendChild(new Text(document(), text), ec);
}

int HTMLOptionElement::index() const
{
    // Let's do this dynamically. Might be a bit slow, but we're sure
    // we won't forget to update a member variable in some cases...
    HTMLSelectElement *select = getSelect();
    if (select) {
        DeprecatedArray<HTMLElement*> items = select->listItems();
        int l = items.count();
        int optionIndex = 0;
        for(int i = 0; i < l; i++) {
            if (items[i]->hasLocalName(optionTag)) {
                if (static_cast<HTMLOptionElement*>(items[i]) == this)
                    return optionIndex;
                optionIndex++;
            }
        }
    }
    return 0;
}

void HTMLOptionElement::setIndex(int, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
    // ###
}

void HTMLOptionElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == selectedAttr)
        m_selected = (!attr->isNull());
    else if (attr->name() == valueAttr)
        m_value = attr->value();
    else
        HTMLGenericFormElement::parseMappedAttribute(attr);
}

String HTMLOptionElement::value() const
{
    if ( !m_value.isNull() )
        return m_value;
    // Use the text if the value wasn't set.
    return text().deprecatedString().stripWhiteSpace();
}

void HTMLOptionElement::setValue(const String &value)
{
    setAttribute(valueAttr, value);
}

void HTMLOptionElement::setSelected(bool _selected)
{
    if(m_selected == _selected)
        return;
    m_selected = _selected;
    HTMLSelectElement *select = getSelect();
    if (select)
        select->notifyOptionSelected(this,_selected);
}

void HTMLOptionElement::childrenChanged()
{
   HTMLSelectElement *select = getSelect();
   if (select)
       select->childrenChanged();
}

HTMLSelectElement *HTMLOptionElement::getSelect() const
{
    Node *select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();
    return static_cast<HTMLSelectElement*>(select);
}

bool HTMLOptionElement::defaultSelected() const
{
    return !getAttribute(selectedAttr).isNull();
}

void HTMLOptionElement::setDefaultSelected(bool b)
{
    setAttribute(selectedAttr, b ? "" : 0);
}

String HTMLOptionElement::label() const
{
    return getAttribute(labelAttr);
}

void HTMLOptionElement::setLabel(const String &value)
{
    setAttribute(labelAttr, value);
}

} // namespace
