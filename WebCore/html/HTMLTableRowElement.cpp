/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "HTMLCollection.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableElement.h"
#include "HTMLTableSectionElement.h"
#include "NodeList.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLTableRowElement::HTMLTableRowElement(Document* doc)
    : HTMLTablePartElement(trTag, doc)
{
}

bool HTMLTableRowElement::checkDTD(const Node* newChild)
{
    if (newChild->isTextNode())
        return static_cast<const Text*>(newChild)->containsOnlyWhitespace();
    return newChild->hasTagName(tdTag) || newChild->hasTagName(thTag) ||
           newChild->hasTagName(formTag) || newChild->hasTagName(scriptTag);
}

ContainerNode* HTMLTableRowElement::addChild(PassRefPtr<Node> child)
{
    if (child->hasTagName(formTag)) {
        // First add the child.
        HTMLTablePartElement::addChild(child);

        // Now simply return ourselves as the container to insert into.
        // This has the effect of demoting the form to a leaf and moving it safely out of the way.
        return this;
    }

    return HTMLTablePartElement::addChild(child);
}

int HTMLTableRowElement::rowIndex() const
{
    Node *table = parentNode();
    if (!table)
        return -1;
    table = table->parentNode();
    if (!table || !table->hasTagName(tableTag))
        return -1;

    // To match Firefox, the row indices work like this:
    //   Rows from the first <thead> are numbered before all <tbody> rows.
    //   Rows from the first <tfoot> are numbered after all <tbody> rows.
    //   Rows from other <thead> and <tfoot> elements don't get row indices at all.

    int rIndex = 0;

    if (HTMLTableSectionElement* head = static_cast<HTMLTableElement*>(table)->tHead()) {
        for (Node *row = head->firstChild(); row; row = row->nextSibling()) {
            if (row == this)
                return rIndex;
            if (row->hasTagName(trTag))
                ++rIndex;
        }
    }
    
    for (Node *node = table->firstChild(); node; node = node->nextSibling()) {
        if (node->hasTagName(tbodyTag)) {
            HTMLTableSectionElement* section = static_cast<HTMLTableSectionElement*>(node);
            for (Node* row = section->firstChild(); row; row = row->nextSibling()) {
                if (row == this)
                    return rIndex;
                if (row->hasTagName(trTag))
                    ++rIndex;
            }
        }
    }

    if (HTMLTableSectionElement* foot = static_cast<HTMLTableElement*>(table)->tFoot()) {
        for (Node *row = foot->firstChild(); row; row = row->nextSibling()) {
            if (row == this)
                return rIndex;
            if (row->hasTagName(trTag))
                ++rIndex;
        }
    }

    // We get here for rows that are in <thead> or <tfoot> sections other than the main header and footer.
    return -1;
}

int HTMLTableRowElement::sectionRowIndex() const
{
    int rIndex = 0;
    const Node *n = this;
    do {
        n = n->previousSibling();
        if (n && n->hasTagName(trTag))
            rIndex++;
    }
    while (n);

    return rIndex;
}

PassRefPtr<HTMLElement> HTMLTableRowElement::insertCell(int index, ExceptionCode& ec)
{
    RefPtr<HTMLCollection> children = cells();
    int numCells = children ? children->length() : 0;
    if (index < -1 || index > numCells) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }

    RefPtr<HTMLTableCellElement> c = new HTMLTableCellElement(tdTag, document());
    if (index < 0 || index >= numCells)
        appendChild(c, ec);
    else {
        Node* n;
        if (index < 1)
            n = firstChild();
        else
            n = children->item(index);
        insertBefore(c, n, ec);
    }
    return c.release();
}

void HTMLTableRowElement::deleteCell(int index, ExceptionCode& ec)
{
    RefPtr<HTMLCollection> children = cells();
    int numCells = children ? children->length() : 0;
    if (index == -1)
        index = numCells-1;
    if (index >= 0 && index < numCells) {
        RefPtr<Node> cell = children->item(index);
        HTMLElement::removeChild(cell.get(), ec);
    } else
        ec = INDEX_SIZE_ERR;
}

PassRefPtr<HTMLCollection> HTMLTableRowElement::cells()
{
    return new HTMLCollection(this, HTMLCollection::TRCells);
}

void HTMLTableRowElement::setCells(HTMLCollection *, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

String HTMLTableRowElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableRowElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

String HTMLTableRowElement::bgColor() const
{
    return getAttribute(bgcolorAttr);
}

void HTMLTableRowElement::setBgColor(const String &value)
{
    setAttribute(bgcolorAttr, value);
}

String HTMLTableRowElement::ch() const
{
    return getAttribute(charAttr);
}

void HTMLTableRowElement::setCh(const String &value)
{
    setAttribute(charAttr, value);
}

String HTMLTableRowElement::chOff() const
{
    return getAttribute(charoffAttr);
}

void HTMLTableRowElement::setChOff(const String &value)
{
    setAttribute(charoffAttr, value);
}

String HTMLTableRowElement::vAlign() const
{
    return getAttribute(valignAttr);
}

void HTMLTableRowElement::setVAlign(const String &value)
{
    setAttribute(valignAttr, value);
}

}
