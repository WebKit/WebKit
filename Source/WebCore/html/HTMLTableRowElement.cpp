/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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
#include "HTMLTableRowElement.h"

#include "ExceptionCode.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableElement.h"
#include "HTMLTableSectionElement.h"
#include "NodeList.h"
#include "NodeRareData.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLTableRowElement::HTMLTableRowElement(const QualifiedName& tagName, Document& document)
    : HTMLTablePartElement(tagName, document)
{
    ASSERT(hasTagName(trTag));
}

Ref<HTMLTableRowElement> HTMLTableRowElement::create(Document& document)
{
    return adoptRef(*new HTMLTableRowElement(trTag, document));
}

Ref<HTMLTableRowElement> HTMLTableRowElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLTableRowElement(tagName, document));
}

int HTMLTableRowElement::rowIndex() const
{
    auto* parent = parentNode();
    if (!parent)
        return -1;

    HTMLTableElement* table;
    if (is<HTMLTableElement>(*parent))
        table = downcast<HTMLTableElement>(parent);
    else {
        if (!is<HTMLTableSectionElement>(*parent) || !is<HTMLTableElement>(parent->parentNode()))
            return -1;
        table = downcast<HTMLTableElement>(parent->parentNode());
    }

    auto rows = table->rows();
    unsigned length = rows->length();
    for (unsigned i = 0; i < length; ++i) {
        if (rows->item(i) == this)
            return i;
    }

    return -1;
}

int HTMLTableRowElement::sectionRowIndex() const
{
    auto* parent = parentNode();
    if (!parent)
        return -1;

    RefPtr<HTMLCollection> rows;
    if (is<HTMLTableSectionElement>(*parent))
        rows = downcast<HTMLTableSectionElement>(*parent).rows();
    else if (is<HTMLTableElement>(*parent))
        rows = downcast<HTMLTableElement>(*parent).rows();
    else
        return -1;

    unsigned length = rows->length();
    for (unsigned i = 0; i < length; ++i) {
        if (rows->item(i) == this)
            return i;
    }

    return -1;
}

RefPtr<HTMLTableCellElement> HTMLTableRowElement::insertCell(int index, ExceptionCode& ec)
{
    Ref<HTMLCollection> children = cells();
    int numCells = children->length();
    if (index < -1 || index > numCells) {
        ec = INDEX_SIZE_ERR;
        return nullptr;
    }

    auto cell = HTMLTableCellElement::create(tdTag, document());
    if (index < 0 || index >= numCells)
        appendChild(cell, ec);
    else {
        Node* n;
        if (index < 1)
            n = firstChild();
        else
            n = children->item(index);
        insertBefore(cell, n, ec);
    }
    return WTFMove(cell);
}

void HTMLTableRowElement::deleteCell(int index, ExceptionCode& ec)
{
    Ref<HTMLCollection> children = cells();
    int numCells = children->length();
    if (index == -1) {
        if (!numCells)
            return;

        index = numCells - 1;
    }
    if (index >= 0 && index < numCells)
        HTMLElement::removeChild(*children->item(index), ec);
    else
        ec = INDEX_SIZE_ERR;
}

Ref<HTMLCollection> HTMLTableRowElement::cells()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<TRCells>::traversalType>>(*this, TRCells);
}

void HTMLTableRowElement::setCells(HTMLCollection*, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

}
