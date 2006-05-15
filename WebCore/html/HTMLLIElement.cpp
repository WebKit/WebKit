/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
#include "HTMLLIElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "RenderListItem.h"

namespace WebCore {

using namespace HTMLNames;

HTMLLIElement::HTMLLIElement(Document* doc)
    : HTMLElement(HTMLNames::liTag, doc)
    , m_isValued(false)
{
}

bool HTMLLIElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == typeAttr) {
        result = eListItem; // Share with <ol> since all the values are the same
        return false;
    }
    
    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLLIElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == valueAttr) {
        m_isValued = true;
        m_requestedValue = !attr->isNull() ? attr->value().toInt() : 0;

        if (renderer() && renderer()->isListItem()) {
            RenderListItem* list = static_cast<RenderListItem*>(renderer());
            // ### work out what to do when attribute removed - use default of some sort?
            list->setValue(m_requestedValue);
        }
    } else if (attr->name() == typeAttr) {
        if (attr->value() == "a")
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_LOWER_ALPHA);
        else if (attr->value() == "A")
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_UPPER_ALPHA);
        else if (attr->value() == "i")
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_LOWER_ROMAN);
        else if (attr->value() == "I")
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_UPPER_ROMAN);
        else if (attr->value() == "1")
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_DECIMAL);
        else
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, attr->value());
    } else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLLIElement::attach()
{
    assert(!attached());

    HTMLElement::attach();

    if (renderer() && renderer()->style()->display() == LIST_ITEM) {
        RenderListItem *render = static_cast<RenderListItem*>(renderer());
        
        // Find the enclosing list node.
        Node* listNode = 0;
        Node* n = this;
        while (!listNode && (n = n->parentNode())) {
            if (n->hasTagName(ulTag) || n->hasTagName(olTag))
                listNode = n;
        }
        
        // If we are not in a list, tell the renderer so it can position us inside.
        // We don't want to change our style to say "inside" since that would affect nested nodes.
        if (!listNode)
            render->setNotInList(true);

        // If we had a value attr.
        if (m_isValued)
            render->setValue(m_requestedValue);
    }
}

String HTMLLIElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLLIElement::setType(const String& value)
{
    setAttribute(typeAttr, value);
}

int HTMLLIElement::value() const
{
    return getAttribute(valueAttr).toInt();
}

void HTMLLIElement::setValue(int value)
{
    setAttribute(valueAttr, String::number(value));
}

}
