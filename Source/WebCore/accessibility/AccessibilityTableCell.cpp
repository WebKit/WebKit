/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilityTableCell.h"

#include "AXObjectCache.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableRow.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "RenderObject.h"
#include "RenderTableCell.h"

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityTableCell::AccessibilityTableCell(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
{
}

AccessibilityTableCell::~AccessibilityTableCell()
{
}

PassRefPtr<AccessibilityTableCell> AccessibilityTableCell::create(RenderObject* renderer)
{
    return adoptRef(new AccessibilityTableCell(renderer));
}

bool AccessibilityTableCell::computeAccessibilityIsIgnored() const
{
    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == IncludeObject)
        return false;
    if (decision == IgnoreObject)
        return true;
    
    // Ignore anonymous table cells.
    if (!node())
        return true;
        
    if (!isTableCell())
        return AccessibilityRenderObject::computeAccessibilityIsIgnored();
    
    return false;
}

AccessibilityTable* AccessibilityTableCell::parentTable() const
{
    if (!m_renderer || !m_renderer->isTableCell())
        return 0;

    // If the document no longer exists, we might not have an axObjectCache.
    if (!axObjectCache())
        return 0;
    
    // Do not use getOrCreate. parentTable() can be called while the render tree is being modified 
    // by javascript, and creating a table element may try to access the render tree while in a bad state.
    // By using only get() implies that the AXTable must be created before AXTableCells. This should
    // always be the case when AT clients access a table.
    // https://bugs.webkit.org/show_bug.cgi?id=42652    
    return toAccessibilityTable(axObjectCache()->get(toRenderTableCell(m_renderer)->table()));
}
    
bool AccessibilityTableCell::isTableCell() const
{
    AccessibilityObject* parent = parentObjectUnignored();
    if (!parent || !parent->isTableRow())
        return false;
    
    return true;
}
    
AccessibilityRole AccessibilityTableCell::determineAccessibilityRole()
{
    // Always call determineAccessibleRole so that the ARIA role is set.
    // Even though this object reports a Cell role, the ARIA role will be used
    // to determine if it's a column header.
    AccessibilityRole defaultRole = AccessibilityRenderObject::determineAccessibilityRole();
    if (!isTableCell())
        return defaultRole;
    
    return CellRole;
}
    
bool AccessibilityTableCell::isTableHeaderCell() const
{
    return node() && node()->hasTagName(thTag);
}

bool AccessibilityTableCell::isTableCellInSameRowGroup(AccessibilityTableCell* otherTableCell)
{
    Node* parentNode = node();
    for ( ; parentNode; parentNode = parentNode->parentNode()) {
        if (parentNode->hasTagName(theadTag) || parentNode->hasTagName(tbodyTag) || parentNode->hasTagName(tfootTag))
            break;
    }
    
    Node* otherParentNode = otherTableCell->node();
    for ( ; otherParentNode; otherParentNode = otherParentNode->parentNode()) {
        if (otherParentNode->hasTagName(theadTag) || otherParentNode->hasTagName(tbodyTag) || otherParentNode->hasTagName(tfootTag))
            break;
    }
    
    return otherParentNode == parentNode;
}


bool AccessibilityTableCell::isTableCellInSameColGroup(AccessibilityTableCell* tableCell)
{
    std::pair<unsigned, unsigned> colRange;
    columnIndexRange(colRange);
    
    std::pair<unsigned, unsigned> otherColRange;
    tableCell->columnIndexRange(otherColRange);
    
    if (colRange.first <= (otherColRange.first + otherColRange.second))
        return true;
    return false;
}
    
String AccessibilityTableCell::expandedTextValue() const
{
    return getAttribute(abbrAttr);
}
    
bool AccessibilityTableCell::supportsExpandedTextValue() const
{
    return isTableHeaderCell() && hasAttribute(abbrAttr);
}
    
void AccessibilityTableCell::columnHeaders(AccessibilityChildrenVector& headers)
{
    AccessibilityTable* parent = parentTable();
    if (!parent)
        return;

    // Choose columnHeaders as the place where the "headers" attribute is reported.
    ariaElementsFromAttribute(headers, headersAttr);
    // If the headers attribute returned valid values, then do not further search for column headers.
    if (!headers.isEmpty())
        return;
    
    std::pair<unsigned, unsigned> rowRange;
    rowIndexRange(rowRange);
    
    std::pair<unsigned, unsigned> colRange;
    columnIndexRange(colRange);
    
    for (unsigned row = 0; row < rowRange.first; row++) {
        AccessibilityTableCell* tableCell = parent->cellForColumnAndRow(colRange.first, row);
        if (!tableCell || tableCell == this || headers.contains(tableCell))
            continue;

        std::pair<unsigned, unsigned> childRowRange;
        tableCell->rowIndexRange(childRowRange);
            
        const AtomicString& scope = tableCell->getAttribute(scopeAttr);
        if (scope == "col" || tableCell->isTableHeaderCell())
            headers.append(tableCell);
        else if (scope == "colgroup" && isTableCellInSameColGroup(tableCell))
            headers.append(tableCell);
    }
}
    
void AccessibilityTableCell::rowHeaders(AccessibilityChildrenVector& headers)
{
    AccessibilityTable* parent = parentTable();
    if (!parent)
        return;

    std::pair<unsigned, unsigned> rowRange;
    rowIndexRange(rowRange);

    std::pair<unsigned, unsigned> colRange;
    columnIndexRange(colRange);

    for (unsigned column = 0; column < colRange.first; column++) {
        AccessibilityTableCell* tableCell = parent->cellForColumnAndRow(column, rowRange.first);
        if (!tableCell || tableCell == this || headers.contains(tableCell))
            continue;
        
        const AtomicString& scope = tableCell->getAttribute(scopeAttr);
        if (scope == "row")
            headers.append(tableCell);
        else if (scope == "rowgroup" && isTableCellInSameRowGroup(tableCell))
            headers.append(tableCell);
    }
}
    
void AccessibilityTableCell::rowIndexRange(std::pair<unsigned, unsigned>& rowRange)
{
    if (!m_renderer || !m_renderer->isTableCell())
        return;
    
    RenderTableCell* renderCell = toRenderTableCell(m_renderer);
    rowRange.first = renderCell->rowIndex();
    rowRange.second = renderCell->rowSpan();
    
    // since our table might have multiple sections, we have to offset our row appropriately
    RenderTableSection* section = renderCell->section();
    RenderTable* table = renderCell->table();
    if (!table || !section)
        return;

    RenderTableSection* tableSection = table->topSection();    
    unsigned rowOffset = 0;
    while (tableSection) {
        if (tableSection == section)
            break;
        rowOffset += tableSection->numRows();
        tableSection = table->sectionBelow(tableSection, SkipEmptySections);
    }

    rowRange.first += rowOffset;
}
    
void AccessibilityTableCell::columnIndexRange(std::pair<unsigned, unsigned>& columnRange)
{
    if (!m_renderer || !m_renderer->isTableCell())
        return;
    
    RenderTableCell* renderCell = toRenderTableCell(m_renderer);
    columnRange.first = renderCell->col();
    columnRange.second = renderCell->colSpan();    
}
    
AccessibilityObject* AccessibilityTableCell::titleUIElement() const
{
    // Try to find if the first cell in this row is a <th>. If it is,
    // then it can act as the title ui element. (This is only in the
    // case when the table is not appearing as an AXTable.)
    if (isTableCell() || !m_renderer || !m_renderer->isTableCell())
        return 0;

    // Table cells that are th cannot have title ui elements, since by definition
    // they are title ui elements
    Node* node = m_renderer->node();
    if (node && node->hasTagName(thTag))
        return 0;
    
    RenderTableCell* renderCell = toRenderTableCell(m_renderer);

    // If this cell is in the first column, there is no need to continue.
    int col = renderCell->col();
    if (!col)
        return 0;

    int row = renderCell->rowIndex();

    RenderTableSection* section = renderCell->section();
    if (!section)
        return 0;
    
    RenderTableCell* headerCell = section->primaryCellAt(row, 0);
    if (!headerCell || headerCell == renderCell)
        return 0;

    if (!headerCell->element() || !headerCell->element()->hasTagName(thTag))
        return 0;
    
    return axObjectCache()->getOrCreate(headerCell);
}
    
} // namespace WebCore
