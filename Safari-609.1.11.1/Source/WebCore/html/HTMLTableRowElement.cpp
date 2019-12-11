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

#include "GenericCachedHTMLCollection.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableElement.h"
#include "HTMLTableSectionElement.h"
#include "NodeList.h"
#include "NodeRareData.h"
#include "Text.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLTableRowElement);

using namespace HTMLNames;

inline HTMLTableRowElement::HTMLTableRowElement(const QualifiedName& tagName, Document& document)
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

static inline RefPtr<HTMLTableElement> findTable(const HTMLTableRowElement& row)
{
    auto* parent = row.parentNode();
    if (is<HTMLTableElement>(parent))
        return downcast<HTMLTableElement>(parent);
    if (is<HTMLTableSectionElement>(parent)) {
        auto* grandparent = parent->parentNode();
        if (is<HTMLTableElement>(grandparent))
            return downcast<HTMLTableElement>(grandparent);
    }
    return nullptr;
}

int HTMLTableRowElement::rowIndex() const
{
    auto table = findTable(*this);
    if (!table)
        return -1;

    auto rows = table->rows();
    unsigned length = rows->length();
    for (unsigned i = 0; i < length; ++i) {
        if (rows->item(i) == this)
            return i;
    }

    return -1;
}

static inline RefPtr<HTMLCollection> findRows(const HTMLTableRowElement& row)
{
    auto parent = makeRefPtr(row.parentNode());
    if (is<HTMLTableSectionElement>(parent))
        return downcast<HTMLTableSectionElement>(*parent).rows();
    if (is<HTMLTableElement>(parent))
        return downcast<HTMLTableElement>(*parent).rows();
    return nullptr;
}

int HTMLTableRowElement::sectionRowIndex() const
{
    auto rows = findRows(*this);
    if (!rows)
        return -1;

    unsigned length = rows->length();
    for (unsigned i = 0; i < length; ++i) {
        if (rows->item(i) == this)
            return i;
    }

    return -1;
}

ExceptionOr<Ref<HTMLTableCellElement>> HTMLTableRowElement::insertCell(int index)
{
    if (index < -1)
        return Exception { IndexSizeError };
    auto children = cells();
    int numCells = children->length();
    if (index > numCells)
        return Exception { IndexSizeError };
    auto cell = HTMLTableCellElement::create(tdTag, document());
    ExceptionOr<void> result;
    if (index < 0 || index >= numCells)
        result = appendChild(cell);
    else
        result = insertBefore(cell, index < 1 ? firstChild() : children->item(index));
    if (result.hasException())
        return result.releaseException();
    return cell;
}

ExceptionOr<void> HTMLTableRowElement::deleteCell(int index)
{
    auto children = cells();
    int numCells = children->length();
    if (index == -1) {
        if (!numCells)
            return { };
        index = numCells - 1;
    }
    if (index < 0 || index >= numCells)
        return Exception { IndexSizeError };
    return removeChild(*children->item(index));
}

Ref<HTMLCollection> HTMLTableRowElement::cells()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<TRCells>::traversalType>>(*this, TRCells);
}

}
