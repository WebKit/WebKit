/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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
#include "HTMLTableSectionElement.h"

#include "ExceptionCode.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLCollection.h"
#include "HTMLNames.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableElement.h"
#include "NodeList.h"
#include "NodeRareData.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLTableSectionElement::HTMLTableSectionElement(const QualifiedName& tagName, Document& document)
    : HTMLTablePartElement(tagName, document)
{
}

Ref<HTMLTableSectionElement> HTMLTableSectionElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLTableSectionElement(tagName, document));
}

const StyleProperties* HTMLTableSectionElement::additionalPresentationAttributeStyle() const
{
    if (HTMLTableElement* table = findParentTable())
        return table->additionalGroupStyle(true);
    return 0;
}

// these functions are rather slow, since we need to get the row at
// the index... but they aren't used during usual HTML parsing anyway
RefPtr<HTMLElement> HTMLTableSectionElement::insertRow(int index, ExceptionCode& ec)
{
    RefPtr<HTMLTableRowElement> row;
    Ref<HTMLCollection> children = rows();
    int numRows = children->length();
    if (index < -1 || index > numRows)
        ec = INDEX_SIZE_ERR; // per the DOM
    else {
        row = HTMLTableRowElement::create(trTag, document());
        if (numRows == index || index == -1)
            appendChild(*row, ec);
        else {
            Node* n;
            if (index < 1)
                n = firstChild();
            else
                n = children->item(index);
            insertBefore(*row, n, ec);
        }
    }
    return row;
}

void HTMLTableSectionElement::deleteRow(int index, ExceptionCode& ec)
{
    Ref<HTMLCollection> children = rows();
    int numRows = children->length();
    if (index == -1) {
        if (!numRows)
            return;

        index = numRows - 1;
    }
    if (index >= 0 && index < numRows)
        HTMLElement::removeChild(*children->item(index), ec);
    else
        ec = INDEX_SIZE_ERR;
}

int HTMLTableSectionElement::numRows() const
{
    int rows = 0;
    const Node *n = firstChild();
    while (n) {
        if (n->hasTagName(trTag))
            rows++;
        n = n->nextSibling();
    }

    return rows;
}

const AtomicString& HTMLTableSectionElement::align() const
{
    return attributeWithoutSynchronization(alignAttr);
}

void HTMLTableSectionElement::setAlign(const AtomicString& value)
{
    setAttributeWithoutSynchronization(alignAttr, value);
}

const AtomicString& HTMLTableSectionElement::ch() const
{
    return attributeWithoutSynchronization(charAttr);
}

void HTMLTableSectionElement::setCh(const AtomicString& value)
{
    setAttributeWithoutSynchronization(charAttr, value);
}

const AtomicString& HTMLTableSectionElement::chOff() const
{
    return attributeWithoutSynchronization(charoffAttr);
}

void HTMLTableSectionElement::setChOff(const AtomicString& value)
{
    setAttributeWithoutSynchronization(charoffAttr, value);
}

const AtomicString& HTMLTableSectionElement::vAlign() const
{
    return attributeWithoutSynchronization(valignAttr);
}

void HTMLTableSectionElement::setVAlign(const AtomicString& value)
{
    setAttributeWithoutSynchronization(valignAttr, value);
}

Ref<HTMLCollection> HTMLTableSectionElement::rows()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<TSectionRows>::traversalType>>(*this, TSectionRows);
}

}
