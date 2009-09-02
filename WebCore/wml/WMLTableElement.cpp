/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 *
 */

#include "config.h"

#if ENABLE(WML)
#include "WMLTableElement.h"

#include "CharacterNames.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "NodeList.h"
#include "RenderObject.h"
#include "Text.h"
#include "WMLErrorHandling.h"
#include "WMLNames.h"

namespace WebCore {

using namespace WMLNames;

WMLTableElement::WMLTableElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
    , m_columns(0)
{
}

WMLTableElement::~WMLTableElement()
{
}

bool WMLTableElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == HTMLNames::alignAttr) {
        result = eTable;
        return false;
    }

    return WMLElement::mapToEntry(attrName, result);
}

void WMLTableElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == columnsAttr) {
        bool isNumber = false;
        m_columns = attr->value().string().toUIntStrict(&isNumber);

        // Spec: This required attribute specifies the number of columns for the table.
        // The user agent must create a table with exactly the number of columns specified
        // by the attribute value. It is an error to specify a value of zero ("0")
        if (!m_columns || !isNumber)
            reportWMLError(document(), WMLErrorInvalidColumnsNumberInTable);
    } else if (attr->name() == HTMLNames::alignAttr)
        m_alignment = parseValueForbiddingVariableReferences(attr->value());
    else
        WMLElement::parseMappedAttribute(attr);
}

void WMLTableElement::finishParsingChildren()
{
    WMLElement::finishParsingChildren();

    if (!m_columns) {
        reportWMLError(document(), WMLErrorInvalidColumnsNumberInTable);
        return;
    }

    Vector<WMLElement*> rowElements = scanTableChildElements(this, trTag);
    if (rowElements.isEmpty())
        return;

    Vector<WMLElement*>::iterator it = rowElements.begin();
    Vector<WMLElement*>::iterator end = rowElements.end();

    for (; it != end; ++it) {
        WMLElement* rowElement = (*it);

        // Squeeze the table to fit in the desired number of columns
        Vector<WMLElement*> columnElements = scanTableChildElements(rowElement, tdTag);
        unsigned actualNumberOfColumns = columnElements.size();

        if (actualNumberOfColumns > m_columns) {
            joinSuperflousColumns(columnElements, rowElement);
            columnElements = scanTableChildElements(rowElement, tdTag);
        } else if (actualNumberOfColumns < m_columns) {
            padWithEmptyColumns(columnElements, rowElement);
            columnElements = scanTableChildElements(rowElement, tdTag);
        }

        // Layout cells according to the 'align' attribute
        alignCells(columnElements, rowElement);
    }
}

Vector<WMLElement*> WMLTableElement::scanTableChildElements(WMLElement* startElement, const QualifiedName& tagName) const
{
    Vector<WMLElement*> childElements;

    RefPtr<NodeList> children = startElement->childNodes();
    if (!children)
        return childElements;

    unsigned length = children->length();
    for (unsigned i = 0; i < length; ++i) {
        Node* child = children->item(i);
        if (child->hasTagName(tagName))
            childElements.append(static_cast<WMLElement*>(child));
    }

    return childElements;
}

void WMLTableElement::transferAllChildrenOfElementToTargetElement(WMLElement* sourceElement, WMLElement* targetElement, unsigned startOffset) const
{
    RefPtr<NodeList> children = sourceElement->childNodes();
    if (!children)
        return;

    ExceptionCode ec = 0;

    unsigned length = children->length();
    for (unsigned i = startOffset; i < length; ++i) {
        RefPtr<Node> clonedNode = children->item(i)->cloneNode(true);
        targetElement->appendChild(clonedNode.release(), ec);
        ASSERT(ec == 0);
    }
}

bool WMLTableElement::tryMergeAdjacentTextCells(Node* item, Node* nextItem) const
{
    if (!item || !nextItem)
        return false;

    if (!item->isTextNode() || !nextItem->isTextNode())
        return false;

    Text* itemText = static_cast<Text*>(item);
    Text* nextItemText = static_cast<Text*>(nextItem);

    String newContent = " ";
    newContent += nextItemText->data();

    ExceptionCode ec = 0;
    itemText->appendData(newContent, ec);
    ASSERT(ec == 0);

    return true;
}

void WMLTableElement::joinSuperflousColumns(Vector<WMLElement*>& columnElements, WMLElement* rowElement) const
{
    // Spec: If the actual number of columns in a row is greater than the value specified
    // by this attribute, the extra columns of the row must be aggregated into the last
    // column such that the row contains exactly the number of columns specified.
    WMLElement* lastColumn = columnElements.at(m_columns - 1);
    ASSERT(lastColumn);

    // Merge superflous columns into a single one
    RefPtr<WMLElement> newCell = WMLElement::create(tdTag, document());
    transferAllChildrenOfElementToTargetElement(lastColumn, newCell.get(), 0);

    ExceptionCode ec = 0;
    unsigned actualNumberOfColumns = columnElements.size();

    for (unsigned i = m_columns; i < actualNumberOfColumns; ++i) {
        WMLElement* columnElement = columnElements.at(i);
        unsigned startOffset = 0;

        // Spec: A single inter-word space must be inserted between two cells that are being aggregated.
        if (tryMergeAdjacentTextCells(newCell->lastChild(), columnElement->firstChild()))
            ++startOffset;

        transferAllChildrenOfElementToTargetElement(columnElement, newCell.get(), startOffset);
    }

    // Remove the columns, that have just been merged
    unsigned i = actualNumberOfColumns;
    for (; i > m_columns; --i) {
        rowElement->removeChild(columnElements.at(i - 1), ec);
        ASSERT(ec == 0);
    }

    // Replace the last column in the row with the new merged column
    rowElement->replaceChild(newCell.release(), lastColumn, ec);
    ASSERT(ec == 0);
}

void WMLTableElement::padWithEmptyColumns(Vector<WMLElement*>& columnElements, WMLElement* rowElement) const
{
    // Spec: If the actual number of columns in a row is less than the value specified by the columns 
    // attribute, the row must be padded with empty columns effectively as if the user agent 
    // appended empty td elements to the row. 
    ExceptionCode ec = 0;

    for (unsigned i = columnElements.size(); i < m_columns; ++i) {
        RefPtr<WMLElement> newCell = WMLElement::create(tdTag, document());
        rowElement->appendChild(newCell.release(), ec);
        ASSERT(ec == 0);
    }
}

void WMLTableElement::alignCells(Vector<WMLElement*>& columnElements, WMLElement* rowElement) const
{
    // Spec: User agents should consider the current language when determining
    // the default alignment and the direction of the table.
    bool rtl = false;
    if (RenderObject* renderer = rowElement->renderer()) {
        if (RenderStyle* style = renderer->style())
            rtl = style->direction() == RTL;
    }

    rowElement->setAttribute(HTMLNames::alignAttr, rtl ? "right" : "left");

    if (m_alignment.isEmpty())
        return;

    unsigned alignLength = m_alignment.length();

    Vector<WMLElement*>::iterator it = columnElements.begin();
    Vector<WMLElement*>::iterator end = columnElements.end();

    for (unsigned i = 0; it != end; ++it, ++i) {
        if (i == alignLength)
            break;

        String alignmentValue;
        switch (m_alignment[i]) {
        case 'C':
            alignmentValue = "center";
            break;
        case 'L':
            alignmentValue = "left";
            break;
        case 'R':
            alignmentValue = "right";
            break;
        default:
            break;
        }

        if (alignmentValue.isEmpty())
            continue;

        (*it)->setAttribute(HTMLNames::alignAttr, alignmentValue);
    }
}

}

#endif
